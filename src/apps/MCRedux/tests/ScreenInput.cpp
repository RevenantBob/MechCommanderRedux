#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"

namespace MCScreenInput
{
    uint32_t ScreenHash()
    {
        const _window* screen = screenPort->bitmap();
        const int32_t width = screen->x_max + 1;
        const int32_t height = screen->y_max + 1;
        uint32_t hash = 0x811c9dc5;
        hash = (hash ^ static_cast<uint32_t>(width)) * 0x01000193;
        hash = (hash ^ static_cast<uint32_t>(height)) * 0x01000193;
        std::vector<uint8_t> shown;

        if (MCInput::Display() != nullptr && MCInput::Display()->Screen()->buffer == screen->buffer)
        {
            shown = MCInput::Display()->ComposeScreen();
        }
        else
        {
            shown.assign(screen->buffer, screen->buffer + static_cast<size_t>(width) * static_cast<size_t>(height));
        }

        const uint8_t* pixels = shown.data();

        for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); i++)
        {
            hash = (hash ^ pixels[i]) * 0x01000193;
        }

        return hash;
    }

    void SaveShot(const std::string& name, uint32_t hash)
    {
        const char* shots = MCTest::Option("shots");

        if (shots == nullptr || MCInput::Display() == nullptr)
        {
            return;
        }

        (void)MCInput::Display()->SaveScreenshot(std::filesystem::path(shots) / (name + ".bmp"));
        std::printf("  %s: 0x%08x\n", name.c_str(), hash);
    }

    void SendMouse(int32_t type, int32_t x, int32_t y, bool leftHeld)
    {
        // A motion event moves the game's cursor without warping the real mouse (SetCursorPos would).
        if (MCDisplay* display = MCInput::Display(); display != nullptr)
        {
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = SDL_GetWindowID(display->Window());
            display->LogicalToWindow(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, motion.motion.x,
                                     motion.motion.y);
            MCInput::HandleEvent(motion);
        }

        mouseScreenX = x;
        mouseScreenY = y;
        oldMouseX = x;
        oldMouseY = y;
        aEvent event;
        event.clear();
        event.type = static_cast<uint8_t>(type);
        event.x = x;
        event.y = y;
        event.leftButton = type == 1 ? 0xff : (leftHeld ? 1 : 0);
        // CheckMouse, run each frame, sees no button held and no move, so it adds no events of its own.
        handleEvent(&event);
    }

    void Click(int32_t x, int32_t y)
    {
        SendMouse(7, x, y, false);
        SendMouse(1, x, y, true);
        MCTestGame::RunFrame(1.0f / 15.0f);
        SendMouse(4, x, y, false);
    }

    void Drag(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
    {
        SendMouse(7, x0, y0, false);
        SendMouse(1, x0, y0, true);
        MCTestGame::RunFrame(1.0f / 15.0f);

        for (int32_t step = 1; step <= 8; step++)
        {
            SendMouse(7, x0 + (x1 - x0) * step / 8, y0 + (y1 - y0) * step / 8, true);
            MCTestGame::RunFrame(1.0f / 15.0f);
        }

        SendMouse(4, x1, y1, false);
    }

    void SendKey(uint8_t key)
    {
        aEvent event;
        event.clear();
        event.type = 10;
        event.key = key;
        handleEvent(&event);
    }

    void RealMove(int32_t x, int32_t y)
    {
        MCDisplay* display = MCInput::Display();
        SDL_Event motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        motion.motion.windowID = SDL_GetWindowID(display->Window());
        display->LogicalToWindow(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, motion.motion.x,
                                 motion.motion.y);
        MCInput::HandleEvent(motion);
        // The first frame's UpdateDisplay reads the cursor; the second's CheckMouse sends the move.
        MCTestGame::RunFrame(1.0f / 15.0f);
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    void RealButton(bool down)
    {
        MCDisplay* display = MCInput::Display();
        SDL_Event button{};
        button.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        button.button.windowID = SDL_GetWindowID(display->Window());
        button.button.button = SDL_BUTTON_LEFT;
        button.button.down = down;
        button.button.clicks = 1;
        display->LogicalToWindow(static_cast<float>(mouseScreenX) + 0.5f, static_cast<float>(mouseScreenY) + 0.5f,
                                 button.button.x, button.button.y);
        MCInput::HandleEvent(button);
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    void RealClick(int32_t x, int32_t y)
    {
        RealMove(x, y);
        RealButton(true);
        RealButton(false);
    }

    void RealKey(SDL_Scancode scancode)
    {
        MCDisplay* display = MCInput::Display();

        for (bool down : {true, false})
        {
            SDL_Event key{};
            key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            key.key.windowID = SDL_GetWindowID(display->Window());
            key.key.scancode = scancode;
            key.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
            key.key.down = down;
            MCInput::HandleEvent(key);
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    }

    void RealWheel(int32_t notches)
    {
        MCDisplay* display = MCInput::Display();
        SDL_Event wheel{};
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.windowID = SDL_GetWindowID(display->Window());
        wheel.wheel.y = static_cast<float>(notches);
        wheel.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
        display->LogicalToWindow(static_cast<float>(mouseScreenX) + 0.5f, static_cast<float>(mouseScreenY) + 0.5f,
                                 wheel.wheel.mouse_x, wheel.wheel.mouse_y);
        MCInput::HandleEvent(wheel);
        MCTestGame::RunFrame(1.0f / 15.0f);
    }
}
