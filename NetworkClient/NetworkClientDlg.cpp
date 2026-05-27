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
// CNetworkClientDlg — 消息映射
// ============================================================================

BEGIN_MESSAGE_MAP(CNetworkClientDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_EN_CHANGE(IDC_EDIT_PORT, &CNetworkClientDlg::OnEnChangeEditPort)
    ON_BN_CLICKED(IDC_BUTTON_CONNECT,    &CNetworkClientDlg::OnBnClickedButtonConnect)
    ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CNetworkClientDlg::OnBnClickedButtonDisconnect)
    ON_BN_CLICKED(IDC_BUTTON_SEND,       &CNetworkClientDlg::OnBnClickedButtonSend)
    ON_BN_CLICKED(IDC_BUTTON_SENDFILE,   &CNetworkClientDlg::OnBnClickedButtonSendFile)
    ON_BN_CLICKED(IDC_BUTTON_ADDFRIEND,  &CNetworkClientDlg::OnBnClickedButtonAddFriend)
    ON_BN_CLICKED(IDC_BUTTON_REMOVEFRIEND, &CNetworkClientDlg::OnBnClickedButtonRemoveFriend)
    ON_BN_CLICKED(IDCANCEL,              &CNetworkClientDlg::OnCancel)
    ON_EN_CHANGE(IDC_EDIT_MSG,           &CNetworkClientDlg::OnEnChangeEditMsg)
END_MESSAGE_MAP()

// ============================================================================
// 构造 / 初始化
// ============================================================================

CNetworkClientDlg::CNetworkClientDlg(CWnd* pParent)
    : CDialogEx(IDD_NETWORKCLIENT_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_connectSocket.m_pDlg = this;
}

void CNetworkClientDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_USERNAME,      m_editUsername);
    DDX_Control(pDX, IDC_LIST_FRIENDS,        m_friendList);
    DDX_Control(pDX, IDC_BUTTON_ADDFRIEND,    m_btnAddFriend);
    DDX_Control(pDX, IDC_BUTTON_REMOVEFRIEND, m_btnRemoveFriend);
    DDX_Control(pDX, IDC_BUTTON_SENDFILE,     m_btnSendFile);
}

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

    // ---- 注册协议处理器（示例） ----
    RegisterProtocolHandlers();

    return TRUE;
}

// ============================================================================
// 协议处理器注册（示例框架，你的网络层代码参考）
// ============================================================================
void CNetworkClientDlg::RegisterProtocolHandlers() {
    // 下面是你收到不同类型消息时的处理逻辑框架。
    // 实际代码中，你在 OnReceive 里把 byte 喂给 m_recvBuf，
    // 然后用 m_dispatcher.dispatch() 来触发这些回调。

    m_dispatcher.on(MsgType::LOGIN_RESP, [this](const std::string& json) {
        // TODO: 解析 JSON，判断 success，调用 OnLoginSuccess()
        // 示例：
        // if (success) {
        //     OnLoginSuccess(userId, username, friends);
        //     UpdateStatus(_T("已登录"));
        // }
    });

    m_dispatcher.on(MsgType::TEXT, [this](const std::string& json) {
        // TODO: 解析 JSON，调用 OnMessageReceived()
    });

    m_dispatcher.on(MsgType::GROUP_TEXT, [this](const std::string& json) {
        // TODO: 解析 JSON，调用 OnMessageReceived()
    });

    m_dispatcher.on(MsgType::STATUS_ONLINE, [this](const std::string& json) {
        // TODO: 解析 JSON，调用 OnFriendStatusChanged(userId, name, true)
    });

    m_dispatcher.on(MsgType::STATUS_OFFLINE, [this](const std::string& json) {
        // TODO: 解析 JSON，调用 OnFriendStatusChanged(userId, name, false)
    });

    m_dispatcher.on(MsgType::FRIEND_LIST, [this](const std::string& json) {
        // TODO: 解析 JSON，调用 RefreshFriendList(friends)
    });

    m_dispatcher.on(MsgType::FILE_REQUEST, [this](const std::string& json) {
        // TODO: 弹出对话框询问用户是否接受
    });

    m_dispatcher.on(MsgType::FILE_RESP, [this](const std::string& json) {
        // TODO: 根据响应决定是否开始下载
    });

    m_dispatcher.on(MsgType::HISTORY, [this](const std::string& json) {
        // TODO: 解析 JSON，在聊天区加载历史消息
    });
}

// ============================================================================
// 已有系统消息处理
// ============================================================================

void CNetworkClientDlg::OnSysCommand(UINT nID, LPARAM lParam) {
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    } else {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}

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
    } else {
        CDialogEx::OnPaint();
    }
}

HCURSOR CNetworkClientDlg::OnQueryDragIcon() {
    return static_cast<HCURSOR>(m_hIcon);
}

// ============================================================================
// 按钮事件
// ============================================================================

void CNetworkClientDlg::OnBnClickedButtonConnect() {
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

    // TODO: 你的网络层代码
    // 1. 连接服务器
    // 2. 发送 LOGIN 消息：{"username":"..."}
    //
    // m_connectSocket.Create();
    // m_connectSocket.Connect(strIP, port);

    GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
    UpdateStatus(_T("正在连接..."));
    UpdateLog(_T("[连接] 正在连接 ") + strIP + _T(":") + strPort);
}

void CNetworkClientDlg::OnBnClickedButtonDisconnect() {
    // TODO: 你的网络层代码：发送 LOGOUT 消息，关闭 socket
    // m_connectSocket.Close();

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

void CNetworkClientDlg::OnBnClickedButtonSend() {
    CString strMsg;
    GetDlgItemText(IDC_EDIT_MSG, strMsg);
    if (strMsg.IsEmpty()) return;

    if (m_userId < 0) {
        AfxMessageBox(_T("请先登录！"));
        return;
    }

    // TODO: 你的网络层代码
    // 根据当前选中的好友构造 TEXT 或 GROUP_TEXT 消息并发送
    //
    // if (m_selectedFriendId > 0) {
    //     // 单聊
    //     auto data = ProtocolEncode(MsgType::TEXT,
    //         FormatJson("sender_id", m_userId, "receiver_id", m_selectedFriendId, ...));
    // } else {
    //     // 群聊（选中"全体"时）
    //     auto data = ProtocolEncode(MsgType::GROUP_TEXT, ...);
    // }

    // 本地显示自己发的消息
    AppendChatMessage(_T("[我] ") + strMsg);
    SetDlgItemText(IDC_EDIT_MSG, _T(""));
}

void CNetworkClientDlg::OnBnClickedButtonSendFile() {
    if (m_userId < 0 || m_selectedFriendId < 0) {
        AfxMessageBox(_T("请先选择一个在线好友！"));
        return;
    }

    CFileDialog dlg(TRUE, nullptr, nullptr,
                    OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                    _T("所有文件 (*.*)|*.*||"), this);
    if (dlg.DoModal() != IDOK) return;

    CString filePath = dlg.GetPathName();
    CString fileName = dlg.GetFileName();

    // TODO: 你的网络层代码
    // 1. 用 FtpHelper::UploadFile() 上传到 FTP 服务器
    // 2. 发送 FILE_REQUEST 消息通知对方：{"sender_id", "receiver_id", "filename", "filesize", "ftp_path"}

    AppendChatMessage(_T("[文件] 发送文件请求: ") + fileName);
}

void CNetworkClientDlg::OnBnClickedButtonAddFriend() {
    if (m_userId < 0) {
        AfxMessageBox(_T("请先登录！"));
        return;
    }

    // 从消息输入框读取好友用户名（输入格式：/addfriend <用户名>）
    CString strMsg;
    GetDlgItemText(IDC_EDIT_MSG, strMsg);
    strMsg.Trim();

    if (strMsg.IsEmpty()) {
        AfxMessageBox(_T("请先在消息框输入 /addfriend <用户名>，再点击添加好友"));
        GetDlgItem(IDC_EDIT_MSG)->SetFocus();
        return;
    }

    // 去掉可能的前缀
    CString strName = strMsg;
    if (strName.Find(_T("/addfriend ")) == 0)
        strName = strName.Mid(11);
    strName.Trim();

    if (strName.IsEmpty()) return;

    // TODO: 你的网络层代码
    // 发送 FRIEND_ADD 消息
    std::string friendName = CT2A(strName);
    UpdateLog(_T("[好友] 发送添加好友请求: ") + strName);
    SetDlgItemText(IDC_EDIT_MSG, _T(""));
}

void CNetworkClientDlg::OnBnClickedButtonRemoveFriend() {
    if (m_selectedFriendId < 0) {
        AfxMessageBox(_T("请先选择一个好友！"));
        return;
    }

    // TODO: 你的网络层代码
    // 发送 FRIEND_DEL 消息

    UpdateLog(_T("[好友] 删除好友请求"));
}

void CNetworkClientDlg::OnCancel() {
    CDialogEx::OnCancel();
}

void CNetworkClientDlg::OnEnChangeEditMsg() {}
void CNetworkClientDlg::OnEnChangeEditPort() {}

// ============================================================================
// 已有的网络回调（你在实现网络层时会调整这些）
// ============================================================================

void CNetworkClientDlg::OnConnect() {
    UpdateStatus(_T("已连接"));
    GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
    UpdateLog(_T("[连接] TCP 连接成功"));

    // TODO: 你的代码 — 连接成功后立即发送 LOGIN
}

void CNetworkClientDlg::OnReceive() {
    // TODO: 你的网络层代码
    //
    // char buf[4096];
    // int n = m_connectSocket.Receive(buf, sizeof(buf));
    // if (n > 0) {
    //     m_recvBuf.append(buf, n);
    //     uint32_t type, length;  std::string json;
    //     while (m_recvBuf.next(type, length, json)) {
    //         m_dispatcher.dispatch(static_cast<MsgType>(type), json);
    //     }
    // }

    // 以下是旧代码（保留作为参考，你可以删除）：
    char szBuf[1024] = { 0 };
    int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
    if (nRead > 0) {
        szBuf[nRead] = '\0';
        UpdateLog(_T("[收到] ") + CString(szBuf));
    }
}

void CNetworkClientDlg::OnClose() {
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

void CNetworkClientDlg::UpdateLog(const CString& str) {
    CString strLog;
    GetDlgItemText(IDC_EDIT_LOG, strLog);
    strLog += str + _T("\r\n");
    SetDlgItemText(IDC_EDIT_LOG, strLog);
}

// ============================================================================
// 公开方法 — 你的网络层调用这些来更新 UI
// ============================================================================

void CNetworkClientDlg::OnLoginSuccess(int userId, const std::string& username,
                                        const std::vector<FriendInfo>& friends) {
    m_userId   = userId;
    m_username = username;

    UpdateStatus(_T("已登录 — ") + CString(username.c_str()));
    AppendChatMessage(_T("[系统] 登录成功，欢迎 ") + CString(username.c_str()));

    RefreshFriendList(friends);

    // 登录后可用
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
    m_btnSendFile.EnableWindow(TRUE);
}

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

void CNetworkClientDlg::RefreshFriendList(const std::vector<FriendInfo>& friends) {
    m_friendList.ResetContent();
    m_friendMap.clear();

    for (const auto& f : friends) {
        AddFriendToList(f.userId, f.username, f.online);
    }

    // 也添加"全体"选项用于群聊
    int idx = m_friendList.AddString(_T("★ 全体（群聊）"));
    m_friendList.SetItemData(idx, 0);  // user_id=0 表示全体
}

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

void CNetworkClientDlg::RemoveFriendFromList(int userId) {
    m_friendMap.erase(userId);

    // 重建显示
    std::vector<FriendInfo> all;
    for (auto& kv : m_friendMap) all.push_back(kv.second);
    RefreshFriendList(all);
}

void CNetworkClientDlg::UpdateStatus(const CString& text) {
    SetDlgItemText(IDC_STATIC_STATUS, text);
}

void CNetworkClientDlg::AppendChatMessage(const CString& msg) {
    CString strLog;
    GetDlgItemText(IDC_EDIT_LOG, strLog);
    strLog += msg + _T("\r\n");
    SetDlgItemText(IDC_EDIT_LOG, strLog);

    // 自动滚动到底部
    int nLen = GetDlgItem(IDC_EDIT_LOG)->SendMessage(WM_GETTEXTLENGTH);
    GetDlgItem(IDC_EDIT_LOG)->SendMessage(EM_SETSEL, nLen, nLen);
    GetDlgItem(IDC_EDIT_LOG)->SendMessage(EM_SCROLLCARET);
}
