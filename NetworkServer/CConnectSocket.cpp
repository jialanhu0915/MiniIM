#include "pch.h"
#include "CConnectSocket.h"
#include "NetworkServerDlg.h"

thread_local CConnectSocket* g_pCurrentSocket = nullptr;

CConnectSocket::CConnectSocket()
    : m_pDlg(nullptr)
    , m_socket(INVALID_SOCKET)
    , m_bDisconnected(false)
    , m_pThread(nullptr)
    , m_bRunning(FALSE)
{
}

CConnectSocket::~CConnectSocket()
{

}

bool CConnectSocket::Start()
{
	m_pThread = AfxBeginThread(ThreadProc, this,
		THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (!m_pThread) return false;
	m_bRunning = TRUE;	// 设置运行标志，线程入口会检查
	m_pThread->m_bAutoDelete = FALSE;  // 线程结束后不自动删除对象
	m_pThread->ResumeThread();	// 线程开始执行 ThreadProc
	return true;
}

UINT AFX_CDECL CConnectSocket::ThreadProc(LPVOID pParam) {
	CConnectSocket* pThis = static_cast<CConnectSocket*>(pParam);
	pThis->RecvLoop();                          // 阻塞循环
	// recv 返回 0 或错误 → 循环退出
	if (!pThis->m_bDisconnected) {
		pThis->m_pDlg->OnClientDisconnected(pThis);  // 通知 dialog 清理
	}
	delete pThis;  // 线程自销毁！
	return 0;
}

void CConnectSocket::RecvLoop() {
	char buf[4096];
	while (InterlockedCompareExchange(&m_bRunning, TRUE, TRUE) == TRUE) {
		int ret = recv(m_socket, buf, sizeof(buf), 0);

		if (ret == 0) {  // 客户端正常断开
			break;
		}

		if (ret == SOCKET_ERROR) {
			break;
		}

		m_recvBuf.append(buf, ret);
		uint32_t msgType, msgLength;
		std::string json;
		while (m_recvBuf.next(msgType, msgLength, json)) {
			g_pCurrentSocket = this;  // 设置线程局部指针，方便 handler 获取当前 socket
			m_pDlg->DispatchMessage(this, static_cast<MsgType>(msgType), json);
			g_pCurrentSocket = nullptr;	// 清除
		}
	}
}

int CConnectSocket::SendData(const void* buf, int nBufLen) {
	// 防御：socket 无效就不发
	if (m_socket == INVALID_SOCKET) return SOCKET_ERROR;

	const char* p = static_cast<const char*>(buf);
	int totalSent = 0;
	while (totalSent < nBufLen) {
		int n = send(m_socket, p + totalSent, nBufLen - totalSent, 0);
		if (n == SOCKET_ERROR) return SOCKET_ERROR;
		totalSent += n;
	}
	return totalSent;
}

void CConnectSocket::Stop() {
	InterlockedExchange(&m_bRunning, FALSE);    // 通知 RecvLoop 退出
	shutdown(m_socket, SD_BOTH);                // 使阻塞的 recv 立即返回 SOCKET_ERROR
	closesocket(m_socket);                      // 关闭 socket
	m_socket = INVALID_SOCKET;

	if (m_pThread) {
		WaitForSingleObject(m_pThread->m_hThread, 5000);  // 最多等 5 秒
		delete m_pThread;
		m_pThread = nullptr;
	}
}

