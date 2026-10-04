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
#include "lib/heap.h"
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

PacketFile* MechActor::shadows = nullptr;
uint8_t** MechActor::shadowShapes = nullptr;
int32_t MechActor::numShadows = 0;

char hotSpotFinderArray[28] = {0,  0,  0,  1,  2,  3,  4,  5,  6,  2,  3,  7, 8,  9,
                               10, 11, 12, 13, 14, 15, 16, 17, 18, 11, 10, 0, 19, 19};

char transitionArray[0x32a] = {
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

int32_t equivalentGestureArray[10] = {0, -1, 1, -1, 2, -1, -1, 3, -1, 0};
int singleStepMode = 1;
int32_t mechElements = 0;

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
    auto exactObjectFacing(GameObject* obj) -> double
    {
        const frame_of_ref frame = obj->getFrame();
        float cosFacing = UnitX.x * frame.i.x + UnitX.y * frame.i.y + UnitX.z * frame.i.z;

        if (cosFacing < -1.0)
        {
            cosFacing = -1.0f;
        }

        if (cosFacing > 1.0)
        {
            cosFacing = 1.0f;
        }

        double facing = acosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

        if (frame.i.y < 0.0)
        {
            facing = -facing;
        }

        return facing;
    }

    auto objectFacing(GameObject* obj) -> float
    {
        return static_cast<float>(exactObjectFacing(obj));
    }

    /// <summary>Wraps <paramref name="rotation"/> into -180..180 (one step each way).</summary>
    auto wrapRotation(float rotation) -> float
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
    auto eyeScale() -> float
    {
        return eye->cameraScale == 1 ? 0.5f : 1.0f;
    }

    /// <summary>Turns <paramref name="mech"/>'s frame half a turn about its up axis (the reversed gestures end
    /// facing the other way) and clears its torso and arm rotations.</summary>
    auto turnAround(BattleMech* mech) -> void
    {
        frame_of_ref frame = mech->getFrame();
        const float s = static_cast<float>(std::sin(TURN_ANGLE));
        const float c = static_cast<float>(std::cos(TURN_ANGLE));
        const vector_3d oldI = frame.i;
        frame.i.x = frame.j.x * s + oldI.x * c;
        frame.i.y = frame.j.y * s + oldI.y * c;
        frame.i.z = frame.j.z * s + oldI.z * c;
        frame.j.x = frame.j.x * c - oldI.x * s;
        frame.j.y = frame.j.y * c - oldI.y * s;
        frame.j.z = frame.j.z * c - oldI.z * s;
        mech->setFrame(frame);
    }

    /// <summary>Clears the mech's torso and arm rotations.</summary>
    auto clearRotations(BattleMech* mech) -> void
    {
        mech->leftArmRotation = 0.0f;
        mech->rightArmRotation = 0.0f;
        mech->torsoRotation = 0.0f;
    }

    /// <summary>The outline table of <paramref name="mech"/> for gesture <paramref name="gesture"/> (the jump and
    /// fall heights), or null.</summary>
    auto gestureHeights(BattleMech* mech, int32_t gesture) -> float*
    {
        auto* type = static_cast<BattleMechType*>(mech->getObjectType());

        if (type->gestureOutlines == nullptr)
        {
            return nullptr;
        }

        return reinterpret_cast<float*>(type->gestureOutlines[static_cast<uint8_t>(hotSpotFinderArray[gesture])]);
    }

    /// <summary>Adds the line from <paramref name="start"/> to <paramref name="end"/> in <paramref name="color"/>.</summary>
    auto addLine(vector_2d start, vector_2d end, int32_t color) -> void
    {
        ElementList->add(new LineElement(start, end, color, nullptr, -50000, -1));
    }
}

//---------------------------------------------------------------------------
// MechActor
//---------------------------------------------------------------------------

auto MechActor::hitMech(int32_t) -> int
{
    return 0;
}

auto MechActor::setGesture(uint32_t gesture) -> int32_t
{
    if (gestureSet != 0)
    {
        return static_cast<int32_t>(0xeade0004);
    }

    gestureSet = 1;
    currentGesture = static_cast<int32_t>(gesture);
    currentStateGesture = equivalentGestureArray[gesture];
    return 0;
}

auto MechActor::getNumFramesInGesture(uint32_t gesture) -> float
{
    return static_cast<float>(mechTree->gestures[gesture].numFrames);
}

auto MechActor::getVelocityOfGesture(uint32_t gesture) -> float
{
    if (static_cast<int32_t>(gesture) < static_cast<int32_t>(mechTree->treeInfo->numGestures))
    {
        return mechTree->gestures[gesture].startVelocity;
    }

    return -1.0f;
}

auto MechActor::getHotSpotIndex(uint32_t location) -> uint32_t
{
    return static_cast<uint32_t>(static_cast<int32_t>(hotSpotFinderArray[location]));
}

auto MechActor::preloadGestures(int32_t gesture, float rotation) -> void
{
    mechTree->preloadGestures(gesture, rotation);
}

auto MechActor::setGestureGoal(int32_t goal) -> int32_t
{
    if (goalPending != 0)
    {
        return static_cast<int32_t>(0xeada0005);
    }

    const int32_t state = currentStateGesture;

    if (state == goal)
    {
        return static_cast<int32_t>(0xeade0001);
    }

    if (inTransition != 0)
    {
        return static_cast<int32_t>(0xeade0002);
    }

    if (goal == 6)
    {
        if (jumpSetup == 0)
        {
            return static_cast<int32_t>(0xeada0006);
        }
    }
    else if (goal < 0 || goal > 8)
    {
        return static_cast<int32_t>(0xeade0003);
    }

    // Running backwards (the torso turned past the side) is walking backwards for mirrored gestures.
    auto* mech = static_cast<BattleMech*>(owner);
    const double facing = exactObjectFacing(mech) + mech->torsoRotation;

    if (!(facing >= 0.0f) || facing >= 180.0)
    {
        const GestureData& data = mechTree->gestures[goal];

        if ((data.symmetrical != 0 || data.armSymmetrical != 0) && goal == 8)
        {
            goal = 7;
        }
    }

    unknownD8 = 0;

    // Faithful: a leftover test of the transition table index that can't fail.
    if ((goal + state * 9) * 10 == -1)
    {
        return static_cast<int32_t>(0xeade0003);
    }

    gestureGoal = goal;
    goalPending = 1;
    return 0;
}

auto MechActor::init(AppearanceType* tree, GameObject* obj) -> int32_t
{
    unknown04 = 0x70000000;
    visible = 0;
    owner = obj;
    mechTree = static_cast<SpriteTree*>(tree);

    if (tree != nullptr)
    {
        tree->addUsers(this);
    }

    unknown12C = 0;
    unknown130 = 0;
    unknown134 = 0;
    gestureSet = 0;
    fadeTableIndex = -1;
    unknown18C = 0;

    // The shadow shapes are loaded once, for every mech.
    if (shadowShapes == nullptr)
    {
        shadowShapes = static_cast<uint8_t**>(systemHeap->malloc(0x80 * sizeof(uint8_t*)));

        for (int32_t i = 0; i < 0x80; i++)
        {
            shadowShapes[i] = nullptr;
        }

        shadows = new PacketFile;

        if (shadows != nullptr)
        {
            FullPathFileName shadowName;
            shadowName.init(spritePath, "shadow", ".pak");
            int32_t result = shadows->open(shadowName, READ, 50);

            if (result != 0)
            {
                FullPathFileName cdName;
                cdName.init(CDspritePath, "shadow", ".pak");
                result = shadows->open(cdName, READ, 50);
            }

            if (result == 0)
            {
                if (shadows->getNumPackets() > 0x7f)
                {
                    Fatal(-1, " Too Many shadow Shapes ", nullptr);
                }

                numShadows = shadows->getNumPackets();

                for (int32_t i = 0; i < shadows->getNumPackets(); i++)
                {
                    shadows->seekPacket(i);
                    const int32_t size = shadows->getPacketSize();
                    shadowShapes[i] = static_cast<uint8_t*>(systemHeap->malloc(static_cast<uint32_t>(size)));

                    if (shadowShapes[i] == nullptr)
                    {
                        Fatal(-1, " no RAM for shadows ", nullptr);
                    }

                    shadows->readPacket(i, shadowShapes[i]);
                    MCRenderer::RegisterData(shadowShapes[i], static_cast<size_t>(size), MCDataKind::Shapes);
                }

                shadows->close();
            }
            else
            {
                numShadows = 0;
            }
        }

        delete shadows;
        shadows = nullptr;
    }

    for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
    {
        partShape[i] = nullptr;
        currentFrame[i] = -1;
        unknown64[i] = 0;
        currentTime[i] = 0.0f;
        lastFrame[i] = 0;
        frameRate[i] = 15.0f;
        unknownB4[i] = 0;
        partOrder[i] = 0;
    }

    visible = 0;
    shapeMinY = -25.0f;
    shapeMinX = -25.0f;
    unknownE0 = 0.0f;
    unknownC8 = 0;
    goalPending = 0;
    currentGesture = 0;
    gestureGoal = -1;
    currentStateGesture = 0;
    inTransition = 0;
    unknown13C = 0;
    unknownDC = -1;
    unknown140 = 0;
    unknown15C = 3.0f;
    unknown160 = 0;
    unknown164 = 0;
    unknown168 = 0;
    unknown150 = 0;
    unknown14C = 0;
    unknown154 = -1;
    unknownE8 = nullptr;
    unknown1A8 = 0;
    inJump = 0;
    jumpSetup = 0;
    jumpGoal.z = 0.0f;
    jumpGoal.y = 0.0f;
    jumpGoal.x = 0.0f;
    jumpParameter = 0.0f;
    unknown114 = 1;
    jumpSpeed = 0.0f;
    unknown11C = 0;
    unknown120 = 0;
    unknown174 = 0;
    unknown178 = 0;
    shapeMaxY = 25.0f;
    shapeMaxX = 25.0f;
    inView = 0;
    unknown194 = 0;
    stopCountdown = 0.0f;
    inCombatMode = 0;
    combatModeRaising = 0;
    combatModeLowering = 0;
    return 0;
}

auto MechActor::setCombatMode(int combatMode) -> void
{
    int32_t gesture;

    if (combatMode == 0)
    {
        if (inCombatMode == 0)
        {
            return;
        }

        if (combatModeLowering == 0)
        {
            inCombatMode = 0;
            combatModeLowering = 1;
            combatModeChanged = 1;
        }

        gesture = currentGesture;

        if (gesture < 2)
        {
            combatModeLowering = 0;
            inCombatMode = 0;
            combatModeChanged = 0;
            return;
        }
    }
    else
    {
        if (inCombatMode != 0)
        {
            return;
        }

        gesture = currentGesture;

        if (gesture < 2 || gesture > 11)
        {
            return;
        }

        // Only from the stand, walk and run gestures the mech has a gun pose for.
        const MechSpecialInfo& info = *mechTree->specialInfo;

        if (info.standToGunPose == 0 && gesture == 2)
        {
            return;
        }

        if (info.walkToGunPose == 0 && gesture > 2 && gesture < 6)
        {
            return;
        }

        if (gesture > 8)
        {
            return;
        }

        if (info.runToGunPose == 0 && gesture > 5 && gesture < 9)
        {
            return;
        }

        if (combatModeRaising == 0)
        {
            combatModeRaising = 1;
            combatModeChanged = 1;
        }
    }

    if (gesture < 12)
    {
        return;
    }

    combatModeLowering = 0;
    inCombatMode = 0;
    combatModeChanged = 0;
}

auto MechActor::setJumpParameters(vector_3d& goal, int) -> int32_t
{
    if (inJump != 0)
    {
        return static_cast<int32_t>(0xeada0007);
    }

    jumpParameter = 4.0f;
    jumpSetup = 1;
    jumpGoal = goal;
    return 0;
}

auto MechActor::getVelocityMagnitude() -> float
{
    const int32_t gesture = currentGesture;

    if (gesture == GESTURE_JUMP || gesture == 12 || gesture == 13)
    {
        return unknownE0;
    }

    const int32_t numGestures = mechTree->treeInfo->numGestures;
    const float startVelocity = gesture < numGestures ? mechTree->gestures[gesture].startVelocity : -1.0f;
    const float endVelocity = gesture < numGestures ? mechTree->gestures[gesture].endVelocity : -1.0f;
    float velocity = 0.0f;

    if ((gesture < 14 || gesture > 19) && (velocity = startVelocity, startVelocity != endVelocity))
    {
        // Ease from the start velocity to the end one over the gesture.
        if (startVelocity >= -1999.0 && endVelocity >= -1999.0)
        {
            const uint32_t numFrames = mechTree->gestures[gesture].numFrames;
            const double framesLeft =
                static_cast<double>(numFrames - static_cast<uint32_t>(currentFrame[MECH_PART_LEGS]));
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

auto MechActor::setMovePath(MovePath* path) -> int32_t
{
    if (path == nullptr)
    {
        stopCountdown = 0.0f;
        unknown194 = 0;
        return 0;
    }

    if (path->numSteps != 0 && path->curStep < path->numSteps)
    {
        stopCountdown = 5.0f;
        return 0;
    }

    stopCountdown = 0.0f;
    return 0;
}

auto MechActor::forceStop() -> void
{
    stopCountdown = 0.0f;
}

auto MechActor::checkStop() -> int
{
    if (unknown1A8 == 0 && stopCountdown == 0.0 && currentGesture > 2 && currentGesture < 12 && gestureGoal < 7)
    {
        return 1;
    }

    return 0;
}

auto MechActor::recalcBounds(Camera* cam) -> int
{
    if (cam == nullptr)
    {
        return 0;
    }

    const vector_2d pos = owner->getScreenPos(cam->cameraId - 1);
    lowerRight.y = pos.y;
    upperLeft.x = pos.x;
    upperLeft.y = pos.y;
    lowerRight.x = pos.x;

    // The bounds are taken once, from every part's first frames seen.
    if (inView == 0)
    {
        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            const uint32_t part = partOrder[i];

            if (partShape[part] == nullptr || partShape[part]->frameList == nullptr || partOrder[i] >= 4)
            {
                continue;
            }

            uint8_t* shapeTable = partShape[part]->frameList;
            int32_t frame = currentFrame[part];

            if (frame < 0)
            {
                frame = 0;
            }

            const int32_t numShapeFrames = VFX_shape_count(shapeTable);

            if (numShapeFrames <= frame)
            {
                frame = numShapeFrames - 1;
            }

            const int32_t minXY = VFX_shape_minxy(shapeTable, frame);

            if (static_cast<float>(minXY >> 16) < shapeMinX)
            {
                shapeMinX = static_cast<float>(minXY >> 16);
            }

            if (static_cast<float>(static_cast<int16_t>(minXY)) < shapeMinY)
            {
                shapeMinY = static_cast<float>(static_cast<int16_t>(minXY));
            }

            const int32_t size = VFX_shape_resolution(partShape[part]->frameList, frame);

            if (shapeMaxX < static_cast<float>(size >> 16))
            {
                shapeMaxX = static_cast<float>(size >> 16);
            }

            if (shapeMaxY < static_cast<float>(static_cast<int16_t>(size)))
            {
                shapeMaxY = static_cast<float>(static_cast<int16_t>(size));
            }
        }

        inView = 1;
    }

    // Faithful: the top-left offset is added twice.
    const float scale = cam->cameraScale == 1 ? 0.5f : 1.0f;
    upperLeft.x = scale * shapeMinX + scale * shapeMinX + upperLeft.x;
    upperLeft.y = scale * shapeMinY + scale * shapeMinY + upperLeft.y;
    lowerRight.x = scale * shapeMaxX + lowerRight.x;
    lowerRight.y = scale * shapeMaxY + lowerRight.y;

    if (0.0f <= lowerRight.x && 0.0f <= lowerRight.y &&
        upperLeft.x <= static_cast<float>(static_cast<int32_t>(std::floor(cam->viewWidth))) &&
        upperLeft.y <= static_cast<float>(static_cast<int32_t>(std::floor(cam->viewHeight))))
    {
        return 1;
    }

    return 0;
}

auto calcRotation(float rotation, int32_t numRotations) -> int32_t
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

auto MechActor::renderJump() -> void
{
    if (unknownE8 == nullptr)
    {
        return;
    }

    // The original also turns i and j by 45 degrees here, then drops them.
    auto* mech = static_cast<BattleMech*>(owner);
    const frame_of_ref frame = mech->getFrame();

    if (currentGesture != GESTURE_JUMP)
    {
        return;
    }

    const int32_t frameNum = currentFrame[MECH_PART_LEGS];
    const float height = unknownE8[frameNum] * 30.0f;

    if (unknown124 <= frameNum)
    {
        unknownE0 = jumpSpeed;
        unknownE4 = unknown11C;
        unknown120 = 1;
    }

    if (unknown128 <= frameNum && unknown120 != 0)
    {
        unknownE0 = 0.0f;
        unknownE4 = 0;
        unknown120 = 0;
    }

    // Project the mech raised along its up axis by the jump height.
    const vector_3d offset(frame.k.x * height, frame.k.y * height, frame.k.z * height);
    const vector_3d position = mech->getPosition();
    const vector_3d raised(position.x + offset.x, position.y + offset.y, position.z + offset.z);
    Camera* cam = eye;
    const vector_3d camPosition = cam->position;
    const float scale = cam->getScaleFactor();
    vector_3d relative(raised.x - camPosition.x, raised.y - camPosition.y, raised.z - camPosition.z);
    relative.x *= scale;
    relative.y *= scale;
    relative.z *= scale;
    const float y = ((relative.x * cam->sinAngle + cam->halfHeight) - relative.y * cam->sinAngle) - relative.z;
    screenPos.x = relative.y * cam->cosAngle + relative.x * cam->cosAngle + cam->halfWidth;
    const float x = screenPos.x;
    upperLeft.x = screenPos.x;
    lowerRight.x = x;
    screenPos.y = y;
    upperLeft.y = y;
    lowerRight.y = y;
    const float boundsScale = eyeScale();
    upperLeft.x = boundsScale * shapeMinX + boundsScale * shapeMinX + x;
    upperLeft.y = boundsScale * shapeMinY + boundsScale * shapeMinY + y;
    lowerRight.x = boundsScale * shapeMaxX + x;
    lowerRight.y = boundsScale * shapeMaxY + y;
}

auto MechActor::render(int32_t depthFixup) -> int32_t
{
    screenPos = owner->getScreenPos(eye->cameraId - 1);
    const float x = screenPos.x;
    const float y = screenPos.y;
    // The 90-pixel part PAKs are the full size ones.
    const int largeSprites = eye->cameraScale != 1 ? 1 : 0;
    renderJump();

    auto* mech = static_cast<BattleMech*>(owner);
    const float facing = objectFacing(mech);
    const float torso = mech->torsoRotation;
    const float rightArm = mech->rightArmRotation;
    const float leftArm = mech->leftArmRotation;

    // The torso and arms play the combat gestures while the gun is up or moving.
    const int32_t upperGesture = inCombatMode != 0                                     ? GESTURE_COMBAT
                                 : (combatModeRaising != 0 || combatModeLowering != 0) ? GESTURE_COMBAT_CHANGE
                                                                                       : currentGesture;
    partShape[MECH_PART_LEGS] =
        mechTree->getGesture(currentGesture, MECH_PART_LEGS, facing, facing, reverse[MECH_PART_LEGS],
                             frameRate[MECH_PART_LEGS], visible, largeSprites);
    partShape[MECH_PART_TORSO] =
        mechTree->getGesture(upperGesture, MECH_PART_TORSO, facing + torso, facing, reverse[MECH_PART_TORSO],
                             frameRate[MECH_PART_TORSO], visible, largeSprites);

    const float rotation2 = facing + leftArm + torso;
    const float rotation3 = facing + rightArm + torso;
    const bool armSymmetrical = mechTree->gestures[currentGesture].armSymmetrical != 0;
    // A mirrored gesture facing left draws each arm from the other arm's sprites.
    const bool swapArms = armSymmetrical && wrapRotation(rotation3) < 0.0;

    if (unknown12C != 0)
    {
        partShape[MECH_PART_RIGHT_ARM] = nullptr;
    }
    else
    {
        partShape[MECH_PART_RIGHT_ARM] =
            mechTree->getGesture(upperGesture, swapArms ? 3 : 2, rotation2, facing, reverse[MECH_PART_RIGHT_ARM],
                                 frameRate[MECH_PART_RIGHT_ARM], visible, largeSprites);
    }

    if (unknown130 != 0)
    {
        partShape[MECH_PART_LEFT_ARM] = nullptr;
    }
    else if (swapArms)
    {
        partShape[MECH_PART_LEFT_ARM] =
            mechTree->getGesture(upperGesture, 2, rotation3, facing, reverse[MECH_PART_LEFT_ARM],
                                 frameRate[MECH_PART_LEFT_ARM], visible, largeSprites);
    }
    else
    {
        // Faithful: asked for twice, first with the legs' gesture.
        partShape[MECH_PART_LEFT_ARM] =
            mechTree->getGesture(currentGesture, 3, rotation3, facing, reverse[MECH_PART_LEFT_ARM],
                                 frameRate[MECH_PART_LEFT_ARM], visible, largeSprites);
        partShape[MECH_PART_LEFT_ARM] =
            mechTree->getGesture(upperGesture, 3, rotation3, facing, reverse[MECH_PART_LEFT_ARM],
                                 frameRate[MECH_PART_LEFT_ARM], visible, largeSprites);
    }

    if (unknown18C != 0)
    {
        partShape[MECH_PART_TORSO] = nullptr;
    }

    // The shadow, one of 32 facings.
    ElementList->openGroup(static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(-y)))), 1);
    auto* shadow =
        new VFXElement(shadowShapes[0], x, y, calcRotation(facing, 0x20), 0, nullptr, 0, use90PixelSprite != 0 ? 1 : 0);

    // Port fix: the original copies the debug name through a null element too.
    if (shadow != nullptr)
    {
        strcpy(shadow->name, use90PixelSprite != 0 ? "mshad2" : "mshad1");
    }

    ElementList->add(shadow);
    mechElements++;

    ElementList->openGroup(static_cast<int16_t>(static_cast<int32_t>(
                               std::floor(static_cast<double>(static_cast<float>(depthFixup) - screenPos.y)))),
                           1);

    for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
    {
        const uint32_t part = partOrder[i];

        if (partShape[part] == nullptr)
        {
            continue;
        }

        uint8_t* fadeTable = nullptr;

        if (fadeTableIndex != -1 && fadeTableIndex >= 0)
        {
            fadeTable = gamePalette->fadePalettes + (fadeTableIndex + gamePalette->numBitmapHazeLevels * 2) * 0x100;
        }

        // Walking and running: a mirrored part runs half a cycle off the part it follows, so the stride
        // alternates; an unmirrored one keeps in step with it.
        const int32_t gesture = currentGesture;
        const bool combatAnim = inCombatMode != 0 || combatModeRaising != 0 || combatModeLowering != 0;
        unknownC4 = (gesture == 4 || gesture == 7 || gesture == 9 || gesture == 11) ? 1 : 0;

        if (unknownC4 != 0 && reverse[part] != 0)
        {
            const GestureData& data = mechTree->gestures[gesture];
            int32_t reference = -1;
            bool follows = false;

            if (data.armSymmetrical == 0)
            {
                reference = currentFrame[MECH_PART_LEFT_ARM];
                follows = true;
            }
            else if (data.symmetrical == 0)
            {
                reference = currentFrame[MECH_PART_TORSO];
                follows = true;
            }

            if (follows && currentFrame[part] == reference && !combatAnim)
            {
                const int32_t frame = currentFrame[part];
                const uint32_t numFrames = data.numFrames;

                if (unknown140 == 0)
                {
                    const uint32_t shifted = (numFrames >> 1) + static_cast<uint32_t>(frame);
                    currentFrame[part] = static_cast<int32_t>(shifted);
                    const uint32_t count = mechTree->gestures[currentGesture].numFrames;

                    if (count - 1 < shifted)
                    {
                        currentFrame[part] = static_cast<int32_t>(shifted % count);
                    }
                }
                else
                {
                    int32_t shifted = frame - static_cast<int32_t>(numFrames >> 1);
                    currentFrame[part] = shifted;

                    if (shifted < 0)
                    {
                        shifted = shifted < 0 ? -shifted : shifted;
                        const uint32_t count = mechTree->gestures[currentGesture].numFrames;
                        const uint32_t wrapped = static_cast<uint32_t>(shifted) % count;
                        currentFrame[part] = static_cast<int32_t>(count - wrapped);
                    }
                }
            }
        }

        unknownC4 = (currentGesture == 4 || currentGesture == 7 || currentGesture == 9 || currentGesture == 11) ? 1 : 0;

        if (unknownC4 != 0 && reverse[part] == 0)
        {
            const GestureData& data = mechTree->gestures[currentGesture];
            int32_t reference = 0;
            bool follows = false;

            if (data.armSymmetrical == 0)
            {
                reference = currentFrame[MECH_PART_LEFT_ARM];
                follows = true;
            }
            else if (data.symmetrical == 0)
            {
                reference = currentFrame[MECH_PART_TORSO];
                follows = true;
            }

            if (follows && currentFrame[part] != reference && !combatAnim)
            {
                currentFrame[part] = reference;
            }
        }

        if (currentFrame[part] < 0)
        {
            currentFrame[part] = 0;
        }

        auto* element = new VFXElement(partShape[part]->frameList, screenPos.x, screenPos.y, currentFrame[part],
                                       reverse[part], fadeTable, 1, 0);

        // Port fix: the original writes the debug names through a null element too, and "%i" can overrun name2.
        if (element != nullptr)
        {
            strcpy(element->name, "mactor");

            if (unknown38 == nullptr)
            {
                strcpy(element->name2, "unknown");
            }
            else
            {
                snprintf(element->name2, sizeof(element->name2), "%i", unknown38->getObjectType()->objTypeNum);
            }
        }

        ElementList->add(element);
        mechElements++;
    }

    // Selection: -1 and 1 draw the bars; 3 white brackets; anything else the alignment's brackets.
    const int32_t selected = owner->selected;
    bool attackerBars = true;

    if (selected == -1 || selected == 1)
    {
        drawBars();
        attackerBars = false;
    }
    else if (selected == 3)
    {
        recalcBounds(eye);
        drawSelectBrackets(0xf8);
    }
    else if (selected != 0)
    {
        recalcBounds(eye);
        GameObject* obj = owner;
        const int32_t alignment = obj->getAlignment();

        if (alignment == -1)
        {
            drawSelectBrackets(0xfd);
        }
        else if (alignment == 0)
        {
            drawSelectBrackets(0xfe);
        }
        else if (alignment == 1)
        {
            drawSelectBrackets(obj->getAlignment() == homeTeam->alignment ? 0xfc : 0xfb);
        }
    }

    if (attackerBars && owner->getNumAttackers() > 0)
    {
        recalcBounds(eye);
        drawBars();
    }

    if (highlighting != 0)
    {
        if (highlightTime > 0.0f)
        {
            highlightTime -= frameLength;
            drawSelectBox(0xfc);
            return 0;
        }

        highlighted = 1;
    }

    return 0;
}

auto MechActor::update() -> int32_t
{
    auto* mech = static_cast<BattleMech*>(owner);
    SpriteTree* tree = mechTree;
    goalPending = 0;

    // Moving with nowhere to go: stand.
    if (checkStop() != 0)
    {
        currentGesture = GESTURE_STAND;
        currentStateGesture = 1;

        if (static_cast<uint8_t>(mech->status) != 4)
        {
            gestureGoal = -1;
            unknownDC = -1;
            inTransition = 0;

            if (mech->isDisabled() == 0)
            {
                mech->status = 0;
            }
        }
    }

    // Start the transition to the goal: the table lists the gestures from the state to it.
    int32_t goal = gestureGoal;

    if (goal != -1 && inTransition == 0 && unknownDC == -1)
    {
        inTransition = 1;
        unknownD8 = 0;
        const int32_t index = (goal + currentStateGesture * 9) * TRANSITION_ROW;
        unknownDC = transitionArray[index];

        if (tree->transitionArray != nullptr)
        {
            unknownDC = tree->transitionArray[index];
        }

        if (unknownDC == -1)
        {
            gestureGoal = -1;
            inTransition = 0;
        }

        goal = gestureGoal;
        unknown13C = 0;

        if (goal > 6)
        {
            unknown13C = 1;
        }
    }

    if (inTransition != 0 && unknown13C != 0)
    {
        // The last gesture played out: on to the next one.
        const int32_t prevGesture = currentGesture;
        currentGesture = unknownDC;
        unknownD8++;
        const int32_t index = unknownD8 + (goal + currentStateGesture * 9) * TRANSITION_ROW;
        unknownDC = transitionArray[index];

        if (tree->transitionArray != nullptr)
        {
            unknownDC = tree->transitionArray[index];
        }

        int32_t startFrame = 0;
        bool checkReverse = true;

        if (unknownDC == -1)
        {
            // Arrived.
            currentStateGesture = goal;

            if (goal == 6)
            {
                currentStateGesture = 1;
            }

            gestureGoal = -1;
            inTransition = 0;

            if (currentStateGesture == 7 || currentStateGesture == 8)
            {
                mech->handleFall(currentStateGesture == 7 ? 1 : 0);
            }

            if (mech->isDisabled() == 0)
            {
                if (currentStateGesture == 1 && static_cast<uint8_t>(mech->status) != 4)
                {
                    mech->status = 0;
                }
                else if (currentStateGesture == 0)
                {
                    mech->status = 5;
                }
            }

            if (unknownDC == -1)
            {
                // A gesture with a negative frame rate plays backwards.
                checkReverse = false;

                if (tree->gestures[currentGesture].frameRate < 0.0)
                {
                    startFrame = static_cast<int32_t>(tree->gestures[currentGesture].numFrames) - 1;
                    unknown140 = 1;
                }
                else
                {
                    startFrame = 0;
                    unknown140 = 0;
                }
            }
        }

        if (checkReverse)
        {
            // The gesture plays backwards when the next one is its reverse result.
            const GestureData& data = tree->gestures[currentGesture];

            if (static_cast<uint32_t>(data.reverseResult) == static_cast<uint32_t>(unknownDC))
            {
                startFrame = static_cast<int32_t>(data.numFrames) - 1;
                unknown140 = 1;
            }
            else
            {
                startFrame = 0;
                unknown140 = 0;
            }
        }

        unknownC4 = 0;
        unknownE8 = nullptr;
        uint32_t gesture = static_cast<uint32_t>(currentGesture);

        if (unknown18C != 0 && gesture != 0x17 && gesture != 0x18)
        {
            inTransition = 0;
            unknown13C = 0;
            gestureGoal = -1;
            currentGesture = static_cast<int32_t>((gesture & 1) + 0x17);
        }

        const MechSpecialInfo& info = *tree->specialInfo;
        // The special frames a gesture starts at, per gesture (-1: none).
        auto setSpecialStart = [this](uint32_t frame)
        {
            if (frame != 0xffffffff)
            {
                unknown16C = 1;
                fallStartFrame = static_cast<int32_t>(frame);
            }
        };

        auto clearCombat = [this]()
        {
            combatModeChanged = 0;
            inCombatMode = 0;
            combatModeRaising = 0;
            combatModeLowering = 0;
        };

        switch (currentGesture)
        {
            case 0:
            case 1:
            case 2:
            {
                unknown174 = 0;
                unknown178 = 0;

                if (unknown168 != 0)
                {
                    turnAround(mech);
                    unknown168 = 0;
                    clearRotations(mech);
                }
                break;
            }
            case 4:
            {
                unknownC4 = 1;
                unknown174 = 0;
                unknown178 = 0;

                if (prevGesture == 3 && info.s_w_to_walk_frame != 0xffffffff)
                {
                    startFrame = static_cast<int32_t>(info.s_w_to_walk_frame);
                }
                break;
            }
            case 7:
            {
                unknownC4 = 1;
                unknown174 = 0;
                unknown178 = 0;

                if (info.specialDuaneFlag != 0 && prevGesture == 6)
                {
                    startFrame = static_cast<int32_t>(tree->gestures[7].numFrames >> 1);
                }
                break;
            }
            case 9:
            {
                unknown174 = 0;
                unknown178 = 0;

                if (info.walk_to_w_s_frame != 0xffffffff)
                {
                    startFrame = static_cast<int32_t>(info.walk_to_w_s_frame);
                }
                break;
            }
            case 11:
            {
                unknownC4 = 0;
                unknown174 = 0;
                unknown178 = 0;
                break;
            }
            case 12:
            case 13:
            case 23:
            case 24:
            {
                unknown174 = 1;
                unknown178 = 1;
                clearCombat();
                break;
            }
            case 14:
            case 15:
            {
                if (unknown16C != 0)
                {
                    startFrame = fallStartFrame;
                }

                if (info.reallyStupidJamieReverseFlag != 0)
                {
                    unknown164 = 1;
                }

                unknown16C = 0;
                unknown174 = 1;
                unknown178 = 1;
                clearRotations(mech);
                clearCombat();
                break;
            }
            case 16:
            case 18:
            {
                setSpecialStart(currentGesture == 16 ? info.r_fb_w_fb_frame : info.s_fb_w_fb_frame);

                if (info.reallyStupidJamieReverseFlag != 0)
                {
                    unknown168 = 1;
                }

                clearRotations(mech);
                unknown174 = 1;
                unknown178 = 1;
                clearCombat();
                break;
            }
            case 17:
            case 19:
            {
                setSpecialStart(currentGesture == 17 ? info.r_ff_w_ff_frame : info.s_ff_w_ff_frame);

                if (info.reallyStupidJamieReverseFlag != 0)
                {
                    unknown164 = 1;
                }

                clearRotations(mech);
                unknown174 = 1;
                unknown178 = 1;
                clearCombat();
                break;
            }
            case GESTURE_JUMP:
            {
                inJump = 1;
                jumpSetup = 0;

                if (mech->getPilot()->curTacOrder.code == TACTICAL_ORDER_JUMPTO_POINT)
                {
                    mech->getPilot()->curTacOrder.stage = 2;
                }

                // Head for the goal: the distance, and the direction in unknown100.
                const vector_3d position = mech->getPosition();
                unknown100.x = jumpGoal.x - position.x;
                unknown100.y = jumpGoal.y - position.y;
                unknown100.z = jumpGoal.z - position.z;
                const double length = std::sqrt((static_cast<double>(unknown100.x) * unknown100.x +
                                                 static_cast<double>(unknown100.y) * unknown100.y) +
                                                static_cast<double>(unknown100.z) * unknown100.z);
                unknown110 = static_cast<float>(length);

                if (length < 0.0 || length > 0.0)
                {
                    unknown100.x = static_cast<float>(unknown100.x / length);
                    unknown100.y = static_cast<float>(unknown100.y / length);
                    unknown100.z = static_cast<float>(unknown100.z / length);
                }

                const float distance = static_cast<float>(unknown110 * 0.3 + unknown110);
                unknown110 = distance;

                // Find the frames in the air: from the bottom of the crouch to the top of the climb.
                float* heights = gestureHeights(mech, GESTURE_JUMP);
                unknownE8 = heights;
                unknown124 = 0;
                unknown128 = 0;
                const GestureData& jump = tree->gestures[GESTURE_JUMP];
                bool descended = false;
                bool climbing = false;

                // Port fix: the original reads a missing height table.
                for (int32_t i = 0; heights != nullptr && i < static_cast<int32_t>(jump.numFrames); i++)
                {
                    const float change = i != 0 ? heights[i] - heights[i - 1] : 0.0f;

                    if (change < 0.0 && !descended)
                    {
                        descended = true;
                    }

                    if (change >= 0.0 && !climbing && descended)
                    {
                        climbing = true;
                        unknown124 = i;
                    }

                    if (change < 0.0 && climbing)
                    {
                        unknown128 = i;
                        break;
                    }
                }

                const int32_t airFrames = (unknown128 - unknown124) + 5;
                unknown11C = 0;
                unknownE4 = 0;
                unknown114 = 1;
                const double speed =
                    (distance / (static_cast<double>(airFrames) / jump.frameRate)) * metersPerWorldUnit;
                jumpSpeed = static_cast<float>(speed);
                jumpParameter = static_cast<float>(distance / speed);
                mech->createJumpFX();
                unknown174 = 0;
                unknown178 = 0;
                break;
            }

            case GESTURE_FALL:
            {
                unknown164 = 1;
                unknownE8 = gestureHeights(mech, GESTURE_FALL);
                unknown174 = 1;
                unknown178 = 1;

                if (info.OtherJamieReverseFlag != 0 || info.reallyStupidJamieReverseFlag != 0)
                {
                    unknown164 = 0;
                }

                if (unknown164 != 0 && info.stupidJamieReverseFlag != 0)
                {
                    turnAround(mech);
                    unknown164 = 0;
                    clearRotations(mech);
                }

                clearCombat();
                break;
            }

            case 22:
            {
                if (unknown164 != 0 && info.stupidJamieReverseFlag == 0)
                {
                    turnAround(mech);
                    unknown164 = 0;
                    clearRotations(mech);
                }

                unknown174 = 1;
                unknown178 = 1;
                clearCombat();

                if (tree->specialInfo->reallyStupidJamieReverseFlag != 0)
                {
                    unknown168 = 1;
                }
                break;
            }
            case 3:
            case 5:
            case 6:
            case 8:
            case 10:
            {
                unknown174 = 0;
                unknown178 = 0;
                break;
            }
            default:
                break;
        }

        unknown13C = 0;

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            currentFrame[i] = startFrame;
        }
    }

    // The gun going up starts the upper body at the change gesture's start, going down at its end.
    if (combatModeChanged != 0 && (combatModeRaising != 0 || combatModeLowering != 0))
    {
        const int32_t frame =
            combatModeRaising != 0 ? 0 : static_cast<int32_t>(tree->gestures[GESTURE_COMBAT_CHANGE].numFrames) - 1;
        currentFrame[MECH_PART_LEFT_ARM] = frame;
        currentFrame[MECH_PART_RIGHT_ARM] = frame;
        currentFrame[MECH_PART_TORSO] = frame;
        combatModeChanged = 0;
    }

    frameRate[MECH_PART_LEGS] = tree->gestures[currentGesture].frameRate;

    if (frameRate[MECH_PART_LEGS] < 0.0)
    {
        frameRate[MECH_PART_LEGS] = -frameRate[MECH_PART_LEGS];
    }

    // Each part's mirroring and rate at its facing.
    const float facing = objectFacing(mech);
    const float torso = mech->torsoRotation;
    const float rotation3 = mech->rightArmRotation + facing + torso;
    const float rotation2 = facing + mech->leftArmRotation + torso;
    tree->setGesture(currentGesture, MECH_PART_LEGS, facing, facing, reverse[MECH_PART_LEGS],
                     frameRate[MECH_PART_LEGS]);
    const int32_t upperGesture = inCombatMode != 0                                     ? GESTURE_COMBAT
                                 : (combatModeRaising != 0 || combatModeLowering != 0) ? GESTURE_COMBAT_CHANGE
                                                                                       : currentGesture;
    tree->setGesture(upperGesture, MECH_PART_TORSO, facing + torso, facing, reverse[MECH_PART_TORSO],
                     frameRate[MECH_PART_TORSO]);

    if (unknown130 == 0)
    {
        tree->setGesture(upperGesture, 3, rotation3, facing, reverse[MECH_PART_LEFT_ARM],
                         frameRate[MECH_PART_LEFT_ARM]);
    }

    if (unknown12C == 0)
    {
        tree->setGesture(upperGesture, 2, rotation2, facing, reverse[MECH_PART_RIGHT_ARM],
                         frameRate[MECH_PART_RIGHT_ARM]);
    }

    const int32_t gesture = currentGesture;

    if (gesture == GESTURE_JUMP)
    {
        if (unknown124 <= currentFrame[MECH_PART_LEGS])
        {
            unknownE0 = jumpSpeed;
            unknownE4 = unknown11C;
            unknown120 = 1;
        }

        if (unknown128 <= currentFrame[MECH_PART_LEGS] && unknown120 != 0)
        {
            unknownE0 = 0.0f;
            unknownE4 = 0;
            unknown120 = 0;
        }
    }

    unknown160 = 0;

    if (currentFrame[MECH_PART_LEGS] == -1)
    {
        currentFrame[MECH_PART_LEGS] = 0;

        if (inCombatMode == 0 && combatModeRaising == 0 && combatModeLowering == 0)
        {
            currentFrame[MECH_PART_TORSO] = 0;
            currentFrame[MECH_PART_RIGHT_ARM] = 0;
            currentFrame[MECH_PART_LEFT_ARM] = 0;
        }
    }
    else if (inTransition == 0 && (gesture == 0x17 || gesture == 0x18))
    {
        // Lying down: hold the last frame.
        const int32_t last = static_cast<int32_t>(tree->gestures[gesture].numFrames) - 1;

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            currentFrame[i] = last;
        }

        unknown160 = 1;
    }
    else
    {
        // The frames to step: from the legs' frame rate, or one at a time in single-step mode.
        int32_t step = 0;

        if (singleStepMode == 0)
        {
            const double interval = 1.0 / frameRate[MECH_PART_LEGS];
            const double time = static_cast<double>(frameLength) + currentTime[MECH_PART_LEGS];
            currentTime[MECH_PART_LEGS] = static_cast<float>(time);

            if (interval <= time)
            {
                step = static_cast<int32_t>(time * frameRate[MECH_PART_LEGS]);
                // Original behaviour (OB-055): the time left is time / step - interval, not time - step * interval.
                currentTime[MECH_PART_LEGS] = static_cast<float>(time / step - interval);
            }

            // The footstep of the fall-down gestures.
            if (currentFrame[MECH_PART_LEGS] == 10 && (gesture == 0xe || gesture == 0xf))
            {
                soundSystem->playDigitalSample(0x1d, 1, owner, 0, 0);
            }
        }
        else if (nextStep != 0)
        {
            step = 1;
        }
        else if (prevStep != 0)
        {
            step = -1;
        }

        for (int32_t i = 0; i < NUM_MECH_PARTS; i++)
        {
            const uint32_t part = partOrder[i];

            // The upper body plays the gun change on its own.
            if (part != 0)
            {
                if (combatModeRaising != 0)
                {
                    currentFrame[part] += step;

                    if (static_cast<int32_t>(tree->gestures[GESTURE_COMBAT_CHANGE].numFrames) <= currentFrame[part])
                    {
                        currentFrame[MECH_PART_TORSO] = 0;
                        currentFrame[MECH_PART_RIGHT_ARM] = 0;
                        currentFrame[MECH_PART_LEFT_ARM] = 0;
                        combatModeRaising = 0;
                        inCombatMode = 1;
                    }

                    continue;
                }

                if (combatModeLowering != 0)
                {
                    const int32_t frame = currentFrame[part] - step;
                    currentFrame[part] = frame;

                    if (frame < 1)
                    {
                        const int32_t legsFrame = currentFrame[MECH_PART_LEGS];
                        combatModeLowering = 0;
                        currentFrame[MECH_PART_TORSO] = legsFrame;
                        currentFrame[MECH_PART_RIGHT_ARM] = legsFrame;
                        currentFrame[MECH_PART_LEFT_ARM] = legsFrame;
                        inCombatMode = 0;
                    }

                    continue;
                }

                if (inCombatMode != 0)
                {
                    continue;
                }
            }

            if (step == 0 || (currentGesture == GESTURE_JUMP && unknown114 == 0))
            {
                continue;
            }

            const int32_t delta = unknown140 != 0 ? -step : step;
            currentFrame[part] += delta;
            const uint32_t numFrames = tree->gestures[currentGesture].numFrames;

            if (static_cast<int32_t>(numFrames) <= currentFrame[part])
            {
                if (inTransition == 0 && unknown14C == 0 && unknown150 == 0)
                {
                    // Loop.
                    currentFrame[part] = static_cast<int32_t>(static_cast<uint32_t>(currentFrame[part]) % numFrames);

                    if (currentGesture == GESTURE_JUMP)
                    {
                        unknown13C = 0;
                    }
                }
                else
                {
                    // Played out: the transition goes on.
                    unknown13C = 1;
                    currentFrame[part] = static_cast<int32_t>(numFrames) - 1;

                    if (currentGesture == GESTURE_JUMP)
                    {
                        inJump = 0;
                        currentStateGesture = 1;

                        if (mech->getPilot()->curTacOrder.code == TACTICAL_ORDER_JUMPTO_POINT)
                        {
                            mech->getPilot()->curTacOrder.stage = 3;
                        }
                    }
                }
            }

            // Some transitions go on from a special frame instead of the gesture's end.
            const int32_t transition = inTransition;

            if (transition != 0)
            {
                const MechSpecialInfo& info = *tree->specialInfo;
                const int32_t frame = currentFrame[part];

                if (currentGesture == 4 && unknownDC == 6)
                {
                    unknown13C = static_cast<int32_t>(info.walk_to_w_r_frame) <= frame ? 1 : 0;
                }

                if (currentGesture == 4 && unknownDC == 5 && info.walk_to_w_s_frame != 0xffffffff)
                {
                    unknown13C = static_cast<int32_t>(info.walk_to_w_s_frame) <= frame ? 1 : 0;
                }

                if (unknownDC == 8 && currentGesture == 7)
                {
                    unknown13C = static_cast<int32_t>(info.run_to_r_w_frame) <= frame ? 1 : 0;
                }
            }

            if (currentFrame[part] < 0)
            {
                if (transition == 0)
                {
                    // Backwards past the start: wrap to the end.
                    const int32_t frame = currentFrame[part];
                    const uint32_t magnitude = static_cast<uint32_t>(frame < 0 ? -frame : frame);
                    const uint32_t count = tree->gestures[currentGesture].numFrames;
                    currentFrame[part] = static_cast<int32_t>(count - magnitude % count);
                }
                else
                {
                    unknown13C = 1;
                    currentFrame[part] = 0;
                }
            }
        }
    }

    // Legs first, then the arm on the far side, the torso and the near arm.
    partOrder[0] = MECH_PART_LEGS;
    partOrder[1] = MECH_PART_RIGHT_ARM;
    partOrder[2] = MECH_PART_TORSO;
    partOrder[3] = MECH_PART_LEFT_ARM;

    if (rotation3 < 0.0)
    {
        partOrder[1] = MECH_PART_LEFT_ARM;
        partOrder[3] = MECH_PART_RIGHT_ARM;
    }

    if (currentGesture == GESTURE_FALL)
    {
        partOrder[1] = MECH_PART_LEFT_ARM;
        partOrder[3] = MECH_PART_RIGHT_ARM;
    }

    if (visible != 0 && owner->isCaptured() != 0 && highlighted == 0 && highlighting == 0)
    {
        highlighting = 1;
        highlightTime = 3.0f;
    }

    return 1;
}

auto destroyMechShadows() -> void
{
    if (MechActor::shadowShapes == nullptr || MechActor::numShadows == 0)
    {
        return;
    }

    for (int32_t i = 0; i < MechActor::numShadows; i++)
    {
        systemHeap->free(MechActor::shadowShapes[i]);
        MechActor::shadowShapes[i] = nullptr;
    }

    systemHeap->free(MechActor::shadowShapes);
    MechActor::shadowShapes = nullptr;
}

auto MechActor::destroy() -> void
{
    mechTree->removeUsers(this);
    appearanceTypeList->removeAppearance(mechTree);
}

auto DrawBox(float left, float top, float right, float bottom) -> void
{
    addLine(vector_2d(left, top), vector_2d(right, top), 0x10);
    addLine(vector_2d(right, top), vector_2d(right, bottom), 0x10);
    addLine(vector_2d(right, bottom), vector_2d(left, bottom), 0x10);
    addLine(vector_2d(left, top), vector_2d(left, bottom), 0x10);
}

auto MechActor::drawBars() -> void
{
    // Port: the bar is an overlay, on the screen over the view: it follows the sprite through the zoom, its size
    // doesn't change.
    const float scale = eyeScale();
    const float barWidth = scale * 38.0f;
    const float barHeight = scale * 4.0f;
    const float barY = (MCOverlayY(upperLeft.y) - scale * 24.0f) - barHeight;
    const float barX = static_cast<float>(std::floor(static_cast<double>(MCOverlayX(screenPos.x) - barWidth * 0.5f)));

    auto* mech = static_cast<Mover*>(owner);

    if (mech == nullptr || mech->weaponEffectiveness < 0.0f || mech->maxWeaponEffectiveness < mech->weaponEffectiveness)
    {
        return;
    }

    const float health = mech->getTotalEffectiveness();
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

    ElementList->openGroup(-50000, 1);
    PolyElementData data;
    data.numVertices = 0;
    data.textureMapOff = 0;
    data.unknown98 = 0;
    data.texture = nullptr;
    data.textureWidth = 0;
    data.textureHeight = 0;
    data.fadeTable = nullptr;
    data.translate = 0;
    data.statusBar = 1;
    data.barColor = barColor;
    auto floorInt = [](float value)
    {
        return static_cast<int32_t>(static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(value)))));
    };

    data.vertices[0].x = floorInt(barX - 1.0f);
    data.vertices[0].y = floorInt(barY - 1.0f);
    data.vertices[1].x = floorInt(barX + barWidth + 1.0f);
    data.vertices[1].y = floorInt(barY + barHeight + 1.0f);
    const int32_t barLength = floorInt(health * barWidth);
    data.barPercent = barLength;

    // A mech that isn't quite dead shows at least one pixel.
    if (static_cast<double>(barLength) < 1.0 && health > 0.001)
    {
        data.barPercent = 1;
    }

    if (mech->isDisabled() == 0 && mech->isDestroyed() == 0)
    {
        ElementList->add(new PolygonElement(&data, -50000));
    }
}

auto MechActor::drawTargetDamage() -> void
{
    GameObject* obj = owner;

    if (obj == nullptr || obj->objectClass != BATTLEMECH)
    {
        return;
    }

    auto* mech = static_cast<BattleMech*>(obj);
    // The enemy mechs.
    ObjectQueueNode* enemies = mech->getAlignment() == 1 ? clanMechList : innerSphereMechList;

    // A ring around the mech, with a line out to each enemy it can see, as long as its expected damage to it.
    // Port: overlays, on the screen over the view: the ring follows the sprite through the zoom.
    vector_2d center =
        MCOverlayPoint(vector_2d((upperLeft.x + lowerRight.x) * 0.5f, (upperLeft.y + lowerRight.y) * 0.5f));
    const float radius = std::sqrt((upperLeft.x - lowerRight.x) * (upperLeft.x - lowerRight.x) +
                                   (upperLeft.y - lowerRight.y) * (upperLeft.y - lowerRight.y)) *
                         0.375f * MCOverlay.ScaleX;
    const vector_2d ownPos = MCOverlayPoint(screenPos);
    vector_2d size(radius, radius);
    BaseObject* current = nullptr;

    while (enemies->Traverse(current) != nullptr)
    {
        auto* target = static_cast<GameObject*>(current);

        if (target->isDisabled() != 0)
        {
            continue;
        }

        if (target->getContactType(mech->getTeam()->id) != 1)
        {
            continue;
        }

        const float damage = mech->calcExpectedTargetDamage(target);

        if (!(damage > 0.0f))
        {
            continue;
        }

        ElementList->openGroup(-50000, 1);
        ElementList->add(new EllipseElement(center, size, 0xb, -50000));

        const vector_2d targetPos = MCOverlayPoint(target->getScreenPos(0));
        const double dx = static_cast<double>(targetPos.x) - ownPos.x;
        const double dy = static_cast<double>(targetPos.y) - ownPos.y;
        const double angle = std::atan(dy / dx);
        const float c = static_cast<float>(std::fabs(std::cos(angle)));
        const float s = static_cast<float>(std::fabs(std::sin(angle)));
        const float signX = dx <= 0.0 ? -1.0f : 1.0f;
        const float signY = static_cast<float>(dy) <= 0.0f ? -1.0f : 1.0f;
        vector_2d start(signX * c * radius + center.x, signY * s * radius + center.y);
        const float length = (damage / mech->maxTargetDamage) * 60.0f;
        vector_2d end(signX * length * c + start.x, signY * length * s + start.y);
        ElementList->add(new LineElement(start, end, 0xef, nullptr, -50000, -1));
    }
}
