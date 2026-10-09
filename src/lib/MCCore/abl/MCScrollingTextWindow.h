#pragma once

#include "gui/MCGuiSystem.h"

/// <summary>
/// Lines of text scrolling up in a VFX pane: the ABL debugger's output and the game system window (honorb.cpp).
/// </summary>
/// <remarks>Original source: <c>abl\abldbug.cpp</c>, 0x4b4 bytes.</remarks>
class MCScrollingTextWindow : public MCGuiObject
{
public:
    /// <summary>Inline in the original (DebuggerWindow::init).</summary>
    MCScrollingTextWindow() { Clear(); }

    ~MCScrollingTextWindow() override;

    /// <summary>Creates the pane and sizes the text grid (10-pixel cells).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    void Resize(int32_t width, int32_t height) override;

    void Draw() override;

    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Zeroes the text grid size.</summary>
    virtual void Clear();

    /// <summary>Scrolls up a line and writes <paramref name="s"/> at the bottom.</summary>
    virtual void Print(char* s);

    /// <summary>Columns and lines of 10-pixel cells.</summary>
    int32_t NumColumns = 0;
    int32_t NumLines = 0;
};
