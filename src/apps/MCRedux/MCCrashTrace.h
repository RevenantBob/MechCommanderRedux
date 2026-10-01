#pragma once

/// <summary>
/// A development aid: on an unhandled crash, prints the exception and a symbolized call stack to stderr, runs the
/// reporter (game state worth knowing), and writes a minidump next to the executable (MCRedux.dmp, open it in Visual
/// Studio with the PDB beside it) before the process dies. Without a terminal, the report goes to MCRedux.crash.txt
/// beside the executable instead, and a message box says where. Windows only (DbgHelp and the build's PDB); elsewhere
/// <see cref="Install"/> does nothing.
/// </summary>
namespace MCCrashTrace
{
    /// <summary>Installs the handler. Call once, early in <c>main</c>.</summary>
    void Install();

    /// <summary>Sets a function the handler calls after the stack, to print game state to stderr.</summary>
    void SetReporter(void (*reporter)());
}
