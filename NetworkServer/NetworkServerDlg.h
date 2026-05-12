
// NetworkServerDlg.h: 头文件
//

#pragma once
#include "CConnectSocket.h"
#include "CListenSocket.h"

// CNetworkServerDlg 对话框
class CNetworkServerDlg : public CDialogEx
{
// 构造
public:
	CNetworkServerDlg(CWnd* pParent = nullptr);	// 标准构造函数

	CListenSocket m_listenSocket;
	CConnectSocket m_connectSocket;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NETWORKSERVER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnStnClickedStaticPort();
	afx_msg void OnEnChangeEditPort();

	void UpdateLog(const CString& str);
	void OnAccept();
	void OnReceive();
	void OnClose();

	afx_msg void OnBnClickedButtonStart();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonSend();
};
