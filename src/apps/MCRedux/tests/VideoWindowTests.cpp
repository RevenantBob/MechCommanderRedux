#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "object/mover.h"
#include "object/warrior.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

using namespace MCScreenInput;

/// <summary>
/// The tactical map's video window shows the idle picture, and while a pilot speaks on the radio the pilot's name over
/// it, with a line from the window to the pilot's unit on the map. Recorded on the code before the window drew itself
/// each frame (renderer phase 3, step 4).
/// </summary>
TEST_CASE_ISOLATED("game: the tactical map's video window matches the pre-renderer frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    MCTacticalMap* map = MCTerrain::TerrainTacticalMap;
    REQUIRE(map != nullptr);
    REQUIRE(map->VideoWindow != nullptr);
    MCMover* mover = GetMoverFromPartId(0x200);
    REQUIRE(mover != nullptr);
    MCMechWarrior* pilot = mover->GetPilot();
    REQUIRE(pilot != nullptr);

    uint32_t presents = 0x811c9dc5;
    REQUIRE(MCInput::Display() != nullptr);
    MCTestGame::OnPresent = [&] { presents = (presents ^ ScreenHash()) * 0x01000193; };

    const auto settle = [](const char* name)
    {
        for (int32_t frame = 0; frame < 10; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }

        const uint32_t hash = ScreenHash();
        SaveShot(std::string("video ") + name, hash);
        return hash;
    };

    CHECK_EQ(settle("idle"), 0xe4b58559u);
    map->VideoWindow->SetStar(pilot);
    CHECK_EQ(settle("speaking"), 0x566829a5u);
    map->VideoWindow->SetStar(nullptr);
    CHECK_EQ(settle("done"), 0xd9566f22u);
    MCTestGame::OnPresent = nullptr;
    CHECK_EQ(presents, 0x861a964eu);
}
