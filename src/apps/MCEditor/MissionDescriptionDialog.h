#pragma once

/// <summary>The IDD_MISSION_DESCRIPTION_DIALOG dialog: a multiplayer mission's description (Mission &gt; Description).</summary>
/// <remarks>0x9c bytes, virtual table 0x004febe8.</remarks>
class MissionDescriptionDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_MISSION_DESCRIPTION_DIALOG
    };

    /// <summary>Creates the dialog with an empty description.</summary>
    /// <remarks>0x004607c0.</remarks>
    explicit MissionDescriptionDialog(CWnd* pParent = nullptr);

    /// <summary>The description (+0x98).</summary>
    CString description;

protected:
    /// <remarks>0x00460860.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Limits the box to 360 characters.</summary>
    /// <remarks>0x00460900.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>
    /// Takes the box's text with its soft line breaks (EM_FMTLINES) as the description, at most 3999 characters.
    /// </summary>
    /// <remarks>0x004608a0.</remarks>
    void OnOK() override;

    /// <summary>The description's box (+0x5c).</summary>
    CEdit edit;

    DECLARE_MESSAGE_MAP()
};
