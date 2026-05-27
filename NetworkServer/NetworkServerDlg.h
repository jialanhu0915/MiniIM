/**
 * @file   NetworkServerDlg.h
 * @brief  即时通讯服务端 - 主界面
 * @author Yan Runxin
 * @date   2026-05-25
 *
 * ## 网络层接口（你需要实现的部分）
 *
 * 服务端 UI 负责显示在线用户和日志。
 * 你的网络层代码在以下时机调用公开方法：
 *
 *   OnUserLogin(userId, username, ip)    — 用户登录成功后
 *   OnUserLogout(userId)                 — 用户断开连接后
 *   UpdateLog(text)                      — 添加日志
 */

#pragma once
#include "CConnectSocket.h"
#include "CListenSocket.h"
#include "SQLiteWrapper.h"
#include "../Common/Protocol.h"
#include <string>
#include <unordered_map>

// 在线用户信息
struct OnlineUser {
    int         userId;
    std::string username;
    std::string ip;
};

class CNetworkServerDlg : public CDialogEx
{
public:
    CNetworkServerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_NETWORKSERVER_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();

public:
    // ================================================================
    //  网络层回调接口
    // ================================================================

    /**
     * @brief  用户登录成功
     */
    void OnUserLogin(int userId, const std::string& username, const std::string& ip);

    /**
     * @brief  用户断开连接
     */
    void OnUserLogout(int userId);

    /**
     * @brief  更新日志
     */
    void UpdateLog(const CString& str);

    /**
     * @brief  更新状态栏
     */
    void UpdateStatus(const CString& text);

    // ================================================================
    //  数据
    // ================================================================

    CListenSocket   m_listenSocket;
    CConnectSocket  m_connectSocket;
    SQLiteWrapper   m_dbWrapper;

    // 在线用户映射
    std::unordered_map<int, OnlineUser> m_onlineUsers;

    // 协议处理
    RecvBuffer          m_recvBuf;
    ProtocolDispatcher  m_dispatcher;

    // ---- 已有网络回调 ----
    void OnAccept();
    void OnReceive();
    void OnClose();

    // ---- 按钮事件 ----
    afx_msg void OnStnClickedStaticPort();
    afx_msg void OnEnChangeEditPort();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonStop();
    afx_msg void OnBnClickedButtonSend();

private:
    void RegisterProtocolHandlers();

    // ---- 控件 ----
    CListCtrl m_userList;   // 在线用户列表

    HICON m_hIcon;
};
