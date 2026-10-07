#include "stdafx.h"
#include "sprite/spritree.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "sprite/sprtmgr.h"
#include "sprite/vfxshape.h"

uint32_t PacketFinderArray[28] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  4,  5,  9, 10, 11,
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
    auto PartRotations(bool symmetrical) -> int32_t
    {
        return symmetrical ? 0x11 : 0x20;
    }

    /// <summary>
    /// Wraps <paramref name="rotation"/> into -180..180, then mirrors it (setting <paramref name="reverse"/>) when
    /// the part is symmetrical, or wraps negatives to 0..360 when not. <paramref name="frameRate"/> gets the
    /// gesture's rate, made positive.
    /// </summary>
    auto FacingAndRate(const MCGestureData& data, bool symmetrical, float& rotation, int& reverse, float& frameRate)
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
        frameRate = data.FrameRate;

        if (frameRate < 0.0)
        {
            frameRate = -frameRate;
        }
    }

    /// <summary>Whether <paramref name="part"/> (0 legs, 1 torso, 2 and 3 arms) is mirrored in the gesture.</summary>
    auto PartSymmetrical(const MCGestureData& data, int32_t part) -> bool
    {
        return (data.Symmetrical != 0 && part == 1) || part == 0 || (data.ArmSymmetrical != 0 && part > 1);
    }

    /// <summary>
    /// Loads (or, when the shape heap is half full or more, only reads ahead) the shapes of one part of
    /// <paramref name="gesture"/>.
    /// </summary>
    auto PreloadPart(MCSpriteTree* tree, uint32_t fileNumber, int32_t part, uint32_t listStart, int32_t gesture,
                     int32_t numRotations) -> void
    {
        for (int32_t i = 0; i < numRotations; i++)
        {
            MCSpriteManager* manager = SpriteManager;

            if (fileNumber == 0xffffffff || manager == nullptr)
            {
                continue;
            }

            const uint32_t packet =
                static_cast<uint32_t>(static_cast<int32_t>(PacketFinderArray[gesture]) * numRotations + i);
            // The original only read ahead (touchMechShapeData) once its shape heap was half full; the port's cache
            // has no fill, so the shapes are always loaded. They load as the large (90-pixel) art, which
            // MechActor::render asks for: cameraScale is pinned to 100. The original loaded the small art here (it
            // started zoomed out), and the cached shape then stood in for the large one, so a preloaded gesture drew at
            // half size.
            tree->ShapeList[listStart + packet] = manager->GetMechShapeData(fileNumber, packet, part, 1, tree, 1);
        }
    }
}

auto MCSpriteTree::Init(MCFile* apprFile, uint32_t fileSize, uint32_t) -> int32_t
{
    GesturesPreloaded = 0;
    const int32_t result = LoadIniFile(apprFile, fileSize);
    NumShapes = NUM_TREE_SHAPES;
    // Port fix: sized by the port's pointer size (the original: count * 4).
    ShapeList =
        static_cast<MCShape**>(SpriteManager->MallocDataRam(NUM_TREE_SHAPES * static_cast<uint32_t>(sizeof(MCShape*))));

    if (ShapeList == nullptr)
    {
        return -1;
    }

    for (int32_t i = 0; i < NUM_TREE_SHAPES; i++)
    {
        ShapeList[i] = nullptr;
    }

    return result;
}

auto MCSpriteTree::RemoveShape(MCShape* shape) -> void
{
    for (int32_t i = 0; i < NumShapes; i++)
    {
        if (ShapeList[i] == shape)
        {
            ShapeList[i] = nullptr;
        }
    }

    // The users are mech appearances, whose four part shapes are at +0x40..+0x4c.
    for (MCAppearanceUser* user = UserList; user != nullptr; user = user->Next)
    {
        auto* partShapes = reinterpret_cast<MCShape**>(static_cast<uint8_t*>(user->User) + 0x40);

        for (int32_t part = 0; part < 4; part++)
        {
            if (partShapes[part] == shape)
            {
                partShapes[part] = nullptr;
            }
        }
    }
}

auto MCSpriteTree::PreloadGestures(int32_t, float) -> void
{
    if (GesturesPreloaded != 0)
    {
        return;
    }

    // Stand, walk and run.
    constexpr int32_t preloadList[3] = {2, 4, 7};
    // Faithful: the torso's "symmetrical" is never cleared once a gesture sets it.
    bool torsoSymmetrical = false;

    for (int32_t gesture : preloadList)
    {
        PreloadPart(this, LegFileNumber, 0, LEG_SHAPES, gesture, NUM_LEG_ROTATIONS);
        const MCGestureData& data = Gestures[gesture];

        if (data.Symmetrical != 0)
        {
            torsoSymmetrical = true;
        }

        PreloadPart(this, TorsoFileNumber, 1, TORSO_SHAPES, gesture, PartRotations(torsoSymmetrical));
        const bool armSymmetrical = data.ArmSymmetrical != 0;
        PreloadPart(this, RightArmFileNumber, 2, RIGHT_ARM_SHAPES, gesture, PartRotations(armSymmetrical));
        PreloadPart(this, LeftArmFileNumber, 3, LEFT_ARM_SHAPES, gesture, PartRotations(armSymmetrical));
    }

    GesturesPreloaded = 1;
}

auto MCSpriteTree::LoadIniFile(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    constexpr int32_t noRam = static_cast<int32_t>(0xbeef0007);
    constexpr int32_t noGestureRam = static_cast<int32_t>(0xbeef000a);

    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    result = iniFile.SeekBlock("Main Info");

    if (result != 0)
    {
        return result;
    }

    char name[52];
    result = iniFile.ReadIdString("Name", name, 0x31);

    if (result != 0)
    {
        return result;
    }

    TreeInfo = static_cast<MCSpriteTreeInfo*>(SpriteManager->MallocDataRam(sizeof(MCSpriteTreeInfo)));

    if (TreeInfo == nullptr)
    {
        return noRam;
    }

    SpecialInfo = static_cast<MCMechSpecialInfo*>(SpriteManager->MallocDataRam(sizeof(MCMechSpecialInfo)));

    if (SpecialInfo == nullptr)
    {
        return noRam;
    }

    if ((result = iniFile.ReadIdULong("legFileNumber", LegFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("torsoFileNumber", TorsoFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("rightArmFileNumber", RightArmFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("leftArmFileNumber", LeftArmFileNumber)) != 0)
    {
        return result;
    }

    if ((result = iniFile.SeekBlock("Parts")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdUChar("NumParts", TreeInfo->NumParts)) != 0)
    {
        return result;
    }

    if ((result = iniFile.SeekBlock("SpecialInfo")) != 0)
    {
        return result;
    }

    MCMechSpecialInfo& info = *SpecialInfo;

    if ((result = iniFile.ReadIdFloat("fb_d_xlat", info.FbDXlat)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("jumpAirborne", info.JumpAirborne)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("jumpHold", info.JumpHold)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("jumpStartLandTime", info.JumpStartLandTime)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("jumpMaxDistance", info.JumpMaxDistance)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("jumpGravity", info.JumpGravity)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdFloat("jumpStartVel", info.JumpStartVel)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("r_fb_w_fb_frame", info.RFbWFbFrame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("r_ff_w_ff_frame", info.RFfWFfFrame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("s_fb_w_fb_frame", info.SFbWFbFrame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("s_ff_w_ff_frame", info.SFfWFfFrame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("walk_to_w_r_frame", info.WalkToWRFrame)) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdULong("run_to_r_w_frame", info.RunToRWFrame)) != 0)
    {
        return result;
    }

    if (iniFile.ReadIdULong("walk_to_w_s_frame", info.WalkToWSFrame) != 0)
    {
        info.WalkToWSFrame = 0xffffffff;
    }

    if (iniFile.ReadIdULong("s_w_to_walk_frame", info.SWToWalkFrame) != 0)
    {
        info.SWToWalkFrame = 0xffffffff;
    }

    // Optional flags: 0 when absent.
    auto readFlag = [&iniFile](const char* key) -> uint32_t
    {
        uint32_t value = 0;
        iniFile.ReadIdULong(key, value);
        return value;
    };

    info.StupidJamieReverseFlag = readFlag("stupidJamieReverseFlag");
    info.OtherJamieReverseFlag = readFlag("OtherJamieReverseFlag");
    info.ReallyStupidJamieReverseFlag = readFlag("reallyStupidJamieReverseFlag");
    info.SpecialDuaneFlag = readFlag("specialDuaneFlag");
    info.StandToGunPose = readFlag("standToGunPose");
    info.WalkToGunPose = readFlag("walkToGunPose");
    info.RunToGunPose = readFlag("runToGunPose");

    if (iniFile.SeekBlock("TransitionTable") == 0)
    {
        TransitionArray = static_cast<char*>(SpriteManager->MallocDataRam(0x32a));

        if (TransitionArray == nullptr)
        {
            return noGestureRam;
        }

        if ((result = iniFile.ReadIdCharArray("TransitionArray", TransitionArray, 0x32a)) != 0)
        {
            return result;
        }
    }
    else
    {
        TransitionArray = nullptr;
    }

    if ((result = iniFile.SeekBlock("Gestures")) != 0)
    {
        return result;
    }

    if ((result = iniFile.ReadIdUChar("NumGestures", TreeInfo->NumGestures)) != 0)
    {
        return result;
    }

    const uint32_t numGestures = TreeInfo->NumGestures;
    Gestures = static_cast<MCGestureData*>(SpriteManager->MallocDataRam(numGestures * sizeof(MCGestureData)));

    if (Gestures == nullptr)
    {
        return noGestureRam;
    }

    for (int32_t i = 0; i < static_cast<int32_t>(numGestures); i++)
    {
        char blockName[20];
        sprintf(blockName, "Gestures%d", i);

        if ((result = iniFile.SeekBlock(blockName)) != 0)
        {
            return result;
        }

        MCGestureData& data = Gestures[i];

        if ((result = iniFile.ReadIdUChar("State", data.State)) != 0)
        {
            return result;
        }

        uint32_t numFrames = 0;

        if ((result = iniFile.ReadIdULong("NumFrames", numFrames)) != 0)
        {
            return result;
        }

        data.NumFrames = numFrames;
        float frameRate = 0.0f;

        if ((result = iniFile.ReadIdFloat("FrameRate", frameRate)) != 0)
        {
            return result;
        }

        data.FrameRate = frameRate;
        uint32_t basePacketNumber = 0;

        if ((result = iniFile.ReadIdULong("BasePacketNumber", basePacketNumber)) != 0)
        {
            return result;
        }

        data.BasePacketNumber = basePacketNumber;

        if ((result = iniFile.ReadIdUCharArray("NumRotations", data.NumRotations, TreeInfo->NumParts)) != 0)
        {
            return result;
        }

        if ((result = iniFile.ReadIdUChar("Symmetrical", data.Symmetrical)) != 0)
        {
            return result;
        }

        if (iniFile.ReadIdUChar("ArmSymmetrical", data.ArmSymmetrical) != 0)
        {
            data.ArmSymmetrical = 1;
        }

        if ((result = iniFile.ReadIdUChar("ForwardResult", data.ForwardResult)) != 0)
        {
            return result;
        }

        if ((result = iniFile.ReadIdUChar("ReverseResult", data.ReverseResult)) != 0)
        {
            return result;
        }

        float velocity = 0.0f;

        if ((result = iniFile.ReadIdFloat("StartVelocity", velocity)) != 0)
        {
            return result;
        }

        data.StartVelocity = velocity;

        if ((result = iniFile.ReadIdFloat("EndVelocity", velocity)) != 0)
        {
            return result;
        }

        data.EndVelocity = velocity;
    }

    iniFile.Close();
    return 0;
}

auto MCSpriteTree::SetGesture(int32_t gesture, int32_t part, float rotation, float, int& reverse, float& frameRate)
    -> void
{
    reverse = 0;
    frameRate = 0.0f;
    const MCGestureData& data = Gestures[gesture];

    if (data.NumFrames == 0)
    {
        return;
    }

    // The original also works out the rotation index here, then drops it.
    FacingAndRate(data, PartSymmetrical(data, part), rotation, reverse, frameRate);
}

auto MCSpriteTree::GetGesture(int32_t gesture, int32_t part, float rotation, float, int& reverse, float& frameRate, int,
                              int zoomedOut) -> MCShape*
{
    reverse = 0;
    frameRate = 0.0f;
    const MCGestureData& data = Gestures[gesture];

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    const bool symmetrical = PartSymmetrical(data, part);
    FacingAndRate(data, symmetrical, rotation, reverse, frameRate);

    const uint32_t numRotations = data.NumRotations[part];
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
    const int32_t finder = static_cast<int32_t>(PacketFinderArray[gesture]);

    switch (part)
    {
        case 0:
        {
            fileNumber = LegFileNumber;
            filePart = 0;
            listStart = LEG_SHAPES;
            packet = static_cast<uint32_t>(index + finder * NUM_LEG_ROTATIONS);
            break;
        }
        case 1:
        {
            fileNumber = TorsoFileNumber;
            filePart = 1;
            listStart = TORSO_SHAPES;
            packet = static_cast<uint32_t>(finder * PartRotations(symmetrical) + index);
            break;
        }
        case 2:
        {
            fileNumber = LeftArmFileNumber;
            filePart = 3;
            listStart = LEFT_ARM_SHAPES;
            packet = static_cast<uint32_t>(finder * PartRotations(symmetrical) + index);
            break;
        }
        case 3:
        {
            fileNumber = RightArmFileNumber;
            filePart = 2;
            listStart = RIGHT_ARM_SHAPES;
            packet = static_cast<uint32_t>(finder * PartRotations(symmetrical) + index);
            break;
        }
        default:
            return nullptr;
    }

    if (fileNumber == 0xffffffff || SpriteManager == nullptr)
    {
        return nullptr;
    }

    MCShape* shape = ShapeList[listStart + packet];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    shape = SpriteManager->GetMechShapeData(fileNumber, packet, filePart, Turn, this, zoomedOut);
    ShapeList[listStart + packet] = shape;
    return shape;
}

auto MCSpriteTree::Destroy() -> void
{
    // The shapes stay in the sprite manager's cache, ownerless.
    for (int32_t i = 0; i < NumShapes; i++)
    {
        if (ShapeList[i] != nullptr)
        {
            ShapeList[i]->Owner = nullptr;
        }
    }

    if (ShapeList != nullptr)
    {
        SpriteManager->FreeDataRam(ShapeList);
        ShapeList = nullptr;
    }

    if (Gestures != nullptr)
    {
        SpriteManager->FreeDataRam(Gestures);
        Gestures = nullptr;
    }

    if (SpecialInfo != nullptr)
    {
        SpriteManager->FreeDataRam(SpecialInfo);
        SpecialInfo = nullptr;
    }

    if (TreeInfo != nullptr)
    {
        SpriteManager->FreeDataRam(TreeInfo);
        TreeInfo = nullptr;
    }

    if (TransitionArray != nullptr)
    {
        SpriteManager->FreeDataRam(TransitionArray);
        TransitionArray = nullptr;
    }
}
