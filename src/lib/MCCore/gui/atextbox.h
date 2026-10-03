#pragma once

#include "gui/asystem.h"

class aToolButton;
class FIDPMessage;

/// <summary>A one-line text field in the grey font; editable (typing, backspace, Enter posts 0x17 to the parent) unless read-only.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x5b8 bytes. Vtable 0x0077a7a4.</remarks>
class aTextObject : public aObject
{
public:
    /// <summary>Like aObject::init; the text is <paramref name="text"/> (at most 254 characters); wipes to colour 0.</summary>
    /// <remarks>MCX.EXE @ 0x00616860</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;
    /// <remarks>MCX.EXE @ 0x00616a70</remarks>
    void draw() override;
    /// <summary>Port: draws itself each frame from its text (and the caret while it has the focus).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>A click takes the keyboard; characters edit the text.</summary>
    /// <remarks>MCX.EXE @ 0x00616920</remarks>
    void handleEvent(aEvent* event) override;

    /// <remarks>MCX.EXE @ 0x00616b80</remarks>
    void setText(char* text);

    aFont* textFont = nullptr; // +0x4ac
    char text[254] = {};       // +0x4b0
    /// <summary>
    /// Cleared by init and never written again: it terminates <see cref="text"/> when that holds 254 characters (the
    /// strncpy of setText leaves no terminator of its own).
    /// </summary>
    uint8_t textTerminator = 0; // +0x5ae
    int16_t textLength = 0;     // +0x5b0
    /// <summary>Nonzero to ignore input (the combo box's field).</summary>
    int32_t readOnly = 0; // +0x5b4
};

/// <summary>A line of text drawn in the large grey font straight onto the screen pane (no box).</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>. Its fields end at 0x5af.</remarks>
class aTransparentTextObject : public aObject
{
public:
    /// <summary>Like aObject::init; wipes to 0xff and sets the text; the colour is 0xfd.</summary>
    /// <remarks>MCX.EXE @ 0x00617b20</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;
    /// <remarks>MCX.EXE @ 0x00617b90</remarks>
    void draw() override;
    /// <summary>Copies the port onto <c>globalPane</c> as a sprite, then displays the children.</summary>
    /// <remarks>MCX.EXE @ 0x00617c40</remarks>
    void display() override;

    /// <summary>Sets the text and sizes the object to it.</summary>
    /// <remarks>MCX.EXE @ 0x00617cb0</remarks>
    void setText(char* newText);

    char text[256] = {};    // +0x4ac
    int16_t textLength = 0; // +0x5ac
    uint8_t textColor = 0;  // +0x5ae
};

/// <summary>
/// A scrolling text window: lines appended by <see cref="Print"/> into a 4 KB buffer (each preceded by its colour),
/// drawn into a port that grows with the text, with a scroll thumb at the right.
/// </summary>
/// <remarks>
/// Original source: <c>gui\atextbox.cpp</c>, 0x4e8 bytes. Vtable 0x007843b4: aObject's, then 77..79 below. The font
/// is <c>fonts</c>[<see cref="fontIndex"/>] (the flat array).
/// </remarks>
class aScrollTextObject : public aObject
{
public:
    /// <summary>
    /// aObject::init inlined with an aScrollPort; makes the thumb and the text buffer, and prints
    /// <paramref name="text"/> in colour 0x1f when given.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00616db0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;
    /// <remarks>MCX.EXE @ 0x006170b0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x006174a0</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Draws the lines into the port.</summary>
    /// <remarks>MCX.EXE @ 0x00617110</remarks>
    void draw() override;
    /// <summary>Port: draws itself each frame: the whole text in a port as tall as it, scrolled by firstPixel.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Copies the visible part of the port (from <see cref="firstPixel"/>) to the screen.</summary>
    /// <remarks>MCX.EXE @ 0x00617420</remarks>
    void display() override;

    /// <summary>Appends a line in <paramref name="color"/> (a blank line for null).</summary>
    /// <remarks>MCX.EXE @ 0x006175e0</remarks>
    virtual void Print(char* line, uint8_t color); // slot 77
    /// <summary>Appends <paramref name="line"/>, wrapped to <paramref name="wrapWidth"/> pixels.</summary>
    /// <remarks>MCX.EXE @ 0x00617740</remarks>
    virtual void PrintWrapped(char* line, uint8_t color, int32_t wrapWidth); // slot 78
    /// <summary>Empties the buffer.</summary>
    /// <remarks>MCX.EXE @ 0x00617810</remarks>
    virtual void Clear(); // slot 79

    /// <summary>Resizes the port to the lines.</summary>
    /// <remarks>MCX.EXE @ 0x00617550</remarks>
    void ResetPortSize();
    /// <summary>The first visible pixel row for thumb position <paramref name="thumbY"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00617860</remarks>
    void CalcFirstPixel(int32_t thumbY);
    /// <remarks>MCX.EXE @ 0x006178d0</remarks>
    void PositionScrollTab();
    /// <summary>
    /// Scrolls: <paramref name="direction"/> -1 a line up, 1 a line down, 0 a page towards <paramref name="yPos"/> (a
    /// click on the track above or below the thumb).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006179f0</remarks>
    void ReceiveClick(int32_t direction, int32_t yPos);
    /// <summary>Port-only: the mouse wheel scrolls a line per notch, as the arrows do. Not taken when the text fits.</summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>The first pixel row of the port shown.</summary>
    int32_t firstPixel = 0; // +0x4ac
    /// <summary>
    /// Colours of up to four sections of the text (0xff by init); the tactical map's weapon list sets them to the
    /// range colours.
    /// </summary>
    uint8_t sectionColors[4] = {}; // +0x4b0
    /// <summary>The first line of each section (-1 by init and Clear).</summary>
    int32_t sectionStarts[4] = {}; // +0x4b4
    /// <summary>The text: lines of (colour byte, text, newline) (0x1001 bytes, GUI heap).</summary>
    char* textBuffer = nullptr; // +0x4c4
    /// <summary>The scroll thumb.</summary>
    aObject* scrollTab = nullptr; // +0x4c8
    int16_t numLines = 0;         // +0x4cc
    /// <summary>The bytes used in <see cref="textBuffer"/>.</summary>
    int32_t textLength = 0; // +0x4d0
    /// <summary>Not accessed in MCX.EXE's aScrollTextObject code.</summary>
    int32_t unknown4D4 = 0; // +0x4d4
    int32_t unknown4D8 = 0; // +0x4d8
    /// <summary>Set to 1 when init or a port resize failed.</summary>
    int32_t initFailed = 0; // +0x4dc
    /// <summary>
    /// The x a tab in a line jumps to (only the first tab of a line counts); negative turns tabs into spaces. Never set
    /// by aScrollTextObject itself.
    /// </summary>
    int32_t tabStop = 0; // +0x4e0
    /// <summary>The font: index into the flat <c>fonts</c> array.</summary>
    int32_t fontIndex = 0; // +0x4e4
};

/// <summary>The multiplayer chat entry line: a team toggle button and the text being typed.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x5c4 bytes. Vtable 0x0077b30c.</remarks>
class aChatInput : public aObject
{
public:
    /// <summary>Like aObject::init; makes the team button; the white font; back colour 0x10.</summary>
    /// <remarks>MCX.EXE @ 0x00617da0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;
    /// <remarks>MCX.EXE @ 0x00617f00</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00617f40</remarks>
    void draw() override;
    /// <summary>Port: draws itself each frame from its text, caret and blink state.</summary>
    bool DrawsLive() override { return true; }
    /// <remarks>MCX.EXE @ 0x006180d0</remarks>
    void display() override;
    /// <summary>Typing, editing keys, and Enter (sends the line to the chat window).</summary>
    /// <remarks>MCX.EXE @ 0x00618130</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// With <paramref name="maxLines"/> 0, draws the text wrapped over lines; otherwise only counts its wrapped lines.
    /// </summary>
    /// <returns>Nonzero when the text needs fewer than <paramref name="maxLines"/> extra lines.</returns>
    /// <remarks>MCX.EXE @ 0x00617f60</remarks>
    int drawAndCheck(int32_t maxLines);
    /// <remarks>MCX.EXE @ 0x00618480</remarks>
    void setCursorPos(int32_t pos);

    /// <summary>The "to team only" toggle.</summary>
    aToolButton* teamButton = nullptr; // +0x4ac
    char text[256] = {};               // +0x4b0
    int32_t cursorPos = 0;             // +0x5b0
    /// <summary>The caret's x (0x15 by init), set by <see cref="setCursorPos"/>.</summary>
    int32_t cursorX = 0; // +0x5b4
    /// <summary>The caret's top y.</summary>
    int32_t cursorY = 0; // +0x5b8
    /// <summary>The caret's blink phase: nonzero draws it in colour 0x10, zero in 0x1f.</summary>
    int32_t cursorVisible = 0;  // +0x5bc
    aFont* inputFont = nullptr; // +0x5c0
};

/// <summary>The in-game chat window: the incoming lines and the <see cref="aChatInput"/> under them.</summary>
/// <remarks>Original source: <c>gui\atextbox.cpp</c>, 0x4b0 bytes. Vtable 0x007848b8.</remarks>
class aChatWindow : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x00618590</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x006186c0</remarks>
    void destroy() override;

    /// <summary>
    /// A network chat message arrived from player <paramref name="fromID"/>: <paramref name="data"/> + 8 is the "to
    /// all" flag, + 9 the text.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00618700</remarks>
    void handleNetworkMessage(uint32_t fromID, void* data);
    /// <summary>Shows <paramref name="text"/> from player <paramref name="playerId"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00618730</remarks>
    void processChatString(uint32_t playerId, char* text, int32_t color);

    /// <summary>
    /// Port: draws the lines from the bottom up, each above the next. (The original scrolled its picture up by a
    /// new line's height and drew the line at the bottom; the lines are kept instead.)
    /// </summary>
    void draw() override;
    /// <summary>Port: draws itself each frame from its lines.</summary>
    bool DrawsLive() override { return true; }

    aChatInput* chatInput = nullptr; // +0x4ac

    /// <summary>Port: a line as the chat formatter takes it (with its colour codes), and its height.</summary>
    struct ChatLine
    {
        std::string text;
        int32_t height = 0;
    };

    /// <summary>Port: the lines still (partly) on show, oldest first.</summary>
    std::vector<ChatLine> chatLines;
};

/// <summary>The scroll text thumb's paint routine.</summary>
/// <remarks>MCX.EXE @ 0x00616bd0</remarks>
void PaintScrollTab(aObject* obj);
/// <summary>The scroll text thumb's event routine.</summary>
/// <remarks>MCX.EXE @ 0x00616c60</remarks>
void ScrollTabEventHandler(aObject* obj, aEvent* event);
/// <summary>The network callback for chat messages during a scenario.</summary>
/// <remarks>MCX.EXE @ 0x00617d80</remarks>
void ScenarioChatCallback(FIDPMessage* message, void* data);

extern int16_t rOffset;
/// <summary>The players' chat colours (initialised data: 1, 3, 4, 2, 6, 5).</summary>
extern int32_t playerColor[6];
extern int FirstReturn;
