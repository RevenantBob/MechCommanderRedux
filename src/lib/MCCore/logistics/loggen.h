#pragma once

#include "gui/MCGuiSystem.h"
#include "gui/scrlpane.h"
#include "logistics/lport.h"

class MCGuiFont;
class MCFitIniFile;
class MCFidpSession;
class MCFileScrollPane;

/// <summary>A button's callback on the logistics screens: an <see cref="MCGuiCallback"/> in a logistics block.</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x10 bytes (no fields of its own).</remarks>
class MCLogCallback : public MCGuiCallback
{
public:
};

/// <summary>
/// A logistics-screen push button: up/over/down/gray pictures, a press sound, an enter (hover) sound and a callback
/// run when it is clicked.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c> and <c>loggen.h</c>, 0x4e4 bytes.</remarks>
class MCLogButton : public MCLogObject
{
public:
    ~MCLogButton() override;

    /// <summary>Places the button, makes its callback, clears its pictures and wipes its port.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>Frees the four pictures and the callback.</summary>
    void Destroy() override;

    /// <summary>
    /// Shows the picture for the button's state: gray while disabled, down while <see cref="Pressed"/> or held (grabbed
    /// with the mouse over it), over while the mouse is over it, else up; the back colour when that picture is
    /// missing. Port: drawn each frame from the state.
    /// </summary>
    void Draw() override;

    /// <summary>Port: the button draws itself each frame from its state.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// On a click: plays the press sound and runs the callback (or the "disabled" sound when grayed), then passes the
    /// event to the event routine. Port: a release ends the press shown.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>The mouse came over the button: highlights it and plays the enter sound.</summary>
    void Enter() override;

    /// <summary>The mouse left the button: drops the highlight (and a press still shown).</summary>
    void Leave() override
    {
        if (OverState)
        {
            OverState = 0;

            if (HeldButton == this)
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

    virtual MCLogPort* GetUpPicture() { return UpPicture; }
    virtual MCLogPort* GetOverPicture() { return OverPicture; }
    virtual MCLogPort* GetDownPicture() { return DownPicture; }
    virtual MCLogPort* GetGrayPicture() { return GrayPicture; }
    /// <summary>The callback run on a click (set its function with <c>aCallback::setExec</c>).</summary>
    virtual MCLogCallback* Callback() { return ButtonCallback; }

    /// <summary>Loads the picture shown normally.</summary>
    int32_t SetUpPicture(char* fileName);
    /// <summary>Loads the picture shown under the mouse.</summary>
    int32_t SetOverPicture(char* fileName);
    /// <summary>Loads the picture shown while disabled.</summary>
    int32_t SetGrayPicture(char* fileName);
    /// <summary>Loads the picture shown while pressed.</summary>
    int32_t SetDownPicture(char* fileName);

    /// <summary>
    /// Nonzero from a click: shows the down picture. Port: until the release or the mouse leaving (the original's next
    /// paint showed it up again).
    /// </summary>
    int32_t Pressed = 0;
    MCLogPort* UpPicture = nullptr;
    MCLogPort* DownPicture = nullptr;
    MCLogPort* GrayPicture = nullptr;
    MCLogPort* OverPicture = nullptr;
    MCLogCallback* ButtonCallback = nullptr;
    /// <summary>Nonzero when the button is grayed out and ignores clicks.</summary>
    int32_t Disabled = 0;
    /// <summary>Nonzero while the mouse is over the button.</summary>
    int32_t OverState = 0;
    /// <summary>The digital sample played on a click (0xf by default; the ini's PressSFX).</summary>
    uint32_t PressSound = 0xf;
    /// <summary>The digital sample played when the mouse comes over the button (the ini's OverSFX; -1 = none).</summary>
    uint32_t OverSound = 0xffffffff;

    /// <summary>Port: the button shown pressed (one mouse: one press at a time), or null.</summary>
    static inline MCLogButton* HeldButton = nullptr;

protected:
    /// <summary>
    /// Port: draws <paramref name="picture"/> as the button's face (with 0xff as a colour key when
    /// <paramref name="keyed"/>), or fills the button with its back colour when there is none, then the background and
    /// children as an lObject. Only in the frame pass.
    /// </summary>
    void DrawFace(MCLogPort* picture, bool keyed);
};

/// <summary>
/// A one-line text entry field: a buffer of <see cref="BufferSize"/> characters edited with the keyboard, a copy of
/// the original text, a blinking cursor, and a filter on the characters it accepts.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\loggen.cpp</c>, 0x4ec bytes. Created with <c>new</c>, <c>lObject::init</c> and
/// <see cref="InitBuffer"/>; it has no init of its own.
/// </remarks>
class MCLogTextObject : public MCLogObject
{
public:
    /// <summary>Which characters <see cref="IsValid"/> accepts.</summary>
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

    ~MCLogTextObject() override;

    /// <summary>Frees the text buffers.</summary>
    void Destroy() override;

    /// <summary>
    /// Wipes the field, writes the text and draws the cursor when it has a valid position (lit, or in the field's
    /// colour; the original drew the cursor in display, over the picture).
    /// </summary>
    void Draw() override;

    /// <summary>Displays as an <see cref="MCLogObject"/> (the cursor is drawn by <see cref="Draw"/>).</summary>
    void Display() override;

    /// <summary>Port: the field draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Port: an edit puts the cursor back to its first blink phase (the original's paint after an edit did).
    /// </summary>
    void RestartBlink();

    /// <summary>Moves the cursor to character <paramref name="pos"/> and works out its pixel position.</summary>
    void SetCursorPos(int32_t pos);

    /// <summary>Whether <paramref name="key"/> may be typed into this field (<see cref="InputType"/>).</summary>
    int IsValid(char key);

    /// <summary>
    /// Typing, backspace, enter (tells the parent), escape (<c>Cancel</c>), focus and the cursor blink timer.
    /// </summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>
    /// (Re)allocates both buffers at <paramref name="size"/> characters (0 keeps the current ones), clears them and
    /// sets the <see cref="InputType"/>.
    /// </summary>
    void InitBuffer(int32_t size, int32_t type);

    /// <summary>Sets the text (truncated to the buffer) and redraws.</summary>
    /// <returns>0, or -1 when it had to be truncated.</returns>
    int32_t SetStringBuffer(char* text);

    /// <summary>The size of both buffers, in characters.</summary>
    int32_t BufferSize = 0;
    /// <summary>The text being edited.</summary>
    char* Buffer = nullptr;
    /// <summary>The text as last set (backspace on unchanged text clears it all).</summary>
    char* OriginalBuffer = nullptr;
    /// <summary>The length of the text.</summary>
    int32_t TextLength = 0;
    /// <summary>The cursor, in characters (-1 = none).</summary>
    int32_t CursorPos = -1;
    /// <summary>The cursor, in pixels from the left.</summary>
    int32_t CursorPixel = 0;
    /// <summary>The <see cref="InputType"/>.</summary>
    int32_t AllowedInput = INPUT_NONE;
    /// <summary>The cursor blink phase (toggled by the blink timer).</summary>
    int32_t CursorOn = -1;
    MCGuiFont* Font = nullptr;
    /// <summary>Nonzero: on focus, text equal to <c>EmptyFile</c> is cleared so the player can type a name.</summary>
    int32_t ClearEmptyOnFocus = 0;
};

/// <summary>
/// A scrolling list of colored text lines with a draggable scroll tab and up to four highlighted lines. Each line is
/// stored as a color byte followed by the text and a newline, in a 4 KB buffer.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4f8 bytes.</remarks>
class MCLogScrollTextObject : public MCLogObject
{
public:
    ~MCLogScrollTextObject() override;

    /// <summary>
    /// Places the list, makes the scroll tab and the text buffer, and prints <paramref name="text"/> if given (a
    /// list with no text doesn't grow its port to fit).
    /// </summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;

    /// <summary>Frees the scroll tab and the text.</summary>
    void Destroy() override;

    /// <summary>Draws the highlight bars and every line in its color (a tab starts a second column).</summary>
    void Draw() override;

    /// <summary>Draws the visible part of the lines (scrolled by <see cref="FirstPixel"/>), then the children.</summary>
    void Display() override;

    /// <summary>Port: the list draws itself each frame; its port is a view as tall as all its lines.</summary>
    bool DrawsLive() override { return true; }

    void Resize(int32_t width, int32_t height) override;

    /// <summary>Appends a line of <paramref name="text"/> in <paramref name="color"/> (null = an empty line).</summary>
    virtual void Print(const char* text, uint8_t color);

    /// <summary>Appends <paramref name="text"/> word-wrapped to <paramref name="width"/> pixels.</summary>
    virtual void PrintWrapped(char* text, uint8_t color, int32_t width);

    /// <summary>Empties the list and its highlights.</summary>
    virtual void Clear();

    /// <summary>Resizes the port to the height of all the lines (never below the visible height).</summary>
    void ResetPortSize();

    /// <summary>Converts a scroll tab position into the first visible pixel row.</summary>
    void CalcFirstPixel(int32_t tabPos);

    /// <summary>Moves the scroll tab to match <see cref="FirstPixel"/>.</summary>
    void PositionScrollTab();

    /// <summary>
    /// Scrolls for a click: <paramref name="direction"/> -1 a line up, 1 a line down, 0 a page towards
    /// <paramref name="yPos"/> (above or below the thumb).
    /// </summary>
    void ReceiveClick(int32_t direction, int32_t yPos);

    /// <summary>Port-only: the mouse wheel scrolls a line per notch, as the arrows do. Not taken when the text fits.</summary>
    bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) override;

    /// <summary>Copies line <paramref name="line"/> (without its color byte) into <paramref name="dest"/>.</summary>
    /// <returns>-1 when the line exists, else 0.</returns>
    int GetTextLine(int32_t line, char* dest, int32_t destSize);

    /// <summary>The first visible pixel row of the port.</summary>
    int32_t FirstPixel = 0;
    /// <summary>The color of each highlight bar.</summary>
    uint8_t HighlightColor[4] = {0xff, 0xff, 0xff, 0xff};
    /// <summary>The line under each highlight bar (-1 = none).</summary>
    int32_t HighlightLine[4] = {-1, -1, -1, -1};
    /// <summary>The lines: a color byte, the text, a newline; 0x1000 bytes plus a terminator.</summary>
    char* Text = nullptr;
    /// <summary>The draggable scroll tab.</summary>
    MCLogObject* ScrollTab = nullptr;
    int16_t NumLines = 0;
    /// <summary>The bytes used in <see cref="Text"/>.</summary>
    int32_t TextLength = 0;
    /// <summary>Nonzero when the list scrolls (the port grows to fit the lines).</summary>
    int32_t Scrolling = -1;
    /// <summary>The x of the second column (text after a tab); negative = tabs print as spaces.</summary>
    int32_t TabColumn = -1;
    /// <summary>The font size column of <c>fonts</c> (the row is the line's color).</summary>
    int32_t FontIndex = 0;
};

/// <summary>
/// The multiplayer game browser: the open sessions (up to 64) as lines of an <see cref="MCLogScrollTextObject"/>, one
/// selected.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x910 bytes.</remarks>
class MCGameList : public MCLogScrollTextObject
{
public:
    static constexpr int32_t MAX_GAMES = 64;

    ~MCGameList() override;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* text) override;

    /// <summary>Port: draws the lines as an <see cref="MCLogScrollTextObject"/>.</summary>
    void Draw() override;

    /// <summary>
    /// Port: the first part of the original's draw: rebuilds the lines from the session list ("name  players" or
    /// "name FULL", the selection highlighted). Called when the sessions or the selection change.
    /// </summary>
    void RebuildLines();

    /// <summary>A click selects a game; a refresh (event 0x13) re-reads the sessions from the session manager.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>The selected session's GUID, or null (and a refresh) when the selection is gone.</summary>
    _GUID* GetSelectedGame();

    _GUID Sessions[MAX_GAMES] = {};
    int32_t NumSessions = -1;
    int32_t SelectedSession = -1;
    /// <summary>The selected session's GUID, to find it again after a refresh.</summary>
    _GUID SelectedGuid = {};
};

/// <summary>A horizontal slider (the preferences screen's volume and brightness bars).</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4cc bytes.</remarks>
class MCLogSlider : public MCLogObject
{
public:
    /// <summary>Loads the thumb picture (<c>prefs_02.tga</c>).</summary>
    MCLogSlider();
    ~MCLogSlider() override;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;

    /// <summary>Frees the thumb picture.</summary>
    void Destroy() override;

    /// <summary>Draws the thumb at the current value.</summary>
    void Draw() override;

    /// <summary>Port: the slider draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sets the value, clamped to [<see cref="MinValue"/>, <see cref="MaxValue"/>].</summary>
    void SetCurrentValue(int32_t value);

    /// <summary>Dragging moves the value; then the event routine runs.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    int32_t MinValue = 0;
    int32_t MaxValue = 0;
    int32_t CurrentValue = 0;
    /// <summary>The thumb picture.</summary>
    MCLogPort* ThumbPort = nullptr;
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
class MCLogComboBox : public MCLogObject
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

    /// <summary>Calls <see cref="Destroy"/>.</summary>
    ~MCLogComboBox() override;

    /// <summary>
    /// Places the field at (<paramref name="xPos"/>, <paramref name="yPos"/>), <paramref name="width"/> wide, editing
    /// <paramref name="setting"/> with <paramref name="items"/>; <paramref name="changed"/> (may be null) runs with the
    /// new value after each choice that changes the setting.
    /// </summary>
    void Init(int32_t xPos, int32_t yPos, int32_t width, int32_t* setting, std::vector<Item> items,
              void (*changed)(int32_t value));

    /// <summary>Closes the list (letting go of the grab) before the object goes.</summary>
    void Destroy() override;

    /// <summary>Draws the field and, while open, the list, from the state.</summary>
    void Draw() override;

    bool DrawsLive() override { return true; }

    /// <summary>The mouse and keys, as the class remarks say.</summary>
    void HandleEvent(MCGuiEvent* event) override;

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
/// Port: one of a <see cref="MCFileScrollPane"/>'s column headers (the selected save's operation, mission or
/// resource points). The original made them plain lObjects and painted their pictures from the pane; this one draws
/// its column of the pane's selected save each frame (white on black).
/// </summary>
class MCFileColumnHeader : public MCLogObject
{
public:
    /// <summary>Wipes the header and writes the selected save's figure for its column, if it has one.</summary>
    void Draw() override;

    /// <summary>The header draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The pane whose selection the header shows.</summary>
    MCFileScrollPane* Pane = nullptr;
    /// <summary>0 the operation, 1 the mission, 2 the resource points.</summary>
    int32_t Column = 0;
};

/// <summary>
/// The save/load game file list: every <c>.sav</c> (or <c>.mpk</c> in multiplayer) file in a directory with its
/// operation, mission and resource points, a scroll bar, and (on a save screen) a name entry field.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x550 bytes.</remarks>
class MCFileScrollPane : public MCScrollPane
{
public:
    ~MCFileScrollPane() override;

    /// <summary>Places the pane, tiles its background and makes the scroll arrows and the three column headers.</summary>
    void Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height);

    /// <summary>Frees the file lists, the entry field, the headers and the arrow pictures.</summary>
    void Destroy() override;

    /// <summary>Draws the pane (the files through <see cref="DrawContent"/>).</summary>
    void Draw() override;

    /// <summary>
    /// Port: the splash arrows have no pressed art. (The original's draw put them back over the pressed art straight
    /// away; see OB-132 for the plain arrows a release left.)
    /// </summary>
    MCLogPort* PressedArrowArt(bool down) override;

    /// <summary>Port: draws the file lines (<see cref="DrawFiles"/>) into the content view.</summary>
    void DrawContent() override;

    /// <summary>Draws the pane when shown, then the column headers and the children (the name entry).</summary>
    void Display() override;

    /// <summary>Selecting a file (fills the entry field on a save pane), scrolling, and double clicks.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Sizes the slider to the number of files.</summary>
    void SetUpSlider() override;

    /// <summary>The file under (<paramref name="xPos"/>, <paramref name="yPos"/>), or -1.</summary>
    int32_t GetFileAtPosition(int32_t xPos, int32_t yPos);

    /// <summary>Sets the directory and lists its files.</summary>
    void SetStartDirectory(char* directory);

    /// <summary>Draws the file lines into the content (a view while the pane draws).</summary>
    void DrawFiles();

    /// <summary>
    /// Port: the first part of the original's drawFiles: sizes the content to the files (at least the pane's height)
    /// and resets the scroll when that changes it. Called when the list changes.
    /// </summary>
    void LayoutFiles();

    /// <summary>
    /// Lists every <paramref name="extension"/> file of the directory, reading each save's mission and resource
    /// points; <paramref name="sort"/> sorts them by name. On a save pane the first entry is the empty file.
    /// </summary>
    void GetAllFiles(char* extension, bool sort);

    /// <summary>Selects file <paramref name="file"/> and scrolls it into view.</summary>
    void SetSelectedFile(int32_t file);

    /// <summary>Switches between single player saves (<c>.sav</c>) and multiplayer ones (<c>.mpk</c>).</summary>
    void SetMultiplayer(int multiplayer);

    int32_t SelectedFile = -1;
    int32_t Multiplayer = 0;
    /// <summary>The three column headers.</summary>
    MCFileColumnHeader* ColumnHeaders[3] = {};
    /// <summary>The scroll-up arrow picture.</summary>
    MCLogPort* UpArrowPort = nullptr;
    /// <summary>The scroll-down arrow picture.</summary>
    MCLogPort* DownArrowPort = nullptr;
    char* StartDirectory = nullptr;
    /// <summary>The file names (without extension).</summary>
    char** FileNames = nullptr;
    /// <summary>Each file's operation number.</summary>
    int32_t* FileOperations = nullptr;
    /// <summary>Each file's mission number.</summary>
    int32_t* FileMissions = nullptr;
    /// <summary>Each file's resource points.</summary>
    uint32_t* FileResourcePoints = nullptr;
    int32_t NumFiles = 0;
    /// <summary>Nonzero on the save screen (the ini's SavePane): has the name entry field.</summary>
    int32_t SavePane = 0;
    /// <summary>The save name entry field.</summary>
    MCLogTextObject* NameEntry = nullptr;
    /// <summary>The height of a file line (the white font's height + 1).</summary>
    int32_t LineHeight = 0;
};

/// <summary>
/// A logistics screen built from an ini file: a list of elements (background, buttons, text fields, file panes,
/// scrolling text, game lists, ...) read from the <c>[Element#]</c> blocks.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\loggen.cpp</c>, 0x4dc bytes. It had no constructor of its own: its fields were
/// set by the derived class's.
/// </remarks>
class MCGenericScreen : public MCLogObject
{
public:
    ~MCGenericScreen() override;

    /// <summary>Loads the palette of a TGA under <c>artPath</c> into <see cref="Palette"/> (6-bit channels).</summary>
    uint8_t* GetPaletteFromArt(char* fileName);

    /// <summary>Makes the elements the ini file describes, with their art, sounds and callbacks.</summary>
    int32_t Init(MCFitIniFile* screenFile);

    /// <summary>Removes and deletes the elements (element 0 is the screen itself) and the palette.</summary>
    void Destroy() override;

    /// <summary>Escape cancels; then the event routine runs.</summary>
    void HandleEvent(MCGuiEvent* event) override;

    /// <summary>Port: the screen draws its background art each frame (its port was the art).</summary>
    void Draw() override;

    /// <summary>Port: the screen draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>
    /// Shows or hides the screen; showing the load/save screen grays the buttons that have nothing to act on.
    /// </summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>The elements; element 0 is the screen itself.</summary>
    MCGuiObject** Elements = nullptr;
    int32_t NumElements = 0;
    /// <summary>The background art's palette (0x300 bytes), when an element has UseBackPalette.</summary>
    uint8_t* Palette = nullptr;
    /// <summary>The file pane element, if any.</summary>
    MCFileScrollPane* FilePane = nullptr;
    /// <summary>The load or save button (callbacks 8 and 9).</summary>
    MCLogButton* LoadSaveButton = nullptr;
    /// <summary>The delete button (callback 10).</summary>
    MCLogButton* DeleteButton = nullptr;
    /// <summary>The cancel button (callback 11).</summary>
    MCLogButton* CancelButton = nullptr;

    /// <summary>
    /// Port: the background art, which the original loaded into the screen's own port (a splash screen's is the
    /// shared <c>genericPort</c>, not owned).
    /// </summary>
    MCLogPort* ArtPort = nullptr;
};

/// <summary>
/// Port: a picture element of a generic screen (element type 6), which the original made as a plain lObject with the
/// art loaded into its port. It draws the art each frame.
/// </summary>
class MCLogImage : public MCLogObject
{
public:
    /// <summary>Frees the art.</summary>
    void Destroy() override;

    /// <summary>Copies the art (opaque, as the picture was copied).</summary>
    void Draw() override;

    /// <summary>The image draws itself each frame.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The picture (owned).</summary>
    MCLogPort* Art = nullptr;
};

/// <summary>
/// A <see cref="MCGenericScreen"/> with "blocks": sets of elements shown together (the main menu's pages), read from
/// the ini. All splash screens share one background port.
/// </summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x4e4 bytes.</remarks>
class MCSplashScreen : public MCGenericScreen
{
public:
    /// <summary>Makes the shared background port with the first instance.</summary>
    MCSplashScreen();
    /// <summary>Frees the shared port with the last instance, and the blocks.</summary>
    ~MCSplashScreen() override;

    /// <summary>Makes the elements and reads the blocks.</summary>
    int32_t Init(MCFitIniFile* screenFile);

    /// <summary>Frees the blocks.</summary>
    void Destroy() override;

    /// <summary>Shows or hides the screen, starting or stopping the attract-mode timer on the main menus.</summary>
    void ShowGuiWindow(bool show) override;

    /// <summary>Shows only the elements listed in block <paramref name="block"/>.</summary>
    void ShowBlock(int32_t block);

protected:
    /// <summary>The background art file loaded into <see cref="_GenericPort"/>.</summary>
    static char _GenericPortFileName[256];
    /// <summary>The background port all splash screens share.</summary>
    static MCLogPort* _GenericPort;
    static int32_t _InstanceCount;

public:
    int32_t NumBlocks = -1;
    /// <summary>Each block: a list of element numbers (0-terminated) to show.</summary>
    uint8_t** Blocks = nullptr;
};

/// <summary>Paints a logistics scroll tab: a filled box with a light top/left and dark bottom/right edge.</summary>
void LogPaintScrollTab(MCGuiObject* tab);

/// <summary>The scroll tab's event routine: dragging it scrolls its <see cref="MCLogScrollTextObject"/>.</summary>
void LogScrollTabHandleEvent(MCGuiObject* tab, MCGuiEvent* event);

/// <summary>Whether <paramref name="session"/> was dropped from the game list (it had no players).</summary>
/// <returns>-1 when it was, else 0.</returns>
int IsSessionDeleted(MCFidpSession* session);

/// <summary>Sessions dropped from the game list because they had no players.</summary>
extern _GUID DeletedSessions[50];
extern int32_t NextDeletedSession;
/// <summary>
/// The placeholder name of an empty save slot (string 0x381, loaded into a 0xff-byte malloc block by the logistics
/// setup; null until then).
/// </summary>
extern char* EmptyFile;
