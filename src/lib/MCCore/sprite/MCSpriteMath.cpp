#include "stdafx.h"
#include "sprite/MCSpriteMath.h"
#include "camera/MCCamera.h"
#include "object/MCBigGameObject.h"
#include "vfx/MCVfxFunctions.h"

auto MCActorFacing(MCGameObject* obj) -> double
{
    const MCFrameOfRef frame = obj->GetFrame();
    float cosFacing = UnitX.X * frame.I.X + UnitX.Y * frame.I.Y + UnitX.Z * frame.I.Z;

    if (cosFacing < -1.0)
    {
        cosFacing = -1.0f;
    }

    if (cosFacing > 1.0)
    {
        cosFacing = 1.0f;
    }

    double facing = AcosMatherr(static_cast<double>(cosFacing)) * 0x1.ca5dc1a6402aap+5;

    if (frame.I.Y < 0.0)
    {
        facing = -facing;
    }

    return facing;
}

auto MCZoomScale(const MCCamera* cam) -> float
{
    return MCCamera::CameraScale == 1 ? 0.5f : 1.0f;
}

auto MCShapeFrameBounds(uint8_t* shapeTable, int32_t frame) -> MCFrameBounds
{
    const int32_t minXY = VfxShapeMinxy(shapeTable, frame);
    const int32_t size = VfxShapeResolution(shapeTable, frame);
    return {static_cast<float>(minXY >> 16), static_cast<float>(static_cast<int16_t>(minXY)),
            static_cast<float>(size >> 16), static_cast<float>(static_cast<int16_t>(size))};
}

auto MCClampShapeFrame(uint8_t* shapeTable, int32_t frame) -> int32_t
{
    const int32_t numFrames = VfxShapeCount(shapeTable);

    if (frame < 0)
    {
        frame = 0;
    }

    if (numFrames <= frame)
    {
        frame = numFrames - 1;
    }

    return frame;
}

auto MCGrowBounds(const MCFrameBounds& bounds, float& minX, float& minY, float& width, float& height) -> void
{
    minX = std::min(minX, bounds.MinX);
    minY = std::min(minY, bounds.MinY);
    width = std::max(width, bounds.Width);
    height = std::max(height, bounds.Height);
}
