#include "stdafx.h"
#include "OpenDialog.h"
#include "resource.h"

OpenDialog::OpenDialog(CWnd* pParent) : CDialog(IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(OpenDialog, CDialog)
ON_BN_CLICKED(IDC_LOAD_MAP_ONLY, &OpenDialog::OnLoadMapOnly)
ON_BN_CLICKED(IDC_LOAD_MAP_WITH_MISSION_DATA, &OpenDialog::OnLoadMapWithMissionData)
END_MESSAGE_MAP()

void OpenDialog::OnLoadMapOnly()
{
    EndDialog(IDC_LOAD_MAP_ONLY);
}

void OpenDialog::OnLoadMapWithMissionData()
{
    EndDialog(IDC_LOAD_MAP_WITH_MISSION_DATA);
}
