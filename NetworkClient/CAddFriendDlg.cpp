#include "pch.h"
#include "NetworkClient.h"
#include "afxdialogex.h"
#include "CAddFriendDlg.h"

/** @file CAddFriendDlg.cpp @brief 好友添加对话框实现 */

IMPLEMENT_DYNAMIC(CAddFriendDlg, CDialogEx)

/** @brief 默认构造，初始化成员变量 */
CAddFriendDlg::CAddFriendDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_ADD_FRIEND, pParent)
	, m_strFriendName(_T(""))
{
}

/** @brief 默认析构 */
CAddFriendDlg::~CAddFriendDlg()
{
}

/** @brief MFC 数据交换（DDX）
 * @param pDX 数据交换上下文
 */
void CAddFriendDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_USERNAME_EDIT, m_strFriendName);
}

BEGIN_MESSAGE_MAP(CAddFriendDlg, CDialogEx)
END_MESSAGE_MAP()

/** @brief 对话框初始化回调
 * @return TRUE（由 Windows 处理焦点）
 */
BOOL CAddFriendDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	return TRUE;
}
