/**
 * @file   FtpHelper.cpp
 * @brief  FTP 文件传输 RAII 封装实现
 * @author Yan Runxin
 * @date   2026-05-25
 */

#include "FtpHelper.h"

FtpHelper::FtpHelper()
    : m_hInternet(nullptr)
    , m_hFtp(nullptr)
    , m_dwLastError(0)
{
}

FtpHelper::~FtpHelper() {
    Disconnect();
}

bool FtpHelper::Connect(const std::wstring& server, int port,
                         const std::wstring& user,
                         const std::wstring& password) {
    Disconnect();  // 先断开旧连接

    // 1. 打开 WinInet 会话
    m_hInternet = InternetOpenW(
        L"ChatApp/1.0",                    // User-Agent
        INTERNET_OPEN_TYPE_DIRECT,         // 不通过代理
        nullptr, nullptr, 0);
    if (!m_hInternet) {
        m_dwLastError = GetLastError();
        return false;
    }

    // 2. 连接 FTP 服务器
    m_hFtp = InternetConnectW(
        m_hInternet,
        server.c_str(),
        static_cast<INTERNET_PORT>(port),
        user.c_str(),
        password.c_str(),
        INTERNET_SERVICE_FTP,               // FTP 服务
        INTERNET_FLAG_PASSIVE,              // 被动模式（穿越防火墙/NAT）
        0);
    if (!m_hFtp) {
        m_dwLastError = GetLastError();
        InternetCloseHandle(m_hInternet);
        m_hInternet = nullptr;
        return false;
    }

    return true;
}

void FtpHelper::Disconnect() {
    if (m_hFtp) {
        InternetCloseHandle(m_hFtp);
        m_hFtp = nullptr;
    }
    if (m_hInternet) {
        InternetCloseHandle(m_hInternet);
        m_hInternet = nullptr;
    }
}

bool FtpHelper::UploadFile(const std::wstring& localFile,
                            const std::wstring& remoteFile) {
    if (!m_hFtp) return false;

    // 处理二进制文件：FTP_TRANSFER_TYPE_BINARY
    // ASCII 模式会破坏图片/zip 等二进制文件
    // 可以用 INTERNET_FLAG_TRANSFER_ASCII 或 INTERNET_FLAG_TRANSFER_BINARY
    if (!FtpPutFileW(m_hFtp, localFile.c_str(), remoteFile.c_str(),
                     FTP_TRANSFER_TYPE_BINARY, 0)) {
        m_dwLastError = GetLastError();
        return false;
    }
    return true;
}

bool FtpHelper::DownloadFile(const std::wstring& remoteFile,
                              const std::wstring& localFile) {
    if (!m_hFtp) return false;

    // 若本地文件已存在，FtpGetFile 默认会失败
    // INTERNET_FLAG_RELOAD 强制覆盖
    if (!FtpGetFileW(m_hFtp, remoteFile.c_str(), localFile.c_str(),
                     FALSE,                                    // 不覆盖已存在的文件
                     FILE_ATTRIBUTE_NORMAL,
                     FTP_TRANSFER_TYPE_BINARY,
                     0)) {
        m_dwLastError = GetLastError();
        return false;
    }
    return true;
}

bool FtpHelper::DeleteFile(const std::wstring& remoteFile) {
    if (!m_hFtp) return false;

    if (!FtpDeleteFileW(m_hFtp, remoteFile.c_str())) {
        m_dwLastError = GetLastError();
        return false;
    }
    return true;
}

long long FtpHelper::GetFileSize(const std::wstring& remoteFile) {
    if (!m_hFtp) return -1;

    // 使用 FtpFindFirstFile 获取文件信息
    WIN32_FIND_DATAW findData = {};
    HINTERNET hFind = FtpFindFirstFileW(m_hFtp, remoteFile.c_str(),
                                         &findData, 0, 0);
    if (!hFind) {
        m_dwLastError = GetLastError();
        return -1;
    }
    InternetCloseHandle(hFind);

    // 组合高低位得到 64 位文件大小
    LARGE_INTEGER size;
    size.LowPart  = findData.nFileSizeLow;
    size.HighPart = findData.nFileSizeHigh;
    return size.QuadPart;
}

std::wstring FtpHelper::GetLastErrorMessage() const {
    if (m_dwLastError == 0) return L"";

    // WinInet 错误需要 InternetGetLastResponseInfo 获取详细描述
    if (m_dwLastError >= INTERNET_ERROR_BASE &&
        m_dwLastError <= INTERNET_ERROR_LAST) {
        DWORD dwBufLen = 0;
        InternetGetLastResponseInfoW(&m_dwLastError, nullptr, &dwBufLen);
        if (dwBufLen > 0) {
            std::wstring msg(dwBufLen, L'\0');
            InternetGetLastResponseInfoW(&m_dwLastError, &msg[0], &dwBufLen);
            msg.resize(dwBufLen);
            return msg;
        }
    }

    // 普通 Windows 错误
    wchar_t* buf = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, m_dwLastError, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring msg;
    if (buf) {
        msg.assign(buf, len);
        LocalFree(buf);
    }
    return msg;
}
