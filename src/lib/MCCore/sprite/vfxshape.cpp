#include "stdafx.h"
#include "sprite/vfxshape.h"
#include "appear/apprtype.h"
#include "sprite/sprtmgr.h"
#include "vfx/vfxfuncs.h"

auto Shape::destroy() -> void
{
    if (owner != nullptr)
    {
        owner->removeShape(this);
    }

    if (stupidHeader != nullptr)
    {
        spriteManager->freeShapeRAM(stupidHeader);
        frameList = nullptr;
        return;
    }

    spriteManager->freeShapeRAM(frameList);
    frameList = nullptr;
}

auto Shape::init(uint8_t* shapeData, AppearanceType* shapeOwner, int32_t dataSize) -> int32_t
{
    // A bare VFX shape table starts with its "1.10" version tag; anything else has a gesture header in front.
    if (std::memcmp(shapeData, "1.10", 4) == 0)
    {
        stupidHeader = nullptr;
    }
    else
    {
        stupidHeader = shapeData;
        shapeData += 6;
    }

    frameList = shapeData;
    const int32_t numFrames = VFX_shape_count(shapeData);

    if (numFrames == 0)
    {
        return -1;
    }

    int32_t firstFrame;
    std::memcpy(&firstFrame, shapeData + 8, sizeof(firstFrame));
    owner = shapeOwner;

    if (dataSize <= firstFrame)
    {
        frameList = nullptr;
        return -3;
    }

    if (numFrames * 8 + 8 != firstFrame)
    {
        frameList = nullptr;
        return -4;
    }

    return 0;
}
