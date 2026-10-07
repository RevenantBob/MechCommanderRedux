#include "stdafx.h"
#include "sprite/mactor.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "camera/camera.h"
#include "color/color.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "engine/cellip.h"
#include "engine/cepoly.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/team.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"
#include "terrain/terrain.h"
#include "vfx/vfxfuncs.h"
#include "platform/MCRenderer.h"

MCPacketFile* MCMechActor::Shadows = nullptr;
std::vector<std::unique_ptr<uint8_t[]>> MCMechActor::ShadowShapes;
int32_t MCMechActor::NumShadows = 0;

char HotSpotFinderArray[28] = {0,  0,  0,  1,  2,  3,  4,  5,  6,  2,  3,  7, 8,  9,
                               10, 11, 12, 13, 14, 15, 16, 17, 18, 11, 10, 0, 19, 19};

char TransitionArray[0x32a] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  2,  -1, -1, -1, -1, -1, -1, -1, -1, 1,  2,  3,  4,  -1, -1, -1, -1, -1,
    -1, 1,  2,  3,  4,  6,  7,  -1, -1, -1, -1, 1,  2,  10, 9,  -1, -1, -1, -1, -1, -1, 1,  2,  3,  11, -1, -1, -1, -1,
    -1, -1, 1,  2,  20, 2,  -1, -1, -1, -1, -1, -1, 19, 15, 23, -1, -1, -1, -1, -1, -1, -1, 18, 14, 24, -1, -1, -1, -1,
    -1, -1, -1, 1,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,  -1, -1, -1, -1,
    -1, -1, -1, -1, 3,  4,  6,  7,  -1, -1, -1, -1, -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  11, -1, -1, -1,
    -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 19, 15, 23, -1, -1, -1, -1, -1, -1, -1, 18, 14, 24, -1,
    -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, 5,  10, 9,  -1, -1, -1, -1, -1, -1, -1, 11, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 15, 23, -1, -1, -1, -1, -1, -1, -1, -1, 14,
    24, -1, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
    4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1, -1,
    -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 17, 15, 23, -1, -1, -1, -1, -1,
    -1, -1, 16, 14, 24, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 2,  3,  4,  -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, 3,  11, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  15, 23, -1, -1,
    -1, -1, -1, -1, -1, 3,  14, 24, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1,
    -1, -1, -1, -1, -1, -1, 4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, 10, 9,  -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 15, 23,
    -1, -1, -1, -1, -1, -1, -1, -1, 14, 24, -1, -1, -1, -1, -1, -1, -1, -1, 1,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,  6,  7,  -1, -1, -1, -1, -1, -1,
    10, 9,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, 1,  0,  -1, -1, -1, -1, -1,
    -1, -1, 22, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 22, 3,  4,  -1, -1, -1, -1, -1, -1, -1, 22, 3,  4,  6,  7,  -1, -1,
    -1, -1, -1, 22, 10, 9,  -1, -1, -1, -1, -1, -1, -1, 22, 3,  11, -1, -1, -1, -1, -1, -1, -1, 22, 20, 2,  -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 21, 22, 1,  0,  -1,
    -1, -1, -1, -1, -1, 21, 22, 2,  -1, -1, -1, -1, -1, -1, -1, 21, 22, 3,  4,  -1, -1, -1, -1, -1, -1, 21, 22, 3,  4,
    6,  7,  -1, -1, -1, -1, 21, 22, 10, 9,  -1, -1, -1, -1, -1, -1, 21, 22, 3,  11, -1, -1, -1, -1, -1, -1, 21, 22, 20,
    2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

int32_t EquivalentGestureArray[10] = {0, -1, 1, -1, 2, -1, -1, 3, -1, 0};
int SingleStepMode = 1;
int32_t MechElements = 0;

namespace
{
    /// <summary>The gestures (the tree's gesture numbers).</summary>
    constexpr int32_t GESTURE_STAND = 2;
    constexpr int32_t GESTURE_JUMP = 20;
    constexpr int32_t GESTURE_FALL = 21;
    constexpr int32_t GESTURE_COMBAT = 0x1a;
    constexpr int32_t GESTURE_COMBAT_CHANGE = 0x1b;

    /// <summary>The number of gestures in a state's row of the transition table.</summary>
    constexpr int32_t TRANSITION_ROW = 10;

    /// <summary>Half a turn: MCX.EXE's constant for pi (a little short of it).</summary>
    constexpr double TURN_ANGLE = 3.1415926535820002;

    /// <summary>The facing of <paramref name="obj"/> in degrees, negative to the right.</summary>
    auto ExactObjectFacing(MCGameObject* obj) -> double
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

        return facing;
    }

    auto ObjectFacing(MCGameObject* obj) -> float
    {
        return static_cast<float>(ExactObjectFacing(obj));
    }

    /// <summary>Wraps <paramref name="rotation"/> into -180..180 (one step each way).</summary>
    auto WrapRotation(float rotation) -> float
    {
        if (rotation > 180.0)
        {
            return (rotation - 180.0f) - 180.0f;
        }

        if (rotation < -180.0)
        {
            return rotation + 180.0f + 180.0f;
        }

        return rotation;
    }

    /// <summary>0.5 when the eye is zoomed out, else 1.</summary>
    auto EyeScale() -> float
    {
        return Eye->CameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>Turns <paramref name="mech"/>'s frame half a turn about its up axis (the reversed gestures end
    /// facing the other way) and clears its torso and arm rotations.</summary>
    auto TurnAround(MCBattleMech* mech) -> void
    {
        MCFrameOfRef frame = mech->GetFrame();
        const float s = static_cast<float>(std::sin(TURN_ANGLE));
        const float c = static_cast<float>(std::cos(TURN_ANGLE));
        const MCVector3D oldI = frame.I;
        frame.I.X = frame.J.X * s + oldI.X * c;
        frame.I.Y = frame.J.Y * s + oldI.Y * c;
        frame.I.Z = frame.J.Z * s + oldI.Z * c;
        frame.J.X = frame.J.X * c - oldI.X * s;
        frame.J.Y = frame.J.Y * c - oldI.Y * s;
        frame.J.Z = frame.J.Z * c - oldI.Z * s;
        mech->SetFrame(frame);
    }

    /// <summary>Clears the mech's torso and arm rotations.</summary>
    auto ClearRotations(MCBattleMech* mech) -> void
    {
        mech->LeftArmRotation = 0.0f;
        mech->RightArmRotation = 0.0f;
        mech->TorsoRotation = 0.0f;
    }

    /// <summary>The outline table of <paramref name="mech"/> for gesture <paramref name="gesture"/> (the jump and
    /// fall heights), or null.</summary>
    auto GestureHeights(MCBattleMech* mech, int32_t gesture) -> float*
    {
        auto* type = static_cast<MCBattleMechType*>(mech->GetObjectType());

        if (type->GestureOutlines == nullptr)
        {
            return nullptr;
        }

        return reinterpret_cast<float*>(type->GestureOutlines[static_cast<uint8_t>(HotSpotFinderArray[gesture])]);
    }

    /// <summary>Adds the line from <paramref name="start"/> to <paramref name="end"/> in <paramref name="color"/>.</summary>
    auto AddLine(MCVector2D start, MCVector2D end, int32_t color) -> void
    {
        ElementList->Add(MCElementPool::Make<MCLineElement>(start, end, color, nullptr, -50000, -1));
    }
}

//---------------------------------------------------------------------------
// MechActor
//---------------------------------------------------------------------------

auto MCMechActor::HitMech(int32_t) -> int
{
    return 0;
}

auto MCMechActor::SetGesture(uint32_t gesture) -> int32_t
{
    if (GestureSet != 0)
    {
        return static_cast<int32_t>(0xeade0004);
    }

    GestureSet = 1;
    CurrentGesture = static_cast<int32_t>(gesture);
    CurrentStateGesture = EquivalentGestureArray[gesture];
    return 0;
}

auto MCMechActor::GetNumFramesInGesture(uint32_t gesture) -> float
{
    return static_cast<float>(MechTree->Gestures[gesture].NumFrames);
}

auto MCMechActor::GetVelocityOfGesture(uint32_t gesture) -> float
{
    if (static_cast<int32_t>(gesture) < static_cast<int32_t>(MechTree->TreeInfo->NumGestures))
    {
        return MechTree->Gestures[gesture].StartVelocity;
    }

    return -1.0f;
}

auto MCMechActor::GetHotSpotIndex(uint32_t location) -> uint32_t
{
    return static_cast<uint32_t>(static_cast<int32_t>(HotSpotFinderArray[location]));
}

auto MCMechActor::PreloadGestures(int32_t gesture, float rotation) -> void
{
    MechTree->PreloadGestures(gesture, rotation);
}

auto MCMechActor::SetGestureGoal(int32_t goal) -> int32_t
{
    if (GoalPending != 0)
    {
        return static_cast<int32_t>(0xeada0005);
    }

    const int32_t state = CurrentStateGesture;

    if (state == goal)
    {
        return static_cast<int32_t>(0xeade0001);
    }

    if (InTransition != 0)
    {
        return static_cast<int32_t>(0xeade0002);
    }

    if (goal == 6)
    {
        if (JumpSetup == 0)
        {
            return static_cast<int32_t>(0xeada0006);
        }
    }
    else if (goal < 0 || goal > 8)
    {
        return static_cast<int32_t>(0xeade0003);
    }

    // Running backwards (the torso turned past the side) is walking backwards for mirrored gestures.
    auto* mech = static_cast<MCBattleMech*>(Owner);
    const double facing = ExactObjectFacing(mech) + mech->TorsoRotation;

    if (!(facing >= 0.0f) || facing >= 180.0)
    {
        const MCGestureData& data = MechTree->Gestures[goal];

        if ((data.Symmetrical != 0 || data.ArmSymmetrical != 0) && goal == 8)
        {
            goal = 7;
        }
    }

    TransitionStep = 0;

    // Faithful: a leftover test of the transition table index that can't fail.
    if ((goal + state * 9) * 10 == -1)
    {
        return static_cast<int32_t>(0xeade0003);
    }

    GestureGoal = goal;
    GoalPending = 1;
    return 0;
}

auto MCMechActor::Init(MCAppearanceType* tree, MCGameObject* obj) -> int32_t
{
    Visible = 0;
    Owner = obj;
    MechTree = static_cast<MCSpriteTree*>(tree);

    if (tree != nullptr)
    {
        tree->AddUsers(this);
    }

    RightArmGone = 0;
    LeftArmGone = 0;
    GestureSet = 0;
    FadeTableIndex = -1;
    Wrecked = 0;

    // The shadow shapes are loaded once, for every mech.
    if (ShadowShapes.empty())
    {
        ShadowShapes.resize(0x80);
        Shadows = new MCPacketFile;

        if (Shadows != nullptr)
        {
            MCFullPathFileName shadowName;
            shadowName.Init(SpritePath, "shadow", ".pak");
            int32_t result = Shadows->Open(shadowName, READ, 50);

            if (result != 0)
            {
                MCFullPathFileName cdName;
                cdName.Init(CDspritePath, "shadow", ".pak");
                result = Shadows->Open(cdName, READ, 50);
            }

            if (result == 0)
            {
                if (Shadows->GetNumPackets() > 0x7f)
                {
                    Fatal(-1, " Too Many shadow Shapes ", nullptr);
                }

                NumShadows = Shadows->GetNumPackets();

                for (int32_t i = 0; i < Shadows->GetNumPackets(); i++)
                {
                    Shadows->SeekPacket(i);
                    const int32_t size = Shadows->GetPacketSize();
                    ShadowShapes[i] = std::make_unique<uint8_t[]>(static_cast<size_t>(size));
                    Shadows->ReadPacket(i, ShadowShapes[i].get());
                    MCRenderer::RegisterData(ShadowShapes[i].get(), static_cast<size_t>(size), MCDataKind::Shapes);
                }

                Shadows->Close();
            }
            else
            {
                NumShadows = 0;
            }
        }

        delete Shadows;
        Shadows = nullptr;
    }

    for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
    {
        PartShape[i] = nullptr;
        CurrentFrame[i] = -1;
        CurrentTime[i] = 0.0f;
        LastFrame[i] = 0;
        FrameRate[i] = 15.0f;
        PartOrder[i] = 0;
    }

    Visible = 0;
    ShapeMinY = -25.0f;
    ShapeMinX = -25.0f;
    JumpVelocity = 0.0f;
    GoalPending = 0;
    CurrentGesture = 0;
    GestureGoal = -1;
    CurrentStateGesture = 0;
    InTransition = 0;
    GestureDone = 0;
    NextGesture = -1;
    PlayBackwards = 0;
    LyingStill = 0;
    FallTurnPending = 0;
    StandTurnPending = 0;
    FrameHeights = nullptr;
    InJump = 0;
    JumpSetup = 0;
    JumpGoal.Z = 0.0f;
    JumpGoal.Y = 0.0f;
    JumpGoal.X = 0.0f;
    JumpParameter = 0.0f;
    JumpSpeed = 0.0f;
    Airborne = 0;
    BodyTurnLocked = 0;
    UpperBodyLocked = 0;
    ShapeMaxY = 25.0f;
    ShapeMaxX = 25.0f;
    InView = 0;
    StopCountdown = 0.0f;
    InCombatMode = 0;
    CombatModeRaising = 0;
    CombatModeLowering = 0;
    return 0;
}

auto MCMechActor::SetCombatMode(int combatMode) -> void
{
    int32_t gesture;

    if (combatMode == 0)
    {
        if (InCombatMode == 0)
        {
            return;
        }

        if (CombatModeLowering == 0)
        {
            InCombatMode = 0;
            CombatModeLowering = 1;
            CombatModeChanged = 1;
        }

        gesture = CurrentGesture;

        if (gesture < 2)
        {
            CombatModeLowering = 0;
            InCombatMode = 0;
            CombatModeChanged = 0;
            return;
        }
    }
    else
    {
        if (InCombatMode != 0)
        {
            return;
        }

        gesture = CurrentGesture;

        if (gesture < 2 || gesture > 11)
        {
            return;
        }

        // Only from the stand, walk and run gestures the mech has a gun pose for.
        const MCMechSpecialInfo& info = *MechTree->SpecialInfo;

        if (info.StandToGunPose == 0 && gesture == 2)
        {
            return;
        }

        if (info.WalkToGunPose == 0 && gesture > 2 && gesture < 6)
        {
            return;
        }

        if (gesture > 8)
        {
            return;
        }

        if (info.RunToGunPose == 0 && gesture > 5 && gesture < 9)
        {
            return;
        }

        if (CombatModeRaising == 0)
        {
            CombatModeRaising = 1;
            CombatModeChanged = 1;
        }
    }

    if (gesture < 12)
    {
        return;
    }

    CombatModeLowering = 0;
    InCombatMode = 0;
    CombatModeChanged = 0;
}

auto MCMechActor::SetJumpParameters(MCVector3D& goal, int) -> int32_t
{
    if (InJump != 0)
    {
        return static_cast<int32_t>(0xeada0007);
    }

    JumpParameter = 4.0f;
    JumpSetup = 1;
    JumpGoal = goal;
    return 0;
}

auto MCMechActor::GetVelocityMagnitude() -> float
{
    const int32_t gesture = CurrentGesture;

    if (gesture == GESTURE_JUMP || gesture == 12 || gesture == 13)
    {
        return JumpVelocity;
    }

    const int32_t numGestures = MechTree->TreeInfo->NumGestures;
    const float startVelocity = gesture < numGestures ? MechTree->Gestures[gesture].StartVelocity : -1.0f;
    const float endVelocity = gesture < numGestures ? MechTree->Gestures[gesture].EndVelocity : -1.0f;
    float velocity = 0.0f;

    if ((gesture < 14 || gesture > 19) && (velocity = startVelocity, startVelocity != endVelocity))
    {
        // Ease from the start velocity to the end one over the gesture.
        if (startVelocity >= -1999.0 && endVelocity >= -1999.0)
        {
            const uint32_t numFrames = MechTree->Gestures[gesture].NumFrames;
            const double framesLeft =
                static_cast<double>(numFrames - static_cast<uint32_t>(CurrentFrame[MECH_PART_LEGS]));
            const double t = 1.0 - framesLeft / static_cast<double>(static_cast<int32_t>(numFrames));
            return static_cast<float>((static_cast<double>(endVelocity) - static_cast<double>(startVelocity)) * t +
                                      startVelocity);
        }
    }
    else if (velocity >= -1999.0)
    {
        return velocity;
    }

    return 0.0f;
}

auto MCMechActor::SetMovePath(MCMovePath* path) -> int32_t
{
    if (path == nullptr)
    {
        StopCountdown = 0.0f;
        return 0;
    }

    if (path->NumSteps != 0 && path->CurStep < path->NumSteps)
    {
        StopCountdown = 5.0f;
        return 0;
    }

    StopCountdown = 0.0f;
    return 0;
}

auto MCMechActor::ForceStop() -> void
{
    StopCountdown = 0.0f;
}

auto MCMechActor::CheckStop() -> int
{
    if (StopCountdown == 0.0 && CurrentGesture > 2 && CurrentGesture < 12 && GestureGoal < 7)
    {
        return 1;
    }

    return 0;
}

auto MCMechActor::RecalcBounds(MCCamera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    const MCVector2D pos = Owner->GetScreenPos(cam->CameraId - 1);
    LowerRight.Y = pos.Y;
    UpperLeft.X = pos.X;
    UpperLeft.Y = pos.Y;
    LowerRight.X = pos.X;

    // The bounds are taken once, from every part's first frames seen.
    if (InView == 0)
    {
        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            const uint32_t part = PartOrder[i];

            if (PartShape[part] == nullptr || PartShape[part]->FrameList == nullptr || PartOrder[i] >= 4)
            {
                continue;
            }

            uint8_t* shapeTable = PartShape[part]->FrameList;
            int32_t frame = CurrentFrame[part];

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

            const int32_t size = VfxShapeResolution(PartShape[part]->FrameList, frame);

            if (ShapeMaxX < static_cast<float>(size >> 16))
            {
                ShapeMaxX = static_cast<float>(size >> 16);
            }

            if (ShapeMaxY < static_cast<float>(static_cast<int16_t>(size)))
            {
                ShapeMaxY = static_cast<float>(static_cast<int16_t>(size));
            }
        }

        InView = 1;
    }

    // Faithful: the top-left offset is added twice.
    const float scale = cam->CameraScale == 1 ? 0.5f : 1.0f;
    UpperLeft.X = scale * ShapeMinX + scale * ShapeMinX + UpperLeft.X;
    UpperLeft.Y = scale * ShapeMinY + scale * ShapeMinY + UpperLeft.Y;
    LowerRight.X = scale * ShapeMaxX + LowerRight.X;
    LowerRight.Y = scale * ShapeMaxY + LowerRight.Y;

    if (0.0f <= LowerRight.X && 0.0f <= LowerRight.Y &&
        UpperLeft.X <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewWidth))) &&
        UpperLeft.Y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->ViewHeight))))
    {
        return 1;
    }

    return 0;
}

auto CalcRotation(float rotation, int32_t numRotations) -> int32_t
{
    double degrees = rotation;

    if (degrees > 180.0)
    {
        degrees -= 360.0;
    }
    else if (degrees < -180.0)
    {
        degrees += 360.0;
    }

    if (degrees < 0.0)
    {
        degrees += 360.0;
    }

    int32_t index = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(degrees * static_cast<double>(numRotations + 1) * (1.0 / 360.0))));

    if (index > 0x1f)
    {
        index = 0x1f;
    }

    return index;
}

auto MCMechActor::RenderJump() -> void
{
    if (FrameHeights == nullptr)
    {
        return;
    }

    // The original also turns i and j by 45 degrees here, then drops them.
    auto* mech = static_cast<MCBattleMech*>(Owner);
    const MCFrameOfRef frame = mech->GetFrame();

    if (CurrentGesture != GESTURE_JUMP)
    {
        return;
    }

    const int32_t frameNum = CurrentFrame[MECH_PART_LEGS];
    const float height = FrameHeights[frameNum] * 30.0f;

    if (LiftOffFrame <= frameNum)
    {
        JumpVelocity = JumpSpeed;
        Airborne = 1;
    }

    if (TouchDownFrame <= frameNum && Airborne != 0)
    {
        JumpVelocity = 0.0f;
        Airborne = 0;
    }

    // Project the mech raised along its up axis by the jump height.
    const MCVector3D offset(frame.K.X * height, frame.K.Y * height, frame.K.Z * height);
    const MCVector3D position = mech->GetPosition();
    const MCVector3D raised(position.X + offset.X, position.Y + offset.Y, position.Z + offset.Z);
    MCCamera* cam = Eye;
    const MCVector3D camPosition = cam->Position;
    const float scale = cam->GetScaleFactor();
    MCVector3D relative(raised.X - camPosition.X, raised.Y - camPosition.Y, raised.Z - camPosition.Z);
    relative.X *= scale;
    relative.Y *= scale;
    relative.Z *= scale;
    const float y = ((relative.X * cam->SinAngle + cam->HalfHeight) - relative.Y * cam->SinAngle) - relative.Z;
    ScreenPos.X = relative.Y * cam->CosAngle + relative.X * cam->CosAngle + cam->HalfWidth;
    const float x = ScreenPos.X;
    UpperLeft.X = ScreenPos.X;
    LowerRight.X = x;
    ScreenPos.Y = y;
    UpperLeft.Y = y;
    LowerRight.Y = y;
    const float boundsScale = EyeScale();
    UpperLeft.X = boundsScale * ShapeMinX + boundsScale * ShapeMinX + x;
    UpperLeft.Y = boundsScale * ShapeMinY + boundsScale * ShapeMinY + y;
    LowerRight.X = boundsScale * ShapeMaxX + x;
    LowerRight.Y = boundsScale * ShapeMaxY + y;
}

auto MCMechActor::Render(int32_t depthFixup) -> int32_t
{
    ScreenPos = Owner->GetScreenPos(Eye->CameraId - 1);
    const float x = ScreenPos.X;
    const float y = ScreenPos.Y;
    // The 90-pixel part PAKs are the full size ones.
    const int largeSprites = Eye->CameraScale != 1 ? 1 : 0;
    RenderJump();

    auto* mech = static_cast<MCBattleMech*>(Owner);
    const float facing = ObjectFacing(mech);
    const float torso = mech->TorsoRotation;
    const float rightArm = mech->RightArmRotation;
    const float leftArm = mech->LeftArmRotation;

    // The torso and arms play the combat gestures while the gun is up or moving.
    const int32_t upperGesture = InCombatMode != 0                                     ? GESTURE_COMBAT
                                 : (CombatModeRaising != 0 || CombatModeLowering != 0) ? GESTURE_COMBAT_CHANGE
                                                                                       : CurrentGesture;
    PartShape[MECH_PART_LEGS] =
        MechTree->GetGesture(CurrentGesture, MECH_PART_LEGS, facing, facing, Reverse[MECH_PART_LEGS],
                             FrameRate[MECH_PART_LEGS], Visible, largeSprites);
    PartShape[MECH_PART_TORSO] =
        MechTree->GetGesture(upperGesture, MECH_PART_TORSO, facing + torso, facing, Reverse[MECH_PART_TORSO],
                             FrameRate[MECH_PART_TORSO], Visible, largeSprites);

    const float rotation2 = facing + leftArm + torso;
    const float rotation3 = facing + rightArm + torso;
    const bool armSymmetrical = MechTree->Gestures[CurrentGesture].ArmSymmetrical != 0;
    // A mirrored gesture facing left draws each arm from the other arm's sprites.
    const bool swapArms = armSymmetrical && WrapRotation(rotation3) < 0.0;

    if (RightArmGone != 0)
    {
        PartShape[MECH_PART_RIGHT_ARM] = nullptr;
    }
    else
    {
        PartShape[MECH_PART_RIGHT_ARM] =
            MechTree->GetGesture(upperGesture, swapArms ? 3 : 2, rotation2, facing, Reverse[MECH_PART_RIGHT_ARM],
                                 FrameRate[MECH_PART_RIGHT_ARM], Visible, largeSprites);
    }

    if (LeftArmGone != 0)
    {
        PartShape[MECH_PART_LEFT_ARM] = nullptr;
    }
    else if (swapArms)
    {
        PartShape[MECH_PART_LEFT_ARM] =
            MechTree->GetGesture(upperGesture, 2, rotation3, facing, Reverse[MECH_PART_LEFT_ARM],
                                 FrameRate[MECH_PART_LEFT_ARM], Visible, largeSprites);
    }
    else
    {
        // Faithful: asked for twice, first with the legs' gesture.
        PartShape[MECH_PART_LEFT_ARM] =
            MechTree->GetGesture(CurrentGesture, 3, rotation3, facing, Reverse[MECH_PART_LEFT_ARM],
                                 FrameRate[MECH_PART_LEFT_ARM], Visible, largeSprites);
        PartShape[MECH_PART_LEFT_ARM] =
            MechTree->GetGesture(upperGesture, 3, rotation3, facing, Reverse[MECH_PART_LEFT_ARM],
                                 FrameRate[MECH_PART_LEFT_ARM], Visible, largeSprites);
    }

    if (Wrecked != 0)
    {
        PartShape[MECH_PART_TORSO] = nullptr;
    }

    // The shadow, one of 32 facings.
    ElementList->OpenGroup(static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(-y)))), 1);
    auto* shadow = MCElementPool::Make<MCVfxElement>(ShadowShapes[0].get(), x, y, CalcRotation(facing, 0x20), 0,
                                                     nullptr, 0, Use90PixelSprite != 0 ? 1 : 0);

    // Port fix: the original copies the debug name through a null element too.
    if (shadow != nullptr)
    {
        strcpy(shadow->Name, Use90PixelSprite != 0 ? "mshad2" : "mshad1");
    }

    ElementList->Add(shadow);
    MechElements++;

    ElementList->OpenGroup(static_cast<int16_t>(static_cast<int32_t>(
                               std::floor(static_cast<double>(static_cast<float>(depthFixup) - ScreenPos.Y)))),
                           1);

    for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
    {
        const uint32_t part = PartOrder[i];

        if (PartShape[part] == nullptr)
        {
            continue;
        }

        uint8_t* fadeTable = nullptr;

        if (FadeTableIndex != -1 && FadeTableIndex >= 0)
        {
            fadeTable =
                GamePalette->FadePalettes.get() + (FadeTableIndex + GamePalette->NumBitmapHazeLevels * 2) * 0x100;
        }

        // Walking and running: a mirrored part runs half a cycle off the part it follows, so the stride
        // alternates; an unmirrored one keeps in step with it.
        const int32_t gesture = CurrentGesture;
        const bool combatAnim = InCombatMode != 0 || CombatModeRaising != 0 || CombatModeLowering != 0;
        StrideGesture = (gesture == 4 || gesture == 7 || gesture == 9 || gesture == 11) ? 1 : 0;

        if (StrideGesture != 0 && Reverse[part] != 0)
        {
            const MCGestureData& data = MechTree->Gestures[gesture];
            int32_t reference = -1;
            bool follows = false;

            if (data.ArmSymmetrical == 0)
            {
                reference = CurrentFrame[MECH_PART_LEFT_ARM];
                follows = true;
            }
            else if (data.Symmetrical == 0)
            {
                reference = CurrentFrame[MECH_PART_TORSO];
                follows = true;
            }

            if (follows && CurrentFrame[part] == reference && !combatAnim)
            {
                const int32_t frame = CurrentFrame[part];
                const uint32_t numFrames = data.NumFrames;

                if (PlayBackwards == 0)
                {
                    const uint32_t shifted = (numFrames >> 1) + static_cast<uint32_t>(frame);
                    CurrentFrame[part] = static_cast<int32_t>(shifted);
                    const uint32_t count = MechTree->Gestures[CurrentGesture].NumFrames;

                    if (count - 1 < shifted)
                    {
                        CurrentFrame[part] = static_cast<int32_t>(shifted % count);
                    }
                }
                else
                {
                    int32_t shifted = frame - static_cast<int32_t>(numFrames >> 1);
                    CurrentFrame[part] = shifted;

                    if (shifted < 0)
                    {
                        shifted = shifted < 0 ? -shifted : shifted;
                        const uint32_t count = MechTree->Gestures[CurrentGesture].NumFrames;
                        const uint32_t wrapped = static_cast<uint32_t>(shifted) % count;
                        CurrentFrame[part] = static_cast<int32_t>(count - wrapped);
                    }
                }
            }
        }

        StrideGesture =
            (CurrentGesture == 4 || CurrentGesture == 7 || CurrentGesture == 9 || CurrentGesture == 11) ? 1 : 0;

        if (StrideGesture != 0 && Reverse[part] == 0)
        {
            const MCGestureData& data = MechTree->Gestures[CurrentGesture];
            int32_t reference = 0;
            bool follows = false;

            if (data.ArmSymmetrical == 0)
            {
                reference = CurrentFrame[MECH_PART_LEFT_ARM];
                follows = true;
            }
            else if (data.Symmetrical == 0)
            {
                reference = CurrentFrame[MECH_PART_TORSO];
                follows = true;
            }

            if (follows && CurrentFrame[part] != reference && !combatAnim)
            {
                CurrentFrame[part] = reference;
            }
        }

        if (CurrentFrame[part] < 0)
        {
            CurrentFrame[part] = 0;
        }

        auto* element = MCElementPool::Make<MCVfxElement>(PartShape[part]->FrameList, ScreenPos.X, ScreenPos.Y,
                                                          CurrentFrame[part], Reverse[part], fadeTable, 1, 0);

        // Port fix: the original writes the debug names through a null element too, and "%i" can overrun name2.
        if (element != nullptr)
        {
            strcpy(element->Name, "mactor");

            if (OwnerMech == nullptr)
            {
                strcpy(element->Name2, "unknown");
            }
            else
            {
                snprintf(element->Name2, sizeof(element->Name2), "%i", OwnerMech->GetObjectType()->ObjTypeNum);
            }
        }

        ElementList->Add(element);
        MechElements++;
    }

    // Selection: -1 and 1 draw the bars; 3 white brackets; anything else the alignment's brackets.
    const int32_t selected = Owner->Selected;
    bool attackerBars = true;

    if (selected == -1 || selected == 1)
    {
        DrawBars();
        attackerBars = false;
    }
    else if (selected == 3)
    {
        RecalcBounds(Eye);
        DrawSelectBrackets(0xf8);
    }
    else if (selected != 0)
    {
        RecalcBounds(Eye);
        MCGameObject* obj = Owner;
        const int32_t alignment = obj->GetAlignment();

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
            DrawSelectBrackets(obj->GetAlignment() == HomeTeam->Alignment ? 0xfc : 0xfb);
        }
    }

    if (attackerBars && Owner->GetNumAttackers() > 0)
    {
        RecalcBounds(Eye);
        DrawBars();
    }

    if (Highlighting != 0)
    {
        if (HighlightTime > 0.0f)
        {
            HighlightTime -= FrameLength;
            DrawSelectBox(0xfc);
            return 0;
        }

        Highlighted = 1;
    }

    return 0;
}

auto MCMechActor::Update() -> int32_t
{
    auto* mech = static_cast<MCBattleMech*>(Owner);
    MCSpriteTree* tree = MechTree;
    GoalPending = 0;

    // Moving with nowhere to go: stand.
    if (CheckStop() != 0)
    {
        CurrentGesture = GESTURE_STAND;
        CurrentStateGesture = 1;

        if (static_cast<uint8_t>(mech->Status) != 4)
        {
            GestureGoal = -1;
            NextGesture = -1;
            InTransition = 0;

            if (mech->IsDisabled() == 0)
            {
                mech->Status = 0;
            }
        }
    }

    // Start the transition to the goal: the table lists the gestures from the state to it.
    int32_t goal = GestureGoal;

    if (goal != -1 && InTransition == 0 && NextGesture == -1)
    {
        InTransition = 1;
        TransitionStep = 0;
        const int32_t index = (goal + CurrentStateGesture * 9) * TRANSITION_ROW;
        NextGesture = TransitionArray[index];

        if (tree->TransitionArray != nullptr)
        {
            NextGesture = tree->TransitionArray[index];
        }

        if (NextGesture == -1)
        {
            GestureGoal = -1;
            InTransition = 0;
        }

        goal = GestureGoal;
        GestureDone = 0;

        if (goal > 6)
        {
            GestureDone = 1;
        }
    }

    if (InTransition != 0 && GestureDone != 0)
    {
        // The last gesture played out: on to the next one.
        const int32_t prevGesture = CurrentGesture;
        CurrentGesture = NextGesture;
        TransitionStep++;
        const int32_t index = TransitionStep + (goal + CurrentStateGesture * 9) * TRANSITION_ROW;
        NextGesture = TransitionArray[index];

        if (tree->TransitionArray != nullptr)
        {
            NextGesture = tree->TransitionArray[index];
        }

        int32_t startFrame = 0;
        bool checkReverse = true;

        if (NextGesture == -1)
        {
            // Arrived.
            CurrentStateGesture = goal;

            if (goal == 6)
            {
                CurrentStateGesture = 1;
            }

            GestureGoal = -1;
            InTransition = 0;

            if (CurrentStateGesture == 7 || CurrentStateGesture == 8)
            {
                mech->HandleFall(CurrentStateGesture == 7 ? 1 : 0);
            }

            if (mech->IsDisabled() == 0)
            {
                if (CurrentStateGesture == 1 && static_cast<uint8_t>(mech->Status) != 4)
                {
                    mech->Status = 0;
                }
                else if (CurrentStateGesture == 0)
                {
                    mech->Status = 5;
                }
            }

            if (NextGesture == -1)
            {
                // A gesture with a negative frame rate plays backwards.
                checkReverse = false;

                if (tree->Gestures[CurrentGesture].FrameRate < 0.0)
                {
                    startFrame = static_cast<int32_t>(tree->Gestures[CurrentGesture].NumFrames) - 1;
                    PlayBackwards = 1;
                }
                else
                {
                    startFrame = 0;
                    PlayBackwards = 0;
                }
            }
        }

        if (checkReverse)
        {
            // The gesture plays backwards when the next one is its reverse result.
            const MCGestureData& data = tree->Gestures[CurrentGesture];

            if (static_cast<uint32_t>(data.ReverseResult) == static_cast<uint32_t>(NextGesture))
            {
                startFrame = static_cast<int32_t>(data.NumFrames) - 1;
                PlayBackwards = 1;
            }
            else
            {
                startFrame = 0;
                PlayBackwards = 0;
            }
        }

        StrideGesture = 0;
        FrameHeights = nullptr;
        uint32_t gesture = static_cast<uint32_t>(CurrentGesture);

        if (Wrecked != 0 && gesture != 0x17 && gesture != 0x18)
        {
            InTransition = 0;
            GestureDone = 0;
            GestureGoal = -1;
            CurrentGesture = static_cast<int32_t>((gesture & 1) + 0x17);
        }

        const MCMechSpecialInfo& info = *tree->SpecialInfo;
        // The special frames a gesture starts at, per gesture (-1: none).
        auto setSpecialStart = [this](uint32_t frame)
        {
            if (frame != 0xffffffff)
            {
                FallStartPending = 1;
                FallStartFrame = static_cast<int32_t>(frame);
            }
        };

        auto clearCombat = [this]()
        {
            CombatModeChanged = 0;
            InCombatMode = 0;
            CombatModeRaising = 0;
            CombatModeLowering = 0;
        };

        switch (CurrentGesture)
        {
            case 0:
            case 1:
            case 2:
            {
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;

                if (StandTurnPending != 0)
                {
                    TurnAround(mech);
                    StandTurnPending = 0;
                    ClearRotations(mech);
                }
                break;
            }
            case 4:
            {
                StrideGesture = 1;
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;

                if (prevGesture == 3 && info.SWToWalkFrame != 0xffffffff)
                {
                    startFrame = static_cast<int32_t>(info.SWToWalkFrame);
                }
                break;
            }
            case 7:
            {
                StrideGesture = 1;
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;

                if (info.SpecialDuaneFlag != 0 && prevGesture == 6)
                {
                    startFrame = static_cast<int32_t>(tree->Gestures[7].NumFrames >> 1);
                }
                break;
            }
            case 9:
            {
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;

                if (info.WalkToWSFrame != 0xffffffff)
                {
                    startFrame = static_cast<int32_t>(info.WalkToWSFrame);
                }
                break;
            }
            case 11:
            {
                StrideGesture = 0;
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;
                break;
            }
            case 12:
            case 13:
            case 23:
            case 24:
            {
                BodyTurnLocked = 1;
                UpperBodyLocked = 1;
                clearCombat();
                break;
            }
            case 14:
            case 15:
            {
                if (FallStartPending != 0)
                {
                    startFrame = FallStartFrame;
                }

                if (info.ReallyStupidJamieReverseFlag != 0)
                {
                    FallTurnPending = 1;
                }

                FallStartPending = 0;
                BodyTurnLocked = 1;
                UpperBodyLocked = 1;
                ClearRotations(mech);
                clearCombat();
                break;
            }
            case 16:
            case 18:
            {
                setSpecialStart(CurrentGesture == 16 ? info.RFbWFbFrame : info.SFbWFbFrame);

                if (info.ReallyStupidJamieReverseFlag != 0)
                {
                    StandTurnPending = 1;
                }

                ClearRotations(mech);
                BodyTurnLocked = 1;
                UpperBodyLocked = 1;
                clearCombat();
                break;
            }
            case 17:
            case 19:
            {
                setSpecialStart(CurrentGesture == 17 ? info.RFfWFfFrame : info.SFfWFfFrame);

                if (info.ReallyStupidJamieReverseFlag != 0)
                {
                    FallTurnPending = 1;
                }

                ClearRotations(mech);
                BodyTurnLocked = 1;
                UpperBodyLocked = 1;
                clearCombat();
                break;
            }
            case GESTURE_JUMP:
            {
                InJump = 1;
                JumpSetup = 0;

                if (mech->GetPilot()->CurTacOrder.Code == TACTICAL_ORDER_JUMPTO_POINT)
                {
                    mech->GetPilot()->CurTacOrder.Stage = 2;
                }

                // Head for the goal: the distance, and the direction in jumpDirection.
                const MCVector3D position = mech->GetPosition();
                JumpDirection.X = JumpGoal.X - position.X;
                JumpDirection.Y = JumpGoal.Y - position.Y;
                JumpDirection.Z = JumpGoal.Z - position.Z;
                const double length = std::sqrt((static_cast<double>(JumpDirection.X) * JumpDirection.X +
                                                 static_cast<double>(JumpDirection.Y) * JumpDirection.Y) +
                                                static_cast<double>(JumpDirection.Z) * JumpDirection.Z);
                const float pathLength = static_cast<float>(length);

                if (length < 0.0 || length > 0.0)
                {
                    JumpDirection.X = static_cast<float>(JumpDirection.X / length);
                    JumpDirection.Y = static_cast<float>(JumpDirection.Y / length);
                    JumpDirection.Z = static_cast<float>(JumpDirection.Z / length);
                }

                const float distance = static_cast<float>(pathLength * 0.3 + pathLength);

                // Find the frames in the air: from the bottom of the crouch to the top of the climb.
                float* heights = GestureHeights(mech, GESTURE_JUMP);
                FrameHeights = heights;
                LiftOffFrame = 0;
                TouchDownFrame = 0;
                const MCGestureData& jump = tree->Gestures[GESTURE_JUMP];
                bool descended = false;
                bool climbing = false;

                // Port fix: the original reads a missing height table.
                for (int32_t i = 0; heights != nullptr && i < static_cast<int32_t>(jump.NumFrames); i++)
                {
                    const float change = i != 0 ? heights[i] - heights[i - 1] : 0.0f;

                    if (change < 0.0 && !descended)
                    {
                        descended = true;
                    }

                    if (change >= 0.0 && !climbing && descended)
                    {
                        climbing = true;
                        LiftOffFrame = i;
                    }

                    if (change < 0.0 && climbing)
                    {
                        TouchDownFrame = i;
                        break;
                    }
                }

                const int32_t airFrames = (TouchDownFrame - LiftOffFrame) + 5;
                const double speed =
                    (distance / (static_cast<double>(airFrames) / jump.FrameRate)) * MetersPerWorldUnit;
                JumpSpeed = static_cast<float>(speed);
                JumpParameter = static_cast<float>(distance / speed);
                mech->CreateJumpFX();
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;
                break;
            }

            case GESTURE_FALL:
            {
                FallTurnPending = 1;
                FrameHeights = GestureHeights(mech, GESTURE_FALL);
                BodyTurnLocked = 1;
                UpperBodyLocked = 1;

                if (info.OtherJamieReverseFlag != 0 || info.ReallyStupidJamieReverseFlag != 0)
                {
                    FallTurnPending = 0;
                }

                if (FallTurnPending != 0 && info.StupidJamieReverseFlag != 0)
                {
                    TurnAround(mech);
                    FallTurnPending = 0;
                    ClearRotations(mech);
                }

                clearCombat();
                break;
            }

            case 22:
            {
                if (FallTurnPending != 0 && info.StupidJamieReverseFlag == 0)
                {
                    TurnAround(mech);
                    FallTurnPending = 0;
                    ClearRotations(mech);
                }

                BodyTurnLocked = 1;
                UpperBodyLocked = 1;
                clearCombat();

                if (tree->SpecialInfo->ReallyStupidJamieReverseFlag != 0)
                {
                    StandTurnPending = 1;
                }
                break;
            }
            case 3:
            case 5:
            case 6:
            case 8:
            case 10:
            {
                BodyTurnLocked = 0;
                UpperBodyLocked = 0;
                break;
            }
            default:
                break;
        }

        GestureDone = 0;

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            CurrentFrame[i] = startFrame;
        }
    }

    // The gun going up starts the upper body at the change gesture's start, going down at its end.
    if (CombatModeChanged != 0 && (CombatModeRaising != 0 || CombatModeLowering != 0))
    {
        const int32_t frame =
            CombatModeRaising != 0 ? 0 : static_cast<int32_t>(tree->Gestures[GESTURE_COMBAT_CHANGE].NumFrames) - 1;
        CurrentFrame[MECH_PART_LEFT_ARM] = frame;
        CurrentFrame[MECH_PART_RIGHT_ARM] = frame;
        CurrentFrame[MECH_PART_TORSO] = frame;
        CombatModeChanged = 0;
    }

    FrameRate[MECH_PART_LEGS] = tree->Gestures[CurrentGesture].FrameRate;

    if (FrameRate[MECH_PART_LEGS] < 0.0)
    {
        FrameRate[MECH_PART_LEGS] = -FrameRate[MECH_PART_LEGS];
    }

    // Each part's mirroring and rate at its facing.
    const float facing = ObjectFacing(mech);
    const float torso = mech->TorsoRotation;
    const float rotation3 = mech->RightArmRotation + facing + torso;
    const float rotation2 = facing + mech->LeftArmRotation + torso;
    tree->SetGesture(CurrentGesture, MECH_PART_LEGS, facing, facing, Reverse[MECH_PART_LEGS],
                     FrameRate[MECH_PART_LEGS]);
    const int32_t upperGesture = InCombatMode != 0                                     ? GESTURE_COMBAT
                                 : (CombatModeRaising != 0 || CombatModeLowering != 0) ? GESTURE_COMBAT_CHANGE
                                                                                       : CurrentGesture;
    tree->SetGesture(upperGesture, MECH_PART_TORSO, facing + torso, facing, Reverse[MECH_PART_TORSO],
                     FrameRate[MECH_PART_TORSO]);

    if (LeftArmGone == 0)
    {
        tree->SetGesture(upperGesture, 3, rotation3, facing, Reverse[MECH_PART_LEFT_ARM],
                         FrameRate[MECH_PART_LEFT_ARM]);
    }

    if (RightArmGone == 0)
    {
        tree->SetGesture(upperGesture, 2, rotation2, facing, Reverse[MECH_PART_RIGHT_ARM],
                         FrameRate[MECH_PART_RIGHT_ARM]);
    }

    const int32_t gesture = CurrentGesture;

    if (gesture == GESTURE_JUMP)
    {
        if (LiftOffFrame <= CurrentFrame[MECH_PART_LEGS])
        {
            JumpVelocity = JumpSpeed;
            Airborne = 1;
        }

        if (TouchDownFrame <= CurrentFrame[MECH_PART_LEGS] && Airborne != 0)
        {
            JumpVelocity = 0.0f;
            Airborne = 0;
        }
    }

    LyingStill = 0;

    if (CurrentFrame[MECH_PART_LEGS] == -1)
    {
        CurrentFrame[MECH_PART_LEGS] = 0;

        if (InCombatMode == 0 && CombatModeRaising == 0 && CombatModeLowering == 0)
        {
            CurrentFrame[MECH_PART_TORSO] = 0;
            CurrentFrame[MECH_PART_RIGHT_ARM] = 0;
            CurrentFrame[MECH_PART_LEFT_ARM] = 0;
        }
    }
    else if (InTransition == 0 && (gesture == 0x17 || gesture == 0x18))
    {
        // Lying down: hold the last frame.
        const int32_t last = static_cast<int32_t>(tree->Gestures[gesture].NumFrames) - 1;

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            CurrentFrame[i] = last;
        }

        LyingStill = 1;
    }
    else
    {
        // The frames to step: from the legs' frame rate, or one at a time in single-step mode.
        int32_t step = 0;

        if (SingleStepMode == 0)
        {
            const double interval = 1.0 / FrameRate[MECH_PART_LEGS];
            const double time = static_cast<double>(FrameLength) + CurrentTime[MECH_PART_LEGS];
            CurrentTime[MECH_PART_LEGS] = static_cast<float>(time);

            if (interval <= time)
            {
                step = static_cast<int32_t>(time * FrameRate[MECH_PART_LEGS]);
                // Original behaviour (OB-055): the time left is time / step - interval, not time - step * interval.
                CurrentTime[MECH_PART_LEGS] = static_cast<float>(time / step - interval);
            }

            // The footstep of the fall-down gestures.
            if (CurrentFrame[MECH_PART_LEGS] == 10 && (gesture == 0xe || gesture == 0xf))
            {
                SoundSystem->PlayDigitalSample(0x1d, 1, Owner, 0, 0);
            }
        }
        else if (NextStep != 0)
        {
            step = 1;
        }
        else if (PrevStep != 0)
        {
            step = -1;
        }

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            const uint32_t part = PartOrder[i];

            // The upper body plays the gun change on its own.
            if (part != 0)
            {
                if (CombatModeRaising != 0)
                {
                    CurrentFrame[part] += step;

                    if (static_cast<int32_t>(tree->Gestures[GESTURE_COMBAT_CHANGE].NumFrames) <= CurrentFrame[part])
                    {
                        CurrentFrame[MECH_PART_TORSO] = 0;
                        CurrentFrame[MECH_PART_RIGHT_ARM] = 0;
                        CurrentFrame[MECH_PART_LEFT_ARM] = 0;
                        CombatModeRaising = 0;
                        InCombatMode = 1;
                    }

                    continue;
                }

                if (CombatModeLowering != 0)
                {
                    const int32_t frame = CurrentFrame[part] - step;
                    CurrentFrame[part] = frame;

                    if (frame < 1)
                    {
                        const int32_t legsFrame = CurrentFrame[MECH_PART_LEGS];
                        CombatModeLowering = 0;
                        CurrentFrame[MECH_PART_TORSO] = legsFrame;
                        CurrentFrame[MECH_PART_RIGHT_ARM] = legsFrame;
                        CurrentFrame[MECH_PART_LEFT_ARM] = legsFrame;
                        InCombatMode = 0;
                    }

                    continue;
                }

                if (InCombatMode != 0)
                {
                    continue;
                }
            }

            if (step == 0)
            {
                continue;
            }

            const int32_t delta = PlayBackwards != 0 ? -step : step;
            CurrentFrame[part] += delta;
            const uint32_t numFrames = tree->Gestures[CurrentGesture].NumFrames;

            if (static_cast<int32_t>(numFrames) <= CurrentFrame[part])
            {
                if (InTransition == 0)
                {
                    // Loop.
                    CurrentFrame[part] = static_cast<int32_t>(static_cast<uint32_t>(CurrentFrame[part]) % numFrames);

                    if (CurrentGesture == GESTURE_JUMP)
                    {
                        GestureDone = 0;
                    }
                }
                else
                {
                    // Played out: the transition goes on.
                    GestureDone = 1;
                    CurrentFrame[part] = static_cast<int32_t>(numFrames) - 1;

                    if (CurrentGesture == GESTURE_JUMP)
                    {
                        InJump = 0;
                        CurrentStateGesture = 1;

                        if (mech->GetPilot()->CurTacOrder.Code == TACTICAL_ORDER_JUMPTO_POINT)
                        {
                            mech->GetPilot()->CurTacOrder.Stage = 3;
                        }
                    }
                }
            }

            // Some transitions go on from a special frame instead of the gesture's end.
            const int32_t transition = InTransition;

            if (transition != 0)
            {
                const MCMechSpecialInfo& info = *tree->SpecialInfo;
                const int32_t frame = CurrentFrame[part];

                if (CurrentGesture == 4 && NextGesture == 6)
                {
                    GestureDone = static_cast<int32_t>(info.WalkToWRFrame) <= frame ? 1 : 0;
                }

                if (CurrentGesture == 4 && NextGesture == 5 && info.WalkToWSFrame != 0xffffffff)
                {
                    GestureDone = static_cast<int32_t>(info.WalkToWSFrame) <= frame ? 1 : 0;
                }

                if (NextGesture == 8 && CurrentGesture == 7)
                {
                    GestureDone = static_cast<int32_t>(info.RunToRWFrame) <= frame ? 1 : 0;
                }
            }

            if (CurrentFrame[part] < 0)
            {
                if (transition == 0)
                {
                    // Backwards past the start: wrap to the end.
                    const int32_t frame = CurrentFrame[part];
                    const uint32_t magnitude = static_cast<uint32_t>(frame < 0 ? -frame : frame);
                    const uint32_t count = tree->Gestures[CurrentGesture].NumFrames;
                    CurrentFrame[part] = static_cast<int32_t>(count - magnitude % count);
                }
                else
                {
                    GestureDone = 1;
                    CurrentFrame[part] = 0;
                }
            }
        }
    }

    // Legs first, then the arm on the far side, the torso and the near arm.
    PartOrder[0] = MECH_PART_LEGS;
    PartOrder[1] = MECH_PART_RIGHT_ARM;
    PartOrder[2] = MECH_PART_TORSO;
    PartOrder[3] = MECH_PART_LEFT_ARM;

    if (rotation3 < 0.0)
    {
        PartOrder[1] = MECH_PART_LEFT_ARM;
        PartOrder[3] = MECH_PART_RIGHT_ARM;
    }

    if (CurrentGesture == GESTURE_FALL)
    {
        PartOrder[1] = MECH_PART_LEFT_ARM;
        PartOrder[3] = MECH_PART_RIGHT_ARM;
    }

    if (Visible != 0 && Owner->IsCaptured() != 0 && Highlighted == 0 && Highlighting == 0)
    {
        Highlighting = 1;
        HighlightTime = 3.0f;
    }

    return 1;
}

auto DestroyMechShadows() -> void
{
    if (MCMechActor::ShadowShapes.empty() || MCMechActor::NumShadows == 0)
    {
        return;
    }

    for (std::unique_ptr<uint8_t[]>& shape : MCMechActor::ShadowShapes)
    {
        if (shape != nullptr)
        {
            MCRenderer::UnregisterData(shape.get());
        }
    }

    MCMechActor::ShadowShapes.clear();
}

auto MCMechActor::Destroy() -> void
{
    MechTree->RemoveUsers(this);
    AppearanceTypeList->RemoveAppearance(MechTree);
}

auto DrawBox(float left, float top, float right, float bottom) -> void
{
    AddLine(MCVector2D(left, top), MCVector2D(right, top), 0x10);
    AddLine(MCVector2D(right, top), MCVector2D(right, bottom), 0x10);
    AddLine(MCVector2D(right, bottom), MCVector2D(left, bottom), 0x10);
    AddLine(MCVector2D(left, top), MCVector2D(left, bottom), 0x10);
}

auto MCMechActor::DrawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = EyeScale();
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(UpperLeft.Y) - scale * 24.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(ScreenPos.X) - barWidth * 0.5f)));

    auto* mech = static_cast<MCMover*>(Owner);

    if (mech == nullptr || mech->WeaponEffectiveness < 0.0f || mech->MaxWeaponEffectiveness < mech->WeaponEffectiveness)
    {
        return;
    }

    const float health = mech->GetTotalEffectiveness();
    // Green, then yellow below half, red at a fifth.
    int32_t barColor;

    if (health < 0.5)
    {
        barColor = health <= 0.2 ? 0x103 : 0x102;
    }
    else
    {
        barColor = 0x101;
    }

    ElementList->OpenGroup(-50000, 1);
    MCPolyElementData data;
    data.NumVertices = 0;
    data.TextureMapOff = 0;
    data.Texture = nullptr;
    data.TextureWidth = 0;
    data.TextureHeight = 0;
    data.FadeTable = nullptr;
    data.Translate = 0;
    data.StatusBar = 1;
    data.BarColor = barColor;
    auto floorInt = [](float value)
    {
        return static_cast<int32_t>(static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(value)))));
    };

    data.Vertices[0].X = floorInt(barX - 1.0f);
    data.Vertices[0].Y = floorInt(barY - 1.0f);
    data.Vertices[1].X = floorInt(barX + barWidth + 1.0f);
    data.Vertices[1].Y = floorInt(barY + barHeight + 1.0f);
    const int32_t barLength = floorInt(health * barWidth);
    data.BarPercent = barLength;

    // A mech that isn't quite dead shows at least one pixel.
    if (static_cast<double>(barLength) < 1.0 && health > 0.001)
    {
        data.BarPercent = 1;
    }

    if (mech->IsDisabled() == 0 && mech->IsDestroyed() == 0)
    {
        ElementList->Add(MCElementPool::Make<MCPolygonElement>(&data, -50000));
    }
}

auto MCMechActor::DrawTargetDamage() -> void
{
    MCGameObject* obj = Owner;

    if (obj == nullptr || obj->ObjectClass != BATTLEMECH)
    {
        return;
    }

    auto* mech = static_cast<MCBattleMech*>(obj);
    // The enemy mechs.
    MCObjectQueueNode* enemies = mech->GetAlignment() == 1 ? ClanMechList : InnerSphereMechList;

    // A ring around the mech, with a line out to each enemy it can see, as long as its expected damage to it.
    // Port: overlays, on the screen over the view: the ring follows the sprite through the zoom.
    MCVector2D center =
        MCOverlayPoint(MCVector2D((UpperLeft.X + LowerRight.X) * 0.5f, (UpperLeft.Y + LowerRight.Y) * 0.5f));
    const float radius = std::sqrt((UpperLeft.X - LowerRight.X) * (UpperLeft.X - LowerRight.X) +
                                   (UpperLeft.Y - LowerRight.Y) * (UpperLeft.Y - LowerRight.Y)) *
                         0.375f * MCOverlay.ScaleX;
    const MCVector2D ownPos = MCOverlayPoint(ScreenPos);
    MCVector2D size(radius, radius);
    MCBaseObject* current = nullptr;

    while (enemies->Traverse(current) != nullptr)
    {
        auto* target = static_cast<MCGameObject*>(current);

        if (target->IsDisabled() != 0)
        {
            continue;
        }

        if (target->GetContactType(mech->GetTeam()->Id) != 1)
        {
            continue;
        }

        const float damage = mech->CalcExpectedTargetDamage(target);

        if (!(damage > 0.0f))
        {
            continue;
        }

        ElementList->OpenGroup(-50000, 1);
        ElementList->Add(MCElementPool::Make<MCEllipseElement>(center, size, 0xb, -50000));

        const MCVector2D targetPos = MCOverlayPoint(target->GetScreenPos(0));
        const double dx = static_cast<double>(targetPos.X) - ownPos.X;
        const double dy = static_cast<double>(targetPos.Y) - ownPos.Y;
        const double angle = std::atan(dy / dx);
        const float c = static_cast<float>(std::fabs(std::cos(angle)));
        const float s = static_cast<float>(std::fabs(std::sin(angle)));
        const float signX = dx <= 0.0 ? -1.0f : 1.0f;
        const float signY = static_cast<float>(dy) <= 0.0f ? -1.0f : 1.0f;
        MCVector2D start(signX * c * radius + center.X, signY * s * radius + center.Y);
        const float length = (damage / mech->MaxTargetDamage) * 60.0f;
        MCVector2D end(signX * length * c + start.X, signY * length * s + start.Y);
        ElementList->Add(MCElementPool::Make<MCLineElement>(start, end, 0xef, nullptr, -50000, -1));
    }
}
