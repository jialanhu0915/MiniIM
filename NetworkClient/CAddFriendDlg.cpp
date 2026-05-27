#include "pch.h"
#include "NetworkClient.h"
#include "afxdialogex.h"
#include "CAddFriendDlg.h"

IMPLEMENT_DYNAMIC(CAddFriendDlg, CDialogEx)

CAddFriendDlg::CAddFriendDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_ADD_FRIEND, pParent)
	, m_strFriendName(_T(""))
{
}

CAddFriendDlg::~CAddFriendDlg()
{
}

void CAddFriendDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_USERNAME_EDIT, m_strFriendName);
}

BEGIN_MESSAGE_MAP(CAddFriendDlg, CDialogEx)
END_MESSAGE_MAP()

BOOL CAddFriendDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	return TRUE;
}
