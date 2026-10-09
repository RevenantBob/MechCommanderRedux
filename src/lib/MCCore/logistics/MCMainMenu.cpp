#include "stdafx.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCConnectMenu.h"
#include "logistics/MCGenericScreen.h"
#include "logistics/MCLoadSaveMenu.h"
#include "logistics/MCLogDialogButton.h"
#include "logistics/MCRegistrySettings.h"
#include "logistics/MCReusableDialog.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/MCMission.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/MCSoundSystem.h"
#include "logistics/MCSplashScreen.h"
#include "main/honorb.h"
#include "linkup/sessionmanager.h"

namespace
{
    /// <summary>
    /// Starts campaign <paramref name="campaign"/> from its start file <paramref name="startFile"/> on the briefing
    /// screen; <paramref name="firstPlanet"/> starts it on the first planet (the expansion's keeps the planet it had).
    /// </summary>
    void StartCampaign(std::string_view campaign, std::string_view startFile, bool firstPlanet)
    {
        EnsureRegistryVersion();
        SoundSystem()->StopDigitalMusic();
        SoundSystem()->PlayBettySample(0x19);
        MCStrCopy(MissionName, std::string(campaign).c_str());
        Mission()->ReloadCampaign(MissionName);

        if (firstPlanet)
        {
            CurPlanet = 0;
        }

        Solo = false;
        LastLogisticsMissionState = 0;
        GlobalLogPtr->LoadCampaign(std::string(startFile).data(), const_cast<char*>(".pkk"), 0, 0);
        GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
        GlobalLogPtr->SetUpBriefingScreen(0);
        GlobalLogPtr->MainScreen->ShowGuiWindow(false);
    }
}

// Each menu action first made sure a campaign CD was in a drive (scanning C: to Z: for data\tiles\gtiles90.pak,
// pointing every drive-letter path at it, and asking for the disc when none had it; the dialog's OK retried the
// action). The port reads everything from the install folder, so the disc is always there (as checkForCDInDrive
// says); only the version check is kept.

void NewCampaign()
{
    StartCampaign("mechcmdr1", "start0", true);
}

void NewMcxCampaign()
{
    StartCampaign("xmechcmdr1", "xstart0", false);
}

void ShowMultiPlayer()
{
    GlobalLogPtr->MainScreen->ShowGuiWindow(false);
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(true);
    Solo = false;
    GlobalLogPtr->CurrentScreen = GlobalLogPtr->MultiplayerScreen;
    GlobalLogPtr->LogisticsState = 10;
}

void ReplayCinema()
{
    EnsureRegistryVersion();
    Mission()->NextState = MCMissionState::PlayMovie;
    Mission()->State = MCMissionState::PlayMovie;
    Mission()->CurrentMovie = 0;
    GlobalLogPtr->MainScreen->ShowGuiWindow(false);
    SoundSystem()->StopDigitalMusic();
}

void ReturnToGame()
{
    if (GlobalLogPtr->CurrentMission == -1)
    {
        return;
    }

    if (GlobalLogPtr->PreviousState == 2)
    {
        GlobalLogPtr->SetUpPurchaseScreen(0);
    }
    else if (GlobalLogPtr->PreviousState == 4)
    {
        GlobalLogPtr->SetUpRepairScreen(0);
    }
    else
    {
        GlobalLogPtr->SetUpBriefingScreen(0);
    }

    GlobalLogPtr->MainScreen->ShowGuiWindow(false);
}

void GameOverMan()
{
    MCInput::PostMessage(WM_DESTROY, 0, 0);
    SoundSystem()->StopDigitalMusic();
}

void Cancel()
{
    if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->MainScreen)
    {
        return;
    }

    WhackTimer = true;
    GlobalLogPtr->SetUpMainScreen(1);
    LoadingSolo = false;
}

void DoExit()
{
    if (MPlayer != nullptr)
    {
        if (LaunchedFromLobby != 0)
        {
            KillTheGame();
            return;
        }

        GlobalLogPtr->DestroyMultiplayer();
        MPlayer->LeaveSession();
    }

    GlobalLogPtr->SetUpMainScreen(0);
}

void CheckExit()
{
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(LoadGameString(0xaf, 0xfe));
    dialog->SetTwoButton(true);
    // The dialog's callback and the cancel button's action are left as they were.
    dialog->OkButton->SetUpPicture("bh_okay.tga");
    dialog->OkButton->SetDownPicture("bg_okay.tga");
    dialog->OkButton->Disabled = false;
    dialog->OkButton->Callback()->SetExec(DoExit);
    dialog->CancelButton->SetUpPicture("bh_cancl.tga");
    dialog->CancelButton->SetDownPicture("bh_cancl.tga");
    dialog->CancelButton->Disabled = false;
    dialog->Activate();
}
