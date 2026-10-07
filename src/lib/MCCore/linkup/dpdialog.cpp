#include "stdafx.h"
#include "linkup/dpdialog.h"
#include "linkup/sessionmanager.h"

// The connection dialog was a Win32 dialog box (resource 201 of MCX.EXE): a list of the connections and Host / Join
// buttons. The shipped game never opens it (the logistics screens connect instead), and the port has no Win32
// dialogs, so ConnectUsingDialog reports a cancelled dialog and the window procedure has nothing to drive.

namespace
{
    /// <summary>The session manager the dialog works on (0x0080a410).</summary>
    SessionManager* dialogSessionManager = nullptr;
}

int32_t ConnectUsingDialog(void*)
{
    dialogSessionManager = SessionManager::GetGlobalPointer(nullptr);
    // Port: no dialog to show; DialogBoxParam returning 0 meant the dialog was cancelled.
    return static_cast<int32_t>(DPERR_USERCANCEL);
}

int ConnectWndProc(void*, uint32_t, uint32_t, int32_t)
{
    // Port: never called (no dialog). The original filled the connection list on WM_INITDIALOG and hosted
    // (MPlayer->createSession(nullptr, nullptr, 6)) or joined (MPlayer->joinSession(nullptr, nullptr)) on its
    // buttons.
    return 0;
}

int32_t ConnectToSelectedService(void*)
{
    dialogSessionManager->SetCurrentConnection(PROTOCOL_IPX);
    return 0;
}

void EnableDlgButton(void*, int, int)
{
    // Port: no dialog items to enable.
}
