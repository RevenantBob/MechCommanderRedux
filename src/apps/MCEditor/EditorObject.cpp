#include "stdafx.h"
#include "EditorObject.h"
#include "ObjectLibrary.h"

namespace
{
    /// <summary>The frames of the type of <paramref name="id"/> (not a drop zone).</summary>
    std::vector<ObjectFrame>& typeFrames(uint32_t id)
    {
        std::vector<ObjectGroup>& groups =
            isMoverClass(objectIdClass(id)) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
        return groups[objectIdGroup(id)].types[objectIdType(id)].frames;
    }
}

void EditorObject::cycleFrame(int32_t delta)
{
    if (objectIdClass(id) == OBJECT_CLASS_DROPZONE)
    {
        return;
    }

    std::vector<ObjectFrame>& frames = typeFrames(id);

    if (frames.empty())
    {
        return;
    }

    const auto last = static_cast<int32_t>(frames.size()) - 1;
    int32_t frame = static_cast<int32_t>(objectIdFrame(id)) + delta;

    if (frame > last)
    {
        frame = 0;
    }
    else if (frame < 0)
    {
        frame = last;
    }

    const uint32_t look = objectIdSlot(id);
    id = (id & 0xffff00ff) | static_cast<uint32_t>(frame) << 8;
    shapeSlot = &frames[static_cast<size_t>(frame)].slots[look];
}

bool EditorObject::hasOtherFrame()
{
    if (objectIdClass(id) == OBJECT_CLASS_DROPZONE)
    {
        return false;
    }

    std::vector<ObjectFrame>& frames = typeFrames(id);

    if (frames.empty())
    {
        return false;
    }

    int32_t frame = static_cast<int32_t>(objectIdFrame(id)) + 1;

    if (static_cast<int32_t>(frames.size()) - 1 < frame)
    {
        frame = 0;
    }

    return &frames[static_cast<size_t>(frame)].slots[objectIdSlot(id)] != shapeSlot;
}

bool EditorObject::coversCell(int32_t col, int32_t row) const
{
    constexpr int32_t FOOTPRINT_SIDE = 8;
    constexpr int32_t FOOTPRINT_CENTRE = 4;
    constexpr int32_t FOOTPRINT_REACH = 8;
    const int32_t dx = col - cellCol;
    const int32_t dy = row - cellRow;

    // The original tests whether either distance is in reach (the footprint test below needs both).
    if (std::abs(dx) > FOOTPRINT_REACH && std::abs(dy) > FOOTPRINT_REACH)
    {
        return false;
    }

    if (col == cellCol && row == cellRow)
    {
        return true;
    }

    const int32_t footCol = dx + FOOTPRINT_CENTRE;
    const int32_t footRow = dy + FOOTPRINT_CENTRE;

    if (footCol < 0 || footCol >= FOOTPRINT_SIDE || footRow < 0 || footRow >= FOOTPRINT_SIDE)
    {
        return false;
    }

    return (frameFootprint(objectFrame(id)) >> (footRow * FOOTPRINT_SIDE + footCol) & 1) != 0;
}

bool EditorObject::isSameAs(const EditorObject& other) const
{
    return cellCol == other.cellCol && cellRow == other.cellRow && id == other.id;
}

void EditorObject::setDestroyed(bool destroyed)
{
    id = destroyed ? id | 1 : id & ~1u;

    if (ShapeSlot* slot = objectLibrary->getShapeSlot(id); slot != nullptr)
    {
        shapeSlot = slot;
    }
}
