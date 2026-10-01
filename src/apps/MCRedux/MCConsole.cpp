// Built without the precompiled header: MCCore's Win32 stand-ins (MCWin32Defs.h) and Windows.h cannot meet.
#include "MCConsole.h"

#include <cstdio>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <io.h>

void MCConsole::AttachParent()
{
    if (HasOutput() || !AttachConsole(ATTACH_PARENT_PROCESS))
    {
        return;
    }

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}

bool MCConsole::HasOutput()
{
    return _fileno(stderr) >= 0 && _get_osfhandle(_fileno(stderr)) >= 0;
}

#else

void MCConsole::AttachParent()
{
}

bool MCConsole::HasOutput()
{
    return true;
}

#endif
