#pragma once
#include <winsock2.h>

class CNetworkServerDlg;

class CListenSocket
{
public:
    CListenSocket();
    ~CListenSocket();

    CNetworkServerDlg* m_pDlg = nullptr;

    bool Start(int port);   // socket+bind+listen → AfxBeginThread
    void Stop();            // closesocket → 等待线程退出

private:
    SOCKET              m_socket = INVALID_SOCKET;
    CWinThread*         m_pThread = nullptr;
    volatile LONG       m_bRunning = FALSE;

    static UINT AFX_CDECL ThreadProc(LPVOID pParam);
    void AcceptLoop();      // while(m_bRunning) { accept → new CConnectSocket → Start() }
};
