#include "stdafx.h"
#include "sprite/MCGVAppearanceType.h"
#include "main/main.h"
#include "sprite/MCGVAppearance.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"

MCGVAppearanceType::~MCGVAppearanceType()
{
    ReleaseShapes(ShapeList);
}

auto MCGVAppearanceType::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("Main Info");
    NumParts = fit.Read<uint32_t>("NumParts");
    TurretOffset = fit.Read<float>("TurretOffset");
    fit.SeekBlock("States");
    const auto numStates = fit.Read<uint8_t>("NumStates");

    if (fit.Failed())
    {
        return fit.Result();
    }

    // Normal, damaged and destroyed, and optionally an extra state.
    if (numStates != 3 && numStates != 4)
    {
        return std::unexpected(std::format("{} states", numStates));
    }

    HasExtraState = numStates == 4;

    for (int32_t i = 0; i < numStates; i++)
    {
        MCGVActorData& data = States[i];
        fit.SeekBlock(std::format("State{}", i));
        data.State = static_cast<MCGVActorState>(fit.Read<uint8_t>("State"));
        data.NumFrames = fit.Read<uint32_t>("NumFrames");
        data.FrameRate = fit.Read<float>("FrameRate");
        data.BasePacketNumber = fit.Read<uint32_t>("BasePacketNumber");
        data.NumRotations = fit.Read<uint8_t>("NumRotations");
    }

    if (fit.Failed())
    {
        return fit.Result();
    }

    return MakeShapeList(ShapeList);
}

auto MCGVAppearanceType::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        auto* appearance = static_cast<MCGVAppearance*>(user);
        std::ranges::replace(appearance->CurrentShape, shape, nullptr);
    }
}

auto MCGVAppearanceType::GetShape(MCGVActorState state, int32_t rotation, int32_t part, float& frameRate) -> MCShape*
{
    if (static_cast<int32_t>(NumParts) <= part)
    {
        return nullptr;
    }

    const MCGVActorData& data = StateData(state);

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    // Into 0..360 (the original's unsigned divisions; the operands are positive).
    if (rotation > 180)
    {
        rotation -= static_cast<int32_t>((static_cast<uint32_t>(rotation) + 179u) / 360u) * 360;
    }

    if (rotation < -180)
    {
        rotation += static_cast<int32_t>((179u - static_cast<uint32_t>(rotation)) / 360u) * 360;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.NumRotations;
    frameRate = data.FrameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    // The turret's rotations follow the body's.
    uint32_t basePacket = data.BasePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    const uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

    if (ShapeList.size() <= packet)
    {
        return nullptr;
    }

    MCShape*& shape = ShapeList[packet];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    shape = SpriteManager()->GetShapeData(ShapeFileNum(), packet, Turn, this);
    return shape;
}
