#include "stdafx.h"
#include "sprite/vfxshape.h"
#include "appear/apprtype.h"
#include "sprite/sprtmgr.h"
#include "vfx/MCVfxFunctions.h"

auto MCShape::Destroy() -> void
{
    if (Owner != nullptr)
    {
        Owner->RemoveShape(this);
    }

    if (StupidHeader != nullptr)
    {
        SpriteManager->FreeShapeRam(StupidHeader);
        FrameList = nullptr;
        return;
    }

    SpriteManager->FreeShapeRam(FrameList);
    FrameList = nullptr;
}

auto MCShape::Init(uint8_t* shapeData, MCAppearanceType* shapeOwner, int32_t dataSize) -> int32_t
{
    // A bare VFX shape table starts with its "1.10" version tag; anything else has a gesture header in front.
    if (std::memcmp(shapeData, "1.10", 4) == 0)
    {
        StupidHeader = nullptr;
    }
    else
    {
        StupidHeader = shapeData;
        shapeData += 6;
    }

    FrameList = shapeData;
    const int32_t numFrames = VfxShapeCount(shapeData);

    if (numFrames == 0)
    {
        return -1;
    }

    int32_t firstFrame;
    std::memcpy(&firstFrame, shapeData + 8, sizeof(firstFrame));
    Owner = shapeOwner;

    if (dataSize <= firstFrame)
    {
        FrameList = nullptr;
        return -3;
    }

    if (numFrames * 8 + 8 != firstFrame)
    {
        FrameList = nullptr;
        return -4;
    }

    return 0;
}
