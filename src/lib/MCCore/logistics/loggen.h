#pragma once

#include "gui/asystem.h"
#include "gui/scrlpane.h"
#include "logistics/lport.h"

class aFont;
class FitIniFile;
class FIDPSession;
class FileScrollPane;

/// <summary>A button's callback on the logistics screens: an <see cref="aCallback"/> in a logistics block.</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x10 bytes (no fields of its own).</remarks>
class lCallback : public aCallback
{
public:
};

/// <summary>
/// A logistics-screen push button: up/over/down/gray pictures, a press sound, an enter (hover) sound and a callback
/// run when it is clicked.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c> and <c>loggen.h</c>, 0x4e4 bytes.</remarks>
class lButton : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006e0750 (vector deleting destructor)</remarks>
    ~lButton() override;

    /// <summary>Places the button, makes its callback, clears its pictures and wipes its port.</summary>
    /// <remarks>MCX.EXE @ 0x006e4420</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>Frees the four pictures and the callback.</summary>
    /// <remarks>MCX.EXE @ 0x006e44e0</remarks>
    void destroy() override;

    /// <summary>
    /// Shows the picture for the button's state: gray while disabled, down while <see cref="pressed"/> or held (grabbed
    /// with the mouse over it), over while the mouse is over it, else up; the back colour when that picture is
    /// missing. Port: drawn each frame from the state.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e49d0</remarks>
    void draw() override;

    /// <summary>Port: the button draws itself each frame from its state.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// On a click: plays the press sound and runs the callback (or the "disabled" sound when grayed), then passes the
    /// event to the event routine. Port: a release ends the press shown.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e4930</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>The mouse came over the button: highlights it and plays the enter sound.</summary>
    /// <remarks>MCX.EXE @ 0x006e4af0</remarks>
    void enter() override;

    /// <summary>The mouse left the button: drops the highlight (and a press still shown).</summary>
    /// <remarks>MCX.EXE @ 0x006e0690</remarks>
    void leave() override
    {
        if (overState)
        {
            overState = 0;

            if (heldButton == this)
            {
                LetGoPress();
            }
        }
    }

    /// <summary>Port: the button shows pressed (a click on it); any other button's press ends.</summary>
    void Press();

    /// <summary>
    /// Port: the press shown ends (the release, the mouse leaving or coming back, or a screen shown afresh: the
    /// original's next paint showed the button up).
    /// </summary>
    static void LetGoPress();

    /// <remarks>MCX.EXE @ 0x006e06b0</remarks>
    virtual lPort* getUpPicture() { return upPicture; }
    /// <remarks>MCX.EXE @ 0x006e06c0</remarks>
    virtual lPort* getOverPicture() { return overPicture; }
    /// <remarks>MCX.EXE @ 0x006e06d0</remarks>
    virtual lPort* getDownPicture() { return downPicture; }
    /// <remarks>MCX.EXE @ 0x006e06e0</remarks>
    virtual lPort* getGrayPicture() { return grayPicture; }
    /// <summary>The callback run on a click (set its function with <c>aCallback::setExec</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006e06f0</remarks>
    virtual lCallback* callback() { return buttonCallback; }

    /// <summary>Loads the picture shown normally.</summary>
    /// <remarks>MCX.EXE @ 0x006e4600</remarks>
    int32_t setUpPicture(char* fileName);
    /// <summary>Loads the picture shown under the mouse.</summary>
    /// <remarks>MCX.EXE @ 0x006e46f0</remarks>
    int32_t setOverPicture(char* fileName);
    /// <summary>Loads the picture shown while disabled.</summary>
    /// <remarks>MCX.EXE @ 0x006e47b0</remarks>
    int32_t setGrayPicture(char* fileName);
    /// <summary>Loads the picture shown while pressed.</summary>
    /// <remarks>MCX.EXE @ 0x006e4870</remarks>
    int32_t setDownPicture(char* fileName);

    /// <summary>
    /// Nonzero from a click: shows the down picture. Port: until the release or the mouse leaving (the original's next
    /// paint showed it up again).
    /// </summary>
    int32_t pressed = 0;                 // +0x4bc
    lPort* upPicture = nullptr;          // +0x4c0
    lPort* downPicture = nullptr;        // +0x4c4
    lPort* grayPicture = nullptr;        // +0x4c8
    lPort* overPicture = nullptr;        // +0x4cc
    lCallback* buttonCallback = nullptr; // +0x4d0
    /// <summary>Nonzero when the button is grayed out and ignores clicks.</summary>
    int32_t disabled = 0; // +0x4d4
    /// <summary>Nonzero while the mouse is over the button.</summary>
    int32_t overState = 0; // +0x4d8
    /// <summary>The digital sample played on a click (0xf by default; the ini's PressSFX).</summary>
    uint32_t pressSound = 0xf; // +0x4dc
    /// <summary>The digital sample played when the mouse comes over the button (the ini's OverSFX; -1 = none).</summary>
    uint32_t overSound = 0xffffffff; // +0x4e0

    /// <summary>Port: the button shown pressed (one mouse: one press at a time), or null.</summary>
    static inline lButton* heldButton = nullptr;

protected:
    /// <summary>
    /// Port: draws <paramref name="picture"/> as the button's face (with 0xff as a colour key when
    /// <paramref name="keyed"/>), or fills the button with its back colour when there is none, then the background and
    /// children as an lObject. Only in the frame pass.
    /// </summary>
    void drawFace(lPort* picture, bool keyed);
};

/// <summary>
/// A one-line text entry field: a buffer of <see cref="bufferSize"/> characters edited with the keyboard, a copy of
/// the original text, a blinking cursor, and a filter on the characters it accepts.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\loggen.cpp</c>, 0x4ec bytes. Created with <c>new</c>, <c>lObject::init</c> and
/// <see cref="initBuffer"/>; it has no init of its own.
/// </remarks>
class lTextObject : public lObject
{
public:
    /// <summary>Which characters <see cref="isValid"/> accepts.</summary>
    enum InputType : int32_t
    {
        /// <summary>Not editable.</summary>
        INPUT_NONE = -1,
        /// <summary>Printable ASCII (' '..'~').</summary>
        INPUT_TEXT = 0,
        /// <summary>Digits only.</summary>
        INPUT_DIGITS = 1,
        /// <summary>Anything.</summary>
        INPUT_ANY = 2,
        /// <summary>'2'..'6' only (serial/modem port numbers).</summary>
        INPUT_PORT = 3
    };

    /// <remarks>MCX.EXE @ 0x006e2640 (vector deleting destructor)</remarks>
    ~lTextObject() override;

    /// <summary>Frees the text buffers.</summary>
    /// <remarks>MCX.EXE @ 0x006e1400</remarks>
    void destroy() override;

    /// <summary>
    /// Wipes the field, writes the text and draws the cursor when it has a valid position (lit, or in the field's
    /// colour; the original drew the cursor in display, over the picture).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e1460</remarks>
    void draw() override;

    /// <summary>Displays as an <see cref="lObject"/> (the cursor is drawn by <see cref="draw"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006e14b0</remarks>
    void display() override;

    /// <summary>Port: the field draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: an edit puts the cursor back to its first blink phase (the original's paint after an edit did).
    /// </summary>
    void RestartBlink();

    /// <summary>Moves the cursor to character <paramref name="pos"/> and works out its pixel position.</summary>
    /// <remarks>MCX.EXE @ 0x006e1520</remarks>
    void setCursorPos(int32_t pos);

    /// <summary>Whether <paramref name="key"/> may be typed into this field (<see cref="InputType"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006e1570</remarks>
    int isValid(char key);

    /// <summary>
    /// Typing, backspace, enter (tells the parent), escape (<c>Cancel</c>), focus and the cursor blink timer.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e15e0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>
    /// (Re)allocates both buffers at <paramref name="size"/> characters (0 keeps the current ones), clears them and
    /// sets the <see cref="InputType"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e1930</remarks>
    void initBuffer(int32_t size, int32_t type);

    /// <summary>Sets the text (truncated to the buffer) and redraws.</summary>
    /// <returns>0, or -1 when it had to be truncated.</returns>
    /// <remarks>MCX.EXE @ 0x006e1a20</remarks>
    int32_t setStringBuffer(char* text);

    /// <summary>The size of both buffers, in characters.</summary>
    int32_t bufferSize = 0; // +0x4bc
    /// <summary>The text being edited.</summary>
    char* buffer = nullptr; // +0x4c0
    /// <summary>The text as last set (backspace on unchanged text clears it all).</summary>
    char* originalBuffer = nullptr; // +0x4c4
    /// <summary>The length of the text.</summary>
    int32_t textLength = 0; // +0x4c8
    /// <summary>The cursor, in characters (-1 = none).</summary>
    int32_t cursorPos = -1; // +0x4cc
    /// <summary>The cursor, in pixels from the left.</summary>
    int32_t cursorPixel = 0; // +0x4d0
    /// <summary>The <see cref="InputType"/>.</summary>
    int32_t inputType = INPUT_NONE; // +0x4dc
    /// <summary>The cursor blink phase (toggled by the blink timer).</summary>
    int32_t cursorOn = -1; // +0x4e0
    aFont* font = nullptr; // +0x4e4
    /// <summary>Nonzero: on focus, text equal to <c>EmptyFile</c> is cleared so the player can type a name.</summary>
    int32_t clearEmptyOnFocus = 0; // +0x4e8
};

/// <summary>
/// A scrolling list of colored text lines with a draggable scroll tab and up to four highlighted lines. Each line is
/// stored as a color byte followed by the text and a newline, in a 4 KB buffer.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4f8 bytes.</remarks>
class lScrollTextObject : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006e5fe0 (vector deleting destructor)</remarks>
    ~lScrollTextObject() override;

    /// <summary>
    /// Places the list, makes the scroll tab and the text buffer, and prints <paramref name="text"/> if given (a
    /// list with no text doesn't grow its port to fit).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e6440</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;

    /// <summary>Frees the scroll tab and the text.</summary>
    /// <remarks>MCX.EXE @ 0x006e65f0</remarks>
    void destroy() override;

    /// <summary>Draws the highlight bars and every line in its color (a tab starts a second column).</summary>
    /// <remarks>MCX.EXE @ 0x006e6650</remarks>
    void draw() override;

    /// <summary>Draws the visible part of the lines (scrolled by <see cref="firstPixel"/>), then the children.</summary>
    /// <remarks>MCX.EXE @ 0x006e6990</remarks>
    void display() override;

    /// <summary>Port: the list draws itself each frame; its port is a view as tall as all its lines.</summary>
    bool DrawsLive() override { return true; }

    /// <remarks>MCX.EXE @ 0x006e6a20</remarks>
    void resize(int32_t width, int32_t height) override;

    /// <summary>Appends a line of <paramref name="text"/> in <paramref name="color"/> (null = an empty line).</summary>
    /// <remarks>MCX.EXE @ 0x006e6b60</remarks>
    virtual void Print(char* text, uint8_t color);

    /// <summary>Appends <paramref name="text"/> word-wrapped to <paramref name="width"/> pixels.</summary>
    /// <remarks>MCX.EXE @ 0x006e6cb0</remarks>
    virtual void PrintWrapped(char* text, uint8_t color, int32_t width);

    /// <summary>Empties the list and its highlights.</summary>
    /// <remarks>MCX.EXE @ 0x006e6d80</remarks>
    virtual void Clear();

    /// <summary>Resizes the port to the height of all the lines (never below the visible height).</summary>
    /// <remarks>MCX.EXE @ 0x006e6ad0</remarks>
    void ResetPortSize();

    /// <summary>Converts a scroll tab position into the first visible pixel row.</summary>
    /// <remarks>MCX.EXE @ 0x006e6dd0</remarks>
    void CalcFirstPixel(int32_t tabPos);

    /// <summary>Moves the scroll tab to match <see cref="firstPixel"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e6e50</remarks>
    void PositionScrollTab();

    /// <summary>
    /// Scrolls for a click: <paramref name="direction"/> -1 a line up, 1 a line down, 0 a page towards
    /// <paramref name="yPos"/> (above or below the thumb).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e6f70</remarks>
    void ReceiveClick(int32_t direction, int32_t yPos);

    /// <summary>Port-only: the mouse wheel scrolls a line per notch, as the arrows do. Not taken when the text fits.</summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>Copies line <paramref name="line"/> (without its color byte) into <paramref name="dest"/>.</summary>
    /// <returns>-1 when the line exists, else 0.</returns>
    /// <remarks>MCX.EXE @ 0x006e7090</remarks>
    int getTextLine(int32_t line, char* dest, int32_t destSize);

    /// <summary>The first visible pixel row of the port.</summary>
    int32_t firstPixel = 0; // +0x4bc
    /// <summary>The color of each highlight bar.</summary>
    uint8_t highlightColor[4] = {0xff, 0xff, 0xff, 0xff}; // +0x4c0
    /// <summary>The line under each highlight bar (-1 = none).</summary>
    int32_t highlightLine[4] = {-1, -1, -1, -1}; // +0x4c4
    /// <summary>The lines: a color byte, the text, a newline; 0x1000 bytes plus a terminator.</summary>
    char* text = nullptr; // +0x4d4
    /// <summary>The draggable scroll tab.</summary>
    lObject* scrollTab = nullptr; // +0x4d8
    int16_t numLines = 0;         // +0x4dc
    /// <summary>The bytes used in <see cref="text"/>.</summary>
    int32_t textLength = 0; // +0x4e0
    /// <summary>Nonzero when the list scrolls (the port grows to fit the lines).</summary>
    int32_t scrolling = -1; // +0x4ec
    /// <summary>The x of the second column (text after a tab); negative = tabs print as spaces.</summary>
    int32_t tabColumn = -1; // +0x4f0
    /// <summary>The font size column of <c>fonts</c> (the row is the line's color).</summary>
    int32_t fontIndex = 0; // +0x4f4
};

/// <summary>
/// The multiplayer game browser: the open sessions (up to 64) as lines of an <see cref="lScrollTextObject"/>, one
/// selected.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x910 bytes.</remarks>
class GameList : public lScrollTextObject
{
public:
    static constexpr int32_t MAX_GAMES = 64;

    /// <remarks>MCX.EXE @ 0x006e6020 (vector deleting destructor)</remarks>
    ~GameList() override;

    /// <remarks>MCX.EXE @ 0x006e7110</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) override;

    /// <summary>Port: draws the lines as an <see cref="lScrollTextObject"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e7160</remarks>
    void draw() override;

    /// <summary>
    /// Port: the first part of the original's draw: rebuilds the lines from the session list ("name  players" or
    /// "name FULL", the selection highlighted). Called when the sessions or the selection change.
    /// </summary>
    void RebuildLines();

    /// <summary>A click selects a game; a refresh (event 0x13) re-reads the sessions from the session manager.</summary>
    /// <remarks>MCX.EXE @ 0x006e72c0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>The selected session's GUID, or null (and a refresh) when the selection is gone.</summary>
    /// <remarks>MCX.EXE @ 0x006e7570</remarks>
    _GUID* getSelectedGame();

    _GUID sessions[MAX_GAMES] = {}; // +0x4f8
    int32_t numSessions = -1;       // +0x8f8
    int32_t selectedSession = -1;   // +0x8fc
    /// <summary>The selected session's GUID, to find it again after a refresh.</summary>
    _GUID selectedGuid = {}; // +0x900
};

/// <summary>A horizontal slider (the preferences screen's volume and brightness bars).</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4cc bytes.</remarks>
class lSlider : public lObject
{
public:
    /// <summary>Loads the thumb picture (<c>prefs_02.tga</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006e75d0</remarks>
    lSlider();
    /// <remarks>MCX.EXE @ 0x006e7660 (vector deleting destructor)</remarks>
    ~lSlider() override;

    /// <remarks>MCX.EXE @ 0x006e76a0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;

    /// <summary>Frees the thumb picture.</summary>
    /// <remarks>MCX.EXE @ 0x006e76e0</remarks>
    void destroy() override;

    /// <summary>Draws the thumb at the current value.</summary>
    /// <remarks>MCX.EXE @ 0x006e7720</remarks>
    void draw() override;

    /// <summary>Port: the slider draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sets the value, clamped to [<see cref="minValue"/>, <see cref="maxValue"/>].</summary>
    /// <remarks>MCX.EXE @ 0x006e77b0</remarks>
    void setCurrentValue(int32_t value);

    /// <summary>Dragging moves the value; then the event routine runs.</summary>
    /// <remarks>MCX.EXE @ 0x006e7800</remarks>
    void handleEvent(aEvent* event) override;

    int32_t minValue = 0;     // +0x4bc
    int32_t maxValue = 0;     // +0x4c0
    int32_t currentValue = 0; // +0x4c4
    /// <summary>The thumb picture.</summary>
    lPort* thumbPort = nullptr; // +0x4c8
};

/// <summary>
/// Port-only: a drop-down list for the logistics screens (the preferences screen's DIFFICULTY and RENDERER). A field
/// shows the current choice; a click opens a list of the choices below it, a click on a row chooses it. The control
/// edits a setting it points at (its model): what it shows is always the setting's value, so a setting changed
/// elsewhere (CANCEL putting the old one back) shows at once. It draws every frame from its state, in the preferences
/// panel's style: a 0x14 outline, white font in 0xe3, the hovered row lit in 0x14.
/// </summary>
/// <remarks>
/// While open the control is taller (the list is part of it), in front of its siblings and holds the mouse grab, so
/// every mouse and key event comes to it until it closes: a press outside closes it without a change. A choice that
/// changes the setting runs the changed routine once, after the grab is let go.
/// Mouse: a click on the field opens or closes; a release over a row chooses it (also at the end of a drag from the
/// field); the wheel moves the choice (closed) or the lit row (open). Keys: up and down do the same, Return chooses the
/// lit row, Escape closes. Keys reach a closed control while the mouse is over it, as for every logistics control.
/// </remarks>
class lComboBox : public lObject
{
public:
    /// <summary>One choice: the text shown and the setting's value for it.</summary>
    struct Item
    {
        /// <summary>The text shown (white font's characters).</summary>
        std::string Label;
        /// <summary>The setting's value for this choice.</summary>
        int32_t Value = 0;
    };

    /// <summary>The field's height, and each row's in the list.</summary>
    static constexpr int32_t FieldHeight = 11;
    static constexpr int32_t RowHeight = 10;
    /// <summary>The rows the list shows at most; longer lists scroll.</summary>
    static constexpr int32_t MaxRows = 8;

    /// <summary>Calls <see cref="destroy"/>.</summary>
    ~lComboBox() override;

    /// <summary>
    /// Places the field at (<paramref name="xPos"/>, <paramref name="yPos"/>), <paramref name="width"/> wide, editing
    /// <paramref name="setting"/> with <paramref name="items"/>; <paramref name="changed"/> (may be null) runs with the
    /// new value after each choice that changes the setting.
    /// </summary>
    void init(int32_t xPos, int32_t yPos, int32_t width, int32_t* setting, std::vector<Item> items,
              void (*changed)(int32_t value));

    /// <summary>Closes the list (letting go of the grab) before the object goes.</summary>
    void destroy() override;

    /// <summary>Draws the field and, while open, the list, from the state.</summary>
    void draw() override;

    bool DrawsLive() override { return true; }

    /// <summary>The mouse and keys, as the class remarks say.</summary>
    void handleEvent(aEvent* event) override;

    /// <summary>The wheel moves the choice while closed, the lit row while open.</summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>Whether the list is open.</summary>
    bool IsOpen() const { return _Open; }

    /// <summary>The index of the item the setting holds, or -1 when it holds none of them.</summary>
    int32_t Selected() const;

    /// <summary>The lit row (an item index) while open.</summary>
    int32_t Hovered() const { return _Hovered; }

    /// <summary>The items.</summary>
    const std::vector<Item>& Items() const { return _Items; }

    /// <summary>Opens the list: the lit row is the current choice; the control comes in front and takes the mouse.</summary>
    void Open();

    /// <summary>Closes the list without a choice and lets go of the mouse.</summary>
    void Close();

    /// <summary>
    /// Sets the setting to item <paramref name="index"/>'s value; when that changes it, runs the changed routine.
    /// </summary>
    void Choose(int32_t index);

    /// <summary>The item under the screen point (<paramref name="xPos"/>, <paramref name="yPos"/>) in the open list, or -1.</summary>
    int32_t RowAt(int32_t xPos, int32_t yPos);

    /// <summary>White font's inked values in the label colour 0xe3 (255 stays out), registered once.</summary>
    static uint8_t* LabelColors();

private:
    /// <summary>The rows the open list shows.</summary>
    int32_t VisibleRows() const;

    /// <summary>Shrinks the control back to its field (the mouse stays as it is).</summary>
    void CloseList();

    /// <summary>Lights row <paramref name="index"/> (clamped) and scrolls it into view.</summary>
    void Hover(int32_t index);

    /// <summary>Whether the screen point lies on the field.</summary>
    bool InField(int32_t xPos, int32_t yPos);

    /// <summary>Puts this object last among its parent's children of its depth, so it draws over them.</summary>
    void RaiseAmongSiblings();

    /// <summary>Writes <paramref name="text"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>) in the label colour.</summary>
    void WriteLabel(int32_t xPos, int32_t yPos, const std::string& text);

    /// <summary>The setting the control edits (its model).</summary>
    int32_t* _Setting = nullptr;
    /// <summary>The choices, top to bottom.</summary>
    std::vector<Item> _Items;
    /// <summary>Runs with the new value after a choice that changed the setting (may be null).</summary>
    void (*_Changed)(int32_t value) = nullptr;
    /// <summary>Whether the list is open.</summary>
    bool _Open = false;
    /// <summary>The lit row (an item index) while open.</summary>
    int32_t _Hovered = -1;
    /// <summary>The first item the open list shows.</summary>
    int32_t _FirstRow = 0;
    /// <summary>A press closed the list: the control keeps the mouse until that press is let go.</summary>
    bool _HoldUntilRelease = false;
};

/// <summary>
/// Port: one of a <see cref="FileScrollPane"/>'s column headers (the selected save's operation, mission or
/// resource points). The original made them plain lObjects and painted their pictures from the pane; this one draws
/// its column of the pane's selected save each frame (white on black).
/// </summary>
class FileColumnHeader : public lObject
{
public:
    /// <summary>Wipes the header and writes the selected save's figure for its column, if it has one.</summary>
    void draw() override;

    /// <summary>The header draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The pane whose selection the header shows.</summary>
    FileScrollPane* pane = nullptr;
    /// <summary>0 the operation, 1 the mission, 2 the resource points.</summary>
    int32_t column = 0;
};

/// <summary>
/// The save/load game file list: every <c>.sav</c> (or <c>.mpk</c> in multiplayer) file in a directory with its
/// operation, mission and resource points, a scroll bar, and (on a save screen) a name entry field.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x550 bytes.</remarks>
class FileScrollPane : public ScrollPane
{
public:
    /// <remarks>MCX.EXE @ 0x006e42c0 (vector deleting destructor)</remarks>
    ~FileScrollPane() override;

    /// <summary>Places the pane, tiles its background and makes the scroll arrows and the three column headers.</summary>
    /// <remarks>MCX.EXE @ 0x006e1ae0</remarks>
    void init(int32_t xPos, int32_t yPos, int32_t width, int32_t height);

    /// <summary>Frees the file lists, the entry field, the headers and the arrow pictures.</summary>
    /// <remarks>MCX.EXE @ 0x006e1e10</remarks>
    void destroy() override;

    /// <summary>Draws the pane (the files through <see cref="drawContent"/>).</summary>
    /// <remarks>MCX.EXE @ 0x006e1fe0</remarks>
    void draw() override;

    /// <summary>
    /// Port: the splash arrows have no pressed art. (The original's draw put them back over the pressed art straight
    /// away; see OB-132 for the plain arrows a release left.)
    /// </summary>
    lPort* PressedArrowArt(bool down) override;

    /// <summary>Port: draws the file lines (<see cref="drawFiles"/>) into the content view.</summary>
    void drawContent() override;

    /// <summary>Draws the pane when shown, then the column headers and the children (the name entry).</summary>
    /// <remarks>MCX.EXE @ 0x006e2220</remarks>
    void display() override;

    /// <summary>Selecting a file (fills the entry field on a save pane), scrolling, and double clicks.</summary>
    /// <remarks>MCX.EXE @ 0x006e2310</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Sizes the slider to the number of files.</summary>
    /// <remarks>MCX.EXE @ 0x006e2680</remarks>
    void setUpSlider() override;

    /// <summary>The file under (<paramref name="xPos"/>, <paramref name="yPos"/>), or -1.</summary>
    /// <remarks>MCX.EXE @ 0x006e27d0</remarks>
    int32_t getFileAtPosition(int32_t xPos, int32_t yPos);

    /// <summary>Sets the directory and lists its files.</summary>
    /// <remarks>MCX.EXE @ 0x006e2860</remarks>
    void setStartDirectory(char* directory);

    /// <summary>Draws the file lines into the content (a view while the pane draws).</summary>
    /// <remarks>MCX.EXE @ 0x006e28d0</remarks>
    void drawFiles();

    /// <summary>
    /// Port: the first part of the original's drawFiles: sizes the content to the files (at least the pane's height)
    /// and resets the scroll when that changes it. Called when the list changes.
    /// </summary>
    void layoutFiles();

    /// <summary>
    /// Lists every <paramref name="extension"/> file of the directory, reading each save's mission and resource
    /// points; <paramref name="sort"/> sorts them by name. On a save pane the first entry is the empty file.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e2a50</remarks>
    void getAllFiles(char* extension, bool sort);

    /// <summary>Selects file <paramref name="file"/> and scrolls it into view.</summary>
    /// <remarks>MCX.EXE @ 0x006e33a0</remarks>
    void setSelectedFile(int32_t file);

    /// <summary>Switches between single player saves (<c>.sav</c>) and multiplayer ones (<c>.mpk</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006e3510</remarks>
    void setMultiplayer(int multiplayer);

    int32_t selectedFile = -1; // +0x508
    int32_t multiplayer = 0;   // +0x50c
    /// <summary>The three column headers.</summary>
    FileColumnHeader* columnHeaders[3] = {}; // +0x510
    /// <summary>The scroll-up arrow picture.</summary>
    lPort* upArrowPort = nullptr; // +0x51c
    /// <summary>The scroll-down arrow picture.</summary>
    lPort* downArrowPort = nullptr; // +0x520
    char* startDirectory = nullptr; // +0x524
    /// <summary>The file names (without extension).</summary>
    char** fileNames = nullptr; // +0x528
    /// <summary>Each file's operation number.</summary>
    int32_t* fileOperations = nullptr; // +0x52c
    /// <summary>Each file's mission number.</summary>
    int32_t* fileMissions = nullptr; // +0x530
    /// <summary>Each file's resource points.</summary>
    uint32_t* fileResourcePoints = nullptr; // +0x534
    int32_t numFiles = 0;                   // +0x538
    /// <summary>Nonzero on the save screen (the ini's SavePane): has the name entry field.</summary>
    int32_t savePane = 0; // +0x544
    /// <summary>The save name entry field.</summary>
    lTextObject* nameEntry = nullptr; // +0x548
    /// <summary>The height of a file line (the white font's height + 1).</summary>
    int32_t lineHeight = 0; // +0x54c
};

/// <summary>
/// A logistics screen built from an ini file: a list of elements (background, buttons, text fields, file panes,
/// scrolling text, game lists, ...) read from the <c>[Element#]</c> blocks.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\loggen.cpp</c>, 0x4dc bytes. It had no constructor of its own: its fields were
/// set by the derived class's.
/// </remarks>
class GenericScreen : public lObject
{
public:
    /// <remarks>MCX.EXE @ 0x006e4d50 (vector deleting destructor)</remarks>
    ~GenericScreen() override;

    /// <summary>Loads the palette of a TGA under <c>artPath</c> into <see cref="palette"/> (6-bit channels).</summary>
    /// <remarks>MCX.EXE @ 0x006e3840</remarks>
    uint8_t* getPaletteFromArt(char* fileName);

    /// <summary>Makes the elements the ini file describes, with their art, sounds and callbacks.</summary>
    /// <remarks>MCX.EXE @ 0x006e3a10</remarks>
    int32_t init(FitIniFile* screenFile);

    /// <summary>Removes and deletes the elements (element 0 is the screen itself) and the palette.</summary>
    /// <remarks>MCX.EXE @ 0x006e4310</remarks>
    void destroy() override;

    /// <summary>Escape cancels; then the event routine runs.</summary>
    /// <remarks>MCX.EXE @ 0x006e43e0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>Port: the screen draws its background art each frame (its port was the art).</summary>
    void draw() override;

    /// <summary>Port: the screen draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Shows or hides the screen; showing the load/save screen grays the buttons that have nothing to act on.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e3540 (name inferred from the vtable slot)</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>The elements; element 0 is the screen itself.</summary>
    aObject** elements = nullptr; // +0x4bc
    int32_t numElements = 0;      // +0x4c0
    /// <summary>The background art's palette (0x300 bytes), when an element has UseBackPalette.</summary>
    uint8_t* palette = nullptr; // +0x4c8
    /// <summary>The file pane element, if any.</summary>
    FileScrollPane* filePane = nullptr; // +0x4cc
    /// <summary>The load or save button (callbacks 8 and 9).</summary>
    lButton* loadSaveButton = nullptr; // +0x4d0
    /// <summary>The delete button (callback 10).</summary>
    lButton* deleteButton = nullptr; // +0x4d4
    /// <summary>The cancel button (callback 11).</summary>
    lButton* cancelButton = nullptr; // +0x4d8

    /// <summary>
    /// Port: the background art, which the original loaded into the screen's own port (a splash screen's is the
    /// shared <c>genericPort</c>, not owned).
    /// </summary>
    lPort* artPort = nullptr;
};

/// <summary>
/// Port: a picture element of a generic screen (element type 6), which the original made as a plain lObject with the
/// art loaded into its port. It draws the art each frame.
/// </summary>
class lImage : public lObject
{
public:
    /// <summary>Frees the art.</summary>
    void destroy() override;

    /// <summary>Copies the art (opaque, as the picture was copied).</summary>
    void draw() override;

    /// <summary>The image draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The picture (owned).</summary>
    lPort* art = nullptr;
};

/// <summary>
/// A <see cref="GenericScreen"/> with "blocks": sets of elements shown together (the main menu's pages), read from
/// the ini. All splash screens share one background port.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4e4 bytes.</remarks>
class MCSplashScreen : public GenericScreen
{
public:
    /// <summary>Makes the shared background port with the first instance.</summary>
    /// <remarks>MCX.EXE @ 0x006e4b70</remarks>
    MCSplashScreen();
    /// <summary>Frees the shared port with the last instance, and the blocks.</summary>
    /// <remarks>MCX.EXE @ 0x006e4c80; vector deleting destructor @ 0x006e4c50</remarks>
    ~MCSplashScreen() override;

    /// <summary>Makes the elements and reads the blocks.</summary>
    /// <remarks>MCX.EXE @ 0x006e4d90</remarks>
    int32_t init(FitIniFile* screenFile);

    /// <summary>Frees the blocks.</summary>
    /// <remarks>MCX.EXE @ 0x006e60a0</remarks>
    void destroy() override;

    /// <summary>Shows or hides the screen, starting or stopping the attract-mode timer on the main menus.</summary>
    /// <remarks>MCX.EXE @ 0x006e6120</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>Shows only the elements listed in block <paramref name="block"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e61d0</remarks>
    void showBlock(int32_t block);

protected:
    /// <summary>The background art file loaded into <see cref="genericPort"/>.</summary>
    static char genericPortFileName[256];
    /// <summary>The background port all splash screens share.</summary>
    static lPort* genericPort;
    static int32_t instanceCount;

public:
    int32_t numBlocks = -1; // +0x4dc
    /// <summary>Each block: a list of element numbers (0-terminated) to show.</summary>
    uint8_t** blocks = nullptr; // +0x4e0
};

/// <summary>Paints a logistics scroll tab: a filled box with a light top/left and dark bottom/right edge.</summary>
/// <remarks>MCX.EXE @ 0x006e6260</remarks>
void LogPaintScrollTab(aObject* tab);

/// <summary>The scroll tab's event routine: dragging it scrolls its <see cref="lScrollTextObject"/>.</summary>
/// <remarks>MCX.EXE @ 0x006e62f0 (original name lost)</remarks>
void LogScrollTabHandleEvent(aObject* tab, aEvent* event);

/// <summary>Whether <paramref name="session"/> was dropped from the game list (it had no players).</summary>
/// <returns>-1 when it was, else 0.</returns>
/// <remarks>MCX.EXE @ 0x006e7270</remarks>
int IsSessionDeleted(FIDPSession* session);

/// <summary>The logistics fonts, by color row and size column.</summary>
extern aFont* fonts[][3];
/// <summary>The large black logistics font.</summary>
extern aFont* lgBlackFont;
/// <summary>The large white logistics font.</summary>
extern aFont* lgWhiteFont;
/// <summary>Sessions dropped from the game list because they had no players.</summary>
extern _GUID deletedSessions[50];
extern int32_t nextDeletedSession;
/// <summary>
/// The placeholder name of an empty save slot (string 0x381, loaded into a 0xff-byte malloc block by the logistics
/// setup; null until then).
/// </summary>
extern char* EmptyFile;
