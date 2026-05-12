
// NetworkServerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "NetworkServer.h"
#include "NetworkServerDlg.h"
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


// CNetworkServerDlg 对话框



CNetworkServerDlg::CNetworkServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_NETWORKSERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_listenSocket.m_pDlg = this;
	m_connectSocket.m_pDlg = this;
}

void CNetworkServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNetworkServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_STN_CLICKED(IDC_STATIC_PORT, &CNetworkServerDlg::OnStnClickedStaticPort)
	ON_EN_CHANGE(IDC_EDIT_PORT, &CNetworkServerDlg::OnEnChangeEditPort)
	ON_BN_CLICKED(IDC_BUTTON_START, &CNetworkServerDlg::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_STOP, &CNetworkServerDlg::OnBnClickedButtonStop)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CNetworkServerDlg::OnBnClickedButtonSend)
END_MESSAGE_MAP()


// CNetworkServerDlg 消息处理程序

BOOL CNetworkServerDlg::OnInitDialog()
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
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	SetDlgItemText(IDC_STATIC_STATUS, _T("空闲"));

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CNetworkServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CNetworkServerDlg::OnPaint()
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
HCURSOR CNetworkServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CNetworkServerDlg::OnStnClickedStaticPort()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CNetworkServerDlg::OnEnChangeEditPort()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
}

void CNetworkServerDlg::UpdateLog(const CString& str)
{
	CString strLog;
	GetDlgItemText(IDC_EDIT_LOG, strLog);
	strLog += str + _T("\r\n");
	SetDlgItemText(IDC_EDIT_LOG, strLog);
}

void CNetworkServerDlg::OnBnClickedButtonStart() 
{
	CString strPort;
	GetDlgItemText(IDC_EDIT_PORT, strPort);
	int port = _ttoi(strPort);
	if (port < 0 || port > 65535)
	{
		AfxMessageBox(_T("请输入有效的端口号（0-65535）"));
		return;
	}

	if (!m_listenSocket.Create(port))
	{
		AfxMessageBox(_T("创建监听套接字失败"));
		return;
	}
	
	m_listenSocket.Listen();
	SetDlgItemText(IDC_STATIC_STATUS, _T("正在监听..."));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(FALSE);
	UpdateLog(_T("[启动] 开始监听端口 ") + strPort);

}

void CNetworkServerDlg::OnBnClickedButtonStop()
{
	m_connectSocket.Close();
	m_listenSocket.Close();

	SetDlgItemText(IDC_STATIC_STATUS, _T("空闲"));
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
	UpdateLog(_T("[停止] 已停止监听"));
}

void CNetworkServerDlg::OnBnClickedButtonSend()
{
	CString strMessage;
	GetDlgItemText(IDC_EDIT_MSG, strMessage);
	if (strMessage.IsEmpty())
	{
		AfxMessageBox(_T("请输入要发送的消息"));
		return;
	}
	//int nSent = m_connectSocket.Send(strMessage.GetBuffer(), strMessage.GetLength());

	CT2A asciiMsg(strMessage);
	int nSent = m_connectSocket.Send((LPCSTR)asciiMsg, strlen(asciiMsg));

	if (nSent == SOCKET_ERROR)
	{
		AfxMessageBox(_T("发送消息失败"));
		UpdateLog(_T("[错误] 发送消息失败"));
		return;
	}
	UpdateLog(_T("[发送] ") + strMessage);
	SetDlgItemText(IDC_EDIT_MSG, _T(""));
}



void CNetworkServerDlg::OnAccept()
{
	if (m_listenSocket.Accept(m_connectSocket))
	{
		// 成功接受连接
		UpdateLog(_T("客户端已连接"));
		SetDlgItemText(IDC_STATIC_STATUS, _T("已连接"));
		GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
		GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
	}
	else
	{
		UpdateLog(_T("接受连接失败"));
	}
}

void CNetworkServerDlg::OnReceive()
{
	char szBuf[1024] = { 0 };
	int nRead = m_connectSocket.Receive(szBuf, sizeof(szBuf) - 1);
	if ( nRead > 0 )
	{
		szBuf[nRead] = '\0';
		UpdateLog(_T("[接收] ") + CString(szBuf));
	}
}

void CNetworkServerDlg::OnClose()
{
	m_connectSocket.Close();
	SetDlgItemText(IDC_STATIC_STATUS, _T("监听中..."));
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	UpdateLog(_T("[断开] 客户端已断开"));
}



