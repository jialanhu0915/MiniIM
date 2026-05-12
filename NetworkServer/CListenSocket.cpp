#include "pch.h"
#include "CListenSocket.h"
#include "NetworkServerDlg.h"

CListenSocket::CListenSocket()
	:m_pDlg(nullptr)
{
}

CListenSocket::~CListenSocket()
{

}

void CListenSocket::OnAccept(int nErrorCode) 
{
	if (nErrorCode == 0)
	{
		if (m_pDlg)
		{
			m_pDlg->OnAccept();
		}
	}
	CAsyncSocket::OnAccept(nErrorCode);
}
