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
#include "gui/updisp.h"
#include "iface/icallbk.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/ffile.h"
#include "lib/file.h"
#include "lib/heap.h"
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
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"
#include "platform/MCSmacker.h"
#include "platform/MCWin32Defs.h"

aSystem* application = nullptr;
aObject* screenWindow = nullptr;
aPort* screenPort = nullptr;
PacketFile* artFile = nullptr;
char* startupPakFile = nullptr;
aMessageBox* versionDialog = nullptr;
aObject* smackWindowPointer = nullptr;
aObject* featureScreen = nullptr;
int featureScreenDone = 0;
int escapedSmackerMovie = 0;
aCallback* mouseTrackerCallback = nullptr;
aFont* systemFont = nullptr;
aFont* blackFont = nullptr;
aFont* greyFont = nullptr;
aFont* whiteFont = nullptr;
aFont* redFont = nullptr;
aFont* greenFont = nullptr;
aFont* blueFont = nullptr;
aFont* dimFont = nullptr;
aFont* yellowFont = nullptr;
aFont* yellowDropFont = nullptr;
aFont* blueDropFont = nullptr;
aFont* medBlackFont = nullptr;
aFont* medGreyFont = nullptr;
aFont* medWhiteFont = nullptr;
aFont* medRedFont = nullptr;
aFont* medGreenFont = nullptr;
aFont* medBlueFont = nullptr;
aFont* medDimFont = nullptr;
aFont* medYellowFont = nullptr;
aFont* lgBlackFont = nullptr;
aFont* lgGreyFont = nullptr;
aFont* lgWhiteFont = nullptr;
aFont* lgRedFont = nullptr;
aFont* lgGreenFont = nullptr;
aFont* lgBlueFont = nullptr;
aFont* lgDimFont = nullptr;
aFont* lgYellowFont = nullptr;
aFont* fonts[10][3] = {};
int gamePaused = 0;
int gameAsked = 0;
Font* lineFont = nullptr;
int gWidth = 640;
int gHeight = 480;
int gBitDepth = 8;
int gFullScreen = 0;
int gStretchToFit = 0;
int gSoftwareCursor = 0;
int gHiddenWindow = 0;
int gRenderer = 0;
int applicationActive = -1;
uint32_t systemHeapSize = 0x100000;
uint32_t guiHeapSize = 0x100000;
uint32_t stackSize = 0x100000;
uint32_t topOfStack = 0;
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
int allowMagicWindowSwitching = -1;
int32_t displayWidth = 0;
int32_t displayHeight = 0;
int oldMouseX = 0;
int oldMouseY = 0;
float frameRate = 0.0f;
int64_t startTime = 0;
int64_t stopTime = 0;
int64_t prevStart = 0;
int64_t countsPerSecond = 0;
int32_t lastX = 0;
int32_t lastY = 0;
int leftMouseButtonDown = 0;
int rightMouseButtonDown = 0;
// appName and WindowTitle are 0x400 bytes (0x007aa9a0..0x007aada0 and 0x007f050c..0x007f090c), paletteName 80.
char appName[0x400] = {};
char WindowTitle[0x400] = {};
char paletteName[80] = {};
char* backPtr = nullptr;
// The cheat codes: a length byte, then the letters plus 0x32 (Cheat subtracts it). Cheat_CantHitMe's length (5) is
// one short of its six letters, so only the first five count.
char Cheat_framegraph[12] = {'\x0a', '\x98', '\xa4', '\x93', '\x9f', '\x97', '\x99', '\xa4', '\x93', '\xa2', '\x9a'};
char Cheat_BunnyStrike[12] = {'\x09', '\x9e', '\xa1', '\xa4', '\x96', '\x94', '\xa7', '\xa0', '\xa0', '\xab'};
char Cheat_HealAll[8] = {'\x06', '\x9e', '\xa1', '\xa4', '\xa4', '\x9b', '\x97'};
char Cheat_DeadEye[8] = {'\x07', '\x96', '\x97', '\x93', '\x96', '\x97', '\xab', '\x97'};
char Cheat_CantHitMe[8] = {'\x05', '\xa1', '\xa5', '\x9f', '\x9b', '\xa7', '\x9f'};
char Cheat_GetSalvage[20] = {'\x12', '\x99', '\x9e', '\x97', '\xa0', '\xa0', '\xa4', '\xa1', '\x95', '\x9d',
                             '\xa5', '\xa6', '\x9a', '\x97', '\x9a', '\xa1', '\xa7', '\xa5', '\x97'};
char Cheat_Reveal[28] = {'\x18', '\x9f', '\x9b', '\xa0', '\x97', '\x97', '\xab', '\x97', '\xa5',
                         '\x9a', '\x93', '\xa8', '\x97', '\xa5', '\x97', '\x97', '\xa0', '\xa6',
                         '\x9a', '\x97', '\x99', '\x9e', '\xa1', '\xa4', '\xab'};
char Cheat_Duh[4] = {'\x03', '\x96', '\xa7', '\x9a'};
char CheatKey[128] = {};
int CheatPointer = 0;
uint32_t CantHitMe = 0;
int cheatsOn = 0;
int CantBlowSalvage = 0;
int BunnyStrikesOn = 0;
int Duh = 0;
int recordClicks = 0;
int SavedPosition = 0;
int lockFrameRate = 0;
int lockActive = 0;
int takeScreenShot = 0;
uint32_t scrollWait = 0;
int32_t displayProfileData = 0;
char keySetting = 0;
int QueuePlayerOrders = 0;
int forceGatesClosed = 0;
int drawTerrainGrid = 0;
VFX_RGB* paletteRgb = nullptr;
int32_t globalEntries = 0;
int32_t globalFirst = 0;
uint32_t Networkframe = 0;
uint32_t MP_Start_Time = 0;
uint32_t uMessage = 0;
std::recursive_mutex MouseCritSec;
volatile int InMouseCritSec = 0;
int AndyFramerate = 0;
int AG_mouseFrame = 0;
int mouseThreadStarted = 0;
void* memoryStatus = nullptr;
void* backbm = nullptr;
void* holdpalette = nullptr;
void* OffScreenhOldBitmap = nullptr;
void* OffScreenBufferDC = nullptr;
void* OffScreenhDIBSection = nullptr;
void* DesktopDC = nullptr;
void* hPalette = nullptr;
void* ghWindow = nullptr;
void* backpbmi = nullptr;
void* thePalette = nullptr;
uint8_t* screenBits = nullptr;
int32_t Processor = 0;

namespace
{
    /// <summary>
    /// Set once the display is up (DirectDraw or the DIB section in the original): palette changes and repaints wait
    /// for it (DAT_007ab108).
    /// </summary>
    int displayReady = 0;

    /// <summary>
    /// A byte the window procedure tests before selecting the GDI palette on repaint and activation; nothing in
    /// MCX.EXE sets it (DAT_007aa994).
    /// </summary>
    uint8_t keepDesktopPalette = 0;

    /// <summary>Two windows <see cref="aSystem::stop"/> destroys; nothing in MCX.EXE sets them (DAT_007ab134/138).</summary>
    aObject* stopWindow1 = nullptr;
    aObject* stopWindow2 = nullptr;

    /// <summary>
    /// The palette as shown: the GDI LOGPALETTE's entries (DAT_007aa3c4) and the DIB colour table's (DAT_007a9fb8)
    /// in the original, the colours handed to the display in the port. <see cref="aSystem::currentPalette"/> through
    /// the gamma table.
    /// </summary>
    VFX_RGB logicalPalette[256] = {};

    /// <summary>The display (DirectDraw's objects and the window in the original).</summary>
    std::unique_ptr<MCDisplay> gameDisplay;

    /// <summary>Hands <paramref name="count"/> shown colours from <paramref name="first"/> to the display.</summary>
    void showPalette(int first, int count)
    {
        if (MCDisplay* display = MCInput::Display())
        {
            display->SetPalette(first, count, &logicalPalette[first]);
        }
    }

    /// <summary>Takes the mouse thread's lock when the thread runs (the original's EnterCriticalSection pairs).</summary>
    void lockMouse()
    {
        if (mouseThreadStarted != 0)
        {
            MouseCritSec.lock();
        }
    }

    void unlockMouse()
    {
        if (mouseThreadStarted != 0)
        {
            MouseCritSec.unlock();
        }
    }

    /// <summary>Ends a movie window: stops the movie, destroys and deletes the window.</summary>
    void closeMovieWindow(aObject*& window)
    {
        static_cast<aSmackerWindow*>(window)->endSmackerMovie();
        window->destroy();
        delete window;
        window = nullptr;
    }

    /// <summary>Frees a font loaded by <see cref="aSystem::start"/>.</summary>
    void deleteFont(aFont*& font)
    {
        if (font != nullptr)
        {
            font->destroy();
            delete font;
            font = nullptr;
        }
    }

    /// <summary>Loads a font for <see cref="aSystem::start"/>.</summary>
    aFont* loadFont(const char* fileName)
    {
        aFont* font = new (std::nothrow) aFont;
        font->init(const_cast<char*>(fileName));
        return font;
    }

    /// <summary>
    /// Reads a TGA's colour map (at byte 0x12, blue-green-red, 8 bits) into a 6-bit VFX palette, as
    /// <see cref="GetPaletteFromArt"/> and <see cref="aSystem::activatePaletteFromTGA"/> do.
    /// </summary>
    void tgaColorMapToPalette(const uint8_t* tga, VFX_RGB* palette)
    {
        const uint8_t* entry = tga + 0x12;

        for (int i = 0; i < 256; i++, entry += 3)
        {
            palette[i].r = static_cast<uint8_t>(entry[2] >> 2);
            palette[i].g = static_cast<uint8_t>(entry[1] >> 2);
            palette[i].b = static_cast<uint8_t>(entry[0] >> 2);
        }
    }

    /// <summary>Whether a key is held (GetAsyncKeyState's top bit).</summary>
    bool keyHeld(int vk)
    {
        return (MCInput::GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    /// <summary>
    /// The scissors of the objects drawing in the frame pass around the one displaying now (innermost last), with the
    /// window each is on: a child that draws itself is cut to its nearest such ancestor's.
    /// </summary>
    std::vector<std::pair<const _window*, MCRect>> viewClips;

    /// <summary>The object drawing in the frame pass right now (its view open), or null.</summary>
    aObject* drawingLive = nullptr;

    /// <summary>The test message <see cref="SendAndReceiveTestMessages"/> sends: a header, the frame and a count.</summary>
#pragma pack(push, 1)
    struct TestMessage
    {
        FIGuaranteedMessageHeader header; // +0x00
        uint32_t frame;                   // +0x08
        uint32_t index;                   // +0x0c
    };
#pragma pack(pop)
    static_assert(sizeof(TestMessage) == 0x10);
}

// aObject's inline virtuals from gui\asystem.h.

auto aObject::drawBox(uint8_t color, tagRECT area) -> void
{
    drawBox(color, area.left, area.top, area.right, area.bottom);
}

// aSystem's DirectDraw layer: the display in the port.

auto aSystem::startupDirectDraw(int32_t width, int32_t height, int32_t bitDepth) -> int32_t
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
    options.Title = appName;
    options.Width = 640;
    options.Height = 480;
    options.FollowWindow = true;
    options.Fullscreen = gFullScreen != 0;
    options.Stretch = gStretchToFit != 0;
    options.Hidden = gHiddenWindow != 0;
    options.Renderer = static_cast<MCRendererKind>(gRenderer);
    auto display = MCDisplay::Create(options);

    if (!display)
    {
        Fatal(0, "Cannot initialize DirectDraw.", display.error().c_str());
    }

    gameDisplay = std::move(*display);
    gWidth = gameDisplay->Width();
    gHeight = gameDisplay->Height();
    displayWidth = gWidth;
    displayHeight = gHeight;
    screenWidth = gWidth;
    screenHeight = gHeight;
    ddObject = gameDisplay.get();
    MCInput::Attach(gameDisplay.get());
    screenBits = gameDisplay->Pixels();
    showPalette(0, 256);
    displayReady = 1;
    return 0;
}

auto aSystem::resetDirectDraw(int32_t width, int32_t height, int32_t bitDepth) -> int32_t
{
    // Port: the original released and remade the surfaces in the new mode (full screen or windowed), restored the
    // window style and re-attached the palette. The port switches the display and, for another size, its buffer.
    gBitDepth = bitDepth;
    gWidth = width;
    gHeight = height;

    if (gameDisplay == nullptr)
    {
        return startupDirectDraw(width, height, bitDepth);
    }

    gameDisplay->SetFullscreen(gFullScreen != 0);

    if (width != gameDisplay->Width() || height != gameDisplay->Height())
    {
        auto resized = gameDisplay->SetLogicalSize(width, height);

        if (!resized)
        {
            Fatal(0, " Unable to Set Display Mode ", resized.error().c_str());
        }

        screenBits = gameDisplay->Pixels();

        // Port fix: the screen port keeps pointing at the display's buffer, which the resize moved.
        if (lockActive != 0 && screenPort != nullptr && screenPort->bitmap() != nullptr)
        {
            screenPort->bitmap()->buffer = screenBits;
        }
    }

    MCInput::RefreshMouseArea();
    showPalette(0, 256);
    displayReady = 1;
    return 0;
}

auto MCFollowWindowSize() -> bool
{
    MCDisplay* display = gameDisplay.get();

    if (display == nullptr || !display->FollowsWindow() || application == nullptr)
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

    gWidth = width;
    gHeight = height;
    displayWidth = width;
    displayHeight = height;
    application->screenWidth = width;
    application->screenHeight = height;
    screenBits = display->Pixels();

    if (screenPort != nullptr && screenPort->bitmap() != nullptr)
    {
        screenPort->resize(width, height);
        screenPort->bitmap()->buffer = screenBits;
    }

    MCInput::RefreshMouseArea();

    if (screenWindow != nullptr)
    {
        screenWindow->resize(width, height);

        if (mainHolder != nullptr)
        {
            // The original's resolution-change broadcast (nothing in MCX.EXE sends it): the main window takes the
            // screen's size and re-tiles its panes, the mech bar goes back to the bottom.
            aEvent event;
            event.clear();
            event.type = 0x12;
            screenWindow->handleEvent(&event);
        }
        else if (theInterface != nullptr && theInterface->mechBar != nullptr)
        {
            // Before the scenario's windows exist (StartScenario after the window was resized in the menus),
            // aMechBar::handleEvent would pass the event to mainHolder's active pane: just move the bar down.
            aMechBar* bar = theInterface->mechBar;
            bar->moveTo(1, application->height() - bar->height() - 1, 0);
        }
    }

    return true;
}

auto aSystem::shutdownDirectDraw() -> int32_t
{
    MCCursor::Shutdown();
    MCInput::Attach(nullptr);
    gameDisplay.reset();
    ddObject = nullptr;
    ddObject2 = nullptr;
    screenBits = nullptr;
    OffScreenBufferDC = nullptr;
    OffScreenhDIBSection = nullptr;
    OffScreenhOldBitmap = nullptr;
    displayReady = 0;
    return 0;
}

auto aSystem::setFlipToGDI() -> void
{
    flipToGDIRequested = -1;
}

auto aSystem::clearFlipToGDI() -> void
{
    flipToGDIRequested = 0;
}

auto aSystem::flipToGDI() -> void
{
}

auto aSystem::setScrollRect() -> void
{
    scrollRect.left = 1;
    scrollRect.right = width() - 4;
    scrollRect.top = 1;
    scrollRect.bottom = height() - 4;
}

// aObject.

aObject::aObject()
{
    framePane = nullptr;
    backgroundPort = nullptr;
    animating = 0;
    displayPort = nullptr;
    gridAligned = 0;
    objectType = 0;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    windowAnimation = nullptr;
    iconAnimation = nullptr;
    dropTargets = nullptr;
    numDropTargets = 0;
}

aObject::~aObject()
{
    destroy();
}

auto aObject::operator new(size_t size) noexcept -> void*
{
    return guiHeap->malloc(static_cast<uint32_t>(size));
}

auto aObject::operator delete(void* ptr) -> void
{
    guiHeap->free(ptr);
}

auto aObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)name;
    winHeight = height;
    maxHeight = height;
    normalHeight = height;
    iconHeight = height;
    winWidth = width;
    winX = xPos;
    winY = yPos;
    maxWidth = width;
    maxX = xPos;
    maxY = yPos;
    normalWidth = width;
    normalX = xPos;
    normalY = yPos;
    iconWidth = width;
    iconX = xPos;
    iconY = yPos;
    hideOffset = 0;
    homeX = xPos;
    homeY = yPos;
    winState = aSTATE_NORMAL;
    showWindow = -1;
    dragOn = 0;
    transparent = 0;
    backgroundColor = 0xff;
    numDropTargets = 0;

    if (displayPort != nullptr)
    {
        displayPort->destroy();
        delete displayPort;
        displayPort = nullptr;
    }

    displayPort = new aPort;
    const int32_t result = DrawsLive() ? displayPort->initView(width, height) : displayPort->init(width, height);

    if (result != 0)
    {
        return result;
    }

    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    framePane = new (std::nothrow) _pane;

    if (framePane == nullptr)
    {
        return 3;
    }

    framePane->window = screenPort->bitmap();
    framePane->x0 = xPos;
    framePane->y0 = yPos;
    framePane->x1 = xPos + width;
    framePane->y1 = yPos + height;
    hidden = 0;
    hideDirection = DIRECTION_DOWN;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    winDepth = 0;
    windowAnimation = nullptr;
    animating = 0;
    iconAnimation = nullptr;
    objectType = -1;
    return 0;
}

auto aObject::destroy() -> void
{
    application->RemoveTimers(this);

    if (displayPort != nullptr)
    {
        displayPort->destroy();
        delete displayPort;
        displayPort = nullptr;
    }

    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    if (backgroundPort != nullptr)
    {
        backgroundPort->destroy();
        delete backgroundPort;
        backgroundPort = nullptr;
    }

    if (iconAnimation != nullptr)
    {
        iconAnimation->destroy();
        delete iconAnimation;
        iconAnimation = nullptr;
    }

    if (windowAnimation != nullptr)
    {
        windowAnimation->destroy();
        delete windowAnimation;
        windowAnimation = nullptr;
    }

    if (dropTargets != nullptr)
    {
        delete[] dropTargets;
        dropTargets = nullptr;
    }

    Assert(numChildren == 0, 0, " Number of Children NOT Zero ");

    if (parent != nullptr)
    {
        parent->removeChild(this);
    }

    parent = nullptr;
    animating = 0;

    if (application->grabbedObject() == this)
    {
        application->release();
    }

    if (application->textObject() == this)
    {
        application->releaseText();
    }

    if (application->modalObject() == this)
    {
        application->clearModal();
    }

    if (application->currentObject() == this)
    {
        const MCPoint cursor = MCInput::GetCursorPos();
        application->setCurrentObject(screenWindow->findObject(cursor.x, cursor.y));
    }
}

auto aObject::setDisplayPort(aPort* newPort) -> void
{
    framePane->window = newPort->bitmap();

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->setDisplayPort(newPort);
    }
}

auto aObject::pointInside(int32_t xPos, int32_t yPos) -> int
{
    if (framePane->x0 <= xPos && xPos <= framePane->x1 && framePane->y0 <= yPos && yPos <= framePane->y1)
    {
        return -1;
    }

    return 0;
}

auto aObject::rectIntersect(int32_t left, int32_t top, int32_t right, int32_t bottom) -> int
{
    if (framePane->x0 < right && left < framePane->x1 && framePane->y0 < bottom && top < framePane->y1)
    {
        return -1;
    }

    return 0;
}

auto aObject::rectIntersect(tagRECT area) -> int
{
    if (framePane->x0 < area.right && area.left < framePane->x1 && framePane->y0 < area.bottom &&
        area.top < framePane->y1)
    {
        return -1;
    }

    return 0;
}

auto aObject::setPaintRoutine(void (*routine)(aObject*)) -> void
{
    paintRoutine = routine;
}

auto aObject::setEventRoutine(void (*routine)(aObject*, aEvent*)) -> void
{
    eventRoutine = routine;
}

auto aObject::paint() -> void
{
    if (paintRoutine != nullptr)
    {
        paintRoutine(this);
    }
}

auto aObject::findObject(int32_t xPos, int32_t yPos) -> aObject*
{
    if (winState != aSTATE_ICONIZED)
    {
        if (showWindow == 0)
        {
            return nullptr;
        }

        for (int32_t i = numChildren; i > 0; i--)
        {
            aObject* found = childList[i - 1]->findObject(xPos, yPos);

            if (found != nullptr)
            {
                return found;
            }
        }
    }

    if (showWindow != 0 && framePane != nullptr && pointInside(xPos, yPos) != 0)
    {
        return this;
    }

    return nullptr;
}

auto aObject::children() -> aObject*
{
    return nullptr;
}

auto aObject::setParent(aObject* newParent) -> void
{
    parent = newParent;
}

auto aObject::setDepth(int32_t newDepth) -> void
{
    aObject* owner = parent;

    if (owner != nullptr)
    {
        owner->removeChild(this);
    }

    winDepth = newDepth;

    if (owner != nullptr)
    {
        owner->addChild(this);
    }
}

auto aObject::depth() -> int32_t
{
    return winDepth;
}

auto aObject::startAnimation() -> void
{
    animating = -1;
}

auto aObject::stopAnimation() -> void
{
    animating = 0;
}

auto aObject::startModal() -> void
{
    application->setModalObject(this);
}

auto aObject::stopModal() -> void
{
    application->clearModal();
}

auto aObject::setBackColor(int32_t color) -> void
{
    backgroundColor = color;
}

auto aObject::backColor() -> int32_t
{
    return backgroundColor;
}

auto aObject::drawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
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
        right = width() - 1;
    }

    if (bottom == -1)
    {
        bottom = height() - 1;
    }

    aPort* port = displayPort;
    VFX_line_draw(port->frame(), left, top, right, top, LD_DRAW, color);
    VFX_line_draw(port->frame(), left, top, left, bottom, LD_DRAW, color);
    VFX_line_draw(port->frame(), left, bottom, right, bottom, LD_DRAW, color);
    VFX_line_draw(port->frame(), right, top, right, bottom, LD_DRAW, color);
}

auto aObject::drawFramed(int pushed, int fill) -> void
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

    if (fill != 0 && backgroundColor != 0xff)
    {
        VFX_pane_wipe(displayPort->frame(), backColor());
    }

    VFX_line_draw(displayPort->frame(), 0, height() - 1, width(), height() - 1, LD_DRAW, 0x10);
    VFX_line_draw(displayPort->frame(), 0, 0, width(), 0, LD_DRAW, 0x10);
    VFX_line_draw(displayPort->frame(), 0, 0, 0, height() - 1, LD_DRAW, 0x10);
    VFX_line_draw(displayPort->frame(), width() - 1, 0, width() - 1, height() - 1, LD_DRAW, 0x10);
    VFX_line_draw(displayPort->frame(), 1, 1, width() - 2, 1, LD_DRAW, innerTopLeft);
    VFX_line_draw(displayPort->frame(), 2, 2, width() - 3, 2, LD_DRAW, outerTopLeft);
    VFX_line_draw(displayPort->frame(), 1, 1, 1, height() - 2, LD_DRAW, innerTopLeft);
    VFX_line_draw(displayPort->frame(), 2, 2, 2, height() - 3, LD_DRAW, outerTopLeft);
    VFX_line_draw(displayPort->frame(), width() - 2, 1, width() - 2, height() - 2, LD_DRAW, innerBottomRight);
    VFX_line_draw(displayPort->frame(), width() - 3, 2, width() - 3, height() - 3, LD_DRAW, outerBottomRight);
    VFX_line_draw(displayPort->frame(), 1, height() - 2, width() - 2, height() - 2, LD_DRAW, innerBottomRight);
    VFX_line_draw(displayPort->frame(), 2, height() - 3, width() - 3, height() - 3, LD_DRAW, outerBottomRight);
}

auto aObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    _pane box = *displayPort->frame();
    box.x0 = left;
    box.y0 = top;
    box.x1 = right;
    box.y1 = bottom;
    VFX_pane_wipe(&box, color);
}

auto aObject::bringToFront(int noShuffle) -> void
{
    if (parent == nullptr)
    {
        return;
    }

    parent->bringToFront(noShuffle);

    if (parent->numberOfChildren() > 1)
    {
        // The parent's children are kept sorted by depth: the ones in front of this object's depth stay, then this
        // object's depth, with this object last (in front), then the rest.
        aObject* sorted[255];
        std::memcpy(sorted, parent->childList, sizeof(sorted));
        int32_t next = 0;
        int32_t placed = 0;

        while (next < parent->numberOfChildren())
        {
            aObject* sibling = sorted[next];

            if (sibling->depth() >= winDepth)
            {
                break;
            }

            parent->childList[next] = sibling;
            next++;
            placed++;
        }
        while (next < parent->numberOfChildren())
        {
            aObject* sibling = sorted[next];

            if (sibling->depth() != winDepth)
            {
                break;
            }

            if (sibling != this)
            {
                parent->childList[placed++] = sibling;
            }

            next++;
        }

        parent->childList[placed] = this;

        for (int32_t slot = placed + 1; slot < parent->numberOfChildren(); slot++)
        {
            parent->childList[slot] = sorted[next++];
        }

        if (gridAligned != 0 && noShuffle == 0)
        {
            for (int32_t i = parent->numberOfChildren() - 1; i >= 0; i--)
            {
                aObject* sibling = parent->child(i);

                if (sibling->IsShowing() != 0 && sibling != this && sibling->gridAligned != 0 &&
                    sibling->x() / 40 == x() / 40 && sibling->y() / 40 == y() / 40)
                {
                    sibling->gridAligned = 0;
                    const int32_t newY = 5 - sibling->y() % 40 + sibling->y();
                    const int32_t newX = 5 - sibling->x() % 40 + sibling->x();
                    sibling->moveTo(newX, newY, 0);
                    sibling->gridAligned = -1;
                }
            }
        }
    }
}

auto aObject::numberOfChildren() -> int32_t
{
    return numChildren;
}

auto aObject::addChild(aObject* child) -> void
{
    Assert(numChildren < 255, numChildren + 1, "Too many children!");
    Assert(child->parent == nullptr || child->parent == this, 0, " Adding child that's someone else's ");

    if (child != nullptr)
    {
        removeChild(child);
        child->setParent(this);
        childList[numChildren] = child;
        numChildren++;
        child->bringToFront(-1);
        child->moveTo(child->x(), child->y(), 0);
    }
}

auto aObject::removeChild(aObject* child) -> void
{
    // Port: the original checked IsBadReadPtr(child) and, for an unreadable pointer, removed it without clearing
    // its parent; a pointer here is always readable or null.
    if (child == nullptr)
    {
        return;
    }

    int32_t index = 0;

    while (index < numChildren && childList[index] != child)
    {
        index++;
    }

    if (index >= numChildren)
    {
        return;
    }

    for (; index < numChildren - 1; index++)
    {
        childList[index] = childList[index + 1];
    }

    childList[numChildren] = nullptr;
    numChildren--;
    child->setParent(nullptr);
}

auto aObject::dragging() -> int
{
    return dragOn;
}

auto aObject::startDrag(int32_t xPos, int32_t yPos) -> void
{
    dragOn = -1;
    dragX = xPos;
    dragY = yPos;
}

auto aObject::stopDrag() -> void
{
    dragOn = 0;
}

auto aObject::dragStartX() -> int32_t
{
    return dragX;
}

auto aObject::dragStartY() -> int32_t
{
    return dragY;
}

auto aObject::foremostChild(int32_t atDepth) -> aObject*
{
    for (int32_t i = numChildren; i > 0; i--)
    {
        if (childList[i - 1]->winDepth == atDepth)
        {
            return childList[i - 1];
        }
    }

    return nullptr;
}

auto aObject::child(int32_t index) -> aObject*
{
    if (numChildren - 1 < index)
    {
        return nullptr;
    }

    return childList[index];
}

auto aObject::width() -> int32_t
{
    return winWidth;
}

auto aObject::height() -> int32_t
{
    return winHeight;
}

auto aObject::ptr() -> void*
{
    if (displayPort != nullptr)
    {
        return displayPort->buffer();
    }

    return nullptr;
}

auto aObject::port() -> aPort*
{
    return displayPort;
}

auto aObject::x() -> int32_t
{
    return winX;
}

auto aObject::y() -> int32_t
{
    return winY;
}

auto aObject::globalX() -> int32_t
{
    int32_t result = winX;

    for (aObject* owner = parent; owner != nullptr; owner = owner->parent)
    {
        result += owner->x();
    }

    return result;
}

auto aObject::globalY() -> int32_t
{
    int32_t result = winY;

    for (aObject* owner = parent; owner != nullptr; owner = owner->parent)
    {
        result += owner->y();
    }

    return result;
}

auto aObject::frame() -> _pane*
{
    return framePane;
}

auto aObject::moveTo(int32_t xPos, int32_t yPos, int temporary) -> void
{
    int32_t parentX = 0;
    int32_t parentY = 0;
    winY = yPos;
    winX = xPos;

    if (parent != nullptr)
    {
        parentX = parent->globalX();
        parentY = parent->globalY();
    }

    framePane->x0 = parentX + xPos;
    framePane->y0 = parentY + yPos;
    framePane->x1 = winWidth - 1 + framePane->x0;
    framePane->y1 = winHeight - 1 + framePane->y0;

    if (temporary == 0)
    {
        homeX = xPos;
        homeY = yPos;
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        aObject* child = childList[i];
        child->moveTo(child->x(), child->y(), 0);
    }
}

auto aObject::resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth > 0 && newHeight > 0 && (newWidth != winWidth || newHeight != winHeight))
    {
        if (gridAligned != 0)
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

        if (displayPort != nullptr)
        {
            displayPort->resize(newWidth, newHeight);
        }

        winWidth = newWidth;
        winHeight = newHeight;
        framePane->x1 = framePane->x0 - 1 + newWidth;
        framePane->y1 = framePane->y0 - 1 + newHeight;
    }
}

auto aObject::draw() -> void
{
    const int32_t state = winState;

    if (state == aSTATE_ICONIZED)
    {
        iconAnimation->draw(displayPort->frame(), 0, 0);
    }
    else
    {
        if (backgroundPort != nullptr)
        {
            backgroundPort->copyTo(displayPort->frame(), 0, 0, -1);
        }

        if (windowAnimation != nullptr && animating != 0)
        {
            windowAnimation->draw(displayPort->frame(), 0, 0);
        }
    }

    if (state != aSTATE_ICONIZED)
    {
        paint();

        for (int32_t i = 0; i < numChildren; i++)
        {
            DrawChild(childList[i]);
        }
    }
}

auto aObject::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    if (DrawsLive())
    {
        SlideStep();
        DrawInFramePass(displayPort);
        return;
    }

    if (winState == aSTATE_ICONIZED)
    {
        if (iconAnimation != nullptr)
        {
            draw();
        }
    }
    else if (windowAnimation != nullptr)
    {
        windowAnimation->draw(displayPort->frame(), 0, 0);
        draw();
    }

    SlideStep();

    if (displayPort != nullptr)
    {
        displayPort->copyTo(framePane, 0, 0, transparent);
    }

    if (winState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->display();
        }
    }
}

auto aObject::SetDrawsLive() -> void
{
    drawsLive = true;

    if (displayPort != nullptr && !displayPort->isView())
    {
        displayPort->initView(width(), height());
    }
}

auto aObject::DrawsChild(aObject* child) -> bool
{
    return !child->DrawsLive() && drawingLive != this;
}

auto aObject::DrawChild(aObject* child) -> void
{
    if (drawingLive == this)
    {
        return;
    }

    if (child->DrawsLive())
    {
        child->Refresh();
    }
    else
    {
        child->draw();
    }
}

auto aObject::Refresh() -> void
{
    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->Refresh();
    }
}

auto aObject::DrawInFramePass(aPort* port, int32_t scrollY, bool wipe, bool displayChildren) -> void
{
    // The view lies over the pane, on the window the pane is on (the screen, or a scroll pane's content), cut to the
    // window and to the scissor of the nearest clipping ancestor on the same window.
    _window* target = framePane->window;
    MCRect scissor{std::max(framePane->x0, 0), std::max(framePane->y0, 0), std::min(framePane->x1, target->x_max),
                   std::min(framePane->y1, target->y_max)};

    if (!viewClips.empty() && viewClips.back().first == target)
    {
        const MCRect& outer = viewClips.back().second;
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

    port->openView(target, framePane->x0, framePane->y0 - scrollY, scissor, transparent != 0);
    aObject* const outerDrawing = drawingLive;
    drawingLive = this;

    // A picture that was never painted held zeros (the port's heap clears new blocks), and an opaque object copied
    // them to the screen; a transparent one let what was under it show.
    if (transparent == 0 && wipe)
    {
        VFX_pane_wipe(port->frame(), 0);
    }

    if (winState != aSTATE_ICONIZED || iconAnimation != nullptr)
    {
        draw();
    }

    drawingLive = outerDrawing;
    port->closeView();

    if (winState != aSTATE_ICONIZED && displayChildren)
    {
        const bool clips = ClipsChildren();

        if (clips)
        {
            viewClips.emplace_back(target, scissor);
        }

        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->display();
        }

        if (clips)
        {
            viewClips.pop_back();
        }
    }
}

auto aObject::SlideStep() -> void
{
    if (hideOffset != 0)
    {
        // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
        if (hideDirection == DIRECTION_LEFT || hideDirection == DIRECTION_RIGHT)
        {
            moveTo(x() + hideOffset, y(), -1);
        }
        else
        {
            moveTo(x(), y() + hideOffset, -1);
        }

        if (hidden != 0)
        {
            const tagRECT screen = {0, 0, application->width(), application->height()};

            if (rectIntersect(screen) == 0)
            {
                hideOffset = 0;
            }
        }
        else
        {
            bool home = false;

            if (hideOffset < 0)
            {
                home = homeX >= globalX() && homeY >= globalY();
            }
            else if (hideOffset > 0)
            {
                home = homeX <= globalX() && homeY <= globalY();
            }

            if (home)
            {
                const int32_t homeYOffset = homeY - parent->globalY();
                moveTo(homeX - parent->globalX(), homeYOffset, -1);
                hideOffset = 0;
            }
        }
    }
}

auto aObject::handleEvent(aEvent* event) -> void
{
    if (event->type == 1)
    {
        if (parent != nullptr)
        {
            bringToFront(0);
            aRedrawScreen();
        }
    }
    else if (event->type == 0x12)
    {
        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->handleEvent(event);
        }
    }

    if (winState == aSTATE_ICONIZED)
    {
        switch (event->type)
        {
            case 1:
            {
                application->grab(this);
                lastX = event->x;
                lastY = event->y;
                break;
            }
            case 4:
            {
                if (application->grabbedObject() == this)
                {
                    application->release();
                    aObject* target = screenWindow->findObject(event->x, event->y);

                    if (target != nullptr)
                    {
                        target->enter();
                    }
                }
                break;
            }
            case 7:
            {
                if (application->grabbedObject() == this)
                {
                    const int32_t eventY = event->y;
                    aObject* target = screenWindow->findObject(event->x, eventY);

                    if (target != nullptr && target->depth() == 100)
                    {
                        return;
                    }

                    const int32_t newY = y() + (eventY - lastY);
                    moveTo(x() + (event->x - lastX), newY, 0);
                    lastX = event->x;
                    lastY = eventY;
                }
                break;
            }
            case 0x10:
            {
                if (application->grabbedObject() == this)
                {
                    application->release();
                }

                normalize();
                break;
            }
        }
    }
    else if (event->type == 0xd)
    {
        destroy();
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

auto aObject::setBackground(char* fileName) -> int32_t
{
    if (backgroundPort != nullptr)
    {
        backgroundPort->destroy();
        delete backgroundPort;
        backgroundPort = nullptr;
    }

    backgroundPort = new aPort;

    if (backgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return backgroundPort->init(fileName);
}

auto aObject::setBackground(int32_t artPacket) -> int32_t
{
    if (backgroundPort != nullptr)
    {
        backgroundPort->destroy();
        delete backgroundPort;
        backgroundPort = nullptr;
    }

    backgroundPort = new aPort;

    if (backgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return backgroundPort->init(artPacket);
}

auto aObject::maximize() -> void
{
    setState(aSTATE_MAXIMIZED);
}

auto aObject::normalize() -> void
{
    setState(aSTATE_NORMAL);
}

auto aObject::iconize() -> void
{
    setState(aSTATE_ICONIZED);
}

auto aObject::state() -> int32_t
{
    return winState;
}

auto aObject::setState(int32_t newState) -> void
{
    // Saves the placement of the state left, then takes the new state's. From the iconized state the new state is
    // stored first, so an unknown state sticks (with the icon's placement).
    switch (winState)
    {
        case aSTATE_NORMAL:
        {
            normalY = winY;
            normalX = winX;
            normalWidth = winWidth;
            normalHeight = winHeight;

            if (newState == aSTATE_MAXIMIZED)
            {
                winX = maxX;
                winState = aSTATE_MAXIMIZED;
                winY = maxY;
                winWidth = maxWidth;
                winHeight = maxHeight;
            }
            else if (newState == aSTATE_ICONIZED)
            {
                winX = iconX;
                winState = aSTATE_ICONIZED;
                winY = iconY;
                winWidth = iconWidth;
                winHeight = iconHeight;
            }
            break;
        }
        case aSTATE_MAXIMIZED:
        {
            maxY = winY;
            maxX = winX;
            maxWidth = winWidth;
            maxHeight = winHeight;

            if (newState == aSTATE_NORMAL)
            {
                winX = normalX;
                winState = aSTATE_NORMAL;
                winY = normalY;
                winWidth = normalWidth;
                winHeight = normalHeight;
            }
            else if (newState == aSTATE_ICONIZED)
            {
                winX = iconX;
                winState = aSTATE_ICONIZED;
                winY = iconY;
                winWidth = iconWidth;
                winHeight = iconHeight;
            }
            break;
        }
        case aSTATE_ICONIZED:
        {
            iconX = winX;
            iconY = winY;
            winState = newState;
            iconWidth = winWidth;
            iconHeight = winHeight;

            if (newState == aSTATE_NORMAL)
            {
                winX = normalX;
                winState = aSTATE_NORMAL;
                winY = normalY;
                winWidth = normalWidth;
                winHeight = normalHeight;
            }
            else if (newState == aSTATE_MAXIMIZED)
            {
                winX = maxX;
                winState = aSTATE_MAXIMIZED;
                winY = maxY;
                winWidth = maxWidth;
                winHeight = maxHeight;
            }
            break;
        }
    }

    resize(winWidth, winHeight);
    moveTo(winX, winY, 0);
    aRedrawScreen();
}

auto aObject::setAnimation(char* fileName) -> int32_t
{
    if (windowAnimation != nullptr)
    {
        windowAnimation->destroy();
        delete windowAnimation;
        windowAnimation = nullptr;
    }

    windowAnimation = new (std::nothrow) aAnimation;
    return windowAnimation->init(fileName);
}

auto aObject::setIcon(char* fileName) -> int32_t
{
    if (iconAnimation != nullptr)
    {
        iconAnimation->destroy();
        delete iconAnimation;
        iconAnimation = nullptr;
    }

    aAnimation* newIcon = new (std::nothrow) aAnimation;
    iconAnimation = newIcon;
    const int32_t result = newIcon->init(fileName);

    if (result == 0)
    {
        iconWidth = newIcon->width();
        iconHeight = newIcon->height();
    }

    return result;
}

auto aObject::animation() -> aAnimation*
{
    return windowAnimation;
}

auto aObject::icon() -> aAnimation*
{
    return iconAnimation;
}

auto aObject::background() -> aPort*
{
    return backgroundPort;
}

auto aObject::SetBit(int32_t xPos, int32_t yPos, uint8_t color) -> void
{
    if (xPos > -1 && xPos < width() && yPos > -1 && yPos < height())
    {
        const int32_t rowLength = width();

        if (displayPort->buffer() != nullptr)
        {
            displayPort->buffer()[rowLength * yPos + xPos] = color;
        }
        else if (displayPort->isView())
        {
            // Port: a view has no pixels; the pixel is drawn through it.
            VFX_pixel_write(displayPort->frame(), xPos, yPos, color);
        }
    }
}

auto aObject::HideMe(int hide) -> void
{
    if (hidden == hide || hideOffset != 0)
    {
        return;
    }

    if (hide != 0)
    {
        homeX = globalX();
        homeY = globalY();

        switch (hideDirection)
        {
            case DIRECTION_LEFT:
            {
                hidden = hide;
                hideOffset = -(globalX() + width());
                return;
            }
            case DIRECTION_UP:
            {
                hidden = hide;
                hideOffset = -(globalY() + height());
                return;
            }
            case DIRECTION_RIGHT:
            {
                hidden = hide;
                hideOffset = application->width() - globalX();
                return;
            }
            case DIRECTION_DOWN:
            {
                hidden = hide;
                hideOffset = application->height() - globalY();
                return;
            }
            default:
            {
                hidden = 0;
                hideOffset = 0;
                return;
            }
        }
    }

    if (homeX != globalX())
    {
        const int32_t current = globalX();
        hidden = 0;
        hideOffset = homeX - current;
        return;
    }

    const int32_t current = globalY();
    hidden = 0;
    hideOffset = homeY - current;
}

// Free functions.

auto createDIBSection(int32_t width, int32_t height, void** bitmapInfo, void** bitmap, uint8_t** bits) -> int32_t
{
    // Port: no GDI. The original filled a BITMAPINFO (0x428 bytes from the GUI heap, colour table = palette indexes)
    // and made an 8-bit top-down DIB section; the port gives a heap buffer as the section and its bits. Uncalled in
    // MCX.EXE.
    if (*bitmapInfo == nullptr)
    {
        *bitmapInfo = guiHeap->malloc(0x428);
    }

    if (*bitmapInfo == nullptr)
    {
        return 3;
    }

    *bits = static_cast<uint8_t*>(guiHeap->malloc(static_cast<uint32_t>(width * height)));
    *bitmap = *bits;
    return *bitmap != nullptr ? 0 : 2;
}

auto CreatePaletteFromGIF(char* fileName) -> void*
{
    // Port: no GDI palettes. The "palette" returned is 256 VFX_RGB from the GUI heap, 8 bits per channel as the
    // original's PALETTEENTRY values. Uncalled in MCX.EXE.
    char path[128];
    char message[256];
    File gifFile;
    std::snprintf(path, sizeof(path), "%s%s", palettePath, fileName);

    if (fileExists(path) == 0)
    {
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
    }

    gifFile.open(path, READ, 50);
    const uint32_t size = gifFile.fileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading from '%s'", path);
        GeneralMsg(message);
    }

    uint8_t* gif = static_cast<uint8_t*>(guiHeap->malloc(size));

    if (gif == nullptr)
    {
        std::snprintf(message, sizeof(message), "Error allocating memory for '%s'", path);
        GeneralMsg(message);
    }

    gifFile.read(gif, static_cast<int32_t>(size));
    gifFile.close();
    VFX_RGB colors[256];
    VFX_GIF_palette(gif, colors);
    guiHeap->free(gif);
    return CreatePaletteFromRAM(colors);
}

auto CreatePaletteFromRAM(void* colors) -> void*
{
    // Port: see CreatePaletteFromGIF. The colours are 6-bit, shifted up.
    VFX_RGB source[256];
    std::memcpy(source, colors, sizeof(source));
    VFX_RGB* palette = static_cast<VFX_RGB*>(guiHeap->malloc(sizeof(VFX_RGB) * 256));

    for (int i = 0; i < 256; i++)
    {
        palette[i].r = static_cast<uint8_t>(source[i].r << 2);
        palette[i].g = static_cast<uint8_t>(source[i].g << 2);
        palette[i].b = static_cast<uint8_t>(source[i].b << 2);
    }

    return palette;
}

auto CreateSmackPaletteFromRAM(void* colors) -> void*
{
    // Port: see CreatePaletteFromGIF. Smacker's colours are already 8-bit.
    VFX_RGB* palette = static_cast<VFX_RGB*>(guiHeap->malloc(sizeof(VFX_RGB) * 256));
    std::memcpy(palette, colors, sizeof(VFX_RGB) * 256);
    return palette;
}

auto aRedrawScreen() -> void
{
    for (int32_t i = 0; i < screenWindow->numberOfChildren(); i++)
    {
        screenWindow->child(i)->draw();
    }
}

auto aLockScreen() -> int32_t
{
    if (lockActive == 0)
    {
        lockActive = -1;
        // Port: the original locked the DirectDraw back surface here in 16-bit full screen and drew straight into
        // it; the port's screen is always 8-bit, so the screen port always shows the display's buffer.
        screenPort->resize(gWidth, gHeight);
        screenPort->bitmap()->buffer = screenBits;
    }

    return 0;
}

auto aUnlockScreen() -> int32_t
{
    // Port: unlocking the 16-bit full-screen surface is gone with it (see aLockScreen).
    if (lockActive != 0)
    {
        lockActive = 0;
    }

    return 0;
}

auto aPostMessage(aObject* obj, int32_t message) -> void
{
    aEvent event;
    event.clear();
    event.type = message;

    // Port: the original skipped unreadable pointers (IsBadReadPtr); the port skips null.
    if (obj != nullptr)
    {
        obj->handleEvent(&event);
    }
}

auto TestMsgCallback(FIDPMessage* message, void* data) -> void
{
    (void)data;
    MPlayer->sessionManager->GetPlayer(message->fromID);
}

auto SendAndReceiveTestMessages() -> void
{
    SessionManager* manager = MPlayer->sessionManager;
    TestMessage test = {};
    test.header.header = 0x1064;
    manager->applicationCallback = TestMsgCallback;
    manager->applicationCallbackData = nullptr;
    MP_Start_Time = MCPort::Milliseconds();

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
                test.frame = Networkframe;
                test.index = i;
                manager->SendMessageToGroup(0, &test.header, sizeof(test));
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
    FullPathFileName fileName;
    FitIniFile gameFile;
    MCInput::GetAsyncKeyState(VK_ESCAPE);
    fileName.init(missionPath, commandLine, "");

    if (gameFile.open(fileName, READ, 50) != 0 || gameFile.seekBlock("Multiplayer") != 0)
    {
        return 0;
    }

    uint32_t tries = 0;
    MultiPlayer* newPlayer = new MultiPlayer;
    MPlayer = newPlayer;
    Assert(MPlayer != nullptr, 0, " Unable to create MultiPlayer object ");
    Assert(MPlayer->init(0x7d000, 0x100, 100) == 0, 0, "could not initialize multiplayer");

    if (MPlayer->init(&gameFile) == static_cast<int32_t>(0x8877042e))
    {
        const int32_t numPlayers = MPlayer->numPlayers();

        if (MPlayer->isServer == 0)
        {
            uint32_t result;

            do
            {
                do
                {
                    tries++;
                } while (tries % 50 != 0);

                Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0, 0, "User exited");
                result = static_cast<uint32_t>(MPlayer->joinSession(nullptr, nullptr));
                Assert(result != 0xfffffffe, result, "Error joining session!");
            } while (result != 0);
        }
        else
        {
            MPlayer->createSession(nullptr, nullptr, 6);

            while (MPlayer->playersInSession() < numPlayers)
            {
                tries++;

                if (tries % 50 == 0)
                {
                    MPlayer->processReceiveList();
                    Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 1) == 0, 0, "User exited");
                }
            }

            Assert(MPlayer->playersInSession() > 1, 0, "No other players joined in time.");
        }

        started = -1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    MPlayer->processReceiveList();

    while (MPlayer->sessionManager->myPlayer->playerNumber < 0)
    {
        MPlayer->processReceiveList();
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
                globalGameSegment = std::atoi(words[i]);

                if (globalGameSegment < 1 || globalGameSegment > 99)
                {
                    globalGameSegment = 0;
                }
            }
        }
        else if (std::strcmp(word, "+") == 0)
        {
            i++;

            if (i < numWords)
            {
                globalGameSegment = std::atoi(words[i]);

                if (globalGameSegment < 1 || globalGameSegment > 50)
                {
                    globalGameSegment = 0;
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
                    gRenderer = static_cast<int>(*kind);
                }
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
                startupPakFile = new char[length];
                std::memcpy(startupPakFile, words[i], length);
            }
        }
    }

    if (network != 0)
    {
        StartMultiplayerGame(networkFile);
    }
}

auto parseCommandLine(char* line, int32_t pos, char* word, int32_t maxLength) -> int32_t
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
    std::srand(static_cast<uint32_t>(std::time(nullptr)));

    if (prevInstance != nullptr)
    {
        GeneralMsg("An instance of this application is already running.");
    }

    thisInstance = instance;
    globalHeapList = new (std::nothrow) HeapList();
    std::strcpy(savePath, "c:\\Program Files\\Honor Bound\\");
    std::strcpy(directXPath, "\\honorb\\directx\\");
    std::strcpy(terrainPath, "data\\terrain\\");
    std::strcpy(palettePath, "data\\palette\\");
    std::strcpy(artPath, "data\\art\\");
    std::strcpy(fontPath, "data\\fonts\\");
    std::strcpy(soundPath, "data\\sound\\");
    std::strcpy(spritePath, "data\\sprites\\");
    std::strcpy(interfacePath, "data\\iface\\");
    std::strcpy(paletteName, "palette.gif");

    // The application comes from the global heap (the GUI heap doesn't exist yet).
    aSystem* newApplication = ::new (std::nothrow) aSystem;

    if (newApplication == nullptr)
    {
        application = nullptr;
        GeneralMsg("Initialization Failure!");
    }

    oldMouseY = -1;
    oldMouseX = -1;
    rightMouseButtonDown = 0;
    leftMouseButtonDown = 0;
    application = newApplication;

    if (application->start(instance, nullptr, commandLine, showCommand, 640, 480) != 0)
    {
        return -4;
    }

    application->run();
    application->stop();

    if (globalHeapList != nullptr)
    {
        *globalHeapList = HeapList();
        delete globalHeapList;
    }

    application->~aSystem();
    ::operator delete(application);
    return 0;
}

auto DestroyVersion() -> void
{
    if (versionDialog != nullptr)
    {
        versionDialog->destroy();
        delete versionDialog;
        versionDialog = nullptr;
    }
}

auto handleEvent(aEvent* event) -> void
{
    if (screenWindow == nullptr)
    {
        return;
    }

    const int32_t type = event->type;

    if (type == 0x13 || (type > 0x13ff && type < 0x2401))
    {
        // Timer events and posted messages go to the tactical interface.
        if (scenario == nullptr || turn < 1)
        {
            return;
        }

        theInterface->handleEvent(event);
        return;
    }

    aObject* target;

    if (application->textObject() != nullptr && (type == 10 || type == 9 || type == 8))
    {
        target = application->textObject();
    }
    else if (application->grabbedObject() != nullptr)
    {
        target = application->grabbedObject();
    }
    else if (EventsToMissionResultsScreen != 0 && mission->resultsScreen != nullptr)
    {
        aObject* results = mission->resultsScreen;

        if (type == 10 && event->key == VK_ESCAPE)
        {
            target = results;
        }
        else
        {
            target = results->findObject(event->x, event->y);
        }
    }
    else
    {
        if (type == 9)
        {
            if (featureScreen != nullptr)
            {
                featureScreenDone = -1;
            }

            const uint8_t key = event->key;

            switch (key)
            {
                case VK_RETURN:
                {
                    if (scenario != nullptr && (gamePaused != 0 || gameAsked != 0) && event->ctrlKey == 0 &&
                        event->altKey == 0)
                    {
                        mission->endScenarioRequested = -1;

                        if (gameAsked != 0)
                        {
                            scenarioResult = 3;
                        }

                        gamePaused = 0;
                        gameAsked = 0;
                    }
                    break;
                }
                default:
                {
                    keySetting = static_cast<char>(key);

                    if (key == VK_F12)
                    {
                        QueuePlayerOrders = (QueuePlayerOrders != 0) - 1;
                    }
                    break;
                }
                case VK_PAUSE:
                {
                    if (scenario != nullptr && MPlayer == nullptr && turn > 0)
                    {
                        gamePaused = ~gamePaused;
                    }

                    if (event->altKey != 0 && AssertTest(0x80, const_cast<char*>("User Break")) != 0)
                    {
                        SDL_TriggerBreakpoint();
                        return;
                    }
                    break;
                }
                case VK_ESCAPE:
                {
                    if (scenario != nullptr && EventsToMissionResultsScreen == 0 && scenario->startingUp == 0 &&
                        scenario->startUpTurns + scenario->unknown2A4 < turn)
                    {
                        if (MPlayer == nullptr)
                        {
                            gamePaused = ~gamePaused;
                        }
                        else
                        {
                            gameAsked = ~gameAsked;
                        }
                    }
                    break;
                }
                case 'D':
                {
                    if (scenario != nullptr && cheatsOn != 0 && MPlayer == nullptr && keyHeld(VK_CONTROL) &&
                        keyHeld(VK_MENU))
                    {
                        disableHomeTeamTargets();
                    }
                    break;
                }
                case 'G':
                {
                    if (scenario == nullptr)
                    {
                        break;
                    }

                    if (cheatsOn != 0 && MPlayer == nullptr && keyHeld(VK_CONTROL) && keyHeld(VK_MENU))
                    {
                        forceGatesClosed = -1;
                    }

                    // Original bug (OB-063): no break, so the gate cheat also runs the kill cheat below.
                    [[fallthrough]];
                }
                case 'K':
                {
                    if (scenario != nullptr && cheatsOn != 0 && MPlayer == nullptr && keyHeld(VK_CONTROL) &&
                        keyHeld(VK_MENU))
                    {
                        killHomeTeamTargets();
                    }
                    break;
                }
                case 'L':
                {
                    if (scenario != nullptr && cheatsOn != 0 && keyHeld(VK_CONTROL))
                    {
                        drawTerrainGrid = ~drawTerrainGrid;
                    }
                    break;
                }
                case 'P':
                {
                    if (keyHeld(VK_CONTROL) && keyHeld(VK_MENU))
                    {
                        if (displayProfileData == 0)
                        {
                            displayProfileData = 1;
                        }
                        else if (displayProfileData == 1)
                        {
                            displayProfileData = 2;
                        }
                        else if (displayProfileData == 2)
                        {
                            displayProfileData = 0;
                        }
                    }
                    break;
                }
                case 'Q':
                {
                    if (scenario != nullptr && cheatsOn != 0 && event->ctrlKey != 0 && event->altKey != 0 &&
                        MPlayer == nullptr)
                    {
                        mission->endScenarioRequested = -1;
                    }
                    break;
                }
                case 'S':
                {
                    if (keyHeld(VK_CONTROL) && keyHeld(VK_MENU))
                    {
                        lockFrameRate = ~lockFrameRate;
                    }
                    break;
                }
                case 'V':
                {
                    if (cheatsOn != 0 && keyHeld(VK_CONTROL) && keyHeld(VK_MENU))
                    {
                        char version[256];
                        char name[256];
                        char release[256];
                        char text[256];
                        cLoadString(thisInstance, 0x280, name, 0xfe);
                        cLoadString(thisInstance, 0x281, version, 0xfe);
                        cLoadString(thisInstance, 0x282, release, 0xfe);
                        std::snprintf(text, sizeof(text), "Release Version: %s", release);
                        DestroyVersion();
                        versionDialog = new aMessageBox;
                        versionDialog->init(reinterpret_cast<uint8_t*>(text));
                        screenWindow->addChild(versionDialog);
                        application->grab(versionDialog);
                    }
                    break;
                }
                case 'W':
                {
                    if (cheatsOn != 0 && scenario != nullptr && event->ctrlKey != 0 && event->altKey != 0 &&
                        MPlayer == nullptr)
                    {
                        scenario->startingUp = 0;
                        mission->endScenarioRequested = -1;
                        scenarioResult = 5;
                        scenario->startUpCountdown = 0;
                    }
                    break;
                }
                case 'Z':
                {
                    if (keyHeld(VK_CONTROL) && application->smackerWindow == nullptr &&
                        application->smackerWindow2 == nullptr)
                    {
                        application->gammaCorrectCurrentPalette();
                    }
                    break;
                }
            }
        }

        target = screenWindow->findObject(event->x, event->y);
    }

    if ((event->type == 8 || event->type == 9) && application->textObject() == nullptr && theInterface != nullptr &&
        EventsToMissionResultsScreen == 0 && scenario != nullptr && turn > 0)
    {
        theInterface->handleEvent(event);
    }

    if (application->grabbedObject() == nullptr && application->modalObject() != nullptr)
    {
        // A modal object only takes events for itself and its children.
        aObject* owner = target;

        while (owner != nullptr && owner != application->modalObject())
        {
            owner = owner->parent;
        }

        if (owner != application->modalObject())
        {
            return;
        }
    }

    if (event->type == 1 || event->type == 3)
    {
        if (application->textObject() != nullptr && target != application->textObject())
        {
            application->releaseText();
        }
    }
    else if (event->type == 7 && application->grabbedObject() == nullptr && target != application->currentObject())
    {
        if (application->currentObject() != nullptr)
        {
            application->currentObject()->leave();
        }

        if (target != nullptr)
        {
            target->enter();
        }

        application->setCurrentObject(target);
    }

    if (target == nullptr)
    {
        return;
    }

    event->target = target;
    target->handleEvent(event);
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

auto translateMessage(void* window, uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
{
    (void)window;

    if (applicationActive == 0)
    {
        return 0;
    }

    const tagPOINT cursor = GetMessageCursorLoc();
    aEvent event;
    event.clear();
    const uint8_t low = static_cast<uint8_t>(wParam);
    const int16_t scanCode = static_cast<int16_t>((lParam >> 16) & 0x1ff);

    switch (message)
    {
        case WM_PAINT:
            event.type = 0xc;
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            // Only the first press, not the repeats.
            if ((lParam & 0xffff) == 1)
            {
                event.type = 9;
                event.key = low;
                event.scanCode = scanCode;
                event.ctrlKey = (MCInput::GetKeyState(VK_CONTROL) & 0x8000) != 0 ? 0xff : 0;
                event.shiftKey = (MCInput::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 0xff : 0;
            }
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            event.type = 8;
            event.key = low;
            event.scanCode = scanCode;
            event.ctrlKey = (MCInput::GetKeyState(VK_CONTROL) & 0x8000) != 0 ? 0xff : 0;
            event.shiftKey = (MCInput::GetKeyState(VK_SHIFT) & 0x8000) != 0 ? 0xff : 0;
            break;
        }
        case WM_CHAR:
        {
            if (cheatsOn != 0)
            {
                CheatKey[CheatPointer] = static_cast<char>(low);
                CheatPointer = (CheatPointer + 1) & 0x7f;

                if (Cheat(Cheat_framegraph) != 0)
                {
                    AndyFramerate ^= 1;
                }

                if (scenario != nullptr && turn > 0 && MPlayer == nullptr)
                {
                    if (Cheat(Cheat_HealAll) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        HealAll();
                    }

                    if (Cheat(Cheat_DeadEye) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        DeadEye();
                    }

                    if (Cheat(Cheat_CantHitMe) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        CantHitMe = CantHitMe == 0;
                    }

                    if (Cheat(Cheat_GetSalvage) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        CantBlowSalvage = CantBlowSalvage == 0;
                    }

                    if (Cheat(Cheat_Reveal) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        RevealAll();
                    }

                    if (Cheat(Cheat_BunnyStrike) != 0)
                    {
                        soundSystem->playBettySample(0x16);
                        BunnyStrikesOn = BunnyStrikesOn == 0;
                    }

                    if (Cheat(Cheat_Duh) != 0)
                    {
                        soundSystem->playBettySample(0x1c);
                        Duh = Duh == 0;
                    }
                }
            }

            event.type = 10;
            event.key = low;
            event.scanCode = scanCode;
            // The character's modifiers are read from wParam's MK_ bits, as for a mouse message.
            event.ctrlKey = low & MK_CONTROL;
            event.shiftKey = low & MK_SHIFT;
            break;
        }
        case WM_TIMER:
            event.type = 0x13;
            break;
        case WM_LBUTTONDBLCLK:
        {
            event.type = 0x10;
            event.ctrlKey = low & MK_CONTROL;
            event.shiftKey = low & MK_SHIFT;
            break;
        }
        case WM_RBUTTONDBLCLK:
        {
            event.type = 0x11;
            event.ctrlKey = low & MK_CONTROL;
            event.shiftKey = low & MK_SHIFT;
            break;
        }
        case WM_MOUSEWHEEL:
        {
            // The original ignored the wheel. Over the battlefield (the main pane itself, not the interface drawn
            // over it) it zooms like the zoom keys: up in, down out. Over anything else it scrolls the first of the
            // object and its parents that has a scroll bar (aObject::MouseWheel).
            if (screenWindow == nullptr || application->grabbedObject() != nullptr)
            {
                return 1;
            }

            const int16_t delta = static_cast<int16_t>(wParam >> 16);
            aObject* target = nullptr;

            if (EventsToMissionResultsScreen != 0 && mission != nullptr && mission->resultsScreen != nullptr)
            {
                target = mission->resultsScreen->findObject(cursor.x, cursor.y);
            }
            else
            {
                target = screenWindow->findObject(cursor.x, cursor.y);
            }

            if (target == nullptr)
            {
                return 1;
            }

            if (theInterface != nullptr && scenario != nullptr && turn > 0 && EventsToMissionResultsScreen == 0 &&
                mainHolder != nullptr && target == mainHolder->GetActivePane())
            {
                // A step per notch (finer wheels zoom finer); not while paused or asked.
                const float step = std::pow(InterfaceObject::ZoomWheelStep, std::fabs(delta / 120.0f));

                if (delta > 0)
                {
                    theInterface->ZoomIn(step, false);
                }
                else if (delta < 0)
                {
                    theInterface->ZoomOut(step, false);
                }

                return 1;
            }

            // A modal object only takes the wheel for itself and its children, as for other events.
            if (application->modalObject() != nullptr)
            {
                aObject* owner = target;

                while (owner != nullptr && owner != application->modalObject())
                {
                    owner = owner->parent;
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

            for (aObject* object = target; object != nullptr; object = object->parent)
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
        event.type = static_cast<int32_t>(message);
    }

    event.lParam = lParam;
    event.x = cursor.x;
    event.data = static_cast<int32_t>(wParam);
    event.leftButton = low & MK_LBUTTON;
    event.middleButton = low & MK_MBUTTON;
    event.rightButton = low & MK_RBUTTON;
    event.y = cursor.y;
    event.target = nullptr;
    event.altKey = (MCInput::GetKeyState(VK_MENU) & 0x8000) != 0 ? 0xff : 0;

    if (event.type != 0)
    {
        handleEvent(&event);
    }

    return 0;
}

auto ScrollScreen() -> void
{
    Camera* camera = nullptr;
    const tagRECT scrollArea = application->scrollRect;

    if (theInterface == nullptr)
    {
        return;
    }

    if (scenario != nullptr && turn < 5)
    {
        return;
    }

    int16_t speed = theInterface->scrollSpeed;

    if (mainHolder != nullptr && mainHolder->GetActivePane() != nullptr)
    {
        camera = mainHolder->GetActivePane()->GetCamera();
    }

    int32_t dx = 0;
    int32_t dy = 0;

    if (camera != nullptr)
    {
        if (camera->cameraScale == 100)
        {
            speed = static_cast<int16_t>(speed / 2);
        }

        // MCX.EXE's constant is a hair under 15 (14.999999).
        float step = frameLength * 0x1.dffffep+3f * static_cast<float>(speed);

        // Port: the same speed on the screen at any zoom (the world surface's pixels per screen pixel).
        if (camera->window != nullptr && camera->window->WorldScaleY() > 0.0f)
        {
            step /= camera->window->WorldScaleY();
        }

        bool scroll = true;

        if (theInterface->scrollDirection == -1)
        {
            // Scroll by the mouse at the screen's edge, after the interface's start delay.
            const MCPoint cursor = MCInput::GetCursorPos();

            if (PtInRect(&scrollArea, POINT{cursor.x, cursor.y}) == 0)
            {
                if (scrollWait == 0)
                {
                    scrollWait = MCPort::Milliseconds();
                }
                else
                {
                    const int16_t delay = theInterface->scrollStart;

                    if (static_cast<uint32_t>(delay + static_cast<int32_t>(scrollWait)) > MCPort::Milliseconds())
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
                scrollWait = 0;
                scroll = false;
            }
        }
        else
        {
            switch (theInterface->scrollDirection)
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
            viewWindow* window = camera->window;
            vector_2d anchor(window->selectionBox[0] / window->WorldScaleX(),
                             window->selectionBox[1] / window->WorldScaleY());
            vector_3d point;
            camera->inverseProject(anchor, point);
            camera->scrollCamera(dx, dy);
            const float scale = camera->cameraScale == 1 ? 0.5f : 1.0f;
            const float offsetX = (point.x - camera->position.x) * scale;
            const float offsetY = (point.y - camera->position.y) * scale;
            const float offsetZ = scale * (point.z - camera->position.z);
            const vector_2d moved(offsetY * camera->cosAngle + offsetX * camera->cosAngle + camera->halfWidth,
                                  ((offsetX * camera->sinAngle + camera->halfHeight) - offsetY * camera->sinAngle) -
                                      offsetZ);
            const vector_2d shown = window->WorldToWindow(moved);
            window->selectionBox[0] = shown.x;
            window->selectionBox[1] = shown.y;
        }
    }

    // The tactical map scrolls by its buttons.
    const int16_t mapSpeed = theInterface->tacScrollSpeed;
    int32_t mapDx = 0;
    int32_t mapDy = 0;

    if (Terrain::terrainTacticalMap == nullptr)
    {
        return;
    }

    switch (theInterface->tacScrollDirection)
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

    Terrain::terrainTacticalMap->scrollMap(mapDx, mapDy);
}

auto WindowProc(void* window, uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
{
    // Port: the GDI palette work (SelectPalette / RealizePalette on the desktop DC) and the window placement calls
    // have no counterpart: the display owns the palette and the window. What remains is when the original repainted.
    constexpr uint32_t WM_ERASEBKGND = 0x14;
    constexpr uint32_t SC_KEYMENU = 0xf100;
    constexpr uint32_t SC_MAXIMIZE = 0xf030;
    constexpr uint32_t SC_MINIMIZE = 0xf020;

    if (message == uMessage)
    {
        return 1;
    }

    switch (message)
    {
        case WM_ERASEBKGND:
        {
            if (displayReady != 0)
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
            application->setScreenOffsetX(lParam & 0xffff);
            application->setScreenOffsetY(static_cast<uint32_t>(lParam) >> 16);
            break;
        }
        case WM_SIZE:
        {
            gWinHeight = static_cast<uint32_t>(lParam) >> 16;
            gWinWidth = lParam & 0xffff;
            // Port: the original snapped the window back to the screen's size (SetWindowPos); the display letterboxes.
            return 0;
        }
        case WM_PAINT:
        {
            if (displayReady != 0 && gFullScreen == 0 && keepDesktopPalette == 0)
            {
                UpdateDisplay(0, 0, 0, 0, 0);
            }
            break;
        }
        case WM_QUERYNEWPALETTE:
        {
            if (displayReady == 0 || gFullScreen != 0)
            {
                return 0;
            }

            return -1;
        }
        case WM_PALETTECHANGED:
            return 0;
        case WM_ACTIVATEAPP:
        {
            if (application->smackerWindow2 != nullptr)
            {
                closeMovieWindow(application->smackerWindow2);
            }

            if (application->smackerWindow != nullptr)
            {
                closeMovieWindow(application->smackerWindow);
            }

            if (MPlayer == nullptr)
            {
                applicationActive = static_cast<int>(wParam);
            }
            else if (gFullScreen != 0 && applicationActive != 0)
            {
                InitWindowMode();
            }

            if (applicationActive != 0 && displayReady != 0 && keepDesktopPalette == 0)
            {
                UpdateDisplay(0, 0, 0, 0, 0);
            }
            break;
        }
        case WM_KEYDOWN:
        {
            if (wParam == VK_ESCAPE)
            {
                if (application->smackerWindow2 != nullptr)
                {
                    blankScreen();
                    blankScreen();
                    closeMovieWindow(application->smackerWindow2);
                    escapedSmackerMovie = -1;
                }
                else if (application->smackerWindow != nullptr)
                {
                    static_cast<aSmackerWindow*>(application->smackerWindow)->endSmackerMovie();
                    escapedSmackerMovie = -1;
                }
            }
            break;
        }
    }

    if (translateMessage(window, message, wParam, lParam) != 0)
    {
        return 0;
    }

    if (message == WM_SYSKEYDOWN)
    {
        // Alt+Enter switches between full screen and a window.
        if (wParam == VK_RETURN && allowMagicWindowSwitching != 0)
        {
            if (application->smackerWindow2 != nullptr || displayReady == 0 || application->smackerWindow != nullptr)
            {
                return 0;
            }

            if (gFullScreen != 0)
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
        if (wParam == SC_KEYMENU || allowMagicWindowSwitching == 0)
        {
            return 0;
        }

        if (wParam == SC_MAXIMIZE && (gFullScreen != 0 || displayReady == 0 || application->smackerWindow2 != nullptr ||
                                      application->smackerWindow != nullptr))
        {
            return 0;
        }

        if (wParam == SC_MINIMIZE)
        {
            return 0;
        }
    }

    return 0;
}

auto InitWindowMode() -> void
{
    if (gFullScreen != 0)
    {
        if (mouseThreadStarted != 0)
        {
            MouseCritSec.lock();
            InMouseCritSec = 1;
        }

        gFullScreen = 0;
        application->resetDirectDraw(gWidth, gHeight, 8);
        displayReady = 0;
        // Port: the original centred the window on the desktop the first time (or restored its saved placement)
        // and showed it; leaving full screen puts the SDL window back where it was.
        displayReady = 1;

        if (mouseThreadStarted != 0)
        {
            MouseCritSec.unlock();
            InMouseCritSec = 0;
        }
    }
}

auto InitFullScreen() -> void
{
    if (gFullScreen == 0 && application->ddObject != nullptr)
    {
        if (mouseThreadStarted != 0)
        {
            MouseCritSec.lock();
            InMouseCritSec = 1;
        }

        displayReady = 0;
        // Port: the original saved the window's placement and made it a popup (WS_POPUP) first.
        SavedPosition = 1;
        gFullScreen = 1;
        application->resetDirectDraw(gWidth, gHeight, 8);

        if (mouseThreadStarted != 0)
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
    aEvent event;

    if (leftMouseButtonDown == 0)
    {
        if (left != 0)
        {
            event.clear();
            event.rightButton = held(right);
            event.ctrlKey = held(ctrl);
            event.shiftKey = held(shift);
            event.altKey = held(alt);
            event.type = 1;
            event.leftButton = 0xff;
            leftMouseButtonDown = -1;
            event.x = mouseScreenX;
            event.y = mouseScreenY;
            handleEvent(&event);
        }
    }
    else if ((left & 0x8000) == 0)
    {
        event.clear();
        event.rightButton = held(right);
        event.ctrlKey = held(ctrl);
        event.shiftKey = held(shift);
        event.altKey = held(alt);
        leftMouseButtonDown = 0;
        event.type = 4;
        event.x = mouseScreenX;
        event.y = mouseScreenY;
        handleEvent(&event);
    }

    bool rightChanged = false;

    if (rightMouseButtonDown == 0)
    {
        if (right != 0)
        {
            event.clear();
            event.leftButton = held(left);
            event.shiftKey = held(shift);
            event.type = 3;
            event.rightButton = 0xff;
            rightMouseButtonDown = -1;
            rightChanged = true;
        }
    }
    else if ((right & 0x8000) == 0)
    {
        event.clear();
        event.leftButton = held(left);
        event.shiftKey = held(shift);
        rightMouseButtonDown = 0;
        event.type = 6;
        rightChanged = true;
    }

    if (rightChanged)
    {
        event.ctrlKey = held(ctrl);
        event.altKey = held(alt);
        event.x = mouseScreenX;
        event.y = mouseScreenY;
        handleEvent(&event);
    }

    if (oldMouseX != mouseScreenX || oldMouseY != mouseScreenY)
    {
        event.clear();
        event.leftButton = held(left);
        event.rightButton = held(right);
        event.type = 7;
        event.altKey = held(alt);
        event.y = mouseScreenY;
        oldMouseY = mouseScreenY;
        event.ctrlKey = held(ctrl);
        event.shiftKey = held(shift);
        event.x = mouseScreenX;
        oldMouseX = mouseScreenX;
        handleEvent(&event);
    }
}

auto GetPaletteFromArt(char* fileName) -> VFX_RGB*
{
    char path[252];
    char message[256];
    File artFileHandle;
    std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

    if (artFileHandle.open(path, READ, 50) != 0)
    {
        MCPort::StrCopy(path, sizeof(path), fileName);

        if (artFileHandle.open(path, READ, 50) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
        }
    }

    const uint32_t size = artFileHandle.fileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
    }

    uint8_t* tga = static_cast<uint8_t*>(systemHeap->malloc(size));

    if (tga == nullptr)
    {
        return nullptr;
    }

    artFileHandle.read(tga, static_cast<int32_t>(size));
    artFileHandle.close();
    VFX_RGB* palette = static_cast<VFX_RGB*>(systemHeap->malloc(sizeof(VFX_RGB) * 256));
    tgaColorMapToPalette(tga, palette);
    systemHeap->free(tga);
    return palette;
}

// aSystem.

auto aSystem::start(void* instance, void* prevInstance, char* commandLine, int showCommand, int16_t screenWidth,
                    int16_t screenHeight) -> int
{
    (void)prevInstance;
    (void)showCommand;
    ddObject = nullptr;
    ddObject2 = nullptr;
    ddPrimarySurface = nullptr;
    ddBackSurface = nullptr;
    ddPalette = nullptr;
    backpbmi = nullptr;
    windowHandle = nullptr;
    numCallbacks = 0;
    numChildren = 0;
    grabbed = nullptr;
    textFocus = nullptr;
    current = nullptr;
    parent = nullptr;
    modal = nullptr;
    thePalette = nullptr;
    gammaLevel = 0;
    smackerWindow = nullptr;
    smackerWindow2 = nullptr;
    openingSmackerWindow = nullptr;
    flipToGDIRequested = 0;
    paletteCycle = -1;
    char title[256];
    char name[256];
    char version[256];
    char release[256];
    cLoadString(thisInstance, 0x284, title, 0xfe);
    cLoadString(thisInstance, 0x283, title, 0xfe);
    cLoadString(thisInstance, 0x280, name, 0xfe);
    cLoadString(thisInstance, 0x281, version, 0xfe);
    cLoadString(thisInstance, 0x282, release, 0xfe);
    // Port: the window is named after the port, not the string table's title (string 0x283).
#ifdef _DEBUG
    std::strcpy(appName, "MechCommander Redux (Debug)");
#else
    std::strcpy(appName, "MechCommander Redux");
#endif
    std::strcpy(WindowTitle, appName);
    offsetX = 0;
    offsetY = 0;

    // Port: the original gave up when another copy was running (a window of class "MCX", brought to the front, or
    // the "MCX" file mapping), registered the window class, and checked for a Pentium with CPUID (Processor 1, 2
    // with MMX, 3 for a 486 without CPUID; 0 refused to run). Every x64 processor has MMX.
    Processor = 2;

    displayWidth = screenWidth;
    displayHeight = screenHeight;
    systemInit();
    const int32_t width = static_cast<int16_t>(displayWidth);
    const int32_t height = static_cast<int16_t>(displayHeight);
    this->screenWidth = width;
    this->screenHeight = height;

    for (aCallback*& callback : callbacks)
    {
        callback = nullptr;
    }

    systemHeap = new (std::nothrow) UserHeap;
    int32_t result = systemHeap->init(systemHeapSize, "SystemHeap");

    if (result != 0)
    {
        GeneralMsg("Unable to initialize system heap");
    }

    systemHeap->unknown2C = -1;
    guiHeap = new (std::nothrow) UserHeap;
    result = guiHeap->init(guiHeapSize, "GuiHeap");

    if (result != 0)
    {
        GeneralMsg("Unable to initialize GUI heap");
    }

    guiHeap->unknown2C = -1;
    ParseCommandLine(commandLine);

    // Port: the window is the display's, made by startupDirectDraw below; the original made it here (a popup in
    // full screen, else a caption window sized to the screen) and gave it the focus. Messages reach WindowProc
    // through the platform layer.
    MCInput::SetWindowProc(
        [](uint32_t message, uint32_t wParam, int32_t lParam) -> int32_t
        { return WindowProc(application != nullptr ? application->windowHandle : nullptr, message, wParam, lParam); });

    blackFont = loadFont("blkfnt.fnt");
    fonts[0][0] = blackFont;
    redFont = loadFont("red.fnt");
    fonts[1][0] = redFont;
    yellowFont = loadFont("yellow.fnt");
    fonts[2][0] = yellowFont;
    greenFont = loadFont("green.fnt");
    fonts[3][0] = greenFont;
    blueFont = loadFont("blue.fnt");
    fonts[4][0] = blueFont;
    greyFont = loadFont("gryfnt.fnt");
    fonts[5][0] = greyFont;
    whiteFont = loadFont("white.fnt");
    fonts[6][0] = whiteFont;
    dimFont = loadFont("dim.fnt");
    fonts[7][0] = dimFont;
    yellowDropFont = loadFont("yelldrp.fnt");
    fonts[8][0] = yellowDropFont;
    blueDropFont = loadFont("bluedrp.fnt");
    fonts[9][0] = blueDropFont;
    medBlackFont = loadFont("blkfnt10.fnt");
    fonts[0][1] = medBlackFont;
    medRedFont = loadFont("red10.fnt");
    fonts[1][1] = medRedFont;
    medYellowFont = loadFont("yellow10.fnt");
    fonts[2][1] = medYellowFont;
    medGreenFont = loadFont("green10.fnt");
    fonts[3][1] = medGreenFont;
    medBlueFont = loadFont("blue10.fnt");
    fonts[4][1] = medBlueFont;
    medGreyFont = loadFont("gryfnt10.fnt");
    fonts[5][1] = medGreyFont;
    medWhiteFont = loadFont("white10.fnt");
    fonts[6][1] = medWhiteFont;
    medDimFont = loadFont("dim10.fnt");
    fonts[7][1] = medDimFont;
    fonts[8][1] = yellowDropFont;
    fonts[9][1] = blueDropFont;
    lgBlackFont = loadFont("blkfnt12.fnt");
    fonts[0][2] = lgBlackFont;
    lgRedFont = loadFont("red12.fnt");
    fonts[1][2] = lgRedFont;
    lgYellowFont = loadFont("yellow12.fnt");
    fonts[2][2] = lgYellowFont;
    lgGreenFont = loadFont("green12.fnt");
    fonts[3][2] = lgGreenFont;
    lgBlueFont = loadFont("blue12.fnt");
    fonts[4][2] = lgBlueFont;
    lgGreyFont = loadFont("gryfnt12.fnt");
    fonts[5][2] = lgGreyFont;
    lgWhiteFont = loadFont("white12.fnt");
    fonts[6][2] = lgWhiteFont;
    lgDimFont = loadFont("dim12.fnt");
    fonts[7][2] = lgDimFont;
    systemFont = greyFont;

    // The engine's line font (Font's constructor, inlined).
    lineFont = new (std::nothrow) Font;
    lineFont->curY = 0;
    lineFont->curX = 0;
    lineFont->color = 0xf;
    lineFont->scale = 2.0f;
    lineFont->unknown14 = 0;
    lineFont->scaled = -1;
    lineFont->fontData = nullptr;
    systemHeap->free(lineFont->fontData);
    lineFont->fontData = nullptr;

    for (uint8_t*& letter : lineFont->letterCache)
    {
        letter = reinterpret_cast<uint8_t*>(intptr_t{-1});
    }

    lineFont->init(const_cast<char*>("font"));

    Palette* palette = new Palette();

    if (palette == nullptr)
    {
        gamePalette = nullptr;
        Fatal(-1, " No RAM for Game palette ");
    }

    palette->init();
    gamePalette = palette;
    const int32_t paletteResult = gamePalette->init(const_cast<char*>("palette"));

    if (paletteResult != 0)
    {
        Fatal(paletteResult, " Unable to initialize game palette ");
    }

    InitAlphaLookup(reinterpret_cast<VFX_RGB*>(gamePalette->rgbData));

    artFile = new PacketFile;
    Assert(artFile != nullptr, 0, "Not enough RAM for artFile (Something's way wrong...)");
    {
        FullPathFileName artFileName;
        artFileName.init(artPath, "art", ".pak");

        if (artFile->open(artFileName, READ, 50) != 0)
        {
            Fatal(0, "Error opening art file");
        }
    }

    startupDirectDraw(width, height, 8);
    windowHandle = gameDisplay.get();
    ghWindow = windowHandle;

    cursorShapes = static_cast<uint8_t**>(systemHeap->malloc(sizeof(uint8_t*) * 128));

    for (int i = 0; i < 128; i++)
    {
        cursorShapes[i] = nullptr;
    }

    FullPathFileName cursorFileName;
    cursorFileName.init(spritePath, "cursors", ".pak");
    PacketFile* cursorFile = new PacketFile;

    if (cursorFile->open(cursorFileName, READ, 50) != 0)
    {
        FullPathFileName cdCursorFileName;
        cdCursorFileName.init(CDspritePath, "cursors", ".pak");

        if (cursorFile->open(cdCursorFileName, READ, 50) != 0)
        {
            Fatal(0, "Cannot find cursors.pak file");
        }
    }

    const int32_t numCursors = cursorFile->getNumPackets();

    if (numCursors > 0x7f)
    {
        Fatal(-1, " Too Many cursor Shapes ");
    }

    for (int32_t i = 0; i < numCursors; i++)
    {
        cursorFile->seekPacket(i);
        cursorShapes[i] = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(cursorFile->getPacketSize())));

        if (cursorShapes[i] == nullptr)
        {
            Fatal(-1, " no RAM for cursors ");
        }

        cursorFile->readPacket(i, cursorShapes[i]);
    }

    cursorFile->close();
    delete cursorFile;

    MCInput::ShowCursor(false);
    cursorShape = -1;
    MouseTimerInit();

    screenPort = new aPort;
    const int32_t portResult = screenPort->init(width, height);

    if (portResult != 0)
    {
        Fatal(portResult, "Unable to create screenPort");
    }

    // The screen port shows the display's buffer, set by aLockScreen.
    screenPort->bitmap()->buffer = nullptr;
    screenWindow = new aObject;
    screenWindow->init(0, 0, this->screenWidth, this->screenHeight, nullptr);
    screenWindow->setDepth(-100);
    screenWindow->objectType = 1;
    gamePalette->activate(0, 0);
    countsPerSecond = MCPort::PerformanceFrequency();
    UpdateDisplay(0, 0, 0, 0, 0);
    aUnlockScreen();

    aTimerManager* timers = new aTimerManager;
    timerManager = timers;
    timers->Init();
    theInterface = new InterfaceObject;

    if (theInterface == nullptr)
    {
        return 2;
    }

    theInterface->init();

    if (userInit() != 0)
    {
        return -10;
    }

    setScrollRect();
    cursorHidden = 0;
    SetCurrentCursor(static_cast<CursorType>(0));
    showCursor(0);
    unknownAE8 = 0xddac0000;
    mouseTrackerCallback = new aCallback;
    mouseTrackerCallback->setExec(CheckMouse);
    application->addCallback(mouseTrackerCallback);
    (void)instance;
    return 0;
}

auto aSystem::stop() -> void
{
    destroyAllFITFiles(saveTempPath);
    // The temp folder is this process's own (temp\<pid>\, see systemInit): it goes with its files.
    MCFileSystem::RemoveDirectory(saveTempPath);
    application->removeCallback(mouseTrackerCallback);

    if (mouseTrackerCallback != nullptr)
    {
        delete mouseTrackerCallback;
    }

    mouseTrackerCallback = nullptr;

    if (mission != nullptr && mission->resultsScreen != nullptr)
    {
        mission->resultsScreen->destroy();
        delete mission->resultsScreen;
        mission->resultsScreen = nullptr;
    }

    userDestroy();

    if (MPlayer != nullptr)
    {
        delete MPlayer;
        MPlayer = nullptr;
    }

    MouseTimerKill();
    SoundRendererUninstall();
    MCInput::ShowCursor(true);
    shutdownDirectDraw();
    MCInput::ClipCursor(nullptr);
    application->setCurrentObject(nullptr);

    if (startupPakFile != nullptr)
    {
        delete[] startupPakFile;
    }

    if (theInterface != nullptr)
    {
        // Destroyed and freed without its destructor, as the original.
        theInterface->destroy();
        guiHeap->free(theInterface);
        theInterface = nullptr;
    }

    if (mainHolder != nullptr)
    {
        mainHolder->destroy();
        delete mainHolder;
        mainHolder = nullptr;
    }

    if (stopWindow1 != nullptr)
    {
        stopWindow1->destroy();
        delete stopWindow1;
        stopWindow1 = nullptr;
    }

    if (stopWindow2 != nullptr)
    {
        stopWindow2->destroy();
        delete stopWindow2;
        stopWindow2 = nullptr;
    }

    if (screenWindow != nullptr)
    {
        screenWindow->destroy();
        delete screenWindow;
        screenWindow = nullptr;
    }

    if (gamePalette != nullptr)
    {
        gamePalette->destroy();
        delete gamePalette;
        gamePalette = nullptr;
    }

    // Port: the GDI palette and back bitmap (thePalette, backbm) never exist.
    thePalette = nullptr;
    backbm = nullptr;

    if (backpbmi != nullptr)
    {
        guiHeap->free(backpbmi);
        backpbmi = nullptr;
    }

    if (artFile != nullptr)
    {
        artFile->close();
        delete artFile;
        artFile = nullptr;
    }

    deleteFont(blackFont);
    deleteFont(greyFont);

    if (greyFont == nullptr)
    {
        systemFont = nullptr;
    }

    deleteFont(whiteFont);
    deleteFont(redFont);
    deleteFont(greenFont);
    deleteFont(blueFont);
    deleteFont(yellowFont);
    deleteFont(dimFont);
    deleteFont(yellowDropFont);
    deleteFont(blueDropFont);
    deleteFont(medBlackFont);
    deleteFont(medGreyFont);
    deleteFont(medWhiteFont);
    deleteFont(medRedFont);
    deleteFont(medGreenFont);
    deleteFont(medBlueFont);
    deleteFont(medYellowFont);
    deleteFont(medDimFont);
    deleteFont(lgBlackFont);
    deleteFont(lgGreyFont);
    deleteFont(lgWhiteFont);
    deleteFont(lgRedFont);
    deleteFont(lgGreenFont);
    deleteFont(lgBlueFont);
    deleteFont(lgYellowFont);
    deleteFont(lgDimFont);

    if (lineFont != nullptr)
    {
        // Font's destroy and destructor, inlined: frees the font data and forgets the cached letters.
        systemHeap->free(lineFont->fontData);
        lineFont->fontData = nullptr;

        for (uint8_t*& letter : lineFont->letterCache)
        {
            letter = reinterpret_cast<uint8_t*>(intptr_t{-1});
        }

        delete lineFont;
        lineFont = nullptr;
    }

    if (timerManager != nullptr)
    {
        // Destroyed and freed without its destructor, as the original.
        timerManager->destroy();
        guiHeap->free(timerManager);
        timerManager = nullptr;
    }

    if (guiHeap != nullptr)
    {
        guiHeap->destroy();
        delete guiHeap;
        guiHeap = nullptr;
    }

    if (systemHeap != nullptr)
    {
        systemHeap->destroy();
        delete systemHeap;
        systemHeap = nullptr;
    }

    if (LZPacketBuffer != nullptr)
    {
        std::free(LZPacketBuffer);
        LZPacketBuffer = nullptr;
    }

    // Port: the original repainted the desktop (InvalidateRect of every window).
}

auto aSystem::startSmackerMovie(char* fileName, uint32_t flags, aObject* window, int exclusive) -> int32_t
{
    (void)flags;
    SmackTag* movie = SmackOpen(fileName, 0xfe000, -1);

    if (movie == nullptr)
    {
        return static_cast<int32_t>(0xddddd002);
    }

    if (window == nullptr)
    {
        // A window of the movie's size, centred on the screen.
        const int32_t movieWidth = movie->Player->Width();
        const int32_t movieHeight = movie->Player->Height();
        const int32_t screenW = application->width();
        const int32_t screenH = application->height();
        aSmackerWindow* movieWindow = new aSmackerWindow;

        if (movieWindow == nullptr)
        {
            return 3;
        }

        window = movieWindow;
        const int32_t result = window->init(static_cast<int32_t>(static_cast<uint32_t>(screenW - movieWidth) >> 1),
                                            static_cast<int32_t>(static_cast<uint32_t>(screenH - movieHeight) >> 1),
                                            movieWidth, movieHeight, const_cast<char*>("Movie Time"));

        if (result != 0)
        {
            return result;
        }
    }

    smackWindowPointer = window;
    const int32_t result = static_cast<aSmackerWindow*>(window)->startSmackerMovie(movie, exclusive);

    if (result != 0)
    {
        return result;
    }

    if (exclusive != 0)
    {
        smackerWindow = window;
    }

    window->setDepth(100);
    screenWindow->addChild(window);
    window->draw();
    return 0;
}

auto aSystem::run() -> void
{
    for (int32_t i = 0; i < screenWindow->numberOfChildren(); i++)
    {
        screenWindow->child(i)->draw();
    }

    UpdateDisplay(0, 0, 0, 0, 0);
    int quit = 0;

    do
    {
        startTime = MCPort::PerformanceCounter();

        // Port: the PeekMessage / TranslateMessage / DispatchMessage loop, which stopped at WM_QUIT.
        if (!MCInput::PumpMessages())
        {
            quit = -1;
        }

        if (applicationActive != 0)
        {
            if (smackerWindow2 == nullptr && smackerWindow == nullptr)
            {
                const int32_t count = numCallbacks;

                for (int32_t i = 0; i < count; i++)
                {
                    if (callbacks[i] != nullptr)
                    {
                        callbacks[i]->execute();
                    }
                }
            }

            int32_t staticNoise = 0;
            int32_t noiseChance = 0;

            if (scenario != nullptr)
            {
                staticNoise = scenario->startingUp;
                noiseChance = scenario->startUpCountdown;
            }

            UpdateDisplay(takeScreenShot, staticNoise, noiseChance, 0, 0);
            takeScreenShot = 0;
        }
        else if (quit == 0)
        {
            MCInput::WaitMessage(-1);
        }
        else
        {
            // Quitting while inactive: close the movies and the feature screen.
            if (application->smackerWindow2 != nullptr)
            {
                closeMovieWindow(application->smackerWindow2);
            }
            else if (application->smackerWindow != nullptr)
            {
                closeMovieWindow(application->smackerWindow);
            }

            if (featureScreen != nullptr)
            {
                screenWindow->removeChild(featureScreen);
                delete featureScreen;
                featureScreen = nullptr;
            }
        }

        stopTime = MCPort::PerformanceCounter();

        // The frame's length. The counters are split in 32-bit halves and the low halves' difference taken as a
        // signed 32-bit number, so a carry into the high half counts 2^32 too many (OB-062): that frame reads as
        // longer than 0.25 s and is clamped.
        const int32_t highDifference = static_cast<int32_t>(static_cast<uint64_t>(stopTime) >> 32) -
                                       static_cast<int32_t>(static_cast<uint64_t>(startTime) >> 32);
        const int32_t lowDifference =
            static_cast<int32_t>(static_cast<uint32_t>(stopTime) - static_cast<uint32_t>(startTime));
        prevStart = startTime;
        double elapsed = static_cast<double>(highDifference) * 4294967296.0 + static_cast<double>(lowDifference);

        if (elapsed == 0.0)
        {
            elapsed = 9.999999747378752e-05;
        }

        const double countsLow = static_cast<double>(static_cast<uint32_t>(countsPerSecond));
        frameLength = static_cast<float>(elapsed / countsLow);
        frameRate = static_cast<float>(
            (static_cast<double>(static_cast<int32_t>(static_cast<uint64_t>(countsPerSecond) >> 32)) * 4294967296.0 +
             countsLow) /
            elapsed);

        if (frameLength > 0.25f)
        {
            frameLength = 0.25f;
        }

        if (frameRate < 4.0f)
        {
            frameRate = 4.0f;
        }

        if (lockFrameRate != 0)
        {
            const int32_t wait = static_cast<int32_t>(67.0f - frameLength * 1000.0f);

            if (wait > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
                frameLength = 0.06666667f;
                frameRate = 15.0f;
            }
        }
    } while (quit == 0);
}

auto aSystem::width() -> int32_t
{
    return screenWidth;
}

auto aSystem::height() -> int32_t
{
    return screenHeight;
}

auto aSystem::screenOffsetX() -> int32_t
{
    return offsetX;
}

auto aSystem::screenOffsetY() -> int32_t
{
    return offsetY;
}

auto aSystem::window() -> void*
{
    return windowHandle;
}

auto aSystem::setScreenOffsetX(int32_t offset) -> void
{
    offsetX = offset;
}

auto aSystem::setScreenOffsetY(int32_t offset) -> void
{
    offsetY = offset;
}

auto aSystem::addCallback(aCallback* callback) -> int32_t
{
    if (numCallbacks == 0x62)
    {
        return 3;
    }

    if (callback == nullptr)
    {
        return 2;
    }

    callbacks[numCallbacks] = callback;
    numCallbacks++;
    return 0;
}

auto aSystem::removeCallback(aCallback* callback) -> int32_t
{
    if (callback == nullptr)
    {
        return 2;
    }

    for (int32_t i = 0; i < numCallbacks; i++)
    {
        if (callbacks[i] == callback)
        {
            for (; i < numCallbacks - 1; i++)
            {
                callbacks[i] = callbacks[i + 1];
            }

            numCallbacks--;
            callbacks[numCallbacks] = nullptr;
            return 0;
        }
    }

    return 1;
}

auto aSystem::setModalObject(aObject* obj) -> void
{
    modal = obj;
    obj->bringToFront(0);
}

auto aSystem::clearModal() -> void
{
    modal = nullptr;
}

auto aSystem::grab(aObject* obj) -> void
{
    Assert(recordClicks == 0 || Terrain::terrainTacticalMap == nullptr ||
               obj != Terrain::terrainTacticalMap->scrollButtons[5],
           0, " Get Jon! Or save this for him! ");
    grabbed = obj;
    MCInput::SetCapture();
}

auto aSystem::setText(aObject* obj) -> void
{
    if (textFocus != nullptr)
    {
        releaseText();
    }

    textFocus = obj;

    if (obj != nullptr)
    {
        aEvent event;
        event.clear();
        event.type = 0x1e;
        event.target = obj;
        event.data = 7;
        obj->handleEvent(&event);
    }
}

auto aSystem::setCurrentObject(aObject* obj) -> void
{
    current = obj;
}

auto aSystem::release() -> void
{
    grabbed = nullptr;
    MCInput::ReleaseCapture();
}

auto aSystem::releaseText() -> void
{
    aObject* obj = textFocus;

    if (obj != nullptr)
    {
        aEvent event;
        event.clear();
        event.type = 0x1e;
        event.data = 8;
        event.target = obj;
        obj->handleEvent(&event);
        textFocus = nullptr;
    }
}

auto aSystem::grabbedObject() -> aObject*
{
    return grabbed;
}

auto aSystem::textObject() -> aObject*
{
    return textFocus;
}

auto aSystem::currentObject() -> aObject*
{
    return current;
}

auto aSystem::tweakDDPalette(int first, int count, VFX_RGB* colors, int sixBit) -> int
{
    if (displayReady == 0)
    {
        return 0;
    }

    if (smackerWindow == nullptr)
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
        VFX_RGB color = *colors;

        if (sixBit != 0)
        {
            color.r = static_cast<uint8_t>(color.r << 2);
            color.g = static_cast<uint8_t>(color.g << 2);
            color.b = static_cast<uint8_t>(color.b << 2);
        }

        currentPalette[i] = color;
        logicalPalette[i] = color;
    }

    if (gammaLevel != 0)
    {
        for (int i = first; i < end; i++)
        {
            logicalPalette[i].r = GammaColorTranslation[currentPalette[i].r];
            logicalPalette[i].g = GammaColorTranslation[currentPalette[i].g];
            logicalPalette[i].b = GammaColorTranslation[currentPalette[i].b];
        }

        if (gammaLevel == 2)
        {
            for (int i = first; i < end; i++)
            {
                logicalPalette[i].r = GammaColorTranslation[logicalPalette[i].r];
                logicalPalette[i].g = GammaColorTranslation[logicalPalette[i].g];
                logicalPalette[i].b = GammaColorTranslation[logicalPalette[i].b];
            }
        }
    }

    showPalette(first, count);
    // The original returned AnimatePalette's -1 in a window, and whether SetEntries succeeded in full screen.
    return gFullScreen != 0 ? 1 : -1;
}

auto aSystem::gammaCorrectCurrentPalette() -> void
{
    if (displayReady != 0)
    {
        gammaLevel++;

        if (gammaLevel > 3)
        {
            gammaLevel = 0;
        }

        gammaCorrectCurrentPalette(gammaLevel);
    }
}

auto aSystem::gammaCorrectCurrentPalette(int32_t level) -> void
{
    if (displayReady == 0)
    {
        return;
    }

    gammaLevel = level;

    // Entries 10..245, through the gamma table once per level.
    for (int i = 10; i < 0xf6; i++)
    {
        logicalPalette[i] = currentPalette[i];
    }

    for (int32_t pass = 0; pass < level && pass < 3; pass++)
    {
        for (int i = 10; i < 0xf6; i++)
        {
            logicalPalette[i].r = GammaColorTranslation[logicalPalette[i].r];
            logicalPalette[i].g = GammaColorTranslation[logicalPalette[i].g];
            logicalPalette[i].b = GammaColorTranslation[logicalPalette[i].b];
        }
    }

    showPalette(10, 0xec);
}

auto aSystem::fadeDownCurrentPalette() -> void
{
    if (displayReady == 0 || currentPalette[10].r == 0)
    {
        return;
    }

    // Darkens entries 10..245 by a step that follows the time each step took (256 levels a second), waiting at
    // least 1/256 s a step, until 256 levels are gone.
    int32_t step = 1;
    int32_t faded = 0;
    const double frequency = static_cast<double>(countsPerSecond);

    do
    {
        const int64_t stepStart = MCPort::PerformanceCounter();

        for (int i = 10; i < 0xf6; i++)
        {
            VFX_RGB& color = currentPalette[i];
            color.r = step < color.r ? static_cast<uint8_t>(color.r - step) : 0;
            color.g = step < color.g ? static_cast<uint8_t>(color.g - step) : 0;
            color.b = step < color.b ? static_cast<uint8_t>(color.b - step) : 0;
            logicalPalette[i] = color;
        }

        showPalette(10, 0xec);

        // Port fix: the palette only shows when presented (the original's SetEntries changed the screen at once).
        if (MCDisplay* display = MCInput::Display())
        {
            (void)display->Present();
        }

        faded += step;
        float elapsed;

        do
        {
            elapsed = static_cast<float>(static_cast<double>(MCPort::PerformanceCounter() - stepStart) / frequency);
        } while (elapsed < 0.00390625f);

        step = static_cast<int32_t>(elapsed * 256.0f);
    } while (faded < 0x100);
}

auto aSystem::activatePalette(uint8_t* colors, int first, int count) -> void
{
    if (first != 0)
    {
        tweakDDPalette(first, count, reinterpret_cast<VFX_RGB*>(colors + first * 3), -1);
        return;
    }

    // From entry 0 the palette is only remembered.
    if (paletteRgb == nullptr)
    {
        paletteRgb = static_cast<VFX_RGB*>(guiHeap->malloc(0x300));
    }

    globalEntries = count;
    globalFirst = 0;
    std::memcpy(paletteRgb, colors, static_cast<size_t>(count * 3));
}

auto aSystem::activatePaletteFromTGA(char* fileName) -> void
{
    char path[252];
    char message[256];
    File tgaFile;
    std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

    if (tgaFile.open(path, READ, 50) != 0)
    {
        MCPort::StrCopy(path, sizeof(path), fileName);

        if (tgaFile.open(path, READ, 50) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
        }
    }

    const uint32_t size = tgaFile.fileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
    }

    uint8_t* tga = static_cast<uint8_t*>(guiHeap->malloc(size));

    if (tga == nullptr)
    {
        return;
    }

    tgaFile.read(tga, static_cast<int32_t>(size));
    tgaFile.close();
    VFX_RGB* palette = static_cast<VFX_RGB*>(guiHeap->malloc(sizeof(VFX_RGB) * 256));
    tgaColorMapToPalette(tga, palette);
    guiHeap->free(tga);
    activatePalette(reinterpret_cast<uint8_t*>(palette), 0, 0x100);
    InitAlphaLookup(palette);
    guiHeap->free(palette);
}

auto aSystem::activatePaletteFromGIF(char* fileName) -> void
{
    // Reads the GIF's palette and does nothing with it.
    char path[128];
    File gifFile;
    std::snprintf(path, sizeof(path), "%s%s", palettePath, fileName);

    if (fileExists(path) == 0)
    {
        std::strncpy(path, fileName, 0x7f);
        Assert(fileExists(path), 0, "Unable to find palette .gif");
    }

    gifFile.open(path, READ, 50);
    const uint32_t size = gifFile.fileSize();
    Assert(size != 0, 0, "Error reading from palette gif");
    uint8_t* gif = static_cast<uint8_t*>(guiHeap->malloc(size));
    Assert(gif != nullptr, 0, "Error allocating memory for palette .gif");
    gifFile.read(gif, static_cast<int32_t>(size));
    gifFile.close();
    VFX_RGB colors[256];
    VFX_GIF_palette(gif, colors);
    guiHeap->free(gif);
}

auto aSystem::activateSmackerPalette(uint8_t* colors) -> void
{
    tweakDDPalette(0, 0x100, reinterpret_cast<VFX_RGB*>(colors), 0);
}

auto aSystem::AddTimer(aObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                       int useScenarioTime) -> int32_t
{
    lockMouse();
    const int32_t result =
        timerManager->AddTimer(target, id, static_cast<uint32_t>(interval), eventType, eventData, useScenarioTime);
    unlockMouse();
    return result;
}

auto aSystem::AddUniqueTimer(aObject* target, int16_t id, int32_t interval, int32_t eventType, int32_t eventData,
                             int useScenarioTime) -> int32_t
{
    lockMouse();
    const int32_t result = timerManager->AddUniqueTimer(target, id, static_cast<uint32_t>(interval), eventType,
                                                        eventData, useScenarioTime);
    unlockMouse();
    return result;
}

auto aSystem::RemoveTimer(aObject* target, int16_t id) -> void
{
    aTimerManager* manager = timerManager;

    if (manager != nullptr)
    {
        lockMouse();
        manager->RemoveTimer(target, id);
        unlockMouse();
    }
}

auto aSystem::RemoveTimers(aObject* target) -> void
{
    aTimerManager* manager = timerManager;

    if (manager != nullptr)
    {
        lockMouse();
        manager->RemoveTimers(target);
        unlockMouse();
    }
}

auto aSystem::SetCurrentCursor(CursorType cursor) -> void
{
    if (cursorHidden != 0)
    {
        return;
    }

    AG_mouseFrame = 0;
    currentCursor = cursor;
    cursorShape = static_cast<int32_t>(cursor);

    // 0xf..0x11 are offset by the interface's cursor set; 0x12 is shape 1.
    switch (static_cast<int32_t>(cursor))
    {
        case 0xf:
        {
            if (theInterface != nullptr)
            {
                cursorShape = theInterface->cursorOffset + 0xf;
            }
            break;
        }
        case 0x10:
        {
            if (theInterface != nullptr)
            {
                cursorShape = theInterface->cursorOffset + 0x2f;
            }
            break;
        }
        case 0x11:
        {
            if (theInterface != nullptr)
            {
                cursorShape = theInterface->cursorOffset + 0x4f;
            }
            break;
        }
        case 0x12:
            cursorShape = 1;
            break;
    }
}

auto aSystem::showCursor(int show) -> void
{
    if (show != 0)
    {
        cursorHidden = 0;
        SetCurrentCursor(static_cast<CursorType>(0));
        return;
    }

    cursorShape = -1;
    cursorHidden = -1;
}

// aCallback.

auto aCallback::operator new(size_t size) noexcept -> void*
{
    return guiHeap->malloc(static_cast<uint32_t>(size));
}

auto aCallback::operator delete(void* ptr) -> void
{
    guiHeap->free(ptr);
}

aCallback::aCallback()
{
    destroy();
}

namespace
{
    /// <summary>
    /// Port: the callback whose function is running (<see cref="aCallback::execute"/>), cleared when it is deleted.
    /// </summary>
    aCallback* executingCallback = nullptr;
}

aCallback::~aCallback()
{
    if (this == executingCallback)
    {
        executingCallback = nullptr;
    }

    destroy();
}

auto aCallback::destroy() -> void
{
    exec = nullptr;
    message = 0;
    object = nullptr;
}

auto aCallback::execute() -> void
{
    if (exec != nullptr)
    {
        // Port fix: a function can delete its own callback (DancingButtons deletes moveCallback). The original then
        // read message and object from the freed block, which destroy() had zeroed, so it posted nothing. Return
        // instead of reading freed memory (OB-109).
        aCallback* outerCallback = executingCallback;
        executingCallback = this;
        exec();
        const bool deleted = executingCallback != this;
        executingCallback = outerCallback;

        if (deleted)
        {
            return;
        }
    }

    if (message != 0 && object != nullptr)
    {
        aPostMessage(object, message);
    }
}

auto aCallback::setExec(void (*func)()) -> void
{
    exec = func;
}

auto aCallback::setMessage(aObject* obj, int32_t msg) -> void
{
    message = msg;
    object = obj;
}

auto aEvent::clear() -> void
{
    // data (+0x1c) and lParam (+0x20) are left as they were.
    type = 0;
    target = nullptr;
    leftButton = 0;
    middleButton = 0;
    rightButton = 0;
    altKey = 0;
    ctrlKey = 0;
    shiftKey = 0;
    key = 0;
    scanCode = 0;
    x = 0;
    y = 0;
    unknown24 = 0;
}

auto TimerCallback() -> void
{
    const uint32_t now = MCPort::Milliseconds();
    int32_t count = application->timerManager->numTimers;

    for (int32_t i = 0; i < application->timerManager->numTimers; i++)
    {
        aTimerManager* manager = application->timerManager;
        aTimer* timer = manager->GetTimer(static_cast<int16_t>(i));

        if (timer == nullptr)
        {
            continue;
        }

        const uint32_t due = timer->lastTime + timer->interval;

        if (timer->useScenarioTime != 0)
        {
            if (!(static_cast<double>(due) < static_cast<double>(scenarioTime) * 1000.0))
            {
                continue;
            }
        }
        else if (now <= due)
        {
            continue;
        }

        const tagPOINT cursor = GetMessageCursorLoc();
        aEvent event;

        if (timer->eventType != 0)
        {
            // A one-shot event: sent, then the timer goes (unless the handler changed the timer list).
            event.clear();
            event.x = cursor.x;
            event.y = cursor.y;
            event.data = timer->eventData;
            event.type = timer->eventType;
            manager = application->timerManager;
            manager->LockTimersExcept(manager->GetTimer(static_cast<int16_t>(i)));
            timer->target->handleEvent(&event);
            application->timerManager->UnlockTimers();

            if (count == application->timerManager->numTimers)
            {
                lockMouse();
                application->timerManager->RemoveTimer(i);
                unlockMouse();
            }
            else
            {
                count = application->timerManager->numTimers;
            }
        }
        else
        {
            event.clear();
            event.x = cursor.x;
            event.y = cursor.y;
            event.data = timer->id;
            event.type = 0x13;
            manager = application->timerManager;
            manager->LockTimersExcept(manager->GetTimer(static_cast<int16_t>(i)));
            timer->target->handleEvent(&event);
            application->timerManager->UnlockTimers();
            const int32_t numTimers = application->timerManager->numTimers;

            if (count == numTimers)
            {
                timer->lastTime = now;
            }
            else
            {
                // Faithful: when the handler removed a timer other than this one, this one's time is set too.
                if (count - 1 != numTimers)
                {
                    timer->lastTime = now;
                }

                count = numTimers;
            }
        }
    }
}

// aTimerManager.

aTimerManager::aTimerManager()
{
    for (aTimer*& timer : timers)
    {
        timer = nullptr;
    }

    numTimers = 0;
    numTimersToWhack = 0;
    locked = 0;
}

aTimerManager::~aTimerManager()
{
    for (int16_t i = 0; i < numTimers; i++)
    {
        guiHeap->free(timers[i]);
    }
}

auto aTimerManager::operator new(size_t size) noexcept -> void*
{
    return guiHeap->malloc(static_cast<uint32_t>(size));
}

auto aTimerManager::operator delete(void* ptr) -> void
{
    guiHeap->free(ptr);
}

auto aTimerManager::Init() -> int32_t
{
    timerCallback = new aCallback;

    if (timerCallback == nullptr)
    {
        return -1;
    }

    timerCallback->setExec(TimerCallback);
    return 0;
}

auto aTimerManager::destroy() -> void
{
    while (numTimers > 0)
    {
        guiHeap->free(timers[numTimers - 1]);
        timers[numTimers - 1] = nullptr;
        numTimers--;
    }

    aCallback* callback = timerCallback;
    application->removeCallback(callback);

    if (callback != nullptr)
    {
        callback->destroy();
        delete callback;
    }

    timerCallback = nullptr;
}

auto aTimerManager::AddUniqueTimer(aObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                                   int useScenarioTime) -> int32_t
{
    for (int16_t i = 0; i < numTimers; i++)
    {
        const aTimer* timer = timers[i];

        if (timer != nullptr && timer->target == target && timer->id == id && timer->interval == interval &&
            timer->eventType == eventType && timer->eventData == eventData)
        {
            return -1;
        }
    }

    return AddTimer(target, id, interval, eventType, eventData, useScenarioTime);
}

auto aTimerManager::AddTimer(aObject* target, int16_t id, uint32_t interval, int32_t eventType, int32_t eventData,
                             int useScenarioTime) -> int32_t
{
    const int32_t index = numTimers;

    // Port fix: the original accepted a 100th timer (index 99), one past the array, over timersToWhack[0].
    if (index >= 99)
    {
        return -1;
    }

    timers[index] = static_cast<aTimer*>(guiHeap->malloc(sizeof(aTimer)));
    aTimer* timer = timers[numTimers];

    if (timer == nullptr)
    {
        return -1;
    }

    if (index == 0)
    {
        application->addCallback(timerCallback);
    }

    uint32_t startTime;

    if (useScenarioTime == 0)
    {
        startTime = MCPort::Milliseconds();
    }
    else
    {
        startTime = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(scenarioTime) * 1000.0));
    }

    timer->target = target;
    timer->id = id;
    timer->interval = interval;
    timer->lastTime = startTime;
    timer->eventType = eventType;
    timer->eventData = eventData;
    numTimers = index + 1;
    timer->useScenarioTime = useScenarioTime;
    return 0;
}

auto aTimerManager::RemoveTimers(aObject* target) -> void
{
    for (int32_t i = 0; i < numTimers; i++)
    {
        aTimer* timer = timers[i];

        if (target != timer->target)
        {
            continue;
        }

        if (locked != 0 && timer != lockedExcept)
        {
            // Locked: queued for UnlockTimers (once per matching timer).
            timersToWhack[numTimersToWhack].target = target;
            timersToWhack[numTimersToWhack].id = -1;
            numTimersToWhack++;
            continue;
        }

        guiHeap->free(timer);
        numTimers--;

        for (int32_t j = i; j < numTimers; j++)
        {
            timers[j] = timers[j + 1];
        }

        timers[numTimers] = nullptr;
        i--;

        if (numTimers == 0)
        {
            application->removeCallback(timerCallback);
        }
    }
}

auto aTimerManager::RemoveTimer(aObject* target, int16_t id) -> void
{
    const int32_t count = numTimers;
    int32_t index = 0;

    while (index < count && !(timers[index]->id == id && target == timers[index]->target))
    {
        index++;
    }

    if (index >= count)
    {
        return;
    }

    if (locked != 0 && timers[index] != lockedExcept)
    {
        timersToWhack[numTimersToWhack].target = target;
        timersToWhack[numTimersToWhack].id = id;
        numTimersToWhack++;
        return;
    }

    guiHeap->free(timers[index]);
    numTimers = count - 1;

    for (; index < numTimers; index++)
    {
        timers[index] = timers[index + 1];
    }

    timers[numTimers] = nullptr;

    if (numTimers == 0)
    {
        application->removeCallback(timerCallback);
    }
}

auto aTimerManager::RemoveTimer(int32_t index) -> void
{
    if (index >= numTimers)
    {
        return;
    }

    if (locked != 0 && timers[index] != lockedExcept)
    {
        timersToWhack[numTimersToWhack].target = nullptr;
        timersToWhack[numTimersToWhack].id = index;
        numTimersToWhack++;
        return;
    }

    guiHeap->free(timers[index]);
    numTimers--;

    for (; index < numTimers; index++)
    {
        timers[index] = timers[index + 1];
    }

    timers[numTimers] = nullptr;

    if (numTimers == 0)
    {
        application->removeCallback(timerCallback);
    }
}

auto aTimerManager::GetTimer(int16_t index) -> aTimer*
{
    if (index < numTimers)
    {
        return timers[index];
    }

    return nullptr;
}

auto aTimerManager::GetTimer(aObject* target, int16_t id) -> aTimer*
{
    int32_t index = 0;

    while (index < numTimers && !(timers[index]->id == id && target == timers[index]->target))
    {
        index++;
    }

    if (index >= numTimers)
    {
        return nullptr;
    }

    return timers[index];
}

auto aTimerManager::LockTimersExcept(aTimer* running) -> void
{
    locked = -1;
    lockedExcept = running;
}

auto aTimerManager::UnlockTimers() -> void
{
    locked = 0;

    // The queued removals, last first.
    for (int32_t i = numTimersToWhack; i > 0; i--)
    {
        const TimerToWhack& whack = timersToWhack[i - 1];

        if (whack.target != nullptr && whack.id != -1)
        {
            lockMouse();
            RemoveTimer(whack.target, static_cast<int16_t>(whack.id));
            unlockMouse();
        }
        else if (whack.target != nullptr)
        {
            lockMouse();
            RemoveTimers(whack.target);
            unlockMouse();
        }
        else if (whack.id != -1)
        {
            lockMouse();
            RemoveTimer(whack.id);
            unlockMouse();
        }
        else
        {
            Fatal(numTimersToWhack, " Illegal timersToWhack structure!");
        }

        numTimersToWhack--;
    }
}

auto GetMessageCursorLoc() -> tagPOINT
{
    // The message's position is already on the logical screen in the port (MapWindowPoints in the original).
    const MCPoint position = MCInput::GetMessagePos();
    return tagPOINT{position.x, position.y};
}

// aHolderObject.

auto aHolderObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    // aObject::init inlined, without the port: a holder only shows its panes.
    (void)name;
    winWidth = width;
    maxWidth = width;
    normalWidth = width;
    iconWidth = width;
    winHeight = height;
    winX = xPos;
    winY = yPos;
    maxHeight = height;
    maxX = xPos;
    maxY = yPos;
    normalHeight = height;
    normalX = xPos;
    normalY = yPos;
    iconHeight = height;
    iconX = xPos;
    iconY = yPos;
    hideOffset = 0;
    hidden = 0;
    winState = aSTATE_NORMAL;
    showWindow = -1;
    dragOn = 0;
    backgroundColor = 0xff;
    displayPort = nullptr;

    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    framePane = new (std::nothrow) _pane;

    if (framePane == nullptr)
    {
        return 3;
    }

    framePane->window = screenPort->bitmap();
    framePane->x0 = xPos;
    framePane->y0 = yPos;
    framePane->x1 = xPos + width;
    framePane->y1 = yPos + height;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    winDepth = 0;
    windowAnimation = nullptr;
    animating = 0;
    iconAnimation = nullptr;
    objectType = -1;
    vertical = 0;
    activePane = -1;
    panes[0] = nullptr;
    panes[1] = nullptr;
    tiled = 0;
    return 0;
}

auto aHolderObject::destroy() -> void
{
    aObject::destroy();
    panes[0] = nullptr;
    panes[1] = nullptr;
}

auto aHolderObject::removeChild(aObject* oldChild) -> void
{
    if (panes[0] == oldChild)
    {
        panes[0] = nullptr;
    }

    if (panes[1] == oldChild)
    {
        panes[1] = nullptr;
    }

    aObject::removeChild(oldChild);
}

auto aHolderObject::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if ((IsHidden() == 0 || hideOffset != 0) && winState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->display();
        }
    }
}

auto aHolderObject::resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth > 0 && newHeight > 0)
    {
        if (gridAligned != 0)
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

        winWidth = newWidth;
        winHeight = newHeight;
        framePane->x1 = framePane->x0 - 1 + newWidth;
        framePane->y1 = framePane->y0 - 1 + newHeight;
        Retile();
    }
}

auto aHolderObject::Retile() -> void
{
    int32_t paneX = 0;
    int32_t paneY = 0;
    int32_t paneWidth = width();
    int32_t paneHeight = height();

    if (panes[1] != nullptr && tiled != 0)
    {
        if (vertical == 0)
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

    if (tiled == 0)
    {
        index = activePane;
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
        aObject* pane = panes[index];

        if (pane == nullptr)
        {
            return;
        }

        pane->moveTo(paneX, paneY, 0);
        pane->resize(paneWidth, paneHeight);

        if (tiled != 0)
        {
            if (vertical == 0)
            {
                if (height() % 2 != 0)
                {
                    paneHeight++;
                }

                paneY += paneHeight;
            }
            else
            {
                if (width() % 2 != 0)
                {
                    paneWidth++;
                }

                paneX += paneWidth;
            }
        }

        index++;
    } while (index < end);
}

auto aHolderObject::AddPane(aObject* pane) -> void
{
    if (panes[0] == nullptr)
    {
        addChild(pane);
        panes[0] = pane;
        activePane = 0;
        Retile();
        return;
    }

    if (panes[1] == nullptr)
    {
        addChild(pane);
        panes[1] = pane;
    }

    Retile();
}

auto aHolderObject::RemovePane(aObject* pane) -> void
{
    aObject* second = panes[1];

    if (second == pane)
    {
        panes[1] = nullptr;
        removeChild(pane);
        Retile();
        return;
    }

    if (panes[0] == pane)
    {
        // Faithful: with a second pane, it moves to the first slot but stays in the second too.
        if (second == nullptr)
        {
            panes[0] = nullptr;
            activePane = -1;
        }
        else
        {
            panes[0] = second;
        }

        removeChild(pane);
    }

    Retile();
}

auto aHolderObject::SetActivePane(aObject* pane) -> void
{
    if (pane == panes[1])
    {
        activePane = 1;
        return;
    }

    activePane = 0;
}

auto aHolderObject::SetTiled(int newTiled) -> void
{
    tiled = newTiled;

    if (newTiled == 0)
    {
        if (GetInactivePane() != nullptr)
        {
            GetInactivePane()->ShowGUIWindow(0);
        }
    }
    else
    {
        if (panes[0] != nullptr)
        {
            panes[0]->ShowGUIWindow(-1);
        }

        if (panes[1] != nullptr)
        {
            panes[1]->ShowGUIWindow(-1);
        }
    }

    Retile();
}

auto aHolderObject::SetActivePaneNumber(char index) -> void
{
    if (index == 0 || (index == 1 && panes[1] != nullptr))
    {
        activePane = index;
    }

    Retile();
}

// aMessageBox.

auto aMessageBox::init(uint8_t* text) -> int32_t
{
    if (whiteFont == nullptr)
    {
        return -3;
    }

    int32_t boxWidth = whiteFont->width(text) + 0xc;

    if (boxWidth < 0x48)
    {
        boxWidth = 0x48;
    }

    const int32_t fontHeight = whiteFont->height();
    const int32_t screenW = application->width();
    const int32_t screenH = application->height();
    int32_t result = aObject::init((screenW - boxWidth) / 2, (screenH - (fontHeight + 0x28)) / 2, boxWidth,
                                   fontHeight + 0x28, nullptr);

    if (result != 0)
    {
        return result;
    }

    aButton* button = new aButton;
    okButton = button;
    result = button->init((boxWidth - 0x30) / 2, fontHeight + 0xe, 0x3c, 0x14, nullptr);

    if (result != 0)
    {
        return result;
    }

    button->setUpPicture(0x10);
    button->setDownPicture(0x11);
    button->callback()->setExec(DestroyVersion);
    button->setDepth(100);
    addChild(button);
    // The box is drawn by draw, each frame.
    message = reinterpret_cast<const char*>(text);
    return 0;
}

auto aMessageBox::draw() -> void
{
    aPort* boxPort = displayPort;
    VFX_pane_wipe(boxPort->frame(), 0x11);
    auto* text = reinterpret_cast<uint8_t*>(message.data());
    const int32_t textWidth = whiteFont->width(text);
    whiteFont->writeString(boxPort->frame(), (width() - textWidth) / 2, 8, text, -1);
    drawBox(0x1f, -1, -1, -1, -1);
    aObject::draw();
}

auto aMessageBox::destroy() -> void
{
    if (okButton != nullptr)
    {
        okButton->destroy();
        delete okButton;
        okButton = nullptr;
    }

    aObject::destroy();
}

auto aMessageBox::handleEvent(aEvent* event) -> void
{
    if (pointInside(event->x, event->y) != 0)
    {
        okButton->handleEvent(event);
    }
}
