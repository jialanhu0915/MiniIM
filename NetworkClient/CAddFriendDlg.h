#pragma once
#include "afxdialogex.h"

/** @file CAddFriendDlg.h @brief 好友添加对话框（输入要添加的好友用户名） @author Yan Runxin @date 2026-05-25 */
/**
 * @brief 好友添加输入对话框
 * @note 模态对话框，用户输入好友用户名后通过 m_strFriendName 传回调用方。
 */
class CAddFriendDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CAddFriendDlg)

public:
	/** @brief 默认构造
	 * @param pParent 父窗口指针（可为空）
	 */
	CAddFriendDlg(CWnd* pParent = nullptr);
	/** @brief 默认析构 */
	virtual ~CAddFriendDlg();

	/** @brief 外部通过此成员读取用户输入（DoModal() 返回 IDOK 后有效） */
	CString m_strFriendName;

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_ADD_FRIEND };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()
};
