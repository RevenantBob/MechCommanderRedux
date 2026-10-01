#include "stdafx.h"
#include "sprite/sprite.h"
#include "vfx/vfxfuncs.h"

auto SpriteGesture::destroy() -> void
{
    gestureData = nullptr;
    shapeTable = nullptr;
}

auto SpriteGesture::init(uint8_t* data, int gestureNum) -> int32_t
{
    gestureData = data;
    auto* header = reinterpret_cast<SpriteGestureHeader*>(data);
    const uint16_t numFrames = header->numFrames;
    shapeTable = data + sizeof(SpriteGestureHeader);

    if (numFrames != static_cast<uint16_t>(VFX_shape_count(shapeTable)))
    {
        return static_cast<int32_t>(0xbeef0005);
    }

    header->gestureNum = static_cast<int16_t>(gestureNum);
    return 0;
}
