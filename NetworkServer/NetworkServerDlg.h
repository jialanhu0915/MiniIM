/**
 * @file   NetworkServerDlg.h
 * @brief  服务端主对话框（管理所有客户端连接）
 * @author Yan Runxin
 * @date   2026-05-25
 */

#pragma once

#include "CConnectSocket.h"
#include "CListenSocket.h"
#include "CFtpListenSocket.h"
#include "SQLiteWrapper.h"
#include "../Common/Protocol.h"
#include <string>
#include <unordered_map>

// 自定义消息 ID
#define WM_USER_UPDATE_UI  (WM_USER + 1)

// 告诉 UI 线程"要做什么"
enum class UIUpdateType : WPARAM {
	LOG = 1,
	LOGIN = 2,
	LOGOUT = 3,
	STATUS_TEXT = 4,
};

// 跨线程 UI 更新数据结构
struct UIUpdateData {
	UIUpdateType type;
	int          userId = -1;
	CString      text;
	std::string  username;
	std::string  ip;
};

/**
 * @brief 服务端主对话框
 * @note 管理监听socket、客户端连接map、两把锁（m_csData/m_csDb）
 */
// CNetworkServerDlg 类
class CNetworkServerDlg : public CDialogEx
{
public:
	/**
	 * @brief  构造函数，初始化对话框
	 * @param  pParent  父窗口指针（默认为nullptr）
	 */
	CNetworkServerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NETWORKSERVER_DIALOG };
#endif

protected:
	/**
	 * @brief  MFC数据交换，用于控件与变量绑定
	 * @param  pDX  数据交换对象[in/out]
	 */
	virtual void DoDataExchange(CDataExchange* pDX);
	DECLARE_MESSAGE_MAP()

	/**
	 * @brief  对话框初始化，完成监听socket和协议处理器的注册
	 * @return TRUE表示初始化成功，FALSE表示失败
	 */
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	/**
	 * @brief  绘制对话框，响应WM_PAINT消息
	 */
	afx_msg void OnPaint();
	/**
	 * @brief  查询拖拽图标，返回光标句柄
	 * @return 光标句柄
	 */
	afx_msg HCURSOR OnQueryDragIcon();
	/**
	 * @brief  消息预处理，用于捕获回车键等
	 * @param  pMsg  消息结构体指针[in]
	 * @return TRUE表示消息已处理，FALSE表示继续传递
	 */
	virtual BOOL PreTranslateMessage(MSG* pMsg);

public:
	/**
	 * @brief  用户登录时更新UI（跨线程调用）
	 * @param  userId    用户ID[in]
	 * @param  username 用户名[in]
	 * @param  ip        IP地址[in]
	 */
	void OnUserLoginUI(int userId, const std::string& username, const std::string& ip);
	/**
	 * @brief  用户登出时更新UI（跨线程调用）
	 * @param  userId    用户ID[in]
	 * @param  username 用户名[in]
	 */
	void OnUserLogoutUI(int userId, const std::string& username);
	/**
	 * @brief  更新日志显示
	 * @param  str  日志内容[in]
	 */
	void UpdateLog(const CString& str);
	/**
	 * @brief  更新状态栏文本
	 * @param  text  状态文本[in]
	 */
	void UpdateStatus(const CString& text);

	/**
	 * @brief  分发消息到对应处理器
	 * @param  pSocket  客户端socket指针[in]
	 * @param  msgType  消息类型[in]
	 * @param  json     JSON消息体[in]
	 */
	void DispatchMessage(CConnectSocket* pSocket, MsgType msgType, const std::string& json);
	/**
	 * @brief  向客户端发送消息
	 * @param  pSocket  目标socket指针[in]
	 * @param  msgType  消息类型[in]
	 * @param  json     JSON消息体[in]
	 */
	void SendToClient(CConnectSocket* pSocket, MsgType msgType, const std::string& json);
	/**
	 * @brief  客户端断开连接时的回调
	 * @param  pSocket  断开的socket指针[in]
	 */
	void OnClientDisconnected(CConnectSocket* pSocket);

	/**
	 * @brief  投递UI更新消息到消息队列
	 * @param  type     更新类型[in]
	 * @param  text     文本内容（可选）[in]
	 * @param  userId   用户ID（可选）[in]
	 * @param  username 用户名（可选）[in]
	 * @param  ip       IP地址（可选）[in]
	 */
	void PostUIUpdate(UIUpdateType type, const CString& text = _T(""),
		int userId = -1, const std::string& username = "",
		const std::string& ip = "");

	/**
	 * @brief  UI更新消息处理函数
	 * @param  wParam  参数1[in]
	 * @param  lParam  参数2[in]
	 * @return 消息处理结果
	 */
	afx_msg LRESULT OnUIUpdate(WPARAM wParam, LPARAM lParam);
	/**
	 * @brief  对话框销毁时清理资源
	 */
	afx_msg void OnDestroy();

	CListenSocket        m_listenSocket;         ///< 监听socket，管理客户端接入
	CFtpListenSocket     m_ftpListenSocket;       ///< FTP 监听 socket（端口 2121），用于文件传输中转
	std::vector<CConnectSocket*> m_connectSockets;  ///< 当前所有客户端连接
	SQLiteWrapper        m_dbWrapper;             ///< SQLite数据库封装

	///< userId到socket的映射
	std::unordered_map<int, CConnectSocket*>  m_userIdToSocket;
	///< socket到userId的映射
	std::unordered_map<CConnectSocket*, int>  m_socketToUserId;
	///< userId到用户名的映射
	std::unordered_map<int, std::string>      m_userIdToName;

	ProtocolDispatcher   m_dispatcher;            ///< 协议消息分发器
	CCriticalSection     m_csData;               ///< 保护内存map/socket列表的临界区
	volatile LONG        m_bRunning = FALSE;      ///< 服务端运行状态标志

	///< 服务端监听端口显示控件点击处理
	afx_msg void OnStnClickedStaticPort();
	///< 端口输入框内容改变处理
	afx_msg void OnEnChangeEditPort();
	///< 启动服务按钮点击处理
	afx_msg void OnBnClickedButtonStart();
	///< 停止服务按钮点击处理
	afx_msg void OnBnClickedButtonStop();

private:
	/**
	 * @brief  注册所有协议处理器到分发器
	 */
	void RegisterProtocolHandlers();

	CListCtrl m_userList;   ///< 用户列表控件（显示在线用户）
	HICON     m_hIcon;       ///< 对话框图标句柄
};
