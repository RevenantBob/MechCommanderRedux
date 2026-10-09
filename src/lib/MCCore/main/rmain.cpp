#include "stdafx.h"
#include "main/rmain.h"
#include "gui/MCGuiStartup.h"
#include "lib/MCFatal.h"
#include "main/main.h"

uint32_t UlDisableAutoRun = 0xff;
uint32_t UlDataSize = 4;

int WinMain(void* instance, void* prevInstance, char* commandLine, int showCommand)
{
    // Port: the original saved HKCU\...\Policies\Explorer NoDriveTypeAutoRun (ulOldAutoRunValue) and set it to
    // ulDisableAutoRun so the CD's autorun stayed quiet, loaded imagehlp for the crash reporter, and ran RealWinMain
    // under a structured exception handler (ProcessException). None of that is needed now.
    (void)prevInstance;
    (void)showCommand;
    const int result = RealWinMain(instance, commandLine);
    FatalShutDown();
    return result;
}
