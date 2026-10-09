#pragma once

#include "logistics/MCLogObject.h"

class MCGuiEvent;
class MCGuiFont;

/// <summary>
/// The scrolling message line along the bottom of the logistics screens (help text for whatever the mouse is over).
/// A text wider than the line scrolls through it on a timer, after half a line of gap.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\ticker.cpp</c> (<c>Ticker</c>). The logistics screen keeps one
/// (<c>Logistics::Ticker</c>). The original rendered the text into a picture of its own and copied a window of it
/// into the screen's picture; the screen it paints into draws the line from the ticker's state each frame
/// (<see cref="DrawLine"/>).
/// </remarks>
class MCTicker : public MCLogObject
{
public:
    /// <summary>The scroll timer's id, and the id <see cref="HandleEvent"/> scrolls on (OB-072).</summary>
    static constexpr int32_t ScrollTimer = 4;
    static constexpr int32_t ScrollStepTimer = 7;

    /// <summary>Starts the scroll timer (every 75 ms).</summary>
    MCTicker();
    /// <summary>Calls <see cref="Destroy"/> and lets the logistics screen know the ticker went.</summary>
    ~MCTicker() override;

    /// <summary>Places the ticker at (<paramref name="xPos"/>, <paramref name="yPos"/>), <paramref name="width"/> wide.</summary>
    void Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height);

    /// <summary>Frees the back pane.</summary>
    void Destroy() override;

    /// <summary>Nothing: the screen draws the line (<see cref="DrawLine"/>).</summary>
    void Draw() override {}

    /// <summary>
    /// On the scroll step timer, advances a wide text two pixels (wrapping round) and shows it; any other event hides
    /// it again (the original painted the back pane alone).
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Nothing: the screen draws the line (<see cref="DrawLine"/>).</summary>
    void Display() override {}

    /// <summary>The font that measures the text (it is written with <c>MedWhiteFont</c>).</summary>
    void SetFont(MCGuiFont* newFont) { Font = newFont; }

    /// <summary>Shows <paramref name="text"/> (an empty one clears the line); unchanged text is ignored.</summary>
    void SetString(std::string_view text);

    /// <summary>Where in the screen the line goes.</summary>
    void SetPos(int32_t xPos, int32_t yPos);

    /// <summary>Keeps a copy of <paramref name="port"/> to draw under the text.</summary>
    void SetBackPane(MCGuiPort* port);

    /// <summary>The screen that draws the line (<see cref="DrawLine"/>).</summary>
    void SetScreen(MCLogObject* screen) { PaintScreen = screen; }

    /// <summary>
    /// Port: draws the line from its state into <paramref name="target"/>: the back pane, then the text (a wide one
    /// scrolled, and only while it scrolls).
    /// </summary>
    void DrawLine(MCPane* target);

    /// <summary>The text shown.</summary>
    std::string Text;
    /// <summary>How far the text has scrolled, in pixels.</summary>
    int32_t ScrollPos = 0;
    /// <summary>The font measuring the text.</summary>
    MCGuiFont* Font = nullptr;
    /// <summary>A copy of the background under the line (<see cref="SetBackPane"/>).</summary>
    std::unique_ptr<MCLogPort> BackPane;
    int32_t XPos = 0;
    int32_t YPos = 0;
    /// <summary>The width of the text, plus half a line of gap when it scrolls; 0 = empty.</summary>
    int32_t TextWidth = 0;
    /// <summary>The line's width.</summary>
    int32_t MaxWidth = 0;
    /// <summary>The screen that draws the line (see <see cref="SetScreen"/>).</summary>
    MCLogObject* PaintScreen = nullptr;
    /// <summary>A text wider than the line shows (the last event was a scroll step).</summary>
    bool ScrollShown = false;
};
