#include "stdafx.h"
#include "ChooseEditorDialog.h"
#include "EditorUtil.h"
#include "resource.h"

ChooseEditorDialog::ChooseEditorDialog(MissionRecordSet* missions, CWnd* pParent)
    : CDialog(IDD, pParent), _missions(missions)
{
}

BEGIN_MESSAGE_MAP(ChooseEditorDialog, CDialog)
ON_BN_CLICKED(IDC_MULTIPLAYER_MISSION_EDITOR, &ChooseEditorDialog::OnMultiplayerMissionEditor)
ON_BN_CLICKED(IDC_SOLO_MISSION_EDITOR, &ChooseEditorDialog::OnSoloMissionEditor)
END_MESSAGE_MAP()

BOOL ChooseEditorDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CString title;
    title.LoadString(IDS_EDITOR_NAME);
    SetWindowText(title);
    return TRUE;
}

void ChooseEditorDialog::OnMultiplayerMissionEditor()
{
    EndDialog(EDITOR_MULTIPLAYER);
}

void ChooseEditorDialog::OnSoloMissionEditor()
{
    EndDialog(EDITOR_SOLO);
}
