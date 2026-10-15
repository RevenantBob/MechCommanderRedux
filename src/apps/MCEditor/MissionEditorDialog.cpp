#include "stdafx.h"
#include "MissionEditorDialog.h"
#include "EditorUtil.h"
#include "resource.h"

MissionEditorDialog::MissionEditorDialog(UINT nIDTemplate, CWnd* pParent) : CDialog(nIDTemplate, pParent)
{
}

BEGIN_MESSAGE_MAP(MissionEditorDialog, CDialog)
ON_COMMAND(IDC_NEW_MISSION, &MissionEditorDialog::OnNewMission)
ON_COMMAND(IDC_EDIT_MISSION, &MissionEditorDialog::OnEditMission)
END_MESSAGE_MAP()

BOOL MissionEditorDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    CString title;
    title.LoadString(IDS_EDITOR_NAME);
    SetWindowText(title);
    ModifyStyle(WS_CAPTION, 0, 0);
    return TRUE;
}

void MissionEditorDialog::OnNewMission()
{
    EndDialog(IDC_NEW_MISSION);
}

void MissionEditorDialog::OnEditMission()
{
    EndDialog(IDC_EDIT_MISSION);
}
