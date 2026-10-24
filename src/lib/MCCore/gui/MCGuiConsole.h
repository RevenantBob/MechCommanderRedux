#pragma once

#include "gui/MCGuiObject.h"

/// <summary>
/// Port-only: the debug console, a line typed over the top of the screen during a single-player mission started with
/// <c>-debug</c>. Enter opens it; Enter again runs the line (<see cref="RunConsoleCommand"/>) and closes it; Escape
/// closes it without running anything.
/// </summary>
class MCGuiConsole : public MCGuiObject
{
public:
    /// <summary>The most characters a line holds.</summary>
    static constexpr size_t MaxLength = 0xff;

    /// <summary>Like the base's init; the white font, back colour 0x10.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, std::string_view name) override;
    /// <summary>The prompt, the text and the caret; closes the console once it can't be used.</summary>
    void Draw() override;
    /// <summary>Draws itself each frame from its text and caret.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Typing, Backspace, Enter (runs the line and closes), Escape (closes), the caret's blink.</summary>
    void HandleEvent(MCGuiEvent& event) override;

    /// <summary>The text typed (the caret is at its end).</summary>
    std::string Text;
    /// <summary>The caret's blink phase: shown while set.</summary>
    bool CursorVisible = true;
    /// <summary>Set until the Enter that opened the console has come through as a character.</summary>
    bool SkipReturn = true;
};

/// <summary>Whether the console can be open: <c>-debug</c>, a single-player mission running, not paused.</summary>
bool ConsoleUsable();
/// <summary>Opens the console (when it is usable and not open) and gives it the keyboard.</summary>
void OpenConsole();
/// <summary>Closes the console, if open (it is deleted after the event or frame that closed it).</summary>
void CloseConsole();
