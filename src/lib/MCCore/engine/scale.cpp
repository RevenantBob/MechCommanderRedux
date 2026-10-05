#include "stdafx.h"
#include "engine/scale.h"
#include "camera/camera.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "vfx/vfxfuncs.h"

auto scaleDraw(uint8_t* shape, uint32_t frameNum, int32_t x, int32_t y, int reverse, uint8_t* fadeTable, int scaleUp)
    -> int32_t
{
    // Zoomed out (cameraScale 1) draws at half size; anything else at full size.
    float scale = 0.5f;

    if (eye->cameraScale != 1)
    {
        scale = 1.0f;
    }

    scaleUp = scale == 1.0f ? 1 : 0;

    if (std::memcmp(shape, "1.10", 4) != 0)
    {
        return -1;
    }

    const int32_t count = VFX_shape_count(shape);

    if (count <= static_cast<int32_t>(frameNum))
    {
        frameNum = static_cast<uint32_t>(count - 1);
    }

    const int32_t bounds = VFX_shape_bounds(shape, static_cast<int32_t>(frameNum));
    const int32_t width = bounds >> 0x10;
    const uint32_t height = static_cast<uint32_t>(bounds) & 0xffff;

    if (width == 0 || height == 0 || std::abs(width - static_cast<int32_t>(height)) > 0x100 || width >= 0x191 ||
        height >= 0x191)
    {
        return -1;
    }

    if (static_cast<int32_t>(height * static_cast<uint32_t>(width)) > 0x1fa3f)
    {
        Fatal(-1, " Sprite too damned big ", nullptr);
    }

    if (fadeTable != nullptr)
    {
        AG_shape_lookaside(fadeTable);

        if (scaleUp != 0 && reverse == 0)
        {
            AG_shape_translate_draw(globalPane, shape, static_cast<int32_t>(frameNum), x, y);
            return bounds;
        }

        AG_shape_translate_transform(globalPane, shape, static_cast<int32_t>(frameNum), x, y, tempBuffer.data(),
                                     reverse, scaleUp);
        return bounds;
    }

    if (scaleUp != 0 && reverse == 0)
    {
        AG_shape_draw(globalPane, shape, static_cast<int32_t>(frameNum), x, y);
        return bounds;
    }

    AG_shape_transform(globalPane, shape, static_cast<int32_t>(frameNum), x, y, tempBuffer.data(), reverse, scaleUp);
    return bounds;
}
