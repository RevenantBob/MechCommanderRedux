#pragma once

// The GUI system (gui\asystem.cpp, gui\asystem.h): aObject, the window class every screen element derives from; the
// application object aSystem that owns the screen, the palette, the callbacks and the timers; events, callbacks,
// timers, the two-pane holder window and the message box. Also the program's startup (RealWinMain) and its message
// translation, which live in the same original file.
//
// Every aObject is a rectangle drawn into an aPort (an 8-bit bitmap) and copied to the screen through its pane. The
// objects form a tree (up to 255 children each) and the vtable below is the one every GUI and logistics class
// extends: 77 slots in aObject, more in aButton (82), aHolderObject (90), ScrollPane (78), ... Other code calls them
// as `(**(code **)(*(int *)obj + 0xNN))`: slot = 0xNN / 4, and each virtual below carries its slot.

#include "logistics/smuti.h"

class aAnimation;
class aCallback;
class aFont;
class aObject;
class aOpeningSmackerWindow;
class aPort;
class aTimerManager;
class Camera;
class FIDPMessage;
class Font;
class PacketFile;
class UserHeap;
class HeapList;

/// <summary>The side an <see cref="aObject"/> slides out to when it hides (<see cref="aObject::HideMe"/>).</summary>
/// <remarks>The original's enumerator names were lost; the values are the original's (stored as a byte at +0x64).</remarks>
enum DIRECTION : int32_t
{
    DIRECTION_LEFT = 0,
    DIRECTION_UP = 1,
    DIRECTION_RIGHT = 2,
    DIRECTION_DOWN = 3
};

/// <summary>
/// The mouse cursor shapes (<see cref="aSystem::SetCurrentCursor"/>). The enumerator names were lost; the values
/// run 0..0x12 (0xf..0x11 are offset by the interface's cursor set, 0x12 maps to shape 1).
/// </summary>
enum CursorType : int32_t;

/// <summary>A window's display state (<see cref="aObject::state"/>).</summary>
/// <remarks>The values are the original's (<c>normalize</c> = 0, <c>maximize</c> = 1, <c>iconize</c> = 2).</remarks>
enum aWindowState : int32_t
{
    aSTATE_NORMAL = 0,
    aSTATE_MAXIMIZED = 1,
    aSTATE_ICONIZED = 2
};

/// <summary>
/// An input or system event passed to <see cref="aObject::handleEvent"/>. <c>translateMessage</c> builds them from
/// window messages, <c>aPostMessage</c> and the timers from a bare type.
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0x28 bytes (the size of the stack copies). Types seen: 1 left button
/// down, 3 left button down again (grab), 4 left button up, 6 right button up, 7 mouse move, 8 key up, 9 key down,
/// 10 character, 0xc paint, 0xd close (destroys the object), 0x10 left double click, 0x11 right double click, 0x12
/// broadcast (passed to every child), 0x13 timer (<see cref="data"/> = the timer id), 0x15/0x16 spinner up/down
/// messages; values from 0x1400 (WM_USER + 0x1000) are posted messages.
/// </remarks>
class aEvent
{
public:
    /// <summary>Zeroes every field but <see cref="data"/> and <see cref="lParam"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006158c0</remarks>
    void clear();

    /// <summary>What happened (see the class remarks).</summary>
    int32_t type = 0; // +0x00
    /// <summary>The object the event is aimed at (the one under the cursor), when the sender set it.</summary>
    aObject* target = nullptr; // +0x04
    /// <summary>Left mouse button held (MK_LBUTTON).</summary>
    uint8_t leftButton = 0; // +0x08
    /// <summary>Middle mouse button held (MK_MBUTTON).</summary>
    uint8_t middleButton = 0; // +0x09
    /// <summary>Right mouse button held (MK_RBUTTON).</summary>
    uint8_t rightButton = 0; // +0x0a
    /// <summary>Alt held.</summary>
    uint8_t altKey = 0; // +0x0b
    /// <summary>Ctrl held.</summary>
    uint8_t ctrlKey = 0; // +0x0c
    /// <summary>Shift held.</summary>
    uint8_t shiftKey = 0; // +0x0d
    /// <summary>The character or virtual-key code of a key event (the message's wParam, low byte).</summary>
    uint8_t key = 0; // +0x0e
    /// <summary>The key's scan code (bits 16..24 of the message's lParam).</summary>
    int16_t scanCode = 0; // +0x10
    /// <summary>The cursor position, in screen-window coordinates.</summary>
    int32_t x = 0; // +0x14
    int32_t y = 0; // +0x18
    /// <summary>The message's wParam; the timer id of a timer event.</summary>
    int32_t data = 0; // +0x1c
    /// <summary>The message's lParam.</summary>
    int32_t lParam = 0; // +0x20
    /// <summary>Cleared with the rest; the logistics blocks store a float here.</summary>
    int32_t unknown24 = 0; // +0x24
};

/// <summary>
/// Something to do when a button is pressed or a callback fires: a plain function, and/or a message posted to an
/// object (<c>aPostMessage(object, message)</c>).
/// </summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x10 bytes. Allocated from the GUI heap.</remarks>
class aCallback
{
public:
    /// <remarks>MCX.EXE @ 0x006157c0</remarks>
    aCallback();
    /// <remarks>MCX.EXE @ 0x006157e0</remarks>
    virtual ~aCallback();

    /// <remarks>MCX.EXE @ 0x00615770</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006157a0</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Runs the callback: calls <see cref="exec"/>, then posts <see cref="message"/> to <see cref="object"/> when
    /// both are set.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00615830 (vtable slot 0; unnamed in the symbols, it is the function every caller runs).</remarks>
    virtual void execute(); // slot 0

    /// <summary>Makes the callback post <paramref name="msg"/> to <paramref name="obj"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006158a0</remarks>
    virtual void setMessage(aObject* obj, int32_t msg); // slot 1

    /// <summary>Clears the function and the message.</summary>
    /// <remarks>MCX.EXE @ 0x00615800</remarks>
    void destroy();

    /// <summary>Sets the function to call.</summary>
    /// <remarks>MCX.EXE @ 0x00615880</remarks>
    void setExec(void (*func)());

    /// <summary>The function to call, or null.</summary>
    void (*exec)() = nullptr; // +0x04
    /// <summary>The message to post, or 0.</summary>
    int32_t message = 0; // +0x08
    /// <summary>The object to post it to, or null.</summary>
    aObject* object = nullptr; // +0x0c
};

/// <summary>
/// The base of every GUI element: a rectangle at (x, y) relative to its parent, with a port it draws into, a pane
/// on the screen it is copied to, an optional background picture, icon and animation, up to 255 children, and
/// optional paint and event routines. It can be maximized, iconized, dragged, and slid off the screen (hidden).
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> and <c>gui\asystem.h</c>, 0x4ac bytes. Allocated from the GUI heap.
/// Vtable 0x0077add4, 77 slots, declared below in slot order (<c>// slot N</c>). A few virtuals were unnamed in
/// MCX.EXE's symbols; their names here (<see cref="width"/>, <see cref="bringToFront"/>, <see cref="enter"/>,
/// <see cref="ShowGUIWindow"/>, <see cref="right"/>) come from what they do and from the derived classes'
/// overrides (aFloatHelp::enter, aMenu::ShowGUIWindow).
/// </remarks>
class aObject
{
public:
    /// <remarks>MCX.EXE @ 0x0060e440</remarks>
    aObject();
    /// <summary>Calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0060e4d0 (vector deleting destructor 0x0060e4a0)</remarks>
    virtual ~aObject(); // slot 0
    aObject(const aObject&) = delete;
    aObject& operator=(const aObject&) = delete;

    /// <summary>Allocates from <c>guiHeap</c>.</summary>
    /// <remarks>MCX.EXE @ 0x0060e4e0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0060e500</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Places the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) with the given size, makes its port
    /// and its screen pane, and resets its state. <paramref name="name"/> is unused by aObject itself.
    /// </summary>
    /// <returns>0, 3 when out of memory, or the port's error.</returns>
    /// <remarks>MCX.EXE @ 0x0060e520</remarks>
    virtual int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name); // slot 1

    /// <summary>
    /// Frees the port, pane, background, icon, animation and drop targets, removes the object from its parent and
    /// its timers, and lets go of every system reference to it (grab, text focus, modal, current object).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060e6b0</remarks>
    virtual void destroy(); // slot 2

    /// <summary>The width.</summary>
    /// <remarks>MCX.EXE @ 0x0060f4c0 (unnamed in the symbols)</remarks>
    virtual int32_t width(); // slot 3
    /// <remarks>MCX.EXE @ 0x0060f4d0</remarks>
    virtual int32_t height(); // slot 4
    /// <summary>The x position, relative to the parent.</summary>
    /// <remarks>MCX.EXE @ 0x0060f510</remarks>
    virtual int32_t x(); // slot 5
    /// <remarks>MCX.EXE @ 0x0060f520</remarks>
    virtual int32_t y(); // slot 6

    /// <summary>
    /// Moves the object to (<paramref name="xPos"/>, <paramref name="yPos"/>) relative to its parent, moves its pane
    /// and its children's. Unless <paramref name="temporary"/>, the position also becomes its home (where
    /// <see cref="HideMe"/> slides back to).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060f5a0</remarks>
    virtual void moveTo(int32_t xPos, int32_t yPos, int temporary = 0); // slot 7

    /// <summary>Resizes the object and its port (snapped to the 40-pixel grid when <see cref="gridAligned"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0060f650</remarks>
    virtual void resize(int32_t newWidth, int32_t newHeight); // slot 8

    /// <summary>The object's pane on the screen.</summary>
    /// <remarks>MCX.EXE @ 0x0060f590</remarks>
    virtual _pane* frame(); // slot 9

    /// <summary>Adds <paramref name="child"/> (at most 255), brings it to the front and places it.</summary>
    /// <remarks>MCX.EXE @ 0x0060f1f0</remarks>
    virtual void addChild(aObject* child); // slot 10
    /// <summary>Removes <paramref name="child"/> from the child list and clears its parent.</summary>
    /// <remarks>MCX.EXE @ 0x0060f2a0</remarks>
    virtual void removeChild(aObject* child); // slot 11
    /// <remarks>MCX.EXE @ 0x0060ea50</remarks>
    virtual void setParent(aObject* newParent); // slot 12
    /// <remarks>MCX.EXE @ 0x0060f1e0</remarks>
    virtual int32_t numberOfChildren(); // slot 13
    /// <summary>The x position on the screen (the sum of the parents' positions).</summary>
    /// <remarks>MCX.EXE @ 0x0060f530</remarks>
    virtual int32_t globalX(); // slot 14
    /// <remarks>MCX.EXE @ 0x0060f560</remarks>
    virtual int32_t globalY(); // slot 15

    /// <summary>
    /// Draws the object into its port: the background (or the icon when iconized), the animation, the paint
    /// routine, then the children.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060f6f0</remarks>
    virtual void draw(); // slot 16

    /// <summary>The front-most visible object at (<paramref name="xPos"/>, <paramref name="yPos"/>): a child or this.</summary>
    /// <remarks>MCX.EXE @ 0x0060e9c0</remarks>
    virtual aObject* findObject(int32_t xPos, int32_t yPos); // slot 17

    /// <summary>Whether the screen point is inside the pane (-1 or 0).</summary>
    /// <remarks>MCX.EXE @ 0x0060e8e0</remarks>
    virtual int pointInside(int32_t xPos, int32_t yPos); // slot 18
    /// <summary>Whether <paramref name="area"/> overlaps the pane (-1 or 0).</summary>
    /// <remarks>MCX.EXE @ 0x0060e950</remarks>
    virtual int rectIntersect(tagRECT area); // slot 19
    /// <summary>Whether the rectangle overlaps the pane (-1 or 0).</summary>
    /// <remarks>MCX.EXE @ 0x0060e910</remarks>
    virtual int rectIntersect(int32_t left, int32_t top, int32_t right, int32_t bottom); // slot 20

    /// <summary>
    /// The default handling: a click brings the window to the front; an iconized window is dragged by the mouse;
    /// the close event destroys it; the broadcast event goes to every child. Then the event routine, if any.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060f950</remarks>
    virtual void handleEvent(aEvent* event); // slot 21

    /// <summary>Loads background picture <paramref name="artPacket"/> of the art file.</summary>
    /// <remarks>MCX.EXE @ 0x0060fba0</remarks>
    virtual int32_t setBackground(int32_t artPacket); // slot 22
    /// <summary>Loads background picture <paramref name="fileName"/> (a TGA under <c>artPath</c>).</summary>
    /// <remarks>MCX.EXE @ 0x0060fb20</remarks>
    virtual int32_t setBackground(char* fileName); // slot 23

    /// <summary>Loads the animation (a VFX shape file under <c>artPath</c>).</summary>
    /// <remarks>MCX.EXE @ 0x0060fda0</remarks>
    virtual int32_t setAnimation(char* fileName); // slot 24
    /// <remarks>MCX.EXE @ 0x0060eab0</remarks>
    virtual void startAnimation(); // slot 25
    /// <remarks>MCX.EXE @ 0x0060eac0</remarks>
    virtual void stopAnimation(); // slot 26
    /// <remarks>MCX.EXE @ 0x0060fec0</remarks>
    virtual aPort* background(); // slot 27
    /// <remarks>MCX.EXE @ 0x0060fea0</remarks>
    virtual aAnimation* animation(); // slot 28
    /// <remarks>MCX.EXE @ 0x0060feb0</remarks>
    virtual aAnimation* icon(); // slot 29

    /// <summary>Changes the depth (removes and re-adds the object to its parent so it sorts again).</summary>
    /// <remarks>MCX.EXE @ 0x0060ea60</remarks>
    virtual void setDepth(int32_t newDepth); // slot 30
    /// <summary>Always null in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0060ea40</remarks>
    virtual aObject* children(); // slot 31

    /// <summary>
    /// Moves the object to the front of its parent's children of the same depth (the child list is kept sorted by
    /// depth). Without <paramref name="noShuffle"/>, grid-aligned siblings on the same 40-pixel cell are nudged
    /// aside.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060ef20 (unnamed in the symbols)</remarks>
    virtual void bringToFront(int noShuffle); // slot 32

    /// <summary>The front-most child of depth <paramref name="atDepth"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0060f460</remarks>
    virtual aObject* foremostChild(int32_t atDepth); // slot 33
    /// <remarks>MCX.EXE @ 0x0060eaa0</remarks>
    virtual int32_t depth(); // slot 34
    /// <summary>Child <paramref name="index"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0060f4a0</remarks>
    virtual aObject* child(int32_t index); // slot 35
    /// <remarks>MCX.EXE @ 0x0060f440</remarks>
    virtual int32_t dragStartX(); // slot 36
    /// <remarks>MCX.EXE @ 0x0060f450</remarks>
    virtual int32_t dragStartY(); // slot 37
    /// <remarks>MCX.EXE @ 0x0060f3f0</remarks>
    virtual int dragging(); // slot 38
    /// <remarks>MCX.EXE @ 0x0060f400</remarks>
    virtual void startDrag(int32_t xPos, int32_t yPos); // slot 39
    /// <remarks>MCX.EXE @ 0x0060f430</remarks>
    virtual void stopDrag(); // slot 40

    /// <summary>The mouse came over the object. Does nothing in aObject.</summary>
    /// <remarks>MCX.EXE @ 0x0060e8c0 (unnamed in the symbols; aFloatHelp and aMechIcon override it as <c>enter</c>)</remarks>
    virtual void enter() {} // slot 41
    /// <summary>The mouse left the object. Does nothing in aObject.</summary>
    /// <remarks>MCX.EXE @ 0x0060e8d0</remarks>
    virtual void leave() {} // slot 42

    /// <remarks>MCX.EXE @ 0x0060eb00</remarks>
    virtual int32_t backColor(); // slot 43
    /// <summary>The colour the port is wiped to (0xff: none).</summary>
    /// <remarks>MCX.EXE @ 0x0060eaf0</remarks>
    virtual void setBackColor(int32_t color); // slot 44
    /// <summary>Loads the icon (a VFX shape file); the iconized size becomes the icon's.</summary>
    /// <remarks>MCX.EXE @ 0x0060fe10</remarks>
    virtual int32_t setIcon(char* fileName); // slot 45
    /// <summary>
    /// Switches between normal, maximized and iconized (<see cref="aWindowState"/>), saving the current placement
    /// and restoring the new state's.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060fc60</remarks>
    virtual void setState(int32_t newState); // slot 46
    /// <remarks>MCX.EXE @ 0x0060fc40</remarks>
    virtual void iconize(); // slot 47
    /// <remarks>MCX.EXE @ 0x0060fc30</remarks>
    virtual void normalize(); // slot 48
    /// <remarks>MCX.EXE @ 0x0060fc20</remarks>
    virtual void maximize(); // slot 49

    /// <summary>
    /// Shows the object: steps a pending slide (<see cref="HideMe"/>), copies the port to the pane (transparently
    /// when <see cref="transparent"/>), then displays the children.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060f790</remarks>
    virtual void display(); // slot 50

    /// <remarks>MCX.EXE @ 0x0060e990</remarks>
    virtual void setPaintRoutine(void (*routine)(aObject*)); // slot 51
    /// <remarks>MCX.EXE @ 0x0060e9a0</remarks>
    virtual void setEventRoutine(void (*routine)(aObject*, aEvent*)); // slot 52
    /// <summary>Runs the paint routine, if any.</summary>
    /// <remarks>MCX.EXE @ 0x0060e9b0</remarks>
    virtual void paint(); // slot 53
    /// <summary>The port's pixels (its bitmap's buffer), or null.</summary>
    /// <remarks>MCX.EXE @ 0x0060f4e0</remarks>
    virtual void* ptr(); // slot 54
    /// <remarks>MCX.EXE @ 0x0060ead0</remarks>
    virtual void startModal(); // slot 55
    /// <remarks>MCX.EXE @ 0x0060eae0</remarks>
    virtual void stopModal(); // slot 56
    /// <remarks>MCX.EXE @ 0x0060fc50</remarks>
    virtual int32_t state(); // slot 57
    /// <summary>The port the object draws into.</summary>
    /// <remarks>MCX.EXE @ 0x0060f500</remarks>
    virtual aPort* port(); // slot 58
    /// <summary>Makes the pane (and the children's) show <paramref name="newPort"/>'s bitmap.</summary>
    /// <remarks>MCX.EXE @ 0x0060e880</remarks>
    virtual void setDisplayPort(aPort* newPort); // slot 59

    /// <summary>
    /// Draws a 3-line bevelled frame around the port (sunken when <paramref name="pushed"/>), after wiping it to the
    /// back colour when <paramref name="fill"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060ebf0</remarks>
    virtual void drawFramed(int pushed, int fill); // slot 60
    /// <summary>Draws the outline of <paramref name="area"/> (calls slot 62).</summary>
    /// <remarks>MCX.EXE @ 0x0060a750 (gui\asystem.h)</remarks>
    virtual void drawBox(uint8_t color, tagRECT area); // slot 61
    /// <summary>Draws a rectangle outline in the port; -1 for a side means the port's edge.</summary>
    /// <remarks>MCX.EXE @ 0x0060eb10</remarks>
    virtual void drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom); // slot 62

    /// <summary>Shows (<paramref name="show"/> nonzero) or hides the window from the object search.</summary>
    /// <remarks>MCX.EXE @ 0x0060a780 (gui\asystem.h; unnamed in the symbols, aMenu overrides it as <c>ShowGUIWindow</c>)</remarks>
    virtual void ShowGUIWindow(int show) { showWindow = show; } // slot 63
    /// <summary>The camera shown in the window; none for an aObject.</summary>
    /// <remarks>MCX.EXE @ 0x0060a790 (gui\asystem.h)</remarks>
    virtual Camera* GetCamera() { return nullptr; } // slot 64
    /// <remarks>MCX.EXE @ 0x0060a7a0 (gui\asystem.h)</remarks>
    virtual int IsShowing() { return showWindow; } // slot 65
    /// <summary>
    /// Starts sliding the object off the screen in its <see cref="hideDirection"/> (<paramref name="hide"/>
    /// nonzero), or back to its home position.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0060ff30</remarks>
    virtual void HideMe(int hide); // slot 66
    /// <remarks>MCX.EXE @ 0x0060a7b0 (gui\asystem.h)</remarks>
    virtual int IsHidden() { return hidden; } // slot 67
    /// <remarks>MCX.EXE @ 0x0060a7c0 (gui\asystem.h)</remarks>
    virtual void SetHideDirection(DIRECTION direction) { hideDirection = static_cast<uint8_t>(direction); } // slot 68
    /// <summary>Whether the port is copied to the screen with colour 0 transparent.</summary>
    /// <remarks>MCX.EXE @ 0x0060a7d0 (gui\asystem.h)</remarks>
    virtual void SetTransparent(int on) { transparent = on; } // slot 69
    /// <remarks>MCX.EXE @ 0x0060a7e0 (gui\asystem.h)</remarks>
    virtual int32_t left() { return x(); } // slot 70
    /// <remarks>MCX.EXE @ 0x0060a7f0 (gui\asystem.h)</remarks>
    virtual int32_t top() { return y(); } // slot 71
    /// <remarks>MCX.EXE @ 0x0060a800 (gui\asystem.h; unnamed in the symbols)</remarks>
    virtual int32_t right() { return x() + width(); } // slot 72
    /// <remarks>MCX.EXE @ 0x0060a820 (gui\asystem.h)</remarks>
    virtual int32_t bottom() { return y() + height(); } // slot 73
    /// <summary>Lets a movie window apply the movie's palette. Does nothing in aObject.</summary>
    /// <remarks>MCX.EXE @ 0x0060a840 (gui\asystem.h)</remarks>
    virtual void checkSmackerPalette() {} // slot 74
    /// <remarks>MCX.EXE @ 0x0060a850 (gui\asystem.h)</remarks>
    virtual int32_t getDropTargetCount() { return numDropTargets; } // slot 75
    /// <summary>Drop target <paramref name="index"/> (a screen rectangle), or null.</summary>
    /// <remarks>MCX.EXE @ 0x0060a860 (gui\asystem.h)</remarks>
    virtual tagRECT* getDropTarget(int32_t index)
    {
        return index < numDropTargets ? &dropTargets[index] : nullptr;
    } // slot 76

    /// <summary>Wipes a rectangle of the port to <paramref name="color"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0060eec0</remarks>
    void FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color);

    /// <summary>Sets one pixel of the port.</summary>
    /// <remarks>MCX.EXE @ 0x0060fed0</remarks>
    void SetBit(int32_t xPos, int32_t yPos, uint8_t color);

    /// <summary>Nonzero when the object snaps to the 40-pixel grid when resized or raised. Only cleared in MCX.EXE's aObject code.</summary>
    int32_t gridAligned = 0; // +0x04
    /// <summary>The object's pane on the screen window (owned; allocated by <see cref="init"/>).</summary>
    _pane* framePane = nullptr; // +0x08
    int32_t winWidth = 0;       // +0x0c
    int32_t winHeight = 0;      // +0x10
    /// <summary>The position relative to the parent.</summary>
    int32_t winX = 0; // +0x14
    int32_t winY = 0; // +0x18
    /// <summary>The depth: children are kept sorted by it (front-most last).</summary>
    int32_t winDepth = 0; // +0x1c
    /// <summary>The placement while maximized (width, height, x, y).</summary>
    int32_t maxWidth = 0;  // +0x20
    int32_t maxHeight = 0; // +0x24
    int32_t maxX = 0;      // +0x28
    int32_t maxY = 0;      // +0x2c
    /// <summary>The placement while normal (width, height, x, y).</summary>
    int32_t normalWidth = 0;  // +0x30
    int32_t normalHeight = 0; // +0x34
    int32_t normalX = 0;      // +0x38
    int32_t normalY = 0;      // +0x3c
    /// <summary>The placement while iconized (the icon's size, and x, y).</summary>
    int32_t iconWidth = 0;  // +0x40
    int32_t iconHeight = 0; // +0x44
    int32_t iconX = 0;      // +0x48
    int32_t iconY = 0;      // +0x4c
    /// <summary>The <see cref="aWindowState"/>.</summary>
    int32_t winState = 0; // +0x50
    /// <summary>Nonzero while the object is shown and can be found (<see cref="IsShowing"/>); -1 after <see cref="init"/>.</summary>
    int32_t showWindow = 0; // +0x54
    /// <summary>What kind of object this is, for the interface's hit tests: -1 by default, 4 a camera view window, 7 a floating help.</summary>
    int16_t objectType = 0; // +0x58
    /// <summary>Nonzero while hidden (slid off the screen).</summary>
    int32_t hidden = 0; // +0x5c
    /// <summary>How far the object still has to slide (signed); 0 when not moving.</summary>
    int32_t hideOffset = 0; // +0x60
    /// <summary>The <see cref="DIRECTION"/> it slides out to (3 after <see cref="init"/>).</summary>
    uint8_t hideDirection = 0; // +0x64
    /// <summary>The home position the slide returns to.</summary>
    int32_t homeX = 0; // +0x68
    int32_t homeY = 0; // +0x6c
    /// <summary>Nonzero when the port is copied transparently.</summary>
    int32_t transparent = 0; // +0x70
    /// <summary>The children, sorted by depth.</summary>
    aObject* childList[255] = {}; // +0x74
    int32_t numChildren = 0;      // +0x470
    aObject* parent = nullptr;    // +0x474
    /// <summary>The background picture, copied into the port by <see cref="draw"/>.</summary>
    aPort* backgroundPort = nullptr; // +0x478
    /// <summary>The icon drawn while iconized.</summary>
    aAnimation* iconAnimation = nullptr; // +0x47c
    /// <summary>The animation drawn over the background while <see cref="animating"/>.</summary>
    aAnimation* windowAnimation = nullptr; // +0x480
    /// <summary>Where a drag started.</summary>
    int32_t dragX = 0;     // +0x484
    int32_t dragY = 0;     // +0x488
    int32_t dragOn = 0;    // +0x48c
    int32_t animating = 0; // +0x490
    /// <summary>The colour the port is wiped to; 0xff for none.</summary>
    int32_t backgroundColor = 0; // +0x494
    /// <summary>The port the object draws into (owned).</summary>
    aPort* displayPort = nullptr;                      // +0x498
    void (*paintRoutine)(aObject*) = nullptr;          // +0x49c
    void (*eventRoutine)(aObject*, aEvent*) = nullptr; // +0x4a0
    int32_t numDropTargets = 0;                        // +0x4a4
    /// <summary>The drop targets (owned; <see cref="destroy"/> frees them with <c>delete[]</c>).</summary>
    tagRECT* dropTargets = nullptr; // +0x4a8
};

/// <summary>A window holding up to two panes, tiled side by side (or stacked) or one at a time.</summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> and <c>gui\asystem.h</c>, 0x4c0 bytes. Vtable: aObject's 77 slots, then
/// 77..89 below. The camera's main window (aMainWindow) and aEmptyTitleWindow derive from it.
/// </remarks>
class aHolderObject : public aObject
{
public:
    /// <summary>Like aObject::init (inlined), and clears the panes; no pane is active.</summary>
    /// <remarks>MCX.EXE @ 0x00616180</remarks>
    int32_t init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    /// <remarks>MCX.EXE @ 0x006162c0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00616330</remarks>
    void resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Forgets <paramref name="oldChild"/> if it is a pane, then removes it.</summary>
    /// <remarks>MCX.EXE @ 0x0061d6d0 (gui\asystem.h)</remarks>
    void removeChild(aObject* oldChild) override;
    /// <remarks>MCX.EXE @ 0x006162e0</remarks>
    void display() override;
    /// <summary>Draws no frame.</summary>
    /// <remarks>MCX.EXE @ 0x0061d6b0 (gui\asystem.h)</remarks>
    void drawFramed(int, int) override {}
    /// <summary>Draws no box.</summary>
    /// <remarks>MCX.EXE @ 0x0061d6c0 (gui\asystem.h)</remarks>
    void drawBox(uint8_t, int32_t, int32_t, int32_t, int32_t) override {}
    using aObject::drawBox;

    /// <summary>Tiles the two panes, or shows only the active one.</summary>
    /// <remarks>MCX.EXE @ 0x006165f0</remarks>
    virtual void SetTiled(int tiled); // slot 77
    /// <remarks>MCX.EXE @ 0x0061d710 (gui\asystem.h)</remarks>
    virtual int GetTiled() { return tiled; } // slot 78
    /// <summary>Lays the panes out for the current mode.</summary>
    /// <remarks>MCX.EXE @ 0x006163c0</remarks>
    virtual void Retile(); // slot 79
    /// <remarks>MCX.EXE @ 0x0061d720 (gui\asystem.h)</remarks>
    virtual void SetVertical(int on)
    {
        vertical = on;
        Retile();
    } // slot 80

    /// <remarks>MCX.EXE @ 0x0061d740 (gui\asystem.h)</remarks>
    virtual int GetVertical() { return vertical; } // slot 81
    /// <summary>Adds <paramref name="pane"/> in the first free pane slot, as a child.</summary>
    /// <remarks>MCX.EXE @ 0x006164e0</remarks>
    virtual void AddPane(aObject* pane); // slot 82
    /// <remarks>MCX.EXE @ 0x00616550</remarks>
    virtual void RemovePane(aObject* pane); // slot 83
    /// <summary>Pane 0 or 1, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0061d750 (gui\asystem.h)</remarks>
    virtual aObject* GetPane(char index)
    {
        return (index >= 0 && index < 2) ? panes[static_cast<int>(index)] : nullptr;
    } // slot 84

    /// <remarks>MCX.EXE @ 0x0061d780 (gui\asystem.h)</remarks>
    virtual aObject* GetActivePane()
    {
        return activePane >= 0 ? panes[static_cast<int>(activePane)] : nullptr;
    } // slot 85

    /// <remarks>MCX.EXE @ 0x0061d7a0 (gui\asystem.h)</remarks>
    virtual aObject* GetInactivePane()
    {
        return activePane == 0 ? panes[1] : (activePane == 1 ? panes[0] : nullptr);
    } // slot 86

    /// <summary>0, 1, or -1 when none.</summary>
    /// <remarks>MCX.EXE @ 0x0061d7c0 (gui\asystem.h)</remarks>
    virtual char GetActivePaneNumber() { return activePane; } // slot 87
    /// <remarks>MCX.EXE @ 0x006165c0</remarks>
    virtual void SetActivePane(aObject* pane); // slot 88
    /// <summary>
    /// Makes pane <paramref name="index"/> active (0 always; 1 only when there is a second pane) and re-tiles.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0061d7d0 (gui\asystem.h; unnamed in the symbols)</remarks>
    virtual void SetActivePaneNumber(char index); // slot 89

    aObject* panes[2] = {}; // +0x4ac
    int32_t tiled = 0;      // +0x4b4
    /// <summary>The active pane: 0, 1 or -1 (none).</summary>
    char activePane = -1; // +0x4b8
    /// <summary>Nonzero to stack the tiled panes vertically.</summary>
    int32_t vertical = 0; // +0x4bc
};

/// <summary>A centred box with a line of text and an OK button (the version dialog).</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x4b0 bytes.</remarks>
class aMessageBox : public aObject
{
public:
    /// <summary>
    /// Sizes the box to <paramref name="text"/> in the white font, centres it on the screen, adds the OK button
    /// (which runs <c>DestroyVersion</c>) and writes the text.
    /// </summary>
    /// <returns>0, -3 without the white font, or the button's error.</returns>
    /// <remarks>MCX.EXE @ 0x00616660 (unnamed in the symbols; an init overload)</remarks>
    int32_t init(uint8_t* text);
    using aObject::init;

    /// <remarks>MCX.EXE @ 0x00616820</remarks>
    void destroy() override;
    /// <summary>Passes events inside the box to the button.</summary>
    /// <remarks>MCX.EXE @ 0x0061ded0</remarks>
    void handleEvent(aEvent* event) override;

    /// <summary>The OK button (an aButton).</summary>
    aObject* okButton = nullptr; // +0x4ac
};

/// <summary>A running GUI timer: sends a timer event (or a given event) to an object every interval.</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x1c bytes (allocated from the GUI heap).</remarks>
struct aTimer
{
    /// <summary>The interval in milliseconds.</summary>
    uint32_t interval; // +0x00
    /// <summary>When it last fired (ms, or scenario ms for a scenario-time timer).</summary>
    uint32_t lastTime; // +0x04
    aObject* target;   // +0x08
    /// <summary>The id passed back in the timer event's <c>data</c>.</summary>
    int16_t id; // +0x0c
    /// <summary>The event type to send instead of a timer event (then the timer fires once), or 0.</summary>
    int32_t eventType; // +0x10
    /// <summary>The <c>data</c> of that event.</summary>
    int32_t eventData; // +0x14
    /// <summary>Nonzero to count in scenario time instead of real time.</summary>
    int32_t useScenarioTime; // +0x18
};

/// <summary>The GUI timers (at most 99), run by a system callback while any exist.</summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0x4b8 bytes. While a timer's event is being handled the list is locked:
/// removals are queued in <see cref="timersToWhack"/> and done by <see cref="UnlockTimers"/>.
/// </remarks>
class aTimerManager
{
public:
    /// <remarks>MCX.EXE @ 0x00615af0</remarks>
    aTimerManager();
    /// <summary>Frees the timers.</summary>
    /// <remarks>MCX.EXE @ 0x00615b10</remarks>
    ~aTimerManager();

    /// <remarks>MCX.EXE @ 0x00615b40</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x00615b60</remarks>
    static void operator delete(void* ptr);

    /// <summary>Makes the callback that runs the timers (<c>TimerCallback</c>).</summary>
    /// <remarks>MCX.EXE @ 0x00615b80</remarks>
    int32_t Init();
    /// <remarks>MCX.EXE @ 0x00615bc0</remarks>
    void destroy();
    /// <summary>Adds a timer unless one with the same object, id, interval and event exists.</summary>
    /// <remarks>MCX.EXE @ 0x00615c30</remarks>
    int32_t AddUniqueTimer(aObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                           int useScenarioTime);
    /// <summary>Adds a timer; the first one registers the system callback.</summary>
    /// <returns>0, or -1 when full or out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x00615cb0</remarks>
    int32_t AddTimer(aObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                     int useScenarioTime);
    /// <summary>Removes every timer of <paramref name="target"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00615d60</remarks>
    void RemoveTimers(aObject* target);
    /// <remarks>MCX.EXE @ 0x00615e40</remarks>
    void RemoveTimer(aObject* target, int16_t id);
    /// <summary>Removes timer number <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00615f10</remarks>
    void RemoveTimer(int32_t index);
    /// <remarks>MCX.EXE @ 0x00615fc0</remarks>
    aTimer* GetTimer(int16_t index);
    /// <remarks>MCX.EXE @ 0x00615fe0</remarks>
    aTimer* GetTimer(aObject* target, int16_t id);
    /// <summary>Locks the list while <paramref name="running"/>'s event is handled.</summary>
    /// <remarks>MCX.EXE @ 0x00616030</remarks>
    void LockTimersExcept(aTimer* running);
    /// <summary>Unlocks the list and does the queued removals.</summary>
    /// <remarks>MCX.EXE @ 0x00616050</remarks>
    void UnlockTimers();

    /// <summary>A queued removal: object and id, object and -1 (all its timers), or null and an index.</summary>
    struct TimerToWhack
    {
        aObject* target; // +0x00
        int32_t id;      // +0x04
    };

    int32_t numTimers = 0;        // +0x00
    int32_t numTimersToWhack = 0; // +0x04
    /// <summary>
    /// The timers. <see cref="AddTimer"/> accepts a 100th, which lands on <c>timersToWhack[0]</c>'s first word.
    /// </summary>
    aTimer* timers[99] = {};             // +0x08
    TimerToWhack timersToWhack[99] = {}; // +0x194
    /// <summary>The system callback that runs the timers.</summary>
    aCallback* timerCallback = nullptr; // +0x4ac
    int32_t locked = 0;                 // +0x4b0
    /// <summary>The timer whose event is being handled while locked (it may remove itself directly).</summary>
    aTimer* lockedExcept = nullptr; // +0x4b4
};

/// <summary>
/// The application: the screen object and root of the window tree (<c>screenWindow</c> is its child), with the
/// display, the palette and its gamma, the mouse cursor, the per-frame callbacks, the object grabbing the mouse or
/// keyboard, the modal object and the timers. There is one, <c>application</c>.
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0xb0c bytes. Vtable 0x0077af08: aObject's with <see cref="width"/> and
/// <see cref="height"/> returning the screen's size. The DirectDraw and window handles are kept as opaque
/// placeholders so the offsets stay documented; the port presents through MCDisplay.
/// </remarks>
class aSystem : public aObject
{
public:
    /// <summary>The screen width.</summary>
    /// <remarks>MCX.EXE @ 0x00614780</remarks>
    int32_t width() override;
    /// <remarks>MCX.EXE @ 0x00614790</remarks>
    int32_t height() override;

    /// <summary>Opens the display at <paramref name="width"/> x <paramref name="height"/>, <paramref name="bitDepth"/> bits.</summary>
    /// <remarks>MCX.EXE @ 0x0060db20</remarks>
    int32_t startupDirectDraw(int32_t width, int32_t height, int32_t bitDepth);
    /// <summary>Re-opens the display in another mode (for full-screen movies).</summary>
    /// <remarks>MCX.EXE @ 0x0060df40</remarks>
    int32_t resetDirectDraw(int32_t width, int32_t height, int32_t bitDepth);
    /// <remarks>MCX.EXE @ 0x0060e330</remarks>
    int32_t shutdownDirectDraw();
    /// <remarks>MCX.EXE @ 0x0060e3d0</remarks>
    void setFlipToGDI();
    /// <remarks>MCX.EXE @ 0x0060e3e0</remarks>
    void clearFlipToGDI();
    /// <summary>Does nothing in MCX.EXE.</summary>
    /// <remarks>MCX.EXE @ 0x0060e3f0</remarks>
    void flipToGDI();
    /// <summary>Sets the scroll-trigger rectangle: (1, 1) to the screen's size less 4.</summary>
    /// <remarks>MCX.EXE @ 0x0060e400</remarks>
    void setScrollRect();

    /// <summary>
    /// Starts the game: the window, the heaps, the fonts, the palette, the display, the screen window, the timers,
    /// the interface and the mouse thread.
    /// </summary>
    /// <param name="instance">The HINSTANCE in the original; unused by the port.</param>
    /// <param name="prevInstance">The previous HINSTANCE in the original; unused by the port.</param>
    /// <returns>Nonzero when started.</returns>
    /// <remarks>MCX.EXE @ 0x00612c10</remarks>
    int start(void* instance, void* prevInstance, char* commandLine, int showCommand, int16_t screenWidth,
              int16_t screenHeight);
    /// <summary>Shuts everything <see cref="start"/> started down.</summary>
    /// <remarks>MCX.EXE @ 0x00613b70</remarks>
    void stop();
    /// <summary>
    /// Plays Smacker movie <paramref name="fileName"/> in <paramref name="window"/> (or a new centred movie window),
    /// blocking the game when <paramref name="exclusive"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00614390</remarks>
    int32_t startSmackerMovie(char* fileName, uint32_t flags, aObject* window, int exclusive);
    /// <summary>The main loop: pumps messages, runs the callbacks and redraws until the game quits.</summary>
    /// <remarks>MCX.EXE @ 0x00614490</remarks>
    void run();

    /// <remarks>MCX.EXE @ 0x006147a0</remarks>
    int32_t screenOffsetX();
    /// <remarks>MCX.EXE @ 0x006147b0</remarks>
    int32_t screenOffsetY();
    /// <summary>The window handle (an HWND in the original; opaque in the port).</summary>
    /// <remarks>MCX.EXE @ 0x006147c0</remarks>
    void* window();
    /// <remarks>MCX.EXE @ 0x006147d0</remarks>
    void setScreenOffsetX(int32_t offset);
    /// <remarks>MCX.EXE @ 0x006147e0</remarks>
    void setScreenOffsetY(int32_t offset);

    /// <summary>Adds a callback run every frame (at most 98).</summary>
    /// <returns>0, 2 for null, 3 when full.</returns>
    /// <remarks>MCX.EXE @ 0x006147f0</remarks>
    int32_t addCallback(aCallback* callback);
    /// <returns>0, 1 when not found, 2 for null.</returns>
    /// <remarks>MCX.EXE @ 0x00614840</remarks>
    int32_t removeCallback(aCallback* callback);
    /// <remarks>MCX.EXE @ 0x006148d0</remarks>
    void setModalObject(aObject* obj);
    /// <summary>The modal object.</summary>
    /// <remarks>MCX.EXE @ 0x006148f0 (unnamed in the symbols)</remarks>
    aObject* modalObject() { return modal; }
    /// <remarks>MCX.EXE @ 0x00614900</remarks>
    void clearModal();
    /// <summary>Gives <paramref name="obj"/> the mouse.</summary>
    /// <remarks>MCX.EXE @ 0x00614910</remarks>
    void grab(aObject* obj);
    /// <summary>Gives <paramref name="obj"/> the keyboard.</summary>
    /// <remarks>MCX.EXE @ 0x00614970</remarks>
    void setText(aObject* obj);
    /// <summary>Sets the object under the cursor.</summary>
    /// <remarks>MCX.EXE @ 0x006149d0</remarks>
    void setCurrentObject(aObject* obj);
    /// <summary>Lets go of the mouse grab.</summary>
    /// <remarks>MCX.EXE @ 0x006149e0</remarks>
    void release();
    /// <summary>Lets go of the keyboard focus.</summary>
    /// <remarks>MCX.EXE @ 0x006149f0</remarks>
    void releaseText();
    /// <remarks>MCX.EXE @ 0x00614a40</remarks>
    aObject* grabbedObject();
    /// <remarks>MCX.EXE @ 0x00614a50</remarks>
    aObject* textObject();
    /// <remarks>MCX.EXE @ 0x00614a60</remarks>
    aObject* currentObject();

    /// <summary>
    /// Sets <paramref name="count"/> palette entries from <paramref name="first"/> (clamped to 10..245 unless a movie
    /// plays), through the gamma table; <paramref name="sixBit"/> shifts 6-bit values up.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00614a70</remarks>
    int tweakDDPalette(int first, int count, VFX_RGB* colors, int sixBit);
    /// <summary>Steps the gamma level (0..3) and re-applies the palette.</summary>
    /// <remarks>MCX.EXE @ 0x00614d60</remarks>
    void gammaCorrectCurrentPalette();
    /// <remarks>MCX.EXE @ 0x00614da0</remarks>
    void gammaCorrectCurrentPalette(int32_t level);
    /// <summary>Fades the palette to black.</summary>
    /// <remarks>MCX.EXE @ 0x00614f90</remarks>
    void fadeDownCurrentPalette();
    /// <summary>Sets the palette (or, with <paramref name="first"/> 0, remembers it in <c>paletteRgb</c>).</summary>
    /// <remarks>MCX.EXE @ 0x006151c0</remarks>
    void activatePalette(uint8_t* colors, int first, int count);
    /// <remarks>MCX.EXE @ 0x00615240</remarks>
    void activatePaletteFromTGA(char* fileName);
    /// <remarks>MCX.EXE @ 0x00615400</remarks>
    void activatePaletteFromGIF(char* fileName);
    /// <remarks>MCX.EXE @ 0x00615510</remarks>
    void activateSmackerPalette(uint8_t* colors);

    /// <remarks>MCX.EXE @ 0x00615530</remarks>
    int32_t AddTimer(aObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                     int useScenarioTime);
    /// <remarks>MCX.EXE @ 0x00615590</remarks>
    int32_t AddUniqueTimer(aObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                           int useScenarioTime);
    /// <remarks>MCX.EXE @ 0x006155f0</remarks>
    void RemoveTimer(aObject* target, int16_t id);
    /// <remarks>MCX.EXE @ 0x00615640</remarks>
    void RemoveTimers(aObject* target);
    /// <summary>Sets the mouse cursor (ignored while the cursor is hidden).</summary>
    /// <remarks>MCX.EXE @ 0x00615690</remarks>
    void SetCurrentCursor(CursorType cursor);
    /// <remarks>MCX.EXE @ 0x00615730</remarks>
    void showCursor(int show);

    /// <summary>IDirectDraw* in the original; the port presents through MCDisplay.</summary>
    void* ddObject = nullptr; // +0x4ac
    /// <summary>The second DirectDraw interface in the original (released at shutdown); opaque.</summary>
    void* ddObject2 = nullptr; // +0x4b0
    /// <summary>The primary DirectDraw surface in the original; opaque.</summary>
    void* ddPrimarySurface = nullptr; // +0x4b4
    /// <summary>The back DirectDraw surface in the original; opaque.</summary>
    void* ddBackSurface = nullptr; // +0x4b8
    /// <summary>Used by startupDirectDraw/resetDirectDraw only (a DirectDraw object); opaque.</summary>
    void* unknown4BC = nullptr; // +0x4bc
    /// <summary>The DirectDraw palette in the original (tweakDDPalette's SetEntries); opaque.</summary>
    void* ddPalette = nullptr; // +0x4c0
    /// <summary>Used by startupDirectDraw/resetDirectDraw only; opaque.</summary>
    void* unknown4C4 = nullptr; // +0x4c4
    /// <summary>
    /// PREFS.CFG "PaletteCycle": cycleColors animates the palette when nonzero (-1 until the prefs are read; the
    /// mission sets 1 after a movie).
    /// </summary>
    int32_t paletteCycle = 0; // +0x4c8
    /// <summary>Nonzero after <see cref="setFlipToGDI"/>.</summary>
    int32_t flipToGDIRequested = 0; // +0x4cc
    /// <summary>The current palette (8 bits per channel), before gamma.</summary>
    VFX_RGB currentPalette[256] = {}; // +0x4d0
    /// <summary>The gamma level, 0..3.</summary>
    int32_t gammaLevel = 0; // +0x7d0
    /// <summary>The text formatter the inventory blocks and the chat window share.</summary>
    SMUTI textFormatter;      // +0x7d4
    int32_t screenWidth = 0;  // +0x8fc
    int32_t screenHeight = 0; // +0x900
    int32_t offsetX = 0;      // +0x904
    int32_t offsetY = 0;      // +0x908
    /// <summary>HWND in the original; opaque (the port's window belongs to MCDisplay).</summary>
    void* windowHandle = nullptr; // +0x90c
    int32_t numCallbacks = 0;     // +0x910
    /// <summary>The per-frame callbacks.</summary>
    aCallback* callbacks[99] = {}; // +0x914
    /// <summary>The object holding the mouse.</summary>
    aObject* grabbed = nullptr; // +0xaa0
    /// <summary>The object holding the keyboard.</summary>
    aObject* textFocus = nullptr; // +0xaa4
    /// <summary>The object under the cursor.</summary>
    aObject* current = nullptr; // +0xaa8
    aObject* modal = nullptr;   // +0xaac
    /// <summary>Not accessed in MCX.EXE.</summary>
    uint8_t unknownAB0[0x2c] = {};                         // +0xab0
    CursorType currentCursor = static_cast<CursorType>(0); // +0xadc
    /// <summary>Nonzero while the cursor is hidden (<see cref="showCursor"/>).</summary>
    int32_t cursorHidden = 0;              // +0xae0
    aTimerManager* timerManager = nullptr; // +0xae4
    /// <summary>Set to 0xddac0000 by start; never read.</summary>
    uint32_t unknownAE8 = 0; // +0xae8
    /// <summary>The exclusive movie window while one plays.</summary>
    aObject* smackerWindow = nullptr; // +0xaec
    /// <summary>A second movie window, destroyed by run when its movie is over.</summary>
    aObject* smackerWindow2 = nullptr; // +0xaf0
    /// <summary>The opening movie's window.</summary>
    aOpeningSmackerWindow* openingSmackerWindow = nullptr; // +0xaf4
    /// <summary>Where the mouse scrolls the map: outside (1, 1)..(width - 4, height - 4).</summary>
    tagRECT scrollRect = {}; // +0xaf8
    /// <summary>The cursor shape drawn (-1 hidden).</summary>
    int32_t cursorShape = 0; // +0xb08
};

// Free functions of gui\asystem.cpp.

/// <summary>Makes an 8-bit DIB section for the windowed display (GDI in the original).</summary>
/// <remarks>MCX.EXE @ 0x00610070. The port has no GDI; kept for the translation's bookkeeping.</remarks>
int32_t createDIBSection(int32_t width, int32_t height, void** bitmapInfo, void** bitmap, uint8_t** bits);
/// <summary>A GDI palette from a GIF's colour table (an HPALETTE in the original).</summary>
/// <remarks>MCX.EXE @ 0x00610140</remarks>
void* CreatePaletteFromGIF(char* fileName);
/// <remarks>MCX.EXE @ 0x006102f0</remarks>
void* CreatePaletteFromRAM(void* colors);
/// <remarks>MCX.EXE @ 0x00610380</remarks>
void* CreateSmackPaletteFromRAM(void* colors);
/// <summary>Draws and displays the whole window tree.</summary>
/// <remarks>MCX.EXE @ 0x00610410</remarks>
void aRedrawScreen();
/// <remarks>MCX.EXE @ 0x00610450</remarks>
int32_t aLockScreen();
/// <remarks>MCX.EXE @ 0x00610530</remarks>
int32_t aUnlockScreen();
/// <summary>Sends an event of type <paramref name="message"/> to <paramref name="obj"/> at once.</summary>
/// <remarks>MCX.EXE @ 0x00610590</remarks>
void aPostMessage(aObject* obj, int32_t message);
/// <remarks>MCX.EXE @ 0x006105d0</remarks>
void TestMsgCallback(FIDPMessage* message, void* data);
/// <remarks>MCX.EXE @ 0x006105f0</remarks>
void SendAndReceiveTestMessages();
/// <remarks>MCX.EXE @ 0x006106a0</remarks>
int StartMultiplayerGame(char* commandLine);
/// <summary>Reads the command line's switches.</summary>
/// <remarks>MCX.EXE @ 0x00610940</remarks>
void ParseCommandLine(char* commandLine);
/// <summary>Copies the next word of <paramref name="line"/> from <paramref name="pos"/> into <paramref name="word"/>.</summary>
/// <remarks>MCX.EXE @ 0x00610b30</remarks>
int32_t parseCommandLine(char* line, int32_t pos, char* word, int32_t maxLength);
/// <summary>The program's entry: starts the application and runs it.</summary>
/// <remarks>MCX.EXE @ 0x00610be0 (the HINSTANCEs are opaque in the port)</remarks>
int RealWinMain(void* instance, void* prevInstance, char* commandLine, int showCommand);
/// <summary>The version box's OK button: destroys it.</summary>
/// <remarks>MCX.EXE @ 0x00610f80</remarks>
void DestroyVersion();
/// <summary>Routes an event to the grabbed, text, modal or found object, and handles the global keys.</summary>
/// <remarks>MCX.EXE @ 0x00610fb0</remarks>
void handleEvent(aEvent* event);
/// <summary>Whether the last keys typed spell <paramref name="code"/>.</summary>
/// <remarks>MCX.EXE @ 0x006117c0</remarks>
int Cheat(char* code);
/// <summary>Turns a window message into an <see cref="aEvent"/> and handles it (the platform layer calls it).</summary>
/// <remarks>MCX.EXE @ 0x00611860 (the HWND is opaque in the port)</remarks>
int32_t translateMessage(void* window, uint32_t message, uint32_t wParam, int32_t lParam);
/// <summary>Scrolls the map when the cursor is at the screen's edge.</summary>
/// <remarks>MCX.EXE @ 0x00611cd0</remarks>
void ScrollScreen();
/// <summary>The window procedure (WM_DESTROY, WM_MOVE, WM_SIZE, WM_PAINT, activation, ...).</summary>
/// <remarks>MCX.EXE @ 0x00612080 (unnamed in the symbols; the HWND is opaque in the port)</remarks>
int32_t WindowProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam);
/// <remarks>MCX.EXE @ 0x006125d0</remarks>
void InitWindowMode();
/// <remarks>MCX.EXE @ 0x00612710</remarks>
void InitFullScreen();
/// <summary>Sends mouse movement and button changes as events.</summary>
/// <remarks>MCX.EXE @ 0x006127e0</remarks>
void CheckMouse();
/// <summary>The palette of art file <paramref name="fileName"/> (allocated).</summary>
/// <remarks>MCX.EXE @ 0x00612a60</remarks>
VFX_RGB* GetPaletteFromArt(char* fileName);
/// <summary>The timer manager's callback: fires the due timers.</summary>
/// <remarks>MCX.EXE @ 0x006158f0</remarks>
void TimerCallback();
/// <summary>Where the cursor was when the current message was sent, in window coordinates.</summary>
/// <remarks>MCX.EXE @ 0x00616140</remarks>
tagPOINT GetMessageCursorLoc();

// Globals of gui\asystem.cpp.

/// <summary>The application.</summary>
extern aSystem* application;
/// <summary>The top-level window every screen is a child of.</summary>
extern aObject* screenWindow;
/// <summary>The port of the whole screen.</summary>
extern aPort* screenPort;
/// <summary>The GUI heap every aObject, aPort and aCallback comes from.</summary>
extern UserHeap* guiHeap;
/// <summary>The art packet file (art\art.pak) <c>aPort::init(long)</c> reads from.</summary>
extern PacketFile* artFile;
extern char* startupPakFile;
extern aMessageBox* versionDialog;
extern aObject* smackWindowPointer;
extern aObject* featureScreen;
extern int featureScreenDone;
extern int escapedSmackerMovie;
extern aCallback* mouseTrackerCallback;
/// <summary>The fonts loaded by <see cref="aSystem::start"/>.</summary>
extern aFont* systemFont;
extern aFont* blackFont;
extern aFont* greyFont;
extern aFont* whiteFont;
extern aFont* redFont;
extern aFont* greenFont;
extern aFont* blueFont;
extern aFont* dimFont;
extern aFont* yellowFont;
extern aFont* yellowDropFont;
extern aFont* blueDropFont;
extern aFont* medBlackFont;
extern aFont* medGreyFont;
extern aFont* medWhiteFont;
extern aFont* medRedFont;
extern aFont* medGreenFont;
extern aFont* medBlueFont;
extern aFont* medDimFont;
extern aFont* medYellowFont;
extern aFont* lgBlackFont;
extern aFont* lgGreyFont;
extern aFont* lgWhiteFont;
extern aFont* lgRedFont;
extern aFont* lgGreenFont;
extern aFont* lgBlueFont;
extern aFont* lgDimFont;
extern aFont* lgYellowFont;
/// <summary>
/// The same fonts by colour and size: row = colour (0 black, 1 red, 2 yellow, 3 green, 4 blue, 5 grey, 6 white,
/// 7 dim, 8 yellow drop, 9 blue drop), column = small, medium, large. The scrolling text objects pick fonts from it.
/// </summary>
extern aFont* fonts[10][3];
/// <summary>Nonzero while the game is paused, and while it asks the player something (quit, ...).</summary>
extern int gamePaused;
extern int gameAsked;
/// <summary>The engine font used for lines of text drawn in the world.</summary>
extern Font* lineFont;
extern int gWidth;
extern int gHeight;
extern int gBitDepth;
extern int gFullScreen;
/// <summary>Port-only: stretch the picture over the whole window instead of keeping 4:3 with bars (PREFS
/// "StretchToFit", read by systemInit).</summary>
extern int gStretchToFit;
/// <summary>Port-only: draw the cursor into the frame as the original did, instead of showing it as the system
/// cursor (PREFS "SoftwareCursor", read by systemInit).</summary>
extern int gSoftwareCursor;
extern int applicationActive;
extern uint32_t systemHeapSize;
extern uint32_t guiHeapSize;
extern uint32_t stackSize;
extern uint32_t topOfStack;
/// <summary>The gamma translation table (initialised data in MCX.EXE @ 0x00789f8c).</summary>
extern uint8_t GammaColorTranslation[256];
extern int allowMagicWindowSwitching;
extern int32_t displayWidth;
extern int32_t displayHeight;

/// <summary>
/// Port-only: makes the screen the window's size in pixels (at least 640x480) when the window has changed: the
/// display's buffer, <c>gWidth</c>/<c>displayWidth</c>/<c>application->screenWidth</c>, the screen window, then the
/// original's 0x12 broadcast so the in-mission windows follow. Called when a scenario starts and every frame of one;
/// the 640x480 screens draw only parts of the buffer, so they never resize it.
/// </summary>
/// <returns>Whether the screen changed size.</returns>
bool MCFollowWindowSize();
extern int oldMouseX;
extern int oldMouseY;
extern float frameRate;
/// <summary>Performance-counter times (LARGE_INTEGER in the original).</summary>
extern int64_t startTime;
extern int64_t stopTime;
extern int64_t prevStart;
extern int64_t countsPerSecond;
/// <summary>The last cursor position of a drag (aObject::handleEvent).</summary>
extern int32_t lastX;
extern int32_t lastY;
extern int leftMouseButtonDown;
extern int rightMouseButtonDown;
extern char appName[];
extern char WindowTitle[];
extern char paletteName[];
extern char* backPtr;
/// <summary>The cheat codes and the ring buffer of keys typed.</summary>
extern char Cheat_framegraph[];
extern char Cheat_BunnyStrike[];
extern char Cheat_HealAll[];
extern char Cheat_DeadEye[];
extern char Cheat_CantHitMe[];
extern char Cheat_GetSalvage[];
extern char Cheat_Reveal[];
extern char Cheat_Duh[];
extern char CheatKey[128];
extern int CheatPointer;
/// <summary>The "can't hit me" cheat: home team movers take no hits outside multiplayer (the name is the port's).</summary>
/// <remarks>MCX.EXE @ 0x007ab0f0</remarks>
extern uint32_t CantHitMe;
extern int cheatsOn;
extern int CantBlowSalvage;
extern int BunnyStrikesOn;
extern int Duh;
extern int recordClicks;
extern int SavedPosition;
extern int lockFrameRate;
extern int lockActive;
extern int takeScreenShot;
extern uint32_t scrollWait;
extern int32_t displayProfileData;
extern char keySetting;
extern int QueuePlayerOrders;
extern int forceGatesClosed;
extern int drawTerrainGrid;
/// <summary>The palette handed to <c>activatePalette</c> (0x300 bytes), and the range last set.</summary>
extern VFX_RGB* paletteRgb;
extern int32_t globalEntries;
extern int32_t globalFirst;
extern uint32_t Networkframe;
extern uint32_t MP_Start_Time;
/// <summary>The registered window message the game answers to (another copy asking if it runs).</summary>
extern uint32_t uMessage;
/// <summary>
/// The pixels the screen port shows: the DIB section's bits in the original, the display's buffer
/// (<c>MCDisplay::Pixels</c>) in the port. <see cref="aLockScreen"/> points the screen port at it.
/// </summary>
/// <remarks>MCX.EXE @ 0x007ab104 (DAT_007ab104; the name is the port's)</remarks>
extern uint8_t* screenBits;
/// <summary>
/// The processor found by <see cref="aSystem::start"/>: 0 none (the game refuses to run), 1 Pentium, 2 Pentium
/// with MMX, 3 486 (no CPUID). Only the blitters' speed depended on it.
/// </summary>
/// <remarks>MCX.EXE @ 0x007ab10c</remarks>
extern int32_t Processor;
/// <summary>The mouse thread's lock (a CRITICAL_SECTION in the original).</summary>
extern std::recursive_mutex MouseCritSec;
extern volatile int InMouseCritSec;
extern int AndyFramerate;
extern int AG_mouseFrame;
extern int mouseThreadStarted;
/// <summary>Memory status for the debug display (a MEMORYSTATUS* in the original; opaque).</summary>
extern void* memoryStatus;
// Win32 handles of the original's GDI/window code, opaque in the port: WindowPlacement, bmi, LogicalPalette, backbm,
// holdpalette, OffScreenhOldBitmap, OffScreenBufferDC, OffScreenhDIBSection, DesktopDC, hPalette, ghWindow,
// backpbmi, thePalette. They are declared for bookkeeping only.
extern void* backbm;
extern void* holdpalette;
extern void* OffScreenhOldBitmap;
extern void* OffScreenBufferDC;
extern void* OffScreenhDIBSection;
extern void* DesktopDC;
extern void* hPalette;
extern void* ghWindow;
extern void* backpbmi;
extern void* thePalette;
