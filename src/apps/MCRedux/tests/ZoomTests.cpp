#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "appear/MCAppearance.h"
#include "camera/MCCamera.h"
#include "object/mover.h"

/// <summary>
/// While the zoom eases, units stay where the terrain is: each frame, the screen position a mover took in its update
/// is the one the camera projects it to when the frame is drawn. (The view's size used to change at the draw, after
/// the units had placed themselves with the last frame's size, so they wobbled until the zoom settled.)
/// </summary>
TEST_CASE_ISOLATED("game: units keep their place on the ground while the zoom eases")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    MCViewWindow* view = MCMainView();
    REQUIRE(view != nullptr);
    MCMover* mover = GetMoverFromPartId(0x200);
    REQUIRE(mover != nullptr);
    REQUIRE(mover->GetAppearance() != nullptr);

    MCFixedZoomHeight = 0.0f;
    REQUIRE(view->ZoomTo(800.0f));
    int32_t easedFrames = 0;

    for (int32_t frame = 0; frame < 30 && view->ZoomHeight != view->ZoomTarget; frame++)
    {
        MCTest::Scope scope(std::format("frame {}, {:.1f} lines", frame, view->ZoomHeight));
        MCTestGame::RunFrame(1.0f / 15.0f);
        const MCVector2D placed = mover->GetScreenPos(0);
        const MCVector2D projected = mover->GetAppearance()->GetScreenPos(view->Camera);
        CHECK(std::fabs(placed.X - projected.X) <= 1.0f);
        CHECK(std::fabs(placed.Y - projected.Y) <= 1.0f);
        easedFrames++;
    }

    // The zoom took several frames to get there, and got there.
    CHECK(easedFrames > 2);
    CHECK_EQ(view->ZoomHeight, 800.0f);
}
