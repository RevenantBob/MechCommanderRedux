#include "stdafx.h"
#include "ValueDialog.h"

namespace
{
    /// <summary>The first value's box.</summary>
    inline constexpr int FIRST_EDIT = 0x406;
    /// <summary>The second value's label.</summary>
    inline constexpr int SECOND_LABEL = 0x407;
    /// <summary>The second value's box.</summary>
    inline constexpr int SECOND_EDIT = 0x409;
}

ValueDialog::ValueDialog(const CString& title, bool twoValues, UINT firstMin, UINT firstMax, UINT secondMin,
                         UINT secondMax, CWnd* pParent)
    : CDialog(IDD, pParent)
    , twoValues(twoValues)
    , title(title)
    , firstMin(firstMin)
    , firstMax(firstMax)
    , secondMin(secondMin)
    , secondMax(secondMax)
{
}

CString ValueDialog::commandTitle(UINT command)
{
    CString text;
    CMenu::FromHandle(::GetMenu(AfxGetMainWnd()->m_hWnd))->GetMenuString(command, text, MF_BYCOMMAND);

    if (!text.IsEmpty() && text[0] == '&')
    {
        text = text.Right(text.GetLength() - 1);
    }

    return text;
}

void ValueDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, SECOND_EDIT, secondEdit);
    DDX_Control(pDX, SECOND_LABEL, secondLabel);
    DDX_Text(pDX, FIRST_EDIT, first);
    DDX_Text(pDX, SECOND_EDIT, second);

    if (firstMin != NO_RANGE)
    {
        DDV_MinMaxDWord(pDX, first, firstMin, firstMax);
    }

    if (secondMin != NO_RANGE)
    {
        DDV_MinMaxDWord(pDX, second, secondMin, secondMax);
    }
}

BEGIN_MESSAGE_MAP(ValueDialog, CDialog)
END_MESSAGE_MAP()

BOOL ValueDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    UpdateData(FALSE);
    SetWindowText(title);

    if (!twoValues)
    {
        secondEdit.ShowWindow(SW_HIDE);
        secondLabel.ShowWindow(SW_HIDE);
    }

    return TRUE;
}

void ValueDialog::OnOK()
{
    UpdateData(TRUE);
    CDialog::OnOK();
}
