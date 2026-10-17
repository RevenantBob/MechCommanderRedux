#include "stdafx.h"
#include "ErrorDialog.h"
#include "resource.h"

ErrorDialog::ErrorDialog(CWnd* pParent) : CDialog(IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(ErrorDialog, CDialog)
ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()

void ErrorDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_ERROR_TEXT, errorText);
    DDX_Text(pDX, IDC_ERROR_TEXT, text);
}

void ErrorDialog::OnShowWindow(BOOL bShow, UINT nStatus)
{
    UpdateData(FALSE);
    CDialog::OnShowWindow(bShow, nStatus);
}
