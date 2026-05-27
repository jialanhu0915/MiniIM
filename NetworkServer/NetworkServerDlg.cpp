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
#include "afxdialogex.h"
#include "SQLiteWrapper.h"
#include "../Common/JsonUtils.h"

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
	//ON_BN_CLICKED(IDC_BUTTON_SEND, &CNetworkServerDlg::OnBnClickedButtonSend)
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
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
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
	m_dispatcher.on(MsgType::LOGIN, [this](const std::string& json) {
		// TODO: 解析用户名
		// 1. 检查用户名是否已存在（DB）
		std::string username = JsonGetString(json, "username");
		if (username.empty()) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","用户名不能为空")
				});
			SendToClient(m_currentSocket, MsgType::LOGIN_RESP, resp);
			return;
		}

		// 2. 分配 user_id
		int iUserID = m_dbWrapper.iAddUser(username);


		// 3. 发送 LOGIN_RESP（含好友列表 + 离线消息）
		m_userIdToSocket[iUserID] = m_currentSocket;
		m_socketToUserId[m_currentSocket] = iUserID;
		m_userIdToName[iUserID] = username;
		OnUserLogin(iUserID, username, "unknown");

		std::string friendListJson = strGetFriendListJson(iUserID);
		std::string resp = "{" +
			JsonSetString("success", "true") + "," +
			JsonSetInt("user_id", iUserID) + "," +
			friendListJson + "}";
		SendToClient(m_currentSocket, MsgType::LOGIN_RESP, resp);
		
		// 4. 广播 STATUS_ONLINE 给其他在线用户
		std::string onlineMsg = "{" +
			JsonSetInt("user_id", iUserID) + "," +
			JsonSetString("username", username) + "}";
		for (auto& it : m_userIdToSocket) {
			if (it.first != iUserID) {
				SendToClient(it.second, MsgType::STATUS_ONLINE, onlineMsg);
			}
		}

		UpdateLog(_T("[登录] ") + CString(username.c_str()) +
			_T(" (ID=") + CString(std::to_string(iUserID).c_str()) + _T(")"));

		});

	m_dispatcher.on(MsgType::LOGOUT, [this](const std::string& json) {
		// TODO: 移除用户，通知其他用户 STATUS_OFFLINE
		if (!m_currentSocket) return;
		auto it = m_socketToUserId.find(m_currentSocket);
		if (it == m_socketToUserId.end()) return;

		int userId = it->second;
		std::string username = m_userIdToName[userId];

		// 广播下线通知
		std::string offlineMsg = "{" +
			JsonSetInt("user_id", userId) + "," +
			JsonSetString("username", username) + "}";

		for (auto& it : m_userIdToSocket) {
			if (it.first != userId) {
				SendToClient(it.second, MsgType::STATUS_OFFLINE, offlineMsg);
			}
		}
		// 从在线列表中移除
		m_userIdToSocket.erase(userId);
		m_socketToUserId.erase(m_currentSocket);
		m_userIdToName.erase(userId);
		OnUserLogout(userId);

		});

	m_dispatcher.on(MsgType::TEXT, [this](const std::string& json) {
		// TODO: 解析 sender_id, receiver_id, content
		// 1. 存 DB
		// 2. 如果 receiver 在线 → 转发
		// 3. 如果 receiver 离线 → 存为离线消息
		});

	m_dispatcher.on(MsgType::GROUP_TEXT, [this](const std::string& json) {
		// TODO: 广播给所有在线用户
		});

	m_dispatcher.on(MsgType::FRIEND_ADD, [this](const std::string& json) {
		// TODO: 添加好友关系（DB），通知双方
		int senderId = JsonGetInt(json, "user_id");
		std::string reciverUsername = JsonGetString(json, "friend_username");
		int reciverId = m_dbWrapper.iGetUserId(reciverUsername);
		if (reciverId == -1) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","用户不存在")
				});
			SendToClient(m_currentSocket, MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		// 检查是否已经是好友
		if (m_dbWrapper.bIsFriend(senderId, reciverId)) {
			std::string resp = JsonMakeObject({
				JsonSetString("success","false"),
				JsonSetString("reason","已经是好友了")
				});
			SendToClient(m_currentSocket, MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		// 通知接收方
		if (m_userIdToSocket.count(reciverId) == 1) {
			std::string notify = "{" +
				JsonSetInt("user_id", senderId) + "," +
				JsonSetString("username", m_userIdToName[senderId].c_str()) + "}";
			SendToClient(m_userIdToSocket[reciverId], MsgType::FRIEND_ADD, notify);
		}
		else
		{
			// 保存离线消息
			std::string offlineMsg = "{" +
				JsonSetString("type", "friend_add") + "," +
				JsonSetInt("user_id", senderId) + "," +
				JsonSetString("username", m_userIdToName[senderId].c_str()) + "}";

			m_dbWrapper.bSaveOfflineMessage(senderId, reciverId, offlineMsg);
		}

		std::string resp = JsonMakeObject({
			JsonSetString("success","waiting")
			});
		SendToClient(m_currentSocket, MsgType::FRIEND_ADD_RESP, resp);
		});

	m_dispatcher.on(MsgType::FRIEND_ACCEPT, [this](const std::string& json) {
		// TODO: 接受好友请求，添加好友关系（DB），通知双方
		int accepterId = JsonGetInt(json, "user_id");
		int requesterId = JsonGetInt(json, "request_user_id");

		if (m_dbWrapper.bAddFriend(requesterId, accepterId)) {
			// 发送方在线则通知，否则不通知，上线自动会获取
			std::string requesterOnline = "false";
			if (m_userIdToSocket.count(requesterId) > 0) {
				requesterOnline = "true";
				std::string notify = JsonMakeObject({
					JsonSetString("success", "true"),
					JsonSetInt("friend_id", accepterId),
					JsonSetString("friend_username", m_userIdToName[accepterId]),
					JsonSetString("online", "true")
					});
				SendToClient(m_userIdToSocket[requesterId], MsgType::FRIEND_ADD_RESP, notify);
			}
			// 接收方此时一定在线
			std::string resp = JsonMakeObject({
				JsonSetString("success","true"),
				JsonSetInt("friend_id", requesterId),
				JsonSetString("friend_username", m_userIdToName[requesterId].c_str()),
				JsonSetString("online",requesterOnline)
				});
			SendToClient(m_currentSocket, MsgType::FRIEND_ADD_RESP, resp);
		}
		else {
			std::string resp = JsonMakeObject({
				JsonSetString("success", "false"),
				JsonSetString("reason", "添加好友失败，请重试")
				});
			SendToClient(m_currentSocket, MsgType::FRIEND_ADD_RESP, resp);
		}
		});
	
	m_dispatcher.on(MsgType::FRIEND_REJECT, [this](const std::string& json) {
		// TODO: 拒绝好友请求，通知请求方
		int rejecterId = JsonGetInt(json, "user_id");
		int requesterId = JsonGetInt(json, "request_user_id");
		std::string rejecterName = m_userIdToName[rejecterId];

		if (m_userIdToSocket.count(requesterId) > 0)
		{
			std::string resp = JsonMakeObject({
				JsonSetString("username", rejecterName.c_str()),
				JsonSetString("success","false"),
				});
			SendToClient(m_userIdToSocket[requesterId], MsgType::FRIEND_ADD_RESP, resp);
			return;
		}

		std::string resp = JsonMakeObject({
			JsonSetString("type", "friend_add_resp"),
			JsonSetString("username", rejecterName.c_str()),
			JsonSetString("success","false"),
			});

		m_dbWrapper.bSaveOfflineMessage(rejecterId, requesterId, resp);

		});

	m_dispatcher.on(MsgType::FRIEND_DEL, [this](const std::string& json) {
		int userId = JsonGetInt(json, "user_id");
		int friendId = JsonGetInt(json, "friend_id");
		m_dbWrapper.bRemoveFriend(userId, friendId);

		// 通知被删好友更新列表
		if (m_userIdToSocket.count(friendId) > 0) {
			std::string notify = JsonMakeObject({
					JsonSetString("type", "friend_removed"),
					JsonSetInt("user_id", userId)
				});
			SendToClient(m_userIdToSocket[friendId], MsgType::FRIEND_DEL, notify);
		}
		});

	m_dispatcher.on(MsgType::FILE_REQUEST, [this](const std::string& json) {
		// TODO: 转发给 receiver
		});

	m_dispatcher.on(MsgType::FILE_RESP, [this](const std::string& json) {
		// TODO: 转发给 sender
		});

	m_dispatcher.on(MsgType::HISTORY, [this](const std::string& json) {
		// TODO: 从 DB 查询聊天记录，返回给请求方
		});

	m_dispatcher.on(MsgType::OFFLINE_READ, [this](const std::string& json) {
		// TODO: 标记离线消息已读
		});
}

std::string CNetworkServerDlg::strGetFriendListJson(int iUserId)
{
	auto friendIds = m_dbWrapper.vecGetFriends(iUserId);
	std::string friendListJson = "\"friends\": [";
	for (size_t i = 0; i < friendIds.size(); ++i) {
		int fid = friendIds[i].first;
		std::string fname = friendIds[i].second;
		bool online = m_userIdToSocket.count(fid) > 0;
		friendListJson += "{" +
			JsonSetInt("user_id", fid) + "," +
			JsonSetString("username", fname) + "," +
			JsonSetString("online", online ? "true" : "false") + "}";
		if (i != friendIds.size() - 1) {
			friendListJson += ",";
		}
	}
	friendListJson += "]";
	return friendListJson;
}

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

	if (!m_listenSocket.Create(port)) {
		AfxMessageBox(_T("创建监听套接字失败"));
		return;
	}

	m_listenSocket.Listen();
	UpdateStatus(_T("正在监听..."));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(FALSE);
	UpdateLog(_T("[启动] 开始监听端口 ") + strPort);
}

void CNetworkServerDlg::OnBnClickedButtonStop() {
	for (CConnectSocket* p : m_connectSockets) {
		p->Close();
		delete p;
	}
	m_connectSockets.clear();
	m_listenSocket.Close();
	m_userIdToSocket.clear();
	m_socketToUserId.clear();
	m_userIdToName.clear();
	// 清空在线用户
	m_userList.DeleteAllItems();

	UpdateStatus(_T("空闲"));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
	UpdateLog(_T("[停止] 已停止监听"));
}
/*
void CNetworkServerDlg::OnBnClickedButtonSend() {
	CString strMessage;
	GetDlgItemText(IDC_EDIT_MSG, strMessage);
	if (strMessage.IsEmpty()) {
		AfxMessageBox(_T("请输入要发送的消息"));
		return;
	}

	CT2A asciiMsg(strMessage);
	int nSent = m_connectSocket.Send((LPCSTR)asciiMsg,
		static_cast<int>(strlen(asciiMsg)));
	if (nSent == SOCKET_ERROR) {
		AfxMessageBox(_T("发送消息失败"));
		UpdateLog(_T("[错误] 发送消息失败"));
		return;
	}
	UpdateLog(_T("[发送] ") + strMessage);
	SetDlgItemText(IDC_EDIT_MSG, _T(""));
}
*/

// ============================================================================
// 网络回调
// ============================================================================

void CNetworkServerDlg::OnAccept() {
	CConnectSocket* pSocket = new CConnectSocket();
	pSocket->m_pDlg = this;

	if (m_listenSocket.Accept(*pSocket)) {
		m_connectSockets.push_back(pSocket);
		UpdateLog(_T("[连接] 新客户端接入"));
		UpdateStatus(_T("已连接"));
		GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
		GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
	}
	else {
		delete pSocket;
		UpdateLog(_T("[错误] Accept 失败"));
	}
}

// ============================================================================
// 公开方法
// ============================================================================

void CNetworkServerDlg::OnUserLogin(int userId,
	const std::string& username,
	const std::string& ip) {
	

	int idx = m_userList.InsertItem(m_userList.GetItemCount(),
		CString(username.c_str()));
	m_userList.SetItemText(idx, 1, CString(ip.c_str()));
	m_userList.SetItemData(idx, static_cast<DWORD_PTR>(userId));

	UpdateLog(_T("[上线] ") + CString(username.c_str()) +
		_T(" (") + CString(ip.c_str()) + _T(")"));
}

void CNetworkServerDlg::OnUserLogout(int userId) {
	auto it = m_userIdToSocket.find(userId);
	if (it != m_userIdToSocket.end()) {
		UpdateLog(_T("[下线] ") + CString(m_userIdToName[it->first].c_str()));
	}

	// 从列表控件移除
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

void CNetworkServerDlg::OnMessageReceived(CConnectSocket* pSocket, MsgType msgType, const std::string& json)
{
	m_currentSocket = pSocket;
	UpdateLog(_T("[收到] 类型=") + 
		CString(std::to_string(static_cast<uint32_t>(msgType)).c_str()) +
		_T(" 内容=") + CString(json.c_str()) );
	m_dispatcher.dispatch(msgType, json);
	m_currentSocket = nullptr;
}

void CNetworkServerDlg::SendToClient(CConnectSocket * pSocket, MsgType msgType, const std::string & json)
{
	std::vector<uint8_t> data = ProtocolEncode(msgType, json);
	pSocket->Send(data.data(), static_cast<int>(data.size()));
}

void CNetworkServerDlg::OnClientDisconnected(CConnectSocket* pSocket)
{
	auto it = m_socketToUserId.find(pSocket);

	if (it != m_socketToUserId.end()) {
		int userId = it->second;
		std::string username = m_userIdToName[userId];
		OnUserLogout(userId);
		m_socketToUserId.erase(it);
		m_userIdToSocket.erase(userId);
		m_userIdToName.erase(userId);

		std::string offlineMsg = JsonMakeObject({
			JsonSetInt("user_id", userId),
			JsonSetString("username", username)
			});

		for (auto& pair : m_userIdToSocket) {
			SendToClient(pair.second, MsgType::STATUS_OFFLINE, offlineMsg);
		}
	}

	auto it2 = std::find(m_connectSockets.begin(), m_connectSockets.end(), pSocket);
	if (it2 != m_connectSockets.end()) {
		m_connectSockets.erase(it2);
	}

	pSocket->Close();
	delete pSocket;
}


