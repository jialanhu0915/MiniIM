#pragma once

/**
 * @file   CConnectSocket.h
 * @brief  服务端客户端连接 socket（每客户端一个线程 recv）
 * @author Yan Runxin
 * @date   2026-05-25
 */

#include "../Common/Protocol.h"
#include <string>

class CNetworkServerDlg;

/**
 * @brief 每客户端一个的连接 socket
 * @note  ThreadProc 创建线程，RecvLoop 接收数据，delete this 自销毁
 */
class CConnectSocket
{
public:
	/** @brief 构造 */
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
