#include "stdafx.h"
#include "EditorSetupDialog.h"
#include "EditorFiles.h"

EditorSetupDialog::EditorSetupDialog(const CString& folder, CWnd* pParent) : CDialog(IDD, pParent), _folder(folder)
{
}

void EditorSetupDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDITOR_SOURCE, _source);
}

BEGIN_MESSAGE_MAP(EditorSetupDialog, CDialog)
ON_BN_CLICKED(IDC_EDITOR_BROWSE, &EditorSetupDialog::OnBrowse)
END_MESSAGE_MAP()

BOOL EditorSetupDialog::OnInitDialog()
{
    _source = FindEditorSetupDisc();
    CDialog::OnInitDialog();

    CString text;
    text.Format("The Mission Editor's own files (its Teditor folder) aren't in %s yet.\n\n"
                "Choose the MechCommander Gold CD (or a mounted copy of it) to install them from the CD's Mission "
                "Editor setup, or a folder where the Mission Editor is installed to copy them from.",
                static_cast<LPCTSTR>(_folder));
    SetDlgItemText(IDC_EDITOR_SETUP_TEXT, text);
    return TRUE;
}

void EditorSetupDialog::OnOK()
{
    if (!UpdateData(TRUE))
    {
        return;
    }

    CWaitCursor wait;
    const auto installed = InstallEditorFiles(_source, _folder);

    if (!installed)
    {
        AfxMessageBox(installed.error().c_str(), MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    CDialog::OnOK();
}

void EditorSetupDialog::OnBrowse()
{
    UpdateData(TRUE);
    CFolderPickerDialog picker(_source.IsEmpty() ? nullptr : static_cast<LPCTSTR>(_source), 0, this);

    if (picker.DoModal() == IDOK)
    {
        _source = picker.GetPathName();
        UpdateData(FALSE);
    }
}
