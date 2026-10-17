#include "stdafx.h"
#include "EditorMap.h"
#include "ControlLink.h"
#include "EditorObject.h"
#include "MoverObject.h"
#include "ObjectLibrary.h"
#include "Objective.h"
#include "UnitBrain.h"
#include "X87Extended.h"
#include "ai/move.h"

// The map's editing operations: selection, the tile under the cursor, and removing and moving objects with what refers
// to them.

namespace
{
    /// <summary>The view directions of the map's projection (0x005025b8, 0x005025c0).</summary>
    inline constexpr double VIEW_ANGLE_ACROSS = -0.7853981633973;
    inline constexpr double VIEW_ANGLE_DOWN = -0.9599310885967;
    /// <summary>What <see cref="EditorMap::vertexElevation"/> gives off the map.</summary>
    inline constexpr int32_t NO_ELEVATION = 0x7fffffff;
    /// <summary>The string of the question asked before an objective's target is removed (IDS_IF_YOU_ERASE_THIS).</summary>
    inline constexpr UINT OBJECTIVE_QUESTION = 0xe011;
    /// <summary>The string of the question asked before a unit's orders' target is removed.</summary>
    inline constexpr UINT ORDERS_QUESTION = 0xe032;
    /// <summary>The caption of the questions ("MCEditor").</summary>
    inline constexpr UINT QUESTION_CAPTION = 2;
    /// <summary>How many cells in from the grid's edges an object must stand.</summary>
    inline constexpr int32_t PLACE_MARGIN = 4;
    /// <summary>The drop zones of a multiplayer mission, and how many of them a side may have.</summary>
    inline constexpr int32_t MULTIPLAYER_DROP_ZONES = 6;
    inline constexpr int32_t MULTIPLAYER_DROP_ZONES_PER_SIDE = 2;

    /// <summary>The name of mover <paramref name="id"/>'s type.</summary>
    const char* moverTypeName(uint32_t id)
    {
        return objectLibrary->moverGroups[objectIdGroup(id)].types[objectIdType(id)].name.c_str();
    }
}

bool EditorMap::viewToCell(int32_t viewX, int32_t viewY, int32_t* col, int32_t* row)
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    viewToMap(viewX, viewY, &x, &y, &z);
    const X87Extended cellSize = X87Extended(tileWorldWidth) / X87Extended(MAP_CELLS_PER_TILE);
    *col = static_cast<int32_t>((X87Extended(x) / cellSize).truncate());
    *row = static_cast<int32_t>((X87Extended(y) / cellSize).truncate());
    return *col >= 0 && *col < maxRow * MAP_CELLS_PER_TILE && *row >= 0 && *row < maxCol * MAP_CELLS_PER_TILE;
}

int32_t EditorMap::areaAt(int32_t col, int32_t row) const
{
    if (globalMap == nullptr)
    {
        return 0;
    }

    return globalMap->calcArea(row / MAP_CELLS_PER_TILE, col / MAP_CELLS_PER_TILE);
}

bool EditorMap::isPlaceable(const EditorObject& object)
{
    const int32_t col = object.cellCol;
    const int32_t row = object.cellRow;
    const int32_t limit = maxRow * MAP_CELLS_PER_TILE - PLACE_MARGIN;

    if (col < PLACE_MARGIN || col >= limit || row < PLACE_MARGIN || row >= limit ||
        cellGrid[static_cast<size_t>(maxRow * MAP_CELLS_PER_TILE * row + col)] != nullptr)
    {
        return false;
    }

    const int32_t objectClass = objectIdClass(object.id);

    if (objectClass == OBJECT_CLASS_DROPZONE)
    {
        const int32_t allowed = maxDropZones();

        if (allowed <= static_cast<int32_t>(dropZones.size()))
        {
            return false;
        }

        if (allowed == MULTIPLAYER_DROP_ZONES)
        {
            int32_t onSide = 0;

            for (const EditorObject* zone : dropZones)
            {
                if (zone->side == object.side)
                {
                    onSide++;
                }

                if (onSide > MULTIPLAYER_DROP_ZONES_PER_SIDE)
                {
                    return false;
                }
            }
        }
    }
    else if (objectClass == OBJECT_CLASS_TURRET && maxTurrets() <= turretCount)
    {
        return false;
    }

    return true;
}

CView* EditorMap::activeView()
{
    // Original behaviour: no main window or view crashes here.
    return static_cast<CFrameWnd*>(AfxGetThread()->GetMainWnd())->GetActiveView();
}

bool EditorMap::viewToTile(int32_t viewX, int32_t viewY, int32_t* col, int32_t* row)
{
    int32_t vertexCol = 0;
    int32_t vertexRow = 0;
    int32_t offsetX = 0;
    int32_t offsetY = 0;
    findNearestVertex(viewX, viewY, &vertexCol, &vertexRow, &offsetX, &offsetY);
    // The ratio of the tile's sides in the view, against the slope of the offset: steep offsets pick the tile above or
    // below the vertex, shallow ones the tile beside it.
    const double across = std::cos(VIEW_ANGLE_ACROSS) * tileWorldWidth;
    const double down = std::cos(VIEW_ANGLE_DOWN) * across;
    const double length = std::sqrt(across * across + down * down);
    const double ratio = (across / length) / (down / length);
    const double slope = std::fabs(static_cast<double>(offsetX) / static_cast<double>(offsetY));

    // An offset of (0, 0) divides zero by zero; the unordered comparison takes the second branch.
    if (ratio >= slope)
    {
        if (offsetY > 0)
        {
            *col = vertexCol;
            *row = vertexRow;
        }
        else
        {
            *col = vertexCol - 1;
            *row = vertexRow - 1;
        }
    }
    else if (offsetX > 0)
    {
        *col = vertexCol;
        *row = vertexRow - 1;
    }
    else
    {
        *col = vertexCol - 1;
        *row = vertexRow;
    }

    return *col >= 0 && *col < maxRow && *row >= 0 && *row < maxCol;
}

int32_t EditorMap::vertexElevation(int32_t col, int32_t row)
{
    const EditorTile* tile = getTile(col - 1, row - 1);
    int32_t corner = 3;

    if (tile == nullptr)
    {
        if (col == 0)
        {
            if (row == 0)
            {
                tile = getTile(0, 0);
                corner = 0;
            }
            else
            {
                tile = getTile(0, row - 1);
                corner = 2;
            }
        }
        else
        {
            if (row != 0)
            {
                return NO_ELEVATION;
            }

            tile = getTile(col - 1, 0);
            corner = 1;
        }

        if (tile == nullptr)
        {
            return NO_ELEVATION;
        }
    }

    return tile->elevations[corner];
}

void EditorMap::setVertexSelected(int32_t col, int32_t row, bool keep)
{
    for (auto vertex = selectedVertices.begin(); vertex != selectedVertices.end(); ++vertex)
    {
        if (vertex->x == col && vertex->y == row)
        {
            if (!keep)
            {
                selectedVertices.erase(vertex);
            }

            return;
        }
    }

    selectedVertices.push_back({col, row});
}

void EditorMap::deselectAll()
{
    selectedVertices.clear();

    for (EditorObject* object : objects)
    {
        object->selected = false;
    }

    for (EditorObject* mover : movers)
    {
        mover->selected = false;
    }

    for (EditorObject* zone : dropZones)
    {
        zone->selected = false;
    }

    for (EditorTile& tile : tiles)
    {
        tile.selected = false;
    }

    tilesSelected = false;
}

void EditorMap::clearHighlight()
{
    for (EditorTile& tile : tiles)
    {
        tile.highlighted = false;
    }
}

EditorObject* EditorMap::objectCoveringCell(int32_t col, int32_t row)
{
    if (objectAtCell(col, row) == nullptr)
    {
        return nullptr;
    }

    for (EditorObject* object : objects)
    {
        if (object->coversCell(col, row))
        {
            return object;
        }
    }

    for (EditorObject* zone : dropZones)
    {
        if (zone->coversCell(col, row))
        {
            return zone;
        }
    }

    return nullptr;
}

void EditorMap::setFootprintCells(uint32_t id, int32_t col, int32_t row, EditorObject* value)
{
    const int32_t cellsPerRow = maxRow * MAP_CELLS_PER_TILE;
    const uint64_t footprint = frameFootprint(objectFrame(id));

    // Original behaviour (EB-037): neither the cell nor the footprint is clipped to the grid; the port clips.
    if (isCellInGrid(col, row))
    {
        cellGrid[static_cast<size_t>(cellsPerRow * row + col)] = value;
    }

    constexpr int32_t SIDE = 8;
    constexpr int32_t CENTRE = 4;

    for (int32_t r = 0; r < SIDE; r++)
    {
        for (int32_t c = 0; c < SIDE; c++)
        {
            if ((footprint >> (r * SIDE + c) & 1) != 0 || (c == CENTRE && r == CENTRE))
            {
                const auto index = static_cast<int64_t>(row - CENTRE + r) * cellsPerRow + (col - CENTRE + c);

                if (index >= 0 && index < static_cast<int64_t>(cellGrid.size()))
                {
                    cellGrid[static_cast<size_t>(index)] = value;
                }
            }
        }
    }
}

bool EditorMap::removeObject(const EditorObject& object, bool checkReferences)
{
    const int32_t col = object.cellCol;
    const int32_t row = object.cellRow;

    if (checkReferences)
    {
        // Every objective that refers to it goes, if the user agrees to each.
        const auto findObjective = [&](const EditorObject& target) -> Objective*
        {
            for (Objective* objective : objectives)
            {
                if (objective->referencesObject(target, *this))
                {
                    return objective;
                }
            }

            return nullptr;
        };

        const EditorObject* current = &object;
        Objective* objective = findObjective(*current);

        while (objective != nullptr)
        {
            CString format;
            format.LoadString(OBJECTIVE_QUESTION);
            CString question;
            question.Format(format, objectTypeName(current->id), static_cast<const char*>(objective->name));
            CString caption;
            caption.LoadString(QUESTION_CAPTION);
            activeView()->SendMessage(WM_MOUSELEAVE, 0, 0);

            if (activeView()->MessageBox(question, caption, MB_YESNO) == IDNO)
            {
                return false;
            }

            objectives.remove(objective);
            objective->release(*this);
            delete objective;

            // The object is looked for again on its cell (the objective may have taken it with it).
            current = objectCoveringCell(col, row);

            if (current == nullptr)
            {
                // Original behaviour: the object is gone (or was a mover), so nothing more is done and it counts as
                // removed.
                return true;
            }

            objective = findObjective(*current);
        }

        // Units whose orders target it get new orders, if the user agrees to each.
        for (EditorObject* moverObject : movers)
        {
            auto* mover = static_cast<MoverObject*>(moverObject);

            if (!mover->brain->referencesObject(object, *this))
            {
                continue;
            }

            CString format;
            format.LoadString(ORDERS_QUESTION);
            const char* moverName = moverTypeName(mover->id);
            CString question;
            question.Format(format, objectTypeName(object.id), moverName, moverName);
            CString caption;
            caption.LoadString(QUESTION_CAPTION);
            activeView()->SendMessage(WM_MOUSELEAVE, 0, 0);

            if (::MessageBoxA(nullptr, question, caption, MB_YESNO) != IDYES)
            {
                return false;
            }

            mover->resetBrain();
        }
    }

    if (objectIdClass(object.id) == OBJECT_CLASS_TURRET)
    {
        turretCount--;
    }

    if (checkReferences)
    {
        // A control structure's links go; a linked structure leaves its controller's links.
        ControlLink* own = nullptr;

        for (ControlLink* link : controlLinks)
        {
            if (link->isFor(object))
            {
                own = link;
                break;
            }
        }

        if (own != nullptr)
        {
            for (auto link = controlLinks.begin(); link != controlLinks.end(); ++link)
            {
                if ((*link)->equals(*own))
                {
                    delete *link;
                    controlLinks.erase(link);
                    break;
                }
            }
        }
        else
        {
            for (ControlLink* link : controlLinks)
            {
                if (link->isFor(object) || link->isLinked(object))
                {
                    link->removeLinked(object);
                    break;
                }
            }
        }
    }

    setFootprintCells(object.id, col, row, nullptr);

    for (auto it = objects.begin(); it != objects.end(); ++it)
    {
        if ((*it)->isSameAs(object))
        {
            delete *it;
            objects.erase(it);
            return true;
        }
    }

    for (auto it = dropZones.begin(); it != dropZones.end(); ++it)
    {
        if ((*it)->isSameAs(object))
        {
            delete *it;
            dropZones.erase(it);
            return true;
        }
    }

    return false;
}

bool EditorMap::removeMover(const EditorObject& mover, bool checkReferences)
{
    const int32_t col = mover.cellCol;
    const int32_t row = mover.cellRow;

    if (checkReferences)
    {
        const auto findObjective = [&]() -> Objective*
        {
            for (Objective* objective : objectives)
            {
                if (objective->referencesObject(mover, *this))
                {
                    return objective;
                }
            }

            return nullptr;
        };

        Objective* objective = findObjective();

        while (objective != nullptr)
        {
            CString format;
            format.LoadString(OBJECTIVE_QUESTION);
            CString question;
            question.Format(format, objectTypeName(mover.id), static_cast<const char*>(objective->name));
            CString caption;
            caption.LoadString(QUESTION_CAPTION);
            activeView()->SendMessage(WM_MOUSELEAVE, 0, 0);

            if (::MessageBoxA(nullptr, question, caption, MB_YESNO) == IDNO)
            {
                return false;
            }

            objectives.remove(objective);
            objective->release(*this);
            delete objective;
            objective = findObjective();
        }

        for (EditorObject* otherObject : movers)
        {
            auto* other = static_cast<MoverObject*>(otherObject);

            if (!other->brain->referencesObject(mover, *this))
            {
                continue;
            }

            CString format;
            format.LoadString(ORDERS_QUESTION);
            const char* otherName = moverTypeName(other->id);
            CString question;
            question.Format(format, moverTypeName(mover.id), otherName, otherName);
            CString caption;
            caption.LoadString(QUESTION_CAPTION);
            activeView()->SendMessage(WM_MOUSELEAVE, 0, 0);

            if (::MessageBoxA(nullptr, question, caption, MB_YESNO) != IDYES)
            {
                return false;
            }

            other->resetBrain();
        }
    }

    setFootprintCells(mover.id, col, row, nullptr);

    for (auto it = movers.begin(); it != movers.end(); ++it)
    {
        if ((*it)->isSameAs(mover))
        {
            delete *it;
            movers.erase(it);
            return true;
        }
    }

    return false;
}

bool EditorMap::removeAnyObject(const EditorObject& object, bool checkReferences)
{
    if (isMoverClass(objectIdClass(object.id)))
    {
        return removeMover(object, checkReferences);
    }

    return removeObject(object, checkReferences);
}

ControlLink* EditorMap::controlLinkFor(const EditorObject& controller)
{
    for (ControlLink* link : controlLinks)
    {
        if (link->isFor(controller))
        {
            return link;
        }
    }

    return nullptr;
}

ControlLink* EditorMap::controlLinkWith(const EditorObject& object)
{
    for (ControlLink* link : controlLinks)
    {
        if (link->isFor(object) || link->isLinked(object))
        {
            return link;
        }
    }

    return nullptr;
}

bool EditorMap::addObjective(Objective* objective)
{
    if (std::find(objectives.begin(), objectives.end(), objective) != objectives.end())
    {
        return false;
    }

    objectives.push_back(objective);
    return true;
}

bool EditorMap::removeObjective(Objective* objective)
{
    const auto found = std::find(objectives.begin(), objectives.end(), objective);

    if (found == objectives.end())
    {
        return false;
    }

    objectives.erase(found);
    objective->release(*this);
    delete objective;
    return true;
}

Objective* EditorMap::objectiveFor(const EditorObject& object)
{
    for (Objective* objective : objectives)
    {
        if (objective->referencesObject(object, *this))
        {
            return objective;
        }
    }

    return nullptr;
}

bool EditorMap::addControlLink(ControlLink* link)
{
    for (const ControlLink* existing : controlLinks)
    {
        if (existing->equals(*link))
        {
            return false;
        }
    }

    controlLinks.push_back(link);
    return true;
}

bool EditorMap::removeControlLink(const ControlLink& link)
{
    for (auto it = controlLinks.begin(); it != controlLinks.end(); ++it)
    {
        if ((*it)->equals(link))
        {
            delete *it;
            controlLinks.erase(it);
            return true;
        }
    }

    return false;
}

bool EditorMap::placeAnyObject(EditorObject* object)
{
    if (isMoverClass(objectIdClass(object->id)))
    {
        return addMover(static_cast<MoverObject*>(object));
    }

    return addObject(object);
}

bool EditorMap::moveObject(EditorObject& object, int32_t col, int32_t row)
{
    const int32_t oldCol = object.cellCol;
    const int32_t oldRow = object.cellRow;

    if (col == oldCol && row == oldRow)
    {
        return true;
    }

    // The objectives' and the orders' references follow it.
    for (bool moved = true; moved;)
    {
        moved = false;

        for (Objective* objective : objectives)
        {
            if (objective->referencesObject(object, *this))
            {
                objective->moveReferences(object, *this, col, row);
                moved = true;
                break;
            }
        }
    }

    for (EditorObject* moverObject : movers)
    {
        auto* mover = static_cast<MoverObject*>(moverObject);

        if (mover->brain->referencesObject(object, *this))
        {
            mover->brain->moveReferences(object, *this, col, row);
        }
    }

    // A control structure's links follow it; a linked structure is linked again on its new cell.
    ControlLink* relink = nullptr;
    EditorObject* structure = objectCoveringCell(oldCol, oldRow);

    if (structure != nullptr)
    {
        ControlLink* own = nullptr;

        for (ControlLink* link : controlLinks)
        {
            if (link->isFor(*structure))
            {
                own = link;
                break;
            }
        }

        if (own != nullptr)
        {
            own->cellCol = col;
            own->cellRow = row;
        }
        else
        {
            for (ControlLink* link : controlLinks)
            {
                if (link->isFor(*structure) || link->isLinked(*structure))
                {
                    relink = link;
                    link->removeLinked(*structure);
                    break;
                }
            }
        }
    }

    setFootprintCells(object.id, oldCol, oldRow, nullptr);
    setFootprintCells(object.id, col, row, &object);
    object.cellCol = col;
    object.cellRow = row;

    if (relink != nullptr)
    {
        relink->addLinked(*structure);
    }

    return true;
}
