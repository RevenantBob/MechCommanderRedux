#include "stdafx.h"
#include "gui/updisp.h"
#include "engine/writegif.h"
#include "gameos/soundchannel.h"
#include "gameos/soundrenderer.h"
#include "gameos/soundresource.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/awindow.h"
#include "gui/mchwcursor.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "logistics/logmain.h"
#include "mission/mission.h"
#include "platform/MCAudio.h"
#include "platform/MCDisplay.h"
#include "platform/MCFrameLog.h"
#include "platform/MCInput.h"
#include "platform/MCRenderer.h"
#include "vfx/vfxfuncs.h"

int AG_oldMouseX = -1;
int AG_oldMouseY = -1;
int AG_oldMouseXh = -1;
int AG_oldMouseYh = -1;
int AG_oldMouse = -1;
int AG_oldMouseW = 0;
int AG_oldMouseH = 0;
int AG_MouseBuffer = 0;
int AG_locked = 0;
char MouseBuffer[0x10000];
float FrameRateArray[512];
int frPointer = 0;
uint32_t HTimer = 0;
int gSoundTimer = 0;
int keepScreenBlack = 0;
volatile int inTimerThread = 0;
int32_t shotNum = 0;
char gifName[20];
uint8_t* connectShape = nullptr;
uint8_t** cursorShapes = nullptr;
int gWinWidth = 0;
int gWinHeight = 0;
_window tempWINDOW = {};
_pane tempPANE = {};
uint32_t MouseTicks = 0;
int mouseScreenX = 320;
int mouseScreenY = 240;
int pageFlipping = 0;
uint8_t displayFrozen = 0;

namespace
{
    /// <summary>The mouse timer's thread (timeSetEvent's periodic callback).</summary>
    std::jthread mouseTimerThread;

    /// <summary>The screen's window: the 8-bit buffer the game draws and the display shows.</summary>
    _window* screenBuffer()
    {
        return screenPort->frame()->window;
    }

    /// <summary>
    /// Port: the frame counter (<c>gShowFps</c>) in the shown view's top-right corner: frames a second and the mean
    /// frame time over the last half second of real time, on a black box that only grows (so shorter text leaves no
    /// digits behind where nothing else redraws the screen).
    /// </summary>
    void drawFrameCounter()
    {
        static std::chrono::steady_clock::time_point periodStart = std::chrono::steady_clock::now();
        static int32_t periodFrames = 0;
        static char text[48] = "-- fps";
        static int32_t boxWidth = 0;
        MCDisplay* display = MCInput::Display();
        aFont* font = medWhiteFont;

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
        const int32_t textWidth = font->width(characters);
        boxWidth = std::max(boxWidth, textWidth + 8);
        const SDL_Rect view = display->View();
        const int32_t right = view.x + view.w - 1;
        const int32_t top = view.y;
        _window* screen = screenBuffer();
        MCRenderer::For(screen).Clear(screen, MCRect{right - boxWidth + 1, top, right, top + font->height() + 3}, 0);
        font->writeString(screenPort->frame(), right - 3 - textWidth, top + 2, characters, -1);
    }

    /// <summary>
    /// Port: picks the part of the screen the display shows. Only a running scenario uses the whole screen; the
    /// menus, logistics and the results screen are laid out for 640x480 and drawn in the top-left corner of a larger
    /// one, so the display shows just that corner, scaled to the window. A full-screen movie is centred on the
    /// screen, so the view is a 640x480 (or the movie's size, if larger) centred on it.
    /// </summary>
    void updateDisplayView(MCDisplay* display)
    {
        constexpr int32_t InterfaceWidth = 640;
        constexpr int32_t InterfaceHeight = 480;
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = display->Width();
        int32_t height = display->Height();
        aObject* movie = application != nullptr ? application->smackerWindow : nullptr;

        if (movie != nullptr)
        {
            width = std::max(InterfaceWidth, movie->width());
            height = std::max(InterfaceHeight, movie->height());
            x = movie->globalX() + movie->width() / 2 - width / 2;
            y = movie->globalY() + movie->height() / 2 - height / 2;
        }
        else if (mission == nullptr || mission->missionState != 7)
        {
            width = InterfaceWidth;
            height = InterfaceHeight;
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
    void presentScreen()
    {
        MCDisplay* display = MCInput::Display();

        if (display != nullptr)
        {
            MCFrameLog::Scope present("present");
            updateDisplayView(display);
            (void)display->Present();
        }
    }

    /// <summary>Where the cursor is on the screen, kept inside it.</summary>
    void readCursorPosition()
    {
        MCPoint cursor = MCInput::GetCursorPos();
        mouseScreenX = cursor.x;
        mouseScreenY = cursor.y;

        if (gWidth <= cursor.x)
        {
            mouseScreenX = gWidth - 1;
        }

        if (gHeight <= cursor.y)
        {
            mouseScreenY = gHeight - 1;
        }
    }

    /// <summary>Saves the screen under the cursor and draws the cursor into the screen buffer.</summary>
    void drawCursorInBuffer()
    {
        // Port fix: the cursor is the system's, which follows the mouse between frames; the original drew it here.
        if (gSoftwareCursor == 0)
        {
            MCFrameLog::Scope cursor("cursor");
            MCHardwareCursorUpdate();
            return;
        }

        if (application->cursorShape == -1)
        {
            return;
        }

        SaveMouseBackBuffer();
        AG_shape_draw(screenPort->frame(), cursorShapes[application->cursorShape], 0, mouseScreenX, mouseScreenY);
    }

    /// <summary>Ends a finished movie window: stops the movie, destroys and deletes the window.</summary>
    void closeMovieWindow(aObject*& window)
    {
        static_cast<aSmackerWindow*>(window)->endSmackerMovie();
        window->destroy();
        delete window;
        window = nullptr;
    }

    /// <summary>Copies the cursor rectangle between <see cref="MouseBuffer"/> and a screen of rows
    /// <paramref name="pitch"/> bytes.</summary>
    void copyMouseRect(uint8_t* screen, int pitch, bool toScreen)
    {
        uint8_t* saved = reinterpret_cast<uint8_t*>(MouseBuffer);
        uint8_t* row = screen + pitch * AG_oldMouseY + AG_oldMouseX;

        for (int y = 0; y < AG_oldMouseH; y++)
        {
            if (toScreen)
            {
                std::memcpy(row, saved, AG_oldMouseW);
            }
            else
            {
                std::memcpy(saved, row, AG_oldMouseW);
            }

            saved += AG_oldMouseW;
            row += pitch;
        }
    }
}

void SaveMouseBackBuffer()
{
    AG_MouseBuffer = 1;
    int32_t resolution = VFX_shape_resolution(cursorShapes[application->cursorShape], 0);
    AG_oldMouseW = resolution >> 16;
    AG_oldMouseH = resolution & 0xffff;
    int32_t minXY = VFX_shape_minxy(cursorShapes[application->cursorShape], 0);
    AG_oldMouse = application->cursorShape;
    AG_oldMouseXh = mouseScreenX;
    AG_oldMouseX = (minXY >> 16) + mouseScreenX;
    AG_oldMouseY = static_cast<int16_t>(minXY) + mouseScreenY;
    AG_oldMouseYh = mouseScreenY;

    if (AG_oldMouseX < 0)
    {
        AG_oldMouseW += AG_oldMouseX;
        AG_oldMouseX = 0;
    }

    if (AG_oldMouseY < 0)
    {
        AG_oldMouseH += AG_oldMouseY;
        AG_oldMouseY = 0;
    }

    if (gWidth < AG_oldMouseX + AG_oldMouseW)
    {
        AG_oldMouseW = gWidth - AG_oldMouseX;
    }

    if (gHeight < AG_oldMouseY + AG_oldMouseH)
    {
        AG_oldMouseH = gHeight - AG_oldMouseY;
    }

    int height = AG_oldMouseH;

    if (AG_oldMouseW < 0)
    {
        AG_oldMouseW = 0;
    }

    if (AG_oldMouseH < 0)
    {
        AG_oldMouseH = 0;
    }

    if (height > 0)
    {
        copyMouseRect(screenBuffer()->buffer, gWidth, false);
    }
}

void blankScreen()
{
    aLockScreen();
    VFX_pane_wipe(screenPort->frame(), 0);
    aUnlockScreen();
    readCursorPosition();
    drawCursorInBuffer();
    presentScreen();
}

int32_t UpdateDisplay(int screenShot, int staticNoise, int32_t noiseChance, int showProgress, int32_t progress)
{
    if (displayFrozen != 0)
    {
        return 0;
    }

    if (application->smackerWindow2 != nullptr)
    {
        application->smackerWindow2->display();

        if (movieOver != 0)
        {
            closeMovieWindow(application->smackerWindow2);
        }

        return 0;
    }

    if (InMouseCritSec != 0)
    {
        return 0;
    }

    if (paletteRgb != nullptr)
    {
        application->fadeDownCurrentPalette();
    }

    std::lock_guard<std::recursive_mutex> lock(MouseCritSec);
    InMouseCritSec = 1;
    // Port: dragged objects draw into the cursor while the frame is drawn (MCHardwareCursorCarry).
    MCHardwareCursorNewFrame();
    // Port: a new frame's op tables (translucent UI over the world view; see MCUnderlay).
    MCRenderer::ResetOpTables();

    if (application->smackerWindow == nullptr)
    {
        if (globalLogPtr != nullptr && screenWindow->frame()->window->buffer != nullptr)
        {
            VFX_pane_wipe(screenWindow->frame(), 0);
        }

        MCFrameLog::Scope draw("draw");

        for (int32_t i = 0; i < screenWindow->numberOfChildren(); i++)
        {
            screenWindow->child(i)->display();
        }
    }
    else
    {
        // Full screen with page flipping, the original first copied the front page to the back one.
        aLockScreen();
        application->smackerWindow->display();
        aUnlockScreen();
    }

    AG_MouseBuffer = 0;
    aLockScreen();

    if (screenShot != 0)
    {
        shotNum++;
        std::snprintf(gifName, sizeof(gifName), "scrn%04d.tga", shotNum);
        // Port: the screen as shown (the world view under the key, read back when the GPU draws the frame), not its
        // memory.
        MCDisplay* display = MCInput::Display();

        if (display != nullptr)
        {
            std::vector<uint8_t> shown = display->ComposeScreen();
            writeTGA8Bit(gifName, shown.data(), static_cast<uint32_t>(display->Width()),
                         static_cast<uint32_t>(display->Height()));
        }
    }

    if (staticNoise != 0 && noiseChance != 0)
    {
        // Port: the original wrote the noise into the screen's memory; the row goes to the renderer.
        std::vector<uint8_t> line(static_cast<size_t>(application->width() >> 1) * 2);

        for (int32_t row = 0; row < application->height(); row++)
        {
            if (RollDice(noiseChance) != 0)
            {
                _window* screen = screenBuffer();
                uint8_t* pixel = line.data();

                for (int32_t i = 0; i < application->width() >> 1; i++)
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

    if (connectShape != nullptr && showProgress != 0)
    {
        int32_t centerY = application->height() >> 1;
        int32_t centerX = application->width() >> 1;
        AG_shape_draw(screenPort->frame(), connectShape, 0, centerX, centerY);
        int32_t left = centerX - 0x5f;
        int32_t right = static_cast<int32_t>(progress * 0.01 * 188.0 + left);
        SCRNVERTEX bar[4] = {};
        bar[0].x = left;
        bar[0].y = centerY + 3;
        bar[1].x = right;
        bar[1].y = centerY + 3;
        bar[2].x = right;
        bar[2].y = centerY + 10;
        bar[3].x = left;
        bar[3].y = centerY + 10;

        for (SCRNVERTEX& vertex : bar)
        {
            vertex.c = 0xe40000;
        }

        VFX_flat_polygon(screenPort->frame(), 4, bar);
    }

    FrameRateArray[frPointer] = frameRate > 60.0f ? 60.0f : frameRate;
    frPointer = (frPointer + 1) & 0x1ff;

    if (AndyFramerate != 0)
    {
        // The frame graph: 512 frames, 3 pixels per frame a second, 60 at the top.
        whiteFont->writeString(screenWindow->frame(), 0x20, 7, reinterpret_cast<uint8_t*>(const_cast<char*>("60+ f/s")),
                               -1);
        whiteFont->writeString(screenWindow->frame(), 0x20, 0xbb,
                               reinterpret_cast<uint8_t*>(const_cast<char*>("0   f/s")), -1);
        // Port: the original set the two lines in the screen's memory.
        _window* screen = screenBuffer();
        MCRenderer::For(screen).Clear(screen, MCRect{0x40, 9, 0x40 + 0x1ff, 9}, 0xff);
        MCRenderer::For(screen).Clear(screen, MCRect{0x40, 0xbf, 0x40 + 0x1ff, 0xbf}, 0xff);

        for (int32_t y = 0xaf; y > 10; y -= 0xf)
        {
            AG_pixel_write(screenPort->frame(), 0x240, y, 0xff);
        }

        int32_t frame = frPointer;

        for (int32_t x = 0; x < 0x200; x++)
        {
            int32_t height = static_cast<int32_t>(FrameRateArray[frame] * 3.0f);
            AG_pixel_write(screenPort->frame(), x + 0x40, 0xbe - height, 0xf9);
            frame = (frame + 1) & 0x1ff;
        }
    }

    if (keepScreenBlack != 0)
    {
        VFX_pane_wipe(screenPort->frame(), 0);
    }

    if (gShowFps != 0)
    {
        drawFrameCounter();
    }

    aUnlockScreen();

    if (application->smackerWindow2 != nullptr && movieOver != 0)
    {
        closeMovieWindow(application->smackerWindow2);
    }

    if (application->smackerWindow != nullptr)
    {
        if (movieOver == 0)
        {
            application->smackerWindow->checkSmackerPalette();
            presentScreen();
        }
        else
        {
            closeMovieWindow(application->smackerWindow);
        }
    }

    readCursorPosition();
    drawCursorInBuffer();
    presentScreen();

    if (paletteRgb != nullptr)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(17));

        if (gBitDepth != 16)
        {
            application->tweakDDPalette(globalFirst, globalEntries, paletteRgb, 1);
        }

        globalFirst = 0;
        globalEntries = 0;
        delete[] paletteRgb;
        paletteRgb = nullptr;
    }

    InMouseCritSec = 0;
    return 0;
}

void MouseTimerInit()
{
    g_SRData = _srdata{};
    // timeGetDevCaps / timeBeginPeriod ("No Mouse Timer available") have no counterpart.
    InMouseCritSec = 0;
    uint32_t period = 1000 / resultsStepTicks;
    mouseTimerThread = std::jthread(
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

    mouseThreadStarted = 1;
}

void MouseTimerKill()
{
    mouseThreadStarted = 0;

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
    while (inTimerThread != 0)
    {
        std::this_thread::yield();
    }

    mouseTimerThread.request_stop();

    if (mouseTimerThread.joinable() && mouseTimerThread.get_id() != std::this_thread::get_id())
    {
        mouseTimerThread.join();
    }
    while (inTimerThread != 0)
    {
        std::this_thread::yield();
    }

    HTimer = 0;
}

void MouseTimer(uint32_t timerId, uint32_t msg, uintptr_t user, uintptr_t dw1, uintptr_t dw2)
{
    inTimerThread = 1;
    {
        std::lock_guard<std::recursive_mutex> lock(MouseCritSec);
        bool unlockScreen = true;

        if (application->smackerWindow2 == nullptr)
        {
            if (gFullScreen == 0 || pageFlipping == 0 || gBitDepth != 8)
            {
                unlockScreen = false;
            }
            else if (application->cursorShape == -1)
            {
                if (AG_MouseBuffer != 0)
                {
                    EraseMouse();
                }
            }
            else
            {
                MCPoint cursor = MCInput::GetCursorPos();
                mouseScreenX = cursor.x;
                mouseScreenY = cursor.y;
                bool moved = true;

                if (AG_MouseBuffer != 0)
                {
                    if (application->cursorShape == AG_oldMouse && cursor.x == AG_oldMouseXh &&
                        cursor.y == AG_oldMouseYh)
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

    gSoundTimer++;

    if (static_cast<int>(resultsStepTicks / 2) < gSoundTimer)
    {
        gSoundTimer = 0;
        std::lock_guard<std::recursive_mutex> soundLock(SoundCritSec);

        for (int32_t i = 0; i < g_SRData.numChannels; i++)
        {
            SoundChannel* channel = g_SRData.channels[i];

            if (channel != nullptr && channel->resource != nullptr &&
                channel->resource->type == SOUND_RESOURCE_STREAM && channel->streaming != 0)
            {
                channel->ServiceBuffer();
            }
        }
    }

    inTimerThread = 0;
}

void EraseMouse()
{
    if (LockScreen() == 0)
    {
        return;
    }

    if (AG_oldMouseH != 0)
    {
        copyMouseRect(tempWINDOW.buffer, tempWINDOW.x_max + 1, true);
    }

    AG_MouseBuffer = 0;
}

void EraseMouseInBuffer()
{
    if (AG_MouseBuffer == 0)
    {
        return;
    }

    if (AG_oldMouseH != 0)
    {
        copyMouseRect(screenBuffer()->buffer, gWidth, true);
    }

    AG_MouseBuffer = 0;
}

void DrawMouse()
{
    if (LockScreen() == 0)
    {
        return;
    }

    int32_t shape = application->cursorShape;

    if (shape < 0 || shape > 0x7f)
    {
        return;
    }

    int32_t resolution = VFX_shape_resolution(cursorShapes[shape], 0);
    AG_oldMouseW = resolution >> 16;
    AG_oldMouseH = resolution & 0xffff;
    int32_t minXY = VFX_shape_minxy(cursorShapes[application->cursorShape], 0);
    AG_oldMouseY = static_cast<int16_t>(minXY) + mouseScreenY;
    AG_oldMouse = application->cursorShape;
    AG_oldMouseX = (minXY >> 16) + mouseScreenX;
    AG_oldMouseXh = mouseScreenX;
    AG_oldMouseYh = mouseScreenY;

    if (AG_oldMouseX < 0)
    {
        AG_oldMouseW += AG_oldMouseX;
        AG_oldMouseX = 0;
    }

    if (AG_oldMouseY < 0)
    {
        AG_oldMouseH += AG_oldMouseY;
        AG_oldMouseY = 0;
    }

    if (tempPANE.x1 + 1 < AG_oldMouseX + AG_oldMouseW)
    {
        AG_oldMouseW = tempPANE.x1 - AG_oldMouseX + 1;
    }

    if (tempPANE.y1 + 1 < AG_oldMouseY + AG_oldMouseH)
    {
        AG_oldMouseH = tempPANE.y1 - AG_oldMouseY + 1;
    }

    if (AG_oldMouseH != 0)
    {
        copyMouseRect(tempWINDOW.buffer, tempWINDOW.x_max + 1, false);
    }

    // Cursor 0x12 (the wait cursor) is animated.
    if (application->cursorShape == 0x12)
    {
        AG_mouseFrame++;

        if (AG_mouseFrame >= VFX_shape_count(cursorShapes[0x12]))
        {
            AG_mouseFrame = 0;
        }
    }
    else
    {
        AG_mouseFrame = 0;
    }

    VFX_shape_draw(&tempPANE, cursorShapes[application->cursorShape], AG_mouseFrame, mouseScreenX, mouseScreenY);
    AG_MouseBuffer = 1;
}

int LockScreen()
{
    if (AG_locked != 0)
    {
        return 1;
    }

    // The original locked the primary surface; the port's "surface" is the screen buffer itself.
    _window* screen = screenBuffer();
    tempWINDOW.buffer = screen->buffer;
    tempPANE.window = &tempWINDOW;
    tempWINDOW.x_max = screen->x_max;
    tempWINDOW.y_max = gHeight - 1;
    tempPANE.x0 = 0;
    tempPANE.y0 = 0;
    tempPANE.x1 = gWidth - 1;
    tempPANE.y1 = gHeight - 1;
    AG_locked = 1;
    return 1;
}

void UnLockScreen()
{
    AG_locked = 0;
}
