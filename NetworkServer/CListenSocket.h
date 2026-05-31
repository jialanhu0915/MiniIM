#pragma once

class CNetworkServerDlg;

class CListenSocket
{
public:
    CListenSocket();
    ~CListenSocket();

    CNetworkServerDlg* m_pDlg;

    bool Start(int port);   // socket+bind+listen → AfxBeginThread
    void Stop();            // closesocket → 等待线程退出

private:
    SOCKET              m_socket;
    CWinThread*         m_pThread;
    volatile LONG       m_bRunning;

    static UINT AFX_CDECL ThreadProc(LPVOID pParam);
    void AcceptLoop();      // while(m_bRunning) { accept → new CConnectSocket → Start() }
};
