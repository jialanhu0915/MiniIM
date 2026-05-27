/**
 * @file   NetworkClientDlg.h
 * @brief  即时通讯客户端 - 主界面
 * @author Yan Runxin
 * @date   2026-05-25
 *
 * ## 网络层接口（你需要实现的部分）
 *
 * 本 UI 类负责界面展示和用户交互，不处理任何 socket 操作。
 * 公开了以下方法供你的网络层代码调用：
 *
 *   // 登录成功（你的网络层收到 LOGIN_RESP 后调用）
 *   OnLoginSuccess(userId, friends);
 *
 *   // 好友上下线（收到 STATUS_ONLINE / STATUS_OFFLINE 后调用）
 *   OnFriendStatusChanged(userId, name, online);
 *
 *   // 收到消息（收到 TEXT / GROUP_TEXT 后调用）
 *   OnMessageReceived(senderId, senderName, content, timestamp);
 *
 *   // 更新好友列表（收到完整列表后调用）
 *   RefreshFriendList(friends);
 */

#pragma once
#include "CConnectSocket.h"
#include "../Common/Protocol.h"
#include <string>
#include <vector>
#include <unordered_map>

// 好友信息
struct FriendInfo {
    int         userId;
    std::string username;
    bool        online;
};

// CNetworkClientDlg 对话框
class CNetworkClientDlg : public CDialogEx
{
public:
    CNetworkClientDlg(CWnd* pParent = nullptr);

    // ---- 对话框数据 ----
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_NETWORKCLIENT_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

    // ---- MFC 消息处理 ----
    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
    // ================================================================
    //  网络层回调接口（你的 socket 代码收到消息后调用这些方法）
    // ================================================================

    /**
     * @brief  登录成功后调用
     * @param userId   服务端分配的用户 ID
     * @param username 登录用户名
     * @param friends  好友列表（含在线状态）
     */
    void OnLoginSuccess(int userId, const std::string& username,
                        const std::vector<FriendInfo>& friends);

    /**
     * @brief  好友在线状态变化
     */
    void OnFriendStatusChanged(int userId, const std::string& name, bool online);

    /**
     * @brief  收到文本消息（单聊或群聊）
     * @param senderName  发送者用户名
     * @param content     消息内容
     * @param timestamp   时间戳
     */
    void OnMessageReceived(const std::string& senderName,
                           const std::string& content,
                           const std::string& timestamp);

    /**
     * @brief  刷新好友列表（服务端推送完整列表时调用）
     */
    void RefreshFriendList(const std::vector<FriendInfo>& friends);

    /**
     * @brief  添加好友到列表
     */
    void AddFriendToList(int userId, const std::string& name, bool online);

    /**
     * @brief  从列表移除好友
     */
    void RemoveFriendFromList(int userId);

    /**
     * @brief  更新状态栏文本
     */
    void UpdateStatus(const CString& text);

    /**
     * @brief  在聊天区显示一条消息（用于发送确认等）
     */
    void AppendChatMessage(const CString& msg);

    // ================================================================
    //  用户当前状态（网络层可读写）
    // ================================================================
    int         m_userId    = -1;
    std::string m_username;

    // ---- RecvBuffer & Dispatcher（网络层在 OnReceive 中使用） ----
    RecvBuffer          m_recvBuf;
    ProtocolDispatcher  m_dispatcher;

    // ---- 网络层自己的 socket（已有）----
    CConnectSocket m_connectSocket;

    // ---- 网络层回调（已有）----
    void OnReceive();
    void OnClose();
    void OnConnect();
    void UpdateLog(const CString& str);



private:
    // ================================================================
    //  UI 控件
    // ================================================================

    // ---- 连接区 ----
    CEdit   m_editUsername;      // 用户名输入

    // ---- 好友列表 ----
    CListBox m_friendList;       // 好友列表框
    CButton  m_btnAddFriend;     // 添加好友按钮
    CButton  m_btnRemoveFriend;  // 删除好友按钮

    // ---- 聊天区 ----
    CEdit   m_chatDisplay;       // 聊天记录显示（多行只读）

    // ---- 文件传输 ----
    CButton m_btnSendFile;       // 发送文件按钮

    // ---- 数据 ----
    std::unordered_map<int, FriendInfo> m_friendMap;  // userId → FriendInfo
    int m_selectedFriendId = -1;                       // 当前选中的好友

    // ================================================================
    //  内部方法
    // ================================================================
    void RegisterProtocolHandlers();  // 注册消息处理器（供你参考）

    // ---- 按钮事件处理 ----
    afx_msg void OnBnClickedButtonConnect();
    afx_msg void OnBnClickedButtonDisconnect();
    afx_msg void OnBnClickedButtonSend();
    afx_msg void OnBnClickedButtonSendFile();
    afx_msg void OnBnClickedButtonAddFriend();
    afx_msg void OnBnClickedButtonRemoveFriend();
    afx_msg void OnCancel();
    afx_msg void OnEnChangeEditMsg();
    afx_msg void OnEnChangeEditPort();
    afx_msg void OnSelChangeListFriends();

    HICON m_hIcon;
};
