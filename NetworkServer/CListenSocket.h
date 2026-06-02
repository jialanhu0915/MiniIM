#pragma once

/**
 * @file   CListenSocket.h
 * @brief  服务端监听 socket（绑定端口+accept客户端连接）
 * @author Yan Runxin
 * @date   2026-05-25
 */

class CNetworkServerDlg;

/**
 * @brief 服务端监听 socket
 * @note 在主线程创建，Start() 启动 accept 循环
 */
class CListenSocket
{
public:
    /** @brief 默认构造 */
    CListenSocket();
    ~CListenSocket();

    CNetworkServerDlg* m_pDlg;  ///< 指向 NetworkServerDlg 主窗口的指针

    /** @brief 绑定端口并启动 accept 循环 @param nPort 监听端口 @return 是否成功 */
    bool Start(int port);
    /** @brief 停止监听，关闭 socket */
    void Stop();            // closesocket → 等待线程退出

private:
    SOCKET              m_socket;  ///< 监听 socket 句柄
    CWinThread*         m_pThread;  ///< accept 线程句柄
    volatile LONG       m_bRunning;  ///< 停止标志位

    /** @brief accept 循环线程函数 @param pParam 指向 CListenSocket 指针 @return 线程退出码 */
    static UINT AFX_CDECL ThreadProc(LPVOID pParam);
    /** @brief 循环 accept 客户端连接，创建 CConnectSocket 并触发 ClientThread */
    void AcceptLoop();      // while(m_bRunning) { accept → new CConnectSocket → Start() }
};
