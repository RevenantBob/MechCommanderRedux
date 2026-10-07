#pragma once

// Original source: mcx\rmain.cpp: the executable's entry point. It switches off CD autorun for the session (restored
// by FatalShutDown), loads imagehlp for crash reports and runs RealWinMain (gui\asystem.cpp) inside a structured
// exception handler that reports through ProcessException.
//
// Port: the SDL application's main (apps/MCRedux) sets up the file roots and calls WinMain with opaque handles
// (HINSTANCE in the original). Autorun, imagehlp and the exception handler are gone.

/// <summary>The NoDriveTypeAutoRun value WinMain writes while the game runs (autorun off for every drive type).</summary>
extern uint32_t UlDisableAutoRun;

/// <summary>The size of the NoDriveTypeAutoRun value read from the registry (4).</summary>
extern uint32_t UlDataSize;

/// <summary>
/// The entry point: saves and disables CD autorun, initializes imagehlp, runs RealWinMain and shuts down with
/// FatalShutDown.
/// </summary>
/// <returns>RealWinMain's exit code.</returns>
int WinMain(void* instance, void* prevInstance, char* commandLine, int showCommand);
