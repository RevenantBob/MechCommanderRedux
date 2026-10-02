#pragma once

#include "gui/asystem.h"
#include "gui/scrlpane.h"
#include "logistics/lport.h"

class aFont;
class FitIniFile;
class FIDPSession;

/// <summary>A button's callback on the logistics screens: an <see cref="aCallback"/> on the logistics heap.</summary>
/// <remarks>Original source: <c>logistics\loggen.cpp</c>, 0x10 bytes (no fields of its own).</remarks>
class lCallback : public aCallback
{
public:
    /// <remarks>MCX.EXE @ 0x006e4b30</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006e4b50</remarks>
    static void operator delete(void* ptr);
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

    /// <summary>Draws the picture for the button's state (gray, pressed, over, up) or wipes to the back color.</summary>
    /// <remarks>MCX.EXE @ 0x006e49d0</remarks>
    void draw() override;

    /// <summary>
    /// On a click: plays the press sound and runs the callback (or the "disabled" sound when grayed), then passes the
    /// event to the event routine.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e4930</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>The mouse came over the button: highlights it and plays the enter sound.</summary>
    /// <remarks>MCX.EXE @ 0x006e4af0</remarks>
    void enter() override;

    /// <summary>The mouse left the button: drops the highlight.</summary>
    /// <remarks>MCX.EXE @ 0x006e0690</remarks>
    void leave() override
    {
        if (overState)
        {
            overState = 0;
            draw();
        }
    }

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

    /// <summary>Nonzero for one draw after a click: shows the down picture.</summary>
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

    /// <summary>Wipes the port and writes the text.</summary>
    /// <remarks>MCX.EXE @ 0x006e1460</remarks>
    void draw() override;

    /// <summary>Draws the cursor (when it has a valid position), then displays as an <see cref="lObject"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006e14b0</remarks>
    void display() override;

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
    /// <summary>Set to -1 by the constructor; not read in loggen.cpp.</summary>
    int32_t unknown4D4 = -1; // +0x4d4
    /// <summary>Cleared by the constructor; not read in loggen.cpp.</summary>
    int32_t unknown4D8 = 0; // +0x4d8
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

    /// <summary>Copies the visible part of the port (scrolled by <see cref="firstPixel"/>) to the screen.</summary>
    /// <remarks>MCX.EXE @ 0x006e6990</remarks>
    void display() override;

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
    bool MouseWheel(int32_t steps) override;

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
    /// <summary>Not accessed in loggen.cpp.</summary>
    int32_t unknown4E4 = 0; // +0x4e4
    /// <summary>Not accessed in loggen.cpp.</summary>
    int32_t unknown4E8 = 0; // +0x4e8
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

    /// <summary>Rebuilds the lines from the session list ("name  players" or "name FULL").</summary>
    /// <remarks>MCX.EXE @ 0x006e7160</remarks>
    void draw() override;

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

    /// <remarks>MCX.EXE @ 0x006e1fe0</remarks>
    void draw() override;

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

    /// <summary>Draws the file lines into the scroll port.</summary>
    /// <remarks>MCX.EXE @ 0x006e28d0</remarks>
    void drawFiles();

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
    lObject* columnHeaders[3] = {}; // +0x510
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
    /// <summary>Not accessed in loggen.cpp.</summary>
    int32_t unknown53C = 0; // +0x53c
    /// <summary>Not accessed in loggen.cpp.</summary>
    int32_t unknown540 = 0; // +0x540
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

    /// <summary>
    /// Shows or hides the screen; showing the load/save screen grays the buttons that have nothing to act on.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006e3540 (name inferred from the vtable slot)</remarks>
    void ShowGUIWindow(int show) override;

    /// <summary>The elements; element 0 is the screen itself.</summary>
    aObject** elements = nullptr; // +0x4bc
    int32_t numElements = 0;      // +0x4c0
    /// <summary>Set to -1 by <see cref="destroy"/>; not read in loggen.cpp.</summary>
    int32_t unknown4C4 = 0; // +0x4c4
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
