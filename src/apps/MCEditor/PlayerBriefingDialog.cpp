#include "stdafx.h"
#include "PlayerBriefingDialog.h"
#include "MissionText.h"
#include "resource.h"

namespace
{
    /// <summary>The longest title.</summary>
    inline constexpr int MAX_TITLE = 0x80;
    /// <summary>The icon the dialog loads.</summary>
    inline constexpr UINT DIALOG_ICON = 0x80;
}

PlayerBriefingDialog::PlayerBriefingDialog(const MissionText& text, CWnd* pParent)
    : CDialog(IDD, pParent)
    , description(text.fireSupport)
    , missionTitle(text.title)
    , objectives(text.dispatch)
    , tacticalNotices(text.closing)
    , tacticalWarnings(text.tacticalNotice)
    , icon(AfxGetApp()->LoadIcon(DIALOG_ICON))
{
}

void PlayerBriefingDialog::copyTo(MissionText& text) const
{
    text.fireSupport = description;
    text.title = missionTitle;
    text.dispatch = objectives;
    text.closing = tacticalNotices;
    text.tacticalNotice = tacticalWarnings;
}

BEGIN_MESSAGE_MAP(PlayerBriefingDialog, CDialog)
END_MESSAGE_MAP()

void PlayerBriefingDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_DESCRIPTION, description);
    DDX_Text(pDX, IDC_MISSION_TITLE, missionTitle);
    DDV_MaxChars(pDX, missionTitle, MAX_TITLE);
    DDX_Text(pDX, IDC_OBJECTIVES, objectives);
    DDX_Text(pDX, IDC_TACTICAL_NOTICES, tacticalNotices);
    DDX_Text(pDX, IDC_TACTICAL_WARNINGS, tacticalWarnings);
}

BOOL PlayerBriefingDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    ModifyStyleEx(WS_EX_APPWINDOW, 0, 0);
    UpdateData(FALSE);
    return TRUE;
}

void PlayerBriefingDialog::OnOK()
{
    UpdateData(TRUE);
    CDialog::OnOK();
}
