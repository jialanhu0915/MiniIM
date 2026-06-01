/**
 * @file   NetworkServerDlg.cpp
 * @brief  即时通讯服务端 - 主界面实现
 * @author Yan Runxin
 * @date   2026-05-25
 */

#include "pch.h"
#include "framework.h"
#include "NetworkServer.h"
#include "NetworkServerDlg.h"
#include "SQLiteWrapper.h"
#include "../Common/JsonUtils.h"
extern thread_local CConnectSocket* g_pCurrentSocket;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// CAboutDlg
// ============================================================================

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// ============================================================================
// CNetworkServerDlg — 消息映射
// ============================================================================

BEGIN_MESSAGE_MAP(CNetworkServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_STN_CLICKED(IDC_STATIC_PORT, &CNetworkServerDlg::OnStnClickedStaticPort)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CNetworkServerDlg::OnEnChangeEditPort)
	ON_BN_CLICKED(IDC_BUTTON_START, &CNetworkServerDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CNetworkServerDlg::OnBnClickedButtonStop)
	ON_MESSAGE(WM_USER_UPDATE_UI, &CNetworkServerDlg::OnUIUpdate)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// ============================================================================
// 构造 / 初始化
// ============================================================================

CNetworkServerDlg::CNetworkServerDlg(CWnd* pParent)
	: CDialogEx(IDD_NETWORKSERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_listenSocket.m_pDlg = this;
}

void CNetworkServerDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_USERS, m_userList);
}

BOOL CNetworkServerDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu) {
		CString strAboutMenu;
		if (strAboutMenu.LoadString(IDS_ABOUTBOX) && !strAboutMenu.IsEmpty()) {
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	// ---- 初始状态 ----
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	SetDlgItemText(IDC_STATIC_STATUS, _T("空闲"));

	// ---- 打开数据库 ----
	try {
		m_dbWrapper.bOpenDb("chat_history.db");
		UpdateLog(_T("[数据库] 数据库已打开"));
	}
	catch (const std::exception& e) {
		UpdateLog(_T("[错误] 无法打开数据库: ") + CString(e.what()));
	}

	// ---- 初始化在线用户列表 ----
	m_userList.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_userList.InsertColumn(0, _T("用户名"), LVCFMT_LEFT, 100);
	m_userList.InsertColumn(1, _T("IP 地址"), LVCFMT_LEFT, 110);

	// ---- 注册协议处理器 ----
	RegisterProtocolHandlers();

	return TRUE;
}

BOOL CNetworkServerDlg::PreTranslateMessage(MSG* pMsg) {
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
		// 如果焦点在消息发送框（IDC_EDIT_MSG），触发发送
		CWnd* pFocus = GetFocus();
		if (pFocus && pFocus->GetDlgCtrlID() == IDC_EDIT_MSG) {
			//OnBnClickedButtonSend();
			return TRUE;  // 已处理，不继续传递
		}
		// 其他编辑框：吞掉 Enter，不做任何事
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

// ============================================================================
// ============================================================================
// 协议处理器（你的网络层代码参考）
// ============================================================================
void CNetworkServerDlg::RegisterProtocolHandlers() {
	// ========================================================================
	  // LOGIN
	  // ========================================================================
	m_dispatcher.on(MsgType::LOGIN, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;

		std::string username = JsonGetString(json, "username");
		if (username.empty()) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","用户名不能为空")
				});
			SendToClient(pSocket, MsgType::LOGIN_RESP, resp);
			return;
		}

		// DB 调用（m_csDb 自动加锁）
		int iUserID = m_dbWrapper.iAddUser(username);

		// 获取在线用户快照（锁内）
		std::vector<std::pair<int, CConnectSocket*>> onlineUsers;
		{
			CSingleLock lock(&m_csData, TRUE);
			for (auto& it : m_userIdToSocket) {
				if (it.first != iUserID) {
					onlineUsers.push_back(it);
				}
			}
		}

		// 查询好友列表（m_csDb 内部加锁）并构造 JSON（锁外）
		auto friendIds = m_dbWrapper.vecGetFriends(iUserID);
		std::string friendListJson = "\"friends\": [";
		for (size_t i = 0; i < friendIds.size(); ++i) {
			int fid = friendIds[i].first;
			std::string fname = friendIds[i].second;
			bool online = false;
			{
				CSingleLock lock(&m_csData, TRUE);
				online = m_userIdToSocket.count(fid) > 0;
			}
			friendListJson += JsonMakeObject({
				JsonSetInt("user_id", fid),
				JsonSetString("username", fname),
				JsonSetString("online", online ? "true" : "false")
			});
			if (i != friendIds.size() - 1) friendListJson += ",";
		}
		friendListJson += "]";

		// map 写入（锁内）
		{
			CSingleLock lock(&m_csData, TRUE);
			m_userIdToSocket[iUserID] = pSocket;
			m_socketToUserId[pSocket] = iUserID;
			m_userIdToName[iUserID] = username;
		}

		// UI 更新
		PostUIUpdate(UIUpdateType::LOGIN, _T(""), iUserID, username, pSocket->m_clientIP);

		// 回复登录成功（不需要锁，只发数据）
		std::string resp = "{" +
			JsonSetString("success", "true") + "," +
			JsonSetInt("user_id", iUserID) + "," +
			friendListJson + "}";
		SendToClient(pSocket, MsgType::LOGIN_RESP, resp);

		// 广播上线给其他用户（锁外发送，不阻塞其他线程）
		std::string onlineMsg = "{" +
			JsonSetInt("user_id", iUserID) + "," +
			JsonSetString("username", username) + "}";
		for (auto& it : onlineUsers) {
			SendToClient(it.second, MsgType::STATUS_ONLINE, onlineMsg);
		}

		PostUIUpdate(UIUpdateType::LOG,
			CString(_T("[登录] ")) + CString(username.c_str())
			+ _T(" (ID=") + CString(std::to_string(iUserID).c_str()) + _T(")"));
		});

	// ========================================================================
	  // LOGOUT
	  // ========================================================================
	m_dispatcher.on(MsgType::LOGOUT, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;

		int userId = -1;
		std::string username;
		std::vector<CConnectSocket*> targets;
		{
			CSingleLock lock(&m_csData, TRUE);
			auto it = m_socketToUserId.find(pSocket);
			if (it == m_socketToUserId.end()) return;

			userId = it->second;
			username = m_userIdToName[userId];
			m_userIdToSocket.erase(userId);
			m_socketToUserId.erase(pSocket);
			m_userIdToName.erase(userId);

			for (auto& pair : m_userIdToSocket) {
				targets.push_back(pair.second);
			}
		}

		PostUIUpdate(UIUpdateType::LOGOUT, _T(""), userId, username, "");

		std::string offlineMsg = "{" +
			JsonSetInt("user_id", userId) + "," +
			JsonSetString("username", username) + "}";
		for (auto* target : targets) {
			SendToClient(target, MsgType::STATUS_OFFLINE, offlineMsg);
		}
		});

	m_dispatcher.on(MsgType::TEXT, [this](const std::string& json) {
		int senderId = JsonGetInt(json, "sender_id");
		int receiverId = JsonGetInt(json, "receiver_id");
		std::string content = JsonGetString(json, "content");

		// 保存到数据库（m_csDb 自动加锁）
		m_dbWrapper.bSaveMessage(senderId, receiverId, content);

		// 查找接收方 socket（锁内）
		CConnectSocket* pReceiver = nullptr;
		{
			CSingleLock lock(&m_csData, TRUE);
			auto it = m_userIdToSocket.find(receiverId);
			if (it != m_userIdToSocket.end()) pReceiver = it->second;
		}

		if (pReceiver) {
			// 在线：直接转发
			std::string forward = JsonMakeObject({
				JsonSetInt("sender_id", senderId),
				JsonSetString("content", content)
				});
			SendToClient(pReceiver, MsgType::TEXT, forward);
		}
		else {
			// 离线：存离线消息（m_csDb 自动加锁）
			std::string offlineMsg = JsonMakeObject({
				JsonSetString("type", "text"),
				JsonSetInt("sender_id", senderId),
				JsonSetString("content", content)
				});
			m_dbWrapper.bSaveOfflineMessage(senderId, receiverId, offlineMsg);
		}

		PostUIUpdate(UIUpdateType::LOG,
			CString(_T("[消息] user=")) + CString(std::to_string(senderId).c_str())
			+ _T(" → user=") + CString(std::to_string(receiverId).c_str()));
		});

	m_dispatcher.on(MsgType::GROUP_TEXT, [this](const std::string& json) {
		int senderId = JsonGetInt(json, "sender_id");
		std::string content = JsonGetString(json, "content");

		// 广播给所有在线用户（除发送者外）
		std::vector<CConnectSocket*> targets;
		{
			CSingleLock lock(&m_csData, TRUE);
			for (auto& pair : m_userIdToSocket) {
				if (pair.first != senderId) {
					targets.push_back(pair.second);
				}
			}
		}

		std::string broadcast = JsonMakeObject({
			JsonSetInt("sender_id", senderId),
			JsonSetString("content", content)
			});
		for (auto* target : targets) {
			SendToClient(target, MsgType::GROUP_TEXT, broadcast);
		}

		PostUIUpdate(UIUpdateType::LOG,
			CString(_T("[群聊] user=")) + CString(std::to_string(senderId).c_str())
			+ _T(" → 全体"));
		});

	m_dispatcher.on(MsgType::FRIEND_ADD, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;

		int senderId = JsonGetInt(json, "user_id");
		std::string reciverUsername = JsonGetString(json, "friend_username");

		// DB 查询用户 ID（m_csDb 自动加锁）
		int reciverId = m_dbWrapper.iGetUserId(reciverUsername);
		if (reciverId == -1) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","用户不存在")
				});
			SendToClient(pSocket, MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		// 检查是否已经是好友（m_csDb 自动加锁）
		bool bIsFriend = m_dbWrapper.bIsFriend(senderId, reciverId);
		if (bIsFriend) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","已经是好友了")
				});
			SendToClient(pSocket, MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		// 通知接收方
		std::string senderName;
		CConnectSocket* pReciverSocket = nullptr;
		{
			CSingleLock lock(&m_csData, TRUE);
			senderName = m_userIdToName[senderId];
			auto it = m_userIdToSocket.find(reciverId);
			pReciverSocket = it != m_userIdToSocket.end() ? it->second : nullptr;
		}
		if (pReciverSocket) {
			std::string notify = "{" +
				JsonSetInt("user_id", senderId) + "," +
				JsonSetString("username", senderName) + "}";
			SendToClient(pReciverSocket, MsgType::FRIEND_ADD, notify);
		}
		else
		{
			// 保存离线消息
			std::string offlineMsg = "{" +
				JsonSetString("type", "friend_add") + "," +
				JsonSetInt("user_id", senderId) + "," +
				JsonSetString("username", senderName) + "}";

			m_dbWrapper.bSaveOfflineMessage(senderId, reciverId, offlineMsg);
		}

		std::string resp = JsonMakeObject({
			JsonSetString("success","waiting")
			});
		SendToClient(pSocket, MsgType::FRIEND_ADD_RESP, resp);
		});

	m_dispatcher.on(MsgType::FRIEND_ACCEPT, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;
		int accepterId = JsonGetInt(json, "user_id");
		int requesterId = JsonGetInt(json, "request_user_id");

		// DB 操作（m_csDb 自动加锁），内存 map 单独加锁（避免嵌套持锁）
		bool bAddSuccess = false;
		std::string strAccepterName, strRequesterName;
		CConnectSocket* pRequesterSocket = nullptr;

		// 1. DB 查询（m_csDb）
		if (!m_dbWrapper.bIsFriend(requesterId, accepterId)) {
			bAddSuccess = m_dbWrapper.bAddFriend(requesterId, accepterId);
		}

		// 2. 查 map（m_csData，不在 DB 锁内）
		{
			CSingleLock lock(&m_csData, TRUE);
			auto it = m_userIdToSocket.find(requesterId);
			pRequesterSocket = it != m_userIdToSocket.end() ? it->second : nullptr;
			strRequesterName = m_userIdToName[requesterId];
			strAccepterName = m_userIdToName[accepterId];
		}
		if (bAddSuccess) {
			std::string requesterOnline = pRequesterSocket ? "true" : "false";
			std::string notify = JsonMakeObject({
				JsonSetString("success", "true"),
				JsonSetInt("friend_id", accepterId),
				JsonSetString("friend_username", strAccepterName),
				JsonSetString("online", "true")
				});
			// 发送方在线则通知，否则不通知，上线自动会获取
			if (pRequesterSocket) {
				SendToClient(pRequesterSocket, MsgType::FRIEND_ADD_RESP, notify);
			}
			// 接收方此时一定在线
			std::string resp = JsonMakeObject({
				JsonSetString("success","true"),
				JsonSetInt("friend_id", requesterId),
				JsonSetString("friend_username", strRequesterName),
				JsonSetString("online",requesterOnline)
				});
			SendToClient(pSocket, MsgType::FRIEND_ADD_RESP, resp);
		}
		else {
			std::string resp = JsonMakeObject({
			JsonSetString("success", "false")
				});
			SendToClient(pSocket, MsgType::FRIEND_ADD_RESP, resp);
		}

		});

	m_dispatcher.on(MsgType::FRIEND_REJECT, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;
		int rejecterId = JsonGetInt(json, "user_id");
		int requesterId = JsonGetInt(json, "request_user_id");
		std::string rejecterName = "";
		CConnectSocket* pRequesterSocket = nullptr;
		{
			CSingleLock lock(&m_csData, TRUE);
			rejecterName = m_userIdToName[rejecterId];
			auto it = m_userIdToSocket.find(requesterId);
			pRequesterSocket = it != m_userIdToSocket.end() ? it->second : nullptr;
		}

		if (pRequesterSocket)
		{
			std::string resp = JsonMakeObject({
				JsonSetString("username", rejecterName),
				JsonSetString("success","false"),
				});
			SendToClient(pRequesterSocket, MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		std::string resp = JsonMakeObject({
			JsonSetString("type", "friend_add_resp"),
			JsonSetString("username", rejecterName),
			JsonSetString("success","false"),
			});

			m_dbWrapper.bSaveOfflineMessage(rejecterId, requesterId, resp);

		});

	m_dispatcher.on(MsgType::FRIEND_DEL, [this](const std::string& json) {
		int userId = JsonGetInt(json, "user_id");
		int friendId = JsonGetInt(json, "friend_id");

		// DB 删除好友（m_csDb 自动加锁），查 socket 单独加锁
		m_dbWrapper.bRemoveFriend(userId, friendId);

		CConnectSocket* pFriendSocket = nullptr;
		{
			CSingleLock lock(&m_csData, TRUE);
			auto it = m_userIdToSocket.find(friendId);
			pFriendSocket = (it != m_userIdToSocket.end()) ? it->second : nullptr;
		}


		// 通知被删好友更新列表
		if (pFriendSocket) {
			std::string notify = JsonMakeObject({
					JsonSetString("type", "friend_removed"),
					JsonSetInt("user_id", userId)
				});
			SendToClient(pFriendSocket, MsgType::FRIEND_DEL, notify);
		}

		});

	m_dispatcher.on(MsgType::FILE_REQUEST, [this](const std::string& json) {
		// TODO: 转发给 receiver
		});

	m_dispatcher.on(MsgType::FILE_RESP, [this](const std::string& json) {
		// TODO: 转发给 sender
		});

	m_dispatcher.on(MsgType::HISTORY, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;

		int userId = JsonGetInt(json, "user_id");
		int otherUserId = JsonGetInt(json, "other_user_id");
		int limit = JsonGetInt(json, "limit");
		if (limit <= 0 || limit > 100) limit = 20;

		std::vector<std::string> records;
		m_dbWrapper.bGetMessages(userId, otherUserId, limit, records);

		// 构造 JSON 数组返回
		std::string resp = "{\"messages\":[";
		for (size_t i = 0; i < records.size(); ++i) {
			resp += "\"" + records[i] + "\"";
			if (i != records.size() - 1) resp += ",";
		}
		resp += "]}";
		SendToClient(pSocket, MsgType::HISTORY, resp);
		});

	m_dispatcher.on(MsgType::OFFLINE_READ, [this](const std::string& json) {
		CConnectSocket* pSocket = g_pCurrentSocket;
		if (!pSocket) return;

		int userId = JsonGetInt(json, "user_id");
		auto arr = JsonGetArray(json, "msg_ids");
		for (const auto& item : arr) {
			int msgId = std::stoi(item);
			m_dbWrapper.bDeleteOfflineMessage(msgId);
		}
		});
}
/*
std::string CNetworkServerDlg::strGetFriendListJson(int iUserId)
{
	auto friendIds = m_dbWrapper.vecGetFriends(iUserId);
	std::string friendListJson = "\"friends\": [";
	for (size_t i = 0; i < friendIds.size(); ++i) {
		int fid = friendIds[i].first;
		std::string fname = friendIds[i].second;
		bool online = m_userIdToSocket.count(fid) > 0;
		friendListJson += JsonMakeObject({
			JsonSetInt("user_id", fid),
			JsonSetString("username", fname),
			JsonSetString("online", online ? "true" : "false")
		});
		if (i != friendIds.size() - 1) {
			friendListJson += ",";
		}
	}
	friendListJson += "]";
	return friendListJson;
}
*/
// ============================================================================
// 系统消息
// ============================================================================

void CNetworkServerDlg::OnSysCommand(UINT nID, LPARAM lParam) {
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void CNetworkServerDlg::OnPaint() {
	if (IsIconic()) {
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND,
			reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CDialogEx::OnPaint();
	}
}

HCURSOR CNetworkServerDlg::OnQueryDragIcon() {
	return static_cast<HCURSOR>(m_hIcon);
}

void CNetworkServerDlg::OnStnClickedStaticPort() {}
void CNetworkServerDlg::OnEnChangeEditPort() {}

// ============================================================================
// 按钮事件
// ============================================================================

void CNetworkServerDlg::OnBnClickedButtonStart() {
	CString strPort;
	GetDlgItemText(IDC_EDIT_PORT, strPort);
	int port = _ttoi(strPort);
	if (port < 0 || port > 65535) {
		AfxMessageBox(_T("请输入有效的端口号（0-65535）"));
		return;
	}

	m_bRunning = TRUE;
	m_listenSocket.m_pDlg = this;

	if (!m_listenSocket.Start(port)) {
		AfxMessageBox(_T("启动监听失败"));
		m_bRunning = FALSE;
		return;
	}

	UpdateStatus(_T("正在监听..."));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
	UpdateLog(_T("[启动] 开始监听端口 ") + strPort);
}

void CNetworkServerDlg::OnBnClickedButtonStop() {
	InterlockedExchange(&m_bRunning, FALSE);

	// 阶段 1：停止接受新连接
	m_listenSocket.Stop();

	// 阶段 2：复制列表 → 逐个关闭（不持锁调用 Stop，避免死锁）
	std::vector<CConnectSocket*> sockets;
	{
		CSingleLock lock(&m_csData, TRUE);
		sockets = m_connectSockets;
	}
	for (auto* s : sockets) {
		s->Stop();  // 阻塞等线程退出；线程自 delete this
		// s 现在是悬空指针，不能再访问！
	}

	// 阶段 3：清空所有 maps
	{
		CSingleLock lock(&m_csData, TRUE);
		m_connectSockets.clear();
		m_userIdToSocket.clear();
		m_socketToUserId.clear();
		m_userIdToName.clear();
	}

	m_userList.DeleteAllItems();
	UpdateStatus(_T("空闲"));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
	UpdateLog(_T("[停止] 已停止监听"));
}

// ============================================================================
// 公开方法
// ============================================================================

void CNetworkServerDlg::OnUserLoginUI(int userId,
	const std::string& username,
	const std::string& ip) {


	int idx = m_userList.InsertItem(m_userList.GetItemCount(),
		CString(username.c_str()));
	m_userList.SetItemText(idx, 1, CString(ip.c_str()));
	m_userList.SetItemData(idx, static_cast<DWORD_PTR>(userId));

	UpdateLog(_T("[上线] ") + CString(username.c_str()) +
		_T(" (") + CString(ip.c_str()) + _T(")"));
}

void CNetworkServerDlg::OnUserLogoutUI(int userId, const std::string& username) {
	// 不再读任何 map —— 用户名通过参数传入（由 PostUIUpdate 携带）
	UpdateLog(_T("[下线] ") + CString(username.c_str()));

	// 从列表控件移除（纯 UI 操作，安全）
	int count = m_userList.GetItemCount();
	for (int i = 0; i < count; i++) {
		if (static_cast<int>(m_userList.GetItemData(i)) == userId) {
			m_userList.DeleteItem(i);
			break;
		}
	}
}

void CNetworkServerDlg::UpdateLog(const CString& str) {
	CString strLog;
	GetDlgItemText(IDC_EDIT_LOG, strLog);
	strLog += str + _T("\r\n");
	SetDlgItemText(IDC_EDIT_LOG, strLog);

	int nLen = GetDlgItem(IDC_EDIT_LOG)->SendMessage(WM_GETTEXTLENGTH);
	GetDlgItem(IDC_EDIT_LOG)->SendMessage(EM_SETSEL, nLen, nLen);
	GetDlgItem(IDC_EDIT_LOG)->SendMessage(EM_SCROLLCARET);
}

void CNetworkServerDlg::UpdateStatus(const CString& text) {
	SetDlgItemText(IDC_STATIC_STATUS, text);
}

void CNetworkServerDlg::DispatchMessage(CConnectSocket* pSocket, MsgType msgType, const std::string& json)
{
	// 记录日志（通过 PostMessage 安全投递到 UI 线程）
	PostUIUpdate(UIUpdateType::LOG,
		CString(_T("[收到] 类型="))
		+ CString(std::to_string(static_cast<uint32_t>(msgType)).c_str())
		+ _T(" 内容=") + CString(json.c_str()));

	// 分发到 handler
	// g_pCurrentSocket 在 RecvLoop 中已设置，handler 通过它获取当前 socket
	m_dispatcher.dispatch(msgType, json);
}

void CNetworkServerDlg::SendToClient(CConnectSocket* pSocket, MsgType msgType, const std::string& json)
{
	if (!pSocket) return;
	std::vector<uint8_t> data = ProtocolEncode(msgType, json);
	pSocket->SendData(data.data(), static_cast<int>(data.size()));
}

void CNetworkServerDlg::OnClientDisconnected(CConnectSocket* pSocket)
{
	// 防重复清理（Stop() 和自然断开可能同时触发）
	{
		CSingleLock lock(&m_csData, TRUE);
		if (pSocket->m_bDisconnected) return;
		pSocket->m_bDisconnected = true;
	}

	int userId = -1;
	std::string username;

	// 第一阶段：加锁提取用户信息，从 maps 移除
	{
		CSingleLock lock(&m_csData, TRUE);
		auto it = m_socketToUserId.find(pSocket);
		if (it != m_socketToUserId.end()) {
			userId = it->second;
			username = m_userIdToName[userId];
			m_socketToUserId.erase(it);
			m_userIdToSocket.erase(userId);
			m_userIdToName.erase(userId);
		}

		auto it2 = std::find(m_connectSockets.begin(), m_connectSockets.end(), pSocket);
		if (it2 != m_connectSockets.end()) {
			m_connectSockets.erase(it2);
		}
	}  // 锁释放

	// 第二阶段：UI 更新（无需锁）
	if (userId >= 0) {
		PostUIUpdate(UIUpdateType::LOGOUT, _T(""), userId, username, "");
	}

	// 第三阶段：广播离线（加锁复制列表 → 解锁 → 逐个发送）
	if (userId >= 0) {
		std::vector<CConnectSocket*> targets;
		{
			CSingleLock lock(&m_csData, TRUE);
			for (auto& pair : m_userIdToSocket) {
				targets.push_back(pair.second);
			}
		}
		std::string offlineMsg = JsonMakeObject({
			JsonSetInt("user_id", userId),
			JsonSetString("username", username)
			});
		for (auto* target : targets) {
			SendToClient(target, MsgType::STATUS_OFFLINE, offlineMsg);
		}
	}

	// 注意：不 delete pSocket！线程在 ThreadProc 返回前会 delete this
}

void CNetworkServerDlg::PostUIUpdate(UIUpdateType type, const CString& text, int userId, const std::string& username, const std::string& ip)
{
	UIUpdateData* pData = new UIUpdateData;
	pData->type = type;
	pData->text = text;
	pData->userId = userId;
	pData->username = username;
	pData->ip = ip;
	PostMessage(WM_USER_UPDATE_UI, static_cast<WPARAM>(type),
		reinterpret_cast<LPARAM>(pData));
}

LRESULT CNetworkServerDlg::OnUIUpdate(WPARAM wParam, LPARAM lParam)
{
	UIUpdateData* pData = reinterpret_cast<UIUpdateData*>(lParam);
	if (!pData) return 0;

	switch (static_cast<UIUpdateType>(wParam)) {
	case UIUpdateType::LOG:
		UpdateLog(pData->text);
		break;
	case UIUpdateType::LOGIN:
		OnUserLoginUI(pData->userId, pData->username, pData->ip);
		break;
	case UIUpdateType::LOGOUT:
		OnUserLogoutUI(pData->userId, pData->username);
		break;
	case UIUpdateType::STATUS_TEXT:
		UpdateStatus(pData->text);
		break;
	}
	delete pData;
	return 0;
}

void CNetworkServerDlg::OnDestroy()
{
	if (InterlockedCompareExchange(&m_bRunning, TRUE, TRUE) == TRUE) {
		OnBnClickedButtonStop();
	}
	CDialogEx::OnDestroy();
}

