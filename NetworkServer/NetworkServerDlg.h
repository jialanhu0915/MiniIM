/**
 * @file   NetworkServerDlg.h
 * @brief  即时通讯服务端 - 主界面（多线程版）
 * @author Yan Runxin
 * @date   2026-05-26
 *
 * ## 架构
 *
 *   主线程 (UI)
 *   ├─ ListenThread:  阻塞 accept() —— 每接入一个客户端就创建 ClientThread
 *   ├─ ClientThread1: 阻塞 recv()  —— RecvBuffer 切包 → DispatchMessage → dispatcher
 *   ├─ ClientThread2: 阻塞 recv()
 *   └─ ClientThread3: ...
 *
 * ## 数据保护
 *
 *   m_csData (CCriticalSection) 保护以下共享数据：
 *   - m_userIdToSocket / m_socketToUserId / m_userIdToName
 *   - m_connectSockets
 *   - m_dbWrapper 的所有调用
 *
 *   重要规则：持有 m_csData 期间绝不调用 send()（send 可能阻塞，会卡住所有线程）
 *   需要广播时：先加锁复制 socket 列表 → 解锁 → 逐个发送
 *
 * ## UI 更新
 *
 *   Worker 线程不能直接操作 MFC 控件 → 通过 PostMessage(WM_USER_UPDATE_UI) 投递到 UI 线程
 *   PostUIUpdate() 负责 new UIUpdateData + PostMessage
 *   OnUIUpdate()  负责接收 → unique_ptr 释放 → 调用具体 UI 方法
 */

#pragma once
#include "CConnectSocket.h"
#include "CListenSocket.h"
#include "SQLiteWrapper.h"
#include "../Common/Protocol.h"
#include <string>
#include <unordered_map>

 // ============================================================================
 // 跨线程 UI 更新机制
 // ============================================================================

 // 自定义消息 ID：worker 线程投递此消息到 UI 线程
#define WM_USER_UPDATE_UI  (WM_USER + 1)

// 告诉 UI 线程"要做什么"（通过 WPARAM 传递）
enum class UIUpdateType : WPARAM {
	LOG = 1,   // Append 日志到 m_editLog
	LOGIN = 2,   // 插入一行到 m_userList（在线用户列表）
	LOGOUT = 3,   // 从 m_userList 删除一行
	STATUS_TEXT = 4,   // 更新状态栏文字
};

// 堆上分配，通过 LPARAM 传递；UI 线程用 unique_ptr 接收保证不泄漏
struct UIUpdateData {
	UIUpdateType type;          // 操作类型
	int          userId = -1;   // 用户 ID（LOGIN/LOGOUT 时用）
	CString      text;          // 日志文本 / 状态文本（LOG/STATUS 时用）
	std::string  username;      // 用户名（LOGIN 时用）
	std::string  ip;            // IP 地址（LOGIN 时用）
};

// ============================================================================
// CNetworkServerDlg
// ============================================================================

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
	//  第一组：纯 UI 方法（只能在 UI 线程调用，直接操作控件）
	// ================================================================

	/// 用户在登录 → UI 在线列表新增一行
	void OnUserLoginUI(int userId, const std::string& username, const std::string& ip);

	/// 用户下线 → UI 在线列表移除对应行
	void OnUserLogoutUI(int userId, const std::string& username);

	/// 追加一行日志 + 自动滚动
	void UpdateLog(const CString& str);

	/// 更新状态栏文字
	void UpdateStatus(const CString& text);

	// ================================================================
	//  第二组：网络层方法（多线程安全，通过 m_csData 保护共享数据）
	// ================================================================

	/// 接收并分发一条完整消息（由 CConnectSocket::RecvLoop 调用，运行在 ClientThread）
	/// 内部：PostMessage 记录日志 → m_dispatcher.dispatch 调用 handler
	void DispatchMessage(CConnectSocket* pSocket, MsgType msgType, const std::string& json);

	/// 向指定客户端发送协议消息（调用 CConnectSocket::Send，阻塞写入）
	/// ⚠ 调用方必须在不持有 m_csData 的前提下调用，否则阻塞会卡住所有线程
	void SendToClient(CConnectSocket* pSocket, MsgType msgType, const std::string& json);

	/// 清理断开连接的客户端（由 CConnectSocket::ThreadProc 在线程退出时调用）
	/// 负责：从 maps 移除 → 广播 STATUS_OFFLINE → 从 m_connectSockets 移除
	void OnClientDisconnected(CConnectSocket* pSocket);

	// ================================================================
	//  第三组：跨线程 UI 中转（任意线程安全，通过 PostMessage 投递）
	// ================================================================

	/// 从任意线程投递 UI 更新到主线程
	/// @param type     操作类型
	/// @param text     日志/状态文字
	/// @param userId   用户ID（LOGIN/LOGOUT）
	/// @param username 用户名
	/// @param ip       IP 地址
	void PostUIUpdate(UIUpdateType type, const CString& text = _T(""),
		int userId = -1, const std::string& username = "",
		const std::string& ip = "");

	// ================================================================
	//  第四组：消息处理
	// ================================================================

	/// WM_USER_UPDATE_UI 消息处理（UI 线程执行）
	/// wParam = UIUpdateType,  lParam = UIUpdateData*（此函数负责 delete）
	afx_msg LRESULT OnUIUpdate(WPARAM wParam, LPARAM lParam);

	/// 窗口关闭时自动停止所有线程（防止后台线程还持有 this 指针）
	afx_msg void OnDestroy();

	// ================================================================
	//  数据成员
	// ================================================================

	CListenSocket        m_listenSocket;          // 监听 socket + accept 线程
	std::vector<CConnectSocket*> m_connectSockets; // 活跃客户端列表（加锁访问）
	SQLiteWrapper        m_dbWrapper;             // 数据库（加锁访问）

	// 在线用户三大映射（加锁访问）
	std::unordered_map<int, CConnectSocket*>  m_userIdToSocket;    // userId → socket
	std::unordered_map<CConnectSocket*, int>  m_socketToUserId;    // socket → userId
	std::unordered_map<int, std::string>      m_userIdToName;      // userId → 用户名

	ProtocolDispatcher   m_dispatcher;           // 消息分发器（无共享状态，线程安全）

	CCriticalSection     m_csData;               // 保护以上所有共享数据
	volatile LONG        m_bRunning = FALSE;     // 服务运行标志，关闭时设置为 FALSE

	// ================================================================
	//  按钮事件
	// ================================================================

	afx_msg void OnStnClickedStaticPort();
	afx_msg void OnEnChangeEditPort();
	afx_msg void OnBnClickedButtonStart();        // 启动服务（创建 ListenThread）
	afx_msg void OnBnClickedButtonStop();         // 停止服务（分阶段关闭所有线程）

private:
	void RegisterProtocolHandlers();               // 注册所有 MsgType → handler 映射
	std::string strGetFriendListJson(int userId);  // 从 DB 拼装好友列表 JSON

	// ---- 控件 ----
	CListCtrl m_userList;   // 在线用户列表控件
	HICON     m_hIcon;
};
