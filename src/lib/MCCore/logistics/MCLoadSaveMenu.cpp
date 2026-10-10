#include "stdafx.h"
#include "logistics/MCLoadSaveMenu.h"
#include "lib/MCFile.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCFileScrollPane.h"
#include "logistics/MCGenericScreen.h"
#include "logistics/MCLogDialogButton.h"
#include "logistics/MCLogMenus.h"
#include "logistics/MCRegistrySettings.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSessionScreen.h"
#include "logistics/MCSplashScreen.h"
#include "main/MCLogistics.h"
#include "main/main.h"
#include "platform/MCFileSystem.h"
#include "sound/MCSoundSystem.h"

bool LoadingSolo = false;

namespace
{
    /// <summary>The longest default save name (the original's 0x68-byte buffer).</summary>
    constexpr size_t MaxDefaultNameLength = 0x67;

    /// <summary>The file pane of the screen being shown (the load or save screen), or null.</summary>
    MCFileScrollPane* ShownFilePane()
    {
        MCFileScrollPane* pane = nullptr;

        if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->LoadScreen.get())
        {
            pane = GlobalLogPtr->LoadScreen->FilePane;
        }

        if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->SaveScreen.get())
        {
            pane = GlobalLogPtr->SaveScreen->FilePane;
        }

        return pane;
    }

    /// <summary>Whether <paramref name="pane"/> has a file selected.</summary>
    bool HasSelection(const MCFileScrollPane* pane)
    {
        return pane != nullptr && pane->HasSelection();
    }

    /// <summary>Takes down the file pane's name entry, if it has one (its <c>destroy</c>).</summary>
    void DestroyNameEntry(MCFileScrollPane* pane)
    {
        if (pane->NameEntry != nullptr)
        {
            pane->NameEntry->Destroy();
        }
    }

    /// <summary>Shows <paramref name="screen"/>'s file list (from the main menu), nothing selected.</summary>
    void OpenFileScreen(MCSplashScreen* screen, int32_t state)
    {
        GlobalLogPtr->MainScreen->ShowGuiWindow(false);
        GlobalLogPtr->CurrentScreen = screen;
        screen->ShowGuiWindow(true);
        GlobalLogPtr->LogisticsState = state;
        screen->FilePane->SetSelectedFile(-1);
    }

    /// <summary>
    /// The first default save name (string 0x37c with a number, 0 to 999) with no save in <paramref name="directory"/>,
    /// or nothing.
    /// </summary>
    std::optional<std::string> FreeDefaultName(std::string_view directory)
    {
        for (int32_t i = 0; i < 1000; i++)
        {
            std::string candidate = MCFormatPrintf(LoadGameString(0x37c, 0x95).c_str(), i);
            candidate.resize(std::min(candidate.size(), MaxDefaultNameLength));

            if (!FileExists(GamePath(directory, candidate, ".sav")))
            {
                return candidate;
            }
        }

        return std::nullopt;
    }
}

void SaveScreen()
{
    EnsureRegistryVersion();

    if (GlobalLogPtr->CurrentMission == -1)
    {
        return;
    }

    OpenFileScreen(GlobalLogPtr->SaveScreen.get(), 6);
}

void LoadScreen()
{
    EnsureRegistryVersion();
    OpenFileScreen(GlobalLogPtr->LoadScreen.get(), 5);
}

void SoloLoadScreen()
{
    EnsureRegistryVersion();
    LoadingSolo = true;
    OpenFileScreen(GlobalLogPtr->LoadScreen.get(), 5);
}

void LoadGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->LoadScreen->FilePane;

    if (!HasSelection(pane))
    {
        return;
    }

    std::string fileName = pane->Files[pane->SelectedFile].Name;
    SoundSystem()->StopDigitalMusic();
    GlobalLogPtr->BriefingScreen->BriefingBox = nullptr;
    const bool campaign = !LoadingSolo;
    LoadingSolo = false;
    Solo = !campaign;
    LastLogisticsMissionState = 0;

    if (GlobalLogPtr->LoadCampaign(fileName, campaign ? ".sav" : ".sol", false, true) == 0)
    {
        // Original behaviour: the save screen is the one hidden, not the load screen shown.
        GlobalLogPtr->SaveScreen->ShowGuiWindow(false);
        GlobalLogPtr->SetUpBriefingScreen(false);
        SoundSystem()->PlayDigitalMusic(0x16, true);
    }
}

void LoadMPGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->LoadScreen->FilePane;

    if (HasSelection(pane))
    {
        GlobalLogPtr->SessionScreen->LoadMission(pane->Files[pane->SelectedFile].Name);
    }
}

void SaveWorkedCallback(int32_t)
{
    GlobalLogPtr->SaveScreen->ShowGuiWindow(false);

    if (GlobalLogPtr->PreviousState == 2)
    {
        GlobalLogPtr->SetUpPurchaseScreen(false);
    }
    else if (GlobalLogPtr->PreviousState != 4)
    {
        GlobalLogPtr->SetUpBriefingScreen(false);
    }
    else
    {
        GlobalLogPtr->SetUpRepairScreen(false);
    }

    SoundSystem()->PlayDigitalMusic(0x16, true);
}

void SaveGameCallback()
{
    int32_t result = -1;
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;

    if (HasSelection(pane))
    {
        std::string fileName = pane->Files[pane->SelectedFile].Name;
        SoundSystem()->StopDigitalMusic();
        result = GlobalLogPtr->SaveCampaign(fileName.data());
    }

    if (result != 0)
    {
        pane->SetSelectedFile(-1);
    }

    pane->GetAllFiles(".sav", true);

    if (result == 0)
    {
        // "Game saved", closing by itself after three seconds.
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog.get();
        dialog->SetText(LoadGameString(0x76, 0xfe));
        dialog->SetTwoButton(false);
        dialog->Callback = SaveWorkedCallback;
        dialog->OkButton->SetUpPicture("bh_okay.tga");
        dialog->OkButton->SetDownPicture("bg_okay.tga");
        dialog->OkButton->Disabled = false;
        dialog->Timeout = 3000;
        dialog->Activate();
        DestroyNameEntry(pane);
    }

    SoundSystem()->PlayDigitalMusic(0x16, true);
}

void ClearForSaveGameCallback()
{
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;

    // The original cleared the file's read-only attribute first.
    if (MCFileSystem::RemoveFile(GamePath(pane->StartDirectory, pane->Files[pane->SelectedFile].Name, ".sav")))
    {
        SaveGameCallback();
    }
}

void SaveGame()
{
    MCFileScrollPane* pane = GlobalLogPtr->SaveScreen->FilePane;

    if (pane == nullptr || !pane->HasSelection())
    {
        return;
    }

    std::string& slot = pane->Files[pane->SelectedFile].Name;
    // The name typed replaces the selected entry's.
    MCLogTextObject* entry = pane->NameEntry.get();

    if (entry != nullptr && entry->Parent == pane)
    {
        const std::string_view typed = entry->Text();
        slot = typed.empty() ? EmptyFile : std::string(typed);
    }

    // The empty entry gets the first free default name.
    if (slot == EmptyFile)
    {
        if (std::optional<std::string> name = FreeDefaultName(pane->StartDirectory))
        {
            slot = std::move(*name);
        }
    }

    // OB-084 (fixed): the original tested the old, freed name here (the empty entry's text); the name now in the slot
    // is tested instead.
    if (!FileExists(GamePath(pane->StartDirectory, slot, ".sav")))
    {
        SaveGameCallback();
        return;
    }

    // Overwrite? (The cancel button keeps whatever action it had.)
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog.get();
    dialog->SetText(LoadGameString(0x75, 0xfe));
    dialog->SetTwoButton(true);
    dialog->Callback = nullptr;
    dialog->OkButton->SetUpPicture("bh_okay.tga");
    dialog->OkButton->SetDownPicture("bg_okay.tga");
    dialog->OkButton->Callback()->SetExec(ClearForSaveGameCallback);
    dialog->CancelButton->SetUpPicture("bh_cancl.tga");
    dialog->CancelButton->SetDownPicture("bg_cancl.tga");
    dialog->Activate();
}

void DeleteCallbackTrue()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (!HasSelection(pane))
    {
        return;
    }

    // The original cleared the file's read-only attribute first.
    MCFileSystem::RemoveFile(
        GamePath(pane->StartDirectory, pane->Files[pane->SelectedFile].Name, LoadingSolo ? ".sol" : ".sav"));
    pane->SetSelectedFile(-1);

    if (!LoadingSolo)
    {
        GlobalLogPtr->LoadScreen->FilePane->GetAllFiles(".sav", true);
        GlobalLogPtr->SaveScreen->FilePane->GetAllFiles(".sav", true);
    }
    else
    {
        GlobalLogPtr->LoadScreen->FilePane->GetAllFiles(".sol", true);
    }
}

void DeleteCallbackFalse()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (pane != nullptr)
    {
        DestroyNameEntry(pane);
    }
}

void DeleteGame()
{
    MCFileScrollPane* pane = ShownFilePane();

    if (!HasSelection(pane))
    {
        return;
    }

    AskMenuQuestion(GlobalLogPtr->MessageDialog.get(), 0x74, DeleteCallbackTrue, DeleteCallbackFalse, "bg_cancl.tga",
                    false);
    DestroyNameEntry(pane);
}

void LoadSaveScreenHandleEvent(MCGuiObject* object, MCGuiEvent* event)
{
    if (event->Type != 0x1e)
    {
        return;
    }

    auto* screen = static_cast<MCGenericScreen*>(object);
    const int32_t message = event->Data;

    if (message == 1)
    {
        // A file was picked: it can be loaded (or saved over); deleted unless it is the empty entry.
        screen->LoadSaveButton->Disabled = false;
        MCFileScrollPane* pane = screen->FilePane;
        screen->DeleteButton->Disabled = !(HasSelection(pane) && pane->Files[pane->SelectedFile].Name != EmptyFile);
    }
    else if (message == 2)
    {
        screen->LoadSaveButton->Disabled = true;
        screen->DeleteButton->Disabled = true;
    }
    else if (message == 5 && object == GlobalLogPtr->SaveScreen.get())
    {
        // Enter in the name entry: save.
        SoundSystem()->PlayDigitalSample(screen->LoadSaveButton->PressSound, 1, nullptr, 0, 0);
        SaveGame();
    }
}
