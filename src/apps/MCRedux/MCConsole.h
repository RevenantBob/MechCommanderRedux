#pragma once

/// <summary>
/// MCRedux is a Windows-subsystem program, so it opens no console window of its own. When it is started from a
/// terminal, it borrows that terminal for stdout and stderr; otherwise their output goes nowhere, and messages the
/// player must see go to a message box instead.
/// </summary>
namespace MCConsole
{
    /// <summary>Attaches stdout and stderr to the parent process's console, if it has one. Call once, early in <c>main</c>.</summary>
    void AttachParent();

    /// <summary>Whether stderr reaches a console or file (false when started without a terminal).</summary>
    bool HasOutput();
}
