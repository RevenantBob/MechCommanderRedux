#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "main/logistics.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"

namespace
{
    /// <summary>FNV-1a of the screen's pixels, with its size folded in first.</summary>
    uint32_t ScreenHash()
    {
        const _window* screen = screenPort->bitmap();
        const int32_t width = screen->x_max + 1;
        const int32_t height = screen->y_max + 1;
        uint32_t hash = 0x811c9dc5;
        hash = (hash ^ static_cast<uint32_t>(width)) * 0x01000193;
        hash = (hash ^ static_cast<uint32_t>(height)) * 0x01000193;
        const uint8_t* pixels = screen->buffer;

        for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); i++)
        {
            hash = (hash ^ pixels[i]) * 0x01000193;
        }

        return hash;
    }

    /// <summary>MC_TEST_SHOTS=&lt;folder&gt;: saves the screen as <paramref name="name"/>.bmp there and prints its hash.</summary>
    void SaveShot(const std::string& name, uint32_t hash)
    {
        const char* shots = std::getenv("MC_TEST_SHOTS");

        if (shots == nullptr || MCInput::Display() == nullptr)
        {
            return;
        }

        (void)MCInput::Display()->SaveScreenshot(std::filesystem::path(shots) / (name + ".bmp"));
        std::printf("  %s: 0x%08x\n", name.c_str(), hash);
    }
}

/// <summary>
/// Mission 1's screen, right after the scenario starts and 60 frames later, is pixel for pixel the recorded frame (a
/// regression check on the whole draw path: terrain, overlays, objects and the interface).
/// </summary>
/// <remarks>
/// Baselines: the pre-renderer code drew 0x1888f902 (both frames: the lance stands still). Renderer phase 1 (the
/// vfx clip fixes, OB-112..128) moved the terrain overlays up a row (OB-117), and nothing else, giving 0x2bc262d5.
/// </remarks>
TEST_CASE_ISOLATED("game: mission 1's screen matches the recorded frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    const uint32_t started = ScreenHash();
    SaveShot("mission1 start", started);

    for (int32_t frame = 0; frame < 60; frame++)
    {
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    const uint32_t later = ScreenHash();
    SaveShot("mission1 later", later);
    CHECK_EQ(started, 0x2bc262d5u);
    CHECK_EQ(later, 0x2bc262d5u);
}

/// <summary>
/// Each logistics screen, reached as its buttons reach it, is pixel for pixel what the game drew before the renderer
/// interface was introduced: the main menu, the preferences and load screens, then a new campaign's briefing, the
/// purchase screen with each inventory tab, the repair screen (mech lab) and the briefing again. (Renderer phase 1's
/// clip fixes changed none of them.)
/// </summary>
TEST_CASE_ISOLATED("game: the logistics screens match the pre-renderer frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());

    // A screen change wipes from one screen to the next over several frames; two seconds settles every one.
    const auto settle = []
    {
        for (int32_t frame = 0; frame < 30; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }

        return ScreenHash();
    };

    struct Step
    {
        const char* Name;
        std::function<void()> Action;
        uint32_t Expected;
    };

    const Step steps[] = {
        {"main menu", [] {}, 0x59acc997u},
        {"preferences", [] { ShowPreferences(); }, 0xe7477674u},
        {"main menu after preferences", [] { CancelPrefs(); }, 0x59acc997u},
        {"load screen", [] { LoadScreen(); }, 0xb773197eu},
        {"main menu after load", [] { Cancel(); }, 0x59acc997u},
        {"briefing", [] { NewCampaign(); }, 0xbcd2ec30u},
        {"purchase", [] { globalLogPtr->setUpPurchaseScreen(-1); }, 0x7e077fadu},
        {"purchase, pilots", [] { globalLogPtr->purchaseScreen->setUpPilotInv(-1, -1); }, 0x4ae27598u},
        {"purchase, components", [] { globalLogPtr->purchaseScreen->setUpCompInv(-1, -1); }, 0x62910f9cu},
        {"purchase, mechs", [] { globalLogPtr->purchaseScreen->setUpMechInv(-1, -1); }, 0x2ef60e5eu},
        {"repair", [] { globalLogPtr->setUpRepairScreen(-1); }, 0xd28d50c5u},
        {"briefing again", [] { globalLogPtr->setUpBriefingScreen(-1); }, 0x042383bfu},
    };

    // MC_TEST_SHOTS=<folder>: a screenshot of each step, to see what a changed hash shows.
    int32_t index = 0;

    for (const Step& step : steps)
    {
        MCTest::Scope scope(step.Name);
        step.Action();
        const uint32_t hash = settle();
        SaveShot(std::format("logistics{:02} {}", index, step.Name), hash);
        CHECK_EQ(hash, step.Expected);
        index++;
    }
}
