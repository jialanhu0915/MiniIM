#pragma once
#include <afxsock.h>

class CNetworkServerDlg;

class CConnectSocket :
    public CAsyncSocket
{
public:
    CConnectSocket();
    virtual ~CConnectSocket();

    CNetworkServerDlg* m_pDlg;

    virtual void OnReceive(int nErrorCode);
    virtual void OnClose(int nErrorCode);
};

