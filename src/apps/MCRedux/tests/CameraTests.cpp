#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "camera/MCMainWindow.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "terrain/MCTerrainWindow.h"

/// <summary>
/// A mission's cameras belong to the camera list. The eye is its current camera (camera 1 in mission 1), the main
/// view shows that camera and refers back to it, the camera draws through one of the terrain's windows, and outside a
/// render everything draws into the screen's frame.
/// </summary>
TEST_CASE_ISOLATED("game: the eye is the mission's current camera, shown by the main view")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    const std::shared_ptr<MCCameraList> list = CameraList();
    REQUIRE(list != nullptr);
    const std::shared_ptr<MCCamera> eye = Eye();
    REQUIRE(eye != nullptr);
    CHECK(eye == list->CurrentCamera.lock());
    CHECK(eye == list->FindCameraFromIDNumber(1));
    CHECK(eye == ActiveMainCamera());
    CHECK(list->FindCameraFromIDNumber(99) == nullptr);

    const std::shared_ptr<MCViewWindow> view = MCMainView();
    REQUIRE(view != nullptr);
    CHECK(view == eye->View());
    CHECK(view->GetCamera() == eye);
    CHECK(MainHolder() == list->MainHolder());
    CHECK(MainHolder() != nullptr);

    CHECK(!eye->TerrainWindow.expired());
    CHECK(GlobalPane == ScreenPort()->SharedFrame());

    // A render makes the view's world surface the global pane for its length only.
    MCTestGame::RunFrame(1.0f / 15.0f);
    CHECK(GlobalPane == ScreenPort()->SharedFrame());
    CHECK(Eye() == eye);
}

/// <summary>
/// A camera that follows a mover lets go of it once the mover leaves the world: the reference reads as none from then
/// on (the original kept a pointer to the freed object, OB-170), and the camera keeps its place.
/// </summary>
TEST_CASE_ISOLATED("game: a camera following a mover lets go of it when the mover leaves the world")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    const std::shared_ptr<MCCamera> eye = Eye();
    REQUIRE(eye != nullptr);
    const std::shared_ptr<MCMover> mover = GetMoverFromPartId(0x200);
    REQUIRE(mover != nullptr);

    eye->ChangeTarget(mover, true);
    CHECK(LockObject(eye->TargetObject) == mover);
    CHECK(eye->CameraClass == MCCameraClass::Object);
    CHECK_EQ(eye->Position.X, mover->GetPosition().X);
    CHECK_EQ(eye->Position.Y, mover->GetPosition().Y);

    mover->MarkRemoved();
    CHECK(LockObject(eye->TargetObject) == nullptr);
    const MCVector3D before = eye->Position;
    eye->Update();
    CHECK_EQ(eye->Position.X, before.X);
    CHECK_EQ(eye->Position.Y, before.Y);

    // Scrolling no longer drops a target first: there is none.
    eye->ScrollCamera(10, 0);
    CHECK_EQ(eye->Position.X, before.X + 10.0f);
    CHECK_EQ(eye->Position.Y, before.Y + 10.0f);
}
