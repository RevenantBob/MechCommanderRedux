#include "stdafx.h"
#include "ControlLink.h"
#include "EditorObject.h"
#include "ObjectLibrary.h"

namespace
{
    /// <summary>The bits of an object id that give its group and type.</summary>
    inline constexpr uint32_t OBJECT_TYPE_BITS = 0xffff0000;

    /// <summary>Whether ids <paramref name="a"/> and <paramref name="b"/> are of the same object type.</summary>
    bool sameType(uint32_t a, uint32_t b)
    {
        return ((a ^ b) & OBJECT_TYPE_BITS) == 0;
    }
}

ControlLink::ControlLink(const EditorObject& controller)
    : id(controller.id), cellCol(controller.cellCol), cellRow(controller.cellRow)
{
}

bool ControlLink::addLinked(const EditorObject& structure)
{
    for (const LinkedStructure& entry : linked)
    {
        if (entry.cellCol == structure.cellCol && entry.cellRow == structure.cellRow && entry.id == structure.id)
        {
            return false;
        }
    }

    linked.push_back({structure.id, structure.cellCol, structure.cellRow});
    return true;
}

int32_t ControlLink::linkedCount() const
{
    return static_cast<int32_t>(linked.size());
}

void ControlLink::getLinkedCells(POINT* cells, int32_t count) const
{
    int32_t i = 0;

    for (const LinkedStructure& entry : linked)
    {
        i++;

        if (i > count)
        {
            return;
        }

        cells[i - 1] = {entry.cellCol, entry.cellRow};
    }
}

void ControlLink::getCell(POINT* cell) const
{
    *cell = {cellCol, cellRow};
}

bool ControlLink::isLinked(const EditorObject& structure) const
{
    for (const LinkedStructure& entry : linked)
    {
        if (entry.cellCol == structure.cellCol && entry.cellRow == structure.cellRow &&
            (entry.id == structure.id || sameType(entry.id, structure.id)))
        {
            return true;
        }
    }

    return false;
}

bool ControlLink::isFor(const EditorObject& controller) const
{
    return cellCol == controller.cellCol && cellRow == controller.cellRow && sameType(controller.id, id);
}

bool ControlLink::removeLinked(const EditorObject& structure)
{
    if (isFor(structure))
    {
        return false;
    }

    for (auto entry = linked.begin(); entry != linked.end(); ++entry)
    {
        if (sameType(entry->id, structure.id) && entry->cellCol == structure.cellCol &&
            entry->cellRow == structure.cellRow)
        {
            linked.erase(entry);
            return true;
        }
    }

    return false;
}

bool ControlLink::equals(const ControlLink& other) const
{
    if (other.id != id || other.cellCol != cellCol || other.cellRow != cellRow || other.linked.size() != linked.size())
    {
        return false;
    }

    for (const LinkedStructure& entry : linked)
    {
        bool found = false;

        for (const LinkedStructure& theirs : other.linked)
        {
            if (theirs.id == entry.id && theirs.cellCol == entry.cellCol && theirs.cellRow == entry.cellRow)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return false;
        }
    }

    return true;
}

bool ControlLink::hasSameController(const ControlLink& other) const
{
    return id == other.id && cellCol == other.cellCol && cellRow == other.cellRow;
}

bool ControlLink::isControllerClass(int32_t objectClass)
{
    return objectClass > OBJECT_CLASS_DROPZONE && objectClass < OBJECT_CLASS_PERIMETER_ALARM;
}

bool ControlLink::controls(int32_t controllerClass, int32_t targetClass)
{
    switch (controllerClass)
    {
        case OBJECT_CLASS_TURRET_CONTROL:
        {
            return targetClass == OBJECT_CLASS_TURRET;
        }
        case OBJECT_CLASS_GATE_CONTROL:
        {
            return targetClass == OBJECT_CLASS_GATE;
        }
        case OBJECT_CLASS_POWER_STATION:
        {
            return targetClass == OBJECT_CLASS_TURRET_CONTROL;
        }
        case OBJECT_CLASS_SENSOR_CONTROL:
        {
            return targetClass == OBJECT_CLASS_SENSOR_TOWER;
        }
        default:
        {
            return false;
        }
    }
}
