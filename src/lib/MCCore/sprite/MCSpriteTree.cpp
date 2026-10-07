#include "stdafx.h"
#include "sprite/MCSpriteTree.h"
#include "main/main.h"
#include "sprite/MCMechActor.h"
#include "sprite/MCShape.h"

namespace
{
    /// <summary>Leg rotations per gesture.</summary>
    constexpr int32_t LegRotationCount = 9;

    /// <summary>The default gesture transition table (a tree's FIT "TransitionArray" replaces it).</summary>
    constexpr MCTransitionTable DefaultTransitions = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1,  2,  -1, -1, -1, -1, -1, -1, -1, -1, 1,  2,  3,  4,  -1, -1, -1, -1,
        -1, -1, 1,  2,  3,  4,  6,  7,  -1, -1, -1, -1, 1,  2,  10, 9,  -1, -1, -1, -1, -1, -1, 1,  2,  3,  11, -1, -1,
        -1, -1, -1, -1, 1,  2,  20, 2,  -1, -1, -1, -1, -1, -1, 19, 15, 23, -1, -1, -1, -1, -1, -1, -1, 18, 14, 24, -1,
        -1, -1, -1, -1, -1, -1, 1,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,
        -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,  6,  7,  -1, -1, -1, -1, -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1, -1, -1,
        3,  11, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 19, 15, 23, -1, -1, -1, -1, -1,
        -1, -1, 18, 14, 24, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, 5,  10, 9,  -1,
        -1, -1, -1, -1, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 15, 23,
        -1, -1, -1, -1, -1, -1, -1, -1, 14, 24, -1, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1,
        2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, 4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1,
        -1, -1, -1, -1, 17, 15, 23, -1, -1, -1, -1, -1, -1, -1, 16, 14, 24, -1, -1, -1, -1, -1, -1, -1, 2,  1,  0,  -1,
        -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, 2,  3,  4,  -1, -1, -1, -1, -1, -1, -1, 2,  3,
        4,  6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3,  11, -1, -1, -1, -1, -1, -1, -1, -1,
        20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  15, 23, -1, -1, -1, -1, -1, -1, -1, 3,  14, 24, -1, -1, -1, -1, -1,
        -1, -1, 2,  1,  0,  -1, -1, -1, -1, -1, -1, -1, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, 4,  -1, -1, -1, -1, -1,
        -1, -1, -1, -1, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, 20, 2,  -1, -1, -1, -1, -1, -1, -1, -1, 15, 23, -1, -1, -1, -1, -1, -1, -1, -1, 14, 24,
        -1, -1, -1, -1, -1, -1, -1, -1, 1,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        3,  4,  -1, -1, -1, -1, -1, -1, -1, -1, 3,  4,  6,  7,  -1, -1, -1, -1, -1, -1, 10, 9,  -1, -1, -1, -1, -1, -1,
        -1, -1, 3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 22, 1,  0,  -1, -1, -1, -1, -1, -1, -1, 22, 2,  -1, -1,
        -1, -1, -1, -1, -1, -1, 22, 3,  4,  -1, -1, -1, -1, -1, -1, -1, 22, 3,  4,  6,  7,  -1, -1, -1, -1, -1, 22, 10,
        9,  -1, -1, -1, -1, -1, -1, -1, 22, 3,  11, -1, -1, -1, -1, -1, -1, -1, 22, 20, 2,  -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 21, 22, 1,  0,  -1, -1, -1, -1,
        -1, -1, 21, 22, 2,  -1, -1, -1, -1, -1, -1, -1, 21, 22, 3,  4,  -1, -1, -1, -1, -1, -1, 21, 22, 3,  4,  6,  7,
        -1, -1, -1, -1, 21, 22, 10, 9,  -1, -1, -1, -1, -1, -1, 21, 22, 3,  11, -1, -1, -1, -1, -1, -1, 21, 22, 20, 2,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    /// <summary>Upper-body rotations per gesture: 17 when the part is mirrored, else 32.</summary>
    auto PartRotations(bool symmetrical) -> int32_t
    {
        return symmetrical ? 0x11 : 0x20;
    }

    /// <summary>Whether <paramref name="part"/> (the tree's numbering) is mirrored in the gesture.</summary>
    auto PartSymmetrical(const MCGestureData& data, int32_t part) -> bool
    {
        return (data.Symmetrical && part == 1) || part == 0 || (data.ArmSymmetrical && part > 1);
    }

    /// <summary>
    /// Wraps <paramref name="rotation"/> into -180..180, then mirrors it (returning true) when the part is
    /// <paramref name="symmetrical"/>, or wraps negatives to 0..360 when not.
    /// </summary>
    auto Facing(bool symmetrical, float& rotation) -> bool
    {
        if (rotation > 180.0)
        {
            rotation = (rotation - 180.0f) - 180.0f;
        }
        else if (rotation < -180.0)
        {
            rotation = rotation + 180.0f + 180.0f;
        }

        if (rotation < 0.0 && symmetrical)
        {
            rotation = -rotation;
            return true;
        }

        if (rotation < 0.0)
        {
            rotation = static_cast<float>(rotation + 360.0);
        }

        return false;
    }

    /// <summary>A gesture's frame rate, made positive.</summary>
    auto PositiveRate(const MCGestureData& data) -> float
    {
        return data.FrameRate < 0.0 ? -data.FrameRate : data.FrameRate;
    }
}

MCSpriteTree::MCSpriteTree() : ShapeList(TreeShapeCount, nullptr)
{
}

MCSpriteTree::~MCSpriteTree()
{
    ReleaseShapes(ShapeList);
}

auto MCSpriteTree::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        std::ranges::replace(static_cast<MCMechActor*>(user)->PartShape, shape, nullptr);
    }
}

auto MCSpriteTree::PreloadGestures() -> void
{
    if (GesturesPreloaded)
    {
        return;
    }

    // The shapes load as the large (90-pixel) art, which MCMechActor::Render asks for: the camera scale is pinned
    // to 100. The original loaded the small art here (it started zoomed out), and the cached shape then stood in for
    // the large one, so a preloaded gesture drew at half size.
    auto preloadPart = [this](MCMechPart part, uint32_t gesture, int32_t numRotations)
    {
        const uint32_t fileNumber = FileNumbers[part];

        for (int32_t i = 0; i < numRotations; i++)
        {
            if (fileNumber == 0xffffffff || SpriteManager() == nullptr)
            {
                continue;
            }

            const auto packet =
                static_cast<uint32_t>(static_cast<int32_t>(PacketFinderArray[gesture]) * numRotations + i);
            ShapeList[PartShapeStart[part] + packet] =
                SpriteManager()->GetMechShapeData(fileNumber, packet, part, 1, this, true);
        }
    };

    // Faithful: the torso's "symmetrical" is never cleared once a gesture sets it.
    bool torsoSymmetrical = false;

    // Stand, walk and run.
    for (const uint32_t gesture : {2u, 4u, 7u})
    {
        preloadPart(MCMechPart::Legs, gesture, LegRotationCount);
        const MCGestureData& data = Gestures[gesture];
        torsoSymmetrical = torsoSymmetrical || data.Symmetrical;
        preloadPart(MCMechPart::Torso, gesture, PartRotations(torsoSymmetrical));
        preloadPart(MCMechPart::RightArm, gesture, PartRotations(data.ArmSymmetrical));
        preloadPart(MCMechPart::LeftArm, gesture, PartRotations(data.ArmSymmetrical));
    }

    GesturesPreloaded = true;
}

auto MCSpriteTree::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("Main Info");
    fit.Read<std::string>("Name");
    FileNumbers[MCMechPart::Legs] = fit.Read<uint32_t>("legFileNumber");
    FileNumbers[MCMechPart::Torso] = fit.Read<uint32_t>("torsoFileNumber");
    FileNumbers[MCMechPart::RightArm] = fit.Read<uint32_t>("rightArmFileNumber");
    FileNumbers[MCMechPart::LeftArm] = fit.Read<uint32_t>("leftArmFileNumber");
    fit.SeekBlock("Parts");
    NumParts = fit.Read<uint8_t>("NumParts");

    fit.SeekBlock("SpecialInfo");
    MCMechSpecialInfo& info = SpecialInfo;
    info.FbDXlat = fit.Read<float>("fb_d_xlat");
    info.JumpAirborne = fit.Read<uint32_t>("jumpAirborne");
    info.JumpHold = fit.Read<uint32_t>("jumpHold");
    info.JumpStartLandTime = fit.Read<float>("jumpStartLandTime");
    info.JumpMaxDistance = fit.Read<float>("jumpMaxDistance");
    info.JumpGravity = fit.Read<float>("jumpGravity");
    info.JumpStartVel = fit.Read<float>("jumpStartVel");
    info.RFbWFbFrame = fit.Read<uint32_t>("r_fb_w_fb_frame");
    info.RFfWFfFrame = fit.Read<uint32_t>("r_ff_w_ff_frame");
    info.SFbWFbFrame = fit.Read<uint32_t>("s_fb_w_fb_frame");
    info.SFfWFfFrame = fit.Read<uint32_t>("s_ff_w_ff_frame");
    info.WalkToWRFrame = fit.Read<uint32_t>("walk_to_w_r_frame");
    info.RunToRWFrame = fit.Read<uint32_t>("run_to_r_w_frame");
    info.WalkToWSFrame = fit.Read<uint32_t>("walk_to_w_s_frame", NoFrame);
    info.SWToWalkFrame = fit.Read<uint32_t>("s_w_to_walk_frame", NoFrame);
    info.StupidJamieReverseFlag = fit.Read<uint32_t>("stupidJamieReverseFlag", 0);
    info.OtherJamieReverseFlag = fit.Read<uint32_t>("OtherJamieReverseFlag", 0);
    info.ReallyStupidJamieReverseFlag = fit.Read<uint32_t>("reallyStupidJamieReverseFlag", 0);
    info.SpecialDuaneFlag = fit.Read<uint32_t>("specialDuaneFlag", 0);
    info.StandToGunPose = fit.Read<uint32_t>("standToGunPose", 0);
    info.WalkToGunPose = fit.Read<uint32_t>("walkToGunPose", 0);
    info.RunToGunPose = fit.Read<uint32_t>("runToGunPose", 0);

    if (fit.HasBlock("TransitionTable"))
    {
        fit.ReadArray<char>("TransitionArray", std::span(TransitionArray.emplace()));
    }

    fit.SeekBlock("Gestures");
    NumGestures = fit.Read<uint8_t>("NumGestures");

    // Faithful: no gestures fails the load (the original's zero-byte allocation).
    if (!fit.Failed() && NumGestures == 0)
    {
        return std::unexpected("no gestures");
    }

    Gestures.resize(fit.Failed() ? 0 : NumGestures);

    for (size_t i = 0; i < Gestures.size() && !fit.Failed(); i++)
    {
        fit.SeekBlock(std::format("Gestures{}", i));
        MCGestureData& data = Gestures[i];
        data.State = fit.Read<uint8_t>("State");
        data.NumFrames = fit.Read<uint32_t>("NumFrames");
        data.FrameRate = fit.Read<float>("FrameRate");
        data.BasePacketNumber = fit.Read<uint32_t>("BasePacketNumber");
        fit.ReadArray<uint8_t>("NumRotations",
                               std::span(data.NumRotations).first(std::min<size_t>(NumParts, MechPartCount)));
        data.Symmetrical = fit.Read<uint8_t>("Symmetrical") != 0;
        data.ArmSymmetrical = fit.Read<uint8_t>("ArmSymmetrical", 1) != 0;
        data.ForwardResult = fit.Read<uint8_t>("ForwardResult");
        data.ReverseResult = fit.Read<uint8_t>("ReverseResult");
        data.StartVelocity = fit.Read<float>("StartVelocity");
        data.EndVelocity = fit.Read<float>("EndVelocity");
    }

    return fit.Result();
}

auto MCSpriteTree::SetGesture(int32_t gesture, int32_t part, float rotation, int32_t& reverse, float& frameRate) const
    -> void
{
    reverse = 0;
    frameRate = 0.0f;
    const MCGestureData& data = Gestures[gesture];

    if (data.NumFrames == 0)
    {
        return;
    }

    reverse = Facing(PartSymmetrical(data, part), rotation) ? 1 : 0;
    frameRate = PositiveRate(data);
}

auto MCSpriteTree::GesturePacket(int32_t gesture, int32_t part, float rotation) const -> std::optional<MCGesturePacket>
{
    const MCGestureData& data = Gestures[gesture];

    if (data.NumFrames == 0 || part < 0 || part >= MechPartCount)
    {
        return std::nullopt;
    }

    const bool symmetrical = PartSymmetrical(data, part);
    bool reverse = Facing(symmetrical, rotation);
    const uint32_t numRotations = data.NumRotations[part];
    int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<int32_t>(numRotations + 1)) * rotation * (1.0 / 360.0))));

    if (rotationIndex >= 0x20 && rotation > 180.0 && !symmetrical)
    {
        rotationIndex = 0x1f;

        if (rotationIndex == static_cast<int32_t>(numRotations >> 1))
        {
            reverse = false;
        }
    }
    else if (rotationIndex == 0 || rotationIndex == static_cast<int32_t>(numRotations >> 1))
    {
        // Straight ahead or straight back: no mirroring.
        reverse = false;
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

    // The tree's parts 2 and 3 are the left and the right arm's sprites.
    constexpr std::array<MCMechPart, MechPartCount> filePart = {MCMechPart::Legs, MCMechPart::Torso,
                                                                MCMechPart::LeftArm, MCMechPart::RightArm};
    const auto finder = static_cast<int32_t>(PacketFinderArray[gesture]);
    const int32_t rotationsStored = part == 0 ? LegRotationCount : PartRotations(symmetrical);
    const auto packet = static_cast<uint32_t>(finder * rotationsStored + index);
    const MCMechPart file = filePart[part];
    return MCGesturePacket{file, packet, PartShapeStart[file] + packet, reverse};
}

auto MCSpriteTree::GetGesture(int32_t gesture, int32_t part, float rotation, int32_t& reverse, float& frameRate,
                              bool zoomedOut) -> MCShape*
{
    reverse = 0;
    frameRate = 0.0f;
    const std::optional<MCGesturePacket> where = GesturePacket(gesture, part, rotation);

    if (!where.has_value())
    {
        return nullptr;
    }

    reverse = where->Reverse ? 1 : 0;
    frameRate = PositiveRate(Gestures[gesture]);
    const uint32_t fileNumber = FileNumbers[where->FilePart];

    if (fileNumber == 0xffffffff || SpriteManager() == nullptr)
    {
        return nullptr;
    }

    MCShape*& shape = ShapeList[where->ListIndex];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    shape = SpriteManager()->GetMechShapeData(fileNumber, where->Packet, where->FilePart, Turn, this, zoomedOut);
    return shape;
}

auto MCSpriteTree::Transition(int32_t index) const -> int32_t
{
    return TransitionArray.has_value() ? (*TransitionArray)[index] : DefaultTransitions[index];
}
