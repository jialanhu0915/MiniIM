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
    ON_EN_CHANGE(IDC_EDIT_PORT,   &CNetworkServerDlg::OnEnChangeEditPort)
    ON_BN_CLICKED(IDC_BUTTON_START, &CNetworkServerDlg::OnBnClickedButtonStart)
    ON_BN_CLICKED(IDC_BUTTON_STOP,  &CNetworkServerDlg::OnBnClickedButtonStop)
    ON_BN_CLICKED(IDC_BUTTON_SEND,  &CNetworkServerDlg::OnBnClickedButtonSend)
END_MESSAGE_MAP()

// ============================================================================
// 构造 / 初始化
// ============================================================================

CNetworkServerDlg::CNetworkServerDlg(CWnd* pParent)
    : CDialogEx(IDD_NETWORKSERVER_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_listenSocket.m_pDlg = this;
    m_connectSocket.m_pDlg = this;
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
    } catch (const std::exception& e) {
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

// ============================================================================
// ============================================================================
// 协议处理器（你的网络层代码参考）
// ============================================================================
void CNetworkServerDlg::RegisterProtocolHandlers() {
    m_dispatcher.on(MsgType::LOGIN, [this](const std::string& json) {
        // TODO: 解析用户名
        // 1. 检查用户名是否已存在（DB）
        // 2. 分配 user_id
        // 3. 发送 LOGIN_RESP（含好友列表 + 离线消息）
        // 4. 通知其他用户：STATUS_ONLINE
        // 5. OnUserLogin(userId, username, ip)
    });

    m_dispatcher.on(MsgType::LOGOUT, [this](const std::string& json) {
        // TODO: 移除用户，通知其他用户 STATUS_OFFLINE
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
    });

    m_dispatcher.on(MsgType::FRIEND_DEL, [this](const std::string& json) {
        // TODO: 删除好友关系
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

// ============================================================================
// 系统消息
// ============================================================================

void CNetworkServerDlg::OnSysCommand(UINT nID, LPARAM lParam) {
    if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    } else {
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
    } else {
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
    m_connectSocket.Close();
    m_listenSocket.Close();

    // 清空在线用户
    m_userList.DeleteAllItems();
    m_onlineUsers.clear();

    UpdateStatus(_T("空闲"));
    GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
    GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
    UpdateLog(_T("[停止] 已停止监听"));
}

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

// ============================================================================
// 网络回调
// ============================================================================

void CNetworkServerDlg::OnAccept() {
    if (m_listenSocket.Accept(m_connectSocket)) {
        UpdateLog(_T("客户端已连接"));
        UpdateStatus(_T("已连接"));
        GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
    } else {
        UpdateLog(_T("接受连接失败"));
    }
}

void CNetworkServerDlg::OnReceive() {
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

    // 旧代码（可删除）：
    char szBuf[1024] = { 0 };
    int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
    if (nRead > 0) {
        szBuf[nRead] = '\0';
        UpdateLog(_T("[接收] ") + CString(szBuf));
    }
}

void CNetworkServerDlg::OnClose() {
    m_connectSocket.Close();

    // 移除该连接对应的在线用户
    // TODO: 你的网络层代码 — 找到对应的 userId 并调用 OnUserLogout

    UpdateStatus(_T("监听中..."));
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
    UpdateLog(_T("[断开] 客户端已断开"));
}

// ============================================================================
// 公开方法
// ============================================================================

void CNetworkServerDlg::OnUserLogin(int userId,
                                     const std::string& username,
                                     const std::string& ip) {
    m_onlineUsers[userId] = { userId, username, ip };

    int idx = m_userList.InsertItem(m_userList.GetItemCount(),
                                     CString(username.c_str()));
    m_userList.SetItemText(idx, 1, CString(ip.c_str()));
    m_userList.SetItemData(idx, static_cast<DWORD_PTR>(userId));

    UpdateLog(_T("[上线] ") + CString(username.c_str()) +
              _T(" (") + CString(ip.c_str()) + _T(")"));
}

void CNetworkServerDlg::OnUserLogout(int userId) {
    auto it = m_onlineUsers.find(userId);
    if (it != m_onlineUsers.end()) {
        UpdateLog(_T("[下线] ") + CString(it->second.username.c_str()));
    }

    // 从列表控件移除
    int count = m_userList.GetItemCount();
    for (int i = 0; i < count; i++) {
        if (static_cast<int>(m_userList.GetItemData(i)) == userId) {
            m_userList.DeleteItem(i);
            break;
        }
    }

    m_onlineUsers.erase(userId);
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
