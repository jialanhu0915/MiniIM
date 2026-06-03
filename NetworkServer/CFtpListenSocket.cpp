/**
 * @file   CFtpListenSocket.cpp
 * @brief  FTP 服务端监听器实现
 */
#include "pch.h"
#include "CFtpListenSocket.h"
#include "CFtpSession.h"
#include "NetworkServerDlg.h"

#include <afxsock.h>   // AfxSocketInit
#include <WS2tcpip.h>  // inet_ntop, getaddrinfo

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

 /**
  * @brief  构造函数
  */
CFtpListenSocket::CFtpListenSocket() = default;

/**
 * @brief  析构（自动 Stop）
 */
CFtpListenSocket::~CFtpListenSocket() {
    Stop();
}

/**
 * @brief  启动监听（socket + bind + listen + 起 accept 线程）
 * @param  port    监听端口
 * @param  rootDir 沙箱根目录
 * @param  pDlg    主对话框（用于 PostMessage 投递日志）
 * @return 成功返回 true
 */
bool CFtpListenSocket::Start(int port, const std::string& rootDir, CNetworkServerDlg* pDlg) {
    if (m_bRunning) {
        return false;  // 已经在跑
    }

    m_port = port;
    m_rootDir = rootDir;
    m_pDlg = pDlg;

    // 1) socket()
    m_listenSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSock == INVALID_SOCKET) {
        return false;
    }

    // 2) SO_REUSEADDR（避免重启时 TIME_WAIT 占用端口）
    int opt = 1;
    ::setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&opt), sizeof(opt));

    // 3) bind()
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听所有网卡
    addr.sin_port = htons(static_cast<u_short>(port));
    if (::bind(m_listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
        return false;
    }

    // 4) listen()
    if (::listen(m_listenSock, 16) == SOCKET_ERROR) {
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
        return false;
    }

    // 5) 起 accept 线程
    m_bRunning = true;
    m_pThread = AfxBeginThread(ThreadProc, this);
    if (!m_pThread) {
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
        m_bRunning = false;
        return false;
    }
    return true;
}

/**
 * @brief 停止监听（关闭 listen socket，accept 线程退出）
 */
void CFtpListenSocket::Stop() {
    if (!m_bRunning) return;
    m_bRunning = false;

    // 关 listen socket → 阻塞的 accept() 立即返回 SOCKET_ERROR
    if (m_listenSock != INVALID_SOCKET) {
        ::closesocket(m_listenSock);
        m_listenSock = INVALID_SOCKET;
    }

    // 等线程退出（最多 2 秒）
    if (m_pThread) {
        // 不能用 TerminateThread（会泄漏资源）
        // AfxBeginThread 默认是 THREAD_PRIORITY_NORMAL，挂起 OK
        ::WaitForSingleObject(m_pThread->m_hThread, 2000);
        m_pThread = nullptr;
    }
}

/**
 * @brief accept 循环（运行在子线程）
 *
 * 每 accept 一个客户端就立即 CFtpSession::Launch(sock) 起独立线程，
 * 本线程继续阻塞在 accept() 等下一个连接。
 */
void CFtpListenSocket::AcceptLoop() {
    while (m_bRunning) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = ::accept(m_listenSock,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &addrLen);
        if (clientSock == INVALID_SOCKET) {
            // Stop() 关闭了 listen socket，或其他错误
            break;
        }

        // 拿到客户端 IP（仅用于日志）
        char ipStr[64] = "unknown";
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));

        // 把控制连接 socket 交给 CFtpSession（新线程里跑）
        CFtpSession::Launch(clientSock, m_rootDir, ipStr, m_pDlg);

        // 投递日志到主对话框（跨线程安全：PostUIUpdate 内部用 PostMessage）
        if (m_pDlg) {
            CString log;
            log.Format(_T("[FTP] 接受连接 from %hs"), ipStr);
            m_pDlg->PostUIUpdate(UIUpdateType::LOG, log);
        }
    }
    m_bRunning = false;
}

/**
 * @brief 线程入口（AfxBeginThread 要求静态函数）
 * @param pParam CFtpListenSocket* 指针
 * @return 线程退出码
 */
UINT CFtpListenSocket::ThreadProc(LPVOID pParam) {
    CFtpListenSocket* pThis = static_cast<CFtpListenSocket*>(pParam);
    if (pThis) {
        pThis->AcceptLoop();
    }
    return 0;
}
