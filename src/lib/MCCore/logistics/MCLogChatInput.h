#pragma once

#include "gui/MCGuiOwned.h"
#include "logistics/MCLogObject.h"
#include "logistics/MCLogToolButton.h"

class MCGuiEvent;
class MCGuiFont;

/// <summary>
/// The chat input line: typed text with a blinking cursor; Enter sends it to everyone, or to the team when the
/// team button is toggled, and echoes it in the parent <see cref="MCLogChatWindow"/>.
/// </summary>
/// <remarks>Original source: <c>logistics\logsession.cpp</c> (<c>lChatInput</c>).</remarks>
class MCLogChatInput : public MCLogObject
{
public:
    /// <summary>The most characters a chat line takes (a game rule: the chat message's size).</summary>
    static constexpr int32_t MaxLength = 0xff;

    ~MCLogChatInput() override;

    /// <summary>Places the line, makes the team button and takes <paramref name="text"/> as the starting text.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;

    void Destroy() override;

    /// <summary>
    /// Clears the box and writes the text, wrapped; it also turned the cursor off. Port: draws the box, the text and
    /// the cursor (which the original's display drew into the picture each frame).
    /// </summary>
    void Draw() override;

    /// <summary>Port: the line draws itself each frame (its port is a view).</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: an edit turns the cursor off, starting its blink over (the original's paint after an edit did).
    /// </summary>
    void RestartBlink();

    /// <summary>Draws the cursor (lit or not) and displays the line. Port: the cursor is drawn by <see cref="Draw"/>.</summary>
    void Display() override;

    /// <summary>Takes the text focus on a click; typing, Backspace, Enter; the blink timer.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Works out the cursor's pixel position for character <paramref name="position"/>.</summary>
    void SetCursorPos(int32_t position);

    /// <summary>
    /// Empties the line as the chat window's reset does: the text shown ends at once, but what was typed stays behind
    /// it (OB-164).
    /// </summary>
    void HideText();

    /// <summary>What the line shows: <see cref="Text"/> up to its first NUL.</summary>
    std::string_view ShownText() const;

    /// <summary>Toggled: send to the team only.</summary>
    MCGuiOwned<MCLogToolButton> TeamButton;
    /// <summary>
    /// The characters typed (<see cref="TextLength"/> of them), NUL-filled to <see cref="MaxLength"/> + 1. What is
    /// shown and sent ends at the first NUL (see <see cref="HideText"/>).
    /// </summary>
    std::string Text = std::string(MaxLength + 1, '\0');
    /// <summary>The number of characters typed.</summary>
    int32_t TextLength = 0;
    /// <summary>The cursor's x in the box.</summary>
    int32_t CursorX = 0;
    /// <summary>The cursor's y in the box.</summary>
    int32_t CursorY = 0;
    /// <summary>The cursor's blink phase.</summary>
    bool CursorOn = false;
    MCGuiFont* Font = nullptr;
};
