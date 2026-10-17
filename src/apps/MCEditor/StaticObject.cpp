#include "stdafx.h"
#include "StaticObject.h"
#include "EditorUtil.h"
#include "ObjectLibrary.h"

StaticObject noStaticObject(0xffffffff, -1, -1, -1);

StaticObject::StaticObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side)
{
    this->cellCol = cellCol;
    this->cellRow = cellRow;
    this->id = id;
    selected = false;
    this->side = side;
    shapeSlot = id != 0xffffffff ? objectLibrary->getShapeSlot(id) : nullptr;
}

StaticObject::StaticObject(const EditorObject& other)
{
    id = other.id;
    selected = other.selected;
    cellCol = other.cellCol;
    cellRow = other.cellRow;
    side = other.side;
    shapeSlot = other.shapeSlot;
}

void StaticObject::draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size)
{
    ShapeSlot* slot = shapeSlot;
    uint8_t** shapes[] = {&slot->shape, &slot->halfShape, &slot->icon};

    if (*shapes[size] == nullptr)
    {
        if (size == 0)
        {
            slot->shape = objectLibrary->getShape(slot->id);
        }
        else if (size == 1)
        {
            slot->halfShape = objectLibrary->getHalfShape(slot->id);
        }
        else if (size == 2)
        {
            objectLibrary->makeIcon(slot->id);
        }
    }

    drawShape(buffer, width, height, *shapes[size], 0, x, y);

    if (slot->delta && size != 2)
    {
        drawShape(buffer, width, height, *shapes[size], 1, x, y);
    }
}

EditorObject* StaticObject::clone()
{
    auto* copy = new StaticObject(*this);
    return copy;
}
