#include "stdafx.h"
#include "LinkTool.h"
#include "DreditorDoc.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorTile.h"
#include "ObjectLibrary.h"
#include "X87Extended.h"
#include "resource.h"

namespace
{
    /// <summary>The Link and Unlink cursors' hot spots.</summary>
    inline constexpr int32_t LINK_HOT_X = 9;
    inline constexpr int32_t LINK_HOT_Y = 0x10;
    inline constexpr int32_t UNLINK_HOT_X = 8;
    inline constexpr int32_t UNLINK_HOT_Y = 0x17;
    /// <summary>The longest action name the tool loads (a 256-byte buffer).</summary>
    inline constexpr int ACTION_NAME_SIZE = 0x100;
    /// <summary>The side a remade set's structures take when its control structure is gone.</summary>
    inline constexpr int32_t DEFAULT_SIDE = 1;

    /// <summary>The object id's class.</summary>
    int32_t classOf(const EditorObject& object)
    {
        return objectIdClass(object.id);
    }
}

LinkAction::LinkAction(const std::string& name, EditorMap* map) : EditorAction(name), map(map)
{
}

bool LinkAction::redo()
{
    return undo();
}

void LinkAction::record(const ControlLink& link, Change change)
{
    POINT cell;
    link.getCell(&cell);

    for (const Record& existing : records)
    {
        POINT recorded;
        existing.link.getCell(&recorded);

        if (recorded.x == cell.x && recorded.y == cell.y)
        {
            return;
        }
    }

    records.push_back({link, change});
}

bool LinkAction::undo()
{
    for (auto it = records.rbegin(); it != records.rend(); ++it)
    {
        Record& entry = *it;
        POINT cell;
        entry.link.getCell(&cell);

        switch (entry.change)
        {
            case Change::Created:
            {
                entry.change = Change::Removed;

                if (const EditorObject* controller = map->objectCoveringCell(cell.x, cell.y); controller != nullptr)
                {
                    if (ControlLink* current = map->controlLinkWith(*controller); current != nullptr)
                    {
                        entry.link = *current;
                        map->removeControlLink(*current);
                    }
                }

                break;
            }

            case Change::Removed:
            {
                entry.change = Change::Created;
                auto* remade = new ControlLink(entry.link);
                const int32_t count = remade->linkedCount();
                std::vector<POINT> cells(static_cast<size_t>(count));
                remade->getLinkedCells(cells.data(), count);

                // EB-087: the original keeps a set it couldn't add allocated, and the cells too.
                if (!map->addControlLink(remade))
                {
                    delete remade;
                }

                const EditorObject* controller = map->objectCoveringCell(cell.x, cell.y);
                const int32_t side = controller != nullptr ? controller->side : DEFAULT_SIDE;

                for (const POINT& linked : cells)
                {
                    if (EditorObject* structure = map->objectCoveringCell(linked.x, linked.y); structure != nullptr)
                    {
                        structure->side = side;
                    }
                }

                break;
            }

            case Change::Changed:
            {
                const EditorObject* controller = map->objectCoveringCell(cell.x, cell.y);

                if (controller == nullptr)
                {
                    return false;
                }

                if (ControlLink* current = map->controlLinkFor(*controller); current != nullptr)
                {
                    std::swap(*current, entry.link);
                }

                break;
            }
        }
    }

    return true;
}

bool LinkMarker::draw(HDC dc, [[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height, int32_t x, int32_t y,
                      [[maybe_unused]] int32_t zoom)
{
    HPEN pen = CreatePen(PS_DASH, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x, y, nullptr);
    LineTo(dc, dx + x, dy + y);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    return true;
}

LinkTool::LinkTool(DreditorDoc& document, bool link)
    : CursorTool(link ? IDB_LINK_CURSOR : IDB_UNLINK_CURSOR, link ? LINK_HOT_X : UNLINK_HOT_X,
                 link ? LINK_HOT_Y : UNLINK_HOT_Y)
    , document(document)
    , link(link)
{
}

LinkTool::~LinkTool()
{
    if (EditorMap* map = document.map; map != nullptr)
    {
        // Original behaviour: the line's mark goes by its position, then the marks at (0, 0) go.
        map->removeMark(nullptr, markX, markY);
        map->removeMark(marker, 0, 0);
        delete marker;
    }
}

POINT LinkTool::cellView(EditorMap& map, int32_t col, int32_t row)
{
    using X = X87Extended;
    const X cellSize = X(tileWorldWidth) / X(MAP_CELLS_PER_TILE);
    const auto x = static_cast<int32_t>((X(col) * cellSize).truncate());
    const auto y = static_cast<int32_t>((X(row) * cellSize).truncate());
    int32_t viewX = 0;
    int32_t viewY = 0;
    map.mapToView(x, y, map.terrainHeight(x, y), &viewX, &viewY);
    return {viewX, viewY};
}

void LinkTool::release(EditorMap& map)
{
    controllerClass = -1;

    if (marker != nullptr)
    {
        map.removeMark(marker, markX, markY);
        delete marker;
        marker = nullptr;
        controllerCol = -1;
        controllerRow = -1;
    }
}

bool LinkTool::begin(EditorMap& map)
{
    char name[ACTION_NAME_SIZE];

    if (LoadStringA(GetModuleHandleA(nullptr), IDS_LINK, name, ACTION_NAME_SIZE) == 0)
    {
        name[0] = '\0';
    }

    linkAction = new LinkAction(name, &map);
    return true;
}

EditorAction* LinkTool::end()
{
    if (linkAction == nullptr)
    {
        return nullptr;
    }

    if (!linkAction->records.empty())
    {
        return linkAction;
    }

    delete linkAction;
    linkAction = nullptr;
    return nullptr;
}

bool LinkTool::apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (!link)
    {
        return applyAgain(map, x, y, keys);
    }

    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    EditorObject* target = map.objectCoveringCell(col, row);

    // The original reads the object without testing for one; canApply refuses an empty cell first.
    if (target == nullptr)
    {
        return false;
    }

    if (ControlLink::isControllerClass(classOf(*target)))
    {
        const EditorObject* taken = map.objectCoveringCell(controllerCol, controllerRow);

        if (taken == target)
        {
            release(map);
            return true;
        }

        // A power station goes on taking turret controls; any other control structure is taken instead.
        if (controllerClass != OBJECT_CLASS_POWER_STATION || classOf(*target) != OBJECT_CLASS_TURRET_CONTROL)
        {
            release(map);
        }
    }

    if (controllerClass == -1)
    {
        controllerClass = classOf(*target);
        ControlLink* links = map.controlLinkFor(*target);
        const POINT view = cellView(map, target->cellCol, target->cellRow);
        markX = view.x;
        markY = view.y;

        if (marker == nullptr)
        {
            marker = new LinkMarker();
        }

        marker->color = 0;
        map.addMark(marker, markX, markY);
        controllerCol = target->cellCol;
        controllerRow = target->cellRow;

        if (links == nullptr)
        {
            links = new ControlLink(*target);
            const bool added = map.addControlLink(links);
            linkAction->record(*links, LinkAction::Change::Created);

            // EB-087: the original keeps a set it couldn't add allocated.
            if (!added)
            {
                delete links;
            }
        }
        else
        {
            linkAction->record(*links, LinkAction::Change::Changed);
        }

        return true;
    }

    const EditorObject* controller = map.objectCoveringCell(controllerCol, controllerRow);

    if (controller == nullptr)
    {
        return false;
    }

    ControlLink* links = map.controlLinkFor(*controller);
    // EB-087: the original keeps a set it couldn't add allocated; the port frees it at the end.
    std::unique_ptr<ControlLink> unadded;

    if (links == nullptr)
    {
        // Original behaviour: the new set is the clicked structure's, not the control structure's.
        links = new ControlLink(*target);

        if (!map.addControlLink(links))
        {
            unadded.reset(links);
        }

        linkAction->record(*links, LinkAction::Change::Created);
    }

    // EB-086: a structure that wasn't linked before is linked without a record, so the use isn't undoable.
    if (ControlLink* owner = map.controlLinkWith(*target); owner != nullptr && owner != links)
    {
        linkAction->record(*owner, LinkAction::Change::Changed);
        owner->removeLinked(*target);
        linkAction->record(*links, LinkAction::Change::Changed);
    }

    links->addLinked(*target);

    if (controllerClass != OBJECT_CLASS_POWER_STATION)
    {
        target->side = controller->side;
    }

    return true;
}

bool LinkTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    const EditorObject* target = map.objectCoveringCell(col, row);

    if (target == nullptr)
    {
        return false;
    }

    if (ControlLink* links = map.controlLinkFor(*target); links != nullptr)
    {
        linkAction->record(*links, LinkAction::Change::Removed);
        map.removeControlLink(*links);
        // Original behaviour: the line is forgotten, not taken off the map (an Unlink tool never has one).
        marker = nullptr;
        controllerClass = -1;
        markY = -1;
        markX = -1;
        return true;
    }

    ControlLink* owner = map.controlLinkWith(*target);

    if (owner == nullptr)
    {
        return true;
    }

    linkAction->record(*owner, LinkAction::Change::Changed);
    owner->removeLinked(*target);
    return true;
}

bool LinkTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    track(map, x, y, keys);

    if (!link)
    {
        return canApplyAgain(map, x, y, keys);
    }

    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    const EditorObject* target = map.objectCoveringCell(col, row);

    if (target == nullptr)
    {
        return false;
    }

    if (ControlLink::isControllerClass(classOf(*target)))
    {
        return true;
    }

    return controllerClass != -1 && ControlLink::controls(controllerClass, classOf(*target));
}

bool LinkTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    const EditorObject* target = map.objectCoveringCell(col, row);

    if (target == nullptr)
    {
        return false;
    }

    return map.controlLinkFor(*target) != nullptr || map.controlLinkWith(*target) != nullptr;
}

bool LinkTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return false;
}

void LinkTool::track(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    if (map.objectCoveringCell(controllerCol, controllerRow) == nullptr)
    {
        controllerClass = -1;
        controllerCol = -1;
        controllerRow = -1;
        map.removeMark(marker, markX, markY);
        delete marker;
        marker = nullptr;
        return;
    }

    if (marker != nullptr)
    {
        marker->dx = x - markX;
        marker->dy = y - markY;
    }
}
