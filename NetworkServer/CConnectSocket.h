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

	bool Start();
	void Stop();
	int  SendData(const void* buf, int nBufLen);

private:
	CWinThread*         m_pThread;
	volatile LONG       m_bRunning;

	static UINT AFX_CDECL ThreadProc(LPVOID pParam);
	void RecvLoop();
};
