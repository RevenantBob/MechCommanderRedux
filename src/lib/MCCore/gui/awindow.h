#pragma once

#include "gui/asystem.h"
#include "gui/abutton.h"

struct SmackTag;
class aFont;

// The window-frame classes of the GUI library: title windows (a title bar, three frame bars and a resize handle
// around the client area), button bars, pop-up menus, the Smacker movie window and the startup (noise) window.

/// <summary>The paint routine of a title bar: its gradient and title text.</summary>
/// <remarks>MCX.EXE @ 0x00618a10</remarks>
void titleBarPaint(aObject* object);
/// <summary>The paint routines of the camera windows' bottom, left and right frame bars.</summary>
/// <remarks>MCX.EXE @ 0x00618a90</remarks>
void cameraBottomBarPaint(aObject* object);
/// <remarks>MCX.EXE @ 0x00618b10</remarks>
void cameraLeftBarPaint(aObject* object);
/// <remarks>MCX.EXE @ 0x00618b80</remarks>
void cameraRightBarPaint(aObject* object);
/// <summary>The paint routines of a title window's bottom, left and right frame bars.</summary>
/// <remarks>MCX.EXE @ 0x00618bb0</remarks>
void bottomBarPaint(aObject* object);
/// <remarks>MCX.EXE @ 0x00618c40</remarks>
void leftBarPaint(aObject* object);
/// <remarks>MCX.EXE @ 0x00618cc0</remarks>
void rightBarPaint(aObject* object);
/// <summary>
/// The event routine of a title window's resize handle: a press grabs the mouse, dragging resizes the window (the
/// handle's parent's parent), a release lets go.
/// </summary>
/// <remarks>MCX.EXE @ 0x006188c0</remarks>
void handleResizeButtonEvent(aObject* object, aEvent* event);
/// <summary>The event routine of the title bar's "swoopy" button: a click flips the camera's flag at +0xe4.</summary>
/// <remarks>MCX.EXE @ 0x00618980</remarks>
void handleSwoopyButtonEvent(aObject* object, aEvent* event);

/// <summary>
/// A button on a title bar (close, zoom, swoopy): an aButton whose events go straight to aObject's routine.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.h</c>, 0x4c8 bytes (no fields of its own). Vtable 0x0077b6a8.</remarks>
class aTitleButton : public aButton
{
private:
    /// <remarks>MCX.EXE @ 0x00619690 (gui\awindow.h)</remarks>
    void handleEvent(aEvent* event) override;
};

/// <summary>
/// A window's title bar: the title text, and close, two zoom and a "swoopy" button, each of which posts its message
/// to the window (the bar's parent). Dragging the bar moves the window.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x540 bytes. Vtable 0x0077b574.</remarks>
class aTitleBar : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x00619380</remarks>
    aTitleBar();

    /// <summary>Makes the four buttons (the close button's message is 0xd).</summary>
    /// <remarks>MCX.EXE @ 0x006193f0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x00619700</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00619cb0</remarks>
    void draw() override;
    /// <summary>Port: draws itself each frame from its title and buttons.</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Drags the window (the parent) while the left button is held on the bar.</summary>
    /// <remarks>MCX.EXE @ 0x00619830</remarks>
    void handleEvent(aEvent* event) override;
    /// <summary>Resizes and moves the buttons to the right edge.</summary>
    /// <remarks>MCX.EXE @ 0x00619eb0</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;

    /// <summary>Points the two zoom buttons' callbacks at the window with messages 0x1a and 0x1b.</summary>
    /// <remarks>MCX.EXE @ 0x006197a0</remarks>
    void SetZoomCallbacks();
    /// <summary>Copies <paramref name="newTitle"/> into <see cref="title"/> (unbounded, as the original).</summary>
    /// <remarks>MCX.EXE @ 0x006197f0</remarks>
    void setTitle(char* newTitle);
    /// <summary>Shows or hides the close button; showing points its callback at the window with message 0xd.</summary>
    /// <remarks>MCX.EXE @ 0x00619de0</remarks>
    void showCloseButton(int show);
    /// <remarks>MCX.EXE @ 0x00619e30</remarks>
    void showZoomButton(int show);
    /// <remarks>MCX.EXE @ 0x00619e50</remarks>
    void showZoomButtons(int show);
    /// <remarks>MCX.EXE @ 0x00619e90</remarks>
    void showSwoopyButton(int show);
    /// <summary>Whether <paramref name="newWidth"/> leaves room for the shown buttons (4 pixels, plus each button).</summary>
    /// <remarks>MCX.EXE @ 0x00619f20</remarks>
    int ResizeOK(int32_t newWidth);

    aFont* font = nullptr; // +0x4ac
    /// <summary>The title text.</summary>
    char title[128] = {};                 // +0x4b0
    aCloseButton* closeButton = nullptr;  // +0x530
    aButton* zoomButton = nullptr;        // +0x534
    aButton* zoomOutButton = nullptr;     // +0x538
    aTitleButton* swoopyButton = nullptr; // +0x53c
};

/// <summary>
/// A framed window: a title bar above (13 pixels), frame bars left, right and below, and a resize handle in the
/// right bar's bottom corner.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x4c0 bytes. Vtable 0x0077b440.</remarks>
class aTitleWindow : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x00618d60</remarks>
    aTitleWindow();
    /// <summary>Destroys the frame (the original's destructor calls <see cref="destroy"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00618da0 (vector deleting destructor)</remarks>
    ~aTitleWindow() override;

    /// <summary>
    /// Makes the title bar (width + 12, 13 high, above the window), the three frame bars and the resize handle, then
    /// moves the window 13 pixels down to make room for the bar.
    /// </summary>
    /// <returns>0, or the first child's error. Out of memory is fatal.</returns>
    /// <remarks>MCX.EXE @ 0x00618dd0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x006190b0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00619180</remarks>
    void draw() override;
    /// <summary>
    /// Port: draws itself each frame (and so do its frame parts and title bar). Derived windows that still paint
    /// into a picture outside draw say otherwise.
    /// </summary>
    bool DrawsLive() override { return true; }
    /// <summary>Resizes the window and moves and resizes its frame.</summary>
    /// <remarks>MCX.EXE @ 0x00619210</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;

    /// <remarks>MCX.EXE @ 0x00619360</remarks>
    void setTitle(char* newTitle);

    aObject* leftBar = nullptr;   // +0x4ac
    aObject* bottomBar = nullptr; // +0x4b0
    aObject* rightBar = nullptr;  // +0x4b4
    /// <summary>The resize handle (a child of the right bar).</summary>
    aObject* resizeButton = nullptr; // +0x4b8
    aTitleBar* titleBar = nullptr;   // +0x4bc
};

/// <summary>
/// A pop-up menu of up to 25 items (40 characters each), each with a callback, a data value and an optional accelerator
/// letter. An item named <c>"::::"</c> is drawn as a separator.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>, 0x5b0 bytes. Vtable 0x0077b938.</remarks>
class aMenu : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x00619fb0</remarks>
    aMenu();

    /// <summary>Also sets <see cref="shown"/> when showing.</summary>
    /// <remarks>MCX.EXE @ 0x00619ff0 (gui\awindow.h)</remarks>
    void ShowGUIWindow(int show) override;
    /// <summary>Allocates the item text (1000 bytes from the GUI heap) and clears the items.</summary>
    /// <returns>0, or 0xbadd0001 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0061a040</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0061a130</remarks>
    void destroy() override;
    /// <summary>Tracks the highlighted item; a release over an item runs its callback and hides the menu.</summary>
    /// <remarks>MCX.EXE @ 0x0061a1c0</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x0061a2a0</remarks>
    void draw() override;
    /// <summary>Port: draws itself each frame from its items and the highlighted one.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>Sizes the menu to its widest item and the item count.</summary>
    /// <remarks>MCX.EXE @ 0x0061a580</remarks>
    void ResizeMenu();
    /// <summary>Appends an item (text cut at 39 characters, in place).</summary>
    /// <returns>The item's index, 0xeeee0001 when the menu is full, 0xeeee0002 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0061a660</remarks>
    int32_t AddItem(char* text);
    /// <remarks>MCX.EXE @ 0x0061a730</remarks>
    int RemoveItem(char* text);
    /// <remarks>MCX.EXE @ 0x0061a780</remarks>
    int RemoveItem(int16_t index);
    /// <summary>Appends a separator (an item named <c>"::::"</c>, with no callback; the menu is not resized).</summary>
    /// <returns>The new item count (not the index), or 0xeeee0001 when the menu is full.</returns>
    /// <remarks>MCX.EXE @ 0x0061a860</remarks>
    int32_t AddSeparator();
    /// <remarks>MCX.EXE @ 0x0061a8c0</remarks>
    void SetCallback(int16_t index, void (*exec)());
    /// <summary>Makes item <paramref name="index"/> post <paramref name="message"/> to <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0061a8f0</remarks>
    void SetMessage(int16_t index, aObject* target, int32_t message);
    /// <remarks>MCX.EXE @ 0x0061a920</remarks>
    int32_t ChangeItemString(int16_t index, char* text);
    /// <summary>Moves the menu back onto the screen.</summary>
    /// <remarks>MCX.EXE @ 0x0061a9a0</remarks>
    void KeepOnScreen();
    /// <remarks>MCX.EXE @ 0x0061aa80</remarks>
    void SetItemData(int16_t index, int32_t data);
    /// <returns>The item's data, or 0xeeee0003 for a bad index.</returns>
    /// <remarks>MCX.EXE @ 0x0061aaa0</remarks>
    int32_t GetItemData(int16_t index);
    /// <remarks>MCX.EXE @ 0x0061aad0</remarks>
    void SetItemLetter(int16_t index, char letter);
    /// <remarks>MCX.EXE @ 0x0061ab00</remarks>
    char GetItemLetter(int16_t index);

    /// <summary>Nonzero to right-align the item text.</summary>
    int32_t rightAligned = 0; // +0x4ac
    int32_t numItems = 0;     // +0x4b0
    /// <summary>The highlighted item, or -1.</summary>
    int32_t selectedItem = -1; // +0x4b4
    /// <summary>The item text: 25 strings of 40 characters, from the GUI heap.</summary>
    char* itemText = nullptr;  // +0x4b8
    char itemLetters[25] = {}; // +0x4bc
    /// <summary>Nonzero once any item has a letter (it widens the menu).</summary>
    int32_t hasLetters = 0;    // +0x4d8
    int32_t itemData[25] = {}; // +0x4dc
    aFont* font = nullptr;     // +0x540
    /// <summary>The height of an item: the font's plus 8.</summary>
    int32_t itemHeight = 0;            // +0x544
    aCallback* itemCallbacks[25] = {}; // +0x548
    /// <summary>Set when the menu is shown.</summary>
    int32_t shown = 0; // +0x5ac
};

/// <summary>
/// A title window holding up to 25 tool buttons in rows (or columns) of <see cref="maxLength"/>; the bar sizes itself
/// to them.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x538 bytes. Vtable 0x0077ba78.</remarks>
class aToolBar : public aTitleWindow
{
public:
    /// <remarks>MCX.EXE @ 0x0061ab20</remarks>
    aToolBar();

    /// <summary>A title window without the close button, 8 buttons to a row.</summary>
    /// <remarks>MCX.EXE @ 0x0061aba0</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0061ac00</remarks>
    void destroy() override;

    /// <returns>0, or 0xeeee0001 when the bar is full.</returns>
    /// <remarks>MCX.EXE @ 0x0061ac70</remarks>
    int32_t AddButton(aToolButton* button);
    /// <remarks>MCX.EXE @ 0x0061acc0</remarks>
    int32_t RemoveButton(int32_t index);
    /// <remarks>MCX.EXE @ 0x0061ad50</remarks>
    aToolButton* GetButton(int32_t index);
    /// <remarks>MCX.EXE @ 0x0061ad70</remarks>
    int IsPushed(int32_t index);
    /// <remarks>MCX.EXE @ 0x0061ada0</remarks>
    void SetHorizontal(int on);
    /// <remarks>MCX.EXE @ 0x0061adc0</remarks>
    void SetMaxLength(int16_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    /// <remarks>MCX.EXE @ 0x0061ade0 (unnamed in MCX.EXE; the same code as aWindowBar::PlaceButtons)</remarks>
    void PlaceButtons();
    /// <remarks>MCX.EXE @ 0x0061aee0</remarks>
    void SetButtonSize(int32_t width, int32_t height);

    int32_t numButtons = 0; // +0x4c0
    /// <summary>Buttons to a row (or column when not horizontal).</summary>
    int16_t maxLength = 0;         // +0x4c4
    aToolButton* buttons[25] = {}; // +0x4c8
    int32_t horizontal = 1;        // +0x52c
    int32_t buttonWidth = 32;      // +0x530
    int32_t buttonHeight = 32;     // +0x534
};

/// <summary>A frameless bar of up to 25 buttons, laid out like <see cref="aToolBar"/>.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x524 bytes (fields end there). Vtable 0x0077bbac.</remarks>
class aWindowBar : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0061af00</remarks>
    aWindowBar();
    /// <summary>Port: draws itself each frame (only its background; the buttons draw themselves).</summary>
    bool DrawsLive() override { return true; }

    /// <remarks>MCX.EXE @ 0x0061af80</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0061afd0</remarks>
    void destroy() override;

    /// <remarks>MCX.EXE @ 0x0061b040</remarks>
    int32_t InsertButton(aButton* button, int32_t index);
    /// <returns>0, or 0xeeee0001 when the bar is full.</returns>
    /// <remarks>MCX.EXE @ 0x0061b0b0</remarks>
    int32_t AddButton(aButton* button);
    /// <remarks>MCX.EXE @ 0x0061b100</remarks>
    int32_t RemoveButton(int32_t index);
    /// <remarks>MCX.EXE @ 0x0061b190</remarks>
    aButton* GetButton(int32_t index);
    /// <remarks>MCX.EXE @ 0x0061b1b0</remarks>
    void SetHorizontal(int on);
    /// <remarks>MCX.EXE @ 0x0061b1d0</remarks>
    void SetMaxLength(int16_t length);
    /// <summary>Sizes the bar to the buttons and lays them out.</summary>
    /// <remarks>MCX.EXE @ 0x0061b1f0</remarks>
    void PlaceButtons();
    /// <remarks>MCX.EXE @ 0x0061b2f0</remarks>
    void SetButtonSize(int32_t width, int32_t height);

    int32_t numButtons = 0;    // +0x4ac
    int16_t maxLength = 0;     // +0x4b0
    aButton* buttons[25] = {}; // +0x4b4
    int32_t horizontal = 1;    // +0x518
    int32_t buttonWidth = 32;  // +0x51c
    int32_t buttonHeight = 32; // +0x520
};

/// <summary>
/// A window playing a Smacker movie, into its own pane or (full screen, in the original) straight to the display.
/// </summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>, 0x4c0 bytes. Vtable 0x0077bce0.</remarks>
class aSmackerWindow : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0061b310</remarks>
    aSmackerWindow();

    /// <summary>Does nothing: the movie sets the size.</summary>
    /// <remarks>MCX.EXE @ 0x0061b340 (gui\awindow.h)</remarks>
    void resize(int32_t, int32_t) override {}
    /// <remarks>MCX.EXE @ 0x0061b380</remarks>
    int32_t init(tagRECT* area, tagPOINT* position);
    /// <summary>
    /// Starts <paramref name="movie"/>: full screen (<paramref name="fullScreen"/> while the game is full screen)
    /// switches the display to 16-bit, otherwise the movie is decoded into a pane of the window's size and its colours
    /// remapped to the game palette.
    /// </summary>
    /// <returns>0, or 0xd4d40000 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0061b3c0</remarks>
    int32_t startSmackerMovie(SmackTag* movie, int fullScreen);
    /// <remarks>MCX.EXE @ 0x0061b550</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0061b5e0</remarks>
    virtual void endSmackerMovie(); // slot 77
    /// <remarks>MCX.EXE @ 0x0061b610</remarks>
    void checkSmackerPalette() override;
    /// <summary>Decodes the next frame into <see cref="moviePane"/> and shows it; the movie's end ends it.</summary>
    /// <remarks>MCX.EXE @ 0x0061b690</remarks>
    void display() override;
    /// <summary>Port: copies the movie frame in <see cref="moviePane"/> to the window.</summary>
    /// <remarks>MCX.EXE @ 0x0061b8c0</remarks>
    void draw() override;
    /// <summary>Port: the window draws itself each frame (see <see cref="draw"/>).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Decodes the current frame into the buffer and steps on.</summary>
    /// <returns>0 on the last frame, 1 otherwise.</returns>
    /// <remarks>MCX.EXE @ 0x0061b8d0 (unnamed in MCX.EXE)</remarks>
    int nextFrame();
    /// <summary>Never hit: returns null.</summary>
    /// <remarks>MCX.EXE @ 0x0061b910</remarks>
    aObject* findObject(int32_t xPos, int32_t yPos) override;

    SmackTag* movie = nullptr; // +0x4ac
    /// <summary>Set until the first frame: the display clears the pane (or the screen) before decoding it.</summary>
    int32_t firstFrame = 1; // +0x4b0
    /// <summary>
    /// The pane the movie decodes into, from the GUI heap: at the window's screen position in a buffer reaching from
    /// the screen's corner to the window's. (The original had one for windowed play only.)
    /// </summary>
    _pane* moviePane = nullptr; // +0x4b4
    int32_t fullScreen = 0;     // +0x4b8
    /// <summary>Smacker's surface type for the DirectDraw surface (full-screen play).</summary>
    int32_t surfaceType = 0; // +0x4bc
};

/// <summary>
/// The startup window: the three art packets 0x2d..0x2f (static noise frames) played over the screen while the game
/// loads.
/// </summary>
/// <remarks>
/// Original source: <c>gui\awindow.cpp</c> and <c>gui\awindow.h</c>; fields end at 0x4c8. Vtable 0x0077be18. The
/// constructor also fills the table of rectangles at 0x007ab230 (<see cref="startupRects"/>).
/// </remarks>
class aStartupWindow : public aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0061b940</remarks>
    aStartupWindow();

    /// <remarks>MCX.EXE @ 0x0061ba40 (gui\awindow.h)</remarks>
    void resize(int32_t, int32_t) override {}
    /// <summary>Sets <see cref="startupState"/> and restarts the frame count.</summary>
    /// <remarks>MCX.EXE @ 0x0061ba50 (gui\awindow.h)</remarks>
    void setState(int32_t newState) override
    {
        frameCount = 0;
        startupState = newState;
    }

    /// <remarks>MCX.EXE @ 0x0061baa0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0061bb00</remarks>
    void doStatic();
    /// <remarks>MCX.EXE @ 0x0061bbf0</remarks>
    void endStatic();
    /// <summary>Runs the next step of the sequence (<see cref="Step"/>) and shows the picture.</summary>
    /// <remarks>MCX.EXE @ 0x0061bc10</remarks>
    void display() override;
    /// <summary>Port: copies <see cref="staticPort"/> to the window.</summary>
    /// <remarks>MCX.EXE @ 0x0061d490</remarks>
    void draw() override;
    /// <summary>Port: the window draws itself each frame (see <see cref="draw"/>).</summary>
    bool DrawsLive() override { return true; }
    /// <summary>Loads art packets 0x2d, 0x2e and 0x2f and picks a random start (0..11).</summary>
    /// <returns>0, -1 when out of memory, or the packet file's error.</returns>
    /// <remarks>MCX.EXE @ 0x0061d4a0</remarks>
    int32_t setup();

    /// <summary>
    /// The art packets 0x2d (the map), 0x2e and 0x2f (noise frames), as shape tables, from the CRT heap.
    /// </summary>
    uint8_t* staticImages[3] = {}; // +0x4ac
    /// <summary>The display step: bumped once per <see cref="display"/>, it drives the whole sequence.</summary>
    int32_t startupState = 0; // +0x4b8
    int32_t frameCount = 0;   // +0x4bc
    /// <summary>The typed text's width so far on the current line.</summary>
    int32_t textX = 0; // +0x4c0
    /// <summary>The uplink's end point: an index 0..11 into <see cref="startupRects"/>, picked by <see cref="setup"/>.</summary>
    int32_t randomStart = 0; // +0x4c4

    /// <summary>
    /// Port: the picture the sequence builds up (made by <see cref="setup"/>). The original drew it on the screen,
    /// and the noise copies rows of what is already there, so the picture is the window's state; like a movie frame,
    /// it is copied to the screen each frame.
    /// </summary>
    aPort* staticPort = nullptr;

private:
    /// <summary>Port: one step of the sequence: its sound and its drawing into <see cref="staticPort"/> (the body of the original's <see cref="display"/>).</summary>
    void Step();
    /// <summary>Port: <see cref="staticPort"/>'s pane, where the sequence draws.</summary>
    _pane* StaticPane();
};

/// <summary>A window with a title bar and frame whose client area is an aHolderObject's two panes.</summary>
/// <remarks>Original source: <c>gui\awindow.cpp</c>, 0x4d4 bytes. Vtable 0x0077bf4c.</remarks>
class aEmptyTitleWindow : public aHolderObject
{
public:
    /// <remarks>MCX.EXE @ 0x0061d670</remarks>
    aEmptyTitleWindow();
    /// <summary>Destroys the frame (the original's destructor calls <see cref="destroy"/>).</summary>
    ~aEmptyTitleWindow() override;

    /// <remarks>MCX.EXE @ 0x0061d830</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x0061db30</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0061dc00</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;
    /// <remarks>MCX.EXE @ 0x0061dd70</remarks>
    void handleEvent(aEvent* event) override;
    /// <remarks>MCX.EXE @ 0x0061deb0</remarks>
    void setBackColor(int32_t color) override;

    /// <remarks>MCX.EXE @ 0x0061de90</remarks>
    void setTitle(char* newTitle);

    aObject* leftBar = nullptr;      // +0x4c0
    aObject* bottomBar = nullptr;    // +0x4c4
    aObject* rightBar = nullptr;     // +0x4c8
    aObject* resizeButton = nullptr; // +0x4cc
    aTitleBar* titleBar = nullptr;   // +0x4d0
};

/// <summary>
/// The startup window's 12 uplink end points, (x, y) pairs (24 ints at 0x007ab230), filled by its constructor.
/// </summary>
extern int32_t startupRects[24];
/// <summary>The sample the startup window's noise plays.</summary>
extern int32_t noiseSample;
/// <summary>Set when a Smacker movie has played to its end (or been ended).</summary>
extern int movieOver;
