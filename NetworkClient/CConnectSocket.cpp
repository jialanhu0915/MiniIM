#include "pch.h"
#include "CConnectSocket.h"
#include "NetworkClientDlg.h"

CConnectSocket::CConnectSocket()
{

}

CConnectSocket::~CConnectSocket()
{

}

void CConnectSocket::OnReceive(int nErrorCode)
{
	if (nErrorCode == 0)
	{
		if (m_pDlg)
		{
			m_pDlg->OnReceive();
		}
	}
	CAsyncSocket::OnReceive(nErrorCode);
}

void CConnectSocket::OnClose(int nErrorCode)
{
	if (nErrorCode == 0)
	{
		if (m_pDlg)
		{
			m_pDlg->OnClose();
		}
	}
	CAsyncSocket::OnClose(nErrorCode);
}

void CConnectSocket::OnConnect(int nErrorCode)
{
	if (nErrorCode == 0)
	{
		if (m_pDlg)
			m_pDlg->OnConnect();
	}
	CAsyncSocket::OnConnect(nErrorCode);
}
