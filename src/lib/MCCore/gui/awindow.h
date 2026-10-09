#pragma once

#include "gui/MCGuiSystem.h"
#include "gui/MCGuiButton.h"
#include "gui/MCGuiHolderObject.h"

struct MCSmackTag;
class MCGuiFont;

// The window-frame classes of the GUI library: title windows (a title bar, three frame bars and a resize handle
// around the client area), button bars, pop-up menus, the Smacker movie window and the startup (noise) window.

/// <summary>The paint routine of a title bar: its gradient and title text.</summary>
void TitleBarPaint(MCGuiObject* object);
/// <summary>The paint routines of the camera windows' bottom, left and right frame bars.</summary>
void CameraBottomBarPaint(MCGuiObject* object);
void CameraLeftBarPaint(MCGuiObject* object);
void CameraRightBarPaint(MCGuiObject* object);
/// <summary>The paint routines of a title window's bottom, left and right frame bars.</summary>
void BottomBarPaint(MCGuiObject* object);
void LeftBarPaint(MCGuiObject* object);
void RightBarPaint(MCGuiObject* object);
/// <summary>
/// The event routine of a title window's resize handle: a press grabs the mouse, dragging resizes the window (the
/// handle's parent's parent), a release lets go.
/// </summary>
void HandleResizeButtonEvent(MCGuiObject* object, MCGuiEvent* event);
/// <summary>The event routine of the title bar's "swoopy" button: a click flips the camera's flag at +0xe4.</summary>
void HandleSwoopyButtonEvent(MCGuiObject* object, MCGuiEvent* event);

/// <summary>
/// A button on a title bar (close, zoom, swoopy): an aButton whose events go straight to aObject's routine.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.h</c>, 0x4c8 bytes (no fields of its own). Vtable 0x0077b6a8.</remarks>
class MCGuiTitleButton : public MCGuiButton
{
private:
    void HandleEvent(MCGuiEvent* event) override;
};

/// <summary>
/// A window's title bar: the title text, and close, two zoom and a "swoopy" button, each of which posts its message
/// to the window (the bar's parent). Dragging the bar moves the window.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x540 bytes. Vtable 0x0077b574.</remarks>
class MCGuiTitleBar : public MCGuiObject
{
public:
    MCGuiTitleBar();

    /// <summary>Makes the four buttons (the close button's message is 0xd).</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;
    void Draw() override;
    /// <summary>Port: draws itself each frame from its title and buttons.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Drags the window (the parent) while the left button is held on the bar.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>Resizes and moves the buttons to the right edge.</summary>
    void Resize(int32_t newWidth, int32_t newHeight) override;

    /// <summary>Points the two zoom buttons' callbacks at the window with messages 0x1a and 0x1b.</summary>
    void SetZoomCallbacks();
    /// <summary>Copies <paramref name="newTitle"/> into <see cref="Title"/> (unbounded, as the original).</summary>
    void SetTitle(char* newTitle);
    /// <summary>Shows or hides the close button; showing points its callback at the window with message 0xd.</summary>
    void ShowCloseButton(int show);
    void ShowZoomButton(int show);
    void ShowZoomButtons(int show);
    void ShowSwoopyButton(int show);
    /// <summary>Whether <paramref name="newWidth"/> leaves room for the shown buttons (4 pixels, plus each button).</summary>
    int ResizeOK(int32_t newWidth);

    MCGuiFont* Font = nullptr;
    /// <summary>The title text.</summary>
    char Title[128] = {};
    MCGuiCloseButton* CloseButton = nullptr;
    MCGuiButton* ZoomButton = nullptr;
    MCGuiButton* ZoomOutButton = nullptr;
    MCGuiTitleButton* SwoopyButton = nullptr;
};

/// <summary>
/// A framed window: a title bar above (13 pixels), frame bars left, right and below, and a resize handle in the
/// right bar's bottom corner.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x4c0 bytes. Vtable 0x0077b440.</remarks>
class MCGuiTitleWindow : public MCGuiObject
{
public:
    MCGuiTitleWindow();
    /// <summary>Destroys the frame (the original's destructor calls <see cref="Destroy"/>).</summary>
    ~MCGuiTitleWindow() override;

    /// <summary>
    /// Makes the title bar (width + 12, 13 high, above the window), the three frame bars and the resize handle, then
    /// moves the window 13 pixels down to make room for the bar.
    /// </summary>
    /// <returns>0, or the first child's error. Out of memory is fatal.</returns>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;
    void Draw() override;
    /// <summary>
    /// Port: draws itself each frame (and so do its frame parts and title bar). Derived windows that still paint
    /// into a picture outside draw say otherwise.
    /// </summary>
    bool DrawsLive() override { return true; }
    /// <summary>Resizes the window and moves and resizes its frame.</summary>
    void Resize(int32_t newWidth, int32_t newHeight) override;

    void SetTitle(char* newTitle);

    MCGuiObject* LeftBar = nullptr;
    MCGuiObject* BottomBar = nullptr;
    MCGuiObject* RightBar = nullptr;
    /// <summary>The resize handle (a child of the right bar).</summary>
    MCGuiObject* ResizeButton = nullptr;
    MCGuiTitleBar* TitleBar = nullptr;
};

/// <summary>
/// A pop-up menu of up to 25 items (40 characters each), each with a callback, a data value and an optional accelerator
/// letter. An item named <c>"::::"</c> is drawn as a separator.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>, 0x5b0 bytes. Vtable 0x0077b938.</remarks>
class MCGuiMenu : public MCGuiObject
{
public:
    MCGuiMenu();

    /// <summary>Also sets <see cref="Shown"/> when showing.</summary>
    void ShowGuiWindow(bool show) override;
    /// <summary>Allocates the item text (1000 bytes) and clears the items.</summary>
    /// <returns>0, or 0xbadd0001 when out of memory.</returns>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;
    /// <summary>Tracks the highlighted item; a release over an item runs its callback and hides the menu.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    void Draw() override;
    /// <summary>Port: draws itself each frame from its items and the highlighted one.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sizes the menu to its widest item and the item count.</summary>
    void ResizeMenu();
    /// <summary>Appends an item (text cut at 39 characters, in place).</summary>
    /// <returns>The item's index, 0xeeee0001 when the menu is full, 0xeeee0002 when out of memory.</returns>
    int32_t AddItem(char* text);
    int RemoveItem(char* text);
    int RemoveItem(int16_t index);
    /// <summary>Appends a separator (an item named <c>"::::"</c>, with no callback; the menu is not resized).</summary>
    /// <returns>The new item count (not the index), or 0xeeee0001 when the menu is full.</returns>
    int32_t AddSeparator();
    void SetCallback(int16_t index, void (*exec)());
    /// <summary>Makes item <paramref name="index"/> post <paramref name="message"/> to <paramref name="target"/>.</summary>
    void SetMessage(int16_t index, MCGuiObject* target, int32_t message);
    int32_t ChangeItemString(int16_t index, char* text);
    /// <summary>Moves the menu back onto the screen.</summary>
    void KeepOnScreen();
    void SetItemData(int16_t index, int32_t data);
    /// <returns>The item's data, or 0xeeee0003 for a bad index.</returns>
    int32_t GetItemData(int16_t index);
    void SetItemLetter(int16_t index, char letter);
    char GetItemLetter(int16_t index);

    /// <summary>Nonzero to right-align the item text.</summary>
    int32_t RightAligned = 0;
    int32_t NumItems = 0;
    /// <summary>The highlighted item, or -1.</summary>
    int32_t SelectedItem = -1;
    /// <summary>The item text: 25 strings of 40 characters.</summary>
    std::unique_ptr<char[]> ItemText;
    char ItemLetters[25] = {};
    /// <summary>Nonzero once any item has a letter (it widens the menu).</summary>
    int32_t HasLetters = 0;
    int32_t ItemData[25] = {};
    MCGuiFont* Font = nullptr;
    /// <summary>The height of an item: the font's plus 8.</summary>
    int32_t ItemHeight = 0;
    MCGuiCallback* ItemCallbacks[25] = {};
    /// <summary>Set when the menu is shown.</summary>
    int32_t Shown = 0;
};

/// <summary>
/// A title window holding up to 25 tool buttons in rows (or columns) of <see cref="MaxLength"/>; the bar sizes itself
/// to them.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x538 bytes. Vtable 0x0077ba78.</remarks>
class MCGuiToolBar : public MCGuiTitleWindow
{
public:
    MCGuiToolBar();

    /// <summary>A title window without the close button, 8 buttons to a row.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;

    /// <returns>0, or 0xeeee0001 when the bar is full.</returns>
    int32_t AddButton(MCGuiToolButton* button);
    int32_t RemoveButton(int32_t index);
    MCGuiToolButton* GetButton(int32_t index);
    int IsPushed(int32_t index);
    void SetHorizontal(int on);
    void SetMaxLength(int16_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    void PlaceButtons();
    void SetButtonSize(int32_t width, int32_t height);

    int32_t NumButtons = 0;
    /// <summary>Buttons to a row (or column when not horizontal).</summary>
    int16_t MaxLength = 0;
    MCGuiToolButton* Buttons[25] = {};
    int32_t Horizontal = 1;
    int32_t ButtonWidth = 32;
    int32_t ButtonHeight = 32;
};

/// <summary>A frameless bar of up to 25 buttons, laid out like <see cref="MCGuiToolBar"/>.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x524 bytes (fields end there). Vtable 0x0077bbac.</remarks>
class MCGuiWindowBar : public MCGuiObject
{
public:
    MCGuiWindowBar();
    /// <summary>Port: draws itself each frame (only its background; the buttons draw themselves).</summary>
    bool DrawsLive() override { return true; }

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;

    int32_t InsertButton(MCGuiButton* button, int32_t index);
    /// <returns>0, or 0xeeee0001 when the bar is full.</returns>
    int32_t AddButton(MCGuiButton* button);
    int32_t RemoveButton(int32_t index);
    MCGuiButton* GetButton(int32_t index);
    void SetHorizontal(int on);
    void SetMaxLength(int16_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    void PlaceButtons();
    void SetButtonSize(int32_t width, int32_t height);

    int32_t NumButtons = 0;
    int16_t MaxLength = 0;
    MCGuiButton* Buttons[25] = {};
    int32_t Horizontal = 1;
    int32_t ButtonWidth = 32;
    int32_t ButtonHeight = 32;
};

/// <summary>
/// A window playing a Smacker movie, into its own pane or (full screen, in the original) straight to the display.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>, 0x4c0 bytes. Vtable 0x0077bce0.</remarks>
class MCGuiSmackerWindow : public MCGuiObject
{
public:
    MCGuiSmackerWindow();

    /// <summary>Does nothing: the movie sets the size.</summary>
    void Resize(int32_t, int32_t) override {}
    int32_t Init(tagRECT* area, tagPOINT* position);
    /// <summary>
    /// Starts <paramref name="movie"/>: full screen (<paramref name="fullScreen"/> while the game is full screen)
    /// switches the display to 16-bit, otherwise the movie is decoded into a pane of the window's size and its colours
    /// remapped to the game palette.
    /// </summary>
    /// <returns>0, or 0xd4d40000 when out of memory.</returns>
    int32_t StartSmackerMovie(MCSmackTag* movie, int fullScreen);
    void Destroy() override;
    virtual void EndSmackerMovie(); // slot 77
    void CheckSmackerPalette() override;
    /// <summary>Decodes the next frame into <see cref="MoviePane"/> and shows it; the movie's end ends it.</summary>
    void Display() override;
    /// <summary>Port: copies the movie frame in <see cref="MoviePane"/> to the window.</summary>
    void Draw() override;
    /// <summary>Port: the window draws itself each frame (see <see cref="Draw"/>).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Decodes the current frame into the buffer and steps on.</summary>
    /// <returns>0 on the last frame, 1 otherwise.</returns>
    int NextFrame();
    /// <summary>Never hit: returns null.</summary>
    MCGuiObject* FindObject(int32_t xPos, int32_t yPos) override;

    MCSmackTag* Movie = nullptr;
    /// <summary>Set until the first frame: the display clears the pane (or the screen) before decoding it.</summary>
    int32_t FirstFrame = 1;
    /// <summary>
    /// The pane the movie decodes into: at the window's screen position in a buffer reaching from
    /// the screen's corner to the window's. (The original had one for windowed play only.)
    /// </summary>
    MCPane* MoviePane = nullptr;
    int32_t FullScreen = 0;
    /// <summary>Smacker's surface type for the DirectDraw surface (full-screen play).</summary>
    int32_t SurfaceType = 0;
};

/// <summary>
/// The startup window: the three art packets 0x2d..0x2f (static noise frames) played over the screen while the game
/// loads.
/// </summary>
/// <remarks>
/// Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>; fields end at 0x4c8. Vtable 0x0077be18. The
/// constructor also fills the table of rectangles at 0x007ab230 (<see cref="StartupRects"/>).
/// </remarks>
class MCGuiStartupWindow : public MCGuiObject
{
public:
    MCGuiStartupWindow();

    void Resize(int32_t, int32_t) override {}
    /// <summary>Sets <see cref="StartupState"/> and restarts the frame count.</summary>
    void SetState(int32_t newState) override
    {
        FrameCount = 0;
        StartupState = newState;
    }

    void Destroy() override;
    void DoStatic();
    void EndStatic();
    /// <summary>Runs the next step of the sequence (<see cref="Step"/>) and shows the picture.</summary>
    void Display() override;
    /// <summary>Port: copies <see cref="StaticPort"/> to the window.</summary>
    void Draw() override;
    /// <summary>Port: the window draws itself each frame (see <see cref="Draw"/>).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Loads art packets 0x2d, 0x2e and 0x2f and picks a random start (0..11).</summary>
    /// <returns>0, -1 when out of memory, or the packet file's error.</returns>
    int32_t Setup();

    /// <summary>
    /// The art packets 0x2d (the map), 0x2e and 0x2f (noise frames), as shape tables, from the CRT heap.
    /// </summary>
    uint8_t* StaticImages[3] = {};
    /// <summary>The display step: bumped once per <see cref="Display"/>, it drives the whole sequence.</summary>
    int32_t StartupState = 0;
    int32_t FrameCount = 0;
    /// <summary>The typed text's width so far on the current line.</summary>
    int32_t TextX = 0;
    /// <summary>The uplink's end point: an index 0..11 into <see cref="StartupRects"/>, picked by <see cref="Setup"/>.</summary>
    int32_t RandomStart = 0;

    /// <summary>
    /// Port: the picture the sequence builds up (made by <see cref="Setup"/>). The original drew it on the screen,
    /// and the noise copies rows of what is already there, so the picture is the window's state; like a movie frame,
    /// it is copied to the screen each frame.
    /// </summary>
    MCGuiPort* StaticPort = nullptr;

private:
    /// <summary>Port: one step of the sequence: its sound and its drawing into <see cref="StaticPort"/> (the body of the original's <see cref="Display"/>).</summary>
    void Step();
    /// <summary>Port: <see cref="StaticPort"/>'s pane, where the sequence draws.</summary>
    MCPane* StaticPane();
};

/// <summary>A window with a title bar and frame whose client area is an aHolderObject's two panes.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x4d4 bytes. Vtable 0x0077bf4c.</remarks>
class MCGuiEmptyTitleWindow : public MCGuiHolderObject
{
public:
    MCGuiEmptyTitleWindow();
    /// <summary>Destroys the frame (the original's destructor calls <see cref="Destroy"/>).</summary>
    ~MCGuiEmptyTitleWindow() override;

    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) override;
    void Destroy() override;
    void Resize(int32_t newWidth, int32_t newHeight) override;
    void HandleEvent(MCGuiEvent* event) override;
    void SetBackColor(int32_t color) override;

    void SetTitle(char* newTitle);

    MCGuiObject* LeftBar = nullptr;
    MCGuiObject* BottomBar = nullptr;
    MCGuiObject* RightBar = nullptr;
    MCGuiObject* ResizeButton = nullptr;
    MCGuiTitleBar* TitleBar = nullptr;
};

/// <summary>
/// The startup window's 12 uplink end points, (x, y) pairs (24 ints at 0x007ab230), filled by its constructor.
/// </summary>
extern int32_t StartupRects[24];
/// <summary>The sample the startup window's noise plays.</summary>
extern int32_t NoiseSample;
/// <summary>Set when a Smacker movie has played to its end (or been ended).</summary>
extern int MovieOver;
