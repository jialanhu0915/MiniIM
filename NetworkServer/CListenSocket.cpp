#include "pch.h"
#include "CListenSocket.h"
#include "NetworkServerDlg.h"

/**
 * @file   CListenSocket.cpp
 * @brief  服务端监听 socket 实现
 * @author Yan Runxin
 * @date   2026-05-25
 */

/** @brief 默认构造 */
CListenSocket::CListenSocket()
	: m_pDlg(nullptr)
	, m_socket(INVALID_SOCKET)
	, m_pThread(nullptr)
	, m_bRunning(FALSE)
{
}

CListenSocket::~CListenSocket()
{

}

/** @brief 创建监听 socket，绑定端口并开始监听 @param nPort 端口号 @return 成功返回 true */
bool CListenSocket::Start(int port)
{
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	// SO_REUSEADDR
	int optval = 1;
	setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));

	// bind
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 或 INADDR_ANY（0.0.0.0）
	addr.sin_port = htons(port);
	bind(m_socket, (sockaddr*)&addr, sizeof(addr));

	listen(m_socket, SOMAXCONN);

	// 创建监听线程
	m_bRunning = TRUE;
	m_pThread = AfxBeginThread(ThreadProc, this,
		THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (!m_pThread) {
		m_socket = INVALID_SOCKET;
		return false;
	}
	m_pThread->m_bAutoDelete = FALSE;  // 线程结束后不自动删除对象
	m_pThread->ResumeThread();	// 线程开始执行 ThreadProc

	return true;
}

/** @brief 通知线程退出并关闭 socket */
void CListenSocket::Stop()
{
	InterlockedExchange(&m_bRunning, FALSE);    // 通知 RecvLoop 退出
	closesocket(m_socket);                      // 关闭 socket
	m_socket = INVALID_SOCKET;

	if (m_pThread) {
		WaitForSingleObject(m_pThread->m_hThread, 5000);  // 最多等 5 秒
		delete m_pThread;
		m_pThread = nullptr;
	}
}

/** @brief 循环 accept() 客户端连接，收到后创建 CConnectSocket 并触发 ClientThread @param pParam CListenSocket 指针 @return 0 */
UINT AFX_CDECL CListenSocket::ThreadProc(LPVOID pParam)
{
	CListenSocket* pThis = static_cast<CListenSocket*>(pParam);
	pThis->AcceptLoop();
	return 0;
}

/** @brief 循环接受客户端连接，创建 CConnectSocket 并启动其线程 */
void CListenSocket::AcceptLoop()
{
	while (InterlockedCompareExchange(&m_bRunning, TRUE, TRUE) == TRUE) {
		// accept 阻塞等待新连接
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSock = accept(m_socket, (sockaddr*)&clientAddr, &addrLen);
		if (clientSock == INVALID_SOCKET) break;

		// 创建 CConnectSocket 对象，传入 clientSock 和 clientAddr.sin_addr
		CConnectSocket* p = new CConnectSocket;
		p->m_socket = clientSock;
		p->m_pDlg = m_pDlg;
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
		p->m_clientIP = ipStr;
		p->Start();  // 创建 recv 线程

		{
			CSingleLock lock(&m_pDlg->m_csData, TRUE);
			m_pDlg->m_connectSockets.push_back(p);
		}

		m_pDlg->PostUIUpdate(UIUpdateType::LOG, _T("[Connect] New client connected"));
		m_pDlg->PostUIUpdate(UIUpdateType::STATUS_TEXT, _T("Connected"));

	}
}
