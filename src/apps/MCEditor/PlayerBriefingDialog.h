#pragma once

class MissionText;

/// <summary>The IDD_PLAYER_BRIEFING_DIALOG dialog: the mission's dispatch, section by section.</summary>
/// <remarks>0x74 bytes, virtual table 0x004fce38.</remarks>
class PlayerBriefingDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_PLAYER_BRIEFING_DIALOG
    };

    /// <summary>Edits a copy of <paramref name="text"/>.</summary>
    /// <remarks>0x004289e0.</remarks>
    PlayerBriefingDialog(const MissionText& text, CWnd* pParent);

    /// <summary>Copies the edited sections to <paramref name="text"/>.</summary>
    /// <remarks>0x00428be0.</remarks>
    void copyTo(MissionText& text) const;

protected:
    /// <remarks>0x00428b60.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Takes the dialog's taskbar button away.</summary>
    /// <remarks>0x00428c50.</remarks>
    BOOL OnInitDialog() override;
    /// <remarks>0x00428c30.</remarks>
    void OnOK() override;

    /// <summary>The sections (+0x5c to +0x6c), as the boxes name them.</summary>
    CString description;
    CString missionTitle;
    CString objectives;
    CString tacticalNotices;
    CString tacticalWarnings;
    /// <summary>The application's icon 128 (+0x70), loaded and not used.</summary>
    HICON icon;

    DECLARE_MESSAGE_MAP()
};
