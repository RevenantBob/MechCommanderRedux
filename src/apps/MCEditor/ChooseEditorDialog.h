#pragma once

class MissionRecordSet;

/// <summary>
/// The first dialog of a new document: the solo or the multiplayer mission editor. It ends with the button's id
/// (<see cref="EDITOR_SOLO"/>, <see cref="EDITOR_MULTIPLAYER"/>) or IDCANCEL.
/// </summary>
class ChooseEditorDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_CHOOSE_EDITOR_DIALOG
    };

    /// <summary>The dialog's results, also the views' types for <c>DreditorApp::SwitchView</c>.</summary>
    enum EditorChoice
    {
        EDITOR_SOLO = 0x72,
        EDITOR_MULTIPLAYER = 0x73,
    };

    /// <summary>Creates the dialog over the open mission recordset <paramref name="missions"/>.</summary>
    /// <remarks>0x0042bec0.</remarks>
    explicit ChooseEditorDialog(MissionRecordSet* missions, CWnd* pParent = nullptr);

    /// <summary>Titles the window with the editor's name (string 0).</summary>
    /// <remarks>0x0042bf60.</remarks>
    BOOL OnInitDialog() override;

protected:
    /// <summary>Ends the dialog with <see cref="EDITOR_MULTIPLAYER"/>.</summary>
    afx_msg void OnMultiplayerMissionEditor();

    /// <summary>Ends the dialog with <see cref="EDITOR_SOLO"/>.</summary>
    afx_msg void OnSoloMissionEditor();

    DECLARE_MESSAGE_MAP()

private:
    /// <summary>Not used; constructed -1 (+0x5c).</summary>
    int32_t _unused = -1;
    /// <summary>The mission recordset the caller opened (+0x60); the dialog doesn't use it.</summary>
    MissionRecordSet* _missions = nullptr;
};
