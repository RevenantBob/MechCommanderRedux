#include "stdafx.h"
#include "sprite/MCPUAppearanceType.h"
#include "camera/MCCamera.h"
#include "main/main.h"
#include "sprite/MCPUAppearance.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"

MCPUAppearanceType::~MCPUAppearanceType()
{
    ReleaseShapes(ShapeList);
}

auto MCPUAppearanceType::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("Main Info");
    fit.SeekBlock("States");
    const auto numStates = fit.Read<uint8_t>("NumStates");

    if (fit.Failed())
    {
        return fit.Result();
    }

    if (numStates != PUActorStateCount)
    {
        return std::unexpected(std::format("{} states", numStates));
    }

    Scaled = fit.Read<uint8_t>("Scaled", 0) != 0;

    for (int32_t i = 0; i < PUActorStateCount; i++)
    {
        fit.SeekBlock(std::format("State{}", i));
        MCPUActorData& data = States[i];
        data.State = static_cast<MCPUActorState>(fit.Read<uint8_t>("State"));
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

auto MCPUAppearanceType::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        auto* appearance = static_cast<MCPUAppearance*>(user);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }
    }
}

auto MCPUAppearanceType::GetShape(MCPUActorState state, int32_t rotation, int32_t part, float& frameRate) -> MCShape*
{
    const MCPUActorData& data = StateData(state);

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    if (rotation < 0)
    {
        rotation += 360;
    }

    const uint32_t numRotations = data.NumRotations;
    frameRate = data.FrameRate;
    const int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(std::floor(
        static_cast<double>(static_cast<int32_t>(numRotations * static_cast<uint32_t>(rotation))) * (1.0 / 360.0))));
    uint32_t basePacket = data.BasePacketNumber;

    if (part > 0)
    {
        basePacket += numRotations * static_cast<uint32_t>(part);
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

    // Scaled types keep their zoomed out rotations after the full size ones.
    if (Eye != nullptr && Eye->CameraScale == 1 && Scaled)
    {
        packet += numRotations;
    }

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
