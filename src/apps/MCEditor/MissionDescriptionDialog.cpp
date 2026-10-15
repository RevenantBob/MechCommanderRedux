#include "stdafx.h"
#include "MissionDescriptionDialog.h"
#include "resource.h"

namespace
{
    /// <summary>The longest description the box takes.</summary>
    inline constexpr WPARAM MAX_TYPED = 360;
    /// <summary>The buffer the box's text is read into.</summary>
    inline constexpr int TEXT_BUFFER = 4000;
}

MissionDescriptionDialog::MissionDescriptionDialog(CWnd* pParent) : CDialog(IDD, pParent), description("")
{
}

BEGIN_MESSAGE_MAP(MissionDescriptionDialog, CDialog)
END_MESSAGE_MAP()

void MissionDescriptionDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_MISSION_DESCRIPTION_TEXT, edit);
    DDX_Text(pDX, IDC_MISSION_DESCRIPTION_TEXT, description);
}

BOOL MissionDescriptionDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    UpdateData(FALSE);
    edit.SendMessage(EM_LIMITTEXT, MAX_TYPED, 0);
    return TRUE;
}

void MissionDescriptionDialog::OnOK()
{
    UpdateData(TRUE);

    // The text is read again with the box's soft line breaks ("\r\r\n" where a line wraps).
    edit.SendMessage(EM_FMTLINES, TRUE, 0);
    edit.GetWindowText(description.GetBuffer(TEXT_BUFFER), TEXT_BUFFER);
    description.ReleaseBuffer(-1);
    CDialog::OnOK();
}
