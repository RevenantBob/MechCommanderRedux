#include "stdafx.h"
#include "sprite/sprite.h"
#include "vfx/MCVfxFunctions.h"

auto MCSpriteGesture::Destroy() -> void
{
    GestureData = nullptr;
    ShapeTable = nullptr;
}

auto MCSpriteGesture::Init(uint8_t* data, int gestureNum) -> int32_t
{
    GestureData = data;
    auto* header = reinterpret_cast<MCSpriteGestureHeader*>(data);
    const uint16_t numFrames = header->NumFrames;
    ShapeTable = data + sizeof(MCSpriteGestureHeader);

    if (numFrames != static_cast<uint16_t>(VfxShapeCount(ShapeTable)))
    {
        return static_cast<int32_t>(0xbeef0005);
    }

    header->GestureNum = static_cast<int16_t>(gestureNum);
    return 0;
}
