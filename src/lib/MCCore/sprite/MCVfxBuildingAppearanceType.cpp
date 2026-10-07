#include "stdafx.h"
#include "sprite/MCVfxBuildingAppearanceType.h"
#include "main/main.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"
#include "sprite/MCVfxBuildingAppearance.h"

namespace
{
    /// <summary>The first building frame's packet (packets 0..9 are the tiles under buildings).</summary>
    constexpr uint32_t FirstFramePacket = 0xc;
    /// <summary>The number of tile packets.</summary>
    constexpr uint32_t TileShapeCount = 10;
}

MCVfxBuildingAppearanceType::~MCVfxBuildingAppearanceType()
{
    ReleaseShapes(ShapeList);
}

auto MCVfxBuildingAppearanceType::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("Main Info");
    NumFrames = fit.Read<uint32_t>("NumFrames");

    if (fit.HasBlock("AnimationInfo"))
    {
        const auto numAnimStates = fit.Read<uint32_t>("NumAnimStates");

        // Faithful: no states fails the load (the original's zero-byte allocation).
        if (!fit.Failed() && numAnimStates == 0)
        {
            return std::unexpected("no animation states");
        }

        AnimStates.resize(fit.Failed() ? 0 : numAnimStates);

        for (uint32_t i = 0; i < AnimStates.size() && !fit.Failed(); i++)
        {
            fit.SeekBlock(std::format("AnimState{}", i));
            AnimStates[i].NumFrames = fit.Read<uint32_t>("numFrames");
            AnimStates[i].FrameRate = fit.Read<float>("frameRate");
        }
    }

    if (fit.Failed())
    {
        return fit.Result();
    }

    return MakeShapeList(ShapeList);
}

auto MCVfxBuildingAppearanceType::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        auto* appearance = static_cast<MCVfxBuildingAppearance*>(user);

        if (appearance->CurrentShape == shape)
        {
            appearance->CurrentShape = nullptr;
        }

        if (appearance->TileShape == shape)
        {
            appearance->TileShape = nullptr;
        }
    }
}

auto MCVfxBuildingAppearanceType::PacketShape(uint32_t packet) -> MCShape*
{
    MCShape*& shape = ShapeList[packet];

    if (shape != nullptr)
    {
        shape->LastTurnUsed = Turn;
        return shape;
    }

    shape = SpriteManager()->GetShapeData(ShapeFileNum(), packet, Turn, this);
    return shape;
}

auto MCVfxBuildingAppearanceType::GetShape(uint32_t frame) -> MCShape*
{
    const uint32_t packet = frame + FirstFramePacket;
    return packet < ShapeList.size() ? PacketShape(packet) : nullptr;
}

auto MCVfxBuildingAppearanceType::GetTileShape(uint32_t tileNum) -> MCShape*
{
    return tileNum < TileShapeCount && tileNum < ShapeList.size() ? PacketShape(tileNum) : nullptr;
}
