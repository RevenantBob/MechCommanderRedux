#include "stdafx.h"
#include "sprite/MCVfxAppearanceType.h"
#include "camera/MCCamera.h"
#include "main/main.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"
#include "sprite/MCVfxAppearance.h"

namespace
{
    /// <summary>Reads the entries states and sub-states share.</summary>
    auto ReadStateData(MCAppearanceFit& fit, MCActorData& data) -> void
    {
        data.NumFrames = fit.Read<uint32_t>("NumFrames");
        data.FrameRate = fit.Read<float>("FrameRate");
        data.BasePacketNumber = fit.Read<uint32_t>("BasePacketNumber");
        data.NumRotations = fit.Read<uint8_t>("NumRotations");
        data.Symmetrical = fit.Read<uint8_t>("Symmetrical") != 0;
    }
}

MCVfxAppearanceType::~MCVfxAppearanceType()
{
    ReleaseShapes(ShapeList);
}

auto MCVfxAppearanceType::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("Main Info");
    Delta = fit.Read<int32_t>("delta", 0) != 0;
    fit.SeekBlock("States");
    NumStates = fit.Read<uint8_t>("NumStates");
    Scaled = fit.Read<uint8_t>("Scaled", 0) != 0;
    States.assign(std::max<size_t>(ActorStateCount, NumStates), MCActorData{});

    for (int32_t i = 0; i < NumStates && !fit.Failed(); i++)
    {
        MCActorData& data = States[i];
        fit.SeekBlock(std::format("State{}", i));
        data.State = static_cast<MCActorState>(fit.Read<uint8_t>("State"));
        const uint8_t numSubStates = fit.Read<uint8_t>("SubStates", 0);
        ReadStateData(fit, data);

        // Only the first state with sub-states gets them (in practice state 0).
        if (numSubStates == 0 || !SubStates.empty())
        {
            continue;
        }

        SubStates.assign(numSubStates, MCActorData{});

        for (int32_t j = 0; j < numSubStates; j++)
        {
            MCActorData& subData = SubStates[j];
            fit.SeekBlock(std::format("Sub{}State{}", j, i));
            subData.State = static_cast<MCActorState>(fit.Read<uint8_t>("State"));
            subData.SubState = fit.Read<uint8_t>("Sub");
            ReadStateData(fit, subData);
            subData.Loop = fit.Read<uint8_t>("Loop") != 0;
        }
    }

    if (fit.Failed())
    {
        return fit.Result();
    }

    return MakeShapeList(ShapeList);
}

auto MCVfxAppearanceType::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        auto* appearance = static_cast<MCVfxAppearance*>(user);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }
    }
}

auto MCVfxAppearanceType::GetShape(MCActorState state, uint8_t subState, int32_t rotation, float& frameRate,
                                   bool& reverse) -> MCShape*
{
    const MCActorData& data = StateData(state);
    reverse = false;

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    // Symmetrical states mirror the negative facings; the others wrap them.
    if (rotation < 0 && data.Symmetrical)
    {
        reverse = true;
        rotation = -rotation;
    }
    else if (rotation < 0)
    {
        rotation += 360;
    }

    int32_t rotationIndex = static_cast<int32_t>(
        std::floor(static_cast<double>(static_cast<int32_t>(data.NumRotations) * rotation) * (1.0 / 360.0)));
    frameRate = data.FrameRate;
    uint32_t basePacket = data.BasePacketNumber;

    if (subState != NoSubState && !SubStates.empty())
    {
        basePacket = SubStates[subState].BasePacketNumber;
        frameRate = SubStates[subState].FrameRate;
    }

    const bool zoomedOut = Eye != nullptr && Eye->CameraScale == 1;

    // Scaled types alternate full size and zoomed out packets.
    if (Scaled)
    {
        rotationIndex <<= 1;
    }

    uint32_t packet = basePacket + static_cast<uint32_t>(rotationIndex);

    if (zoomedOut && Scaled)
    {
        packet++;
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
