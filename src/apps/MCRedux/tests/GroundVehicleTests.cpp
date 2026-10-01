#include "stdafx.h"
#include "MCTest.h"
#include "object/gvehicl.h"

/// <summary>
/// In MechCommander Gold no tile or overlay slows a ground vehicle: every throttle multiplier, for every chassis, is
/// 1.0 in MCX.EXE's data. A zero here pins every vehicle's throttle at 0 (the vehicles never move).
/// </summary>
TEST_CASE("gvehicl: terrain throttle multipliers leave the throttle whole")
{
    for (int32_t chassis = 0; chassis < 3; chassis++)
    {
        for (int32_t tile = 0; tile < NUM_THROTTLE_TILE_TYPES; tile++)
        {
            MCTest::Scope scope("chassis " + std::to_string(chassis) + " tile " + std::to_string(tile));
            CHECK_EQ(TileThrottleMultiplier[chassis][tile], 1.0f);
        }

        for (int32_t overlay = 0; overlay < NUM_THROTTLE_OVERLAY_TYPES; overlay++)
        {
            MCTest::Scope scope("chassis " + std::to_string(chassis) + " overlay " + std::to_string(overlay));
            CHECK_EQ(OverlayThrottleMultiplier[chassis][overlay], 1.0f);
        }
    }
}
