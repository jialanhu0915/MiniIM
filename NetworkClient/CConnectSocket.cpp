/**
 * @file CConnectSocket.cpp
 * @brief 客户端异步 socket 实现（事件转发到对话框）
 */
#include "pch.h"
#include "CConnectSocket.h"
#include "NetworkClientDlg.h"

/** @brief 默认构造 */
CConnectSocket::CConnectSocket()
{

}

/** @brief 默认析构 */
CConnectSocket::~CConnectSocket()
{

}

/** @brief 接收到数据回调，转发到对话框
 * @param nErrorCode 错误码（0 表示正常）
 */
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

/** @brief 连接关闭回调，转发到对话框
 * @param nErrorCode 错误码（0 表示正常）
 */
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

/** @brief 连接建立回调（成功或失败均转发）
 * @param nErrorCode 错误码（0 表示成功）
 * @note nErrorCode != 0 时调用 OnConnectError 显示错误信息
 */
void CConnectSocket::OnConnect(int nErrorCode)
{
	if (m_pDlg)
	{
		if (nErrorCode == 0)
			m_pDlg->OnConnect();
		else
			m_pDlg->OnConnectError(nErrorCode);
	}
	CAsyncSocket::OnConnect(nErrorCode);
}
