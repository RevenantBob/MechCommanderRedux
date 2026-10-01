#include "stdafx.h"
#include "sprite/spritree.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"

uint32_t packetFinderArray[28] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  4,  5,  9, 10, 11,
                                  12, 13, 14, 15, 16, 17, 18, 19, 20, 13, 12, 0, 21, 22};

namespace
{
    /// <summary>The number of entries of the shape list (all four parts).</summary>
    constexpr int32_t NUM_TREE_SHAPES = 0x96f;
    /// <summary>Where each part's shapes start in the shape list.</summary>
    constexpr uint32_t LEG_SHAPES = 0;
    constexpr uint32_t TORSO_SHAPES = 0x33c / 4;
    constexpr uint32_t RIGHT_ARM_SHAPES = 0xebc / 4;
    constexpr uint32_t LEFT_ARM_SHAPES = 0x1a3c / 4;
    /// <summary>Leg rotations per gesture.</summary>
    constexpr int32_t NUM_LEG_ROTATIONS = 9;

    /// <summary>Upper-body rotations per gesture: 17 when the part is mirrored, else 32.</summary>
    auto partRotations(bool symmetrical) -> int32_t
    {
        return symmetrical ? 0x11 : 0x20;
    }

    /// <summary>
    /// Wraps <paramref name="rotation"/> into -180..180, then mirrors it (setting <paramref name="reverse"/>) when
    /// the part is symmetrical, or wraps negatives to 0..360 when not. <paramref name="frameRate"/> gets the
    /// gesture's rate, made positive.
    /// </summary>
    auto facingAndRate(const GestureData& data, bool symmetrical, float& rotation, int& reverse, float& frameRate)
        -> void
    {
        if (rotation > 180.0)
        {
            rotation = (rotation - 180.0f) - 180.0f;
        }
        else if (rotation < -180.0)
        {
            rotation = rotation + 180.0f + 180.0f;
        }

        int isReversed = 0;

        if (rotation < 0.0 && symmetrical)
        {
            rotation = -rotation;
            isReversed = 1;
        }
        else if (rotation < 0.0 && !symmetrical)
        {
            rotation = static_cast<float>(rotation + 360.0);
        }

        reverse = isReversed;
        frameRate = data.frameRate;

        if (frameRate < 0.0)
        {
            frameRate = -frameRate;
        }
    }

    /// <summary>Whether <paramref name="part"/> (0 legs, 1 torso, 2 and 3 arms) is mirrored in the gesture.</summary>
    auto partSymmetrical(const GestureData& data, int32_t part) -> bool
    {
        return (data.symmetrical != 0 && part == 1) || part == 0 || (data.armSymmetrical != 0 && part > 1);
    }

    /// <summary>
    /// Loads (or, when the shape heap is half full or more, only reads ahead) the shapes of one part of
    /// <paramref name="gesture"/>.
    /// </summary>
    auto preloadPart(SpriteTree* tree, uint32_t fileNumber, int32_t part, uint32_t listStart, int32_t gesture,
                     int32_t numRotations) -> void
    {
        for (int32_t i = 0; i < numRotations; i++)
        {
            SpriteManager* manager = spriteManager;

            if (fileNumber == 0xffffffff || manager == nullptr)
            {
                continue;
            }

            const uint32_t packet =
                static_cast<uint32_t>(static_cast<int32_t>(packetFinderArray[gesture]) * numRotations + i);
            const int32_t percentFree =
                static_cast<int32_t>(static_cast<double>(static_cast<int32_t>(manager->shapeHeap->totalCoreLeft())) /
                                     static_cast<int32_t>(manager->shapeHeapSize) * 100.0);

            if (percentFree < 51)
            {
                manager->touchMechShapeData(fileNumber, packet, part);
            }
            else
            {
                tree->shapeList[listStart + packet] = manager->getMechShapeData(fileNumber, packet, part, 1, tree, 0);
            }
        }
    }
}

auto SpriteTree::init(File* apprFile, uint32_t fileSize, uint32_t) -> int32_t
{
    gesturesPreloaded = 0;
    const int32_t result = loadIniFile(apprFile, fileSize);
    numShapes = NUM_TREE_SHAPES;
    // Port fix: sized by the port's pointer size (the original: count * 4).
    shapeList =
        static_cast<Shape**>(spriteManager->mallocDataRAM(NUM_TREE_SHAPES * static_cast<uint32_t>(sizeof(Shape*))));

    if (shapeList == nullptr)
    {
        return -1;
    }

    for (int32_t i = 0; i < NUM_TREE_SHAPES; i++)
    {
        shapeList[i] = nullptr;
    }

    return result;
}

auto SpriteTree::removeShape(Shape* shape) -> void
{
    for (int32_t i = 0; i < numShapes; i++)
    {
        if (shapeList[i] == shape)
        {
            shapeList[i] = nullptr;
        }
    }

    // The users are mech appearances, whose four part shapes are at +0x40..+0x4c.
    for (AppearanceUser* user = userList; user != nullptr; user = user->next)
    {
        auto* partShapes = reinterpret_cast<Shape**>(static_cast<uint8_t*>(user->user) + 0x40);

        for (int32_t part = 0; part < 4; part++)
        {
            if (partShapes[part] == shape)
            {
                partShapes[part] = nullptr;
            }
        }
    }
}

auto SpriteTree::preloadGestures(int32_t, float) -> void
{
    if (gesturesPreloaded != 0)
    {
        return;
    }

    // Stand, walk and run.
    constexpr int32_t preloadList[3] = {2, 4, 7};
    // Faithful: the torso's "symmetrical" is never cleared once a gesture sets it.
    bool torsoSymmetrical = false;

    for (int32_t gesture : preloadList)
    {
        preloadPart(this, legFileNumber, 0, LEG_SHAPES, gesture, NUM_LEG_ROTATIONS);
        const GestureData& data = gestures[gesture];

        if (data.symmetrical != 0)
        {
            torsoSymmetrical = true;
        }

        preloadPart(this, torsoFileNumber, 1, TORSO_SHAPES, gesture, partRotations(torsoSymmetrical));
        const bool armSymmetrical = data.armSymmetrical != 0;
        preloadPart(this, rightArmFileNumber, 2, RIGHT_ARM_SHAPES, gesture, partRotations(armSymmetrical));
        preloadPart(this, leftArmFileNumber, 3, LEFT_ARM_SHAPES, gesture, partRotations(armSymmetrical));
    }

    gesturesPreloaded = 1;
}

auto SpriteTree::loadIniFile(File* apprFile, uint32_t fileSize) -> int32_t
{
    constexpr int32_t NO_RAM = static_cast<int32_t>(0xbeef0007);
    constexpr int32_t NO_GESTURE_RAM = static_cast<int32_t>(0xbeef000a);

    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.seekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    char name[52];
    result = iniFile.readIdString("Name", name, 0x31);

    if (result != 0)
    {
        return result;
    }

    treeInfo = static_cast<SpriteTreeInfo*>(spriteManager->mallocDataRAM(sizeof(SpriteTreeInfo)));

    if (treeInfo == nullptr)
    {
        return NO_RAM;
    }

    specialInfo = static_cast<MechSpecialInfo*>(spriteManager->mallocDataRAM(sizeof(MechSpecialInfo)));

    if (specialInfo == nullptr)
    {
        return NO_RAM;
    }

    if ((result = iniFile.readIdULong("legFileNumber", legFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("torsoFileNumber", torsoFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("rightArmFileNumber", rightArmFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("leftArmFileNumber", leftArmFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.seekBlock("Parts")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdUChar("NumParts", treeInfo->numParts)) != 0)
    {
        return result;
    }

    if ((result = iniFile.seekBlock("SpecialInfo")) != 0)
    {
        return result;
    }

    MechSpecialInfo& info = *specialInfo;

    if ((result = iniFile.readIdFloat("fb_d_xlat", info.fb_d_xlat)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("jumpAirborne", info.jumpAirborne)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("jumpHold", info.jumpHold)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("jumpStartLandTime", info.jumpStartLandTime)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("jumpMaxDistance", info.jumpMaxDistance)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("jumpGravity", info.jumpGravity)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdFloat("jumpStartVel", info.jumpStartVel)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("r_fb_w_fb_frame", info.r_fb_w_fb_frame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("r_ff_w_ff_frame", info.r_ff_w_ff_frame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("s_fb_w_fb_frame", info.s_fb_w_fb_frame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("s_ff_w_ff_frame", info.s_ff_w_ff_frame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("walk_to_w_r_frame", info.walk_to_w_r_frame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdULong("run_to_r_w_frame", info.run_to_r_w_frame)) != 0)
    {
        return result;
    }

    if (iniFile.readIdULong("walk_to_w_s_frame", info.walk_to_w_s_frame) != 0)
    {
        info.walk_to_w_s_frame = 0xffffffff;
    }

    if (iniFile.readIdULong("s_w_to_walk_frame", info.s_w_to_walk_frame) != 0)
    {
        info.s_w_to_walk_frame = 0xffffffff;
    }

    // Optional flags: 0 when absent.
    auto readFlag = [&iniFile](const char* key) -> uint32_t
    {
        uint32_t value = 0;
        iniFile.readIdULong(key, value);
        return value;
    };

    info.stupidJamieReverseFlag = readFlag("stupidJamieReverseFlag");
    info.OtherJamieReverseFlag = readFlag("OtherJamieReverseFlag");
    info.reallyStupidJamieReverseFlag = readFlag("reallyStupidJamieReverseFlag");
    info.specialDuaneFlag = readFlag("specialDuaneFlag");
    info.standToGunPose = readFlag("standToGunPose");
    info.walkToGunPose = readFlag("walkToGunPose");
    info.runToGunPose = readFlag("runToGunPose");

    if (iniFile.seekBlock("TransitionTable") == 0)
    {
        transitionArray = static_cast<char*>(spriteManager->mallocDataRAM(0x32a));

        if (transitionArray == nullptr)
        {
            return NO_GESTURE_RAM;
        }

        if ((result = iniFile.readIdCharArray("TransitionArray", transitionArray, 0x32a)) != 0)
        {
            return result;
        }
    }
    else
    {
        transitionArray = nullptr;
    }

    if ((result = iniFile.seekBlock("Gestures")) != 0)
    {
        return result;
    }

    if ((result = iniFile.readIdUChar("NumGestures", treeInfo->numGestures)) != 0)
    {
        return result;
    }

    const uint32_t numGestures = treeInfo->numGestures;
    gestures = static_cast<GestureData*>(spriteManager->mallocDataRAM(numGestures * sizeof(GestureData)));

    if (gestures == nullptr)
    {
        return NO_GESTURE_RAM;
    }

    for (int32_t i = 0; i < static_cast<int32_t>(numGestures); i++)
    {
        char blockName[20];
        sprintf(blockName, "Gestures%d", i);

        if ((result = iniFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        GestureData& data = gestures[i];

        if ((result = iniFile.readIdUChar("State", data.state)) != 0)
        {
            return result;
        }

        uint32_t numFrames = 0;

        if ((result = iniFile.readIdULong("NumFrames", numFrames)) != 0)
        {
            return result;
        }

        data.numFrames = numFrames;
        float frameRate = 0.0f;

        if ((result = iniFile.readIdFloat("FrameRate", frameRate)) != 0)
        {
            return result;
        }

        data.frameRate = frameRate;
        uint32_t basePacketNumber = 0;

        if ((result = iniFile.readIdULong("BasePacketNumber", basePacketNumber)) != 0)
        {
            return result;
        }

        data.basePacketNumber = basePacketNumber;

        if ((result = iniFile.readIdUCharArray("NumRotations", data.numRotations, treeInfo->numParts)) != 0)
        {
            return result;
        }

        if ((result = iniFile.readIdUChar("Symmetrical", data.symmetrical)) != 0)
        {
            return result;
        }

        if (iniFile.readIdUChar("ArmSymmetrical", data.armSymmetrical) != 0)
        {
            data.armSymmetrical = 1;
        }

        if ((result = iniFile.readIdUChar("ForwardResult", data.forwardResult)) != 0)
        {
            return result;
        }

        if ((result = iniFile.readIdUChar("ReverseResult", data.reverseResult)) != 0)
        {
            return result;
        }

        float velocity = 0.0f;

        if ((result = iniFile.readIdFloat("StartVelocity", velocity)) != 0)
        {
            return result;
        }

        data.startVelocity = velocity;

        if ((result = iniFile.readIdFloat("EndVelocity", velocity)) != 0)
        {
            return result;
        }

        data.endVelocity = velocity;
    }

    iniFile.close();
    return 0;
}

auto SpriteTree::setGesture(int32_t gesture, int32_t part, float rotation, float, int& reverse, float& frameRate)
    -> void
{
    reverse = 0;
    frameRate = 0.0f;
    const GestureData& data = gestures[gesture];

    if (data.numFrames == 0)
    {
        return;
    }

    // The original also works out the rotation index here, then drops it.
    facingAndRate(data, partSymmetrical(data, part), rotation, reverse, frameRate);
}

auto SpriteTree::getGesture(int32_t gesture, int32_t part, float rotation, float, int& reverse, float& frameRate, int,
                            int zoomedOut) -> Shape*
{
    reverse = 0;
    frameRate = 0.0f;
    const GestureData& data = gestures[gesture];

    if (data.numFrames == 0)
    {
        return nullptr;
    }

    const bool symmetrical = partSymmetrical(data, part);
    facingAndRate(data, symmetrical, rotation, reverse, frameRate);

    const uint32_t numRotations = data.numRotations[part];
    int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<int32_t>(numRotations + 1)) * rotation * (1.0 / 360.0))));

    if (rotationIndex >= 0x20 && rotation > 180.0 && !symmetrical)
    {
        rotationIndex = 0x1f;

        if (rotationIndex == static_cast<int32_t>(numRotations >> 1))
        {
            reverse = 0;
        }
    }
    else if (rotationIndex == 0)
    {
        reverse = 0;
    }
    else if (rotationIndex == static_cast<int32_t>(numRotations >> 1))
    {
        // Straight back: no mirroring.
        reverse = 0;
    }

    // Gestures 14..19 and 21..24 only have every fourth facing.
    int32_t index = rotationIndex;

    if ((gesture > 0xd && gesture < 0x14) || (gesture > 0x14 && gesture < 0x19))
    {
        if (rotationIndex < 2)
        {
            index = 0;
        }
        else if (rotationIndex < 6)
        {
            index = 4;
        }
        else if (rotationIndex < 10)
        {
            index = 8;
        }
        else if (rotationIndex < 0xe)
        {
            index = 0xc;
        }
        else if (rotationIndex < 0x12)
        {
            index = 0x10;
        }
        else if (rotationIndex < 0x16)
        {
            index = 0x14;
        }
        else if (rotationIndex > 0x19)
        {
            index = 0x1c;
        }
        else
        {
            index = 0x18;
        }
    }

    uint32_t fileNumber;
    int32_t filePart;
    uint32_t listStart;
    uint32_t packet;
    const int32_t finder = static_cast<int32_t>(packetFinderArray[gesture]);

    switch (part)
    {
        case 0:
        {
            fileNumber = legFileNumber;
            filePart = 0;
            listStart = LEG_SHAPES;
            packet = static_cast<uint32_t>(index + finder * NUM_LEG_ROTATIONS);
            break;
        }
        case 1:
        {
            fileNumber = torsoFileNumber;
            filePart = 1;
            listStart = TORSO_SHAPES;
            packet = static_cast<uint32_t>(finder * partRotations(symmetrical) + index);
            break;
        }
        case 2:
        {
            fileNumber = leftArmFileNumber;
            filePart = 3;
            listStart = LEFT_ARM_SHAPES;
            packet = static_cast<uint32_t>(finder * partRotations(symmetrical) + index);
            break;
        }
        case 3:
        {
            fileNumber = rightArmFileNumber;
            filePart = 2;
            listStart = RIGHT_ARM_SHAPES;
            packet = static_cast<uint32_t>(finder * partRotations(symmetrical) + index);
            break;
        }
        default:
            return nullptr;
    }

    if (fileNumber == 0xffffffff || spriteManager == nullptr)
    {
        return nullptr;
    }

    Shape* shape = shapeList[listStart + packet];

    if (shape != nullptr)
    {
        shape->lastTurnUsed = turn;
        return shape;
    }

    shape = spriteManager->getMechShapeData(fileNumber, packet, filePart, turn, this, zoomedOut);
    shapeList[listStart + packet] = shape;
    return shape;
}

auto SpriteTree::destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < numShapes; i++)
    {
        if (shapeList[i] != nullptr)
        {
            shapeList[i]->owner = nullptr;
        }
    }

    if (shapeList != nullptr)
    {
        spriteManager->freeDataRAM(shapeList);
        shapeList = nullptr;
    }

    if (gestures != nullptr)
    {
        spriteManager->freeDataRAM(gestures);
        gestures = nullptr;
    }

    if (specialInfo != nullptr)
    {
        spriteManager->freeDataRAM(specialInfo);
        specialInfo = nullptr;
    }

    if (treeInfo != nullptr)
    {
        spriteManager->freeDataRAM(treeInfo);
        treeInfo = nullptr;
    }

    if (transitionArray != nullptr)
    {
        spriteManager->freeDataRAM(transitionArray);
        transitionArray = nullptr;
    }
}
