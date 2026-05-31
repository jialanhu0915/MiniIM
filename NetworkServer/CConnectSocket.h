#pragma once
#include "../Common/Protocol.h"
#include <string>

class CNetworkServerDlg;

class CConnectSocket
{
public:
    CConnectSocket();
    ~CConnectSocket();

    CNetworkServerDlg*  m_pDlg;
    SOCKET              m_socket;
    RecvBuffer          m_recvBuf;
    std::string         m_clientIP;
    bool                m_bDisconnected;

    bool Start();                                // 创建 recv 线程
    void Stop();                                 // 关闭 socket + 等待线程退出
    int  SendData(const void* buf, int nBufLen); // 阻塞 send，循环写完整

private:
    CWinThread*         m_pThread;
    volatile LONG       m_bRunning;              // InterlockedExchange 操作

    static UINT AFX_CDECL ThreadProc(LPVOID pParam); // 线程入口，调用 RecvLoop
    void RecvLoop();                             // while(recv) → m_recvBuf → dispatch
};
