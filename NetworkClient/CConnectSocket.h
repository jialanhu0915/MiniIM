#pragma once
#include <afxsock.h>

class CNetworkClientDlg;

class CConnectSocket :
    public CAsyncSocket
{
public:
    CConnectSocket();
    virtual ~CConnectSocket();

    CNetworkClientDlg* m_pDlg;

    virtual void OnReceive(int nErrorCode);
    virtual void OnClose(int nErrorCode);
    virtual void OnConnect(int nErrorCode);
};

