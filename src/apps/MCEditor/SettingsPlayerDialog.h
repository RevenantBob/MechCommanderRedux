#pragma once

class MissionText;
struct MissionSettings;

/// <summary>
/// The IDD_SETTINGS_PLAYER_DIALOG dialog: a side's fire support, resource points, drop weight, time limit and
/// technology base, and its briefing (<see cref="PlayerBriefingDialog"/>).
/// </summary>
/// <remarks>0x270 bytes, virtual table 0x004fd7b0.</remarks>
class SettingsPlayerDialog : public CDialog
{
public:
    /// <summary>The dialog template.</summary>
    enum
    {
        IDD = IDD_SETTINGS_PLAYER_DIALOG
    };

    /// <summary>
    /// The settings <paramref name="settings"/> and briefing <paramref name="text"/> of a side, titled with string
    /// <paramref name="titleId"/>.
    /// </summary>
    /// <param name="multiplayer">
    /// A multiplayer side: no technology base or camera drones, and a drop weight of at most 990 (else 1200).
    /// </param>
    /// <param name="hasTimeLimit">Whether the time limit is shown.</param>
    /// <remarks>0x0042d9e0.</remarks>
    SettingsPlayerDialog(MissionText& text, MissionSettings& settings, bool multiplayer, UINT titleId, CWnd* pParent,
                         bool hasTimeLimit);

protected:
    /// <summary>
    /// Exchanges the values with their ranges; a time limit of 1 or 2 minutes is refused (also when the dialog
    /// opens).
    /// </summary>
    /// <remarks>0x0042dba0.</remarks>
    void DoDataExchange(CDataExchange* pDX) override;
    /// <summary>Hides what the side doesn't have, titles the dialog and limits the boxes' lengths.</summary>
    /// <remarks>0x0042dec0.</remarks>
    BOOL OnInitDialog() override;
    /// <summary>Copies the values to the settings (the time limit in seconds).</summary>
    /// <remarks>0x0042de10.</remarks>
    void OnOK() override;

    /// <summary>Handles ON_BN_CLICKED(IDC_PLAYER_BRIEFING): edits the briefing, kept when OK is pressed.</summary>
    /// <remarks>0x0042e0b0.</remarks>
    afx_msg void OnPlayerBriefing();

    /// <summary>The boxes and the technology base label (+0x5c to +0x200).</summary>
    CEdit smallArtilleryBox;
    CEdit sensorProbesBox;
    CEdit resourcePointsBox;
    CEdit largeArtilleryBox;
    CEdit dropWeightBox;
    CEdit cameraDronesBox;
    CEdit timeLimitBox;
    CStatic technologyBaseLabel;
    /// <summary>The values being edited (+0x23c to +0x254); the time limit in minutes.</summary>
    int cameraDrones;
    int dropWeight;
    int largeArtillery;
    int resourcePoints;
    int sensorProbes;
    int smallArtillery;
    UINT timeLimitMinutes;
    /// <summary>The technology base buttons' choice: 0 Inner Sphere (no Uller), 1 Clan (+0x258).</summary>
    int technologyBase;
    /// <summary>The settings (+0x25c).</summary>
    MissionSettings& settings;
    /// <summary>The briefing (+0x260).</summary>
    MissionText& text;
    /// <summary>A multiplayer side (+0x264).</summary>
    bool multiplayer;
    /// <summary>The title's string (+0x268).</summary>
    UINT titleId;
    /// <summary>Whether the time limit is shown (+0x26c).</summary>
    bool hasTimeLimit;

    DECLARE_MESSAGE_MAP()
};
