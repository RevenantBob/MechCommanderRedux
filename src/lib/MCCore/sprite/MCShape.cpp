#include "stdafx.h"
#include "sprite/MCShape.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The size of the gesture header in front of a mech gesture packet's shape table.</summary>
    constexpr size_t GestureHeaderSize = 6;
}

MCShape::MCShape(MCRegisteredBlock packet, MCAppearanceType* owner, int32_t lastTurnUsed)
    : LastTurnUsed(lastTurnUsed), Owner(owner), _Packet(std::move(packet))
{
    uint8_t* shapeData = _Packet.Data();

    // A bare VFX shape table starts with its "1.10" version tag; anything else has a gesture header in front.
    if (std::memcmp(shapeData, "1.10", 4) != 0)
    {
        shapeData += GestureHeaderSize;
    }

    FrameList = shapeData;
    const int32_t numFrames = VfxShapeCount(shapeData);

    // OB-140 fixed: the original left a shape without frames ownerless (its type wasn't told when it went).
    if (numFrames == 0)
    {
        return;
    }

    int32_t firstFrame;
    std::memcpy(&firstFrame, shapeData + 8, sizeof(firstFrame));

    if (static_cast<int32_t>(_Packet.Size()) <= firstFrame || numFrames * 8 + 8 != firstFrame)
    {
        FrameList = nullptr;
    }
}
