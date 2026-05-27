#pragma once
#include <winsock2.h>
#include "../Common/Protocol.h"
#include <string>

class CNetworkServerDlg;

class CConnectSocket
{
public:
    CConnectSocket();
    ~CConnectSocket();

    CNetworkServerDlg*  m_pDlg = nullptr;
    SOCKET              m_socket = INVALID_SOCKET;
    RecvBuffer          m_recvBuf;
    std::string         m_clientIP;
    bool                m_bDisconnected = false;

    bool Start();                                // 创建 recv 线程
    void Stop();                                 // 关闭 socket + 等待线程退出
    int  Send(const void* buf, int nBufLen);     // 阻塞 send，循环写完整

private:
    CWinThread*         m_pThread = nullptr;
    volatile LONG       m_bRunning = FALSE;      // InterlockedExchange 操作

    static UINT AFX_CDECL ThreadProc(LPVOID pParam);
    void RecvLoop();                             // while(recv) → m_recvBuf → dispatch
};
