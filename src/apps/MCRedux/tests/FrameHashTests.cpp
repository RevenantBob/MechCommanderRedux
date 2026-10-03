#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "camera/camera.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "iface/iface.h"
#include "logistics/logmain.h"
#include "logistics/logpur.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "gui/updisp.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

namespace
{
    /// <summary>
    /// FNV-1a of the screen as shown (its pixels with the world view composited under the key, in palette indices),
    /// with its size folded in first.
    /// </summary>
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

    /// <summary>
    /// Sends the game a mouse event at (<paramref name="x"/>, <paramref name="y"/>) as CheckMouse makes them (type 1
    /// left down, 4 left up, 7 a move), with the cursor moved there first.
    /// </summary>
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

    /// <summary>Moves the mouse to (<paramref name="x"/>, <paramref name="y"/>) and clicks there.</summary>
    void Click(int32_t x, int32_t y)
    {
        SendMouse(7, x, y, false);
        SendMouse(1, x, y, true);
        MCTestGame::RunFrame(1.0f / 15.0f);
        SendMouse(4, x, y, false);
    }

    /// <summary>Presses at (<paramref name="x0"/>, <paramref name="y0"/>), drags to (<paramref name="x1"/>, <paramref name="y1"/>) over a few frames and lets go.</summary>
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
}

/// <summary>
/// Mission 1's screen, right after the scenario starts and 60 frames later, is pixel for pixel the recorded frame (a
/// regression check on the whole draw path: terrain, overlays, objects and the interface).
/// </summary>
/// <remarks>
/// Baselines: the pre-renderer code drew 0x1888f902 (both frames: the lance stands still). Renderer phase 1 (the
/// vfx clip fixes, OB-112..128) moved the terrain overlays up a row (OB-117), and nothing else, giving 0x2bc262d5.
/// Renderer phase 2 (zoom): the mission starts zoomed out (camera scale 1, the half-size art) in the original; now
/// the camera stays at scale 100 and the test fixes the zoom at 480 lines, one world pixel per screen pixel, so the
/// frame shows the full-size art closer in: 0xd9566f22.
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
    CHECK_EQ(started, 0xd9566f22u);
    CHECK_EQ(later, 0xd9566f22u);
}

/// <summary>
/// The composite shader shows what the CPU composite computes: in mission 1, at one world pixel per screen pixel and
/// zoomed out and in (the world scaled into the view, the overlays and translucent bars over it), every pixel of the
/// frame read back from the GPU is the CPU composite's palette colour.
/// </summary>
TEST_CASE_ISOLATED("game: the GPU composite matches the CPU composite")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    MCDisplay* display = MCInput::Display();
    REQUIRE(display != nullptr);

    if (!display->CompositesOnGpu())
    {
        std::cout << "  (skipped: no GPU composite on this machine)\n";
        return;
    }

    for (const float zoom : {480.0f, 720.0f, 400.0f})
    {
        MCTest::Scope scope(std::format("{} lines", zoom));
        MCFixedZoomHeight = zoom;

        for (int32_t frame = 0; frame < 3; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }

        const auto read = display->ReadFrame();
        REQUIRE(read.has_value());
        const std::vector<uint8_t> composed = display->ComposeScreen();
        SDL_Color colors[256];
        display->GetShownColors(colors);
        int32_t different = 0;

        for (size_t i = 0; i < composed.size(); i++)
        {
            const SDL_Color& expected = colors[composed[i]];
            const SDL_Color& actual = (*read)[i];

            if (expected.r != actual.r || expected.g != actual.g || expected.b != actual.b)
            {
                different++;
            }
        }

        CHECK_EQ(different, 0);
    }
}

/// <summary>
/// The tactical map draws each of its pages (it draws itself each frame from its state since renderer phase 3): the
/// map, the info page with the first mech of the lance and without a unit, the mission page and the salvage page all
/// show, each different from the others, and the map page comes back as it was.
/// </summary>
TEST_CASE_ISOLATED("game: the tactical map draws every page")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    TacticalMap* map = Terrain::terrainTacticalMap;
    REQUIRE(map != nullptr);
    FriendlyMechIcon* icon = theInterface->mechBar->getButton(0);
    REQUIRE(icon != nullptr);

    // Each page shown for 10 frames (two thirds of a second: the info and mission pages refresh every half second).
    auto show = [map](const char* name, auto choose)
    {
        choose();

        for (int32_t frame = 0; frame < 10; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }

        const uint32_t hash = ScreenHash();
        SaveShot(std::string("tacmap ") + name, hash);
        return hash;
    };

    const uint32_t mapPage = show("map", [] {});
    const uint32_t infoEmpty = show("info empty",
                                    [map]
                                    {
                                        map->infoObject = nullptr;
                                        map->SetDisplayType(TACMAP_INFO);
                                    });
    const uint32_t infoMech = show("info mech", [map, icon] { map->SetID(icon->partId); });
    const uint32_t infoPayload = show("info payload", [map] { map->SetDataDisplayMode(2, -1); });
    const uint32_t missionPage = show("mission", [map] { map->SetDisplayType(TACMAP_MISSION); });
    const uint32_t salvagePage = show("salvage", [map] { map->SetDisplayType(TACMAP_SALVAGE); });
    const uint32_t mapAgain = show("map again", [map] { map->SetDisplayType(TACMAP_MAP); });

    const uint32_t pages[] = {mapPage, infoEmpty, infoMech, infoPayload, missionPage, salvagePage};

    for (size_t i = 0; i < std::size(pages); i++)
    {
        for (size_t j = i + 1; j < std::size(pages); j++)
        {
            MCTest::Scope scope(std::format("pages {} and {}", i, j));
            CHECK(pages[i] != pages[j]);
        }
    }

    // The lance stands still in mission 1, so the map page looks as it did.
    CHECK_EQ(mapAgain, mapPage);
}

/// <summary>
/// Mission 1 won (every objective succeeded): the results screen counts the resource points up and lists the
/// statistics, the objectives and the pilots a step at a time, then the debriefing. Every frame of that, and the
/// finished screen, is pixel for pixel what the screen drew when it still painted each step into its picture (before
/// renderer phase 3 step 3).
/// </summary>
TEST_CASE_ISOLATED("game: the mission results screen matches the recorded frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    // The screen's steps are timed in MouseTicks, which the mouse timer's thread counts in real time; the test counts
    // them itself.
    MouseTimerKill();

    for (uint32_t i = 0; i < scenario->numObjectives; i++)
    {
        scenario->objectives[i].status = 1;
    }

    scenarioResult = 4;

    for (int32_t frame = 0; frame < 30 && mission->missionState != 6; frame++)
    {
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    REQUIRE_EQ(mission->missionState, 6);
    REQUIRE(mission->resultsScreen != nullptr);

    // Five ticks a frame: a resource point step takes one tick, the others resultsStepTicks (20).
    uint32_t frames = 0x811c9dc5;
    uint32_t last = 0;
    int32_t frame = 0;

    for (; frame < 3000 && mission->resultsScreen != nullptr && !mission->resultsScreen->Finished(); frame++)
    {
        MouseTicks += 5;
        MCTestGame::RunFrame(1.0f / 15.0f);
        last = ScreenHash();
        frames = (frames ^ last) * 0x01000193;

        if (frame % 40 == 0)
        {
            SaveShot(std::format("results {:04}", frame), last);
        }
    }

    for (int32_t settle = 0; settle < 10; settle++)
    {
        MouseTicks += 5;
        MCTestGame::RunFrame(1.0f / 15.0f);
    }

    const uint32_t finished = ScreenHash();
    SaveShot("results finished", finished);
    CHECK_EQ(frame, 55);
    CHECK_EQ(frames, 0xa17463bcu);
    CHECK_EQ(finished, 0xea82833au);
}

/// <summary>
/// Each logistics screen, reached as its buttons reach it, is pixel for pixel what the game drew before the renderer
/// interface was introduced: the main menu, the preferences and load screens, then a new campaign's briefing, the
/// purchase screen with each inventory tab, the repair screen (mech lab) and the briefing again. (Renderer phase 1's
/// clip fixes changed none of them.)
/// </summary>
/// <remarks>
/// Then, driven by the mouse, the steps renderer phase 3 step 4 (logistics drawn from its state) must keep: the
/// mission briefing tab, two units dragged into force group 1, a hovered screen button, a mech bought (drag, purchase
/// dialog, quantity, accept), an inventory row and a variant button clicked, the shop scrolled, the "not enough
/// points" message and its OK, the component inventory, and the mech bay's payload list and second mech. Those were
/// recorded on the code before step 4, as was the fold of every frame of the run (wipes and dialogs included).
/// </remarks>
TEST_CASE_ISOLATED("game: the logistics screens match the pre-renderer frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());

    // A screen change wipes from one screen to the next over several frames; two seconds settles every one.
    // Every frame on the way (the wipes, the dialogs opening) is folded into one more hash.
    uint32_t frames = 0x811c9dc5;
    const auto settle = [&frames]
    {
        uint32_t hash = 0;

        for (int32_t frame = 0; frame < 30; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
            hash = ScreenHash();
            frames = (frames ^ hash) * 0x01000193;
        }

        return hash;
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
        {"briefing, mission tab", [] { Click(202, 265); }, 0xd643b44du},
        {"briefing, deploy", [] { Drag(40, 385, 245, 60); }, 0x9339c191u},
        {"briefing, deploy second", [] { Drag(90, 385, 300, 60); }, 0x9a752797u},
        {"briefing, hover", [] { SendMouse(7, 100, 60, false); }, 0x6391f04fu},
        {"briefing, slot to slot", [] { Drag(250, 60, 250, 110); }, 0x6ec3ed17u},
        {"briefing, onto a unit", [] { Drag(250, 110, 300, 60); }, 0x378f5ca2u},
        {"briefing, box list scroll", [] { Click(626, 467); }, 0x262779fcu},
        {"briefing, back to pane", [] { Drag(300, 60, 60, 400); }, 0xdcb32f52u},
        {"briefing, deploy again", [] { Drag(40, 385, 245, 60); }, 0x2be92259u},
        {"purchase again",
         []
         {
             // Enough points and stock to buy with; the store shows the stock, as the game redraws a row whose stock
             // changed.
             ResourcePoints = 100000;

             for (PurMech* mech = globalLogPtr->purMechList->first; mech != nullptr; mech = mech->next)
             {
                 for (PurMechData* variant : mech->variants)
                 {
                     if (variant != nullptr)
                     {
                         variant->numAvailable = 2;
                     }
                 }

                 mech->block->drawBackground(mech->block->row);
             }

             for (_LogInventoryItem* item = globalLogPtr->purchaseComponents->items; item != nullptr; item = item->next)
             {
                 item->count = 2;
                 item->purchaseBlock->drawBackground(item->purchaseBlock->row, item->masterID);
             }

             for (PurVehicle* vehicle = globalLogPtr->purVehicleList->first; vehicle != nullptr;
                  vehicle = vehicle->next)
             {
                 vehicle->data->numAvailable = 2;
                 vehicle->block->drawBackground(vehicle->block->row);
             }

             Click(100, 60);
         },
         0x7949d30bu},
        {"purchase, buy drag", [] { Drag(300, 80, 100, 200); }, 0x829d115au},
        {"purchase, quantity up", [] { Click(378, 249); }, 0x58349157u},
        {"purchase, accept", [] { Click(316, 299); }, 0x3ff329e7u},
        {"purchase, inventory row", [] { Click(100, 120); }, 0xfdd71060u},
        {"purchase, variant", [] { Click(393, 32); }, 0xcccc0558u},
        {"purchase, scroll", [] { Click(631, 474); }, 0x112ee248u},
        {"purchase, too poor",
         []
         {
             ResourcePoints = 0;
             Drag(300, 80, 100, 200);
         },
         0x25dfbc93u},
        {"purchase, message ok", [] { Click(366, 283); }, 0x8ec3109bu},
        {"purchase, inventory info", [] { Click(60, 165); }, 0xfda8bafbu},
        {"purchase, pilots tab", [] { Click(202, 180); }, 0x4d9d3575u},
        {"purchase, components tab",
         []
         {
             ResourcePoints = 100000;
             Click(202, 265);
         },
         0xe76ce289u},
        {"purchase, component drag", [] { Drag(300, 80, 100, 200); }, 0xe85a82e2u},
        {"purchase, component accept", [] { Click(316, 299); }, 0xb8424311u},
        {"purchase, vehicles tab", [] { Click(202, 335); }, 0x95c3c146u},
        {"purchase, vehicle drag", [] { Drag(300, 80, 100, 200); }, 0xb60a5029u},
        {"purchase, vehicle accept", [] { Click(316, 299); }, 0x441a706du},
        {"purchase, mechs tab", [] { Click(202, 125); }, 0xdc78c644u},
        {"repair again", [] { Click(100, 78); }, 0xa345ababu},
        {"repair, payload", [] { Click(560, 60); }, 0xe9a831a2u},
        {"repair, second mech", [] { Click(300, 170); }, 0x440f6f6eu},
        {"repair, component info", [] { SendMouse(7, 100, 160, false); }, 0x686410c7u},
        {"repair, weapon info", [] { SendMouse(7, 560, 175, false); }, 0xb84e5197u},
        {"repair, mech info", [] { SendMouse(7, 300, 200, false); }, 0xcc3e106du},
        {"repair, weapon off", [] { Drag(560, 175, 100, 200); }, 0x40a5652du},
        {"repair, mount laser", [] { Drag(100, 160, 300, 190); }, 0xdfeac413u},
        {"repair, pilots tab", [] { Click(202, 180); }, 0xea90325bu},
        {"repair, pilot info", [] { SendMouse(7, 100, 140, false); }, 0xb5025f17u},
        {"repair, vehicles tab", [] { Click(202, 335); }, 0xabf60696u},
        {"repair, mechs tab", [] { Click(202, 125); }, 0xb51cd8ddu},
        {"repair, mech row info", [] { SendMouse(7, 100, 140, false); }, 0xceec3967u},
        {"repair, mech into force", [] { Drag(100, 140, 300, 300); }, 0x9afd98a2u},
        {"repair, select first", [] { Click(300, 60); }, 0x0754fe4cu},
        {"repair, mech out", [] { Drag(260, 170, 100, 200); }, 0xed7346bcu},
        {"repair, weapon list scroll", [] { Click(560, 125); }, 0xe5740f69u},
        {"repair, first mech out", [] { Drag(260, 60, 100, 200); }, 0x36d78e5cu},
        {"briefing after repair", [] { globalLogPtr->setUpBriefingScreen(-1); }, 0xad84942du},
        {"briefing, unit out of slot", [] { Drag(300, 60, 60, 400); }, 0x578b67a0u},
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

    SaveShot("logistics every frame", frames);
    CHECK_EQ(frames, 0x16f4b349u);
}
