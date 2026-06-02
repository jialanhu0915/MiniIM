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

/**
 * @brief 好友信息结构体
 */
struct FriendInfo {
    int         userId;    ///< 用户 ID
    std::string username;  ///< 用户名
    bool        online;   ///< 是否在线
};

/**
 * @brief 主界面对话框
 */
class CNetworkClientDlg : public CDialogEx
{
public:
    /** @brief 默认构造函数 */
    CNetworkClientDlg(CWnd* pParent = nullptr);

    // ---- 对话框数据 ----
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_NETWORKCLIENT_DIALOG };
#endif

protected:
    /** @brief MFC 数据交换（DDX/DDV） @param pDX 数据交换上下文 */
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()

    // ---- MFC 消息处理 ----
    /** @brief 对话框初始化，创建控件 */
    virtual BOOL OnInitDialog();
    /** @brief 系统命令处理 @param nID 命令 ID @param lParam 命令参数 */
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    /** @brief 窗口重绘处理 */
    afx_msg void OnPaint();
    /** @brief 查询拖拽图标 @return HCURSOR 图标句柄 */
    afx_msg HCURSOR OnQueryDragIcon();
    /** @brief 按键预处理（拦截 Enter 发送消息） @param pMsg 消息结构体 @return TRUE 已处理，FALSE 继续传递 */
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
     * @param userId 好友用户 ID
     * @param name   好友用户名
     * @param online  是否在线
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
     * @param friends 好友信息数组
     */
    void RefreshFriendList(const std::vector<FriendInfo>& friends);

    /**
     * @brief  添加好友到列表
     * @param userId 好友用户 ID
     * @param name   好友用户名
     * @param online 是否在线
     */
    void AddFriendToList(int userId, const std::string& name, bool online);

    /**
     * @brief  从列表移除好友
     * @param userId 好友用户 ID
     */
    void RemoveFriendFromList(int userId);

    /**
     * @brief  更新状态栏文本
     * @param text 状态栏文本
     */
    void UpdateStatus(const CString& text);

    /**
     * @brief  在聊天区显示一条消息（用于发送确认等）
     * @param msg 聊天消息文本
     */
    void AppendChatMessage(const CString& msg);

    // ================================================================
    //  用户当前状态（网络层可读写）
    // ================================================================
    int         m_userId    = -1;         ///< 当前登录用户 ID
    std::string m_username;                ///< 当前登录用户名
    bool        m_bConnecting = false;    ///< 正在连接中（防止重复点连接）

    // ---- RecvBuffer & Dispatcher（网络层在 OnReceive 中使用） ----
    RecvBuffer          m_recvBuf;     ///< 接收缓冲区
    ProtocolDispatcher  m_dispatcher;  ///< 协议消息分发器

    // ---- 网络层自己的 socket（已有）----
    CConnectSocket m_connectSocket;    ///< 到服务端连接 socket

    // ---- 网络层回调（已有）----
    /** @brief 接收到数据回调 */
    void OnReceive();
    /** @brief 连接关闭回调 */
    void OnClose();
    /** @brief 网络连接成功回调 */
    void OnConnect();
    /** @brief 网络连接失败回调 @param nErrorCode 错误码 */
    void OnConnectError(int nErrorCode);
    /** @brief 在日志区追加文本 @param str 日志内容 */
    void UpdateLog(const CString& str);



private:
    // ================================================================
    //  UI 控件
    // ================================================================

    // ---- 连接区 ----
    CEdit   m_editUsername;      // 用户名输入

    // ---- 好友列表 ----
    CListBox m_friendList;           ///< 好友列表框
    CButton  m_btnAddFriend;        ///< 添加好友按钮
    CButton  m_btnRemoveFriend;     ///< 删除好友按钮

    // ---- 聊天区（复用 IDC_EDIT_LOG，m_chatDisplay 字段已删除）----

    // ---- 文件传输 ----
    CButton m_btnSendFile;           ///< 发送文件按钮

    // ---- 数据 ----
    std::unordered_map<int, FriendInfo> m_friendMap;  ///< userId 到好友信息映射
    int m_selectedFriendId = -1;                       ///< 当前选中好友 ID

    // ================================================================
    //  内部方法
    // ================================================================
    /** @brief 注册所有协议消息处理器 */
    void RegisterProtocolHandlers();

    // ---- 按钮事件处理 ----
    /** @brief 连接/断开按钮点击 */
    afx_msg void OnBnClickedButtonConnect();
    /** @brief 断开服务器连接 */
    afx_msg void OnBnClickedButtonDisconnect();
    /** @brief 发送消息按钮点击 */
    afx_msg void OnBnClickedButtonSend();
    /** @brief 发送文件按钮点击 */
    afx_msg void OnBnClickedButtonSendFile();
    /** @brief 添加好友按钮点击 */
    afx_msg void OnBnClickedButtonAddFriend();
    /** @brief 删除好友按钮点击 */
    afx_msg void OnBnClickedButtonRemoveFriend();
    /** @brief 对话框关闭 */
    afx_msg void OnCancel();
    /** @brief 消息编辑框变化 @note 预留，当前为空 */
    afx_msg void OnEnChangeEditMsg();
    /** @brief 端口编辑框变化 @note 预留，当前为空 */
    afx_msg void OnEnChangeEditPort();
    /** @brief 好友列表选中项变化 */
    afx_msg void OnSelChangeListFriends();

    HICON m_hIcon;  ///< 程序图标句柄
};
