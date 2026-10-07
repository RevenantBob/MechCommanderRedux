#include "stdafx.h"
#include "sprite/lactor.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/team.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>The jump gesture.</summary>
    constexpr int32_t GESTURE_JUMP = 2;

    /// <summary>The facing of <paramref name="obj"/> in degrees, negative to the right.</summary>
    auto ObjectFacing(MCGameObject* obj) -> float
    {
        const MCFrameOfRef frame = obj->GetFrame();
        float cosFacing = UnitX.X * frame.I.X + UnitX.Y * frame.I.Y + UnitX.Z * frame.I.Z;

        if (cosFacing < -1.0)
        {
            cosFacing = -1.0f;
        }

        if (cosFacing > 1.0)
        {
            cosFacing = 1.0f;
        }

        double facing = AcosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

        if (frame.I.Y < 0.0)
        {
            facing = -facing;
        }

        return static_cast<float>(facing);
    }
}

auto MCElementalActor::GetNumFramesInGesture(uint32_t gesture) -> float
{
    // Port fix: the original tests numGestures < gesture, so gesture == numGestures reads past the table.
    if (AppearType->NumGestures <= gesture)
    {
        return 0.0f;
    }

    return static_cast<float>(AppearType->Gestures[gesture].NumFrames);
}

auto MCElementalActor::GetVelocityOfGesture(uint32_t gesture) -> float
{
    // Port fix: as in getNumFramesInGesture.
    if (AppearType->NumGestures <= gesture)
    {
        return 0.0f;
    }

    return AppearType->Gestures[gesture].Velocity;
}

auto MCElementalActor::PreloadGestures(int32_t gesture, float preloadRotation) -> void
{
    AppearType->PreloadGestures(gesture, preloadRotation);
}

auto MCElementalActor::SetGestureGoal(int32_t goal) -> int32_t
{
    if (GoalPending != 0)
    {
        return static_cast<int32_t>(0xeadd0005);
    }

    if (goal == GESTURE_JUMP)
    {
        if (JumpSetup == 0)
        {
            return static_cast<int32_t>(0xeadd0006);
        }
    }
    else if (goal < 0)
    {
        return static_cast<int32_t>(0xeadd0003);
    }

    if (static_cast<int32_t>(AppearType->NumGestures) < goal)
    {
        return static_cast<int32_t>(0xeadd0003);
    }

    GestureGoal = goal;
    GoalPending = 1;
    return 0;
}

auto MCElementalActor::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    AppearType = static_cast<MCElementalTree*>(tree);

    if (tree != nullptr)
    {
        tree->AddUsers(this);
    }

    Visible = 0;
    FadeTableIndex = -1;
    CurrentFrame = -1;
    ShapeMinY = -15.0f;
    ShapeMinX = -15.0f;
    CurrentShape = nullptr;
    CurrentTime = 0.0f;
    LastFrame = 0;
    Velocity = 0.0f;
    GoalPending = 0;
    CurrentGesture = 0;
    OldGesture = 0;
    Jumping = 0;
    JumpSetup = 0;
    InView = 0;
    FrameRate = 15.0f;
    VelocityPercentage = 1.0f;
    ShapeMaxY = 30.0f;
    ShapeMaxX = 30.0f;
    return 0;
}

auto MCElementalActor::SetJumpParameters(float jumpDistance) -> int32_t
{
    if (Jumping != 0)
    {
        return static_cast<int32_t>(0xeadd0007);
    }

    GestureGoal = GESTURE_JUMP;
    JumpSetup = 1;
    // Cover the distance in the jump gesture's time.
    const MCElementalGestureData& jump = AppearType->Gestures[GESTURE_JUMP];
    Velocity = jumpDistance / (static_cast<float>(jump.NumFrames) / jump.FrameRate);
    return 0;
}

auto MCElementalActor::GetVelocityMagnitude() -> float
{
    if (Jumping == 0 && JumpSetup == 0)
    {
        Velocity = AppearType->Gestures[CurrentGesture].Velocity;
    }

    return Velocity;
}

auto MCElementalActor::SetVelocityPercentage(float percent) -> void
{
    // Keep the animation at the same frame when the speed changes.
    if (percent != VelocityPercentage)
    {
        CurrentTime = (VelocityPercentage * CurrentTime * FrameRate) / (percent * FrameRate);
    }

    VelocityPercentage = percent;
}

auto MCElementalActor::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    const MCVector2D pos = Owner->GetScreenPos(cam->CameraId - 1);
    UpperLeft.X = pos.X;
    UpperLeft.Y = pos.Y;
    LowerRight.Y = pos.Y;
    LowerRight.X = pos.X;

    // The shape's bounds are taken once, from the first shape seen.
    if (CurrentShape != nullptr && CurrentShape->FrameList != nullptr && InView == 0)
    {
        uint8_t* shapeTable = CurrentShape->FrameList;
        int32_t frame = CurrentFrame;

        if (frame < 0)
        {
            frame = 0;
        }

        const int32_t numShapeFrames = VfxShapeCount(shapeTable);

        if (numShapeFrames <= frame)
        {
            frame = numShapeFrames - 1;
        }

        const int32_t minXY = VfxShapeMinxy(shapeTable, frame);

        if (static_cast<float>(minXY >> 16) < ShapeMinX)
        {
            ShapeMinX = static_cast<float>(minXY >> 16);
        }

        if (static_cast<float>(static_cast<int16_t>(minXY)) < ShapeMinY)
        {
            ShapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
        }

        const int32_t size = VfxShapeResolution(shapeTable, frame);

        if (ShapeMaxX < static_cast<float>(size >> 16))
        {
            ShapeMaxX = static_cast<float>(size >> 16);
        }

        if (ShapeMaxY < static_cast<float>(static_cast<int16_t>(size)))
        {
            ShapeMaxY = static_cast<float>(static_cast<int16_t>(size));
        }

        InView = 1;
    }

    const float scale = cam->CameraScale == 1 ? 0.5f : 1.0f;
    UpperLeft.X = scale * ShapeMinX + pos.X;
    UpperLeft.Y = scale * ShapeMinY + pos.Y;
    LowerRight.X = scale * ShapeMaxX + UpperLeft.X;
    LowerRight.Y = scale * ShapeMaxY + UpperLeft.Y;

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y &&
        UpperLeft.X <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewWidth))) &&
        UpperLeft.Y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewHeight))))
    {
        return 1;
    }

    return 0;
}

auto MCElementalActor::Render(int32_t depthFixup) -> int32_t
{
    MCGameObject* obj = Owner;
    ScreenPos = obj->GetScreenPos(Eye->CameraId - 1);
    const float facing = ObjectFacing(obj);
    CurrentShape = AppearType->GetGesture(CurrentGesture, facing, FrameRate, Visible);
    RecalcBounds(Eye);
    ElementList->OpenGroup(static_cast<int32_t>(static_cast<float>(depthFixup) - ScreenPos.Y), 1);

    if (DrawTerrainGrid != 0)
    {
        DrawSelectBox(0xff);
    }

    if (CurrentShape == nullptr || CurrentShape->FrameList == nullptr)
    {
        return 0;
    }

    uint8_t* fadeTable = nullptr;

    if (FadeTableIndex != -1 && FadeTableIndex >= 0)
    {
        fadeTable = GamePalette->FadePalettes.get() + (FadeTableIndex + GamePalette->NumBitmapHazeLevels * 2) * 0x100;
    }

    if (CurrentFrame < 0)
    {
        CurrentFrame = 0;
    }

    ElementList->Add(MCElementPool::Make<MCVfxElement>(CurrentShape->FrameList, ScreenPos.X, ScreenPos.Y, CurrentFrame,
                                                       0, fadeTable, 1, 0));

    // Selection: -1 and 1 draw the bars, 2 the brackets in the owner's alignment colour.
    const int32_t selected = Owner->Selected;

    if (selected == -1 || selected == 1)
    {
        RecalcBounds(Eye);
        DrawBars();
    }
    else if (selected == 2)
    {
        RecalcBounds(Eye);
        MCGameObject* selectedObj = Owner;
        const int32_t alignment = selectedObj->GetAlignment();

        if (alignment == -1)
        {
            DrawSelectBrackets(0xfd);
        }
        else if (alignment == 0)
        {
            DrawSelectBrackets(0xfe);
        }
        else if (alignment == 1)
        {
            DrawSelectBrackets(selectedObj->GetAlignment() == HomeTeam->Alignment ? 0xfc : 0xfb);
        }
    }

    return 0;
}

auto MCElementalActor::Update() -> int32_t
{
    int32_t gesture = CurrentGesture;
    GoalPending = 0;

    if (GestureGoal != gesture)
    {
        gesture = GestureGoal;
        CurrentGesture = gesture;
        OldGesture = gesture;
        JumpSetup = 0;
        Jumping = gesture == GESTURE_JUMP ? 1 : 0;

        if (Owner->IsDisabled() == 0)
        {
            Owner->Status = 0;
        }

        CurrentFrame = 0;
        // Keep the animation time consistent with the new gesture's rate.
        const float newRate = std::fabs(AppearType->Gestures[gesture].FrameRate);

        if (newRate != FrameRate || VelocityPercentage != 1.0)
        {
            CurrentTime = (FrameRate * CurrentTime) / (VelocityPercentage * newRate);
        }
    }

    FrameRate = AppearType->Gestures[gesture].FrameRate;

    if (FrameRate < 0.0)
    {
        FrameRate = -FrameRate;
    }

    AppearType->SetGesture(gesture, ObjectFacing(Owner), FrameRate);

    if (CurrentFrame == -1)
    {
        CurrentFrame = 0;
        return 1;
    }

    FrameRate = VelocityPercentage * FrameRate;
    CurrentTime = FrameLength + CurrentTime;
    const double frames = static_cast<double>(CurrentTime * FrameRate);
    const int32_t wholeFrames = static_cast<int32_t>(std::floor(frames));

    if (LastFrame < wholeFrames)
    {
        const int32_t played = LastFrame;
        LastFrame = wholeFrames;
        const int32_t advanced = wholeFrames - played;
        // The original also let a flag (always 1) stop the jump gesture from animating.
        const int32_t current = CurrentGesture;

        if (advanced != 0)
        {
            const uint32_t frame = static_cast<uint32_t>(CurrentFrame + advanced);
            CurrentFrame = static_cast<int32_t>(frame);
            const uint32_t numFrames = AppearType->Gestures[current].NumFrames;

            if (static_cast<int32_t>(numFrames) <= static_cast<int32_t>(frame))
            {
                CurrentFrame = static_cast<int32_t>(frame % numFrames);

                if (current == GESTURE_JUMP)
                {
                    // The jump is over: back to gesture 0.
                    SetGestureGoal(0);
                    const int32_t goal = GestureGoal;
                    CurrentGesture = goal;
                    OldGesture = goal;
                    Jumping = 0;
                    const float newRate = std::fabs(AppearType->Gestures[goal].FrameRate);

                    if (newRate != FrameRate || VelocityPercentage != 1.0)
                    {
                        CurrentTime = (FrameRate * CurrentTime) / (VelocityPercentage * newRate);
                    }

                    CurrentFrame = 0;
                }
            }
        }
    }

    return 1;
}

auto MCElementalActor::Destroy() -> void
{
    AppearType->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(AppearType);
}

auto MCElementalActor::DrawBars() -> void
{
}
