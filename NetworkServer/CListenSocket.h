#pragma once
#include <afxsock.h>

class CNetworkServerDlg;

class CListenSocket :
    public CAsyncSocket
{
public:
    CListenSocket();
    virtual ~CListenSocket();

    CNetworkServerDlg* m_pDlg;

    virtual void OnAccept(int nErrorCode);
};

