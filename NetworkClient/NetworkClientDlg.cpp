
// NetworkClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "NetworkClient.h"
#include "NetworkClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CNetworkClientDlg 对话框



CNetworkClientDlg::CNetworkClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_NETWORKCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_connectSocket.m_pDlg = this;
}

void CNetworkClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNetworkClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_EN_CHANGE(IDC_EDIT_PORT, &CNetworkClientDlg::OnEnChangeEditPort)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CNetworkClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CNetworkClientDlg::OnBnClickedButtonDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CNetworkClientDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDCANCEL, &CNetworkClientDlg::OnBnClickedCancel)
	ON_EN_CHANGE(IDC_EDIT_MSG, &CNetworkClientDlg::OnEnChangeEditMsg)
END_MESSAGE_MAP()


// CNetworkClientDlg 消息处理程序

BOOL CNetworkClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	SetDlgItemText(IDC_STATIC_STATUS, _T("未连接"));

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CNetworkClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CNetworkClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CNetworkClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CNetworkClientDlg::OnEnChangeEdit1()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}

void CNetworkClientDlg::OnIpnFieldchangedIpaddress1(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}

void CNetworkClientDlg::OnEnChangeEditPort()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}

void CNetworkClientDlg::OnBnClickedButtonConnect()
{
	// TODO: 在此添加控件通知处理程序代码
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
	CString strIP, strPort;
	GetDlgItemText(IDC_IPADDRESS, strIP);
	GetDlgItemText(IDC_EDIT_PORT, strPort);
	int port = _ttoi(strPort);

	if (strIP.IsEmpty() || port <= 0 || port > 65535)
	{
		AfxMessageBox(_T("请输入有效的IP地址和端口！"));
		GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
		return;
	}

	if (!m_connectSocket.Create())
	{
		UpdateLog(_T("[错误] 创建 socket 失败"));
		GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
		return;
	}

	m_connectSocket.Connect(strIP, port);
	UpdateLog(_T("[连接] 正在连接 ") + strIP + _T(":") + strPort);
}

void CNetworkClientDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码
	m_connectSocket.Close();
	SetDlgItemText(IDC_STATIC_STATUS, _T("未连接"));
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	UpdateLog(_T("[断开] 已断开连接"));
}

void CNetworkClientDlg::OnBnClickedButtonSend()
{
	// TODO: 在此添加控件通知处理程序代码
	CString strMsg;
	GetDlgItemText(IDC_EDIT_MSG, strMsg);
	if (strMsg.IsEmpty())
	{
		AfxMessageBox(_T("请输入消息！"));
		return;
	}

	CT2A asciiMsg(strMsg);
	int nSent = m_connectSocket.Send((LPCSTR)asciiMsg, strlen(asciiMsg));
	if (nSent == SOCKET_ERROR)
	{
		UpdateLog(_T("[错误] 发送失败"));
		return;
	}
	UpdateLog(_T("[发送] ") + strMsg);
	SetDlgItemText(IDC_EDIT_MSG, _T(""));
}

void CNetworkClientDlg::OnConnect()
{
	SetDlgItemText(IDC_STATIC_STATUS, _T("已连接"));
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
	UpdateLog(_T("[连接] 连接成功"));
}

void CNetworkClientDlg::OnReceive()
{
	char szBuf[1024] = { 0 };
	int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
	if (nRead > 0)
	{
		szBuf[nRead] = '\0';
		UpdateLog(_T("[收到] ") + CString(szBuf));
	}
}

void CNetworkClientDlg::OnClose()
{
	m_connectSocket.Close();
	SetDlgItemText(IDC_STATIC_STATUS, _T("未连接"));
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_DISCONNECT)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	UpdateLog(_T("[断开] 服务端已断开"));
}

void CNetworkClientDlg::UpdateLog(const CString& str)
{
	CString strLog;
	GetDlgItemText(IDC_EDIT_LOG, strLog);
	strLog += str + _T("\r\n");
	SetDlgItemText(IDC_EDIT_LOG, strLog);
}


void CNetworkClientDlg::OnBnClickedCancel()
{
	// TODO: 在此添加控件通知处理程序代码
	CDialogEx::OnCancel();
}

void CNetworkClientDlg::OnEnChangeEditMsg()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}
