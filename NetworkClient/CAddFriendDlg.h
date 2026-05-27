#pragma once
#include "afxdialogex.h"

// CAddFriendDlg 对话框 — 用于输入要添加的好友用户名
class CAddFriendDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CAddFriendDlg)

public:
	CAddFriendDlg(CWnd* pParent = nullptr);
	virtual ~CAddFriendDlg();

	CString m_strFriendName;  // 外部通过此成员读取用户输入

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_ADD_FRIEND };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};
