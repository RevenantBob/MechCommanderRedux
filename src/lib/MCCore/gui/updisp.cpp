#include "stdafx.h"
#include "gui/updisp.h"
#include "engine/MCWriteTga.h"
#include "gameos/MCSoundRenderer.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/awindow.h"
#include "gui/mchwcursor.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "logistics/logmain.h"
#include "mission/MCMission.h"
#include "mission/MCMissionResultsScreen.h"
#include "platform/MCAudio.h"
#include "platform/MCDisplay.h"
#include "platform/MCFrameLog.h"
#include "platform/MCInput.h"
#include "platform/MCRenderer.h"
#include "vfx/MCVfxFunctions.h"

int AGOldMouseX = -1;
int AGOldMouseY = -1;
int AGOldMouseXh = -1;
int AGOldMouseYh = -1;
int AGOldMouse = -1;
int AGOldMouseW = 0;
int AGOldMouseH = 0;
int AGMouseBuffer = 0;
int AGLocked = 0;
char MouseBuffer[0x10000];
float FrameRateArray[512];
int FrPointer = 0;
uint32_t HTimer = 0;
int GSoundTimer = 0;
int KeepScreenBlack = 0;
volatile int InTimerThread = 0;
int32_t ShotNum = 0;
char GifName[20];
uint8_t* ConnectShape = nullptr;
uint8_t** CursorShapes = nullptr;
int GWinWidth = 0;
int GWinHeight = 0;
MCWindow TempWindow = {};
MCPane TempPane = {};
uint32_t MouseTicks = 0;
int MouseScreenX = 320;
int MouseScreenY = 240;
int PageFlipping = 0;
uint8_t DisplayFrozen = 0;

namespace
{
    /// <summary>The mouse timer's thread (timeSetEvent's periodic callback).</summary>
    std::jthread MouseTimerThread;

    /// <summary>The screen's window: the 8-bit buffer the game draws and the display shows.</summary>
    MCWindow* ScreenBuffer()
    {
        return ScreenPort->Frame()->Window;
    }

    /// <summary>
    /// Port: the frame counter (<c>gShowFps</c>) in the shown view's top-right corner: frames a second and the mean
    /// frame time over the last half second of real time, on a black box that only grows (so shorter text leaves no
    /// digits behind where nothing else redraws the screen).
    /// </summary>
    void DrawFrameCounter()
    {
        static std::chrono::steady_clock::time_point periodStart = std::chrono::steady_clock::now();
        static int32_t periodFrames = 0;
        static char text[48] = "-- fps";
        static int32_t boxWidth = 0;
        MCDisplay* display = MCInput::Display();
        MCGuiFont* font = MedWhiteFont;

        if (display == nullptr || font == nullptr)
        {
            return;
        }

        periodFrames++;
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - periodStart).count();

        if (seconds >= 0.5)
        {
            std::snprintf(text, sizeof(text), "%.0f fps  %.1f ms", periodFrames / seconds,
                          seconds * 1000.0 / periodFrames);
            periodStart = now;
            periodFrames = 0;
        }

        uint8_t* characters = reinterpret_cast<uint8_t*>(text);
        const int32_t textWidth = font->Width(characters);
        boxWidth = std::max(boxWidth, textWidth + 8);
        const SDL_Rect view = display->View();
        const int32_t right = view.x + view.w - 1;
        const int32_t top = view.y;
        MCWindow* screen = ScreenBuffer();
        MCRenderer::For(screen).Clear(screen, MCRect{right - boxWidth + 1, top, right, top + font->Height() + 3}, 0);
        font->WriteString(ScreenPort->Frame(), right - 3 - textWidth, top + 2, characters, -1);
    }

    /// <summary>
    /// Port: picks the part of the screen the display shows. Only a running scenario uses the whole screen; the
    /// menus, logistics and the results screen are laid out for 640x480 and drawn in the top-left corner of a larger
    /// one, so the display shows just that corner, scaled to the window. A full-screen movie is centred on the
    /// screen, so the view is a 640x480 (or the movie's size, if larger) centred on it.
    /// </summary>
    void UpdateDisplayView(MCDisplay* display)
    {
        constexpr int32_t interfaceWidth = 640;
        constexpr int32_t interfaceHeight = 480;
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = display->Width();
        int32_t height = display->Height();
        MCGuiObject* movie = Application != nullptr ? Application->SmackerWindow : nullptr;

        if (movie != nullptr)
        {
            width = std::max(interfaceWidth, movie->Width());
            height = std::max(interfaceHeight, movie->Height());
            x = movie->GlobalX() + movie->Width() / 2 - width / 2;
            y = movie->GlobalY() + movie->Height() / 2 - height / 2;
        }
        else if (Mission() == nullptr || Mission()->State != MCMissionState::Scenario)
        {
            width = interfaceWidth;
            height = interfaceHeight;
        }

        if (display->SetView(x, y, width, height))
        {
            MCInput::RefreshMouseArea();
        }
    }

    /// <summary>
    /// Shows the screen buffer: the original's flip (DirectDraw Blt/Flip full screen, BitBlt in a window, with the
    /// lost-surface retries and display resets) is the port's present.
    /// </summary>
    void PresentScreen()
    {
        MCDisplay* display = MCInput::Display();

        if (display != nullptr)
        {
            MCFrameLog::Scope present("present");
            UpdateDisplayView(display);
            (void)display->Present();
        }
    }

    /// <summary>Where the cursor is on the screen, kept inside it.</summary>
    void ReadCursorPosition()
    {
        MCPoint cursor = MCInput::GetCursorPos();
        MouseScreenX = cursor.x;
        MouseScreenY = cursor.y;

        if (GWidth <= cursor.x)
        {
            MouseScreenX = GWidth - 1;
        }

        if (GHeight <= cursor.y)
        {
            MouseScreenY = GHeight - 1;
        }
    }

    /// <summary>Saves the screen under the cursor and draws the cursor into the screen buffer.</summary>
    void DrawCursorInBuffer()
    {
        // Port fix: the cursor is the system's, which follows the mouse between frames; the original drew it here.
        if (GSoftwareCursor == 0)
        {
            MCFrameLog::Scope cursor("cursor");
            MCHardwareCursorUpdate();
            return;
        }

        if (Application->CursorShape == -1)
        {
            return;
        }

        SaveMouseBackBuffer();
        AGShapeDraw(ScreenPort->Frame(), CursorShapes[Application->CursorShape], 0, MouseScreenX, MouseScreenY);
    }

    /// <summary>Ends a finished movie window: stops the movie, destroys and deletes the window.</summary>
    void CloseMovieWindow(MCGuiObject*& window)
    {
        static_cast<MCGuiSmackerWindow*>(window)->EndSmackerMovie();
        window->Destroy();
        delete window;
        window = nullptr;
    }

    /// <summary>Copies the cursor rectangle between <see cref="MouseBuffer"/> and a screen of rows
    /// <paramref name="pitch"/> bytes.</summary>
    void CopyMouseRect(uint8_t* screen, int pitch, bool toScreen)
    {
        uint8_t* saved = reinterpret_cast<uint8_t*>(MouseBuffer);
        uint8_t* row = screen + pitch * AGOldMouseY + AGOldMouseX;

        for (int y = 0; y < AGOldMouseH; y++)
        {
            if (toScreen)
            {
                std::memcpy(row, saved, AGOldMouseW);
            }
            else
            {
                std::memcpy(saved, row, AGOldMouseW);
            }

            saved += AGOldMouseW;
            row += pitch;
        }
    }
}

void SaveMouseBackBuffer()
{
    AGMouseBuffer = 1;
    int32_t resolution = VfxShapeResolution(CursorShapes[Application->CursorShape], 0);
    AGOldMouseW = resolution >> 16;
    AGOldMouseH = resolution & 0xffff;
    int32_t minXY = VfxShapeMinxy(CursorShapes[Application->CursorShape], 0);
    AGOldMouse = Application->CursorShape;
    AGOldMouseXh = MouseScreenX;
    AGOldMouseX = (minXY >> 16) + MouseScreenX;
    AGOldMouseY = static_cast<int16_t>(minXY) + MouseScreenY;
    AGOldMouseYh = MouseScreenY;

    if (AGOldMouseX < 0)
    {
        AGOldMouseW += AGOldMouseX;
        AGOldMouseX = 0;
    }

    if (AGOldMouseY < 0)
    {
        AGOldMouseH += AGOldMouseY;
        AGOldMouseY = 0;
    }

    if (GWidth < AGOldMouseX + AGOldMouseW)
    {
        AGOldMouseW = GWidth - AGOldMouseX;
    }

    if (GHeight < AGOldMouseY + AGOldMouseH)
    {
        AGOldMouseH = GHeight - AGOldMouseY;
    }

    int height = AGOldMouseH;

    if (AGOldMouseW < 0)
    {
        AGOldMouseW = 0;
    }

    if (AGOldMouseH < 0)
    {
        AGOldMouseH = 0;
    }

    if (height > 0)
    {
        CopyMouseRect(ScreenBuffer()->Buffer, GWidth, false);
    }
}

void BlankScreen()
{
    ALockScreen();
    VfxPaneWipe(ScreenPort->Frame(), 0);
    AUnlockScreen();
    ReadCursorPosition();
    DrawCursorInBuffer();
    PresentScreen();
}

int32_t UpdateDisplay(int screenShot, int staticNoise, int32_t noiseChance, int showProgress, int32_t progress)
{
    if (DisplayFrozen != 0)
    {
        return 0;
    }

    if (Application->SmackerWindow2 != nullptr)
    {
        Application->SmackerWindow2->Display();

        if (MovieOver != 0)
        {
            CloseMovieWindow(Application->SmackerWindow2);
        }

        return 0;
    }

    if (InMouseCritSec != 0)
    {
        return 0;
    }

    if (PaletteRgb != nullptr)
    {
        Application->FadeDownCurrentPalette();
    }

    std::lock_guard<std::recursive_mutex> lock(MouseCritSec);
    InMouseCritSec = 1;
    // Port: dragged objects draw into the cursor while the frame is drawn (MCHardwareCursorCarry).
    MCHardwareCursorNewFrame();
    // Port: a new frame's op tables (translucent UI over the world view; see MCUnderlay).
    MCRenderer::ResetOpTables();

    if (Application->SmackerWindow == nullptr)
    {
        if (GlobalLogPtr != nullptr && ScreenWindow->Frame()->Window->Buffer != nullptr)
        {
            VfxPaneWipe(ScreenWindow->Frame(), 0);
        }

        MCFrameLog::Scope draw("draw");

        for (int32_t i = 0; i < ScreenWindow->NumberOfChildren(); i++)
        {
            ScreenWindow->Child(i)->Display();
        }
    }
    else
    {
        // Full screen with page flipping, the original first copied the front page to the back one.
        ALockScreen();
        Application->SmackerWindow->Display();
        AUnlockScreen();
    }

    AGMouseBuffer = 0;
    ALockScreen();

    if (screenShot != 0)
    {
        ShotNum++;
        std::snprintf(GifName, sizeof(GifName), "scrn%04d.tga", ShotNum);
        // Port: the screen as shown (the world view under the key, read back when the GPU draws the frame), not its
        // memory.
        MCDisplay* display = MCInput::Display();

        if (display != nullptr)
        {
            std::vector<uint8_t> shown = display->ComposeScreen();
            WriteTga8Bit(GifName, shown, static_cast<uint32_t>(display->Width()),
                         static_cast<uint32_t>(display->Height()));
        }
    }

    if (staticNoise != 0 && noiseChance != 0)
    {
        // Port: the original wrote the noise into the screen's memory; the row goes to the renderer.
        std::vector<uint8_t> line(static_cast<size_t>(Application->Width() >> 1) * 2);

        for (int32_t row = 0; row < Application->Height(); row++)
        {
            if (RollDice(noiseChance) != 0)
            {
                MCWindow* screen = ScreenBuffer();
                uint8_t* pixel = line.data();

                for (int32_t i = 0; i < Application->Width() >> 1; i++)
                {
                    int32_t noise = MCPort::Rand();
                    pixel[0] = static_cast<uint8_t>(noise & 0x1f);
                    pixel[1] = static_cast<uint8_t>((noise >> 5) & 0x1f);
                    pixel += 2;
                }

                MCRenderer::For(screen).Write(screen, 0, row, line.data(), static_cast<int32_t>(line.size()));
            }
        }
    }

    if (ConnectShape != nullptr && showProgress != 0)
    {
        int32_t centerY = Application->Height() >> 1;
        int32_t centerX = Application->Width() >> 1;
        AGShapeDraw(ScreenPort->Frame(), ConnectShape, 0, centerX, centerY);
        int32_t left = centerX - 0x5f;
        int32_t right = static_cast<int32_t>(progress * 0.01 * 188.0 + left);
        MCScreenVertex bar[4] = {};
        bar[0].X = left;
        bar[0].Y = centerY + 3;
        bar[1].X = right;
        bar[1].Y = centerY + 3;
        bar[2].X = right;
        bar[2].Y = centerY + 10;
        bar[3].X = left;
        bar[3].Y = centerY + 10;

        for (MCScreenVertex& vertex : bar)
        {
            vertex.C = 0xe40000;
        }

        VfxFlatPolygon(ScreenPort->Frame(), std::span(bar, 4));
    }

    FrameRateArray[FrPointer] = FrameRate > 60.0f ? 60.0f : FrameRate;
    FrPointer = (FrPointer + 1) & 0x1ff;

    if (AndyFramerate != 0)
    {
        // The frame graph: 512 frames, 3 pixels per frame a second, 60 at the top.
        WhiteFont->WriteString(ScreenWindow->Frame(), 0x20, 7, reinterpret_cast<uint8_t*>(const_cast<char*>("60+ f/s")),
                               -1);
        WhiteFont->WriteString(ScreenWindow->Frame(), 0x20, 0xbb,
                               reinterpret_cast<uint8_t*>(const_cast<char*>("0   f/s")), -1);
        // Port: the original set the two lines in the screen's memory.
        MCWindow* screen = ScreenBuffer();
        MCRenderer::For(screen).Clear(screen, MCRect{0x40, 9, 0x40 + 0x1ff, 9}, 0xff);
        MCRenderer::For(screen).Clear(screen, MCRect{0x40, 0xbf, 0x40 + 0x1ff, 0xbf}, 0xff);

        for (int32_t y = 0xaf; y > 10; y -= 0xf)
        {
            AGPixelWrite(ScreenPort->Frame(), 0x240, y, 0xff);
        }

        int32_t frame = FrPointer;

        for (int32_t x = 0; x < 0x200; x++)
        {
            int32_t height = static_cast<int32_t>(FrameRateArray[frame] * 3.0f);
            AGPixelWrite(ScreenPort->Frame(), x + 0x40, 0xbe - height, 0xf9);
            frame = (frame + 1) & 0x1ff;
        }
    }

    if (KeepScreenBlack != 0)
    {
        VfxPaneWipe(ScreenPort->Frame(), 0);
    }

    if (GShowFps != 0)
    {
        DrawFrameCounter();
    }

    AUnlockScreen();

    if (Application->SmackerWindow2 != nullptr && MovieOver != 0)
    {
        CloseMovieWindow(Application->SmackerWindow2);
    }

    if (Application->SmackerWindow != nullptr)
    {
        if (MovieOver == 0)
        {
            Application->SmackerWindow->CheckSmackerPalette();
            PresentScreen();
        }
        else
        {
            CloseMovieWindow(Application->SmackerWindow);
        }
    }

    ReadCursorPosition();
    DrawCursorInBuffer();
    PresentScreen();

    if (PaletteRgb != nullptr)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(17));

        if (GBitDepth != 16)
        {
            Application->TweakDDPalette(GlobalFirst, GlobalEntries, PaletteRgb, 1);
        }

        GlobalFirst = 0;
        GlobalEntries = 0;
        delete[] PaletteRgb;
        PaletteRgb = nullptr;
    }

    InMouseCritSec = 0;
    return 0;
}

void MouseTimerInit()
{
    // timeGetDevCaps / timeBeginPeriod ("No Mouse Timer available") have no counterpart.
    InMouseCritSec = 0;
    uint32_t period = 1000 / ResultsStepTicks;
    MouseTimerThread = std::jthread(
        [period](std::stop_token stop)
        {
            auto next = std::chrono::steady_clock::now();

            while (!stop.stop_requested())
            {
                next += std::chrono::milliseconds(period);
                std::this_thread::sleep_until(next);

                if (stop.stop_requested())
                {
                    break;
                }

                MouseTimer(1, 0, 0, 0, 0);
            }
        });
    HTimer = 1;

    for (float& rate : FrameRateArray)
    {
        rate = 0.0f;
    }

    MouseThreadStarted = 1;
}

void MouseTimerKill()
{
    MouseThreadStarted = 0;

    if (HTimer == 0)
    {
        return;
    }

    if (InMouseCritSec != 0)
    {
        // Killed from inside UpdateDisplay (a Fatal while drawing): the original left the lock it held.
        MouseCritSec.unlock();
        InMouseCritSec = 0;
    }
    while (InTimerThread != 0)
    {
        std::this_thread::yield();
    }

    MouseTimerThread.request_stop();

    if (MouseTimerThread.joinable() && MouseTimerThread.get_id() != std::this_thread::get_id())
    {
        MouseTimerThread.join();
    }
    while (InTimerThread != 0)
    {
        std::this_thread::yield();
    }

    HTimer = 0;
}

void MouseTimer(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2)
{
    InTimerThread = 1;
    {
        std::lock_guard<std::recursive_mutex> lock(MouseCritSec);
        bool unlockScreen = true;

        if (Application->SmackerWindow2 == nullptr)
        {
            if (GFullScreen == 0 || PageFlipping == 0 || GBitDepth != 8)
            {
                unlockScreen = false;
            }
            else if (Application->CursorShape == -1)
            {
                if (AGMouseBuffer != 0)
                {
                    EraseMouse();
                }
            }
            else
            {
                MCPoint cursor = MCInput::GetCursorPos();
                MouseScreenX = cursor.x;
                MouseScreenY = cursor.y;
                bool moved = true;

                if (AGMouseBuffer != 0)
                {
                    if (Application->CursorShape == AGOldMouse && cursor.x == AGOldMouseXh && cursor.y == AGOldMouseYh)
                    {
                        moved = false;
                    }
                    else
                    {
                        EraseMouse();
                    }
                }

                if (moved)
                {
                    DrawMouse();
                }
            }
        }

        if (unlockScreen)
        {
            UnLockScreen();
        }

        MouseTicks++;
    }

    GSoundTimer++;

    if (static_cast<int>(ResultsStepTicks / 2) < GSoundTimer)
    {
        GSoundTimer = 0;

        if (MCSoundRenderer* renderer = SoundRenderer(); renderer != nullptr)
        {
            renderer->ServiceStreams();
        }
    }

    InTimerThread = 0;
}

void EraseMouse()
{
    if (LockScreen() == 0)
    {
        return;
    }

    if (AGOldMouseH != 0)
    {
        CopyMouseRect(TempWindow.Buffer, TempWindow.XMax + 1, true);
    }

    AGMouseBuffer = 0;
}

void EraseMouseInBuffer()
{
    if (AGMouseBuffer == 0)
    {
        return;
    }

    if (AGOldMouseH != 0)
    {
        CopyMouseRect(ScreenBuffer()->Buffer, GWidth, true);
    }

    AGMouseBuffer = 0;
}

void DrawMouse()
{
    if (LockScreen() == 0)
    {
        return;
    }

    int32_t shape = Application->CursorShape;

    if (shape < 0 || shape > 0x7f)
    {
        return;
    }

    int32_t resolution = VfxShapeResolution(CursorShapes[shape], 0);
    AGOldMouseW = resolution >> 16;
    AGOldMouseH = resolution & 0xffff;
    int32_t minXY = VfxShapeMinxy(CursorShapes[Application->CursorShape], 0);
    AGOldMouseY = static_cast<int16_t>(minXY) + MouseScreenY;
    AGOldMouse = Application->CursorShape;
    AGOldMouseX = (minXY >> 16) + MouseScreenX;
    AGOldMouseXh = MouseScreenX;
    AGOldMouseYh = MouseScreenY;

    if (AGOldMouseX < 0)
    {
        AGOldMouseW += AGOldMouseX;
        AGOldMouseX = 0;
    }

    if (AGOldMouseY < 0)
    {
        AGOldMouseH += AGOldMouseY;
        AGOldMouseY = 0;
    }

    if (TempPane.X1 + 1 < AGOldMouseX + AGOldMouseW)
    {
        AGOldMouseW = TempPane.X1 - AGOldMouseX + 1;
    }

    if (TempPane.Y1 + 1 < AGOldMouseY + AGOldMouseH)
    {
        AGOldMouseH = TempPane.Y1 - AGOldMouseY + 1;
    }

    if (AGOldMouseH != 0)
    {
        CopyMouseRect(TempWindow.Buffer, TempWindow.XMax + 1, false);
    }

    // Cursor 0x12 (the wait cursor) is animated.
    if (Application->CursorShape == 0x12)
    {
        AGMouseFrame++;

        if (AGMouseFrame >= VfxShapeCount(CursorShapes[0x12]))
        {
            AGMouseFrame = 0;
        }
    }
    else
    {
        AGMouseFrame = 0;
    }

    VfxShapeDraw(&TempPane, CursorShapes[Application->CursorShape], AGMouseFrame, MouseScreenX, MouseScreenY);
    AGMouseBuffer = 1;
}

int LockScreen()
{
    if (AGLocked != 0)
    {
        return 1;
    }

    // The original locked the primary surface; the port's "surface" is the screen buffer itself.
    MCWindow* screen = ScreenBuffer();
    TempWindow.Buffer = screen->Buffer;
    TempPane.Window = &TempWindow;
    TempWindow.XMax = screen->XMax;
    TempWindow.YMax = GHeight - 1;
    TempPane.X0 = 0;
    TempPane.Y0 = 0;
    TempPane.X1 = GWidth - 1;
    TempPane.Y1 = GHeight - 1;
    AGLocked = 1;
    return 1;
}

void UnLockScreen()
{
    AGLocked = 0;
}
