#include "stdafx.h"
#include "main/main.h"
#include "main/honorb.h"
#include "lib/MCFatal.h"
#include "platform/MCStringTable.h"
#include "main/logistics.h"
#include "logistics/MCBriefingScreen.h"

float ScenarioTime = 0.0f;
int32_t Turn = 0;
float FrameLength = 0.05f;
float WorldUnitsPerMeter = 3.34f;
float MetersPerWorldUnit = 0.2994f;
char* ExceptionGameMsg = nullptr;
uint32_t UlOldAutoRunValue = 0x95;
void* ThisInstance = nullptr;

int32_t CLoadString(void* instance, uint32_t id, char* buffer, int bufferSize)
{
    uint32_t stringId = id + static_cast<uint32_t>(LanguageOffset);
    std::memset(buffer, 0, static_cast<size_t>(bufferSize));
    // LoadStringA of the executable's string table: the port reads MCX.EXE's resources itself.
    return MCStringTable::Game().LoadString(stringId, buffer, bufferSize);
}

std::string LoadGameString(uint32_t id, int bufferSize)
{
    std::vector<char> buffer(static_cast<size_t>(bufferSize), '\0');
    CLoadString(nullptr, id, buffer.data(), bufferSize);
    return buffer.data();
}

namespace
{
    /// <summary>Set on entry to AssertTest: a second error while reporting one can only quit (0x0080bb1a).</summary>
    bool InAssertTest = false;
}

int AssertTest(int errorCode, char* text)
{
    // Port: the original was a crash reporter. It walked the stack with imagehlp, wrote the symbol, source line and
    // message to default.1st, gathered the machine, DLL and game details, the log files and a screen grab, and showed
    // a report dialog that could mail it all to FASA (MAPI) before breaking into the debugger or exiting. There is
    // nowhere to send a report now, so the port logs the error and asks whether to carry on, break or quit.
    if (InAssertTest)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to continue execution.", nullptr);
        std::exit(1);
    }

    InAssertTest = true;

    if (GlobalLogPtr != nullptr && GlobalLogPtr->BriefingScreen != nullptr &&
        GlobalLogPtr->BriefingScreen->SmackerWindow != nullptr)
    {
        GlobalLogPtr->BriefingScreen->StopSmackerMovies();
    }

    const char* message = text != nullptr ? text : "";
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AssertTest %02X: %s", static_cast<uint32_t>(errorCode), message);

    // An unattended run (mc_tests) can't answer the box: the error fails the run.
    if (MCNoMessageBoxes)
    {
        std::exit(1);
    }

    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Continue"},
        {0, 1, "Debug"},
        {0, 2, "Exit"},
    };

    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_ERROR,   nullptr, "ERROR", message,
                                    SDL_arraysize(buttons), buttons, nullptr};
    int choice = 0;

    if (!SDL_ShowMessageBox(&box, &choice))
    {
        choice = 0;
    }

    InAssertTest = false;

    if (choice == 2)
    {
        KillTheGame();
    }

    return choice == 1 ? 1 : 0;
}

void FatalShutDown()
{
    // The original put back the screen saver and power-down timeouts it had turned off (SystemParametersInfo),
    // shut imagehlp down and restored the CD autorun setting (NoDriveTypeAutoRun) in the registry. The port changes
    // none of them.
}
