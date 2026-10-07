#include "stdafx.h"
#include "gui/asystem.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/font.h"
#include "gameos/soundrenderer.h"
#include "gui/aanim.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/awindow.h"
#include "gui/mchwcursor.h"
#include "gui/updisp.h"
#include "iface/icallbk.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/ficommonnetwork.h"
#include "linkup/sessionmanager.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/misslog.h"
#include "main/honorb.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"
#include "platform/MCCursor.h"
#include "platform/MCBlockStore.h"
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCFrameLog.h"
#include "platform/MCInput.h"
#include "platform/MCRenderer.h"
#include "platform/MCSmacker.h"
#include "platform/MCWin32Defs.h"

MCGuiSystem* Application = nullptr;
MCGuiObject* ScreenWindow = nullptr;
MCGuiPort* ScreenPort = nullptr;
MCPacketFile* ArtFile = nullptr;
char* StartupPakFile = nullptr;
MCGuiMessageBox* VersionDialog = nullptr;
MCGuiObject* SmackWindowPointer = nullptr;
MCGuiObject* FeatureScreen = nullptr;
int FeatureScreenDone = 0;
int EscapedSmackerMovie = 0;
MCGuiCallback* MouseTrackerCallback = nullptr;
MCGuiFont* SystemFont = nullptr;
MCGuiFont* BlackFont = nullptr;
MCGuiFont* GreyFont = nullptr;
MCGuiFont* WhiteFont = nullptr;
MCGuiFont* RedFont = nullptr;
MCGuiFont* GreenFont = nullptr;
MCGuiFont* BlueFont = nullptr;
MCGuiFont* DimFont = nullptr;
MCGuiFont* YellowFont = nullptr;
MCGuiFont* YellowDropFont = nullptr;
MCGuiFont* BlueDropFont = nullptr;
MCGuiFont* MedBlackFont = nullptr;
MCGuiFont* MedGreyFont = nullptr;
MCGuiFont* MedWhiteFont = nullptr;
MCGuiFont* MedRedFont = nullptr;
MCGuiFont* MedGreenFont = nullptr;
MCGuiFont* MedBlueFont = nullptr;
MCGuiFont* MedDimFont = nullptr;
MCGuiFont* MedYellowFont = nullptr;
MCGuiFont* LgBlackFont = nullptr;
MCGuiFont* LgGreyFont = nullptr;
MCGuiFont* LgWhiteFont = nullptr;
MCGuiFont* LgRedFont = nullptr;
MCGuiFont* LgGreenFont = nullptr;
MCGuiFont* LgBlueFont = nullptr;
MCGuiFont* LgDimFont = nullptr;
MCGuiFont* LgYellowFont = nullptr;
MCGuiFont* Fonts[10][3] = {};
int GamePaused = 0;
int GameAsked = 0;
MCFont* LineFont = nullptr;
int GWidth = 640;
int GHeight = 480;
int GBitDepth = 8;
int GFullScreen = 0;
int GStretchToFit = 0;
int GSoftwareCursor = 0;
int GHiddenWindow = 0;
int GRenderer = 0;
int GRendererPreference = 0;
int GShowFps = 0;
int GShowFpsPreference = 0;
int GVSync = 1;
int ApplicationActive = -1;
uint32_t StackSize = 0x100000;
uint32_t TopOfStack = 0;
uint8_t GammaColorTranslation[256] = {
    0,   6,   10,  13,  16,  19,  21,  23,  25,  27,  29,  31,  33,  35,  37,  39,  40,  42,  44,  45,  47,  48,
    50,  51,  53,  54,  56,  57,  58,  60,  61,  63,  64,  65,  67,  68,  69,  70,  72,  73,  74,  75,  77,  78,
    79,  80,  81,  83,  84,  85,  86,  87,  88,  89,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 103,
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125,
    125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 138, 139, 140, 141, 142, 143, 144, 145,
    146, 146, 147, 148, 149, 150, 151, 152, 153, 153, 154, 155, 156, 157, 158, 159, 159, 160, 161, 162, 163, 164,
    164, 165, 166, 167, 168, 169, 169, 170, 171, 172, 173, 173, 174, 175, 176, 177, 177, 178, 179, 180, 181, 181,
    182, 183, 184, 185, 185, 186, 187, 188, 188, 189, 190, 191, 192, 192, 193, 194, 195, 195, 196, 197, 198, 198,
    199, 200, 201, 201, 202, 203, 204, 204, 205, 206, 207, 207, 208, 209, 210, 210, 211, 212, 213, 213, 214, 215,
    215, 216, 217, 218, 218, 219, 220, 220, 221, 222, 223, 223, 224, 225, 225, 226, 227, 228, 228, 229, 230, 230,
    231, 232, 232, 233, 234, 235, 235, 236, 237, 237, 238, 239, 239, 240, 241, 241, 242, 243, 244, 244, 245, 246,
    246, 247, 248, 248, 249, 250, 250, 251, 252, 252, 253, 254, 254, 255};
int AllowMagicWindowSwitching = -1;
int32_t DisplayWidth = 0;
int32_t DisplayHeight = 0;
int OldMouseX = 0;
int OldMouseY = 0;
float FrameRate = 0.0f;
int64_t PerfStartTime = 0;
int64_t PerfStopTime = 0;
int64_t PrevStart = 0;
int64_t CountsPerSecond = 0;
int32_t LastX = 0;
int32_t LastY = 0;
int LeftMouseButtonDown = 0;
int RightMouseButtonDown = 0;
// appName and WindowTitle are 0x400 bytes (0x007aa9a0..0x007aada0 and 0x007f050c..0x007f090c), paletteName 80.
char AppName[0x400] = {};
char WindowTitle[0x400] = {};
char PaletteName[80] = {};
char* BackPtr = nullptr;
// The cheat codes: a length byte, then the letters plus 0x32 (Cheat subtracts it). Cheat_CantHitMe's length (5) is
// one short of its six letters, so only the first five count.
char CheatFramegraph[12] = {'\x0a', '\x98', '\xa4', '\x93', '\x9f', '\x97', '\x99', '\xa4', '\x93', '\xa2', '\x9a'};
char CheatBunnyStrike[12] = {'\x09', '\x9e', '\xa1', '\xa4', '\x96', '\x94', '\xa7', '\xa0', '\xa0', '\xab'};
char CheatHealAll[8] = {'\x06', '\x9e', '\xa1', '\xa4', '\xa4', '\x9b', '\x97'};
char CheatDeadEye[8] = {'\x07', '\x96', '\x97', '\x93', '\x96', '\x97', '\xab', '\x97'};
char CheatCantHitMe[8] = {'\x05', '\xa1', '\xa5', '\x9f', '\x9b', '\xa7', '\x9f'};
char CheatGetSalvage[20] = {'\x12', '\x99', '\x9e', '\x97', '\xa0', '\xa0', '\xa4', '\xa1', '\x95', '\x9d',
                            '\xa5', '\xa6', '\x9a', '\x97', '\x9a', '\xa1', '\xa7', '\xa5', '\x97'};
char CheatReveal[28] = {'\x18', '\x9f', '\x9b', '\xa0', '\x97', '\x97', '\xab', '\x97', '\xa5',
                        '\x9a', '\x93', '\xa8', '\x97', '\xa5', '\x97', '\x97', '\xa0', '\xa6',
                        '\x9a', '\x97', '\x99', '\x9e', '\xa1', '\xa4', '\xab'};
char CheatDuh[4] = {'\x03', '\x96', '\xa7', '\x9a'};
char CheatKey[128] = {};
int CheatPointer = 0;
uint32_t CantHitMe = 0;
int CheatsOn = 0;
int CantBlowSalvage = 0;
int BunnyStrikesOn = 0;
int Duh = 0;
int RecordClicks = 0;
int SavedPosition = 0;
int LockFrameRate = 0;
int LockActive = 0;
int TakeScreenShot = 0;
uint32_t ScrollWait = 0;
int32_t DisplayProfileData = 0;
char KeySetting = 0;
int QueuePlayerOrders = 0;
int ForceGatesClosed = 0;
int DrawTerrainGrid = 0;
MCVfxRgb* PaletteRgb = nullptr;
int32_t GlobalEntries = 0;
int32_t GlobalFirst = 0;
uint32_t Networkframe = 0;
uint32_t MPStartTime = 0;
uint32_t UMessage = 0;
std::recursive_mutex MouseCritSec;
volatile int InMouseCritSec = 0;
int AndyFramerate = 0;
int AGMouseFrame = 0;
int MouseThreadStarted = 0;
void* MemoryStatus = nullptr;
void* Backbm = nullptr;
void* Holdpalette = nullptr;
void* OffScreenhOldBitmap = nullptr;
void* OffScreenBufferDC = nullptr;
void* OffScreenhDibSection = nullptr;
void* DesktopDC = nullptr;
void* HPalette = nullptr;
void* GhWindow = nullptr;
void* Backpbmi = nullptr;
void* ThePalette = nullptr;
uint8_t* ScreenBits = nullptr;
int32_t Processor = 0;

namespace
{
    /// <summary>
    /// Set once the display is up (DirectDraw or the DIB section in the original): palette changes and repaints wait
    /// for it (0x007ab108).
    /// </summary>
    int DisplayReady = 0;

    /// <summary>
    /// A byte the window procedure tests before selecting the GDI palette on repaint and activation; nothing in
    /// MCX.EXE sets it (0x007aa994).
    /// </summary>
    uint8_t KeepDesktopPalette = 0;

    /// <summary>Two windows <see cref="MCGuiSystem::Stop"/> destroys; nothing in MCX.EXE sets them (0x007ab134/138).</summary>
    MCGuiObject* StopWindow1 = nullptr;
    MCGuiObject* StopWindow2 = nullptr;

    /// <summary>
    /// The palette as shown: the GDI LOGPALETTE's entries (0x007aa3c4) and the DIB colour table's (0x007a9fb8)
    /// in the original, the colours handed to the display in the port. <see cref="MCGuiSystem::CurrentPalette"/> through
    /// the gamma table.
    /// </summary>
    MCVfxRgb LogicalPalette[256] = {};

    /// <summary>The display (DirectDraw's objects and the window in the original).</summary>
    std::unique_ptr<MCDisplay> GameDisplay;

    /// <summary>Hands <paramref name="count"/> shown colours from <paramref name="first"/> to the display.</summary>
    void ShowPalette(int first, int count)
    {
        if (MCDisplay* display = MCInput::Display())
        {
            display->SetPalette(first, count, &LogicalPalette[first]);
        }
    }

    /// <summary>Takes the mouse thread's lock when the thread runs (the original's EnterCriticalSection pairs).</summary>
    void LockMouse()
    {
        if (MouseThreadStarted != 0)
        {
            MouseCritSec.lock();
        }
    }

    void UnlockMouse()
    {
        if (MouseThreadStarted != 0)
        {
            MouseCritSec.unlock();
        }
    }

    /// <summary>Ends a movie window: stops the movie, destroys and deletes the window.</summary>
    void CloseMovieWindow(MCGuiObject*& window)
    {
        static_cast<MCGuiSmackerWindow*>(window)->EndSmackerMovie();
        window->Destroy();
        delete window;
        window = nullptr;
    }

    /// <summary>
    /// The table <c>cursorShapes</c> points at: one slot per cursor shape id. 128 slots are kept, since the cursor
    /// ids the game sets are indexes into it.
    /// </summary>
    std::array<uint8_t*, 128> CursorShapeTable = {};

    /// <summary>Owns the cursor shapes (registered with the renderers) until <see cref="MCGuiSystem::Stop"/>.</summary>
    MCBlockStore CursorShapeBlocks;

    /// <summary>Frees a font loaded by <see cref="MCGuiSystem::Start"/>.</summary>
    void DeleteFont(MCGuiFont*& font)
    {
        if (font != nullptr)
        {
            font->Destroy();
            delete font;
            font = nullptr;
        }
    }

    /// <summary>Loads a font for <see cref="MCGuiSystem::Start"/>.</summary>
    MCGuiFont* LoadFont(const char* fileName)
    {
        MCGuiFont* font = new (std::nothrow) MCGuiFont;
        font->Init(const_cast<char*>(fileName));
        return font;
    }

    /// <summary>
    /// Reads a TGA's colour map (at byte 0x12, blue-green-red, 8 bits) into a 6-bit VFX palette, as
    /// <see cref="GetPaletteFromArt"/> and <see cref="MCGuiSystem::ActivatePaletteFromTga"/> do.
    /// </summary>
    void TgaColorMapToPalette(const uint8_t* tga, MCVfxRgb* palette)
    {
        const uint8_t* entry = tga + 0x12;

        for (int i = 0; i < 256; i++, entry += 3)
        {
            palette[i].R = static_cast<uint8_t>(entry[2] >> 2);
            palette[i].G = static_cast<uint8_t>(entry[1] >> 2);
            palette[i].B = static_cast<uint8_t>(entry[0] >> 2);
        }
    }

    /// <summary>Whether a key is held (GetAsyncKeyState's top bit).</summary>
    bool KeyHeld(int vk)
    {
        return (MCInput::GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    /// <summary>
    /// The scissors of the objects drawing in the frame pass around the one displaying now (innermost last), with the
    /// window each is on: a child that draws itself is cut to its nearest such ancestor's.
    /// </summary>
    std::vector<std::pair<const MCWindow*, MCRect>> ViewClips;

    /// <summary>The object drawing in the frame pass right now (its view open), or null.</summary>
    MCGuiObject* DrawingLive = nullptr;

    /// <summary>The test message <see cref="SendAndReceiveTestMessages"/> sends: a header, the frame and a count.</summary>
#pragma pack(push, 1)
    struct MCTestMessage
    {
        MCFIGuaranteedMessageHeader Header;
        uint32_t Frame = 0;
        uint32_t Index = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(MCTestMessage) == 0x10);
}

// aObject's inline virtuals from gui\asystem.h.

auto MCGuiObject::DrawBox(uint8_t color, tagRECT area) -> void
{
    DrawBox(color, area.left, area.top, area.right, area.bottom);
}

// aSystem's DirectDraw layer: the display in the port.

auto MCGuiSystem::StartupDirectDraw(int32_t width, int32_t height, int32_t bitDepth) -> int32_t
{
    // Port: the original made the DirectDraw object (exclusive full screen, a primary surface and its palette) or,
    // in a window, a GDI palette from the system palette and an 8-bit DIB section to BitBlt from. The port opens
    // the SDL display, whose 8-bit buffer the game draws into.
    // Port: the screen is the window's size in pixels (at least 640x480), not the mode asked for (PREFS
    // "Resolution"); a scenario draws on all of it and follows the window (MCFollowWindowSize), the 640x480 screens
    // use its top-left corner.
    (void)bitDepth;
    (void)width;
    (void)height;
    MCDisplayOptions options;
    options.Title = AppName;
    options.Width = 640;
    options.Height = 480;
    options.FollowWindow = true;
    options.Fullscreen = GFullScreen != 0;
    options.Stretch = GStretchToFit != 0;
    options.Hidden = GHiddenWindow != 0;
    options.Renderer = static_cast<MCRendererKind>(GRenderer);
    options.VSync = GVSync != 0;
    auto display = MCDisplay::Create(options);

    if (!display)
    {
        Fatal(0, "Cannot initialize DirectDraw.", display.error().c_str());
    }

    GameDisplay = std::move(*display);
    GWidth = GameDisplay->Width();
    GHeight = GameDisplay->Height();
    DisplayWidth = GWidth;
    DisplayHeight = GHeight;
    ScreenWidth = GWidth;
    ScreenHeight = GHeight;
    DdObject = GameDisplay.get();
    MCInput::Attach(GameDisplay.get());
    ScreenBits = GameDisplay->Pixels();
    ShowPalette(0, 256);
    DisplayReady = 1;
    return 0;
}

auto MCGuiSystem::ResetDirectDraw(int32_t width, int32_t height, int32_t bitDepth) -> int32_t
{
    // Port: the original released and remade the surfaces in the new mode (full screen or windowed), restored the
    // window style and re-attached the palette. The port switches the display and, for another size, its buffer.
    GBitDepth = bitDepth;
    GWidth = width;
    GHeight = height;

    if (GameDisplay == nullptr)
    {
        return StartupDirectDraw(width, height, bitDepth);
    }

    GameDisplay->SetFullscreen(GFullScreen != 0);

    if (width != GameDisplay->Width() || height != GameDisplay->Height())
    {
        auto resized = GameDisplay->SetLogicalSize(width, height);

        if (!resized)
        {
            Fatal(0, " Unable to Set Display Mode ", resized.error().c_str());
        }

        ScreenBits = GameDisplay->Pixels();

        // Port fix: the screen port keeps pointing at the display's buffer, which the resize moved.
        if (LockActive != 0 && ScreenPort != nullptr && ScreenPort->Bitmap() != nullptr)
        {
            ScreenPort->Bitmap()->Buffer = ScreenBits;
        }
    }

    MCInput::RefreshMouseArea();
    ShowPalette(0, 256);
    DisplayReady = 1;
    return 0;
}

auto MCFollowWindowSize() -> bool
{
    MCDisplay* display = GameDisplay.get();

    if (display == nullptr || !display->FollowsWindow() || Application == nullptr)
    {
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;
    display->WindowScreenSize(width, height);

    if (width == display->Width() && height == display->Height())
    {
        return false;
    }

    auto resized = display->SetLogicalSize(width, height);

    if (!resized)
    {
        Fatal(0, " Unable to Set Display Mode ", resized.error().c_str());
    }

    GWidth = width;
    GHeight = height;
    DisplayWidth = width;
    DisplayHeight = height;
    Application->ScreenWidth = width;
    Application->ScreenHeight = height;
    ScreenBits = display->Pixels();

    if (ScreenPort != nullptr && ScreenPort->Bitmap() != nullptr)
    {
        ScreenPort->Resize(width, height);
        ScreenPort->Bitmap()->Buffer = ScreenBits;
    }

    MCInput::RefreshMouseArea();

    if (ScreenWindow != nullptr)
    {
        ScreenWindow->Resize(width, height);

        if (MainHolder != nullptr)
        {
            // The original's resolution-change broadcast (nothing in MCX.EXE sends it): the main window takes the
            // screen's size and re-tiles its panes, the mech bar goes back to the bottom.
            MCGuiEvent event;
            event.Clear();
            event.Type = 0x12;
            ScreenWindow->HandleEvent(&event);
        }
        else if (TheInterface != nullptr && TheInterface->MechBar != nullptr)
        {
            // Before the scenario's windows exist (StartScenario after the window was resized in the menus),
            // aMechBar::handleEvent would pass the event to mainHolder's active pane: just move the bar down.
            MCMechBar* bar = TheInterface->MechBar;
            bar->MoveTo(1, Application->Height() - bar->Height() - 1, 0);
        }
    }

    return true;
}

auto MCGuiSystem::ShutdownDirectDraw() -> int32_t
{
    MCCursor::Shutdown();
    MCInput::Attach(nullptr);
    GameDisplay.reset();
    DdObject = nullptr;
    DdObject2 = nullptr;
    ScreenBits = nullptr;
    OffScreenBufferDC = nullptr;
    OffScreenhDibSection = nullptr;
    OffScreenhOldBitmap = nullptr;
    DisplayReady = 0;
    return 0;
}

auto MCGuiSystem::SetFlipToGdi() -> void
{
    FlipToGdiRequested = -1;
}

auto MCGuiSystem::ClearFlipToGdi() -> void
{
    FlipToGdiRequested = 0;
}

auto MCGuiSystem::FlipToGdi() -> void
{
}

auto MCGuiSystem::SetScrollRect() -> void
{
    ScrollRect.left = 1;
    ScrollRect.right = Width() - 4;
    ScrollRect.top = 1;
    ScrollRect.bottom = Height() - 4;
}

// aObject.

MCGuiObject::MCGuiObject()
{
    FramePane = nullptr;
    BackgroundPort = nullptr;
    Animating = 0;
    DisplayPort = nullptr;
    GridAligned = 0;
    ObjectType = 0;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    NumChildren = 0;
    Parent = nullptr;
    WindowAnimation = nullptr;
    IconAnimation = nullptr;
    DropTargets = nullptr;
    NumDropTargets = 0;
}

MCGuiObject::~MCGuiObject()
{
    Destroy();
}

auto MCGuiObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)name;
    WinHeight = height;
    MaxHeight = height;
    NormalHeight = height;
    IconHeight = height;
    WinWidth = width;
    WinX = xPos;
    WinY = yPos;
    MaxWidth = width;
    MaxX = xPos;
    MaxY = yPos;
    NormalWidth = width;
    NormalX = xPos;
    NormalY = yPos;
    IconWidth = width;
    IconX = xPos;
    IconY = yPos;
    HideOffset = 0;
    HomeX = xPos;
    HomeY = yPos;
    WinState = aSTATE_NORMAL;
    ShowWindow = -1;
    DragOn = 0;
    Transparent = 0;
    BackgroundColor = 0xff;
    NumDropTargets = 0;

    if (DisplayPort != nullptr)
    {
        DisplayPort->Destroy();
        delete DisplayPort;
        DisplayPort = nullptr;
    }

    DisplayPort = new MCGuiPort;
    const int32_t result = DrawsLive() ? DisplayPort->InitView(width, height) : DisplayPort->Init(width, height);

    if (result != 0)
    {
        return result;
    }

    if (FramePane != nullptr)
    {
        delete FramePane;
        FramePane = nullptr;
    }

    FramePane = new (std::nothrow) MCPane;

    if (FramePane == nullptr)
    {
        return 3;
    }

    FramePane->Window = ScreenPort->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    FramePane->Y1 = yPos + height;
    Hidden = 0;
    HideDirection = DIRECTION_DOWN;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    NumChildren = 0;
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation = nullptr;
    Animating = 0;
    IconAnimation = nullptr;
    ObjectType = -1;
    return 0;
}

auto MCGuiObject::Destroy() -> void
{
    Application->RemoveTimers(this);

    if (DisplayPort != nullptr)
    {
        DisplayPort->Destroy();
        delete DisplayPort;
        DisplayPort = nullptr;
    }

    if (FramePane != nullptr)
    {
        delete FramePane;
        FramePane = nullptr;
    }

    if (BackgroundPort != nullptr)
    {
        BackgroundPort->Destroy();
        delete BackgroundPort;
        BackgroundPort = nullptr;
    }

    if (IconAnimation != nullptr)
    {
        IconAnimation->Destroy();
        delete IconAnimation;
        IconAnimation = nullptr;
    }

    if (WindowAnimation != nullptr)
    {
        WindowAnimation->Destroy();
        delete WindowAnimation;
        WindowAnimation = nullptr;
    }

    if (DropTargets != nullptr)
    {
        delete[] DropTargets;
        DropTargets = nullptr;
    }

    Assert(NumChildren == 0, 0, " Number of Children NOT Zero ");

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    Parent = nullptr;
    Animating = 0;

    if (Application->GrabbedObject() == this)
    {
        Application->Release();
    }

    if (Application->TextObject() == this)
    {
        Application->ReleaseText();
    }

    if (Application->ModalObject() == this)
    {
        Application->ClearModal();
    }

    if (Application->CurrentObject() == this)
    {
        const MCPoint cursor = MCInput::GetCursorPos();
        Application->SetCurrentObject(ScreenWindow->FindObject(cursor.x, cursor.y));
    }
}

auto MCGuiObject::SetDisplayPort(MCGuiPort* newPort) -> void
{
    FramePane->Window = newPort->Bitmap();

    for (int32_t i = 0; i < NumChildren; i++)
    {
        ChildList[i]->SetDisplayPort(newPort);
    }
}

auto MCGuiObject::PointInside(int32_t xPos, int32_t yPos) -> int
{
    if (FramePane->X0 <= xPos && xPos <= FramePane->X1 && FramePane->Y0 <= yPos && yPos <= FramePane->Y1)
    {
        return -1;
    }

    return 0;
}

auto MCGuiObject::RectIntersect(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int
{
    if (FramePane->X0 < right && left < FramePane->X1 && FramePane->Y0 < bottom && top < FramePane->Y1)
    {
        return -1;
    }

    return 0;
}

auto MCGuiObject::RectIntersect(tagRECT area) -> int
{
    if (FramePane->X0 < area.right && area.left < FramePane->X1 && FramePane->Y0 < area.bottom &&
        area.top < FramePane->Y1)
    {
        return -1;
    }

    return 0;
}

auto MCGuiObject::SetPaintRoutine(void (*routine)(MCGuiObject*)) -> void
{
    PaintRoutine = routine;
}

auto MCGuiObject::SetEventRoutine(void (*routine)(MCGuiObject*, MCGuiEvent*)) -> void
{
    EventRoutine = routine;
}

auto MCGuiObject::Paint() -> void
{
    if (PaintRoutine != nullptr)
    {
        PaintRoutine(this);
    }
}

auto MCGuiObject::FindObject(int32_t xPos, int32_t yPos) -> MCGuiObject*
{
    if (WinState != aSTATE_ICONIZED)
    {
        if (ShowWindow == 0)
        {
            return nullptr;
        }

        for (int32_t i = NumChildren; i > 0; i--)
        {
            MCGuiObject* found = ChildList[i - 1]->FindObject(xPos, yPos);

            if (found != nullptr)
            {
                return found;
            }
        }
    }

    if (ShowWindow != 0 && FramePane != nullptr && PointInside(xPos, yPos) != 0)
    {
        return this;
    }

    return nullptr;
}

auto MCGuiObject::Children() -> MCGuiObject*
{
    return nullptr;
}

auto MCGuiObject::SetParent(MCGuiObject* newParent) -> void
{
    Parent = newParent;
}

auto MCGuiObject::SetDepth(int32_t newDepth) -> void
{
    MCGuiObject* owner = Parent;

    if (owner != nullptr)
    {
        owner->RemoveChild(this);
    }

    WinDepth = newDepth;

    if (owner != nullptr)
    {
        owner->AddChild(this);
    }
}

auto MCGuiObject::Depth() -> int32_t
{
    return WinDepth;
}

auto MCGuiObject::StartAnimation() -> void
{
    Animating = -1;
}

auto MCGuiObject::StopAnimation() -> void
{
    Animating = 0;
}

auto MCGuiObject::StartModal() -> void
{
    Application->SetModalObject(this);
}

auto MCGuiObject::StopModal() -> void
{
    Application->ClearModal();
}

auto MCGuiObject::SetBackColor(int32_t color) -> void
{
    BackgroundColor = color;
}

auto MCGuiObject::BackColor() -> int32_t
{
    return BackgroundColor;
}

auto MCGuiObject::DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
{
    if (left == -1)
    {
        left = 0;
    }

    if (top == -1)
    {
        top = 0;
    }

    if (right == -1)
    {
        right = Width() - 1;
    }

    if (bottom == -1)
    {
        bottom = Height() - 1;
    }

    MCGuiPort* port = DisplayPort;
    VfxLineDraw(port->Frame(), left, top, right, top, LD_DRAW, color);
    VfxLineDraw(port->Frame(), left, top, left, bottom, LD_DRAW, color);
    VfxLineDraw(port->Frame(), left, bottom, right, bottom, LD_DRAW, color);
    VfxLineDraw(port->Frame(), right, top, right, bottom, LD_DRAW, color);
}

auto MCGuiObject::DrawFramed(int pushed, int fill) -> void
{
    int32_t innerTopLeft = 0xc;
    int32_t outerTopLeft = 10;
    int32_t innerBottomRight = 4;
    int32_t outerBottomRight = 6;

    if (pushed != 0)
    {
        innerTopLeft = 4;
        outerTopLeft = 6;
        innerBottomRight = 0xc;
        outerBottomRight = 10;
    }

    if (fill != 0 && BackgroundColor != 0xff)
    {
        VfxPaneWipe(DisplayPort->Frame(), BackColor());
    }

    VfxLineDraw(DisplayPort->Frame(), 0, Height() - 1, Width(), Height() - 1, LD_DRAW, 0x10);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, Width(), 0, LD_DRAW, 0x10);
    VfxLineDraw(DisplayPort->Frame(), 0, 0, 0, Height() - 1, LD_DRAW, 0x10);
    VfxLineDraw(DisplayPort->Frame(), Width() - 1, 0, Width() - 1, Height() - 1, LD_DRAW, 0x10);
    VfxLineDraw(DisplayPort->Frame(), 1, 1, Width() - 2, 1, LD_DRAW, innerTopLeft);
    VfxLineDraw(DisplayPort->Frame(), 2, 2, Width() - 3, 2, LD_DRAW, outerTopLeft);
    VfxLineDraw(DisplayPort->Frame(), 1, 1, 1, Height() - 2, LD_DRAW, innerTopLeft);
    VfxLineDraw(DisplayPort->Frame(), 2, 2, 2, Height() - 3, LD_DRAW, outerTopLeft);
    VfxLineDraw(DisplayPort->Frame(), Width() - 2, 1, Width() - 2, Height() - 2, LD_DRAW, innerBottomRight);
    VfxLineDraw(DisplayPort->Frame(), Width() - 3, 2, Width() - 3, Height() - 3, LD_DRAW, outerBottomRight);
    VfxLineDraw(DisplayPort->Frame(), 1, Height() - 2, Width() - 2, Height() - 2, LD_DRAW, innerBottomRight);
    VfxLineDraw(DisplayPort->Frame(), 2, Height() - 3, Width() - 3, Height() - 3, LD_DRAW, outerBottomRight);
}

auto MCGuiObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    MCPane box = *DisplayPort->Frame();
    box.X0 = left;
    box.Y0 = top;
    box.X1 = right;
    box.Y1 = bottom;
    VfxPaneWipe(&box, color);
}

auto MCGuiObject::BringToFront(int noShuffle) -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    Parent->BringToFront(noShuffle);

    if (Parent->NumberOfChildren() > 1)
    {
        // The parent's children are kept sorted by depth: the ones in front of this object's depth stay, then this
        // object's depth, with this object last (in front), then the rest.
        MCGuiObject* sorted[255];
        std::memcpy(sorted, Parent->ChildList, sizeof(sorted));
        int32_t next = 0;
        int32_t placed = 0;

        while (next < Parent->NumberOfChildren())
        {
            MCGuiObject* sibling = sorted[next];

            if (sibling->Depth() >= WinDepth)
            {
                break;
            }

            Parent->ChildList[next] = sibling;
            next++;
            placed++;
        }
        while (next < Parent->NumberOfChildren())
        {
            MCGuiObject* sibling = sorted[next];

            if (sibling->Depth() != WinDepth)
            {
                break;
            }

            if (sibling != this)
            {
                Parent->ChildList[placed++] = sibling;
            }

            next++;
        }

        Parent->ChildList[placed] = this;

        for (int32_t slot = placed + 1; slot < Parent->NumberOfChildren(); slot++)
        {
            Parent->ChildList[slot] = sorted[next++];
        }

        if (GridAligned != 0 && noShuffle == 0)
        {
            for (int32_t i = Parent->NumberOfChildren() - 1; i >= 0; i--)
            {
                MCGuiObject* sibling = Parent->Child(i);

                if (sibling->IsShowing() != 0 && sibling != this && sibling->GridAligned != 0 &&
                    sibling->X() / 40 == X() / 40 && sibling->Y() / 40 == Y() / 40)
                {
                    sibling->GridAligned = 0;
                    const int32_t newY = 5 - sibling->Y() % 40 + sibling->Y();
                    const int32_t newX = 5 - sibling->X() % 40 + sibling->X();
                    sibling->MoveTo(newX, newY, 0);
                    sibling->GridAligned = -1;
                }
            }
        }
    }
}

auto MCGuiObject::NumberOfChildren() -> int32_t
{
    return NumChildren;
}

auto MCGuiObject::AddChild(MCGuiObject* child) -> void
{
    Assert(NumChildren < 255, NumChildren + 1, "Too many children!");
    Assert(child->Parent == nullptr || child->Parent == this, 0, " Adding child that's someone else's ");

    if (child != nullptr)
    {
        RemoveChild(child);
        child->SetParent(this);
        ChildList[NumChildren] = child;
        NumChildren++;
        child->BringToFront(-1);
        child->MoveTo(child->X(), child->Y(), 0);
    }
}

auto MCGuiObject::RemoveChild(MCGuiObject* child) -> void
{
    // Port: the original checked IsBadReadPtr(child) and, for an unreadable pointer, removed it without clearing
    // its parent; a pointer here is always readable or null.
    if (child == nullptr)
    {
        return;
    }

    int32_t index = 0;

    while (index < NumChildren && ChildList[index] != child)
    {
        index++;
    }

    if (index >= NumChildren)
    {
        return;
    }

    for (; index < NumChildren - 1; index++)
    {
        ChildList[index] = ChildList[index + 1];
    }

    ChildList[NumChildren] = nullptr;
    NumChildren--;
    child->SetParent(nullptr);
}

auto MCGuiObject::Dragging() -> int
{
    return DragOn;
}

auto MCGuiObject::StartDrag(int32_t xPos, int32_t yPos) -> void
{
    DragOn = -1;
    DragX = xPos;
    DragY = yPos;
}

auto MCGuiObject::StopDrag() -> void
{
    DragOn = 0;
}

auto MCGuiObject::DragStartX() -> int32_t
{
    return DragX;
}

auto MCGuiObject::DragStartY() -> int32_t
{
    return DragY;
}

auto MCGuiObject::ForemostChild(int32_t atDepth) -> MCGuiObject*
{
    for (int32_t i = NumChildren; i > 0; i--)
    {
        if (ChildList[i - 1]->WinDepth == atDepth)
        {
            return ChildList[i - 1];
        }
    }

    return nullptr;
}

auto MCGuiObject::Child(int32_t index) -> MCGuiObject*
{
    if (NumChildren - 1 < index)
    {
        return nullptr;
    }

    return ChildList[index];
}

auto MCGuiObject::Width() -> int32_t
{
    return WinWidth;
}

auto MCGuiObject::Height() -> int32_t
{
    return WinHeight;
}

auto MCGuiObject::Ptr() -> void*
{
    if (DisplayPort != nullptr)
    {
        return DisplayPort->Buffer();
    }

    return nullptr;
}

auto MCGuiObject::Port() -> MCGuiPort*
{
    return DisplayPort;
}

auto MCGuiObject::X() -> int32_t
{
    return WinX;
}

auto MCGuiObject::Y() -> int32_t
{
    return WinY;
}

auto MCGuiObject::GlobalX() -> int32_t
{
    int32_t result = WinX;

    for (MCGuiObject* owner = Parent; owner != nullptr; owner = owner->Parent)
    {
        result += owner->X();
    }

    return result;
}

auto MCGuiObject::GlobalY() -> int32_t
{
    int32_t result = WinY;

    for (MCGuiObject* owner = Parent; owner != nullptr; owner = owner->Parent)
    {
        result += owner->Y();
    }

    return result;
}

auto MCGuiObject::Frame() -> MCPane*
{
    return FramePane;
}

auto MCGuiObject::MoveTo(int32_t xPos, int32_t yPos, int temporary) -> void
{
    int32_t parentX = 0;
    int32_t parentY = 0;
    WinY = yPos;
    WinX = xPos;

    if (Parent != nullptr)
    {
        parentX = Parent->GlobalX();
        parentY = Parent->GlobalY();
    }

    FramePane->X0 = parentX + xPos;
    FramePane->Y0 = parentY + yPos;
    FramePane->X1 = WinWidth - 1 + FramePane->X0;
    FramePane->Y1 = WinHeight - 1 + FramePane->Y0;

    if (temporary == 0)
    {
        HomeX = xPos;
        HomeY = yPos;
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        MCGuiObject* child = ChildList[i];
        child->MoveTo(child->X(), child->Y(), 0);
    }
}

auto MCGuiObject::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth > 0 && newHeight > 0 && (newWidth != WinWidth || newHeight != WinHeight))
    {
        if (GridAligned != 0)
        {
            if (newWidth % 40 > 19)
            {
                newWidth += 40;
            }

            newWidth -= newWidth % 40;

            if (newWidth == 0)
            {
                newWidth = 40;
            }

            if (newHeight % 40 > 19)
            {
                newHeight += 40;
            }

            newHeight -= newHeight % 40;
        }

        if (DisplayPort != nullptr)
        {
            DisplayPort->Resize(newWidth, newHeight);
        }

        WinWidth = newWidth;
        WinHeight = newHeight;
        FramePane->X1 = FramePane->X0 - 1 + newWidth;
        FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
    }
}

auto MCGuiObject::Draw() -> void
{
    const int32_t state = WinState;

    if (state == aSTATE_ICONIZED)
    {
        IconAnimation->Draw(DisplayPort->Frame(), 0, 0);
    }
    else
    {
        if (BackgroundPort != nullptr)
        {
            BackgroundPort->CopyTo(DisplayPort->Frame(), 0, 0, -1);
        }

        if (WindowAnimation != nullptr && Animating != 0)
        {
            WindowAnimation->Draw(DisplayPort->Frame(), 0, 0);
        }
    }

    if (state != aSTATE_ICONIZED)
    {
        Paint();

        for (int32_t i = 0; i < NumChildren; i++)
        {
            DrawChild(ChildList[i]);
        }
    }
}

auto MCGuiObject::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    if (DrawsLive())
    {
        SlideStep();
        DrawInFramePass(DisplayPort);
        return;
    }

    if (WinState == aSTATE_ICONIZED)
    {
        if (IconAnimation != nullptr)
        {
            Draw();
        }
    }
    else if (WindowAnimation != nullptr)
    {
        WindowAnimation->Draw(DisplayPort->Frame(), 0, 0);
        Draw();
    }

    SlideStep();

    if (DisplayPort != nullptr)
    {
        DisplayPort->CopyTo(FramePane, 0, 0, Transparent);
    }

    if (WinState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->Display();
        }
    }
}

auto MCGuiObject::SetDrawsLive() -> void
{
    LiveDraw = true;

    if (DisplayPort != nullptr && !DisplayPort->IsView())
    {
        DisplayPort->InitView(Width(), Height());
    }
}

auto MCGuiObject::DrawsChild(MCGuiObject* child) -> bool
{
    return !child->DrawsLive() && DrawingLive != this;
}

auto MCGuiObject::DrawChild(MCGuiObject* child) -> void
{
    if (DrawingLive == this || child->DrawsLive())
    {
        return;
    }

    child->Draw();
}

auto MCGuiObject::DrawInFramePass(MCGuiPort* port, int32_t scrollY, bool wipe, bool displayChildren) -> void
{
    // The view lies over the pane, on the window the pane is on (the screen, or a scroll pane's content), cut to the
    // window and to the scissor of the nearest clipping ancestor on the same window.
    MCWindow* target = FramePane->Window;
    MCRect scissor{std::max(FramePane->X0, 0), std::max(FramePane->Y0, 0), std::min(FramePane->X1, target->XMax),
                   std::min(FramePane->Y1, target->YMax)};

    if (!ViewClips.empty() && ViewClips.back().first == target)
    {
        const MCRect& outer = ViewClips.back().second;
        scissor.X0 = std::max(scissor.X0, outer.X0);
        scissor.Y0 = std::max(scissor.Y0, outer.Y0);
        scissor.X1 = std::min(scissor.X1, outer.X1);
        scissor.Y1 = std::min(scissor.Y1, outer.Y1);
    }

    if (scissor.X1 < scissor.X0 || scissor.Y1 < scissor.Y0)
    {
        // An empty scissor: the shut one draws nothing either way, and the children are cut away by it.
        scissor = MCRect{0, 0, -1, -1};
    }

    port->OpenView(target, FramePane->X0, FramePane->Y0 - scrollY, scissor, Transparent != 0);
    MCGuiObject* const outerDrawing = DrawingLive;
    DrawingLive = this;

    // A picture that was never painted held zeros (a port's bitmap starts zeroed), and an opaque object copied
    // them to the screen; a transparent one let what was under it show.
    if (Transparent == 0 && wipe)
    {
        VfxPaneWipe(port->Frame(), 0);
    }

    if (WinState != aSTATE_ICONIZED || IconAnimation != nullptr)
    {
        Draw();
    }

    DrawingLive = outerDrawing;
    port->CloseView();

    if (WinState != aSTATE_ICONIZED && displayChildren)
    {
        const bool clips = ClipsChildren();

        if (clips)
        {
            ViewClips.emplace_back(target, scissor);
        }

        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->Display();
        }

        if (clips)
        {
            ViewClips.pop_back();
        }
    }
}

auto MCGuiObject::SlideStep() -> void
{
    if (HideOffset != 0)
    {
        // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
        if (HideDirection == DIRECTION_LEFT || HideDirection == DIRECTION_RIGHT)
        {
            MoveTo(X() + HideOffset, Y(), -1);
        }
        else
        {
            MoveTo(X(), Y() + HideOffset, -1);
        }

        if (Hidden != 0)
        {
            const tagRECT screen = {0, 0, Application->Width(), Application->Height()};

            if (RectIntersect(screen) == 0)
            {
                HideOffset = 0;
            }
        }
        else
        {
            bool home = false;

            if (HideOffset < 0)
            {
                home = HomeX >= GlobalX() && HomeY >= GlobalY();
            }
            else if (HideOffset > 0)
            {
                home = HomeX <= GlobalX() && HomeY <= GlobalY();
            }

            if (home)
            {
                const int32_t homeYOffset = HomeY - Parent->GlobalY();
                MoveTo(HomeX - Parent->GlobalX(), homeYOffset, -1);
                HideOffset = 0;
            }
        }
    }
}

auto MCGuiObject::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        if (Parent != nullptr)
        {
            BringToFront(0);
            ARedrawScreen();
        }
    }
    else if (event->Type == 0x12)
    {
        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->HandleEvent(event);
        }
    }

    if (WinState == aSTATE_ICONIZED)
    {
        switch (event->Type)
        {
            case 1:
            {
                Application->Grab(this);
                LastX = event->X;
                LastY = event->Y;
                break;
            }
            case 4:
            {
                if (Application->GrabbedObject() == this)
                {
                    Application->Release();
                    MCGuiObject* target = ScreenWindow->FindObject(event->X, event->Y);

                    if (target != nullptr)
                    {
                        target->Enter();
                    }
                }
                break;
            }
            case 7:
            {
                if (Application->GrabbedObject() == this)
                {
                    const int32_t eventY = event->Y;
                    MCGuiObject* target = ScreenWindow->FindObject(event->X, eventY);

                    if (target != nullptr && target->Depth() == 100)
                    {
                        return;
                    }

                    const int32_t newY = Y() + (eventY - LastY);
                    MoveTo(X() + (event->X - LastX), newY, 0);
                    LastX = event->X;
                    LastY = eventY;
                }
                break;
            }
            case 0x10:
            {
                if (Application->GrabbedObject() == this)
                {
                    Application->Release();
                }

                Normalize();
                break;
            }
        }
    }
    else if (event->Type == 0xd)
    {
        Destroy();
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCGuiObject::SetBackground(char* fileName) -> int32_t
{
    if (BackgroundPort != nullptr)
    {
        BackgroundPort->Destroy();
        delete BackgroundPort;
        BackgroundPort = nullptr;
    }

    BackgroundPort = new MCGuiPort;

    if (BackgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return BackgroundPort->Init(fileName);
}

auto MCGuiObject::SetBackground(int32_t artPacket) -> int32_t
{
    if (BackgroundPort != nullptr)
    {
        BackgroundPort->Destroy();
        delete BackgroundPort;
        BackgroundPort = nullptr;
    }

    BackgroundPort = new MCGuiPort;

    if (BackgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return BackgroundPort->Init(artPacket);
}

auto MCGuiObject::Maximize() -> void
{
    SetState(aSTATE_MAXIMIZED);
}

auto MCGuiObject::Normalize() -> void
{
    SetState(aSTATE_NORMAL);
}

auto MCGuiObject::Iconize() -> void
{
    SetState(aSTATE_ICONIZED);
}

auto MCGuiObject::State() -> int32_t
{
    return WinState;
}

auto MCGuiObject::SetState(int32_t newState) -> void
{
    // Saves the placement of the state left, then takes the new state's. From the iconized state the new state is
    // stored first, so a state the switch doesn't list sticks (with the icon's placement).
    switch (WinState)
    {
        case aSTATE_NORMAL:
        {
            NormalY = WinY;
            NormalX = WinX;
            NormalWidth = WinWidth;
            NormalHeight = WinHeight;

            if (newState == aSTATE_MAXIMIZED)
            {
                WinX = MaxX;
                WinState = aSTATE_MAXIMIZED;
                WinY = MaxY;
                WinWidth = MaxWidth;
                WinHeight = MaxHeight;
            }
            else if (newState == aSTATE_ICONIZED)
            {
                WinX = IconX;
                WinState = aSTATE_ICONIZED;
                WinY = IconY;
                WinWidth = IconWidth;
                WinHeight = IconHeight;
            }
            break;
        }
        case aSTATE_MAXIMIZED:
        {
            MaxY = WinY;
            MaxX = WinX;
            MaxWidth = WinWidth;
            MaxHeight = WinHeight;

            if (newState == aSTATE_NORMAL)
            {
                WinX = NormalX;
                WinState = aSTATE_NORMAL;
                WinY = NormalY;
                WinWidth = NormalWidth;
                WinHeight = NormalHeight;
            }
            else if (newState == aSTATE_ICONIZED)
            {
                WinX = IconX;
                WinState = aSTATE_ICONIZED;
                WinY = IconY;
                WinWidth = IconWidth;
                WinHeight = IconHeight;
            }
            break;
        }
        case aSTATE_ICONIZED:
        {
            IconX = WinX;
            IconY = WinY;
            WinState = newState;
            IconWidth = WinWidth;
            IconHeight = WinHeight;

            if (newState == aSTATE_NORMAL)
            {
                WinX = NormalX;
                WinState = aSTATE_NORMAL;
                WinY = NormalY;
                WinWidth = NormalWidth;
                WinHeight = NormalHeight;
            }
            else if (newState == aSTATE_MAXIMIZED)
            {
                WinX = MaxX;
                WinState = aSTATE_MAXIMIZED;
                WinY = MaxY;
                WinWidth = MaxWidth;
                WinHeight = MaxHeight;
            }
            break;
        }
    }

    Resize(WinWidth, WinHeight);
    MoveTo(WinX, WinY, 0);
    ARedrawScreen();
}

auto MCGuiObject::SetAnimation(char* fileName) -> int32_t
{
    if (WindowAnimation != nullptr)
    {
        WindowAnimation->Destroy();
        delete WindowAnimation;
        WindowAnimation = nullptr;
    }

    WindowAnimation = new (std::nothrow) MCGuiAnimation;
    return WindowAnimation->Init(fileName);
}

auto MCGuiObject::SetIcon(char* fileName) -> int32_t
{
    if (IconAnimation != nullptr)
    {
        IconAnimation->Destroy();
        delete IconAnimation;
        IconAnimation = nullptr;
    }

    MCGuiAnimation* newIcon = new (std::nothrow) MCGuiAnimation;
    IconAnimation = newIcon;
    const int32_t result = newIcon->Init(fileName);

    if (result == 0)
    {
        IconWidth = newIcon->Width();
        IconHeight = newIcon->Height();
    }

    return result;
}

auto MCGuiObject::Animation() -> MCGuiAnimation*
{
    return WindowAnimation;
}

auto MCGuiObject::Icon() -> MCGuiAnimation*
{
    return IconAnimation;
}

auto MCGuiObject::Background() -> MCGuiPort*
{
    return BackgroundPort;
}

auto MCGuiObject::SetBit(int32_t xPos, int32_t yPos, uint8_t color) -> void
{
    if (xPos > -1 && xPos < Width() && yPos > -1 && yPos < Height())
    {
        const int32_t rowLength = Width();

        if (DisplayPort->Buffer() != nullptr)
        {
            DisplayPort->Buffer()[rowLength * yPos + xPos] = color;
        }
        else if (DisplayPort->IsView())
        {
            // Port: a view has no pixels; the pixel is drawn through it.
            VfxPixelWrite(DisplayPort->Frame(), xPos, yPos, color);
        }
    }
}

auto MCGuiObject::HideMe(int hide) -> void
{
    if (Hidden == hide || HideOffset != 0)
    {
        return;
    }

    if (hide != 0)
    {
        HomeX = GlobalX();
        HomeY = GlobalY();

        switch (HideDirection)
        {
            case DIRECTION_LEFT:
            {
                Hidden = hide;
                HideOffset = -(GlobalX() + Width());
                return;
            }
            case DIRECTION_UP:
            {
                Hidden = hide;
                HideOffset = -(GlobalY() + Height());
                return;
            }
            case DIRECTION_RIGHT:
            {
                Hidden = hide;
                HideOffset = Application->Width() - GlobalX();
                return;
            }
            case DIRECTION_DOWN:
            {
                Hidden = hide;
                HideOffset = Application->Height() - GlobalY();
                return;
            }
            default:
            {
                Hidden = 0;
                HideOffset = 0;
                return;
            }
        }
    }

    if (HomeX != GlobalX())
    {
        const int32_t current = GlobalX();
        Hidden = 0;
        HideOffset = HomeX - current;
        return;
    }

    const int32_t current = GlobalY();
    Hidden = 0;
    HideOffset = HomeY - current;
}

// Free functions.

auto CreateDibSection(int32_t width, int32_t height, void** bitmapInfo, void** bitmap, uint8_t** bits) -> int32_t
{
    // Port: no GDI. The original filled a BITMAPINFO (0x428 bytes from the GUI heap, colour table = palette indexes)
    // and made an 8-bit top-down DIB section; the port gives a zeroed buffer (the caller deletes[] it) as the section
    // and its bits. Uncalled in MCX.EXE.
    if (*bitmapInfo == nullptr)
    {
        *bitmapInfo = new uint8_t[0x428]{};
    }

    *bits = new uint8_t[static_cast<size_t>(width * height)]{};
    *bitmap = *bits;
    return *bitmap != nullptr ? 0 : 2;
}

auto CreatePaletteFromGif(char* fileName) -> void*
{
    // Port: no GDI palettes. The "palette" returned is 256 VFX_RGB (the caller deletes[] them), 8 bits per channel as the
    // original's PALETTEENTRY values. Uncalled in MCX.EXE.
    char path[128];
    char message[256];
    MCFile gifFile;
    std::snprintf(path, sizeof(path), "%s%s", PalettePath, fileName);

    if (FileExists(path) == 0)
    {
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
    }

    gifFile.Open(path, READ, 50);
    const uint32_t size = gifFile.FileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading from '%s'", path);
        GeneralMsg(message);
    }

    std::vector<uint8_t> gif(size);
    gifFile.Read(gif.data(), static_cast<int32_t>(size));
    gifFile.Close();
    MCVfxRgb colors[256];
    VfxGifPalette(gif.data(), colors);
    return CreatePaletteFromRam(colors);
}

auto CreatePaletteFromRam(void* colors) -> void*
{
    // Port: see CreatePaletteFromGIF. The colours are 6-bit, shifted up.
    MCVfxRgb source[256];
    std::memcpy(source, colors, sizeof(source));
    auto* palette = new MCVfxRgb[256]{};

    for (int i = 0; i < 256; i++)
    {
        palette[i].R = static_cast<uint8_t>(source[i].R << 2);
        palette[i].G = static_cast<uint8_t>(source[i].G << 2);
        palette[i].B = static_cast<uint8_t>(source[i].B << 2);
    }

    return palette;
}

auto CreateSmackPaletteFromRam(void* colors) -> void*
{
    // Port: see CreatePaletteFromGIF. Smacker's colours are already 8-bit.
    auto* palette = new MCVfxRgb[256];
    std::memcpy(palette, colors, sizeof(MCVfxRgb) * 256);
    return palette;
}

auto ARedrawScreen() -> void
{
    for (int32_t i = 0; i < ScreenWindow->NumberOfChildren(); i++)
    {
        ScreenWindow->Child(i)->Draw();
    }
}

auto ALockScreen() -> int32_t
{
    if (LockActive == 0)
    {
        LockActive = -1;
        // Port: the original locked the DirectDraw back surface here in 16-bit full screen and drew straight into
        // it; the port's screen is always 8-bit, so the screen port always shows the display's buffer.
        ScreenPort->Resize(GWidth, GHeight);
        ScreenPort->Bitmap()->Buffer = ScreenBits;
    }

    return 0;
}

auto AUnlockScreen() -> int32_t
{
    // Port: unlocking the 16-bit full-screen surface is gone with it (see aLockScreen).
    if (LockActive != 0)
    {
        LockActive = 0;
    }

    return 0;
}

auto APostMessage(MCGuiObject* obj, int32_t message) -> void
{
    MCGuiEvent event;
    event.Clear();
    event.Type = message;

    // Port: the original skipped unreadable pointers (IsBadReadPtr); the port skips null.
    if (obj != nullptr)
    {
        obj->HandleEvent(&event);
    }
}

auto TestMsgCallback(MCFidpMessage* message, void* data) -> void
{
    (void)data;
    MPlayer->SessionManager->GetPlayer(message->FromID);
}

auto SendAndReceiveTestMessages() -> void
{
    MCSessionManager* manager = MPlayer->SessionManager;
    MCTestMessage test = {};
    test.Header.Header = 0x1064;
    manager->ApplicationCallback = TestMsgCallback;
    manager->ApplicationCallbackData = nullptr;
    MPStartTime = MCPort::Milliseconds();

    for (;;)
    {
        uint32_t frameStart;

        do
        {
            (void)MCPort::Milliseconds();
            Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 1) == 0, 0, "User exited");
            manager->ProcessMessages();

            for (uint32_t i = 0; i < 2; i++)
            {
                test.Frame = Networkframe;
                test.Index = i;
                manager->SendMessageToGroup(0, &test.Header, sizeof(test));
            }

            Networkframe++;
            frameStart = MCPort::Milliseconds();
        } while (frameStart + 50 <= MCPort::Milliseconds());

        while (MCPort::Milliseconds() < frameStart + 50)
        {
        }
    }
}

auto StartMultiplayerGame(char* commandLine) -> int
{
    int started = 0;
    MCFullPathFileName fileName;
    MCFitIniFile gameFile;
    MCInput::GetAsyncKeyState(VK_ESCAPE);
    fileName.Init(MissionPath, commandLine, "");

    if (gameFile.Open(fileName, READ, 50) != 0 || gameFile.SeekBlock("Multiplayer") != 0)
    {
        return 0;
    }

    uint32_t tries = 0;
    MCMultiPlayer* newPlayer = new MCMultiPlayer;
    MPlayer = newPlayer;
    Assert(MPlayer != nullptr, 0, " Unable to create MultiPlayer object ");
    Assert(MPlayer->Init(0x7d000, 0x100, 100) == 0, 0, "could not initialize multiplayer");

    if (MPlayer->Init(&gameFile) == static_cast<int32_t>(0x8877042e))
    {
        const int32_t numPlayers = MPlayer->NumPlayers();

        if (MPlayer->IsServer == 0)
        {
            uint32_t result;

            do
            {
                do
                {
                    tries++;
                } while (tries % 50 != 0);

                Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0, 0, "User exited");
                result = static_cast<uint32_t>(MPlayer->JoinSession(nullptr, nullptr));
                Assert(result != 0xfffffffe, result, "Error joining session!");
            } while (result != 0);
        }
        else
        {
            MPlayer->CreateSession(nullptr, nullptr, 6);

            while (MPlayer->PlayersInSession() < numPlayers)
            {
                tries++;

                if (tries % 50 == 0)
                {
                    MPlayer->ProcessReceiveList();
                    Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 1) == 0, 0, "User exited");
                }
            }

            Assert(MPlayer->PlayersInSession() > 1, 0, "No other players joined in time.");
        }

        started = -1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MPlayer->ProcessReceiveList();

    while (MPlayer->SessionManager->MyPlayer->PlayerNumber < 0)
    {
        MPlayer->ProcessReceiveList();
        Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0, 0, "User exited");
    }

    SendAndReceiveTestMessages();
    return started;
}

auto ParseCommandLine(char* commandLine) -> void
{
    char* words[32];
    int32_t numWords = 0;
    int network = 0;
    char* networkFile = nullptr;

    if (*commandLine != '\0')
    {
        int32_t pos = 0;

        do
        {
            words[numWords++] = commandLine + pos;

            while (commandLine[pos] != ' ' && commandLine[pos] != '\0')
            {
                pos++;
            }
            while (commandLine[pos] == ' ')
            {
                commandLine[pos] = '\0';
                pos++;
            }
        } while (commandLine[pos] != '\0');
    }

    for (int32_t i = 0; i < numWords; i++)
    {
        char* word = words[i];

        if (MCPort::StrICmp(word, "-mission") == 0)
        {
            i++;

            if (i < numWords)
            {
                GlobalGameSegment = std::atoi(words[i]);

                if (GlobalGameSegment < 1 || GlobalGameSegment > 99)
                {
                    GlobalGameSegment = 0;
                }
            }
        }
        else if (std::strcmp(word, "+") == 0)
        {
            i++;

            if (i < numWords)
            {
                GlobalGameSegment = std::atoi(words[i]);

                if (GlobalGameSegment < 1 || GlobalGameSegment > 50)
                {
                    GlobalGameSegment = 0;
                }
            }
        }
        else if (MCPort::StrICmp(word, "-renderer") == 0)
        {
            // Port: "-renderer vulkan|software" picks the renderer over PREFS "Renderer".
            i++;

            if (i < numWords)
            {
                if (const std::optional<MCRendererKind> kind = MCRendererKindFromName(words[i]))
                {
                    GRenderer = static_cast<int>(*kind);
                }
            }
        }
        else if (MCPort::StrICmp(word, "-gpudraw") == 0)
        {
            // Port: "-gpudraw off|on|mirror": who draws the frame with the Vulkan renderer (see MCGpuDrawing).
            i++;

            if (i < numWords)
            {
                if (const std::optional<MCGpuDrawing> drawing = MCGpuDrawingFromName(words[i]))
                {
                    MCRenderer::RequestGpuDrawing(*drawing);
                }
            }
        }
        else if (MCPort::StrICmp(word, "-fps") == 0)
        {
            // Port: "-fps" draws the frame counter (as PREFS "ShowFps").
            GShowFps = 1;
        }
        else if (MCPort::StrICmp(word, "-framelog") == 0)
        {
            // Port: "-framelog <file>" writes the slow frames there, with what took their time (MCFrameLog).
            i++;

            if (i < numWords && !MCFrameLog::Open(words[i]))
            {
                SDL_Log("-framelog: can't write %s", words[i]);
            }
        }
        else if (MCPort::StrICmp(word, "-novsync") == 0)
        {
            // Port: "-novsync" shows frames as soon as they are drawn instead of at the display's refresh.
            GVSync = 0;
        }
        else if (MCPort::StrICmp(word, "-gpudump") == 0)
        {
            // Port: "-gpudump <folder>": mirror mode saves the first frame that differs there.
            i++;

            if (i < numWords)
            {
                MCRenderer::SetMirrorDumpFolder(words[i]);
            }
        }
        else if (MCPort::StrICmp(word, "-network") == 0)
        {
            i++;

            if (i < numWords)
            {
                network = -1;
                networkFile = words[i];
            }
        }
        else if (MCPort::StrICmp(word, "-load") == 0)
        {
            i++;

            if (i < numWords)
            {
                const size_t length = std::strlen(words[i]) + 1;
                StartupPakFile = new char[length];
                std::memcpy(StartupPakFile, words[i], length);
            }
        }
    }

    if (network != 0)
    {
        StartMultiplayerGame(networkFile);
    }
}

auto NextCommandLineWord(char* line, int32_t pos, char* word, int32_t maxLength) -> int32_t
{
    int32_t count = 0;

    while (*line == ' ' || *line == '\t')
    {
        line++;
    }

    if (pos != 0)
    {
        do
        {
            if (*line == '\0')
            {
                break;
            }
            while (*line != ' ' && *line != '\t' && *line != '\0')
            {
                line++;
            }

            count++;
        } while (count != pos);
    }

    if (*line == '\0')
    {
        *word = '\0';
        return 0;
    }
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }

    char* end = line;

    while (*end != ' ' && *end != '\t' && *end != '\0')
    {
        end++;
    }

    const int32_t length = static_cast<int32_t>(end - line);

    if (length < maxLength)
    {
        std::strncpy(word, line, static_cast<size_t>(length));
        word[length] = '\0';
        return length;
    }

    *word = '\0';
    return -1;
}

auto RealWinMain(void* instance, void* prevInstance, char* commandLine, int showCommand) -> int
{
    // Port: the original noted the stack top (topOfStack) and warned when the page file was under 48,000,000
    // bytes (GlobalMemoryStatus, string 0x355); neither applies to the port.
    MCPort::SeedRand(static_cast<uint32_t>(std::time(nullptr)));

    if (prevInstance != nullptr)
    {
        GeneralMsg("An instance of this application is already running.");
    }

    ThisInstance = instance;
    std::strcpy(SavePath, "c:\\Program Files\\Honor Bound\\");
    std::strcpy(DirectXPath, "\\honorb\\directx\\");
    std::strcpy(TerrainPath, "data\\terrain\\");
    std::strcpy(PalettePath, "data\\palette\\");
    std::strcpy(ArtPath, "data\\art\\");
    std::strcpy(FontPath, "data\\fonts\\");
    std::strcpy(SoundPath, "data\\sound\\");
    std::strcpy(SpritePath, "data\\sprites\\");
    std::strcpy(InterfacePath, "data\\iface\\");
    std::strcpy(PaletteName, "palette.gif");

    OldMouseY = -1;
    OldMouseX = -1;
    RightMouseButtonDown = 0;
    LeftMouseButtonDown = 0;
    Application = new MCGuiSystem;

    if (Application->Start(instance, nullptr, commandLine, showCommand, 640, 480) != 0)
    {
        return -4;
    }

    Application->Run();
    Application->Stop();
    delete Application;
    return 0;
}

auto DestroyVersion() -> void
{
    if (VersionDialog != nullptr)
    {
        VersionDialog->Destroy();
        delete VersionDialog;
        VersionDialog = nullptr;
    }
}

auto HandleEvent(MCGuiEvent* event) -> void
{
    if (ScreenWindow == nullptr)
    {
        return;
    }

    const int32_t type = event->Type;

    if (type == 0x13 || (type > 0x13ff && type < 0x2401))
    {
        // Timer events and posted messages go to the tactical interface.
        if (Scenario == nullptr || Turn < 1)
        {
            return;
        }

        TheInterface->HandleEvent(event);
        return;
    }

    MCGuiObject* target;

    if (Application->TextObject() != nullptr && (type == 10 || type == 9 || type == 8))
    {
        target = Application->TextObject();
    }
    else if (Application->GrabbedObject() != nullptr)
    {
        target = Application->GrabbedObject();
    }
    else if (EventsToMissionResultsScreen != 0 && Mission->ResultsScreen != nullptr)
    {
        MCGuiObject* results = Mission->ResultsScreen;

        if (type == 10 && event->Key == VK_ESCAPE)
        {
            target = results;
        }
        else
        {
            target = results->FindObject(event->X, event->Y);
        }
    }
    else
    {
        if (type == 9)
        {
            if (FeatureScreen != nullptr)
            {
                FeatureScreenDone = -1;
            }

            const uint8_t key = event->Key;

            switch (key)
            {
                case VK_RETURN:
                {
                    if (Scenario != nullptr && (GamePaused != 0 || GameAsked != 0) && event->CtrlKey == 0 &&
                        event->AltKey == 0)
                    {
                        Mission->EndScenarioRequested = -1;

                        if (GameAsked != 0)
                        {
                            ScenarioResult = 3;
                        }

                        GamePaused = 0;
                        GameAsked = 0;
                    }
                    break;
                }
                default:
                {
                    KeySetting = static_cast<char>(key);

                    if (key == VK_F12)
                    {
                        QueuePlayerOrders = (QueuePlayerOrders != 0) - 1;
                    }
                    break;
                }
                case VK_PAUSE:
                {
                    if (Scenario != nullptr && MPlayer == nullptr && Turn > 0)
                    {
                        GamePaused = ~GamePaused;
                    }

                    if (event->AltKey != 0 && AssertTest(0x80, const_cast<char*>("User Break")) != 0)
                    {
                        SDL_TriggerBreakpoint();
                        return;
                    }
                    break;
                }
                case VK_ESCAPE:
                {
                    if (Scenario != nullptr && EventsToMissionResultsScreen == 0 && Scenario->StartingUp == 0 &&
                        Scenario->StartUpTurns < Turn)
                    {
                        if (MPlayer == nullptr)
                        {
                            GamePaused = ~GamePaused;
                        }
                        else
                        {
                            GameAsked = ~GameAsked;
                        }
                    }
                    break;
                }
                case 'D':
                {
                    if (Scenario != nullptr && CheatsOn != 0 && MPlayer == nullptr && KeyHeld(VK_CONTROL) &&
                        KeyHeld(VK_MENU))
                    {
                        DisableHomeTeamTargets();
                    }
                    break;
                }
                case 'G':
                {
                    if (Scenario == nullptr)
                    {
                        break;
                    }

                    if (CheatsOn != 0 && MPlayer == nullptr && KeyHeld(VK_CONTROL) && KeyHeld(VK_MENU))
                    {
                        ForceGatesClosed = -1;
                    }

                    // Original bug (OB-063): no break, so the gate cheat also runs the kill cheat below.
                    [[fallthrough]];
                }
                case 'K':
                {
                    if (Scenario != nullptr && CheatsOn != 0 && MPlayer == nullptr && KeyHeld(VK_CONTROL) &&
                        KeyHeld(VK_MENU))
                    {
                        KillHomeTeamTargets();
                    }
                    break;
                }
                case 'L':
                {
                    if (Scenario != nullptr && CheatsOn != 0 && KeyHeld(VK_CONTROL))
                    {
                        DrawTerrainGrid = ~DrawTerrainGrid;
                    }
                    break;
                }
                case 'P':
                {
                    if (KeyHeld(VK_CONTROL) && KeyHeld(VK_MENU))
                    {
                        if (DisplayProfileData == 0)
                        {
                            DisplayProfileData = 1;
                        }
                        else if (DisplayProfileData == 1)
                        {
                            DisplayProfileData = 2;
                        }
                        else if (DisplayProfileData == 2)
                        {
                            DisplayProfileData = 0;
                        }
                    }
                    break;
                }
                case 'Q':
                {
                    if (Scenario != nullptr && CheatsOn != 0 && event->CtrlKey != 0 && event->AltKey != 0 &&
                        MPlayer == nullptr)
                    {
                        Mission->EndScenarioRequested = -1;
                    }
                    break;
                }
                case 'S':
                {
                    if (KeyHeld(VK_CONTROL) && KeyHeld(VK_MENU))
                    {
                        LockFrameRate = ~LockFrameRate;
                    }
                    break;
                }
                case 'V':
                {
                    if (CheatsOn != 0 && KeyHeld(VK_CONTROL) && KeyHeld(VK_MENU))
                    {
                        char version[256];
                        char name[256];
                        char release[256];
                        char text[256];
                        CLoadString(ThisInstance, 0x280, name, 0xfe);
                        CLoadString(ThisInstance, 0x281, version, 0xfe);
                        CLoadString(ThisInstance, 0x282, release, 0xfe);
                        std::snprintf(text, sizeof(text), "Release Version: %s", release);
                        DestroyVersion();
                        VersionDialog = new MCGuiMessageBox;
                        VersionDialog->Init(reinterpret_cast<uint8_t*>(text));
                        ScreenWindow->AddChild(VersionDialog);
                        Application->Grab(VersionDialog);
                    }
                    break;
                }
                case 'W':
                {
                    if (CheatsOn != 0 && Scenario != nullptr && event->CtrlKey != 0 && event->AltKey != 0 &&
                        MPlayer == nullptr)
                    {
                        Scenario->StartingUp = 0;
                        Mission->EndScenarioRequested = -1;
                        ScenarioResult = 5;
                        Scenario->StartUpCountdown = 0;
                    }
                    break;
                }
                case 'Z':
                {
                    if (KeyHeld(VK_CONTROL) && Application->SmackerWindow == nullptr &&
                        Application->SmackerWindow2 == nullptr)
                    {
                        Application->GammaCorrectCurrentPalette();
                    }
                    break;
                }
            }
        }

        target = ScreenWindow->FindObject(event->X, event->Y);
    }

    if ((event->Type == 8 || event->Type == 9) && Application->TextObject() == nullptr && TheInterface != nullptr &&
        EventsToMissionResultsScreen == 0 && Scenario != nullptr && Turn > 0)
    {
        TheInterface->HandleEvent(event);
    }

    if (Application->GrabbedObject() == nullptr && Application->ModalObject() != nullptr)
    {
        // A modal object only takes events for itself and its children.
        MCGuiObject* owner = target;

        while (owner != nullptr && owner != Application->ModalObject())
        {
            owner = owner->Parent;
        }

        if (owner != Application->ModalObject())
        {
            return;
        }
    }

    if (event->Type == 1 || event->Type == 3)
    {
        if (Application->TextObject() != nullptr && target != Application->TextObject())
        {
            Application->ReleaseText();
        }
    }
    else if (event->Type == 7 && Application->GrabbedObject() == nullptr && target != Application->CurrentObject())
    {
        if (Application->CurrentObject() != nullptr)
        {
            Application->CurrentObject()->Leave();
        }

        if (target != nullptr)
        {
            target->Enter();
        }

        Application->SetCurrentObject(target);
    }

    if (target == nullptr)
    {
        return;
    }

    event->Target = target;
    target->HandleEvent(event);
}

auto Cheat(char* code) -> int
{
    const int32_t length = code[0];
    uint32_t pos = static_cast<uint32_t>(CheatPointer - length);
    int matched = -1;

    for (int32_t i = 0; i < length; i++)
    {
        if (matched == 0)
        {
            return 0;
        }

        const char typed = CheatKey[pos & 0x7f];
        pos = (pos & 0x7f) + 1;
        const int wanted = std::tolower(static_cast<uint8_t>(code[i + 1] - 0x32));

        if (static_cast<char>(std::tolower(static_cast<uint8_t>(typed))) != static_cast<char>(wanted))
        {
            matched = 0;
        }
    }

    if (matched == 0)
    {
        return 0;
    }

    // Port: MessageBeep(0) (the cheat's confirmation) is not played.
    return matched;
}

auto TranslateMessage(void* window, uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
{
    (void)window;

    if (ApplicationActive == 0)
    {
        return 0;
    }

    const tagPOINT cursor = GetMessageCursorLoc();
    MCGuiEvent event;
    event.Clear();
    const uint8_t low = static_cast<uint8_t>(wParam);
    const int16_t scanCode = static_cast<int16_t>((lParam >> 16) & 0x1ff);

    switch (message)
    {
        case WM_PAINT:
            event.Type = 0xc;
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            // Only the first press, not the repeats.
            if ((lParam & 0xffff) == 1)
            {
                event.Type = 9;
                event.Key = low;
                event.ScanCode = scanCode;
                event.CtrlKey = (MCInput::GetKeyState(VK_CONTROL) & 0x8000) != 0 ? 0xff : 0;
                event.ShiftKey = (MCInput::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 0xff : 0;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            event.Type = 8;
            event.Key = low;
            event.ScanCode = scanCode;
            event.CtrlKey = (MCInput::GetKeyState(VK_CONTROL) & 0x8000) != 0 ? 0xff : 0;
            event.ShiftKey = (MCInput::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 0xff : 0;
            break;
        }
        case WM_CHAR:
        {
            if (CheatsOn != 0)
            {
                CheatKey[CheatPointer] = static_cast<char>(low);
                CheatPointer = (CheatPointer + 1) & 0x7f;

                if (Cheat(CheatFramegraph) != 0)
                {
                    AndyFramerate ^= 1;
                }

                if (Scenario != nullptr && Turn > 0 && MPlayer == nullptr)
                {
                    if (Cheat(CheatHealAll) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        HealAll();
                    }

                    if (Cheat(CheatDeadEye) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        DeadEye();
                    }

                    if (Cheat(CheatCantHitMe) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        CantHitMe = CantHitMe == 0;
                    }

                    if (Cheat(CheatGetSalvage) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        CantBlowSalvage = CantBlowSalvage == 0;
                    }

                    if (Cheat(CheatReveal) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        RevealAll();
                    }

                    if (Cheat(CheatBunnyStrike) != 0)
                    {
                        SoundSystem->PlayBettySample(0x16);
                        BunnyStrikesOn = BunnyStrikesOn == 0;
                    }

                    if (Cheat(CheatDuh) != 0)
                    {
                        SoundSystem->PlayBettySample(0x1c);
                        Duh = Duh == 0;
                    }
                }
            }

            event.Type = 10;
            event.Key = low;
            event.ScanCode = scanCode;
            // The character's modifiers are read from wParam's MK_ bits, as for a mouse message.
            event.CtrlKey = low & MK_CONTROL;
            event.ShiftKey = low & MK_SHIFT;
            break;
        }
        case WM_TIMER:
            event.Type = 0x13;
            break;
        case WM_LBUTTONDBLCLK:
        {
            event.Type = 0x10;
            event.CtrlKey = low & MK_CONTROL;
            event.ShiftKey = low & MK_SHIFT;
            break;
        }
        case WM_RBUTTONDBLCLK:
        {
            event.Type = 0x11;
            event.CtrlKey = low & MK_CONTROL;
            event.ShiftKey = low & MK_SHIFT;
            break;
        }
        case WM_MOUSEWHEEL:
        {
            // The original ignored the wheel. Over the battlefield (the main pane itself, not the interface drawn
            // over it) it zooms like the zoom keys: up in, down out. Over anything else it scrolls the first of the
            // object and its parents that has a scroll bar (aObject::MouseWheel). While an object holds the mouse,
            // only that object is offered it (an open drop-down list; a dragged thumb doesn't take it).
            if (ScreenWindow == nullptr)
            {
                return 1;
            }

            const int16_t delta = static_cast<int16_t>(wParam >> 16);
            MCGuiObject* const grabbed = Application->GrabbedObject();
            MCGuiObject* target = nullptr;

            if (grabbed != nullptr)
            {
                target = grabbed;
            }
            else if (EventsToMissionResultsScreen != 0 && Mission != nullptr && Mission->ResultsScreen != nullptr)
            {
                target = Mission->ResultsScreen->FindObject(cursor.x, cursor.y);
            }
            else
            {
                target = ScreenWindow->FindObject(cursor.x, cursor.y);
            }

            if (target == nullptr)
            {
                return 1;
            }

            if (grabbed == nullptr && TheInterface != nullptr && Scenario != nullptr && Turn > 0 &&
                EventsToMissionResultsScreen == 0 && MainHolder != nullptr && target == MainHolder->GetActivePane())
            {
                // A step per notch (finer wheels zoom finer); not while paused or asked.
                const float step = std::pow(MCInterfaceObject::ZoomWheelStep, std::fabs(delta / 120.0f));

                if (delta > 0)
                {
                    TheInterface->ZoomIn(step, false);
                }
                else if (delta < 0)
                {
                    TheInterface->ZoomOut(step, false);
                }

                return 1;
            }

            // A modal object only takes the wheel for itself and its children, as for other events.
            if (grabbed == nullptr && Application->ModalObject() != nullptr)
            {
                MCGuiObject* owner = target;

                while (owner != nullptr && owner != Application->ModalObject())
                {
                    owner = owner->Parent;
                }

                if (owner == nullptr)
                {
                    return 1;
                }
            }

            // A notch is 120; finer wheels and touchpads send less, so the remainder carries over. Wheel up scrolls
            // up (negative steps).
            static int32_t wheelRemainder = 0;

            if ((wheelRemainder > 0 && delta < 0) || (wheelRemainder < 0 && delta > 0))
            {
                wheelRemainder = 0;
            }

            wheelRemainder += delta;
            const int32_t steps = -(wheelRemainder / 120);
            wheelRemainder %= 120;

            if (steps == 0)
            {
                return 1;
            }

            for (MCGuiObject* object = target; object != nullptr;
                 object = grabbed != nullptr ? nullptr : object->Parent)
            {
                if (object->MouseWheel(steps, cursor.x, cursor.y))
                {
                    break;
                }
            }

            return 1;
        }
    }

    // Mouse moves and presses come from CheckMouse, not from here. Messages from WM_USER + 0x1000 up are posted
    // game messages.
    if (message >= 0x1400)
    {
        event.Type = static_cast<int32_t>(message);
    }

    event.LParam = lParam;
    event.X = cursor.x;
    event.Data = static_cast<int32_t>(wParam);
    event.LeftButton = low & MK_LBUTTON;
    event.MiddleButton = low & MK_MBUTTON;
    event.RightButton = low & MK_RBUTTON;
    event.Y = cursor.y;
    event.Target = nullptr;
    event.AltKey = (MCInput::GetKeyState(VK_MENU) & 0x8000) != 0 ? 0xff : 0;

    if (event.Type != 0)
    {
        HandleEvent(&event);
    }

    return 0;
}

auto ScrollScreen() -> void
{
    MCCamera* camera = nullptr;
    const tagRECT scrollArea = Application->ScrollRect;

    if (TheInterface == nullptr)
    {
        return;
    }

    if (Scenario != nullptr && Turn < 5)
    {
        return;
    }

    int16_t speed = TheInterface->ScrollSpeed;

    if (MainHolder != nullptr && MainHolder->GetActivePane() != nullptr)
    {
        camera = MainHolder->GetActivePane()->GetCamera();
    }

    int32_t dx = 0;
    int32_t dy = 0;

    if (camera != nullptr)
    {
        if (camera->CameraScale == 100)
        {
            speed = static_cast<int16_t>(speed / 2);
        }

        // MCX.EXE's constant is a hair under 15 (14.999999).
        float step = FrameLength * 0x1.dffffep+3f * static_cast<float>(speed);

        // Port: the same speed on the screen at any zoom (the world surface's pixels per screen pixel).
        if (camera->Window != nullptr && camera->Window->WorldScaleY() > 0.0f)
        {
            step /= camera->Window->WorldScaleY();
        }

        bool scroll = true;

        if (TheInterface->ScrollDirection == -1)
        {
            // Scroll by the mouse at the screen's edge, after the interface's start delay.
            const MCPoint cursor = MCInput::GetCursorPos();

            if (PtInRect(&scrollArea, POINT{cursor.x, cursor.y}) == 0)
            {
                if (ScrollWait == 0)
                {
                    ScrollWait = MCPort::Milliseconds();
                }
                else
                {
                    const int16_t delay = TheInterface->ScrollStart;

                    if (static_cast<uint32_t>(delay + static_cast<int32_t>(ScrollWait)) > MCPort::Milliseconds())
                    {
                        scroll = false;
                    }
                }

                if (scroll)
                {
                    if (cursor.x < scrollArea.left)
                    {
                        dx = static_cast<int32_t>(-step);
                    }
                    else if (cursor.x > scrollArea.right)
                    {
                        dx = static_cast<int32_t>(step);
                    }

                    if (cursor.y < scrollArea.top)
                    {
                        dy = static_cast<int32_t>(-step);
                    }
                    else if (cursor.y > scrollArea.bottom)
                    {
                        dy = static_cast<int32_t>(step);
                    }
                }
            }
            else
            {
                ScrollWait = 0;
                scroll = false;
            }
        }
        else
        {
            switch (TheInterface->ScrollDirection)
            {
                case 0:
                    dy = static_cast<int32_t>(-step);
                    break;
                case 1:
                {
                    dx = static_cast<int32_t>(step);
                    dy = static_cast<int32_t>(-step);
                    break;
                }
                case 2:
                    dx = static_cast<int32_t>(step);
                    break;
                case 3:
                {
                    dx = static_cast<int32_t>(step);
                    dy = dx;
                    break;
                }
                case 4:
                    dy = static_cast<int32_t>(step);
                    break;
                case 5:
                {
                    dx = static_cast<int32_t>(-step);
                    dy = static_cast<int32_t>(step);
                    break;
                }
                case 6:
                    dx = static_cast<int32_t>(-step);
                    break;
                case 7:
                {
                    dx = static_cast<int32_t>(-step);
                    dy = dx;
                    break;
                }
                default:
                    scroll = false;
                    break;
            }
        }

        if (scroll && (dx != 0 || dy != 0))
        {
            // Keep the window's anchor point (selectionBox's first corner) on the same spot of the world.
            // Port: the box is in the view's own coordinates, the projection on its world surface (through the zoom).
            MCViewWindow* window = camera->Window;
            MCVector2D anchor(window->SelectionBox[0] / window->WorldScaleX(),
                              window->SelectionBox[1] / window->WorldScaleY());
            MCVector3D point;
            camera->InverseProject(anchor, point);
            camera->ScrollCamera(dx, dy);
            const float scale = camera->CameraScale == 1 ? 0.5f : 1.0f;
            const float offsetX = (point.X - camera->Position.X) * scale;
            const float offsetY = (point.Y - camera->Position.Y) * scale;
            const float offsetZ = scale * (point.Z - camera->Position.Z);
            const MCVector2D moved(offsetY * camera->CosAngle + offsetX * camera->CosAngle + camera->HalfWidth,
                                   ((offsetX * camera->SinAngle + camera->HalfHeight) - offsetY * camera->SinAngle) -
                                       offsetZ);
            const MCVector2D shown = window->WorldToWindow(moved);
            window->SelectionBox[0] = shown.X;
            window->SelectionBox[1] = shown.Y;
        }
    }

    // The tactical map scrolls by its buttons.
    const int16_t mapSpeed = TheInterface->TacScrollSpeed;
    int32_t mapDx = 0;
    int32_t mapDy = 0;

    if (MCTerrain::TerrainTacticalMap == nullptr)
    {
        return;
    }

    switch (TheInterface->TacScrollDirection)
    {
        case 0:
            mapDy = -mapSpeed;
            break;
        case 1:
        {
            mapDx = mapSpeed;
            mapDy = -mapDx;
            break;
        }
        case 2:
            mapDx = mapSpeed;
            break;
        case 3:
        {
            mapDx = mapSpeed;
            mapDy = mapDx;
            break;
        }
        case 4:
            mapDy = mapSpeed;
            break;
        case 5:
        {
            mapDy = mapSpeed;
            mapDx = -mapDy;
            break;
        }
        case 6:
            mapDx = -mapSpeed;
            break;
        case 7:
        {
            mapDx = -mapSpeed;
            mapDy = mapDx;
            break;
        }
        default:
            return;
    }

    if (mapDx == 0 && mapDy == 0)
    {
        return;
    }

    MCTerrain::TerrainTacticalMap->ScrollMap(mapDx, mapDy);
}

auto WindowProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
{
    // Port: the GDI palette work (SelectPalette / RealizePalette on the desktop DC) and the window placement calls
    // have no counterpart: the display owns the palette and the window. What remains is when the original repainted.
    constexpr uint32_t wmErasebkgnd = 0x14;
    constexpr uint32_t scKeymenu = 0xf100;
    constexpr uint32_t scMaximize = 0xf030;
    constexpr uint32_t scMinimize = 0xf020;

    if (message == UMessage)
    {
        return 1;
    }

    switch (message)
    {
        case wmErasebkgnd:
        {
            if (DisplayReady != 0)
            {
                return -1;
            }
            break;
        }
        case WM_DESTROY:
            MCInput::PostQuitMessage(0);
            break;
        case WM_MOVE:
        {
            Application->SetScreenOffsetX(lParam & 0xffff);
            Application->SetScreenOffsetY(static_cast<uint32_t>(lParam) >> 16);
            break;
        }
        case WM_SIZE:
        {
            GWinHeight = static_cast<uint32_t>(lParam) >> 16;
            GWinWidth = lParam & 0xffff;
            // Port: the original snapped the window back to the screen's size (SetWindowPos); the display letterboxes.
            return 0;
        }
        case WM_PAINT:
        {
            if (DisplayReady != 0 && GFullScreen == 0 && KeepDesktopPalette == 0)
            {
                UpdateDisplay(0, 0, 0, 0, 0);
            }
            break;
        }
        case WM_QUERYNEWPALETTE:
        {
            if (DisplayReady == 0 || GFullScreen != 0)
            {
                return 0;
            }

            return -1;
        }
        case WM_PALETTECHANGED:
            return 0;
        case WM_ACTIVATEAPP:
        {
            if (Application->SmackerWindow2 != nullptr)
            {
                CloseMovieWindow(Application->SmackerWindow2);
            }

            if (Application->SmackerWindow != nullptr)
            {
                CloseMovieWindow(Application->SmackerWindow);
            }

            if (MPlayer == nullptr)
            {
                ApplicationActive = static_cast<int>(wParam);
            }
            else if (GFullScreen != 0 && ApplicationActive != 0)
            {
                InitWindowMode();
            }

            if (ApplicationActive != 0 && DisplayReady != 0 && KeepDesktopPalette == 0)
            {
                UpdateDisplay(0, 0, 0, 0, 0);
            }
            break;
        }
        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                if (Application->SmackerWindow2 != nullptr)
                {
                    BlankScreen();
                    BlankScreen();
                    CloseMovieWindow(Application->SmackerWindow2);
                    EscapedSmackerMovie = -1;
                }
                else if (Application->SmackerWindow != nullptr)
                {
                    static_cast<MCGuiSmackerWindow*>(Application->SmackerWindow)->EndSmackerMovie();
                    EscapedSmackerMovie = -1;
                }
            }
            break;
        }
    }

    if (TranslateMessage(window, message, wParam, lParam) != 0)
    {
        return 0;
    }

    if (message == WM_SYSKEYDOWN)
    {
        // Alt+Enter switches between full screen and a window.
        if (wParam == VK_RETURN && AllowMagicWindowSwitching != 0)
        {
            if (Application->SmackerWindow2 != nullptr || DisplayReady == 0 || Application->SmackerWindow != nullptr)
            {
                return 0;
            }

            if (GFullScreen != 0)
            {
                InitWindowMode();
            }
            else
            {
                InitFullScreen();
            }

            return 0;
        }
    }
    else if (message == WM_SYSCHAR)
    {
        return 0;
    }
    else if (message == WM_SYSCOMMAND)
    {
        if (wParam == scKeymenu || AllowMagicWindowSwitching == 0)
        {
            return 0;
        }

        if (wParam == scMaximize && (GFullScreen != 0 || DisplayReady == 0 || Application->SmackerWindow2 != nullptr ||
                                     Application->SmackerWindow != nullptr))
        {
            return 0;
        }

        if (wParam == scMinimize)
        {
            return 0;
        }
    }

    return 0;
}

auto InitWindowMode() -> void
{
    if (GFullScreen != 0)
    {
        if (MouseThreadStarted != 0)
        {
            MouseCritSec.lock();
            InMouseCritSec = 1;
        }

        GFullScreen = 0;
        Application->ResetDirectDraw(GWidth, GHeight, 8);
        DisplayReady = 0;
        // Port: the original centred the window on the desktop the first time (or restored its saved placement)
        // and showed it; leaving full screen puts the SDL window back where it was.
        DisplayReady = 1;

        if (MouseThreadStarted != 0)
        {
            MouseCritSec.unlock();
            InMouseCritSec = 0;
        }
    }
}

auto InitFullScreen() -> void
{
    if (GFullScreen == 0 && Application->DdObject != nullptr)
    {
        if (MouseThreadStarted != 0)
        {
            MouseCritSec.lock();
            InMouseCritSec = 1;
        }

        DisplayReady = 0;
        // Port: the original saved the window's placement and made it a popup (WS_POPUP) first.
        SavedPosition = 1;
        GFullScreen = 1;
        Application->ResetDirectDraw(GWidth, GHeight, 8);

        if (MouseThreadStarted != 0)
        {
            MouseCritSec.unlock();
            InMouseCritSec = 0;
        }
    }
}

auto CheckMouse() -> void
{
    const int16_t alt = MCInput::GetAsyncKeyState(VK_MENU);
    const int16_t ctrl = MCInput::GetAsyncKeyState(VK_CONTROL);
    const int16_t shift = MCInput::GetAsyncKeyState(VK_SHIFT);
    const int16_t left = MCInput::GetAsyncKeyState(VK_LBUTTON);
    const int16_t right = MCInput::GetAsyncKeyState(VK_RBUTTON);
    const auto held = [](int16_t state) -> uint8_t { return (static_cast<uint16_t>(state) >> 15) & 1; };
    MCGuiEvent event;

    if (LeftMouseButtonDown == 0)
    {
        if (left != 0)
        {
            event.Clear();
            event.RightButton = held(right);
            event.CtrlKey = held(ctrl);
            event.ShiftKey = held(shift);
            event.AltKey = held(alt);
            event.Type = 1;
            event.LeftButton = 0xff;
            LeftMouseButtonDown = -1;
            event.X = MouseScreenX;
            event.Y = MouseScreenY;
            HandleEvent(&event);
        }
    }
    else if ((left & 0x8000) == 0)
    {
        event.Clear();
        event.RightButton = held(right);
        event.CtrlKey = held(ctrl);
        event.ShiftKey = held(shift);
        event.AltKey = held(alt);
        LeftMouseButtonDown = 0;
        event.Type = 4;
        event.X = MouseScreenX;
        event.Y = MouseScreenY;
        HandleEvent(&event);
    }

    bool rightChanged = false;

    if (RightMouseButtonDown == 0)
    {
        if (right != 0)
        {
            event.Clear();
            event.LeftButton = held(left);
            event.ShiftKey = held(shift);
            event.Type = 3;
            event.RightButton = 0xff;
            RightMouseButtonDown = -1;
            rightChanged = true;
        }
    }
    else if ((right & 0x8000) == 0)
    {
        event.Clear();
        event.LeftButton = held(left);
        event.ShiftKey = held(shift);
        RightMouseButtonDown = 0;
        event.Type = 6;
        rightChanged = true;
    }

    if (rightChanged)
    {
        event.CtrlKey = held(ctrl);
        event.AltKey = held(alt);
        event.X = MouseScreenX;
        event.Y = MouseScreenY;
        HandleEvent(&event);
    }

    if (OldMouseX != MouseScreenX || OldMouseY != MouseScreenY)
    {
        event.Clear();
        event.LeftButton = held(left);
        event.RightButton = held(right);
        event.Type = 7;
        event.AltKey = held(alt);
        event.Y = MouseScreenY;
        OldMouseY = MouseScreenY;
        event.CtrlKey = held(ctrl);
        event.ShiftKey = held(shift);
        event.X = MouseScreenX;
        OldMouseX = MouseScreenX;
        HandleEvent(&event);
    }
}

auto GetPaletteFromArt(char* fileName) -> MCVfxRgb*
{
    char path[252];
    char message[256];
    MCFile artFileHandle;
    std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

    if (artFileHandle.Open(path, READ, 50) != 0)
    {
        MCPort::StrCopy(path, sizeof(path), fileName);

        if (artFileHandle.Open(path, READ, 50) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
        }
    }

    const uint32_t size = artFileHandle.FileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
    }

    if (size == 0)
    {
        return nullptr;
    }

    std::vector<uint8_t> tga(size);
    artFileHandle.Read(tga.data(), static_cast<int32_t>(size));
    artFileHandle.Close();
    // The caller deletes[] the palette.
    auto* palette = new MCVfxRgb[256]{};
    TgaColorMapToPalette(tga.data(), palette);
    return palette;
}

// aSystem.

auto MCGuiSystem::Start(void* instance, void* prevInstance, char* commandLine, int showCommand, int16_t screenWidth,
                        int16_t screenHeight) -> int
{
    (void)prevInstance;
    (void)showCommand;
    DdObject = nullptr;
    DdObject2 = nullptr;
    DdPrimarySurface = nullptr;
    DdBackSurface = nullptr;
    DdPalette = nullptr;
    Backpbmi = nullptr;
    WindowHandle = nullptr;
    NumCallbacks = 0;
    NumChildren = 0;
    Grabbed = nullptr;
    TextFocus = nullptr;
    Current = nullptr;
    Parent = nullptr;
    Modal = nullptr;
    ThePalette = nullptr;
    GammaLevel = 0;
    SmackerWindow = nullptr;
    SmackerWindow2 = nullptr;
    OpeningSmackerWindow = nullptr;
    FlipToGdiRequested = 0;
    PaletteCycle = -1;
    char title[256];
    char name[256];
    char version[256];
    char release[256];
    CLoadString(ThisInstance, 0x284, title, 0xfe);
    CLoadString(ThisInstance, 0x283, title, 0xfe);
    CLoadString(ThisInstance, 0x280, name, 0xfe);
    CLoadString(ThisInstance, 0x281, version, 0xfe);
    CLoadString(ThisInstance, 0x282, release, 0xfe);
    // Port: the window is named after the port, not the string table's title (string 0x283).
#ifdef _DEBUG
    std::strcpy(AppName, "MechCommander Redux (Debug)");
#else
    std::strcpy(AppName, "MechCommander Redux");
#endif
    std::strcpy(WindowTitle, AppName);
    OffsetX = 0;
    OffsetY = 0;

    // Port: the original gave up when another copy was running (a window of class "MCX", brought to the front, or
    // the "MCX" file mapping), registered the window class, and checked for a Pentium with CPUID (Processor 1, 2
    // with MMX, 3 for a 486 without CPUID; 0 refused to run). Every x64 processor has MMX.
    Processor = 2;

    DisplayWidth = screenWidth;
    DisplayHeight = screenHeight;
    SystemInit();
    const int32_t width = static_cast<int16_t>(DisplayWidth);
    const int32_t height = static_cast<int16_t>(DisplayHeight);
    this->ScreenWidth = width;
    this->ScreenHeight = height;

    for (MCGuiCallback*& callback : Callbacks)
    {
        callback = nullptr;
    }

    ParseCommandLine(commandLine);

    // Port: the window is the display's, made by startupDirectDraw below; the original made it here (a popup in
    // full screen, else a caption window sized to the screen) and gave it the focus. Messages reach WindowProc
    // through the platform layer.
    MCInput::SetWindowProc(
        [](uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
        { return WindowProc(Application != nullptr ? Application->WindowHandle : nullptr, message, wParam, lParam); });

    BlackFont = LoadFont("blkfnt.fnt");
    Fonts[0][0] = BlackFont;
    RedFont = LoadFont("red.fnt");
    Fonts[1][0] = RedFont;
    YellowFont = LoadFont("yellow.fnt");
    Fonts[2][0] = YellowFont;
    GreenFont = LoadFont("green.fnt");
    Fonts[3][0] = GreenFont;
    BlueFont = LoadFont("blue.fnt");
    Fonts[4][0] = BlueFont;
    GreyFont = LoadFont("gryfnt.fnt");
    Fonts[5][0] = GreyFont;
    WhiteFont = LoadFont("white.fnt");
    Fonts[6][0] = WhiteFont;
    DimFont = LoadFont("dim.fnt");
    Fonts[7][0] = DimFont;
    YellowDropFont = LoadFont("yelldrp.fnt");
    Fonts[8][0] = YellowDropFont;
    BlueDropFont = LoadFont("bluedrp.fnt");
    Fonts[9][0] = BlueDropFont;
    MedBlackFont = LoadFont("blkfnt10.fnt");
    Fonts[0][1] = MedBlackFont;
    MedRedFont = LoadFont("red10.fnt");
    Fonts[1][1] = MedRedFont;
    MedYellowFont = LoadFont("yellow10.fnt");
    Fonts[2][1] = MedYellowFont;
    MedGreenFont = LoadFont("green10.fnt");
    Fonts[3][1] = MedGreenFont;
    MedBlueFont = LoadFont("blue10.fnt");
    Fonts[4][1] = MedBlueFont;
    MedGreyFont = LoadFont("gryfnt10.fnt");
    Fonts[5][1] = MedGreyFont;
    MedWhiteFont = LoadFont("white10.fnt");
    Fonts[6][1] = MedWhiteFont;
    MedDimFont = LoadFont("dim10.fnt");
    Fonts[7][1] = MedDimFont;
    Fonts[8][1] = YellowDropFont;
    Fonts[9][1] = BlueDropFont;
    LgBlackFont = LoadFont("blkfnt12.fnt");
    Fonts[0][2] = LgBlackFont;
    LgRedFont = LoadFont("red12.fnt");
    Fonts[1][2] = LgRedFont;
    LgYellowFont = LoadFont("yellow12.fnt");
    Fonts[2][2] = LgYellowFont;
    LgGreenFont = LoadFont("green12.fnt");
    Fonts[3][2] = LgGreenFont;
    LgBlueFont = LoadFont("blue12.fnt");
    Fonts[4][2] = LgBlueFont;
    LgGreyFont = LoadFont("gryfnt12.fnt");
    Fonts[5][2] = LgGreyFont;
    LgWhiteFont = LoadFont("white12.fnt");
    Fonts[6][2] = LgWhiteFont;
    LgDimFont = LoadFont("dim12.fnt");
    Fonts[7][2] = LgDimFont;
    SystemFont = GreyFont;

    // The engine's line font (Font's constructor, inlined).
    LineFont = new (std::nothrow) MCFont;
    LineFont->CurY = 0;
    LineFont->CurX = 0;
    LineFont->Color = 0xf;
    LineFont->Scale = 2.0f;
    LineFont->Scaled = -1;
    LineFont->FontData.reset();

    for (uint8_t*& letter : LineFont->LetterCache)
    {
        letter = reinterpret_cast<uint8_t*>(intptr_t{-1});
    }

    LineFont->Init(const_cast<char*>("font"));

    MCPalette* palette = new MCPalette();

    if (palette == nullptr)
    {
        GamePalette = nullptr;
        Fatal(-1, " No RAM for Game palette ");
    }

    palette->Init();
    GamePalette = palette;
    const int32_t paletteResult = GamePalette->Init(const_cast<char*>("palette"));

    if (paletteResult != 0)
    {
        Fatal(paletteResult, " Unable to initialize game palette ");
    }

    InitAlphaLookup(reinterpret_cast<MCVfxRgb*>(GamePalette->RgbData.get()));

    ArtFile = new MCPacketFile;
    Assert(ArtFile != nullptr, 0, "Not enough RAM for artFile (Something's way wrong...)");
    {
        MCFullPathFileName artFileName;
        artFileName.Init(ArtPath, "art", ".pak");

        if (ArtFile->Open(artFileName, READ, 50) != 0)
        {
            Fatal(0, "Error opening art file");
        }
    }

    StartupDirectDraw(width, height, 8);
    WindowHandle = GameDisplay.get();
    GhWindow = WindowHandle;

    CursorShapeBlocks.Clear();
    CursorShapeTable.fill(nullptr);
    CursorShapes = CursorShapeTable.data();
    MCFullPathFileName cursorFileName;
    cursorFileName.Init(SpritePath, "cursors", ".pak");
    MCPacketFile* cursorFile = new MCPacketFile;

    if (cursorFile->Open(cursorFileName, READ, 50) != 0)
    {
        MCFullPathFileName cdCursorFileName;
        cdCursorFileName.Init(CDspritePath, "cursors", ".pak");

        if (cursorFile->Open(cdCursorFileName, READ, 50) != 0)
        {
            Fatal(0, "Cannot find cursors.pak file");
        }
    }

    const int32_t numCursors = cursorFile->GetNumPackets();

    if (numCursors > 0x7f)
    {
        Fatal(-1, " Too Many cursor Shapes ");
    }

    for (int32_t i = 0; i < numCursors; i++)
    {
        cursorFile->SeekPacket(i);
        const auto size = static_cast<uint32_t>(cursorFile->GetPacketSize());
        CursorShapes[i] = static_cast<uint8_t*>(CursorShapeBlocks.Allocate(size));

        // An empty packet still fails, as it did when systemHeap's malloc(0) returned null.
        if (CursorShapes[i] == nullptr)
        {
            Fatal(-1, " no RAM for cursors ");
        }

        cursorFile->ReadPacket(i, CursorShapes[i]);
        MCRenderer::RegisterData(CursorShapes[i], size, MCDataKind::Shapes);
    }

    cursorFile->Close();
    delete cursorFile;
    MCHardwareCursorPreload();

    MCInput::ShowCursor(false);
    CursorShape = -1;
    MouseTimerInit();

    ScreenPort = new MCGuiPort;
    const int32_t portResult = ScreenPort->Init(width, height);

    if (portResult != 0)
    {
        Fatal(portResult, "Unable to create screenPort");
    }

    // The screen port shows the display's buffer, set by aLockScreen.
    ScreenPort->Bitmap()->Buffer = nullptr;
    ScreenWindow = new MCGuiObject;
    ScreenWindow->Init(0, 0, this->ScreenWidth, this->ScreenHeight, nullptr);
    ScreenWindow->SetDepth(-100);
    ScreenWindow->ObjectType = 1;
    GamePalette->Activate(0, 0);
    CountsPerSecond = MCPort::PerformanceFrequency();
    UpdateDisplay(0, 0, 0, 0, 0);
    AUnlockScreen();

    MCGuiTimerManager* timers = new MCGuiTimerManager;
    TimerManager = timers;
    timers->Init();
    TheInterface = new MCInterfaceObject;

    if (TheInterface == nullptr)
    {
        return 2;
    }

    TheInterface->Init();

    if (UserInit() != 0)
    {
        return -10;
    }

    SetScrollRect();
    CursorHidden = 0;
    SetCurrentCursor(static_cast<MCCursorType>(0));
    SetCursorVisible(0);
    MouseTrackerCallback = new MCGuiCallback;
    MouseTrackerCallback->SetExec(CheckMouse);
    Application->AddCallback(MouseTrackerCallback);
    (void)instance;
    return 0;
}

auto MCGuiSystem::Stop() -> void
{
    DestroyAllFitFiles(SaveTempPath);
    // The temp folder is this process's own (temp\<pid>\, see systemInit): it goes with its files.
    MCFileSystem::RemoveDirectory(SaveTempPath);
    Application->RemoveCallback(MouseTrackerCallback);

    if (MouseTrackerCallback != nullptr)
    {
        delete MouseTrackerCallback;
    }

    MouseTrackerCallback = nullptr;

    if (Mission != nullptr && Mission->ResultsScreen != nullptr)
    {
        Mission->ResultsScreen->Destroy();
        delete Mission->ResultsScreen;
        Mission->ResultsScreen = nullptr;
    }

    UserDestroy();

    if (MPlayer != nullptr)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    MouseTimerKill();
    SoundRendererUninstall();
    MCInput::ShowCursor(true);
    ShutdownDirectDraw();
    MCInput::ClipCursor(nullptr);
    Application->SetCurrentObject(nullptr);

    if (StartupPakFile != nullptr)
    {
        delete[] StartupPakFile;
    }

    if (TheInterface != nullptr)
    {
        // The original freed it without its destructor (the aObject teardown: its port and timers).
        TheInterface->Destroy();
        delete TheInterface;
        TheInterface = nullptr;
    }

    if (MainHolder != nullptr)
    {
        MainHolder->Destroy();
        delete MainHolder;
        MainHolder = nullptr;
    }

    if (StopWindow1 != nullptr)
    {
        StopWindow1->Destroy();
        delete StopWindow1;
        StopWindow1 = nullptr;
    }

    if (StopWindow2 != nullptr)
    {
        StopWindow2->Destroy();
        delete StopWindow2;
        StopWindow2 = nullptr;
    }

    if (ScreenWindow != nullptr)
    {
        ScreenWindow->Destroy();
        delete ScreenWindow;
        ScreenWindow = nullptr;
    }

    if (GamePalette != nullptr)
    {
        GamePalette->Destroy();
        delete GamePalette;
        GamePalette = nullptr;
    }

    // Port: the GDI palette, back bitmap and its BITMAPINFO (thePalette, backbm, backpbmi) never exist.
    ThePalette = nullptr;
    Backbm = nullptr;
    Backpbmi = nullptr;

    if (ArtFile != nullptr)
    {
        ArtFile->Close();
        delete ArtFile;
        ArtFile = nullptr;
    }

    DeleteFont(BlackFont);
    DeleteFont(GreyFont);

    if (GreyFont == nullptr)
    {
        SystemFont = nullptr;
    }

    DeleteFont(WhiteFont);
    DeleteFont(RedFont);
    DeleteFont(GreenFont);
    DeleteFont(BlueFont);
    DeleteFont(YellowFont);
    DeleteFont(DimFont);
    DeleteFont(YellowDropFont);
    DeleteFont(BlueDropFont);
    DeleteFont(MedBlackFont);
    DeleteFont(MedGreyFont);
    DeleteFont(MedWhiteFont);
    DeleteFont(MedRedFont);
    DeleteFont(MedGreenFont);
    DeleteFont(MedBlueFont);
    DeleteFont(MedYellowFont);
    DeleteFont(MedDimFont);
    DeleteFont(LgBlackFont);
    DeleteFont(LgGreyFont);
    DeleteFont(LgWhiteFont);
    DeleteFont(LgRedFont);
    DeleteFont(LgGreenFont);
    DeleteFont(LgBlueFont);
    DeleteFont(LgYellowFont);
    DeleteFont(LgDimFont);

    if (LineFont != nullptr)
    {
        // Font's destroy and destructor, inlined: frees the font data and forgets the cached letters.
        LineFont->FontData.reset();

        for (uint8_t*& letter : LineFont->LetterCache)
        {
            letter = reinterpret_cast<uint8_t*>(intptr_t{-1});
        }

        delete LineFont;
        LineFont = nullptr;
    }

    if (TimerManager != nullptr)
    {
        // The destructor has no timers left to free after destroy.
        TimerManager->Destroy();
        delete TimerManager;
        TimerManager = nullptr;
    }

    // The cursor shapes went with systemHeap in the original.
    CursorShapes = nullptr;
    CursorShapeBlocks.Clear();

    if (LZPacketBuffer != nullptr)
    {
        std::free(LZPacketBuffer);
        LZPacketBuffer = nullptr;
    }

    // Port: the original repainted the desktop (InvalidateRect of every window).
}

auto MCGuiSystem::StartSmackerMovie(char* fileName, uint32_t flags, MCGuiObject* window, int exclusive) -> int32_t
{
    (void)flags;
    MCSmackTag* movie = SmackOpen(fileName, 0xfe000, -1);

    if (movie == nullptr)
    {
        return static_cast<int32_t>(0xddddd002);
    }

    if (window == nullptr)
    {
        // A window of the movie's size, centred on the screen.
        const int32_t movieWidth = movie->Player->Width();
        const int32_t movieHeight = movie->Player->Height();
        const int32_t screenW = Application->Width();
        const int32_t screenH = Application->Height();
        MCGuiSmackerWindow* movieWindow = new MCGuiSmackerWindow;

        if (movieWindow == nullptr)
        {
            return 3;
        }

        window = movieWindow;
        const int32_t result = window->Init(static_cast<int32_t>(static_cast<uint32_t>(screenW - movieWidth) >> 1),
                                            static_cast<int32_t>(static_cast<uint32_t>(screenH - movieHeight) >> 1),
                                            movieWidth, movieHeight, const_cast<char*>("Movie Time"));

        if (result != 0)
        {
            return result;
        }
    }

    SmackWindowPointer = window;
    const int32_t result = static_cast<MCGuiSmackerWindow*>(window)->StartSmackerMovie(movie, exclusive);

    if (result != 0)
    {
        return result;
    }

    if (exclusive != 0)
    {
        SmackerWindow = window;
    }

    window->SetDepth(100);
    ScreenWindow->AddChild(window);
    window->Draw();
    return 0;
}

auto MCGuiSystem::Run() -> void
{
    for (int32_t i = 0; i < ScreenWindow->NumberOfChildren(); i++)
    {
        ScreenWindow->Child(i)->Draw();
    }

    UpdateDisplay(0, 0, 0, 0, 0);
    int quit = 0;

    do
    {
        PerfStartTime = MCPort::PerformanceCounter();
        MCFrameLog::NextFrame();

        // Port: the PeekMessage / TranslateMessage / DispatchMessage loop, which stopped at WM_QUIT.
        if (MCFrameLog::Scope pump("pump"); !MCInput::PumpMessages())
        {
            quit = -1;
        }

        if (ApplicationActive != 0)
        {
            if (SmackerWindow2 == nullptr && SmackerWindow == nullptr)
            {
                MCFrameLog::Scope logic("logic");
                const int32_t count = NumCallbacks;

                for (int32_t i = 0; i < count; i++)
                {
                    if (Callbacks[i] != nullptr)
                    {
                        Callbacks[i]->Execute();
                    }
                }
            }

            int32_t staticNoise = 0;
            int32_t noiseChance = 0;

            if (Scenario != nullptr)
            {
                staticNoise = Scenario->StartingUp;
                noiseChance = Scenario->StartUpCountdown;
            }

            UpdateDisplay(TakeScreenShot, staticNoise, noiseChance, 0, 0);
            TakeScreenShot = 0;
        }
        else if (quit == 0)
        {
            MCInput::WaitMessage(-1);
        }
        else
        {
            // Quitting while inactive: close the movies and the feature screen.
            if (Application->SmackerWindow2 != nullptr)
            {
                CloseMovieWindow(Application->SmackerWindow2);
            }
            else if (Application->SmackerWindow != nullptr)
            {
                CloseMovieWindow(Application->SmackerWindow);
            }

            if (FeatureScreen != nullptr)
            {
                ScreenWindow->RemoveChild(FeatureScreen);
                delete FeatureScreen;
                FeatureScreen = nullptr;
            }
        }

        PerfStopTime = MCPort::PerformanceCounter();

        // The frame's length. The counters are split in 32-bit halves and the low halves' difference taken as a
        // signed 32-bit number, so a carry into the high half counts 2^32 too many (OB-062): that frame reads as
        // longer than 0.25 s and is clamped.
        const int32_t highDifference = static_cast<int32_t>(static_cast<uint64_t>(PerfStopTime) >> 32) -
                                       static_cast<int32_t>(static_cast<uint64_t>(PerfStartTime) >> 32);
        const int32_t lowDifference =
            static_cast<int32_t>(static_cast<uint32_t>(PerfStopTime) - static_cast<uint32_t>(PerfStartTime));
        PrevStart = PerfStartTime;
        double elapsed = static_cast<double>(highDifference) * 4294967296.0 + static_cast<double>(lowDifference);

        if (elapsed == 0.0)
        {
            elapsed = 9.999999747378752e-05;
        }

        const double countsLow = static_cast<double>(static_cast<uint32_t>(CountsPerSecond));
        FrameLength = static_cast<float>(elapsed / countsLow);
        FrameRate = static_cast<float>(
            (static_cast<double>(static_cast<int32_t>(static_cast<uint64_t>(CountsPerSecond) >> 32)) * 4294967296.0 +
             countsLow) /
            elapsed);

        if (FrameLength > 0.25f)
        {
            FrameLength = 0.25f;
        }

        if (FrameRate < 4.0f)
        {
            FrameRate = 4.0f;
        }

        if (LockFrameRate != 0)
        {
            const int32_t wait = static_cast<int32_t>(67.0f - FrameLength * 1000.0f);

            if (wait > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                FrameLength = 0.06666667f;
                FrameRate = 15.0f;
            }
        }
    } while (quit == 0);
}

auto MCGuiSystem::Width() -> int32_t
{
    return ScreenWidth;
}

auto MCGuiSystem::Height() -> int32_t
{
    return ScreenHeight;
}

auto MCGuiSystem::ScreenOffsetX() -> int32_t
{
    return OffsetX;
}

auto MCGuiSystem::ScreenOffsetY() -> int32_t
{
    return OffsetY;
}

auto MCGuiSystem::Window() -> void*
{
    return WindowHandle;
}

auto MCGuiSystem::SetScreenOffsetX(int32_t offset) -> void
{
    OffsetX = offset;
}

auto MCGuiSystem::SetScreenOffsetY(int32_t offset) -> void
{
    OffsetY = offset;
}

auto MCGuiSystem::AddCallback(MCGuiCallback* callback) -> int32_t
{
    if (NumCallbacks == 0x62)
    {
        return 3;
    }

    if (callback == nullptr)
    {
        return 2;
    }

    Callbacks[NumCallbacks] = callback;
    NumCallbacks++;
    return 0;
}

auto MCGuiSystem::RemoveCallback(MCGuiCallback* callback) -> int32_t
{
    if (callback == nullptr)
    {
        return 2;
    }

    for (int32_t i = 0; i < NumCallbacks; i++)
    {
        if (Callbacks[i] == callback)
        {
            for (; i < NumCallbacks - 1; i++)
            {
                Callbacks[i] = Callbacks[i + 1];
            }

            NumCallbacks--;
            Callbacks[NumCallbacks] = nullptr;
            return 0;
        }
    }

    return 1;
}

auto MCGuiSystem::SetModalObject(MCGuiObject* obj) -> void
{
    Modal = obj;
    obj->BringToFront(0);
}

auto MCGuiSystem::ClearModal() -> void
{
    Modal = nullptr;
}

auto MCGuiSystem::Grab(MCGuiObject* obj) -> void
{
    Assert(RecordClicks == 0 || MCTerrain::TerrainTacticalMap == nullptr ||
               obj != MCTerrain::TerrainTacticalMap->ScrollButtons[5],
           0, " Get Jon! Or save this for him! ");
    Grabbed = obj;
    MCInput::SetCapture();
}

auto MCGuiSystem::SetText(MCGuiObject* obj) -> void
{
    if (TextFocus != nullptr)
    {
        ReleaseText();
    }

    TextFocus = obj;

    if (obj != nullptr)
    {
        MCGuiEvent event;
        event.Clear();
        event.Type = 0x1e;
        event.Target = obj;
        event.Data = 7;
        obj->HandleEvent(&event);
    }
}

auto MCGuiSystem::SetCurrentObject(MCGuiObject* obj) -> void
{
    Current = obj;
}

auto MCGuiSystem::Release() -> void
{
    Grabbed = nullptr;
    MCInput::ReleaseCapture();
}

auto MCGuiSystem::ReleaseText() -> void
{
    MCGuiObject* obj = TextFocus;

    if (obj != nullptr)
    {
        MCGuiEvent event;
        event.Clear();
        event.Type = 0x1e;
        event.Data = 8;
        event.Target = obj;
        obj->HandleEvent(&event);
        TextFocus = nullptr;
    }
}

auto MCGuiSystem::GrabbedObject() -> MCGuiObject*
{
    return Grabbed;
}

auto MCGuiSystem::TextObject() -> MCGuiObject*
{
    return TextFocus;
}

auto MCGuiSystem::CurrentObject() -> MCGuiObject*
{
    return Current;
}

auto MCGuiSystem::TweakDDPalette(int first, int count, MCVfxRgb* colors, int sixBit) -> int
{
    if (DisplayReady == 0)
    {
        return 0;
    }

    if (SmackerWindow == nullptr)
    {
        // Entries 0..9 and 246..255 are Windows' own unless a movie plays.
        if (first < 10)
        {
            const int skipped = 10 - first;
            first = 10;
            colors += skipped;
            count -= skipped;
        }

        if (count + first > 0xf6)
        {
            count = 0xf6 - first;
        }
    }

    const int end = count + first;

    for (int i = first; i < end; i++, colors++)
    {
        MCVfxRgb color = *colors;

        if (sixBit != 0)
        {
            color.R = static_cast<uint8_t>(color.R << 2);
            color.G = static_cast<uint8_t>(color.G << 2);
            color.B = static_cast<uint8_t>(color.B << 2);
        }

        CurrentPalette[i] = color;
        LogicalPalette[i] = color;
    }

    if (GammaLevel != 0)
    {
        for (int i = first; i < end; i++)
        {
            LogicalPalette[i].R = GammaColorTranslation[CurrentPalette[i].R];
            LogicalPalette[i].G = GammaColorTranslation[CurrentPalette[i].G];
            LogicalPalette[i].B = GammaColorTranslation[CurrentPalette[i].B];
        }

        if (GammaLevel == 2)
        {
            for (int i = first; i < end; i++)
            {
                LogicalPalette[i].R = GammaColorTranslation[LogicalPalette[i].R];
                LogicalPalette[i].G = GammaColorTranslation[LogicalPalette[i].G];
                LogicalPalette[i].B = GammaColorTranslation[LogicalPalette[i].B];
            }
        }
    }

    ShowPalette(first, count);
    // The original returned AnimatePalette's -1 in a window, and whether SetEntries succeeded in full screen.
    return GFullScreen != 0 ? 1 : -1;
}

auto MCGuiSystem::GammaCorrectCurrentPalette() -> void
{
    if (DisplayReady != 0)
    {
        GammaLevel++;

        if (GammaLevel > 3)
        {
            GammaLevel = 0;
        }

        GammaCorrectCurrentPalette(GammaLevel);
    }
}

auto MCGuiSystem::GammaCorrectCurrentPalette(int32_t level) -> void
{
    if (DisplayReady == 0)
    {
        return;
    }

    GammaLevel = level;

    // Entries 10..245, through the gamma table once per level.
    for (int i = 10; i < 0xf6; i++)
    {
        LogicalPalette[i] = CurrentPalette[i];
    }

    for (int32_t pass = 0; pass < level && pass < 3; pass++)
    {
        for (int i = 10; i < 0xf6; i++)
        {
            LogicalPalette[i].R = GammaColorTranslation[LogicalPalette[i].R];
            LogicalPalette[i].G = GammaColorTranslation[LogicalPalette[i].G];
            LogicalPalette[i].B = GammaColorTranslation[LogicalPalette[i].B];
        }
    }

    ShowPalette(10, 0xec);
}

auto MCGuiSystem::FadeDownCurrentPalette() -> void
{
    if (DisplayReady == 0 || CurrentPalette[10].R == 0)
    {
        return;
    }

    // Darkens the screen by a step that follows the time each step took (256 levels a second), waiting at least
    // 1/256 s a step, until 256 levels are gone. The original took each step off palette entries 10..245; the port
    // fades the shown picture (MCDisplay::SetFade): the GPU's surfaces hold colours, so a palette change alone doesn't
    // reach what's already drawn, and the frame isn't redrawn during the fade.
    MCDisplay* display = MCInput::Display();
    int32_t step = 1;
    int32_t faded = 0;
    const double frequency = static_cast<double>(CountsPerSecond);

    do
    {
        const int64_t stepStart = MCPort::PerformanceCounter();
        faded += step;

        if (display != nullptr)
        {
            display->SetFade(faded);
            (void)display->Present();
        }

        float elapsed;

        do
        {
            elapsed = static_cast<float>(static_cast<double>(MCPort::PerformanceCounter() - stepStart) / frequency);
        } while (elapsed < 0.00390625f);

        step = static_cast<int32_t>(elapsed * 256.0f);
    } while (faded < 0x100);

    // Where the original ends: entries 10..245 black until the next palette is set. The fade comes off with them, so
    // the screen stays black.
    for (int i = 10; i < 0xf6; i++)
    {
        CurrentPalette[i] = {};
        LogicalPalette[i] = {};
    }

    ShowPalette(10, 0xec);

    if (display != nullptr)
    {
        display->SetFade(0);
    }
}

auto MCGuiSystem::ActivatePalette(uint8_t* colors, int first, int count) -> void
{
    if (first != 0)
    {
        TweakDDPalette(first, count, reinterpret_cast<MCVfxRgb*>(colors + first * 3), -1);
        return;
    }

    // From entry 0 the palette is only remembered.
    if (PaletteRgb == nullptr)
    {
        PaletteRgb = new MCVfxRgb[256]{};
    }

    GlobalEntries = count;
    GlobalFirst = 0;
    std::memcpy(PaletteRgb, colors, static_cast<size_t>(count * 3));
}

auto MCGuiSystem::ActivatePaletteFromTga(char* fileName) -> void
{
    char path[252];
    char message[256];
    MCFile tgaFile;
    std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

    if (tgaFile.Open(path, READ, 50) != 0)
    {
        MCPort::StrCopy(path, sizeof(path), fileName);

        if (tgaFile.Open(path, READ, 50) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
        }
    }

    const uint32_t size = tgaFile.FileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
    }

    if (size == 0)
    {
        return;
    }

    std::vector<uint8_t> tga(size);
    tgaFile.Read(tga.data(), static_cast<int32_t>(size));
    tgaFile.Close();
    std::array<MCVfxRgb, 256> palette = {};
    TgaColorMapToPalette(tga.data(), palette.data());
    ActivatePalette(reinterpret_cast<uint8_t*>(palette.data()), 0, 0x100);
    InitAlphaLookup(palette.data());
}

auto MCGuiSystem::ActivatePaletteFromGif(char* fileName) -> void
{
    // Reads the GIF's palette and does nothing with it.
    char path[128];
    MCFile gifFile;
    std::snprintf(path, sizeof(path), "%s%s", PalettePath, fileName);

    if (FileExists(path) == 0)
    {
        std::strncpy(path, fileName, 0x7f);
        Assert(FileExists(path), 0, "Unable to find palette .gif");
    }

    gifFile.Open(path, READ, 50);
    const uint32_t size = gifFile.FileSize();
    Assert(size != 0, 0, "Error reading from palette gif");
    std::vector<uint8_t> gif(size);
    gifFile.Read(gif.data(), static_cast<int32_t>(size));
    gifFile.Close();
    MCVfxRgb colors[256];
    VfxGifPalette(gif.data(), colors);
}

auto MCGuiSystem::ActivateSmackerPalette(uint8_t* colors) -> void
{
    TweakDDPalette(0, 0x100, reinterpret_cast<MCVfxRgb*>(colors), 0);
}

auto MCGuiSystem::AddTimer(MCGuiObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                           int useScenarioTime) -> int32_t
{
    LockMouse();
    const int32_t result =
        TimerManager->AddTimer(target, id, static_cast<uint32_t>(interval), eventType, eventData, useScenarioTime);
    UnlockMouse();
    return result;
}

auto MCGuiSystem::AddUniqueTimer(MCGuiObject* target, int16_t id, int32_t interval, int32_t eventType,
                                 int32_t eventData, int useScenarioTime) -> int32_t
{
    LockMouse();
    const int32_t result = TimerManager->AddUniqueTimer(target, id, static_cast<uint32_t>(interval), eventType,
                                                        eventData, useScenarioTime);
    UnlockMouse();
    return result;
}

auto MCGuiSystem::RemoveTimer(MCGuiObject* target, int16_t id) -> void
{
    MCGuiTimerManager* manager = TimerManager;

    if (manager != nullptr)
    {
        LockMouse();
        manager->RemoveTimer(target, id);
        UnlockMouse();
    }
}

auto MCGuiSystem::RemoveTimers(MCGuiObject* target) -> void
{
    MCGuiTimerManager* manager = TimerManager;

    if (manager != nullptr)
    {
        LockMouse();
        manager->RemoveTimers(target);
        UnlockMouse();
    }
}

auto MCGuiSystem::SetCurrentCursor(MCCursorType cursor) -> void
{
    if (CursorHidden != 0)
    {
        return;
    }

    AGMouseFrame = 0;
    CurrentCursor = cursor;
    CursorShape = static_cast<int32_t>(cursor);

    // 0xf..0x11 are offset by the interface's cursor set; 0x12 is shape 1.
    switch (static_cast<int32_t>(cursor))
    {
        case 0xf:
        {
            if (TheInterface != nullptr)
            {
                CursorShape = TheInterface->CursorOffset + 0xf;
            }
            break;
        }
        case 0x10:
        {
            if (TheInterface != nullptr)
            {
                CursorShape = TheInterface->CursorOffset + 0x2f;
            }
            break;
        }
        case 0x11:
        {
            if (TheInterface != nullptr)
            {
                CursorShape = TheInterface->CursorOffset + 0x4f;
            }
            break;
        }
        case 0x12:
            CursorShape = 1;
            break;
    }
}

auto MCGuiSystem::SetCursorVisible(int show) -> void
{
    if (show != 0)
    {
        CursorHidden = 0;
        SetCurrentCursor(static_cast<MCCursorType>(0));
        return;
    }

    CursorShape = -1;
    CursorHidden = -1;
}

// aCallback.

MCGuiCallback::MCGuiCallback()
{
    Destroy();
}

namespace
{
    /// <summary>
    /// Port: the callback whose function is running (<see cref="MCGuiCallback::Execute"/>), cleared when it is deleted.
    /// </summary>
    MCGuiCallback* ExecutingCallback = nullptr;
}

MCGuiCallback::~MCGuiCallback()
{
    if (this == ExecutingCallback)
    {
        ExecutingCallback = nullptr;
    }

    Destroy();
}

auto MCGuiCallback::Destroy() -> void
{
    Exec = nullptr;
    Message = 0;
    Object = nullptr;
}

auto MCGuiCallback::Execute() -> void
{
    if (Exec != nullptr)
    {
        // Port fix: a function can delete its own callback (DancingButtons deletes moveCallback). The original then
        // read message and object from the freed block, which destroy() had zeroed, so it posted nothing. Return
        // instead of reading freed memory (OB-109).
        MCGuiCallback* outerCallback = ExecutingCallback;
        ExecutingCallback = this;
        Exec();
        const bool deleted = ExecutingCallback != this;
        ExecutingCallback = outerCallback;

        if (deleted)
        {
            return;
        }
    }

    if (Message != 0 && Object != nullptr)
    {
        APostMessage(Object, Message);
    }
}

auto MCGuiCallback::SetExec(void (*func)()) -> void
{
    Exec = func;
}

auto MCGuiCallback::SetMessage(MCGuiObject* obj, int32_t msg) -> void
{
    Message = msg;
    Object = obj;
}

auto MCGuiEvent::Clear() -> void
{
    // data (+0x1c) and lParam (+0x20) are left as they were.
    Type = 0;
    Target = nullptr;
    LeftButton = 0;
    MiddleButton = 0;
    RightButton = 0;
    AltKey = 0;
    CtrlKey = 0;
    ShiftKey = 0;
    Key = 0;
    ScanCode = 0;
    X = 0;
    Y = 0;
}

auto TimerCallback() -> void
{
    const uint32_t now = MCPort::Milliseconds();
    int32_t count = Application->TimerManager->NumTimers;

    for (int32_t i = 0; i < Application->TimerManager->NumTimers; i++)
    {
        MCGuiTimerManager* manager = Application->TimerManager;
        MCGuiTimer* timer = manager->GetTimer(static_cast<int16_t>(i));

        if (timer == nullptr)
        {
            continue;
        }

        const uint32_t due = timer->LastTime + timer->Interval;

        if (timer->UseScenarioTime != 0)
        {
            if (!(static_cast<double>(due) < static_cast<double>(ScenarioTime) * 1000.0))
            {
                continue;
            }
        }
        else if (now <= due)
        {
            continue;
        }

        const tagPOINT cursor = GetMessageCursorLoc();
        MCGuiEvent event;

        if (timer->EventType != 0)
        {
            // A one-shot event: sent, then the timer goes (unless the handler changed the timer list).
            event.Clear();
            event.X = cursor.x;
            event.Y = cursor.y;
            event.Data = timer->EventData;
            event.Type = timer->EventType;
            manager = Application->TimerManager;
            manager->LockTimersExcept(manager->GetTimer(static_cast<int16_t>(i)));
            timer->Target->HandleEvent(&event);
            Application->TimerManager->UnlockTimers();

            if (count == Application->TimerManager->NumTimers)
            {
                LockMouse();
                Application->TimerManager->RemoveTimer(i);
                UnlockMouse();
            }
            else
            {
                count = Application->TimerManager->NumTimers;
            }
        }
        else
        {
            event.Clear();
            event.X = cursor.x;
            event.Y = cursor.y;
            event.Data = timer->Id;
            event.Type = 0x13;
            manager = Application->TimerManager;
            manager->LockTimersExcept(manager->GetTimer(static_cast<int16_t>(i)));
            timer->Target->HandleEvent(&event);
            Application->TimerManager->UnlockTimers();
            const int32_t numTimers = Application->TimerManager->NumTimers;

            if (count == numTimers)
            {
                timer->LastTime = now;
            }
            else
            {
                // Faithful: when the handler removed a timer other than this one, this one's time is set too.
                if (count - 1 != numTimers)
                {
                    timer->LastTime = now;
                }

                count = numTimers;
            }
        }
    }
}

// aTimerManager.

MCGuiTimerManager::MCGuiTimerManager()
{
    for (MCGuiTimer*& timer : Timers)
    {
        timer = nullptr;
    }

    NumTimers = 0;
    NumTimersToWhack = 0;
    Locked = 0;
}

MCGuiTimerManager::~MCGuiTimerManager()
{
    for (int16_t i = 0; i < NumTimers; i++)
    {
        delete Timers[i];
    }
}

auto MCGuiTimerManager::Init() -> int32_t
{
    RunTimersCallback = new MCGuiCallback;

    if (RunTimersCallback == nullptr)
    {
        return -1;
    }

    RunTimersCallback->SetExec(TimerCallback);
    return 0;
}

auto MCGuiTimerManager::Destroy() -> void
{
    while (NumTimers > 0)
    {
        delete Timers[NumTimers - 1];
        Timers[NumTimers - 1] = nullptr;
        NumTimers--;
    }

    MCGuiCallback* callback = RunTimersCallback;
    Application->RemoveCallback(callback);

    if (callback != nullptr)
    {
        callback->Destroy();
        delete callback;
    }

    RunTimersCallback = nullptr;
}

auto MCGuiTimerManager::AddUniqueTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType,
                                       int32_t eventData, int useScenarioTime) -> int32_t
{
    for (int16_t i = 0; i < NumTimers; i++)
    {
        const MCGuiTimer* timer = Timers[i];

        if (timer != nullptr && timer->Target == target && timer->Id == id && timer->Interval == interval &&
            timer->EventType == eventType && timer->EventData == eventData)
        {
            return -1;
        }
    }

    return AddTimer(target, id, interval, eventType, eventData, useScenarioTime);
}

auto MCGuiTimerManager::AddTimer(MCGuiObject* target, int16_t id, uint32_t interval, int32_t eventType,
                                 int32_t eventData, int useScenarioTime) -> int32_t
{
    const int32_t index = NumTimers;

    // Port fix: the original accepted a 100th timer (index 99), one past the array, over timersToWhack[0].
    if (index >= 99)
    {
        return -1;
    }

    Timers[index] = new MCGuiTimer{};
    MCGuiTimer* timer = Timers[NumTimers];

    if (index == 0)
    {
        Application->AddCallback(RunTimersCallback);
    }

    uint32_t startTime;

    if (useScenarioTime == 0)
    {
        startTime = MCPort::Milliseconds();
    }
    else
    {
        startTime = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(ScenarioTime) * 1000.0));
    }

    timer->Target = target;
    timer->Id = id;
    timer->Interval = interval;
    timer->LastTime = startTime;
    timer->EventType = eventType;
    timer->EventData = eventData;
    NumTimers = index + 1;
    timer->UseScenarioTime = useScenarioTime;
    return 0;
}

auto MCGuiTimerManager::RemoveTimers(MCGuiObject* target) -> void
{
    for (int32_t i = 0; i < NumTimers; i++)
    {
        MCGuiTimer* timer = Timers[i];

        if (target != timer->Target)
        {
            continue;
        }

        if (Locked != 0 && timer != LockedExcept)
        {
            // Locked: queued for UnlockTimers (once per matching timer).
            TimersToWhack[NumTimersToWhack].Target = target;
            TimersToWhack[NumTimersToWhack].Id = -1;
            NumTimersToWhack++;
            continue;
        }

        delete timer;
        NumTimers--;

        for (int32_t j = i; j < NumTimers; j++)
        {
            Timers[j] = Timers[j + 1];
        }

        Timers[NumTimers] = nullptr;
        i--;

        if (NumTimers == 0)
        {
            Application->RemoveCallback(RunTimersCallback);
        }
    }
}

auto MCGuiTimerManager::RemoveTimer(MCGuiObject* target, int16_t id) -> void
{
    const int32_t count = NumTimers;
    int32_t index = 0;

    while (index < count && !(Timers[index]->Id == id && target == Timers[index]->Target))
    {
        index++;
    }

    if (index >= count)
    {
        return;
    }

    if (Locked != 0 && Timers[index] != LockedExcept)
    {
        TimersToWhack[NumTimersToWhack].Target = target;
        TimersToWhack[NumTimersToWhack].Id = id;
        NumTimersToWhack++;
        return;
    }

    delete Timers[index];
    NumTimers = count - 1;

    for (; index < NumTimers; index++)
    {
        Timers[index] = Timers[index + 1];
    }

    Timers[NumTimers] = nullptr;

    if (NumTimers == 0)
    {
        Application->RemoveCallback(RunTimersCallback);
    }
}

auto MCGuiTimerManager::RemoveTimer(int32_t index) -> void
{
    if (index >= NumTimers)
    {
        return;
    }

    if (Locked != 0 && Timers[index] != LockedExcept)
    {
        TimersToWhack[NumTimersToWhack].Target = nullptr;
        TimersToWhack[NumTimersToWhack].Id = index;
        NumTimersToWhack++;
        return;
    }

    delete Timers[index];
    NumTimers--;

    for (; index < NumTimers; index++)
    {
        Timers[index] = Timers[index + 1];
    }

    Timers[NumTimers] = nullptr;

    if (NumTimers == 0)
    {
        Application->RemoveCallback(RunTimersCallback);
    }
}

auto MCGuiTimerManager::GetTimer(int16_t index) -> MCGuiTimer*
{
    if (index < NumTimers)
    {
        return Timers[index];
    }

    return nullptr;
}

auto MCGuiTimerManager::GetTimer(MCGuiObject* target, int16_t id) -> MCGuiTimer*
{
    int32_t index = 0;

    while (index < NumTimers && !(Timers[index]->Id == id && target == Timers[index]->Target))
    {
        index++;
    }

    if (index >= NumTimers)
    {
        return nullptr;
    }

    return Timers[index];
}

auto MCGuiTimerManager::LockTimersExcept(MCGuiTimer* running) -> void
{
    Locked = -1;
    LockedExcept = running;
}

auto MCGuiTimerManager::UnlockTimers() -> void
{
    Locked = 0;

    // The queued removals, last first.
    for (int32_t i = NumTimersToWhack; i > 0; i--)
    {
        const TimerToWhack& whack = TimersToWhack[i - 1];

        if (whack.Target != nullptr && whack.Id != -1)
        {
            LockMouse();
            RemoveTimer(whack.Target, static_cast<int16_t>(whack.Id));
            UnlockMouse();
        }
        else if (whack.Target != nullptr)
        {
            LockMouse();
            RemoveTimers(whack.Target);
            UnlockMouse();
        }
        else if (whack.Id != -1)
        {
            LockMouse();
            RemoveTimer(whack.Id);
            UnlockMouse();
        }
        else
        {
            Fatal(NumTimersToWhack, " Illegal timersToWhack structure!");
        }

        NumTimersToWhack--;
    }
}

auto GetMessageCursorLoc() -> tagPOINT
{
    // The message's position is already on the logical screen in the port (MapWindowPoints in the original).
    const MCPoint position = MCInput::GetMessagePos();
    return tagPOINT{position.x, position.y};
}

// aHolderObject.

auto MCGuiHolderObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    // aObject::init inlined, without the port: a holder only shows its panes.
    (void)name;
    WinWidth = width;
    MaxWidth = width;
    NormalWidth = width;
    IconWidth = width;
    WinHeight = height;
    WinX = xPos;
    WinY = yPos;
    MaxHeight = height;
    MaxX = xPos;
    MaxY = yPos;
    NormalHeight = height;
    NormalX = xPos;
    NormalY = yPos;
    IconHeight = height;
    IconX = xPos;
    IconY = yPos;
    HideOffset = 0;
    Hidden = 0;
    WinState = aSTATE_NORMAL;
    ShowWindow = -1;
    DragOn = 0;
    BackgroundColor = 0xff;
    DisplayPort = nullptr;

    if (FramePane != nullptr)
    {
        delete FramePane;
        FramePane = nullptr;
    }

    FramePane = new (std::nothrow) MCPane;

    if (FramePane == nullptr)
    {
        return 3;
    }

    FramePane->Window = ScreenPort->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    FramePane->Y1 = yPos + height;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    NumChildren = 0;
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation = nullptr;
    Animating = 0;
    IconAnimation = nullptr;
    ObjectType = -1;
    Vertical = 0;
    ActivePane = -1;
    Panes[0] = nullptr;
    Panes[1] = nullptr;
    Tiled = 0;
    return 0;
}

auto MCGuiHolderObject::Destroy() -> void
{
    MCGuiObject::Destroy();
    Panes[0] = nullptr;
    Panes[1] = nullptr;
}

auto MCGuiHolderObject::RemoveChild(MCGuiObject* oldChild) -> void
{
    if (Panes[0] == oldChild)
    {
        Panes[0] = nullptr;
    }

    if (Panes[1] == oldChild)
    {
        Panes[1] = nullptr;
    }

    MCGuiObject::RemoveChild(oldChild);
}

auto MCGuiHolderObject::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if ((IsHidden() == 0 || HideOffset != 0) && WinState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->Display();
        }
    }
}

auto MCGuiHolderObject::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth > 0 && newHeight > 0)
    {
        if (GridAligned != 0)
        {
            if (newWidth % 40 > 19)
            {
                newWidth += 40;
            }

            newWidth -= newWidth % 40;

            if (newWidth == 0)
            {
                newWidth = 40;
            }

            if (newHeight % 40 > 19)
            {
                newHeight += 40;
            }

            newHeight -= newHeight % 40;
        }

        WinWidth = newWidth;
        WinHeight = newHeight;
        FramePane->X1 = FramePane->X0 - 1 + newWidth;
        FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
        Retile();
    }
}

auto MCGuiHolderObject::Retile() -> void
{
    int32_t paneX = 0;
    int32_t paneY = 0;
    int32_t paneWidth = Width();
    int32_t paneHeight = Height();

    if (Panes[1] != nullptr && Tiled != 0)
    {
        if (Vertical == 0)
        {
            paneHeight /= 2;
        }
        else
        {
            paneWidth /= 2;
        }
    }

    int16_t index = 0;
    int16_t end;

    if (Tiled == 0)
    {
        index = ActivePane;
        end = static_cast<int16_t>(index + 1);

        // Port fix: with no active pane (-1) the original read the word before panes[0] (the drop-target list) as a
        // pane; it is null in a holder, which ends the loop, so the port stops here.
        if (index < 0)
        {
            return;
        }
    }
    else
    {
        end = 2;
    }

    do
    {
        MCGuiObject* pane = Panes[index];

        if (pane == nullptr)
        {
            return;
        }

        pane->MoveTo(paneX, paneY, 0);
        pane->Resize(paneWidth, paneHeight);

        if (Tiled != 0)
        {
            if (Vertical == 0)
            {
                if (Height() % 2 != 0)
                {
                    paneHeight++;
                }

                paneY += paneHeight;
            }
            else
            {
                if (Width() % 2 != 0)
                {
                    paneWidth++;
                }

                paneX += paneWidth;
            }
        }

        index++;
    } while (index < end);
}

auto MCGuiHolderObject::AddPane(MCGuiObject* pane) -> void
{
    if (Panes[0] == nullptr)
    {
        AddChild(pane);
        Panes[0] = pane;
        ActivePane = 0;
        Retile();
        return;
    }

    if (Panes[1] == nullptr)
    {
        AddChild(pane);
        Panes[1] = pane;
    }

    Retile();
}

auto MCGuiHolderObject::RemovePane(MCGuiObject* pane) -> void
{
    MCGuiObject* second = Panes[1];

    if (second == pane)
    {
        Panes[1] = nullptr;
        RemoveChild(pane);
        Retile();
        return;
    }

    if (Panes[0] == pane)
    {
        // Faithful: with a second pane, it moves to the first slot but stays in the second too.
        if (second == nullptr)
        {
            Panes[0] = nullptr;
            ActivePane = -1;
        }
        else
        {
            Panes[0] = second;
        }

        RemoveChild(pane);
    }

    Retile();
}

auto MCGuiHolderObject::SetActivePane(MCGuiObject* pane) -> void
{
    if (pane == Panes[1])
    {
        ActivePane = 1;
        return;
    }

    ActivePane = 0;
}

auto MCGuiHolderObject::SetTiled(int newTiled) -> void
{
    Tiled = newTiled;

    if (newTiled == 0)
    {
        if (GetInactivePane() != nullptr)
        {
            GetInactivePane()->ShowGuiWindow(0);
        }
    }
    else
    {
        if (Panes[0] != nullptr)
        {
            Panes[0]->ShowGuiWindow(-1);
        }

        if (Panes[1] != nullptr)
        {
            Panes[1]->ShowGuiWindow(-1);
        }
    }

    Retile();
}

auto MCGuiHolderObject::SetActivePaneNumber(char index) -> void
{
    if (index == 0 || (index == 1 && Panes[1] != nullptr))
    {
        ActivePane = index;
    }

    Retile();
}

// aMessageBox.

auto MCGuiMessageBox::Init(uint8_t* text) -> int32_t
{
    if (WhiteFont == nullptr)
    {
        return -3;
    }

    int32_t boxWidth = WhiteFont->Width(text) + 0xc;

    if (boxWidth < 0x48)
    {
        boxWidth = 0x48;
    }

    const int32_t fontHeight = WhiteFont->Height();
    const int32_t screenW = Application->Width();
    const int32_t screenH = Application->Height();
    int32_t result = MCGuiObject::Init((screenW - boxWidth) / 2, (screenH - (fontHeight + 0x28)) / 2, boxWidth,
                                       fontHeight + 0x28, nullptr);

    if (result != 0)
    {
        return result;
    }

    MCGuiButton* button = new MCGuiButton;
    OkButton = button;
    result = button->Init((boxWidth - 0x30) / 2, fontHeight + 0xe, 0x3c, 0x14, nullptr);

    if (result != 0)
    {
        return result;
    }

    button->SetUpPicture(0x10);
    button->SetDownPicture(0x11);
    button->Callback()->SetExec(DestroyVersion);
    button->SetDepth(100);
    AddChild(button);
    // The box is drawn by draw, each frame.
    Message = reinterpret_cast<const char*>(text);
    return 0;
}

auto MCGuiMessageBox::Draw() -> void
{
    MCGuiPort* boxPort = DisplayPort;
    VfxPaneWipe(boxPort->Frame(), 0x11);
    auto* text = reinterpret_cast<uint8_t*>(Message.data());
    const int32_t textWidth = WhiteFont->Width(text);
    WhiteFont->WriteString(boxPort->Frame(), (Width() - textWidth) / 2, 8, text, -1);
    DrawBox(0x1f, -1, -1, -1, -1);
    MCGuiObject::Draw();
}

auto MCGuiMessageBox::Destroy() -> void
{
    if (OkButton != nullptr)
    {
        OkButton->Destroy();
        delete OkButton;
        OkButton = nullptr;
    }

    MCGuiObject::Destroy();
}

auto MCGuiMessageBox::HandleEvent(MCGuiEvent* event) -> void
{
    if (PointInside(event->X, event->Y) != 0)
    {
        OkButton->HandleEvent(event);
    }
}
