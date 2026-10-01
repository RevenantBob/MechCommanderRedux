#include "stdafx.h"
#include "main/rmain.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "main/main.h"

uint32_t ulDisableAutoRun = 0xff;
uint32_t ulDataSize = 4;

int WinMain(void* instance, void* prevInstance, char* commandLine, int showCommand)
{
    // Port: the original saved HKCU\...\Policies\Explorer NoDriveTypeAutoRun (ulOldAutoRunValue) and set it to
    // ulDisableAutoRun so the CD's autorun stayed quiet, loaded imagehlp for the crash reporter, and ran RealWinMain
    // under a structured exception handler (ProcessException). None of that is needed now.
    McMsg1[0] = '\0';
    const int result = RealWinMain(instance, prevInstance, commandLine, showCommand);
    FatalShutDown();
    return result;
}
