#pragma once

/// <summary>
/// The solo or multiplayer mission editor's start: a new mission or an existing one. It ends with the button's id
/// (IDC_NEW_MISSION, IDC_EDIT_MISSION) or IDCANCEL.
/// </summary>
class MissionEditorDialog : public CDialog
{
public:
    /// <summary>
    /// Creates the dialog from IDD_SOLO_MISSION_EDITOR_DIALOG or IDD_MULTIPLAYER_MISSION_EDITOR_DIALOG.
    /// </summary>
    /// <remarks>0x00426d90.</remarks>
    MissionEditorDialog(UINT nIDTemplate, CWnd* pParent = nullptr);

    /// <summary>Titles the window with the editor's name (string 0) and takes its caption bar away.</summary>
    /// <remarks>0x00426e20.</remarks>
    BOOL OnInitDialog() override;

protected:
    /// <summary>Ends the dialog with IDC_NEW_MISSION.</summary>
    afx_msg void OnNewMission();

    /// <summary>Ends the dialog with IDC_EDIT_MISSION.</summary>
    afx_msg void OnEditMission();

    DECLARE_MESSAGE_MAP()
};
