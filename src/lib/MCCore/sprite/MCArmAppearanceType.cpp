#include "stdafx.h"
#include "sprite/MCArmAppearanceType.h"
#include "main/MCMissionGlobals.h"
#include "sprite/MCArmAppearance.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"

auto MCArmAppearanceType::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("State");
    ActorData.NumFrames = fit.Read<uint32_t>("NumFrames");
    ActorData.FrameRate = fit.Read<float>("FrameRate");
    ActorData.BasePacketNumber = fit.Read<uint32_t>("BasePacketNumber");
    ActorData.NumRotations = fit.Read<uint8_t>("NumRotations");
    ActorData.Symmetrical = fit.Read<uint32_t>("Symmetrical") != 0;
    CheckForHeader = fit.Read<uint8_t>("CheckForHeader", 1);

    if (fit.Failed())
    {
        return fit.Result();
    }

    return MakeShapeList(ShapeList);
}

auto MCArmAppearanceType::GetShape(int32_t rotation, float& frameRate, bool& reverse) -> std::shared_ptr<MCShape>
{
    const MCArmActorData& data = ActorData;

    if (data.NumFrames == 0)
    {
        return nullptr;
    }

    if (rotation > 180)
    {
        rotation -= 360;
    }
    else if (rotation < -180)
    {
        rotation += 360;
    }

    if (rotation < 0 && data.Symmetrical)
    {
        rotation = -rotation;
        // Original behaviour (OB-053): reverse is only ever set, never cleared.
        reverse = true;
    }
    else if (rotation < 0)
    {
        rotation += 360;
    }

    frameRate = data.FrameRate;

    int32_t rotationIndex = static_cast<int16_t>(
        static_cast<int32_t>(std::floor(static_cast<double>((data.NumRotations + 1) * rotation) * (1.0 / 360.0))));

    if (rotationIndex > 0x1f && rotation > 180 && !data.Symmetrical)
    {
        rotationIndex = 0x1f;
    }

    const uint32_t packet = data.BasePacketNumber + static_cast<uint32_t>(rotationIndex);

    if (ShapeList.size() <= packet)
    {
        return nullptr;
    }

    return PacketShape(ShapeList[packet], packet);
}
