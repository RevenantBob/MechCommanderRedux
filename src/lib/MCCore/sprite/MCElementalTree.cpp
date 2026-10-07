#include "stdafx.h"
#include "sprite/MCElementalTree.h"
#include "camera/MCCamera.h"
#include "main/main.h"
#include "sprite/MCElementalActor.h"
#include "sprite/MCShape.h"
#include "sprite/MCSpriteManager.h"

MCElementalTree::~MCElementalTree()
{
    ReleaseShapes(ShapeList);
}

auto MCElementalTree::Load(MCFile& apprFile, uint32_t fileSize) -> MCAppearanceLoad
{
    MCAppearanceFit fit(apprFile, fileSize);
    fit.SeekBlock("SpecialInfo");
    JumpMaxDistance = fit.Read<float>("jumpMaxDistance");
    fit.SeekBlock("Gestures");
    const auto numGestures = fit.Read<uint32_t>("NumGestures");

    // Faithful: no gestures fails the load (the original's zero-byte allocation).
    if (!fit.Failed() && numGestures == 0)
    {
        return std::unexpected("no gestures");
    }

    Gestures.resize(fit.Failed() ? 0 : numGestures);

    for (uint32_t i = 0; i < Gestures.size() && !fit.Failed(); i++)
    {
        fit.SeekBlock(std::format("Gestures{}", i));
        MCElementalGestureData& data = Gestures[i];
        data.State = fit.Read<uint8_t>("State");
        data.NumFrames = fit.Read<uint32_t>("NumFrames");
        data.FrameRate = fit.Read<float>("FrameRate");
        data.BasePacketNumber = fit.Read<uint32_t>("BasePacketNumber");
        data.NumRotations = fit.Read<uint8_t>("NumRotations");
        data.Velocity = fit.Read<float>("Velocity");
    }

    if (fit.Failed())
    {
        return fit.Result();
    }

    return MakeShapeList(ShapeList);
}

auto MCElementalTree::RemoveShape(MCShape* shape) -> void
{
    ForgetShape(ShapeList, shape);

    for (MCAppearance* user : Users())
    {
        auto* actor = static_cast<MCElementalActor*>(user);

        if (actor->CurrentShape == shape)
        {
            actor->CurrentShape = nullptr;
        }
    }
}

auto MCElementalTree::GestureFrameRate(int32_t gesture) const -> float
{
    if (gesture < 0 || gesture >= static_cast<int32_t>(Gestures.size()) || Gestures[gesture].NumFrames == 0)
    {
        return 0.0f;
    }

    const float frameRate = Gestures[gesture].FrameRate;
    return frameRate < 0.0 ? -frameRate : frameRate;
}

auto MCElementalTree::GetGesture(int32_t gesture, float rotation) -> MCShape*
{
    if (gesture < 0 || gesture >= static_cast<int32_t>(Gestures.size()) || Gestures[gesture].NumFrames == 0)
    {
        return nullptr;
    }

    const MCElementalGestureData& data = Gestures[gesture];

    // Into 0..360.
    if (rotation > 180.0)
    {
        rotation = (rotation - 180.0f) - 180.0f;
    }
    else if (rotation < -180.0)
    {
        rotation = rotation + 180.0f + 180.0f;
    }

    if (rotation < 0.0)
    {
        rotation = static_cast<float>(rotation + 360.0);
    }

    const int32_t numRotations = data.NumRotations;
    int32_t rotationIndex = static_cast<int16_t>(static_cast<int32_t>(
        std::floor(static_cast<double>(rotation * static_cast<float>(numRotations + 1)) * (1.0 / 360.0))));

    if (numRotations <= rotationIndex)
    {
        rotationIndex = numRotations - 1;
    }

    // Full size and zoomed out packets alternate.
    uint32_t packet = data.BasePacketNumber + static_cast<uint32_t>(rotationIndex * 2);

    if (Eye != nullptr && Eye->CameraScale == 1)
    {
        packet++;
    }

    if (packet >= ShapeList.size())
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
