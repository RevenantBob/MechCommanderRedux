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

class MCGuiAnimation;
class MCGuiCallback;
class MCGuiFont;
class MCGuiObject;
class MCGuiOpeningSmackerWindow;
class MCGuiPort;
class MCGuiTimerManager;
class MCCamera;
class MCFidpMessage;
class MCFont;
class MCPacketFile;

/// <summary>The side an <see cref="MCGuiObject"/> slides out to when it hides (<see cref="MCGuiObject::HideMe"/>).</summary>
/// <remarks>The original's enumerator names were lost; the values are the original's (stored as a byte at +0x64).</remarks>
enum MCDirection : int32_t
{
    DIRECTION_LEFT = 0,
    DIRECTION_UP = 1,
    DIRECTION_RIGHT = 2,
    DIRECTION_DOWN = 3
};

/// <summary>
/// The mouse cursor shapes (<see cref="MCGuiSystem::SetCurrentCursor"/>). The enumerator names were lost; the values
/// run 0..0x12 (0xf..0x11 are offset by the interface's cursor set, 0x12 maps to shape 1).
/// </summary>
enum MCCursorType : int32_t;

/// <summary>A window's display state (<see cref="MCGuiObject::State"/>).</summary>
/// <remarks>The values are the original's (<c>normalize</c> = 0, <c>maximize</c> = 1, <c>iconize</c> = 2).</remarks>
enum MCGuiWindowState : int32_t
{
    aSTATE_NORMAL = 0,
    aSTATE_MAXIMIZED = 1,
    aSTATE_ICONIZED = 2
};

/// <summary>
/// An input or system event passed to <see cref="MCGuiObject::HandleEvent"/>. <c>translateMessage</c> builds them from
/// window messages, <c>aPostMessage</c> and the timers from a bare type.
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0x28 bytes (the size of the stack copies). Types seen: 1 left button
/// down, 3 left button down again (grab), 4 left button up, 6 right button up, 7 mouse move, 8 key up, 9 key down,
/// 10 character, 0xc paint, 0xd close (destroys the object), 0x10 left double click, 0x11 right double click, 0x12
/// broadcast (passed to every child), 0x13 timer (<see cref="Data"/> = the timer id), 0x15/0x16 spinner up/down
/// messages; values from 0x1400 (WM_USER + 0x1000) are posted messages.
/// </remarks>
class MCGuiEvent
{
public:
    /// <summary>Zeroes every field but <see cref="Data"/> and <see cref="LParam"/>.</summary>
    void Clear();

    /// <summary>What happened (see the class remarks).</summary>
    int32_t Type = 0;
    /// <summary>The object the event is aimed at (the one under the cursor), when the sender set it.</summary>
    MCGuiObject* Target = nullptr;
    /// <summary>Left mouse button held (MK_LBUTTON).</summary>
    uint8_t LeftButton = 0;
    /// <summary>Middle mouse button held (MK_MBUTTON).</summary>
    uint8_t MiddleButton = 0;
    /// <summary>Right mouse button held (MK_RBUTTON).</summary>
    uint8_t RightButton = 0;
    /// <summary>Alt held.</summary>
    uint8_t AltKey = 0;
    /// <summary>Ctrl held.</summary>
    uint8_t CtrlKey = 0;
    /// <summary>Shift held.</summary>
    uint8_t ShiftKey = 0;
    /// <summary>The character or virtual-key code of a key event (the message's wParam, low byte).</summary>
    uint8_t Key = 0;
    /// <summary>The key's scan code (bits 16..24 of the message's lParam).</summary>
    int16_t ScanCode = 0;
    /// <summary>The cursor position, in screen-window coordinates.</summary>
    int32_t X = 0;
    int32_t Y = 0;
    /// <summary>The message's wParam; the timer id of a timer event.</summary>
    int32_t Data = 0;
    /// <summary>The message's lParam.</summary>
    int32_t LParam = 0;
};

/// <summary>
/// Something to do when a button is pressed or a callback fires: a plain function, and/or a message posted to an
/// object (<c>aPostMessage(object, message)</c>).
/// </summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x10 bytes. Allocated from the GUI heap.</remarks>
class MCGuiCallback
{
public:
    MCGuiCallback();
    virtual ~MCGuiCallback();

    /// <summary>
    /// Runs the callback: calls <see cref="Exec"/>, then posts <see cref="Message"/> to <see cref="Object"/> when
    /// both are set.
    /// </summary>
    virtual void Execute(); // slot 0

    /// <summary>Makes the callback post <paramref name="msg"/> to <paramref name="obj"/>.</summary>
    virtual void SetMessage(MCGuiObject* obj, int32_t msg); // slot 1

    /// <summary>Clears the function and the message.</summary>
    void Destroy();

    /// <summary>Sets the function to call.</summary>
    void SetExec(void (*func)());

    /// <summary>The function to call, or null.</summary>
    void (*Exec)() = nullptr;
    /// <summary>The message to post, or 0.</summary>
    int32_t Message = 0;
    /// <summary>The object to post it to, or null.</summary>
    MCGuiObject* Object = nullptr;
};

/// <summary>
/// The base of every GUI element: a rectangle at (x, y) relative to its parent, with a port it draws into, a pane
/// on the screen it is copied to, an optional background picture, icon and animation, up to 255 children, and
/// optional paint and event routines. It can be maximized, iconized, dragged, and slid off the screen (hidden).
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> and <c>gui\asystem.h</c>, 0x4ac bytes. Allocated from the GUI heap.
/// Vtable 0x0077add4, 77 slots, declared below in slot order (<c>// slot N</c>). A few virtuals were unnamed in
/// MCX.EXE's symbols; their names here (<see cref="Width"/>, <see cref="BringToFront"/>, <see cref="Enter"/>,
/// <see cref="ShowGuiWindow"/>, <see cref="Right"/>) come from what they do and from the derived classes'
/// overrides (aFloatHelp::enter, aMenu::ShowGUIWindow).
/// </remarks>
class MCGuiObject
{
public:
    MCGuiObject();
    /// <summary>Calls <see cref="Destroy"/>.</summary>
    virtual ~MCGuiObject(); // slot 0
    MCGuiObject(const MCGuiObject&) = delete;
    MCGuiObject& operator=(const MCGuiObject&) = delete;

    /// <summary>
    /// Places the object at (<paramref name="xPos"/>, <paramref name="yPos"/>) with the given size, makes its port
    /// and its screen pane, and resets its state. <paramref name="name"/> is unused by aObject itself.
    /// </summary>
    /// <returns>0, 3 when out of memory, or the port's error.</returns>
    virtual int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name); // slot 1

    /// <summary>
    /// Frees the port, pane, background, icon, animation and drop targets, removes the object from its parent and
    /// its timers, and lets go of every system reference to it (grab, text focus, modal, current object).
    /// </summary>
    virtual void Destroy(); // slot 2

    /// <summary>The width.</summary>
    virtual int32_t Width();  // slot 3
    virtual int32_t Height(); // slot 4
    /// <summary>The x position, relative to the parent.</summary>
    virtual int32_t X(); // slot 5
    virtual int32_t Y(); // slot 6

    /// <summary>
    /// Moves the object to (<paramref name="xPos"/>, <paramref name="yPos"/>) relative to its parent, moves its pane
    /// and its children's. Unless <paramref name="temporary"/>, the position also becomes its home (where
    /// <see cref="HideMe"/> slides back to).
    /// </summary>
    virtual void MoveTo(int32_t xPos, int32_t yPos, int temporary = 0); // slot 7

    /// <summary>Resizes the object and its port (snapped to the 40-pixel grid when <see cref="GridAligned"/>).</summary>
    virtual void Resize(int32_t newWidth, int32_t newHeight); // slot 8

    /// <summary>The object's pane on the screen.</summary>
    virtual MCPane* Frame(); // slot 9

    /// <summary>Adds <paramref name="child"/> (at most 255), brings it to the front and places it.</summary>
    virtual void AddChild(MCGuiObject* child); // slot 10
    /// <summary>Removes <paramref name="child"/> from the child list and clears its parent.</summary>
    virtual void RemoveChild(MCGuiObject* child);   // slot 11
    virtual void SetParent(MCGuiObject* newParent); // slot 12
    virtual int32_t NumberOfChildren();             // slot 13
    /// <summary>The x position on the screen (the sum of the parents' positions).</summary>
    virtual int32_t GlobalX(); // slot 14
    virtual int32_t GlobalY(); // slot 15

    /// <summary>
    /// Draws the object into its port: the background (or the icon when iconized), the animation, the paint
    /// routine, then the children.
    /// </summary>
    virtual void Draw(); // slot 16

    /// <summary>The front-most visible object at (<paramref name="xPos"/>, <paramref name="yPos"/>): a child or this.</summary>
    virtual MCGuiObject* FindObject(int32_t xPos, int32_t yPos); // slot 17

    /// <summary>Whether the screen point is inside the pane (-1 or 0).</summary>
    virtual int PointInside(int32_t xPos, int32_t yPos); // slot 18
    /// <summary>Whether <paramref name="area"/> overlaps the pane (-1 or 0).</summary>
    virtual int RectIntersect(tagRECT area); // slot 19
    /// <summary>Whether the rectangle overlaps the pane (-1 or 0).</summary>
    virtual int RectIntersect(int32_t left, int32_t top, int32_t right, int32_t bottom); // slot 20

    /// <summary>
    /// The default handling: a click brings the window to the front; an iconized window is dragged by the mouse;
    /// the close event destroys it; the broadcast event goes to every child. Then the event routine, if any.
    /// </summary>
    virtual void HandleEvent(MCGuiEvent* event); // slot 21

    /// <summary>Loads background picture <paramref name="artPacket"/> of the art file.</summary>
    virtual int32_t SetBackground(int32_t artPacket); // slot 22
    /// <summary>Loads background picture <paramref name="fileName"/> (a TGA under <c>artPath</c>).</summary>
    virtual int32_t SetBackground(char* fileName); // slot 23

    /// <summary>Loads the animation (a VFX shape file under <c>artPath</c>).</summary>
    virtual int32_t SetAnimation(char* fileName); // slot 24
    virtual void StartAnimation();                // slot 25
    virtual void StopAnimation();                 // slot 26
    virtual MCGuiPort* Background();              // slot 27
    virtual MCGuiAnimation* Animation();          // slot 28
    virtual MCGuiAnimation* Icon();               // slot 29

    /// <summary>Changes the depth (removes and re-adds the object to its parent so it sorts again).</summary>
    virtual void SetDepth(int32_t newDepth); // slot 30
    /// <summary>Always null in MCX.EXE.</summary>
    virtual MCGuiObject* Children(); // slot 31

    /// <summary>
    /// Moves the object to the front of its parent's children of the same depth (the child list is kept sorted by
    /// depth). Without <paramref name="noShuffle"/>, grid-aligned siblings on the same 40-pixel cell are nudged
    /// aside.
    /// </summary>
    virtual void BringToFront(int noShuffle); // slot 32

    /// <summary>The front-most child of depth <paramref name="atDepth"/>, or null.</summary>
    virtual MCGuiObject* ForemostChild(int32_t atDepth); // slot 33
    virtual int32_t Depth();                             // slot 34
    /// <summary>Child <paramref name="index"/>, or null.</summary>
    virtual MCGuiObject* Child(int32_t index);          // slot 35
    virtual int32_t DragStartX();                       // slot 36
    virtual int32_t DragStartY();                       // slot 37
    virtual int Dragging();                             // slot 38
    virtual void StartDrag(int32_t xPos, int32_t yPos); // slot 39
    virtual void StopDrag();                            // slot 40

    /// <summary>The mouse came over the object. Does nothing in aObject.</summary>
    virtual void Enter() {} // slot 41
    /// <summary>The mouse left the object. Does nothing in aObject.</summary>
    virtual void Leave() {} // slot 42

    virtual int32_t BackColor(); // slot 43
    /// <summary>The colour the port is wiped to (0xff: none).</summary>
    virtual void SetBackColor(int32_t color); // slot 44
    /// <summary>Loads the icon (a VFX shape file); the iconized size becomes the icon's.</summary>
    virtual int32_t SetIcon(char* fileName); // slot 45
    /// <summary>
    /// Switches between normal, maximized and iconized (<see cref="MCGuiWindowState"/>), saving the current placement
    /// and restoring the new state's.
    /// </summary>
    virtual void SetState(int32_t newState); // slot 46
    virtual void Iconize();                  // slot 47
    virtual void Normalize();                // slot 48
    virtual void Maximize();                 // slot 49

    /// <summary>
    /// Shows the object: steps a pending slide (<see cref="HideMe"/>), copies the port to the pane (transparently
    /// when <see cref="Transparent"/>), then displays the children.
    /// </summary>
    virtual void Display(); // slot 50

    virtual void SetPaintRoutine(void (*routine)(MCGuiObject*));              // slot 51
    virtual void SetEventRoutine(void (*routine)(MCGuiObject*, MCGuiEvent*)); // slot 52
    /// <summary>Runs the paint routine, if any.</summary>
    virtual void Paint(); // slot 53
    /// <summary>The port's pixels (its bitmap's buffer), or null.</summary>
    virtual void* Ptr();       // slot 54
    virtual void StartModal(); // slot 55
    virtual void StopModal();  // slot 56
    virtual int32_t State();   // slot 57
    /// <summary>The port the object draws into.</summary>
    virtual MCGuiPort* Port(); // slot 58
    /// <summary>Makes the pane (and the children's) show <paramref name="newPort"/>'s bitmap.</summary>
    virtual void SetDisplayPort(MCGuiPort* newPort); // slot 59

    /// <summary>
    /// Draws a 3-line bevelled frame around the port (sunken when <paramref name="pushed"/>), after wiping it to the
    /// back colour when <paramref name="fill"/>.
    /// </summary>
    virtual void DrawFramed(int pushed, int fill); // slot 60
    /// <summary>Draws the outline of <paramref name="area"/> (calls slot 62).</summary>
    virtual void DrawBox(uint8_t color, tagRECT area); // slot 61
    /// <summary>Draws a rectangle outline in the port; -1 for a side means the port's edge.</summary>
    virtual void DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom); // slot 62

    /// <summary>Shows (<paramref name="show"/> nonzero) or hides the window from the object search.</summary>
    virtual void ShowGuiWindow(int show) { ShowWindow = show; } // slot 63
    /// <summary>The camera shown in the window; none for an aObject.</summary>
    virtual MCCamera* GetCamera() { return nullptr; } // slot 64
    virtual int IsShowing() { return ShowWindow; }    // slot 65
    /// <summary>
    /// Starts sliding the object off the screen in its <see cref="HideDirection"/> (<paramref name="hide"/>
    /// nonzero), or back to its home position.
    /// </summary>
    virtual void HideMe(int hide);                                                                            // slot 66
    virtual int IsHidden() { return Hidden; }                                                                 // slot 67
    virtual void SetHideDirection(MCDirection direction) { HideDirection = static_cast<uint8_t>(direction); } // slot 68
    /// <summary>Whether the port is copied to the screen with colour 0 transparent.</summary>
    virtual void SetTransparent(int on) { Transparent = on; } // slot 69
    virtual int32_t Left() { return X(); }                    // slot 70
    virtual int32_t Top() { return Y(); }                     // slot 71
    virtual int32_t Right() { return X() + Width(); }         // slot 72
    virtual int32_t Bottom() { return Y() + Height(); }       // slot 73
    /// <summary>Lets a movie window apply the movie's palette. Does nothing in aObject.</summary>
    virtual void CheckSmackerPalette() {}                           // slot 74
    virtual int32_t GetDropTargetCount() { return NumDropTargets; } // slot 75
    /// <summary>Drop target <paramref name="index"/> (a screen rectangle), or null.</summary>
    virtual tagRECT* GetDropTarget(int32_t index)
    {
        return index < NumDropTargets ? &DropTargets[index] : nullptr;
    } // slot 76

    /// <summary>
    /// Port-only (the original had no wheel): scrolls by <paramref name="steps"/> arrow clicks, negative up, positive
    /// down. translateMessage offers the wheel to the object under the mouse (at <paramref name="xPos"/>,
    /// <paramref name="yPos"/>), then to its parents, until one takes it. The position lets an object that draws a
    /// hidden child itself (the logistics weapon lists) pass the wheel on to that child.
    /// </summary>
    /// <returns>Whether the object scrolls (whether or not it could move); false passes the wheel to the parent.</returns>
    virtual bool MouseWheel(int32_t steps, int32_t xPos, int32_t yPos)
    {
        (void)steps;
        (void)xPos;
        (void)yPos;
        return false;
    }

    /// <summary>
    /// Port-only: whether the object draws itself in the frame pass instead of keeping a picture. Its port is then a
    /// view (<see cref="MCGuiPort::InitView"/>): each frame <see cref="Display"/> opens the view's scissor (its rectangle
    /// on the screen, cut to a clipping ancestor's: <see cref="ClipsChildren"/>), calls <see cref="Draw"/>, which
    /// renders the object from its state, and shuts it again; draws at any other time do nothing. Classes switch to this as their drawing moves out of
    /// event handlers and setters into <see cref="Draw"/>. A class decides for itself; a plain aObject draws itself
    /// once its owner calls <see cref="SetDrawsLive"/>.
    /// </summary>
    virtual bool DrawsLive() { return LiveDraw; }

    /// <summary>
    /// Port-only: makes this object draw itself in the frame pass (see <see cref="DrawsLive"/>): for an object
    /// whose picture holds only what <see cref="Draw"/> puts there (its background, animation and paint routine).
    /// Its port becomes a view now, or when <see cref="Init"/> makes it.
    /// </summary>
    void SetDrawsLive();

    /// <summary>
    /// Port-only: whether the children that draw themselves are cut to this object's rectangle. Not by default: the
    /// original copied each picture to the screen on its own, and children often lie outside their parent (a scroll
    /// thumb beside its text, a combo box's list below it). A pane that scrolls its children opts in.
    /// </summary>
    virtual bool ClipsChildren() { return false; }

    /// <summary>Wipes a rectangle of the port to <paramref name="color"/>.</summary>
    void FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color);

    /// <summary>Sets one pixel of the port.</summary>
    void SetBit(int32_t xPos, int32_t yPos, uint8_t color);

    /// <summary>Nonzero when the object snaps to the 40-pixel grid when resized or raised. Only cleared in MCX.EXE's aObject code.</summary>
    int32_t GridAligned = 0;
    /// <summary>The object's pane on the screen window (owned; allocated by <see cref="Init"/>).</summary>
    MCPane* FramePane = nullptr;
    int32_t WinWidth = 0;
    int32_t WinHeight = 0;
    /// <summary>The position relative to the parent.</summary>
    int32_t WinX = 0;
    int32_t WinY = 0;
    /// <summary>The depth: children are kept sorted by it (front-most last).</summary>
    int32_t WinDepth = 0;
    /// <summary>The placement while maximized (width, height, x, y).</summary>
    int32_t MaxWidth = 0;
    int32_t MaxHeight = 0;
    int32_t MaxX = 0;
    int32_t MaxY = 0;
    /// <summary>The placement while normal (width, height, x, y).</summary>
    int32_t NormalWidth = 0;
    int32_t NormalHeight = 0;
    int32_t NormalX = 0;
    int32_t NormalY = 0;
    /// <summary>The placement while iconized (the icon's size, and x, y).</summary>
    int32_t IconWidth = 0;
    int32_t IconHeight = 0;
    int32_t IconX = 0;
    int32_t IconY = 0;
    /// <summary>The <see cref="MCGuiWindowState"/>.</summary>
    int32_t WinState = 0;
    /// <summary>Nonzero while the object is shown and can be found (<see cref="IsShowing"/>); -1 after <see cref="Init"/>.</summary>
    int32_t ShowWindow = 0;
    /// <summary>What kind of object this is, for the interface's hit tests: -1 by default, 4 a camera view window, 7 a floating help.</summary>
    int16_t ObjectType = 0;
    /// <summary>Nonzero while hidden (slid off the screen).</summary>
    int32_t Hidden = 0;
    /// <summary>How far the object still has to slide (signed); 0 when not moving.</summary>
    int32_t HideOffset = 0;
    /// <summary>The <see cref="MCDirection"/> it slides out to (3 after <see cref="Init"/>).</summary>
    uint8_t HideDirection = 0;
    /// <summary>The home position the slide returns to.</summary>
    int32_t HomeX = 0;
    int32_t HomeY = 0;
    /// <summary>Nonzero when the port is copied transparently.</summary>
    int32_t Transparent = 0;
    /// <summary>The children, sorted by depth.</summary>
    MCGuiObject* ChildList[255] = {};
    int32_t NumChildren = 0;
    MCGuiObject* Parent = nullptr;
    /// <summary>The background picture, copied into the port by <see cref="Draw"/>.</summary>
    MCGuiPort* BackgroundPort = nullptr;
    /// <summary>The icon drawn while iconized.</summary>
    MCGuiAnimation* IconAnimation = nullptr;
    /// <summary>The animation drawn over the background while <see cref="Animating"/>.</summary>
    MCGuiAnimation* WindowAnimation = nullptr;
    /// <summary>Where a drag started.</summary>
    int32_t DragX = 0;
    int32_t DragY = 0;
    int32_t DragOn = 0;
    int32_t Animating = 0;
    /// <summary>The colour the port is wiped to; 0xff for none.</summary>
    int32_t BackgroundColor = 0;
    /// <summary>The port the object draws into (owned).</summary>
    MCGuiPort* DisplayPort = nullptr;
    void (*PaintRoutine)(MCGuiObject*) = nullptr;
    void (*EventRoutine)(MCGuiObject*, MCGuiEvent*) = nullptr;
    int32_t NumDropTargets = 0;
    /// <summary>The drop targets (owned; <see cref="Destroy"/> frees them with <c>delete[]</c>).</summary>
    tagRECT* DropTargets = nullptr;
    /// <summary>Port: set by <see cref="SetDrawsLive"/>.</summary>
    bool LiveDraw = false;

protected:
    /// <summary>
    /// Port: steps a pending slide (<see cref="HideMe"/>) by its whole offset, stopping it once the object is off the
    /// screen or back home. Part of <see cref="Display"/>.
    /// </summary>
    void SlideStep();

    /// <summary>
    /// Port: the frame pass of an object that <see cref="DrawsLive"/>: draws it into <paramref name="port"/>'s view
    /// over its pane (the view's pixel (0, <paramref name="scrollY"/>) at the pane's corner, for a port taller than
    /// the object), then displays the children unless <paramref name="displayChildren"/> is false. Without
    /// <paramref name="wipe"/>, an opaque object leaves what is under the parts it doesn't draw (the original copied
    /// pieces to the screen, not a whole picture).
    /// </summary>
    void DrawInFramePass(MCGuiPort* port, int32_t scrollY = 0, bool wipe = true, bool displayChildren = true);

    /// <summary>
    /// Port: whether <see cref="Draw"/> draws <paramref name="child"/> too: only a child with a picture, and not while
    /// this object draws in the frame pass (the children display themselves after it).
    /// </summary>
    bool DrawsChild(MCGuiObject* child);

    /// <summary>
    /// Port: the children part of a <see cref="Draw"/>: nothing in the frame pass (the children display themselves
    /// after it); otherwise, as the original's paint, each child with a picture draws into it (a child that draws
    /// itself is drawn in the frame pass only).
    /// </summary>
    void DrawChild(MCGuiObject* child);
};

/// <summary>A window holding up to two panes, tiled side by side (or stacked) or one at a time.</summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c> and <c>gui\asystem.h</c>, 0x4c0 bytes. Vtable: aObject's 77 slots, then
/// 77..89 below. The camera's main window (aMainWindow) and aEmptyTitleWindow derive from it.
/// </remarks>
class MCGuiHolderObject : public MCGuiObject
{
public:
    /// <summary>Like aObject::init (inlined), and clears the panes; no pane is active.</summary>
    int32_t Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) override;
    void Destroy() override;
    void Resize(int32_t newWidth, int32_t newHeight) override;
    /// <summary>Forgets <paramref name="oldChild"/> if it is a pane, then removes it.</summary>
    void RemoveChild(MCGuiObject* oldChild) override;
    void Display() override;
    /// <summary>Draws no frame.</summary>
    void DrawFramed(int, int) override {}
    /// <summary>Draws no box.</summary>
    void DrawBox(uint8_t, int32_t, int32_t, int32_t, int32_t) override {}
    using MCGuiObject::DrawBox;

    /// <summary>Tiles the two panes, or shows only the active one.</summary>
    virtual void SetTiled(int tiled);        // slot 77
    virtual int GetTiled() { return Tiled; } // slot 78
    /// <summary>Lays the panes out for the current mode.</summary>
    virtual void Retile(); // slot 79
    virtual void SetVertical(int on)
    {
        Vertical = on;
        Retile();
    } // slot 80

    virtual int GetVertical() { return Vertical; } // slot 81
    /// <summary>Adds <paramref name="pane"/> in the first free pane slot, as a child.</summary>
    virtual void AddPane(MCGuiObject* pane);    // slot 82
    virtual void RemovePane(MCGuiObject* pane); // slot 83
    /// <summary>Pane 0 or 1, or null.</summary>
    virtual MCGuiObject* GetPane(char index)
    {
        return (index >= 0 && index < 2) ? Panes[static_cast<int>(index)] : nullptr;
    } // slot 84

    virtual MCGuiObject* GetActivePane()
    {
        return ActivePane >= 0 ? Panes[static_cast<int>(ActivePane)] : nullptr;
    } // slot 85

    virtual MCGuiObject* GetInactivePane()
    {
        return ActivePane == 0 ? Panes[1] : (ActivePane == 1 ? Panes[0] : nullptr);
    } // slot 86

    /// <summary>0, 1, or -1 when none.</summary>
    virtual char GetActivePaneNumber() { return ActivePane; } // slot 87
    virtual void SetActivePane(MCGuiObject* pane);            // slot 88
    /// <summary>
    /// Makes pane <paramref name="index"/> active (0 always; 1 only when there is a second pane) and re-tiles.
    /// </summary>
    virtual void SetActivePaneNumber(char index); // slot 89

    MCGuiObject* Panes[2] = {};
    int32_t Tiled = 0;
    /// <summary>The active pane: 0, 1 or -1 (none).</summary>
    char ActivePane = -1;
    /// <summary>Nonzero to stack the tiled panes vertically.</summary>
    int32_t Vertical = 0;
};

/// <summary>A centred box with a line of text and an OK button (the version dialog).</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x4b0 bytes.</remarks>
class MCGuiMessageBox : public MCGuiObject
{
public:
    /// <summary>
    /// Sizes the box to <paramref name="text"/> in the white font, centres it on the screen, adds the OK button
    /// (which runs <c>DestroyVersion</c>) and writes the text.
    /// </summary>
    /// <returns>0, -3 without the white font, or the button's error.</returns>
    int32_t Init(uint8_t* text);
    using MCGuiObject::Init;

    void Destroy() override;
    /// <summary>Passes events inside the box to the button.</summary>
    void HandleEvent(MCGuiEvent* event) override;
    /// <summary>
    /// Port: the box, its text and its outline (the original wrote them into the picture once, in init).
    /// </summary>
    void Draw() override;
    /// <summary>Port: draws itself each frame from its text.</summary>
    bool DrawsLive() override { return true; }

    /// <summary>The OK button (an aButton).</summary>
    MCGuiObject* OkButton = nullptr;
    /// <summary>Port: the text shown.</summary>
    std::string Message;
};

/// <summary>A running GUI timer: sends a timer event (or a given event) to an object every interval.</summary>
/// <remarks>Original source: <c>gui\asystem.cpp</c>, 0x1c bytes (allocated from the GUI heap).</remarks>
struct MCGuiTimer
{
    /// <summary>The interval in milliseconds.</summary>
    uint32_t Interval = 0;
    /// <summary>When it last fired (ms, or scenario ms for a scenario-time timer).</summary>
    uint32_t LastTime = 0;
    MCGuiObject* Target = nullptr;
    /// <summary>The id passed back in the timer event's <c>data</c>.</summary>
    int16_t Id = 0;
    /// <summary>The event type to send instead of a timer event (then the timer fires once), or 0.</summary>
    int32_t EventType = 0;
    /// <summary>The <c>data</c> of that event.</summary>
    int32_t EventData = 0;
    /// <summary>Nonzero to count in scenario time instead of real time.</summary>
    int32_t UseScenarioTime = 0;
};

/// <summary>The GUI timers (at most 99), run by a system callback while any exist.</summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0x4b8 bytes. While a timer's event is being handled the list is locked:
/// removals are queued in <see cref="TimersToWhack"/> and done by <see cref="UnlockTimers"/>.
/// </remarks>
class MCGuiTimerManager
{
public:
    MCGuiTimerManager();
    /// <summary>Frees the timers.</summary>
    ~MCGuiTimerManager();

    /// <summary>Makes the callback that runs the timers (<c>TimerCallback</c>).</summary>
    int32_t Init();
    void Destroy();
    /// <summary>Adds a timer unless one with the same object, id, interval and event exists.</summary>
    int32_t AddUniqueTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                           int useScenarioTime);
    /// <summary>Adds a timer; the first one registers the system callback.</summary>
    /// <returns>0, or -1 when full or out of memory.</returns>
    int32_t AddTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                     int useScenarioTime);
    /// <summary>Removes every timer of <paramref name="target"/>.</summary>
    void RemoveTimers(MCGuiObject* target);
    void RemoveTimer(MCGuiObject* target, int16_t id);
    /// <summary>Removes timer number <paramref name="index"/>.</summary>
    void RemoveTimer(int32_t index);
    MCGuiTimer* GetTimer(int16_t index);
    MCGuiTimer* GetTimer(MCGuiObject* target, int16_t id);
    /// <summary>Locks the list while <paramref name="running"/>'s event is handled.</summary>
    void LockTimersExcept(MCGuiTimer* running);
    /// <summary>Unlocks the list and does the queued removals.</summary>
    void UnlockTimers();

    /// <summary>A queued removal: object and id, object and -1 (all its timers), or null and an index.</summary>
    struct TimerToWhack
    {
        MCGuiObject* Target = nullptr;
        int32_t Id = 0;
    };

    int32_t NumTimers = 0;
    int32_t NumTimersToWhack = 0;
    /// <summary>
    /// The timers. <see cref="AddTimer"/> accepts a 100th, which lands on <c>timersToWhack[0]</c>'s first word.
    /// </summary>
    MCGuiTimer* Timers[99] = {};
    TimerToWhack TimersToWhack[99] = {};
    /// <summary>The system callback that runs the timers.</summary>
    MCGuiCallback* RunTimersCallback = nullptr;
    int32_t Locked = 0;
    /// <summary>The timer whose event is being handled while locked (it may remove itself directly).</summary>
    MCGuiTimer* LockedExcept = nullptr;
};

/// <summary>
/// The application: the screen object and root of the window tree (<c>screenWindow</c> is its child), with the
/// display, the palette and its gamma, the mouse cursor, the per-frame callbacks, the object grabbing the mouse or
/// keyboard, the modal object and the timers. There is one, <c>application</c>.
/// </summary>
/// <remarks>
/// Original source: <c>gui\asystem.cpp</c>, 0xb0c bytes. Vtable 0x0077af08: aObject's with <see cref="Width"/> and
/// <see cref="Height"/> returning the screen's size. The DirectDraw and window handles are kept as opaque
/// placeholders so the offsets stay documented; the port presents through MCDisplay.
/// </remarks>
class MCGuiSystem : public MCGuiObject
{
public:
    /// <summary>The screen width.</summary>
    int32_t Width() override;
    int32_t Height() override;

    /// <summary>Opens the display at <paramref name="width"/> x <paramref name="height"/>, <paramref name="bitDepth"/> bits.</summary>
    int32_t StartupDirectDraw(int32_t width, int32_t height, int32_t bitDepth);
    /// <summary>Re-opens the display in another mode (for full-screen movies).</summary>
    int32_t ResetDirectDraw(int32_t width, int32_t height, int32_t bitDepth);
    int32_t ShutdownDirectDraw();
    void SetFlipToGdi();
    void ClearFlipToGdi();
    /// <summary>Does nothing in MCX.EXE.</summary>
    void FlipToGdi();
    /// <summary>Sets the scroll-trigger rectangle: (1, 1) to the screen's size less 4.</summary>
    void SetScrollRect();

    /// <summary>
    /// Starts the game: the window, the fonts, the palette, the display, the screen window, the timers,
    /// the interface and the mouse thread.
    /// </summary>
    /// <param name="instance">The HINSTANCE in the original; unused by the port.</param>
    /// <param name="prevInstance">The previous HINSTANCE in the original; unused by the port.</param>
    /// <returns>Nonzero when started.</returns>
    int Start(void* instance, void* prevInstance, char* commandLine, int showCommand, int16_t screenWidth,
              int16_t screenHeight);
    /// <summary>Shuts everything <see cref="Start"/> started down.</summary>
    void Stop();
    /// <summary>
    /// Plays Smacker movie <paramref name="fileName"/> in <paramref name="window"/> (or a new centred movie window),
    /// blocking the game when <paramref name="exclusive"/>.
    /// </summary>
    int32_t StartSmackerMovie(char* fileName, uint32_t flags, MCGuiObject* window, int exclusive);
    /// <summary>The main loop: pumps messages, runs the callbacks and redraws until the game quits.</summary>
    void Run();

    int32_t ScreenOffsetX();
    int32_t ScreenOffsetY();
    /// <summary>The window handle (an HWND in the original; opaque in the port).</summary>
    void* Window();
    void SetScreenOffsetX(int32_t offset);
    void SetScreenOffsetY(int32_t offset);

    /// <summary>Adds a callback run every frame (at most 98).</summary>
    /// <returns>0, 2 for null, 3 when full.</returns>
    int32_t AddCallback(MCGuiCallback* callback);
    /// <returns>0, 1 when not found, 2 for null.</returns>
    int32_t RemoveCallback(MCGuiCallback* callback);
    void SetModalObject(MCGuiObject* obj);
    /// <summary>The modal object.</summary>
    MCGuiObject* ModalObject() { return Modal; }
    void ClearModal();
    /// <summary>Gives <paramref name="obj"/> the mouse.</summary>
    void Grab(MCGuiObject* obj);
    /// <summary>Gives <paramref name="obj"/> the keyboard.</summary>
    void SetText(MCGuiObject* obj);
    /// <summary>Sets the object under the cursor.</summary>
    void SetCurrentObject(MCGuiObject* obj);
    /// <summary>Lets go of the mouse grab.</summary>
    void Release();
    /// <summary>Lets go of the keyboard focus.</summary>
    void ReleaseText();
    MCGuiObject* GrabbedObject();
    MCGuiObject* TextObject();
    MCGuiObject* CurrentObject();

    /// <summary>
    /// Sets <paramref name="count"/> palette entries from <paramref name="first"/> (clamped to 10..245 unless a movie
    /// plays), through the gamma table; <paramref name="sixBit"/> shifts 6-bit values up.
    /// </summary>
    int TweakDDPalette(int first, int count, MCVfxRgb* colors, int sixBit);
    /// <summary>Steps the gamma level (0..3) and re-applies the palette.</summary>
    void GammaCorrectCurrentPalette();
    void GammaCorrectCurrentPalette(int32_t level);
    /// <summary>Fades the palette to black.</summary>
    void FadeDownCurrentPalette();
    /// <summary>Sets the palette (or, with <paramref name="first"/> 0, remembers it in <c>paletteRgb</c>).</summary>
    void ActivatePalette(uint8_t* colors, int first, int count);
    void ActivatePaletteFromTga(char* fileName);
    void ActivatePaletteFromGif(char* fileName);
    void ActivateSmackerPalette(uint8_t* colors);

    int32_t AddTimer(MCGuiObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                     int useScenarioTime);
    int32_t AddUniqueTimer(MCGuiObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                           int useScenarioTime);
    void RemoveTimer(MCGuiObject* target, int16_t id);
    void RemoveTimers(MCGuiObject* target);
    /// <summary>Sets the mouse cursor (ignored while the cursor is hidden).</summary>
    void SetCurrentCursor(MCCursorType cursor);
    void SetCursorVisible(int show);

    /// <summary>IDirectDraw* in the original; the port presents through MCDisplay.</summary>
    void* DdObject = nullptr;
    /// <summary>The second DirectDraw interface in the original (released at shutdown); opaque.</summary>
    void* DdObject2 = nullptr;
    /// <summary>The primary DirectDraw surface in the original; opaque.</summary>
    void* DdPrimarySurface = nullptr;
    /// <summary>The back DirectDraw surface in the original; opaque.</summary>
    void* DdBackSurface = nullptr;
    /// <summary>The DirectDraw palette in the original (tweakDDPalette's SetEntries); opaque.</summary>
    void* DdPalette = nullptr;
    /// <summary>
    /// PREFS.CFG "PaletteCycle": cycleColors animates the palette when nonzero (-1 until the prefs are read; the
    /// mission sets 1 after a movie).
    /// </summary>
    int32_t PaletteCycle = 0;
    /// <summary>Nonzero after <see cref="SetFlipToGdi"/>.</summary>
    int32_t FlipToGdiRequested = 0;
    /// <summary>The current palette (8 bits per channel), before gamma.</summary>
    MCVfxRgb CurrentPalette[256] = {};
    /// <summary>The gamma level, 0..3.</summary>
    int32_t GammaLevel = 0;
    /// <summary>The text formatter the inventory blocks and the chat window share.</summary>
    MCSmuti TextFormatter;
    int32_t ScreenWidth = 0;
    int32_t ScreenHeight = 0;
    int32_t OffsetX = 0;
    int32_t OffsetY = 0;
    /// <summary>HWND in the original; opaque (the port's window belongs to MCDisplay).</summary>
    void* WindowHandle = nullptr;
    int32_t NumCallbacks = 0;
    /// <summary>The per-frame callbacks.</summary>
    MCGuiCallback* Callbacks[99] = {};
    /// <summary>The object holding the mouse.</summary>
    MCGuiObject* Grabbed = nullptr;
    /// <summary>The object holding the keyboard.</summary>
    MCGuiObject* TextFocus = nullptr;
    /// <summary>The object under the cursor.</summary>
    MCGuiObject* Current = nullptr;
    MCGuiObject* Modal = nullptr;
    MCCursorType CurrentCursor = static_cast<MCCursorType>(0);
    /// <summary>Nonzero while the cursor is hidden (<see cref="SetCursorVisible"/>).</summary>
    int32_t CursorHidden = 0;
    MCGuiTimerManager* TimerManager = nullptr;
    /// <summary>The exclusive movie window while one plays.</summary>
    MCGuiObject* SmackerWindow = nullptr;
    /// <summary>A second movie window, destroyed by run when its movie is over.</summary>
    MCGuiObject* SmackerWindow2 = nullptr;
    /// <summary>The opening movie's window.</summary>
    MCGuiOpeningSmackerWindow* OpeningSmackerWindow = nullptr;
    /// <summary>Where the mouse scrolls the map: outside (1, 1)..(width - 4, height - 4).</summary>
    tagRECT ScrollRect = {};
    /// <summary>The cursor shape drawn (-1 hidden).</summary>
    int32_t CursorShape = 0;
};

// Free functions of gui\asystem.cpp.

/// <summary>Makes an 8-bit DIB section for the windowed display (GDI in the original).</summary>
int32_t CreateDibSection(int32_t width, int32_t height, void** bitmapInfo, void** bitmap, uint8_t** bits);
/// <summary>A GDI palette from a GIF's colour table (an HPALETTE in the original).</summary>
void* CreatePaletteFromGif(char* fileName);
void* CreatePaletteFromRam(void* colors);
void* CreateSmackPaletteFromRam(void* colors);
/// <summary>Draws and displays the whole window tree.</summary>
void ARedrawScreen();
int32_t ALockScreen();
int32_t AUnlockScreen();
/// <summary>Sends an event of type <paramref name="message"/> to <paramref name="obj"/> at once.</summary>
void APostMessage(MCGuiObject* obj, int32_t message);
void TestMsgCallback(MCFidpMessage* message, void* data);
void SendAndReceiveTestMessages();
int StartMultiplayerGame(char* commandLine);
/// <summary>Reads the command line's switches.</summary>
void ParseCommandLine(char* commandLine);
/// <summary>Copies the next word of <paramref name="line"/> from <paramref name="pos"/> into <paramref name="word"/>.</summary>
int32_t NextCommandLineWord(char* line, int32_t pos, char* word, int32_t maxLength);
/// <summary>The program's entry: starts the application and runs it.</summary>
int RealWinMain(void* instance, void* prevInstance, char* commandLine, int showCommand);
/// <summary>The version box's OK button: destroys it.</summary>
void DestroyVersion();
/// <summary>Routes an event to the grabbed, text, modal or found object, and handles the global keys.</summary>
void HandleEvent(MCGuiEvent* event);
/// <summary>Whether the last keys typed spell <paramref name="code"/>.</summary>
int Cheat(char* code);
/// <summary>Turns a window message into an <see cref="MCGuiEvent"/> and handles it (the platform layer calls it).</summary>
int32_t TranslateMessage(void* window, uint32_t message, uint32_t wParam, int32_t lParam);
/// <summary>Scrolls the map when the cursor is at the screen's edge.</summary>
void ScrollScreen();
/// <summary>The window procedure (WM_DESTROY, WM_MOVE, WM_SIZE, WM_PAINT, activation, ...).</summary>
int32_t WindowProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam);
void InitWindowMode();
void InitFullScreen();
/// <summary>Sends mouse movement and button changes as events.</summary>
void CheckMouse();
/// <summary>The palette of art file <paramref name="fileName"/> (allocated).</summary>
MCVfxRgb* GetPaletteFromArt(char* fileName);
/// <summary>The timer manager's callback: fires the due timers.</summary>
void TimerCallback();
/// <summary>Where the cursor was when the current message was sent, in window coordinates.</summary>
tagPOINT GetMessageCursorLoc();

// Globals of gui\asystem.cpp.

/// <summary>The application.</summary>
extern MCGuiSystem* Application;
/// <summary>The top-level window every screen is a child of.</summary>
extern MCGuiObject* ScreenWindow;
/// <summary>The port of the whole screen.</summary>
extern MCGuiPort* ScreenPort;
/// <summary>The art packet file (art\art.pak) <c>aPort::init(long)</c> reads from.</summary>
extern MCPacketFile* ArtFile;
extern char* StartupPakFile;
extern MCGuiMessageBox* VersionDialog;
extern MCGuiObject* SmackWindowPointer;
extern MCGuiObject* FeatureScreen;
extern int FeatureScreenDone;
extern int EscapedSmackerMovie;
extern MCGuiCallback* MouseTrackerCallback;
/// <summary>The fonts loaded by <see cref="MCGuiSystem::Start"/>.</summary>
extern MCGuiFont* SystemFont;
extern MCGuiFont* BlackFont;
extern MCGuiFont* GreyFont;
extern MCGuiFont* WhiteFont;
extern MCGuiFont* RedFont;
extern MCGuiFont* GreenFont;
extern MCGuiFont* BlueFont;
extern MCGuiFont* DimFont;
extern MCGuiFont* YellowFont;
extern MCGuiFont* YellowDropFont;
extern MCGuiFont* BlueDropFont;
extern MCGuiFont* MedBlackFont;
extern MCGuiFont* MedGreyFont;
extern MCGuiFont* MedWhiteFont;
extern MCGuiFont* MedRedFont;
extern MCGuiFont* MedGreenFont;
extern MCGuiFont* MedBlueFont;
extern MCGuiFont* MedDimFont;
extern MCGuiFont* MedYellowFont;
extern MCGuiFont* LgBlackFont;
extern MCGuiFont* LgGreyFont;
extern MCGuiFont* LgWhiteFont;
extern MCGuiFont* LgRedFont;
extern MCGuiFont* LgGreenFont;
extern MCGuiFont* LgBlueFont;
extern MCGuiFont* LgDimFont;
extern MCGuiFont* LgYellowFont;
/// <summary>
/// The same fonts by colour and size: row = colour (0 black, 1 red, 2 yellow, 3 green, 4 blue, 5 grey, 6 white,
/// 7 dim, 8 yellow drop, 9 blue drop), column = small, medium, large. The scrolling text objects pick fonts from it.
/// </summary>
extern MCGuiFont* Fonts[10][3];
/// <summary>Nonzero while the game is paused, and while it asks the player something (quit, ...).</summary>
extern int GamePaused;
extern int GameAsked;
/// <summary>The engine font used for lines of text drawn in the world.</summary>
extern MCFont* LineFont;
extern int GWidth;
extern int GHeight;
extern int GBitDepth;
extern int GFullScreen;
/// <summary>Port-only: stretch the picture over the whole window instead of keeping 4:3 with bars (PREFS
/// "StretchToFit", read by systemInit).</summary>
extern int GStretchToFit;
/// <summary>Port-only: draw the cursor into the frame as the original did, instead of showing it as the system
/// cursor (PREFS "SoftwareCursor", read by systemInit).</summary>
extern int GSoftwareCursor;
/// <summary>Port-only: open the display's window hidden (the tests that run a mission).</summary>
extern int GHiddenWindow;
/// <summary>Port-only: the renderer asked for, an <c>MCRendererKind</c> (PREFS "Renderer", read by systemInit, then
/// the command line's <c>-renderer</c>).</summary>
extern int GRenderer;
/// <summary>Port-only: the renderer PREFS "Renderer" asks for (the preferences screen's choice, written back by
/// WritePrefs; it takes effect at the next start). <see cref="GRenderer"/> is what this run uses.</summary>
extern int GRendererPreference;
/// <summary>Port-only: draw the frame counter in the top-right corner (PREFS "ShowFps", read by systemInit, or the
/// command line's <c>-fps</c>).</summary>
extern int GShowFps;
/// <summary>Port-only: PREFS "ShowFps" as read (written back by WritePrefs, whatever the command line said).</summary>
extern int GShowFpsPreference;
/// <summary>Port-only: wait for the display's refresh when showing a frame (cleared by the command line's
/// <c>-novsync</c>).</summary>
extern int GVSync;
extern int ApplicationActive;
extern uint32_t StackSize;
extern uint32_t TopOfStack;
/// <summary>The gamma translation table (initialised data in MCX.EXE).</summary>
extern uint8_t GammaColorTranslation[256];
extern int AllowMagicWindowSwitching;
extern int32_t DisplayWidth;
extern int32_t DisplayHeight;

/// <summary>
/// Port-only: makes the screen the window's size in pixels (at least 640x480) when the window has changed: the
/// display's buffer, <c>gWidth</c>/<c>displayWidth</c>/<c>application->screenWidth</c>, the screen window, then the
/// original's 0x12 broadcast so the in-mission windows follow. Called when a scenario starts and every frame of one;
/// the 640x480 screens draw only parts of the buffer, so they never resize it.
/// </summary>
/// <returns>Whether the screen changed size.</returns>
bool MCFollowWindowSize();
extern int OldMouseX;
extern int OldMouseY;
extern float FrameRate;
/// <summary>Performance-counter times (LARGE_INTEGER in the original).</summary>
extern int64_t PerfStartTime;
extern int64_t PerfStopTime;
extern int64_t PrevStart;
extern int64_t CountsPerSecond;
/// <summary>The last cursor position of a drag (aObject::handleEvent).</summary>
extern int32_t LastX;
extern int32_t LastY;
extern int LeftMouseButtonDown;
extern int RightMouseButtonDown;
extern char AppName[];
extern char WindowTitle[];
extern char PaletteName[];
extern char* BackPtr;
/// <summary>The cheat codes and the ring buffer of keys typed.</summary>
extern char CheatFramegraph[];
extern char CheatBunnyStrike[];
extern char CheatHealAll[];
extern char CheatDeadEye[];
extern char CheatCantHitMe[];
extern char CheatGetSalvage[];
extern char CheatReveal[];
extern char CheatDuh[];
extern char CheatKey[128];
extern int CheatPointer;
/// <summary>The "can't hit me" cheat: home team movers take no hits outside multiplayer (the name is the port's).</summary>
extern uint32_t CantHitMe;
extern int CheatsOn;
extern int CantBlowSalvage;
extern int BunnyStrikesOn;
extern int Duh;
extern int RecordClicks;
extern int SavedPosition;
extern int LockFrameRate;
extern int LockActive;
extern int TakeScreenShot;
extern uint32_t ScrollWait;
extern int32_t DisplayProfileData;
extern char KeySetting;
extern int QueuePlayerOrders;
extern int ForceGatesClosed;
extern int DrawTerrainGrid;
/// <summary>The palette handed to <c>activatePalette</c> (0x300 bytes), and the range last set.</summary>
extern MCVfxRgb* PaletteRgb;
extern int32_t GlobalEntries;
extern int32_t GlobalFirst;
extern uint32_t Networkframe;
extern uint32_t MPStartTime;
/// <summary>The registered window message the game answers to (another copy asking if it runs).</summary>
extern uint32_t UMessage;
/// <summary>
/// The pixels the screen port shows: the DIB section's bits in the original, the display's buffer
/// (<c>MCDisplay::Pixels</c>) in the port. <see cref="ALockScreen"/> points the screen port at it.
/// </summary>
extern uint8_t* ScreenBits;
/// <summary>
/// The processor found by <see cref="MCGuiSystem::Start"/>: 0 none (the game refuses to run), 1 Pentium, 2 Pentium
/// with MMX, 3 486 (no CPUID). Only the blitters' speed depended on it.
/// </summary>
extern int32_t Processor;
/// <summary>The mouse thread's lock (a CRITICAL_SECTION in the original).</summary>
extern std::recursive_mutex MouseCritSec;
extern volatile int InMouseCritSec;
extern int AndyFramerate;
extern int AGMouseFrame;
extern int MouseThreadStarted;
/// <summary>Memory status for the debug display (a MEMORYSTATUS* in the original; opaque).</summary>
extern void* MemoryStatus;
// Win32 handles of the original's GDI/window code, opaque in the port: WindowPlacement, bmi, LogicalPalette, backbm,
// holdpalette, OffScreenhOldBitmap, OffScreenBufferDC, OffScreenhDIBSection, DesktopDC, hPalette, ghWindow,
// backpbmi, thePalette. They are declared for bookkeeping only.
extern void* Backbm;
extern void* Holdpalette;
extern void* OffScreenhOldBitmap;
extern void* OffScreenBufferDC;
extern void* OffScreenhDibSection;
extern void* DesktopDC;
extern void* HPalette;
extern void* GhWindow;
extern void* Backpbmi;
extern void* ThePalette;
