/**
 * @file   FtpHelper.h
 * @brief  FTP 文件传输 RAII 封装（基于 WinInet）
 * @author Yan Runxin
 * @date   2026-05-25
 *
 * 即时通讯系统文件传输流程：
 *   1. 聊天服务端同时运行一个 FTP 服务（如 FileZilla Server）
 *   2. 发送方：UploadFile() 上传文件到 FTP→通过聊天协议发 FILE_REQUEST 通知对方
 *   3. 接收方：收到 FILE_REQUEST→DownloadFile() 从 FTP 下载
 */

#pragma once

#include <Windows.h>
#include <WinInet.h>
#include <string>

#pragma comment(lib, "wininet.lib")

class FtpHelper
{
public:
    FtpHelper();
    ~FtpHelper();

    // 禁止拷贝
    FtpHelper(const FtpHelper&) = delete;
    FtpHelper& operator=(const FtpHelper&) = delete;

    // ====================== 连接管理 ======================

    /**
     * @brief       连接到 FTP 服务器
     * @param[in]   server    FTP 服务器地址（如 "192.168.1.100" 或 "ftp.example.com"）
     * @param[in]   port      FTP 端口（默认 21）
     * @param[in]   user      用户名（匿名登录传 "anonymous"）
     * @param[in]   password  密码
     * @return      成功返回 true
     */
    bool Connect(const std::wstring& server, int port = 21,
                 const std::wstring& user = L"anonymous",
                 const std::wstring& password = L"");

    /**
     * @brief  断开 FTP 连接
     */
    void Disconnect();

    /**
     * @brief  是否已连接
     */
    bool IsConnected() const { return m_hFtp != nullptr; }

    // ====================== 文件操作 ======================

    /**
     * @brief       上传本地文件到 FTP 服务器
     * @param[in]   localFile   本地文件完整路径
     * @param[in]   remoteFile  远端相对路径名（如 "/files/photo.jpg"）
     * @return      成功返回 true
     */
    bool UploadFile(const std::wstring& localFile,
                    const std::wstring& remoteFile);

    /**
     * @brief       从 FTP 服务器下载文件到本地
     * @param[in]   remoteFile  远端文件路径
     * @param[in]   localFile   本地保存路径
     * @return      成功返回 true
     */
    bool DownloadFile(const std::wstring& remoteFile,
                      const std::wstring& localFile);

    /**
     * @brief       删除 FTP 服务器上的文件
     * @param[in]   remoteFile  远端文件路径
     * @return      成功返回 true
     */
    bool DeleteFile(const std::wstring& remoteFile);

    /**
     * @brief       获取文件大小
     * @param[in]   remoteFile  远端文件路径
     * @return      文件大小（字节），失败返回 -1
     */
    long long GetFileSize(const std::wstring& remoteFile);

    // ====================== 错误信息 ======================

    /**
     * @brief  获取最后一次操作的错误码
     */
    DWORD GetLastErrorCode() const { return m_dwLastError; }

    /**
     * @brief  获取最后一次操作的错误描述
     */
    std::wstring GetLastErrorMessage() const;

private:
    HINTERNET m_hInternet;  // WinInet 根句柄
    HINTERNET m_hFtp;       // FTP 会话句柄
    DWORD     m_dwLastError;
};
