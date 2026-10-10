#pragma once

#include "gui/MCGuiCallback.h"
#include "gui/MCGuiOwned.h"

class MCAblDebuggerWindow;
class MCScrollingTextWindow;

/// <summary>
/// The game's systems made after the display is up and before the first frame (honorb.cpp's userInit), taken down in
/// the original's order when it is destroyed (userDestroy). The GUI system owns it from its start to its stop.
/// </summary>
/// <remarks>
/// Making it: the game system's text window (SYSTEM.CFG "DebugGameSystem"), the ABL debugger window (placed by iface.fit's
/// "ABL Window"), the sound system (unless one is installed), the palette cycling callback, multiplayer (kept only when a
/// lobby launched the game, which the port's never does), and the mission, loaded with the campaign (or SYSTEM.CFG's
/// mission for a game segment). Destroying it: the debugger window, the mission, the callback, multiplayer, the
/// FastFiles and the sound system, then the game system's window (which the original never freed).
/// </remarks>
class MCGameSession
{
public:
    /// <summary>Makes the systems. A mission that fails to load is fatal.</summary>
    MCGameSession();

    /// <summary>Takes the systems down in the original's order.</summary>
    ~MCGameSession();

    MCGameSession(const MCGameSession&) = delete;
    MCGameSession& operator=(const MCGameSession&) = delete;

    /// <summary>The ABL debugger's window, when SYSTEM.CFG enables the debugger.</summary>
    MCAblDebuggerWindow* DebuggerWindow() const { return _DebuggerWindow.get(); }

    /// <summary>The palette cycling callback (runs every frame).</summary>
    const MCGuiCallback& ColorCallback() const { return _ColorCallback; }

private:
    /// <summary>The game system's text window (also <c>GameSystemWindow</c>), when SYSTEM.CFG asks for it.</summary>
    MCGuiOwned<MCScrollingTextWindow> _GameSystemWindow;
    MCGuiOwned<MCAblDebuggerWindow> _DebuggerWindow;
    MCGuiCallback _ColorCallback;
};

/// <summary>Shuts down multiplayer, the mouse timer, sound and the display, then exits with 1.</summary>
[[noreturn]] void KillTheGame();
