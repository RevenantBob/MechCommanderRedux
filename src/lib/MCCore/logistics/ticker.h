#pragma once

#include "logistics/lport.h"

class MCGuiEvent;
class MCGuiFont;

/// <summary>
/// The scrolling message line along the bottom of the logistics screens (help text for whatever the mouse is
/// over). The text is rendered once into <see cref="TextPort"/>; a timer scrolls it through
/// <see cref="WindowPort"/> when it is wider than <see cref="MaxWidth"/>.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\ticker.cpp</c> / <c>logistics\ticker.h</c>, 0x5e0 bytes. The logistics screen
/// owns one (<c>Logistics</c> +0x0).
/// </remarks>
class MCTicker : public MCLogObject
{
public:
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    ~MCTicker() override;

    /// <summary>Clears the state and starts the scroll timer (id 4, every 75 ms).</summary>
    void Init();

    /// <summary>Places the ticker at (<paramref name="xPos"/>, <paramref name="yPos"/>) drawing into <paramref name="port"/>.</summary>
    void Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, MCLogPort* port);

    /// <summary>Frees the three work ports.</summary>
    void Destroy() override;

    /// <summary>Nothing: the ticker draws itself on its timer.</summary>
    void Draw() override {}

    /// <summary>
    /// On the scroll timer, advances the text two pixels (wrapping round) and copies it to the port. Port: the line
    /// is drawn by <see cref="DrawLine"/>; this keeps the scroll and whether a wide text shows.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Nothing: the ticker draws itself on its timer.</summary>
    void Display() override {}

    void SetFont(MCGuiFont* newFont);

    /// <summary>Shows <paramref name="string"/> (an empty one clears the line); unchanged text is ignored.</summary>
    void SetString(char* string);

    /// <summary>Where in the port the text goes.</summary>
    void SetPos(int32_t xPos, int32_t yPos);

    /// <summary>Sets the visible width and makes the window port of that width.</summary>
    void SetMaxWidth(int32_t width);

    /// <summary>The port the ticker is drawn into (its own port pointer).</summary>
    void SetPort(MCLogPort* port);

    /// <summary>Keeps a copy of <paramref name="port"/> to restore under the text.</summary>
    void SetBackPane(MCLogPort* port);

    /// <summary>
    /// Port: the screen whose port the ticker paints into (<see cref="SetPort"/> gets only its port); that screen
    /// draws the line (<see cref="DrawLine"/>).
    /// </summary>
    void SetScreen(MCLogObject* screen) { PaintScreen = screen; }

    /// <summary>
    /// Port: draws the ticker line from its state into <paramref name="target"/>: the back pane, then the text (a wide
    /// one scrolled, and only while it scrolls).
    /// </summary>
    void DrawLine(MCPane* target);

    /// <summary>The text shown.</summary>
    char Text[256] = {};
    /// <summary>How far the text has scrolled, in pixels.</summary>
    int32_t ScrollPos = 0;
    /// <summary>The font measuring the text (it is written with <c>medWhiteFont</c>).</summary>
    MCGuiFont* Font = nullptr;
    /// <summary>A copy of the background under the ticker (<see cref="SetBackPane"/>).</summary>
    MCLogPort* BackPane = nullptr;
    /// <summary>The rendered text.</summary>
    MCLogPort* TextPort = nullptr;
    /// <summary>The visible window, <see cref="MaxWidth"/> wide.</summary>
    MCLogPort* WindowPort = nullptr;
    int32_t XPos = 0;
    int32_t YPos = 0;
    /// <summary>The width of <see cref="TextPort"/> (the text plus half a window of gap when it scrolls); 0 = empty.</summary>
    int32_t TextWidth = 0;
    /// <summary>The visible width.</summary>
    int32_t MaxWidth = 0;

    /// <summary>Port: the screen painted into (see <see cref="SetScreen"/>).</summary>
    MCLogObject* PaintScreen = nullptr;
    /// <summary>
    /// Port: a text wider than the line shows (the last event was a scroll step; any other event painted the back
    /// pane alone).
    /// </summary>
    bool ScrollShown = false;
};
