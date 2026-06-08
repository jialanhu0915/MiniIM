/**
 * @file   NetworkClientDlg.cpp
 * @brief  即时通讯客户端 - 主界面实现
 * @author Yan Runxin
 * @date   2026-05-25
 */

#include "pch.h"
#include "framework.h"
#include "NetworkClient.h"
#include "NetworkClientDlg.h"
#include "afxdialogex.h"
#include "../Common/JsonUtils.h"
#include "FtpHelper.h"
#include "CAddFriendDlg.h"

#include <ctime>
#include <string>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

 // ============================================================================
 // CAboutDlg
 // ============================================================================

/**
 * @brief 关于对话框
 */
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()
};

/**
 * @brief 默认构造
 */
CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

/**
 * @brief MFC 数据交换（DDX）
 * @param pDX 数据交换上下文
 */
void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// ============================================================================
// CNetworkClientDlg — 消息映射
// ============================================================================

BEGIN_MESSAGE_MAP(CNetworkClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CTLCOLOR()
	ON_EN_CHANGE(IDC_EDIT_PORT, &CNetworkClientDlg::OnEnChangeEditPort)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CNetworkClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CNetworkClientDlg::OnBnClickedButtonDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CNetworkClientDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_BUTTON_SENDFILE, &CNetworkClientDlg::OnBnClickedButtonSendFile)
	ON_BN_CLICKED(IDC_BUTTON_ADDFRIEND, &CNetworkClientDlg::OnBnClickedButtonAddFriend)
	ON_BN_CLICKED(IDC_BUTTON_REMOVEFRIEND, &CNetworkClientDlg::OnBnClickedButtonRemoveFriend)
	ON_BN_CLICKED(IDCANCEL, &CNetworkClientDlg::OnCancel)
	ON_EN_CHANGE(IDC_EDIT_MSG, &CNetworkClientDlg::OnEnChangeEditMsg)
	ON_LBN_SELCHANGE(IDC_LIST_FRIENDS, &OnSelChangeListFriends)
END_MESSAGE_MAP()

// ============================================================================
// 构造 / 初始化
// ============================================================================

/**
 * @brief 默认构造函数
 */
CNetworkClientDlg::CNetworkClientDlg(CWnd* pParent)
	: CDialogEx(IDD_NETWORKCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_connectSocket.m_pDlg = this;
}

/**
 * @brief MFC 数据交换（DDX/DDV）
 * @param pDX 数据交换上下文
 */
void CNetworkClientDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_USERNAME, m_editUsername);
	DDX_Control(pDX, IDC_LIST_FRIENDS, m_friendList);
	DDX_Control(pDX, IDC_LIST_LOG, m_logList);
	DDX_Control(pDX, IDC_BUTTON_ADDFRIEND, m_btnAddFriend);
	DDX_Control(pDX, IDC_BUTTON_REMOVEFRIEND, m_btnRemoveFriend);
	DDX_Control(pDX, IDC_BUTTON_SENDFILE, m_btnSendFile);
}

/**
 * @brief 对话框初始化，创建控件
 * @return TRUE
 */
BOOL CNetworkClientDlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	// 系统菜单
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
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	m_btnSendFile.EnableWindow(FALSE);
	SetDlgItemText(IDC_STATIC_STATUS, _T("未连接"));

	// ---- 按钮美化：扁平 + 柔和配色（Material 300 浅色板）----
	// 关键：m_bDontUseWinXPTheme = TRUE 让 CMFCButton 走自绘，SetFaceColor 才生效
	m_btnAddFriend.m_bDontUseWinXPTheme = TRUE;
	m_btnAddFriend.m_bDrawFocus = FALSE;
	m_btnAddFriend.SetFaceColor(RGB(100, 181, 246), RGB(60, 140, 200));   // 柔和蓝
	m_btnAddFriend.SetTextColor(RGB(255, 255, 255));
	m_btnRemoveFriend.m_bDontUseWinXPTheme = TRUE;
	m_btnRemoveFriend.m_bDrawFocus = FALSE;
	m_btnRemoveFriend.SetFaceColor(RGB(229, 115, 115), RGB(200, 90, 90)); // 柔和红
	m_btnRemoveFriend.SetTextColor(RGB(255, 255, 255));
	m_btnSendFile.m_bDontUseWinXPTheme = TRUE;
	m_btnSendFile.m_bDrawFocus = FALSE;
	m_btnSendFile.SetFaceColor(RGB(129, 199, 132), RGB(90, 165, 95));     // 柔和绿
	m_btnSendFile.SetTextColor(RGB(255, 255, 255));

	// ---- 初始化日志/聊天列表（每行一条，避免 CEdit 多行渲染 overlap）----
	// 列宽 800 比控件宽 275 大，确保 WS_HSCROLL 出滚动条，长 JSON 也能横向看完
	m_logList.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_logList.InsertColumn(0, _T(""), LVCFMT_LEFT, 800);  // LVS_NOCOLUMNHEADER 隐藏列头
	// SysListView32 默认用自己的字体（不含 CJK glyph 会显示成 tofu 豆腐块），
	// 强制继承对话框字体，对话框字体在中文系统下会被映射到 Microsoft YaHei UI
	m_logList.SetFont(GetFont());

	// ---- 注册协议处理器（示例） ----
	RegisterProtocolHandlers();

	return TRUE;
}

/**
 * @brief 按键预处理（拦截 Enter 发送消息）
 * @param pMsg 消息结构体
 * @return TRUE 已处理，FALSE 继续传递
 */
BOOL CNetworkClientDlg::PreTranslateMessage(MSG* pMsg) {
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
		// 如果焦点在消息发送框（IDC_EDIT_MSG），触发发送
		CWnd* pFocus = GetFocus();
		if (pFocus && pFocus->GetDlgCtrlID() == IDC_EDIT_MSG) {
			OnBnClickedButtonSend();
			return TRUE;  // 已处理，不继续传递
		}
		// 其他编辑框：吞掉 Enter，不做任何事
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

// ============================================================================
// 协议处理器注册（示例框架，你的网络层代码参考）
// ============================================================================
/**
 * @brief 注册所有协议消息处理器
 */
void CNetworkClientDlg::RegisterProtocolHandlers() {
	// 下面是你收到不同类型消息时的处理逻辑框架。
	// 实际代码中，你在 OnReceive 里把 byte 喂给 m_recvBuf，
	// 然后用 m_dispatcher.dispatch() 来触发这些回调。

	m_dispatcher.on(MsgType::LOGIN_RESP, [this](const std::string& json) {
		std::string success = JsonGetString(json, "success");
		if (success == "true") {
			int userId = JsonGetInt(json, "user_id");
			std::string username = JsonGetString(json, "username");
			// 解析好友列表数组
			std::vector<FriendInfo> friends;
			auto arr = JsonGetArray(json, "friends");
			for (const auto& item : arr) {
				FriendInfo f;
				f.userId = JsonGetInt(item, "user_id");
				f.username = JsonGetString(item, "username");
				f.online = JsonGetString(item, "online") == "true";
				friends.push_back(f);
			}

			OnLoginSuccess(userId, username, friends);
		}

		});

	// 共享 helper：按 senderId 解析展示名（自己 / 好友 / 未知用户）
	auto resolveName = [this](int senderId) -> std::string {
		if (senderId == m_userId) return "我";
		auto it = m_friendMap.find(senderId);
		if (it != m_friendMap.end()) return it->second.username;
		return "用户" + std::to_string(senderId);
	};

	m_dispatcher.on(MsgType::TEXT, [this, resolveName](const std::string& json) {
		int senderId = JsonGetInt(json, "sender_id");
		std::string content = JsonGetString(json, "content");
		std::string timestamp = JsonGetString(json, "timestamp");

		OnMessageReceived(resolveName(senderId), content, timestamp);
		});

	m_dispatcher.on(MsgType::GROUP_TEXT, [this, resolveName](const std::string& json) {
		int senderId = JsonGetInt(json, "sender_id");
		std::string content = JsonGetString(json, "content");
		std::string timestamp = JsonGetString(json, "timestamp");

		OnMessageReceived(resolveName(senderId), content, timestamp);
		});

	m_dispatcher.on(MsgType::STATUS_ONLINE, [this](const std::string& json) {
		int userId = JsonGetInt(json, "user_id");
		std::string username = JsonGetString(json, "username");
		OnFriendStatusChanged(userId, username, true);
		});

	m_dispatcher.on(MsgType::STATUS_OFFLINE, [this](const std::string& json) {
		int userId = JsonGetInt(json, "user_id");
		std::string username = JsonGetString(json, "username");
		OnFriendStatusChanged(userId, username, false);
		});

	m_dispatcher.on(MsgType::FRIEND_LIST, [this](const std::string& json) {
		std::vector<FriendInfo> friends;
		auto arr = JsonGetArray(json, "friends");
		for (const auto& item : arr) {
			FriendInfo f;
			f.userId = JsonGetInt(item, "user_id");
			f.username = JsonGetString(item, "username");
			f.online = JsonGetString(item, "online") == "true";
			friends.push_back(f);
		}
		RefreshFriendList(friends);
		});

	m_dispatcher.on(MsgType::FILE_REQUEST, [this](const std::string& json) {
		// 解析 FILE_REQUEST
		int senderId   = JsonGetInt(json, "sender_id");
		std::string filename = JsonGetString(json, "filename");
		long long filesize   = 0;
		// filesize 在 JSON 里是数字字面量，JsonGetInt 会返回 int，丢失精度
		// 这里简化处理：直接用 JsonGetInt 拿 int 即可（小文件够用）
		filesize = JsonGetInt(json, "filesize");
		std::string ftpPath  = JsonGetString(json, "ftp_path");

		// 1) 询问是否接受
		CString prompt;
		prompt.Format(_T("收到来自用户 %d 的文件：%hs（%lld 字节），是否接受？"),
			senderId, filename.c_str(), filesize);
		if (AfxMessageBox(prompt, MB_YESNO | MB_ICONQUESTION) != IDYES) {
			// 拒绝 → 发 FILE_RESP{accepted=false}
			std::string respJson = JsonMakeObject({
				JsonSetInt("sender_id", m_userId),        // 回给原发送方
				JsonSetInt("receiver_id", senderId),       // 原发送方 userId
				JsonSetString("filename", filename),
				JsonSetInt("accepted", 0),
				});
			auto data = ProtocolEncode(MsgType::FILE_RESP, respJson);
			m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
			AppendChatMessage(_T("[文件] 已拒绝接收 ") + CString(filename.c_str()));
			return;
		}

		// 2) 选保存路径
		CFileDialog saveDlg(FALSE, nullptr, CString(filename.c_str()),
			OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
			_T("所有文件 (*.*)|*.*||"), this);
		if (saveDlg.DoModal() != IDOK) {
			AppendChatMessage(_T("[文件] 已取消保存 ") + CString(filename.c_str()));
			return;
		}
		CString savePath = saveDlg.GetPathName();

		// 3) 从 FTP 下载
		FtpHelper ftp;
		if (!ftp.Connect(std::wstring(CStringW(m_serverIp)), 2121, L"anonymous", L"x")) {
			AfxMessageBox(_T("连接 FTP 失败！"));
			return;
		}
		if (!ftp.DownloadFile(std::wstring(CStringW(ftpPath.c_str())), std::wstring(CStringW(savePath)))) {
			CString err;
			err.Format(_T("下载失败：%s"), ftp.GetLastErrorMessage().c_str());
			AfxMessageBox(err);
			ftp.Disconnect();
			return;
		}
		ftp.Disconnect();

		// 4) 发 FILE_RESP{accepted=true} 给原发送方
		std::string respJson = JsonMakeObject({
			JsonSetInt("sender_id", m_userId),
			JsonSetInt("receiver_id", senderId),
			JsonSetString("filename", filename),
			JsonSetInt("accepted", 1),
			});
		auto data = ProtocolEncode(MsgType::FILE_RESP, respJson);
		m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

		AppendChatMessage(_T("[文件] 已保存: ") + savePath);
		});

	m_dispatcher.on(MsgType::FILE_RESP, [this](const std::string& json) {
		// 发送方收到接收方的反馈
		int accepted = JsonGetInt(json, "accepted");
		std::string filename = JsonGetString(json, "filename");
		if (accepted) {
			AppendChatMessage(_T("[文件] 对方已接收 ") + CString(filename.c_str()));
		} else {
			AppendChatMessage(_T("[文件] 对方拒绝接收 ") + CString(filename.c_str()));
		}
		});

	m_dispatcher.on(MsgType::HISTORY, [this, resolveName](const std::string& json) {
		auto arr = JsonGetArray(json, "messages");
		if (!arr.empty()) {
			AppendChatMessage(_T("[系统] 历史消息 ") +
				CString(std::to_string(arr.size()).c_str()) + _T(" 条"));
		}
		for (const auto& item : arr) {
			int senderId = JsonGetInt(item, "sender_id");
			std::string content = JsonGetString(item, "content");
			std::string timestamp = JsonGetString(item, "timestamp");

			// 时间戳只取 HH:MM:SS 部分（DB 返回的格式是 "YYYY-MM-DD HH:MM:SS"）
			std::string shortTime = timestamp;
			auto sp = timestamp.find(' ');
			if (sp != std::string::npos) {
				shortTime = timestamp.substr(sp + 1);  // "HH:MM:SS"
			}

			CString line;
			line.Format(_T("[%s] %s: %s"),
				CString(shortTime.c_str()),
				CString(resolveName(senderId).c_str()),
				CString(content.c_str()));
			AppendChatMessage(line);
		}
		});

	m_dispatcher.on(MsgType::OFFLINE_MSGS, [this](const std::string& json) {
		auto arr = JsonGetArray(json, "messages");
		if (arr.empty()) return;
		AppendChatMessage(_T("[系统] 收到 ") +
			CString(std::to_string(arr.size()).c_str()) +
			_T(" 条离线消息"));
		for (const auto& item : arr) {
			std::string content = JsonGetString(item, "content");
			AppendChatMessage(CString(CA2W(content.c_str(), CP_UTF8)));
		}
		});

	m_dispatcher.on(MsgType::FRIEND_DEL, [this](const std::string& json) {
		int friendId = JsonGetInt(json, "friend_id");
		RemoveFriendFromList(friendId);
		});

	m_dispatcher.on(MsgType::FRIEND_ADD_RESP, [this](const std::string& json) {
		std::string success = JsonGetString(json, "success");

		if (success == "true") {
			int friendId = JsonGetInt(json, "friend_id");
			std::string friendName = JsonGetString(json, "friend_username");
			bool online = JsonGetString(json, "online") == "true";
			AddFriendToList(friendId, friendName, online);
			AfxMessageBox(_T("好友添加成功！"));
		}
		else if (success == "waiting") {
			AfxMessageBox(_T("好友请求已发送，等待对方确认"));
		}
		else {
			AfxMessageBox(_T("好友添加失败: "));
		}

		});

	m_dispatcher.on(MsgType::FRIEND_ADD, [this](const std::string& json) {
		int senderId = JsonGetInt(json, "user_id");
		std::string senderName = JsonGetString(json, "username");

		CString msg;
		msg.Format(_T("%s 请求添加你为好友，是否接受？"), CString(senderName.c_str()));
		int ret = AfxMessageBox(msg, MB_YESNO | MB_ICONQUESTION);

		if (ret == IDYES) {
			std::string accept = JsonMakeObject({
					JsonSetInt("user_id", m_userId),
					JsonSetInt("request_user_id", senderId)
				});
			auto data = ProtocolEncode(MsgType::FRIEND_ACCEPT, accept);
			m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
		}
		else {
			std::string reject = JsonMakeObject({
					JsonSetInt("user_id", m_userId),
					JsonSetInt("request_user_id", senderId)
				});
			auto data = ProtocolEncode(MsgType::FRIEND_REJECT, reject);
			m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
		}
		});
}

// ============================================================================
// 已有系统消息处理
// ============================================================================

/**
 * @brief 系统命令处理
 * @param nID 命令 ID
 * @param lParam 命令参数
 */
void CNetworkClientDlg::OnSysCommand(UINT nID, LPARAM lParam) {
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

/**
 * @brief 窗口重绘处理
 */
void CNetworkClientDlg::OnPaint() {
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

/**
 * @brief 查询拖拽图标
 * @return HCURSOR 图标句柄
 */
HCURSOR CNetworkClientDlg::OnQueryDragIcon() {
	return static_cast<HCURSOR>(m_hIcon);
}

/**
 * @brief  控件颜色设置（让静态标签背景透明，露出对话框底色）
 * @param  pDC       设备上下文[in]
 * @param  pWnd      控件指针[in]
 * @param  nCtlColor 控件类型[in]
 * @return 背景画刷句柄
 * @note   1) 静态标签透明；2) IDC_STATIC_STATUS 走默认 brush（动态变化时需擦背景）
 *         3) 编辑框/列表框/对话框走默认，避免悬停变黑
 */
HBRUSH CNetworkClientDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
	if (nCtlColor == CTLCOLOR_STATIC) {
		// 状态文字会动态变化（未连接/已连接/已登录 xxx），
		// 若返回 NULL_BRUSH 会导致旧文字残影与新文字重叠
		if (pWnd->GetDlgCtrlID() == IDC_STATIC_STATUS) {
			return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
		}
		// 其他静态标签（如"服务器IP:"、"好友列表"）走透明
		pDC->SetBkMode(TRANSPARENT);
		return (HBRUSH)GetStockObject(NULL_BRUSH);
	}
	// 编辑框、列表框、对话框等走默认行为（保留正常背景色）
	return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

// ============================================================================
// 按钮事件
// ============================================================================

/**
 * @brief 连接/断开按钮点击处理
 */
void CNetworkClientDlg::OnBnClickedButtonConnect() {
	if (m_bConnecting) return;  // 正在连接中，忽略重复点击

	CString strIP, strPort, strUser;
	GetDlgItemText(IDC_IPADDRESS, strIP);
	GetDlgItemText(IDC_EDIT_PORT, strPort);
	m_editUsername.GetWindowText(strUser);

	int port = _ttoi(strPort);
	if (strIP.IsEmpty() || port <= 0 || port > 65535) {
		AfxMessageBox(_T("请输入有效的IP地址和端口！"));
		return;
	}
	if (strUser.IsEmpty()) {
		AfxMessageBox(_T("请输入用户名！"));
		return;
	}

	m_username = CT2A(strUser);

	m_connectSocket.Close();
	if (!m_connectSocket.Create()) {
		AfxMessageBox(_T("创建 socket 失败"));
		return;
	}
	m_connectSocket.Connect(strIP, port);

	m_serverIp = strIP;  // 记住服务端 IP，文件传输时要连同一个服务器的 2121 端口
	m_bConnecting = true;
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
	UpdateStatus(_T("正在连接..."));
	UpdateLog(_T("[连接] 正在连接 ") + strIP + _T(":") + strPort);
}

/**
 * @brief 断开服务器连接
 */
void CNetworkClientDlg::OnBnClickedButtonDisconnect() {
	// 通知服务端
	// 通知服务端
	if (m_userId > 0) {
		std::string json = JsonMakeObject({ JsonSetInt("user_id", m_userId) });
		auto data = ProtocolEncode(MsgType::LOGOUT, json);
		m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
	}

	m_bConnecting = false;
	m_connectSocket.Close();
	m_userId = -1;
	m_friendList.ResetContent();
	m_friendMap.clear();
	m_selectedFriendId = -1;
	SetDlgItemText(IDC_STATIC_STATUS, _T("未连接"));
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	m_btnSendFile.EnableWindow(FALSE);
	UpdateLog(_T("[断开] 已断开连接"));
}

/**
 * @brief 发送消息按钮点击处理
 */
void CNetworkClientDlg::OnBnClickedButtonSend() {
	CString strMsg;
	GetDlgItemText(IDC_EDIT_MSG, strMsg);
	if (strMsg.IsEmpty()) return;

	if (m_userId < 0) {
		AfxMessageBox(_T("请先登录！"));
		return;
	}

	// 未选中好友时提示（群聊 receiver_id=0 视作有效）
	if (m_selectedFriendId < 0) {
		AfxMessageBox(_T("请先选择一个好友！"));
		return;
	}

	MsgType msgType = (m_selectedFriendId == 0) ? MsgType::GROUP_TEXT : MsgType::TEXT;

	std::string content = CT2A(strMsg);

	// 客户端时间戳（秒）
	std::time_t now = std::time(nullptr);
	struct tm timeInfo;
	localtime_s(&timeInfo, &now);
	char timeBuf[32];
	std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeInfo);

	std::string json = JsonMakeObject({
		JsonSetInt("sender_id", m_userId),
		JsonSetInt("receiver_id", m_selectedFriendId),
		JsonSetString("content", content),
		JsonSetString("timestamp", timeBuf)
		});

	auto data = ProtocolEncode(msgType, json);
	m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

	// 本地显示自己发的消息
	AppendChatMessage(_T("[") + CString(timeBuf) + _T("] [我]: ") + strMsg);
	SetDlgItemText(IDC_EDIT_MSG, _T(""));
}

/**
 * @brief 发送文件按钮点击处理
 */
void CNetworkClientDlg::OnBnClickedButtonSendFile() {
	if (m_userId < 0 || m_selectedFriendId < 0) {
		AfxMessageBox(_T("请先选择一个在线好友！"));
		return;
	}
	if (m_serverIp.IsEmpty()) {
		AfxMessageBox(_T("尚未连接服务器！"));
		return;
	}

	CFileDialog dlg(TRUE, nullptr, nullptr,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("所有文件 (*.*)|*.*||"), this);
	if (dlg.DoModal() != IDOK) return;

	CString filePath = dlg.GetPathName();
	CString fileName = dlg.GetFileName();

	// 1) 本地获取文件大小
	CFileStatus status;
	if (!CFile::GetStatus(filePath, status)) {
		AfxMessageBox(_T("无法读取文件信息！"));
		return;
	}
	long long fileSize = static_cast<long long>(status.m_size);

	// 2) 上传到 FTP（端口 2121）
	//    远端文件名直接用 fileName 即可（CFtpSession 在 m_root 下定位）
	FtpHelper ftp;
	if (!ftp.Connect(std::wstring(CStringW(m_serverIp)), 2121, L"anonymous", L"x")) {
		CString err;
		err.Format(_T("连接 FTP 失败：%s"), ftp.GetLastErrorMessage().c_str());
		AfxMessageBox(err);
		return;
	}
	if (!ftp.UploadFile(std::wstring(CStringW(filePath)), std::wstring(CStringW(fileName)))) {
		CString err;
		err.Format(_T("上传文件失败：%s"), ftp.GetLastErrorMessage().c_str());
		AfxMessageBox(err);
		ftp.Disconnect();
		return;
	}
	ftp.Disconnect();

	// 3) 发 FILE_REQUEST 协议
	//    {"sender_id":int,"receiver_id":int,"filename":"...","filesize":int,"ftp_path":"..."}
	std::string fileNameA = CT2A(fileName);
	std::string json = JsonMakeObject({
		JsonSetInt("sender_id", m_userId),
		JsonSetInt("receiver_id", m_selectedFriendId),
		JsonSetString("filename", fileNameA),
		JsonSetLongLong("filesize", fileSize),  // 需 JsonUtils 支持 long long
		JsonSetString("ftp_path", fileNameA),
		});
	auto data = ProtocolEncode(MsgType::FILE_REQUEST, json);
	m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

	AppendChatMessage(_T("[文件] 已发送请求: ") + fileName +
		_T(" (") + CString(std::to_string(fileSize).c_str()) + _T(" 字节)"));
}

/**
 * @brief 添加好友按钮点击处理
 */
void CNetworkClientDlg::OnBnClickedButtonAddFriend() {
	if (m_userId < 0) {
		AfxMessageBox(_T("请先登录！"));
		return;
	}

	CAddFriendDlg dlg;
	if (dlg.DoModal() != IDOK) return;

	CString strName = dlg.m_strFriendName;
	strName.Trim();
	if (strName.IsEmpty()) return;

	// 不允许添加自己
	if (strName == CString(m_username.c_str())) {
		AfxMessageBox(_T("不能添加自己为好友！"));
		return;
	}

	std::string friendName = CT2A(strName);
	std::string json = JsonMakeObject({
			JsonSetInt("user_id", m_userId),
			JsonSetString("friend_username", friendName)
		});
	auto data = ProtocolEncode(MsgType::FRIEND_ADD, json);
	m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

	UpdateLog(_T("[好友] 发送添加好友请求: ") + strName);
}

/**
 * @brief 删除好友按钮点击处理
 */
void CNetworkClientDlg::OnBnClickedButtonRemoveFriend() {
	if (m_selectedFriendId <= 0) {
		AfxMessageBox(_T("请先选择一个好友！"));
		return;
	}

	int removedId = m_selectedFriendId;
	std::string json = JsonMakeObject({
		JsonSetInt("user_id", m_userId),
		JsonSetInt("friend_id", removedId)
	});
	auto data = ProtocolEncode(MsgType::FRIEND_DEL, json);
	m_connectSocket.Send(data.data(), static_cast<int>(data.size()));

	// 本地立即移除
	RemoveFriendFromList(removedId);
	UpdateLog(_T("[好友] 已发送删除好友请求"));
}

/**
 * @brief 对话框关闭（发送 LOGOUT 后关闭）
 */
void CNetworkClientDlg::OnCancel() {
	// 关闭前通知服务端，避免服务端误判异常断开
	if (m_userId > 0) {
		std::string json = JsonMakeObject({ JsonSetInt("user_id", m_userId) });
		auto data = ProtocolEncode(MsgType::LOGOUT, json);
		m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
	}
	m_connectSocket.Close();
	CDialogEx::OnCancel();
}

/**
 * @brief 消息编辑框内容变化
 * @note 预留，当前为空
 */
void CNetworkClientDlg::OnEnChangeEditMsg() {

}

/**
 * @brief 端口编辑框内容变化
 * @note 预留，当前为空
 */
void CNetworkClientDlg::OnEnChangeEditPort() {

}

/**
 * @brief 好友列表选中项变化处理
 */
void CNetworkClientDlg::OnSelChangeListFriends() {
	int idx = m_friendList.GetCurSel();
	if (idx == LB_ERR) return;

	m_selectedFriendId = static_cast<int>(m_friendList.GetItemData(idx));

	// 切换好友后自动请求与该好友的聊天历史（群聊没有持久化历史，跳过）
	if (m_userId > 0 && m_selectedFriendId > 0) {
		std::string json = JsonMakeObject({
			JsonSetInt("user_id", m_userId),
			JsonSetInt("other_user_id", m_selectedFriendId),
			JsonSetInt("limit", 50)
			});
		auto data = ProtocolEncode(MsgType::HISTORY, json);
		m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
	}
}

// ============================================================================
// 已有的网络回调（你在实现网络层时会调整这些）
// ============================================================================

/**
 * @brief 网络连接成功回调
 */
void CNetworkClientDlg::OnConnect() {
	m_bConnecting = false;
	UpdateStatus(_T("已连接"));
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
	UpdateLog(_T("[连接] TCP 连接成功"));

	// 连接成功后立即发送 LOGIN
	std::string json = "{\"username\":\"" + m_username + "\"}";
	std::vector<uint8_t> data = ProtocolEncode(MsgType::LOGIN, json);
	m_connectSocket.Send(data.data(), static_cast<int>(data.size()));
}

/**
 * @brief 网络连接失败回调
 * @param nErrorCode 错误码
 */
void CNetworkClientDlg::OnConnectError(int nErrorCode) {
	m_bConnecting = false;
	m_connectSocket.Close();
	UpdateStatus(_T("未连接"));
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	CString msg;
	msg.Format(_T("[连接] TCP 连接失败，错误码: %d"), nErrorCode);
	UpdateLog(msg);
	AfxMessageBox(_T("连接服务器失败，请检查 IP 和端口！"));
}

/**
 * @brief 接收到数据回调
 */
void CNetworkClientDlg::OnReceive() {
	/* 旧代码：
	char szBuf[1024] = { 0 };
	int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
	if (nRead > 0) {
		szBuf[nRead] = '\0';
		UpdateLog(_T("[接收] ") + CString(szBuf));
	}
	*/

	// 网络层：粘包/分帧处理 + 分发到 dispatcher
	char buf[4096];
	int n = m_connectSocket.Receive(buf, sizeof(buf));
	if (n > 0) {
		m_recvBuf.append(buf, n);
		uint32_t type, length;
		std::string json;
		while (m_recvBuf.next(type, length, json)) {
			// json 是 UTF-8 字节流，需要 CA2W 走 UTF-8 转换避免乱码
			UpdateLog(_T("[接收] 类型: ") + CString(std::to_string(type).c_str()) +
				_T(", 内容: ") + CString(CA2W(json.c_str(), CP_UTF8)));
			m_dispatcher.dispatch(static_cast<MsgType>(type), json);
		}
	}
}

/**
 * @brief 连接关闭回调
 */
void CNetworkClientDlg::OnClose() {
	m_bConnecting = false;
	m_connectSocket.Close();
	m_friendList.ResetContent();
	m_friendMap.clear();
	m_selectedFriendId = -1;
	UpdateStatus(_T("未连接"));
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	m_btnSendFile.EnableWindow(FALSE);
	UpdateLog(_T("[断开] 服务端已断开"));
}

/**
 * @brief 在日志列表追加一行文本
 * @param str 日志内容
 * @note   用 CListCtrl 替代原来的 CEdit，每行独立 row，从根本上避免
 *         CEdit 多行控件快速连续修改时的 paint 撕裂 / 文字重叠问题
 */
void CNetworkClientDlg::UpdateLog(const CString& str) {
	const int nRow = m_logList.InsertItem(m_logList.GetItemCount(), str);
	if (nRow >= 0) {
		m_logList.EnsureVisible(nRow, FALSE);
	}
}

// ============================================================================
// 公开方法 — 你的网络层调用这些来更新 UI
// ============================================================================

/**
 * @brief 登录成功后更新 UI（好友列表、状态）
 * @param userId 用户 ID
 * @param username 用户名
 * @param friends 好友列表
 */
void CNetworkClientDlg::OnLoginSuccess(int userId, const std::string& username,
	const std::vector<FriendInfo>& friends) {
	m_userId = userId;
	m_username = username;

	UpdateStatus(_T("已登录 — ") + CString(username.c_str()));
	AppendChatMessage(_T("[系统] 登录成功，欢迎 ") + CString(username.c_str()));

	RefreshFriendList(friends);

	// 登录后可用
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
	m_btnSendFile.EnableWindow(TRUE);
}

/**
 * @brief 好友上下线状态变化处理
 * @param userId 好友用户 ID
 * @param name 好友用户名
 * @param online 是否在线
 */
void CNetworkClientDlg::OnFriendStatusChanged(int userId,
	const std::string& name,
	bool online) {
	auto it = m_friendMap.find(userId);
	if (it != m_friendMap.end()) {
		it->second.online = online;
	}

	// 重建列表显示（简单实现；好友数量大时可优化）
	std::vector<FriendInfo> all;
	for (auto& kv : m_friendMap) all.push_back(kv.second);
	RefreshFriendList(all);

	CString status = online ? _T("上线") : _T("离线");
	UpdateLog(CString(name.c_str()) + _T(" ") + status);
}

/**
 * @brief 收到文本消息处理
 * @param senderName 发送者用户名
 * @param content 消息内容
 * @param timestamp 时间戳
 */
void CNetworkClientDlg::OnMessageReceived(const std::string& senderName,
	const std::string& content,
	const std::string& timestamp) {
	CString msg;
	msg.Format(_T("[%s] %s: %s"),
		CString(timestamp.c_str()),
		CString(senderName.c_str()),
		CString(content.c_str()));
	AppendChatMessage(msg);
}

/**
 * @brief 刷新好友列表控件
 * @param friends 好友信息数组
 */
void CNetworkClientDlg::RefreshFriendList(const std::vector<FriendInfo>& friends) {
	int prevSelected = m_selectedFriendId;

	m_friendList.ResetContent();
	m_friendMap.clear();

	for (const auto& f : friends) {
		AddFriendToList(f.userId, f.username, f.online);
	}

	// 也添加"全体"选项用于群聊
	int idx = m_friendList.AddString(_T("★ 全体（群聊）"));
	m_friendList.SetItemData(idx, 0);  // user_id=0 表示全体

	// 恢复选中（如果之前选中的好友还在列表里）
	if (prevSelected >= 0) {
		int count = m_friendList.GetCount();
		for (int i = 0; i < count; ++i) {
			if (static_cast<int>(m_friendList.GetItemData(i)) == prevSelected) {
				m_friendList.SetCurSel(i);
				break;
			}
		}
	}
}

/**
 * @brief 向列表中添加一个好友
 * @param userId 好友用户 ID
 * @param name 好友用户名
 * @param online 是否在线
 */
void CNetworkClientDlg::AddFriendToList(int userId, const std::string& name,
	bool online) {
	m_friendMap[userId] = { userId, name, online };

	CString display;
	if (online)
		display.Format(_T("● %s (在线)"), CString(name.c_str()));
	else
		display.Format(_T("○ %s (离线)"), CString(name.c_str()));

	int idx = m_friendList.AddString(display);
	m_friendList.SetItemData(idx, userId);
}

/**
 * @brief 从列表中移除一个好友
 * @param userId 好友用户 ID
 */
void CNetworkClientDlg::RemoveFriendFromList(int userId) {
	// 删除的就是当前选中的，清空选中
	if (m_selectedFriendId == userId) {
		m_selectedFriendId = -1;
	}
	m_friendMap.erase(userId);

	// 重建显示
	std::vector<FriendInfo> all;
	for (auto& kv : m_friendMap) all.push_back(kv.second);
	RefreshFriendList(all);
}

/**
 * @brief 更新状态栏文本
 * @param text 状态栏内容
 */
void CNetworkClientDlg::UpdateStatus(const CString& text) {
	SetDlgItemText(IDC_STATIC_STATUS, text);
}

/**
 * @brief 在聊天区追加一条消息
 * @param msg 聊天消息文本
 * @note   复用 m_logList，与 UpdateLog 走同一渲染路径，不再走 CEdit
 */
void CNetworkClientDlg::AppendChatMessage(const CString& msg) {
	const int nRow = m_logList.InsertItem(m_logList.GetItemCount(), msg);
	if (nRow >= 0) {
		m_logList.EnsureVisible(nRow, FALSE);
	}
}
