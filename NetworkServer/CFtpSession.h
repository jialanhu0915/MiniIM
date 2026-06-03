/**
 * @file   CFtpSession.h
 * @brief  FTP 单个客户端会话处理（控制连接 + 数据连接 + 命令分发表）
 * @author Yan Runxin
 * @date   2026-06-02
 *
 * 设计原则：
 *   1. 每 accept 一个客户端 → new CFtpSession + 起新线程
 *   2. Run() 阻塞循环，recv 命令 → 分发 → 应答
 *   3. 收到 QUIT / socket 关闭 → 释放资源 → delete this
 *   4. 沙箱：所有文件操作路径必须解析后以 m_root 为前缀
 */
#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <winsock2.h>

// 前向声明
class CNetworkServerDlg;

/**
 * @brief 单个 FTP 客户端会话（自起线程、自销毁）
 */
class CFtpSession
{
public:
    /**
     * @brief  启动一个新会话（static 工厂）
     * @param  ctrlSocket 已 accept 的控制连接 socket（所有权转给本会话）
     * @param  rootDir    沙箱根目录
     * @param  clientIp   客户端 IP（仅用于日志）
     * @param  pDlg       主对话框（用于投递日志，可为 nullptr）
     */
    static void Launch(SOCKET ctrlSocket,
                       const std::string& rootDir,
                       const std::string& clientIp,
                       CNetworkServerDlg* pDlg);

    // 禁止拷贝
    CFtpSession(const CFtpSession&) = delete;
    CFtpSession& operator=(const CFtpSession&) = delete;

private:
    CFtpSession(SOCKET sock, std::string root,
                std::string ip, CNetworkServerDlg* pDlg);
    ~CFtpSession();

    /**
     * @brief 会话主循环（线程入口）
     */
    void Run();

    // ---- 网络 I/O ----
    /**
     * @brief  收一行命令（以 \r\n 结尾）
     * @param  out  输出
     * @return 成功 true，连接断开 false
     */
    bool RecvLine(std::string& out);

    /**
     * @brief  发一行响应（自动加 \r\n）
     */
    void SendResponse(int code, const std::string& text);

    /**
     * @brief  发原始字符串（PASV 响应需要特殊格式，不加 code 前缀）
     */
    void SendRaw(const std::string& s);

    // ---- 命令处理（参数是已去除命令名和首尾空白的剩余部分）----
    void HandleUSER(const std::string& arg);
    void HandlePASS(const std::string& arg);
    void HandleQUIT(const std::string& arg);
    void HandlePWD(const std::string& arg);
    void HandleCWD(const std::string& arg);
    void HandleTYPE(const std::string& arg);
    void HandlePASV(const std::string& arg);  ///< PASV 进入被动模式，arg 忽略
    void HandleLIST(const std::string& arg);  ///< LIST [path] 列目录，arg 为空则列当前目录
    void HandleRETR(const std::string& arg);  ///< RETR filename 下载文件，arg=文件名
    void HandleSTOR(const std::string& arg);  ///< STOR filename 上传文件，arg=文件名
    void HandleDELE(const std::string& arg);  ///< DELE filename 删除文件，arg=文件名
    void HandleMKD(const std::string& arg);  ///< MKD dirname 创建目录，arg=目录名
    void HandleRMD(const std::string& arg);  ///< RMD dirname 删除目录，arg=目录名
    void HandleNOOP(const std::string& arg);
    void HandleUnknown(const std::string& cmd, const std::string& arg);

    // ---- 辅助 ----
    /**
     * @brief  把虚拟路径（"/xxx/yyy"）解析为真实路径（"D:\FTPRoot\xxx\yyy"）
     * @param  vpath   虚拟路径
     * @param  outReal 输出真实路径
     * @return 沙箱合法返回 true，逃逸返回 false
     */
    bool ResolveRealPath(const std::string& vpath, std::string& outReal) const;
    /**
	* @brief  打开一个临时监听 socket，返回端口号
    * @param  outPort  输出：实际绑定的端口
    * @param  outIp    输出: 实际的IP地址
    * @return true=成功，false=失败
    */
    bool OpenDataConnection(int& outPort, std::string& outIp);

    // ---- 状态 ----
    SOCKET       m_ctrl;          // 控制连接
    SOCKET       m_dataListen;    // PASV 模式下的临时数据监听 socket
    std::string  m_recvBuf;       // 接收缓存（处理半包）
    std::string  m_root;          // 沙箱根目录
    std::string  m_cwd;           // 虚拟当前目录（始终 "/" 开头）
    std::string  m_clientIp;      // 客户端 IP（日志用）
    std::string  m_user;          // 已认证用户名
    bool         m_authed;        // 是否完成认证
    char         m_type;          // 'I' = binary, 'A' = ascii
    CNetworkServerDlg* m_pDlg;    // 主对话框（日志，可 nullptr）
};
