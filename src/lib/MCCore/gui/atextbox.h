#pragma once

#include "gui/MCGuiSystem.h"

class MCGuiToolButton;
class MCFidpMessage;

/// <summary>A one-line text field in the grey font; editable (typing, backspace, Enter posts 0x17 to the parent) unless read-only.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x5b8 bytes. Vtable 0x0077a7a4.</remarks>
class MCGuiTextObject : public MCGuiObject
{
public:
    /// <summary>Like aObject::init; the text is <paramref name="text"/> (at most 254 characters); wipes to colour 0.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    void Draw() override;
    /// <summary>Port: draws itself each frame from its text (and the caret while it has the focus).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>A click takes the keyboard; characters edit the text.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    void SetText(const char* text);

    MCGuiFont* TextFont = nullptr;
    char Text[254] = {};
    /// <summary>
    /// Cleared by init and never written again: it terminates <see cref="Text"/> when that holds 254 characters (the
    /// strncpy of setText leaves no terminator of its own).
    /// </summary>
    uint8_t TextTerminator = 0;
    int16_t TextLength = 0;
    /// <summary>Nonzero to ignore input (the combo box's field).</summary>
    int32_t ReadOnly = 0;
};

/// <summary>A line of text drawn in the large grey font straight onto the screen pane (no box).</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>. Its fields end at 0x5af.</remarks>
class MCGuiTransparentTextObject : public MCGuiObject
{
public:
    /// <summary>Like aObject::init; wipes to 0xff and sets the text; the colour is 0xfd.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    void Draw() override;
    /// <summary>Copies the port onto <c>globalPane</c> as a sprite, then displays the children.</summary>
    void Display() override;

    /// <summary>Sets the text and sizes the object to it.</summary>
    void SetText(const char* newText);

    char Text[256] = {};
    int16_t TextLength = 0;
    uint8_t TextColor = 0;
};

/// <summary>
/// A scrolling text window: lines appended by <see cref="Print"/> into a 4 KB buffer (each preceded by its colour),
/// drawn into a port that grows with the text, with a scroll thumb at the right.
/// </summary>
/// <remarks>
/// Original source: <c>gui\atextbox.cpp</c>, 0x4e8 bytes. Vtable 0x007843b4: aObject's, then 77..79 below. The font
/// is <c>fonts</c>[<see cref="FontIndex"/>] (the flat array).
/// </remarks>
class MCGuiScrollTextObject : public MCGuiObject
{
public:
    /// <summary>
    /// aObject::init inlined with an aScrollPort; makes the thumb and the text buffer, and prints
    /// <paramref name="text"/> in colour 0x1f when given.
    /// </summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    void Destroy() override;
    void Resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Draws the lines into the port.</summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame: the whole text in a port as tall as it, scrolled by firstPixel.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Copies the visible part of the port (from <see cref="FirstPixel"/>) to the screen.</summary>
    void Display() override;

    /// <summary>Appends a line in <paramref name="color"/> (a blank line for null).</summary>
    virtual void Print(const char* line, uint8_t color); // slot 77
    /// <summary>Appends <paramref name="line"/>, wrapped to <paramref name="wrapWidth"/> pixels.</summary>
    virtual void PrintWrapped(char* line, uint8_t color, int32_t wrapWidth); // slot 78
    /// <summary>Empties the buffer.</summary>
    virtual void Clear(); // slot 79

    /// <summary>Resizes the port to the lines.</summary>
    void ResetPortSize();
    /// <summary>The first visible pixel row for thumb position <paramref name="thumbY"/>.</summary>
    void CalcFirstPixel(int32_t thumbY);
    void PositionScrollTab();
    /// <summary>
    /// Scrolls: <paramref name="direction"/> -1 a line up, 1 a line down, 0 a page towards <paramref name="yPos"/> (a
    /// click on the track above or below the thumb).
    /// </summary>
    void ReceiveClick(int32_t direction, int32_t yPos);
    /// <summary>Port-only: the mouse wheel scrolls a line per notch, as the arrows do. Not taken when the text fits.</summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>The first pixel row of the port shown.</summary>
    int32_t FirstPixel = 0;
    /// <summary>
    /// Colours of up to four sections of the text (0xff by init); the tactical map's weapon list sets them to the
    /// range colours.
    /// </summary>
    uint8_t SectionColors[4] = {};
    /// <summary>The first line of each section (-1 by init and Clear).</summary>
    int32_t SectionStarts[4] = {};
    /// <summary>The text: lines of (colour byte, text, newline) (0x1001 bytes).</summary>
    std::unique_ptr<char[]> TextBuffer;
    /// <summary>The scroll thumb.</summary>
    MCGuiObject* ScrollTab = nullptr;
    int16_t NumLines = 0;
    /// <summary>The bytes used in <see cref="TextBuffer"/>.</summary>
    int32_t TextLength = 0;
    /// <summary>Set to 1 when init or a port resize failed.</summary>
    int32_t InitFailed = 0;
    /// <summary>
    /// The x a tab in a line jumps to (only the first tab of a line counts); negative turns tabs into spaces. Never set
    /// by aScrollTextObject itself.
    /// </summary>
    int32_t TabStop = 0;
    /// <summary>The font: index into the flat <c>fonts</c> array.</summary>
    int32_t FontIndex = 0;
};

/// <summary>The multiplayer chat entry line: a team toggle button and the text being typed.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x5c4 bytes. Vtable 0x0077b30c.</remarks>
class MCGuiChatInput : public MCGuiObject
{
public:
    /// <summary>Like aObject::init; makes the team button; the white font; back colour 0x10.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;
    void Destroy() override;
    void Draw() override;
    /// <summary>Port: draws itself each frame from its text, caret and blink state.</summary>
    bool DrawsLive() override { return true; }
    void Display() override;
    /// <summary>Typing, editing keys, and Enter (sends the line to the chat window).</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// With <paramref name="maxLines"/> 0, draws the text wrapped over lines; otherwise only counts its wrapped lines.
    /// </summary>
    /// <returns>Nonzero when the text needs fewer than <paramref name="maxLines"/> extra lines.</returns>
    int DrawAndCheck(int32_t maxLines);
    void SetCursorPos(int32_t pos);

    /// <summary>The "to team only" toggle.</summary>
    MCGuiToolButton* TeamButton = nullptr;
    char Text[256] = {};
    int32_t CursorPos = 0;
    /// <summary>The caret's x (0x15 by init), set by <see cref="SetCursorPos"/>.</summary>
    int32_t CursorX = 0;
    /// <summary>The caret's top y.</summary>
    int32_t CursorY = 0;
    /// <summary>The caret's blink phase: nonzero draws it in colour 0x10, zero in 0x1f.</summary>
    int32_t CursorVisible = 0;
    MCGuiFont* InputFont = nullptr;
};

/// <summary>The in-game chat window: the incoming lines and the <see cref="MCGuiChatInput"/> under them.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x4b0 bytes. Vtable 0x007848b8.</remarks>
class MCGuiChatWindow : public MCGuiObject
{
public:
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;

    /// <summary>
    /// A network chat message arrived from player <paramref name="fromID"/>: <paramref name="data"/> + 8 is the "to
    /// all" flag, + 9 the text.
    /// </summary>
    void HandleNetworkMessage(uint32_t fromID, void* data);
    /// <summary>Shows <paramref name="text"/> from player <paramref name="playerId"/>.</summary>
    void ProcessChatString(uint32_t playerId, char* text, int32_t color);

    /// <summary>
    /// Port: draws the lines from the bottom up, each above the next. (The original scrolled its picture up by a
    /// new line's height and drew the line at the bottom; the lines are kept instead.)
    /// </summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame from its lines.</summary>
    bool DrawsLive() override { return true; }

    MCGuiChatInput* ChatInput = nullptr;

    /// <summary>Port: a line as the chat formatter takes it (with its colour codes), and its height.</summary>
    struct ChatLine
    {
        std::string Text;
        int32_t Height = 0;
    };

    /// <summary>Port: the lines still (partly) on show, oldest first.</summary>
    std::vector<ChatLine> ChatLines;
};

/// <summary>The scroll text thumb's paint routine.</summary>
void PaintScrollTab(MCGuiObject* obj);
/// <summary>The scroll text thumb's event routine.</summary>
void ScrollTabEventHandler(MCGuiObject* obj, MCGuiEvent* event);
/// <summary>The network callback for chat messages during a scenario.</summary>
void ScenarioChatCallback(MCFidpMessage* message, void* data);

extern int16_t ROffset;
/// <summary>The players' chat colours (initialised data: 1, 3, 4, 2, 6, 5).</summary>
extern int32_t PlayerColor[6];
extern int FirstReturn;
