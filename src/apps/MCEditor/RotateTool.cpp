#include "stdafx.h"
#include "RotateTool.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "PlaceObjectTool.h"
#include "resource.h"

namespace
{
    /// <summary>The tool's cursor hot spot.</summary>
    inline constexpr int32_t ROTATE_HOT_X = 0x11;
    inline constexpr int32_t ROTATE_HOT_Y = 0xe;
    /// <summary>The longest action name the tool loads.</summary>
    inline constexpr int ACTION_NAME_SIZE = 0x100;

    /// <summary>
    /// The object on the cell under view position (<paramref name="x"/>, <paramref name="y"/>), or a mech or vehicle
    /// one cell down-right of it.
    /// </summary>
    EditorObject* objectUnder(EditorMap& map, int32_t x, int32_t y)
    {
        int32_t col = 0;
        int32_t row = 0;

        if (!map.viewToCell(x, y, &col, &row))
        {
            return nullptr;
        }

        return map.objectAt(col, row);
    }
}

RotateTool::RotateTool(int32_t step) : BrushTool(false, IDB_ROTATE_CURSOR, ROTATE_HOT_X, ROTATE_HOT_Y), step(step)
{
}

bool RotateTool::begin(EditorMap& map)
{
    char name[ACTION_NAME_SIZE];

    if (LoadStringA(GetModuleHandleA(nullptr), ID_ROTATION, name, ACTION_NAME_SIZE) == 0)
    {
        name[0] = '\0';
    }

    objectsAction = new ObjectsAction(name, &map, false);
    objectsAction->replace = true;
    return true;
}

EditorAction* RotateTool::end()
{
    return objectsAction;
}

bool RotateTool::rotateObject(EditorMap& map, EditorObject& object)
{
    objectsAction->recordObject(object);
    turn(map, object);
    return true;
}

void RotateTool::turn(EditorMap& map, EditorObject& object) const
{
    EditorObject* turned = object.clone();
    map.removeAnyObject(object, false);
    turned->cycleFrame(step);
    map.placeAnyObject(turned);
}

bool RotateTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    if (EditorObject* object = objectUnder(map, x, y); object != nullptr)
    {
        objectsAction->recordObject(*object);
        turn(map, *object);
    }

    return true;
}

bool RotateTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    step = -step;
    const bool result = apply(map, x, y, keys);
    step = -step;
    return result;
}

bool RotateTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    EditorObject* object = objectUnder(map, x, y);
    return object != nullptr && object->hasOtherFrame();
}

bool RotateTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return canApply(map, x, y, keys);
}

bool RotateTool::hasTarget(EditorMap& map)
{
    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (EditorObject* object : *list)
        {
            if (object->selected && object->hasOtherFrame())
            {
                return true;
            }
        }
    }

    return false;
}

bool RotateTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (EditorObject* object : *list)
        {
            if (object->selected)
            {
                objectsAction->recordObject(*object);
            }
        }
    }

    // The map's objects are replaced by their turned copies through the records, each like its object.
    for (EditorObject* record : objectsAction->objects)
    {
        turn(map, *record);
    }

    return true;
}
