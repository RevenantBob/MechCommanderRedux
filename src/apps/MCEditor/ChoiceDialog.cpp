#include "stdafx.h"
#include "ChoiceDialog.h"

namespace
{
    /// <summary>The second group's label.</summary>
    inline constexpr int SECOND_LABEL = 0x407;
    /// <summary>The first group's first button.</summary>
    inline constexpr int FIRST_GROUP = 0x408;
    /// <summary>The second group's first button.</summary>
    inline constexpr int SECOND_GROUP = 0x40c;
    /// <summary>The second group's frame.</summary>
    inline constexpr int SECOND_FRAME = 0x40d;
}

ChoiceDialog::ChoiceDialog(bool twoGroups, CWnd* pParent) : CDialog(IDD, pParent), twoGroups(twoGroups)
{
}

void ChoiceDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, SECOND_LABEL, secondLabel);
    DDX_Control(pDX, SECOND_GROUP, secondButton);
    DDX_Radio(pDX, FIRST_GROUP, first);
    DDX_Radio(pDX, SECOND_GROUP, second);
}

BEGIN_MESSAGE_MAP(ChoiceDialog, CDialog)
END_MESSAGE_MAP()

BOOL ChoiceDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    UpdateData(FALSE);

    if (!twoGroups)
    {
        secondButton.ShowWindow(SW_HIDE);
        secondLabel.ShowWindow(SW_HIDE);
        GetDlgItem(SECOND_FRAME)->ShowWindow(SW_HIDE);
    }

    return TRUE;
}

void ChoiceDialog::OnOK()
{
    UpdateData(TRUE);
    CDialog::OnOK();
}
