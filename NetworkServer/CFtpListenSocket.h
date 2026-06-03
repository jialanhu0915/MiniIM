/**
 * @file   CFtpListenSocket.h
 * @brief  FTP 服务端监听器（端口 21），每 accept 一个客户端交给 CFtpSession
 * @author Yan Runxin
 * @date   2026-06-02
 *
 * 教学用 BSD socket 封装：
 *   socket() → bind() → listen() → 循环 accept() → CFtpSession::Launch()
 *
 * 线程模型：
 *   主线程：Start() 调 AfxBeginThread 起监听线程
 *   监听线程：循环 accept()，每 accept 一个客户端立即 Launch 新会话线程
 *
 * 停止机制：
 *   Stop() 关闭 listen socket，accept() 立即返回错误，线程退出
 */
#pragma once

#include <string>
#include <winsock2.h>  // SOCKET 类型

 // 前向声明，避免头文件互相包含
class CFtpSession;
class CNetworkServerDlg;

class CFtpListenSocket
{
public:
    CFtpListenSocket();
    ~CFtpListenSocket();

    // 禁止拷贝（socket 资源不能由两个对象共同持有）
    CFtpListenSocket(const CFtpListenSocket&) = delete;
    CFtpListenSocket& operator=(const CFtpListenSocket&) = delete;

    /**
     * @brief  启动监听（创建 socket、bind、listen、起 accept 线程）
     * @param  port    监听端口（FTP 默认 21）
     * @param  rootDir 沙箱根目录（用户不能逃出此目录）
     * @param  pDlg    主对话框指针（用于 PostMessage 打日志，可为 nullptr）
     * @return 成功返回 true
     */
    bool Start(int port, const std::string& rootDir, CNetworkServerDlg* pDlg);

    /**
     * @brief 停止监听（关闭 listen socket，accept 线程退出）
     */
    void Stop();

    /**
     * @brief  是否正在监听
     */
    bool IsRunning() const { return m_bRunning; }

    /**
     * @brief  获取监听端口
     */
    int GetPort() const { return m_port; }

private:
    /**
     * @brief  accept 循环（线程入口）
     */
    void AcceptLoop();

    /**
     * @brief  线程静态入口（传给 AfxBeginThread）
     */
    static UINT ThreadProc(LPVOID pParam);

    SOCKET       m_listenSock = INVALID_SOCKET;  // 监听 socket
    CWinThread* m_pThread = nullptr;         // accept 线程
    bool         m_bRunning = false;           // 运行标志
    int          m_port = 21;              // 监听端口
    std::string  m_rootDir;                      // 沙箱根目录
    CNetworkServerDlg* m_pDlg = nullptr;         // 主对话框（PostMessage 用）
};
