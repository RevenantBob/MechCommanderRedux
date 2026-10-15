#include "stdafx.h"
#include "PlaceObjectTool.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "StaticObject.h"
#include "TileLibrary.h"
#include "X87Extended.h"
#include "resource.h"

namespace
{
    /// <summary>Where the object's hot spot is in the cursor image.</summary>
    inline constexpr int32_t IMAGE_HOT_X = 0xa0;
    inline constexpr int32_t IMAGE_HOT_Y = 0x78;
    /// <summary>The colour index of a transparent cursor pixel.</summary>
    inline constexpr uint8_t TRANSPARENT_PIXEL = 0xff;
    /// <summary>The footprint's side; the object's own cell is at (4, 4) in it.</summary>
    inline constexpr int32_t FOOTPRINT_SIDE = 8;
    inline constexpr int32_t FOOTPRINT_CENTRE = 4;
    /// <summary>The most objects a tile may hold.</summary>
    inline constexpr int32_t MAX_TILE_OBJECTS = 7;
    /// <summary>The <c>tileSetStringOffset</c> of the terrain set whose dirt roads take no objects.</summary>
    inline constexpr int32_t ROADLESS_TILE_SET = 0x32;
    /// <summary>The drop zones a side may have, as the error box says.</summary>
    inline constexpr int32_t DROP_ZONES_PER_SIDE = 3;
    /// <summary>A cell's place within its tile: the first and last of the three.</summary>
    inline constexpr int32_t FIRST_CELL = 0;
    inline constexpr int32_t LAST_CELL = 2;

    /// <summary>
    /// Whether a corner of <paramref name="tile"/> has <paramref name="texture"/> (water also where a corner is at
    /// elevation 0); a concrete or crop tile has only its own texture, on every corner.
    /// </summary>
    /// <remarks>The editor's loops over the corners, which stop at the first on a concrete or crop tile.</remarks>
    bool hasTexture(const EditorTile& tile, uint8_t texture)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (tile.concIndex != -1)
            {
                return texture == TEXTURE_CONC;
            }

            if (tile.cropIndex != -1)
            {
                return texture == TEXTURE_CROP;
            }

            if (tile.textures[i] == texture || (tile.elevations[i] == 0 && texture == TEXTURE_WATER))
            {
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Whether <paramref name="tile"/>'s terrain takes an object of <paramref name="frame"/>: no water (unless it
    /// stands in the air), no cliff, and no corner steeper than its slope.
    /// </summary>
    bool terrainAllows(const EditorTile& tile, const ObjectFrame& frame)
    {
        if (!frame.airTerrain && hasTexture(tile, TEXTURE_WATER))
        {
            return false;
        }

        if (hasTexture(tile, TEXTURE_CLIFF))
        {
            return false;
        }

        uint8_t heights[4];
        tile.getHeights(heights);

        for (const uint8_t height : heights)
        {
            if (height > frame.slope)
            {
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Whether every cell of <paramref name="id"/>'s footprint around cell (<paramref name="col"/>,
    /// <paramref name="row"/>) is free of objects (and of a mech or vehicle one cell down-right), but
    /// <paramref name="ignored"/>.
    /// </summary>
    bool footprintFree(EditorMap& map, uint32_t id, int32_t col, int32_t row, const EditorObject* ignored)
    {
        const uint64_t footprint = frameFootprint(objectFrame(id));

        for (int32_t r = 0; r < FOOTPRINT_SIDE; r++)
        {
            for (int32_t c = 0; c < FOOTPRINT_SIDE; c++)
            {
                const bool centre = c == FOOTPRINT_CENTRE && r == FOOTPRINT_CENTRE;

                if ((footprint >> (r * FOOTPRINT_SIDE + c) & 1) == 0 && !centre)
                {
                    continue;
                }

                const EditorObject* other = map.objectAt(col + c - FOOTPRINT_CENTRE, row + r - FOOTPRINT_CENTRE);

                if (other != nullptr && other != ignored)
                {
                    return false;
                }
            }
        }

        return true;
    }

    /// <summary>
    /// Whether tile (<paramref name="tileCol"/>, <paramref name="tileRow"/>) holds at most
    /// <see cref="MAX_TILE_OBJECTS"/> objects (by the cells they cover), <paramref name="ignored"/> not counting.
    /// </summary>
    bool tileHasRoom(EditorMap& map, int32_t tileCol, int32_t tileRow, const EditorObject* ignored)
    {
        const EditorObject* found[MAX_TILE_OBJECTS + 1] = {};

        for (int32_t r = 0; r < MAP_CELLS_PER_TILE; r++)
        {
            for (int32_t c = 0; c < MAP_CELLS_PER_TILE; c++)
            {
                const EditorObject* object =
                    map.objectCoveringCell(tileCol * MAP_CELLS_PER_TILE + c, tileRow * MAP_CELLS_PER_TILE + r);

                if (object == nullptr || object == ignored)
                {
                    continue;
                }

                for (int32_t i = 0;; i++)
                {
                    if (found[i] == object)
                    {
                        break;
                    }

                    if (i >= MAX_TILE_OBJECTS)
                    {
                        return false;
                    }

                    if (found[i] == nullptr)
                    {
                        found[i] = object;
                        break;
                    }
                }
            }
        }

        return true;
    }

    /// <summary>Moves a gate's cell to the middle cell of its tile.</summary>
    void centreInTile(int32_t* col, int32_t* row)
    {
        for (int32_t* cell : {col, row})
        {
            if (*cell % MAP_CELLS_PER_TILE == FIRST_CELL)
            {
                (*cell)++;
            }
            else if (*cell % MAP_CELLS_PER_TILE == LAST_CELL)
            {
                (*cell)--;
            }
        }
    }

    /// <summary>Draws <paramref name="slot"/>'s shape at zoom level <paramref name="zoom"/> into <paramref name="image"/>.</summary>
    void drawSlot(uint8_t* image, ShapeSlot& slot, int32_t zoom)
    {
        uint8_t** shapes[] = {&slot.shape, &slot.halfShape, &slot.icon};

        if (*shapes[zoom] == nullptr)
        {
            if (zoom == 0)
            {
                slot.shape = objectLibrary->getShape(slot.id);
            }
            else if (zoom == 1)
            {
                slot.halfShape = objectLibrary->getHalfShape(slot.id);
            }
            else if (zoom == 2)
            {
                objectLibrary->makeIcon(slot.id);
            }
        }

        drawShape(image, PlaceObjectTool::IMAGE_WIDTH, PlaceObjectTool::IMAGE_HEIGHT, *shapes[zoom], 0, IMAGE_HOT_X,
                  IMAGE_HOT_Y);

        if (slot.delta && zoom != 2)
        {
            drawShape(image, PlaceObjectTool::IMAGE_WIDTH, PlaceObjectTool::IMAGE_HEIGHT, *shapes[zoom], 1, IMAGE_HOT_X,
                      IMAGE_HOT_Y);
        }
    }
}

ObjectsAction::ObjectsAction(const std::string& name, EditorMap* map, bool erase)
    : TileAction(name, map), erase(erase), objectMap(map)
{
}

ObjectsAction::~ObjectsAction()
{
    for (EditorObject* object : objects)
    {
        delete object;
    }
}

bool ObjectsAction::redo()
{
    TileAction::redo();
    return erase ? removeRecorded() : placeRecorded();
}

bool ObjectsAction::undo()
{
    TileAction::undo();
    return erase ? placeRecorded() : removeRecorded();
}

bool ObjectsAction::isRecorded(const EditorObject& object) const
{
    for (const EditorObject* recorded : objects)
    {
        if (object.isSameAs(*recorded))
        {
            return true;
        }
    }

    return false;
}

void ObjectsAction::recordObject(EditorObject& object)
{
    if (!isRecorded(object))
    {
        objects.push_back(object.clone());
    }
}

bool ObjectsAction::removeRecorded()
{
    if (replace)
    {
        swapRecorded();
        return true;
    }

    bool removed = true;

    for (EditorObject* object : objects)
    {
        if (!objectMap->removeAnyObject(*object, true))
        {
            removed = false;
        }
    }

    return removed;
}

bool ObjectsAction::placeRecorded()
{
    if (replace)
    {
        swapRecorded();
        return true;
    }

    bool placed = true;

    for (EditorObject* object : objects)
    {
        // Original behaviour: a copy the map refuses is leaked.
        if (!objectMap->placeAnyObject(object->clone()))
        {
            placed = false;
        }
    }

    return placed;
}

void ObjectsAction::swapRecorded()
{
    for (EditorObject*& recorded : objects)
    {
        // Original behaviour: no object on the recorded cell is a crash; the port leaves the record.
        EditorObject* current = objectMap->objectAt(recorded->cellCol, recorded->cellRow);

        if (current == nullptr)
        {
            continue;
        }

        EditorObject* copy = current->clone();
        objectMap->removeAnyObject(*current, true);
        objectMap->placeAnyObject(recorded);
        recorded = copy;
    }
}

MoveObjectsAction::MoveObjectsAction(EditorMap* map) : ObjectsAction("", map, false)
{
}

bool MoveObjectsAction::redo()
{
    auto destination = destinations.begin();

    for (EditorObject* recorded : objects)
    {
        EditorObject* current = objectMap->objectAt(recorded->cellCol, recorded->cellRow);

        if (current == nullptr)
        {
            return false;
        }

        if (current->isSameAs(*recorded))
        {
            objectMap->moveObject(*current, destination->x, destination->y);
        }

        ++destination;
    }

    return true;
}

bool MoveObjectsAction::undo()
{
    auto destination = destinations.begin();

    for (EditorObject* recorded : objects)
    {
        if (EditorObject* current = objectMap->objectAt(destination->x, destination->y); current != nullptr)
        {
            objectMap->moveObject(*current, recorded->cellCol, recorded->cellRow);
        }

        ++destination;
    }

    return true;
}

PlaceObjectTool::PlaceObjectTool(uint32_t id, int32_t side, bool erase) : id(id), erase(erase), side(side)
{
    if (erase)
    {
        return;
    }

    ShapeSlot* slot = objectLibrary->getShapeSlot(id);

    if (slot == nullptr)
    {
        return;
    }

    std::memset(image, TRANSPARENT_PIXEL, sizeof(image));

    if (slot->shape == nullptr)
    {
        slot->shape = objectLibrary->getShape(slot->id);
    }

    drawShape(image, IMAGE_WIDTH, IMAGE_HEIGHT, slot->shape, 0, IMAGE_HOT_X, IMAGE_HOT_Y);

    if (slot->delta)
    {
        drawShape(image, IMAGE_WIDTH, IMAGE_HEIGHT, slot->shape, 1, IMAGE_HOT_X, IMAGE_HOT_Y);
    }

    for (uint8_t& pixel : image)
    {
        if (pixel != TRANSPARENT_PIXEL)
        {
            pixel = editorPalette->greenTint[pixel];
        }
    }
}

bool PlaceObjectTool::begin(EditorMap& map)
{
    std::string name;

    if (!erase)
    {
        name = objectTypeName(id);
    }
    else
    {
        CString text;
        text.LoadString(ID_ERASE_OVERLAYS);
        name = static_cast<LPCTSTR>(text);
    }

    action = new ObjectsAction(name, &map, erase);
    active = true;
    return true;
}

EditorAction* PlaceObjectTool::end()
{
    if (action != nullptr && action->objects.empty())
    {
        delete action;
        action = nullptr;
    }

    EditorAction* result = action;
    action = nullptr;
    active = false;
    return result;
}

bool PlaceObjectTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    if (objectIdClass(id) == OBJECT_CLASS_GATE)
    {
        // A gate takes the walls off its tile.
        const int32_t tileCol = col / MAP_CELLS_PER_TILE;
        const int32_t tileRow = row / MAP_CELLS_PER_TILE;
        EditorTile* tile = map.getTile(tileCol, tileRow);
        action->recordTile(tileCol, tileRow, *tile);

        if (tile->overlay != nullptr)
        {
            for (const int32_t kind : tile->overlay->kinds)
            {
                if (kind == OVERLAY_WALL || kind == OVERLAY_HEAVY_WALL || kind == OVERLAY_LIGHT_WALL)
                {
                    // EB-078 fixed: the original goes on reading the corners of the overlay it has deleted.
                    delete tile->overlay;
                    tile->overlay = nullptr;
                    break;
                }
            }
        }

        centreInTile(&col, &row);
    }

    if (!erase)
    {
        auto* object = new StaticObject(id, col, row, side);

        if (!map.addObject(object))
        {
            delete object;
            return true;
        }

        action->recordObject(*object);
        return true;
    }

    EditorObject* object = map.objectAt(col, row);

    if (object == nullptr)
    {
        return false;
    }

    action->recordObject(*object);

    if (!map.removeAnyObject(*object, true))
    {
        // The user kept the object: the drag ends.
        CView* view = static_cast<CFrameWnd*>(AfxGetThread()->GetMainWnd())->GetActiveView();
        ::SendMessageA(view->m_hWnd, WM_LBUTTONUP, 0, 0);
    }

    return true;
}

bool PlaceObjectTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (!erase)
    {
        erase = true;
        action->erase = true;
        apply(map, x, y, keys);
        erase = !erase;
    }

    // The original returns what was left in a register.
    return true;
}

bool PlaceObjectTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    return canPlaceAt(map, col, row);
}

bool PlaceObjectTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    if (erase)
    {
        return false;
    }

    erase = true;
    const bool result = canApply(map, x, y, keys);
    erase = !erase;
    return result;
}

bool PlaceObjectTool::hasTarget(EditorMap& map)
{
    if (!erase)
    {
        return false;
    }

    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (const EditorObject* object : *list)
        {
            if (object->selected)
            {
                return true;
            }
        }
    }

    return false;
}

const uint8_t* PlaceObjectTool::cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                            int32_t zoom)
{
    if (erase || id == 0xffffffff)
    {
        return nullptr;
    }

    return drawCursor(map, position, topLeft, size, zoom, editorPalette->greenTint);
}

const uint8_t* PlaceObjectTool::notAllowedImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                                int32_t zoom)
{
    if (erase || id == 0xffffffff)
    {
        return nullptr;
    }

    return drawCursor(map, position, topLeft, size, zoom, editorPalette->redTint);
}

bool PlaceObjectTool::canPlaceAt(EditorMap& map, int32_t col, int32_t row)
{
    if (erase)
    {
        return true;
    }

    const int32_t tileCol = col / MAP_CELLS_PER_TILE;
    const int32_t tileRow = row / MAP_CELLS_PER_TILE;
    const ObjectFrame& frame = objectFrame(id);
    const int32_t objectClass = objectIdClass(id);
    // Original behaviour: the caller keeps the cell on the grid, so the tile exists.
    const EditorTile& tile = *map.getTile(tileCol, tileRow);

    if (objectClass == OBJECT_CLASS_SPRAY_CAN && (tile.concIndex != -1 || tile.cropIndex != -1))
    {
        return false;
    }

    if (!frame.airTerrain && hasTexture(tile, TEXTURE_WATER))
    {
        return false;
    }

    if (hasTexture(tile, TEXTURE_CLIFF))
    {
        return false;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        const int32_t kind = tile.overlay != nullptr ? tile.overlay->kinds[i] : OVERLAY_NONE;
        bool blocked = kind == OVERLAY_FOREST || kind == OVERLAY_BRIDGE ||
                       (kind == OVERLAY_DIRT_ROAD && tileSetStringOffset == ROADLESS_TILE_SET);

        // A gate goes into a wall.
        if (objectClass != OBJECT_CLASS_GATE &&
            (kind == OVERLAY_WALL || kind == OVERLAY_HEAVY_WALL || kind == OVERLAY_LIGHT_WALL))
        {
            blocked = true;
        }

        if (blocked)
        {
            return false;
        }
    }

    uint8_t heights[4];
    tile.getHeights(heights);

    for (const uint8_t height : heights)
    {
        if (height > frame.slope)
        {
            return false;
        }
    }

    StaticObject probe(id, col, row, side);

    if (map.isPlaceable(probe))
    {
        return footprintFree(map, id, col, row, nullptr) && tileHasRoom(map, tileCol, tileRow, nullptr);
    }

    if (!active)
    {
        return false;
    }

    if (objectClass == OBJECT_CLASS_TURRET && map.turretCount >= map.maxTurrets())
    {
        CString text;
        text.Format(IDS_THE_MAXIMUM_NUMBER_OF, map.maxTurrets());
        showError(text);
    }
    else if (objectClass == OBJECT_CLASS_DROPZONE)
    {
        int32_t onSide = 0;

        for (const EditorObject* zone : map.dropZones)
        {
            if (zone->side == side)
            {
                onSide++;
            }
        }

        if (onSide >= DROP_ZONES_PER_SIDE)
        {
            CString text;
            text.Format(IDS_A_PLAYER_MAY_ONLY, DROP_ZONES_PER_SIDE);
            showError(text);
        }
    }

    return false;
}

void PlaceObjectTool::showError(const CString& text)
{
    errorDialog.text = text;
    active = false;

    if (errorDialog.m_hWnd == nullptr)
    {
        errorDialog.Create(ErrorDialog::IDD, AfxGetThread()->GetMainWnd());
    }

    errorDialog.CenterWindow();
    errorDialog.ShowWindow(SW_SHOW);
}

bool PlaceObjectTool::eraseSelected(EditorMap& map)
{
    if (!erase)
    {
        return true;
    }

    std::list<EditorObject*> selected;

    for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones, &map.movers})
    {
        for (EditorObject* object : *list)
        {
            if (object->selected)
            {
                selected.push_back(object);
            }
        }
    }

    for (EditorObject* object : selected)
    {
        action->recordObject(*object);
        map.removeAnyObject(*object, true);
    }

    return true;
}

bool PlaceObjectTool::stepFrame(int32_t delta)
{
    if (erase)
    {
        return false;
    }

    // Only a type with another look steps.
    StaticObject probe(id, 0, 0, side);
    uint32_t nextId = probe.id;
    const ShapeSlot* next = objectLibrary->stepFrame(1, &nextId);

    if (next == nullptr || next == probe.shapeSlot)
    {
        return false;
    }

    ShapeSlot* slot = objectLibrary->stepFrame(delta, &id);

    if (slot == nullptr)
    {
        return false;
    }

    std::memset(image, TRANSPARENT_PIXEL, sizeof(image));
    drawSlot(image, *slot, 0);
    return true;
}

const uint8_t* PlaceObjectTool::drawCursor(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                           int32_t zoom, const uint8_t* tint)
{
    size->cx = IMAGE_WIDTH;
    size->cy = IMAGE_HEIGHT;
    // The cells' half size, kept as a float.
    const float halfCell =
        (X87Extended(tileWorldWidth) / (X87Extended(MAP_CELLS_PER_TILE) + X87Extended(MAP_CELLS_PER_TILE))).toFloat();
    int32_t mapX = 0;
    int32_t mapY = 0;
    int32_t mapZ = 0;
    map.viewToMap(position.x, position.y, &mapX, &mapY, &mapZ);
    const X87Extended cellSize = X87Extended(tileWorldWidth) / X87Extended(MAP_CELLS_PER_TILE);
    int32_t col = static_cast<int32_t>((X87Extended(mapX) / cellSize).truncate());
    int32_t row = static_cast<int32_t>((X87Extended(mapY) / cellSize).truncate());

    if (objectIdClass(id) == OBJECT_CLASS_GATE)
    {
        centreInTile(&col, &row);
    }

    if (col < 0 || row < 0 || col >= map.maxRow * MAP_CELLS_PER_TILE || row >= map.maxCol * MAP_CELLS_PER_TILE)
    {
        return nullptr;
    }

    // The cell's centre on the map, then in the view.
    const int32_t centreX =
        static_cast<int32_t>(((X87Extended(col * 2) + X87Extended(1.0f)) * X87Extended(halfCell)).truncate());
    const int32_t centreY =
        static_cast<int32_t>(((X87Extended(row * 2) + X87Extended(1.0f)) * X87Extended(halfCell)).truncate());
    const int32_t centreZ = map.terrainHeight(centreX, centreY);
    int32_t viewX = 0;
    int32_t viewY = 0;
    map.mapToView(centreX, centreY, centreZ, &viewX, &viewY);
    topLeft->x = viewX;
    topLeft->y = viewY;
    ShapeSlot* slot = objectLibrary->getShapeSlot(id);

    if (slot != nullptr)
    {
        std::memset(image, TRANSPARENT_PIXEL, sizeof(image));
        drawSlot(image, *slot, zoom);

        for (uint8_t& pixel : image)
        {
            if (pixel != TRANSPARENT_PIXEL)
            {
                pixel = tint[pixel];
            }
        }
    }

    return image;
}

MoveObjectsTool::MoveObjectsTool() : PlaceObjectTool(0, 0, false)
{
    id = 0xffffffff;
}

bool MoveObjectsTool::begin(EditorMap& map)
{
    dragging = true;
    action = new MoveObjectsAction(&map);
    return true;
}

EditorAction* MoveObjectsTool::end()
{
    dragging = false;

    if (object != nullptr)
    {
        static_cast<MoveObjectsAction*>(action)->destinations.push_back({object->cellCol, object->cellRow});
    }

    object = nullptr;
    return PlaceObjectTool::end();
}

bool MoveObjectsTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;
    map.viewToCell(x, y, &col, &row);
    map.moveObject(*object, col, row);
    return true;
}

bool MoveObjectsTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!dragging || object == nullptr)
    {
        if (!map.viewToCell(x, y, &col, &row))
        {
            return false;
        }

        object = map.objectAt(col, row);

        if (object != nullptr)
        {
            id = object->id;
            action->recordObject(*object);
        }

        return object != nullptr;
    }

    if (!map.viewToCell(x, y, &col, &row))
    {
        return false;
    }

    const int32_t tileCol = col / MAP_CELLS_PER_TILE;
    const int32_t tileRow = row / MAP_CELLS_PER_TILE;
    // Original behaviour: the cell is on the grid, so the tile exists.
    const EditorTile& tile = *map.getTile(tileCol, tileRow);
    return terrainAllows(tile, objectFrame(id)) && footprintFree(map, id, col, row, object) &&
           tileHasRoom(map, tileCol, tileRow, object);
}

bool MoveObjectsTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x,
                                    [[maybe_unused]] int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return false;
}

bool MoveObjectsTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return false;
}

const uint8_t* MoveObjectsTool::cursorImage(EditorMap& map, const POINT& position, POINT* topLeft, SIZE* size,
                                            int32_t zoom)
{
    if (erase || id == 0xffffffff)
    {
        return nullptr;
    }

    return drawCursor(map, position, topLeft, size, zoom, editorPalette->greenTint);
}
