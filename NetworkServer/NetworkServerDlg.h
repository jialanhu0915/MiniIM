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
    virtual BOOL PreTranslateMessage(MSG* pMsg);

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

    /**
	* @brief  处理接收到的消息
    */
	void OnMessageReceived(CConnectSocket* pSocket, MsgType msgType, const std::string& json);

    /**
    * @brief 发送消息到客户端
    */
    void SendToClient(CConnectSocket* pSocket, MsgType msgType, const std::string& json);

    /**
	* @brief 处理客户端断开连接
	 * @param[in] pSocket 断开连接的客户端套接字
	 * @note 该方法会在 CConnectSocket::OnClose 中被调用，负责清理相关资源和更新在线用户列表
    */
    void OnClientDisconnected(CConnectSocket* pSocket);

    // ================================================================
    //  数据
    // ================================================================

    CListenSocket   m_listenSocket;
	std::vector<CConnectSocket*> m_connectSockets;  // 活跃连接列表
	CConnectSocket* m_currentSocket = nullptr;   // 临时变量，用于 Accept 时创建新连接
    SQLiteWrapper   m_dbWrapper;

    // 在线用户映射
    std::unordered_map<int, CConnectSocket*>  m_userIdToSocket;
    std::unordered_map<CConnectSocket*, int>  m_socketToUserId;
    std::unordered_map<int, std::string>      m_userIdToName;

    // 协议处理
    ProtocolDispatcher  m_dispatcher;

    // ---- 已有网络回调 ----
    void OnAccept();


    // ---- 按钮事件 ----
    afx_msg void OnStnClickedStaticPort();
    afx_msg void OnEnChangeEditPort();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnBnClickedButtonStop();
    afx_msg void OnBnClickedButtonSend();

private:
    void RegisterProtocolHandlers();
    std::string strGetFriendListJson(int userId);
    // ---- 控件 ----
    CListCtrl m_userList;   // 在线用户列表

    HICON m_hIcon;
};
