#pragma once

// Original source: mcx\linkup\dpdialog.cpp: a Win32 dialog (resource 201) to pick a connection and host or join a
// session, a developer path the shipped game doesn't reach. Window handles are opaque pointers in the port
// (HINSTANCE / HWND in the original); the port has no Win32 dialogs, so these report "not connected".
//
// Its two file-static globals (DAT_0080a410: the SessionManager the dialog works on; DAT_0080a414: whether a join
// should be offered again) stay in the .cpp.

/// <summary>Runs the connection dialog.</summary>
/// <returns>0 when a session was hosted or joined, else 0x88770118 (the dialog was cancelled).</returns>
/// <remarks>MCX.EXE @ 0x0074a0b0. <paramref name="instance"/> is the HINSTANCE.</remarks>
int32_t ConnectUsingDialog(void* instance);

/// <summary>The dialog's procedure: lists the connections, hosts (MPlayer-&gt;createSession) or joins.</summary>
/// <remarks>MCX.EXE @ 0x0074a0f0 (__stdcall). <paramref name="window"/> is the HWND.</remarks>
int ConnectWndProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam);

/// <summary>Selects the connection of protocol type 2 (IPX).</summary>
/// <remarks>MCX.EXE @ 0x0074a3e0</remarks>
int32_t ConnectToSelectedService(void* window);

/// <summary>Enables or disables dialog item <paramref name="itemID"/>.</summary>
/// <remarks>MCX.EXE @ 0x0074a400</remarks>
void EnableDlgButton(void* window, int itemID, int enable);
