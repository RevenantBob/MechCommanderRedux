#include "stdafx.h"
#include "SettingsPlayerDialog.h"
#include "EditorMap.h"
#include "MissionText.h"
#include "PlayerBriefingDialog.h"
#include "resource.h"

namespace
{
    /// <summary>The caption of the dialogs' message boxes ("MCEditor").</summary>
    inline constexpr UINT MESSAGE_CAPTION = IDS_MCEDITOR;
    /// <summary>The fire support counts' range.</summary>
    inline constexpr int MAX_SUPPORT = 5;
    /// <summary>The drop weight's range: a multiplayer side's limit, else a solo one's.</summary>
    inline constexpr int MIN_DROP_WEIGHT = 5;
    inline constexpr int MAX_MULTIPLAYER_DROP_WEIGHT = 0x3de;
    inline constexpr int MAX_SOLO_DROP_WEIGHT = 0x4b0;
    /// <summary>The resource points' range.</summary>
    inline constexpr int MAX_RESOURCE_POINTS = 9999999;
    /// <summary>The shortest time limit there can be (shorter ones but 0 are refused).</summary>
    inline constexpr UINT MIN_TIME_LIMIT = 3;
    /// <summary>Seconds in a minute.</summary>
    inline constexpr long SECONDS_PER_MINUTE = 0x3c;
    /// <summary>The boxes' lengths.</summary>
    inline constexpr UINT SUPPORT_DIGITS = 1;
    inline constexpr UINT SOLO_DROP_WEIGHT_DIGITS = 4;
    inline constexpr UINT RESOURCE_POINT_DIGITS = 7;
    inline constexpr UINT TIME_LIMIT_DIGITS = 3;
}

SettingsPlayerDialog::SettingsPlayerDialog(MissionText& text, MissionSettings& settings, bool multiplayer, UINT titleId,
                                           CWnd* pParent, bool hasTimeLimit)
    : CDialog(IDD, pParent)
    , cameraDrones(settings.artilleryCamera)
    , dropWeight(settings.playerForceLimit)
    , largeArtillery(settings.artilleryLarge)
    , resourcePoints(settings.resourcePoints)
    , sensorProbes(settings.artillerySensor)
    , smallArtillery(settings.artillerySmall)
    , timeLimitMinutes(settings.timeLimit / SECONDS_PER_MINUTE)
    , technologyBase(settings.noUller ? 0 : 1)
    , settings(settings)
    , text(text)
    , multiplayer(multiplayer)
    , titleId(titleId)
    , hasTimeLimit(hasTimeLimit)
{
}

BEGIN_MESSAGE_MAP(SettingsPlayerDialog, CDialog)
ON_BN_CLICKED(IDC_PLAYER_BRIEFING, &SettingsPlayerDialog::OnPlayerBriefing)
END_MESSAGE_MAP()

void SettingsPlayerDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Control(pDX, IDC_SMALL_ARTILLERY, smallArtilleryBox);
    DDX_Control(pDX, IDC_SENSOR_PROBES, sensorProbesBox);
    DDX_Control(pDX, IDC_RESOURCE_POINTS, resourcePointsBox);
    DDX_Control(pDX, IDC_LARGE_ARTILLERY, largeArtilleryBox);
    DDX_Control(pDX, IDC_DROP_WEIGHT, dropWeightBox);
    DDX_Control(pDX, IDC_CAMERA_DRONES, cameraDronesBox);
    DDX_Control(pDX, IDC_TECHNOLOGY_BASE, technologyBaseLabel);
    DDX_Control(pDX, IDC_TIME_LIMIT_MIN, timeLimitBox);
    DDX_Text(pDX, IDC_CAMERA_DRONES, cameraDrones);
    DDV_MinMaxInt(pDX, cameraDrones, 0, MAX_SUPPORT);
    DDX_Text(pDX, IDC_DROP_WEIGHT, dropWeight);
    DDV_MinMaxInt(pDX, dropWeight, MIN_DROP_WEIGHT, multiplayer ? MAX_MULTIPLAYER_DROP_WEIGHT : MAX_SOLO_DROP_WEIGHT);
    DDX_Text(pDX, IDC_LARGE_ARTILLERY, largeArtillery);
    DDV_MinMaxInt(pDX, largeArtillery, 0, MAX_SUPPORT);
    DDX_Text(pDX, IDC_RESOURCE_POINTS, resourcePoints);
    DDV_MinMaxInt(pDX, resourcePoints, 0, MAX_RESOURCE_POINTS);
    DDX_Text(pDX, IDC_SENSOR_PROBES, sensorProbes);
    DDV_MinMaxInt(pDX, sensorProbes, 0, MAX_SUPPORT);
    DDX_Text(pDX, IDC_SMALL_ARTILLERY, smallArtillery);
    DDV_MinMaxInt(pDX, smallArtillery, 0, MAX_SUPPORT);
    DDX_Text(pDX, IDC_TIME_LIMIT_MIN, timeLimitMinutes);
    DDX_Radio(pDX, IDC_IS, technologyBase);

    if (timeLimitMinutes != 0 && timeLimitMinutes < MIN_TIME_LIMIT)
    {
        CString message;
        message.LoadString(IDS_THE_TIME_LIMIT_MUST);
        CString caption;
        caption.LoadString(MESSAGE_CAPTION);
        MessageBox(message, caption, MB_OK);
        GetDlgItem(IDC_TIME_LIMIT_MIN)->SetFocus();
        pDX->Fail();
    }
}

BOOL SettingsPlayerDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    UpdateData(FALSE);

    if (multiplayer)
    {
        GetDlgItem(IDC_CLAN)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_IS)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_TECHNOLOGY_BASE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_CAMERA_DRONES_LABEL)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_CAMERA_DRONES)->ShowWindow(SW_HIDE);
    }
    else
    {
        CString title;
        title.LoadString(IDS_SETTINGS);
        SetWindowText(title);
    }

    if (!hasTimeLimit)
    {
        GetDlgItem(IDC_TIME_LIMIT_MIN)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_TIME_LIMIT_MIN_LABEL)->ShowWindow(SW_HIDE);
    }

    CString title;
    title.LoadString(titleId);
    SetWindowText(title);
    cameraDronesBox.SetLimitText(SUPPORT_DIGITS);
    dropWeightBox.SetLimitText(multiplayer ? SOLO_DROP_WEIGHT_DIGITS - 1 : SOLO_DROP_WEIGHT_DIGITS);
    largeArtilleryBox.SetLimitText(SUPPORT_DIGITS);
    resourcePointsBox.SetLimitText(RESOURCE_POINT_DIGITS);
    smallArtilleryBox.SetLimitText(SUPPORT_DIGITS);
    sensorProbesBox.SetLimitText(SUPPORT_DIGITS);
    timeLimitBox.SetLimitText(TIME_LIMIT_DIGITS);
    return TRUE;
}

void SettingsPlayerDialog::OnOK()
{
    if (UpdateData(TRUE))
    {
        settings.artilleryCamera = cameraDrones;
        settings.playerForceLimit = dropWeight;
        settings.artilleryLarge = largeArtillery;
        settings.resourcePoints = resourcePoints;
        settings.artillerySensor = sensorProbes;
        settings.artillerySmall = smallArtillery;
        settings.timeLimit = static_cast<long>(timeLimitMinutes) * SECONDS_PER_MINUTE;
        settings.noUller = technologyBase == 0;
        EndDialog(IDOK);
    }
}

void SettingsPlayerDialog::OnPlayerBriefing()
{
    PlayerBriefingDialog dialog(text, GetParent());

    if (dialog.DoModal() == IDOK)
    {
        dialog.copyTo(text);
    }
}
