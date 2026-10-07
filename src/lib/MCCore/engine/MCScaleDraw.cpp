#include "stdafx.h"
#include "engine/MCScaleDraw.h"
#include "camera/camera.h"
#include "vfx/MCVfxFunctions.h"

auto ScaleDraw(uint8_t* shape, uint32_t frameNum, int32_t x, int32_t y, int32_t reverse, uint8_t* fadeTable) -> int32_t
{
    // Zoomed out (camera scale 1) draws at half size; anything else at full size.
    const bool fullSize = Eye->CameraScale != 1;

    if (!VfxIsShapeTable(shape))
    {
        return -1;
    }

    const int32_t count = VfxShapeCount(shape);

    if (count <= static_cast<int32_t>(frameNum))
    {
        frameNum = static_cast<uint32_t>(count - 1);
    }

    const int32_t bounds = VfxShapeBounds(shape, static_cast<int32_t>(frameNum));
    const int32_t width = bounds >> 0x10;
    const int32_t height = bounds & 0xffff;

    if (width == 0 || height == 0 || std::abs(width - height) > 0x100 || width >= 0x191 || height >= 0x191)
    {
        return -1;
    }

    const auto frame = static_cast<int32_t>(frameNum);

    if (fadeTable != nullptr)
    {
        AGShapeLookaside(fadeTable);

        if (fullSize && reverse == 0)
        {
            AGShapeTranslateDraw(GlobalPane, shape, frame, x, y);
            return bounds;
        }

        AGShapeTranslateTransform(GlobalPane, shape, frame, x, y, reverse, fullSize ? 1 : 0);
        return bounds;
    }

    if (fullSize && reverse == 0)
    {
        AGShapeDraw(GlobalPane, shape, frame, x, y);
        return bounds;
    }

    AGShapeTransform(GlobalPane, shape, frame, x, y, reverse, fullSize ? 1 : 0);
    return bounds;
}
