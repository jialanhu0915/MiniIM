
// NetworkClientDlg.h: 头文件
//

#pragma once
#include "CConnectSocket.h"

// CNetworkClientDlg 对话框
class CNetworkClientDlg : public CDialogEx
{
// 构造
public:
	CNetworkClientDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NETWORKCLIENT_DIALOG };
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
	CConnectSocket m_connectSocket; // 连接套接字
	void OnReceive();
	void OnClose();
	void UpdateLog(const CString& str);
	void OnConnect();

	afx_msg void OnEnChangeEdit1();
	afx_msg void OnIpnFieldchangedIpaddress1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEnChangeEditPort();
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnEnChangeEditMsg();
};
