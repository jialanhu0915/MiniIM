/**
 * @file   NetworkServerDlg.h
 * @brief  即时通讯服务端 - 主界面（多线程版）
 * @author Yan Runxin
 * @date   2026-05-26
 */

#pragma once

#include "CConnectSocket.h"
#include "CListenSocket.h"
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

// CNetworkServerDlg 类
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
	void OnUserLoginUI(int userId, const std::string& username, const std::string& ip);
	void OnUserLogoutUI(int userId, const std::string& username);
	void UpdateLog(const CString& str);
	void UpdateStatus(const CString& text);

	void DispatchMessage(CConnectSocket* pSocket, MsgType msgType, const std::string& json);
	void SendToClient(CConnectSocket* pSocket, MsgType msgType, const std::string& json);
	void OnClientDisconnected(CConnectSocket* pSocket);

	void PostUIUpdate(UIUpdateType type, const CString& text = _T(""),
		int userId = -1, const std::string& username = "",
		const std::string& ip = "");

	afx_msg LRESULT OnUIUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();

	CListenSocket        m_listenSocket;
	std::vector<CConnectSocket*> m_connectSockets;
	SQLiteWrapper        m_dbWrapper;

	std::unordered_map<int, CConnectSocket*>  m_userIdToSocket;
	std::unordered_map<CConnectSocket*, int>  m_socketToUserId;
	std::unordered_map<int, std::string>      m_userIdToName;

	ProtocolDispatcher   m_dispatcher;
	CCriticalSection     m_csData;
	volatile LONG        m_bRunning = FALSE;

	afx_msg void OnStnClickedStaticPort();
	afx_msg void OnEnChangeEditPort();
	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonStop();

private:
	void RegisterProtocolHandlers();
	std::string strGetFriendListJson(int userId);

	CListCtrl m_userList;
	HICON     m_hIcon;
};
