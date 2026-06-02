#pragma once
#include <afxsock.h>

class CNetworkClientDlg;

/**
 * @file CConnectSocket.h
 * @brief 客户端异步 socket 实现（事件转发到对话框）
 * @author Yan Runxin
 * @date 2026-05-25
 */
/** @brief 客户端异步 socket 包装类
 * @note CAsyncSocket 子类，将 OnConnect/OnReceive/OnClose 事件转发到 m_pDlg（CNetworkClientDlg）。所有回调在 UI 线程执行。
 */
class CConnectSocket :
    public CAsyncSocket
{
public:
    /** @brief 默认构造 */
    CConnectSocket();
    /** @brief 默认析构 */
    virtual ~CConnectSocket();

    CNetworkClientDlg* m_pDlg;  ///< 指向对话框的指针（由 CNetworkClientDlg 在构造时设置）

    /** @brief 接收到数据回调，转发到对话框
     * @param nErrorCode 错误码（0 表示正常）
     */
    virtual void OnReceive(int nErrorCode);
    /** @brief 连接关闭回调，转发到对话框
     * @param nErrorCode 错误码（0 表示正常）
     */
    virtual void OnClose(int nErrorCode);
    /** @brief 连接建立回调（成功或失败均转发）
     * @param nErrorCode 错误码（0 表示成功）
     * @note nErrorCode != 0 时调用 OnConnectError 显示错误信息
     */
    virtual void OnConnect(int nErrorCode);
};

