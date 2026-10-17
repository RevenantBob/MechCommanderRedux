#include "stdafx.h"
#include "VertexTool.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "PlaceObjectTool.h"
#include "TileLibrary.h"

namespace
{
    /// <summary>The highest elevation a vertex may have.</summary>
    inline constexpr int32_t MAX_ELEVATION = 0x80;
    /// <summary>The most two corners of a tile may differ (1 when the tile touches water).</summary>
    inline constexpr int32_t TILE_REACH = 2;
    /// <summary>The elevation a lowered vertex is taken 2 down from when a selected neighbour is at 1.</summary>
    inline constexpr int32_t STEEP_LOWER_FROM = 3;
    /// <summary>The bits of each random draw <see cref="VertexTool::makeHilly"/> uses before drawing again.</summary>
    inline constexpr int32_t HILL_BITS = 16;
    /// <summary>The look index of a concrete or crop change: its low bits.</summary>
    inline constexpr int32_t LOOK_INDEX_MASK = 0xf;
    /// <summary>A concrete or crop change's texture is above this shift.</summary>
    inline constexpr int32_t LOOK_TEXTURE_SHIFT = 16;

    /// <summary>The vertex at corner <paramref name="corner"/> of tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    POINT cornerVertex(int32_t col, int32_t row, int32_t corner)
    {
        return {col + (corner & 1), row + (corner >> 1)};
    }

    /// <summary>
    /// The view distance from (<paramref name="x"/>, <paramref name="y"/>) to the nearest vertex, which goes to
    /// (<paramref name="col"/>, <paramref name="row"/>).
    /// </summary>
    /// <remarks>0x00452ba0.</remarks>
    float nearestVertexDistance(EditorMap& map, int32_t x, int32_t y, int32_t* col, int32_t* row)
    {
        int32_t offsetX = 0;
        int32_t offsetY = 0;
        map.findNearestVertex(x, y, col, row, &offsetX, &offsetY);
        // The squares wrap as the original's 32-bit products do.
        const auto squares = static_cast<int32_t>(static_cast<uint32_t>(offsetX) * static_cast<uint32_t>(offsetX) +
                                                  static_cast<uint32_t>(offsetY) * static_cast<uint32_t>(offsetY));
        return std::sqrt(static_cast<float>(squares));
    }

    /// <summary>The tile's textures, water where a corner is at elevation 0.</summary>
    void shownTextures(const EditorTile& tile, uint8_t* textures)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            textures[i] = tile.elevations[i] == 0 ? static_cast<uint8_t>(TEXTURE_WATER) : tile.textures[i];
        }
    }

    /// <summary>Whether vertex (<paramref name="col"/>, <paramref name="row"/>) is selected.</summary>
    bool isVertexSelected(const EditorMap& map, int32_t col, int32_t row)
    {
        return std::any_of(map.selectedVertices.begin(), map.selectedVertices.end(),
                           [col, row](const POINT& vertex) { return vertex.x == col && vertex.y == row; });
    }
}

VertexTool::VertexTool(int32_t step, uint8_t texture, bool ownAction, UINT bitmapId)
    : TextureTool(texture, bitmapId != 0 ? bitmapId : (step != 1 ? IDB_LOWER_CURSOR : IDB_RAISE_CURSOR))
    , lastX(MARK_ANYWHERE)
    , lastY(MARK_ANYWHERE)
    , ownAction(ownAction)
    , step(step)
{
}

HeightAction::HeightAction(const std::string& name, EditorMap* map) : ObjectsAction(name, map, true)
{
}

bool VertexTool::begin(EditorMap& map)
{
    lastX = MARK_ANYWHERE;
    lastY = MARK_ANYWHERE;

    if (!ownAction)
    {
        return true;
    }

    char name[0x100];

    if (LoadStringA(GetModuleHandleA(nullptr), ID_CHANGE_HEIGHT, name, static_cast<int>(sizeof(name))) == 0)
    {
        name[0] = '\0';
    }

    // Original behaviour: a height action not ended is dropped without being deleted.
    heightAction = new HeightAction(name, &map);
    return startAction(map, ID_CHANGE_HEIGHT);
}

EditorAction* VertexTool::end()
{
    if (!ownAction)
    {
        return nullptr;
    }

    if (EditorAction* painted = TextureTool::end(); painted != nullptr)
    {
        for (const TileAction::TileRecord& record : static_cast<TileAction*>(painted)->records)
        {
            heightAction->recordTile(record.col, record.row, record.tile);
        }

        delete painted;
    }

    ObjectsAction* result = heightAction;

    if (result != nullptr && result->records.empty())
    {
        delete result;
        result = nullptr;
    }

    heightAction = nullptr;
    return result;
}

bool VertexTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    lastY = y;
    lastX = x;
    int32_t col = 0;
    int32_t row = 0;

    if (nearestVertexDistance(map, x, y, &col, &row) < static_cast<float>(VERTEX_REACH * 2))
    {
        map.setVertexSelected(col, row, true);
    }

    const bool result = applyToSelection(map, step);
    map.selectedVertices.clear();
    return result;
}

bool VertexTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    lastY = y;
    lastX = x;
    int32_t col = 0;
    int32_t row = 0;

    if (nearestVertexDistance(map, x, y, &col, &row) < static_cast<float>(VERTEX_REACH * 2))
    {
        map.setVertexSelected(col, row, true);
    }

    const bool result = applyToSelection(map, -1);
    map.selectedVertices.clear();
    return result;
}

bool VertexTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;
    nearestVertexDistance(map, x, y, &col, &row);
    const int32_t elevation = map.vertexElevation(col, row);
    const auto signedKeys = static_cast<int32_t>(keys);

    if (elevation == 0)
    {
        return signedKeys >= 0;
    }

    return elevation != MAX_ELEVATION || signedKeys <= 0;
}

bool VertexTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (nearestVertexDistance(map, x, y, &col, &row) < static_cast<float>(VERTEX_REACH * 2))
    {
        if (map.vertexElevation(col, row) > 0 && step == 1)
        {
            return true;
        }

        // Original behaviour (EB-082): lowering asks for more than the highest elevation.
        if (map.vertexElevation(col, row) > MAX_ELEVATION && step == -1)
        {
            return true;
        }
    }

    return false;
}

bool VertexTool::applyToSelection(EditorMap& map, int32_t step)
{
    Work work;
    bool result = true;
    const bool wholeMap = map.selectedVertices.size() == static_cast<size_t>((map.maxCol + 1) * (map.maxRow + 1));

    if (wholeMap)
    {
        ownAction = false;

        for (int32_t col = 0; col < map.maxRow; col++)
        {
            for (int32_t row = 0; row < map.maxCol; row++)
            {
                if (heightAction != nullptr)
                {
                    heightAction->appendRecord(col, row, *map.getTile(col, row));
                }
            }
        }
    }

    if (step == -1)
    {
        // A vertex at 3 next to a selected one at 1 goes down 2, so that the slope between them stays.
        static constexpr POINT NEIGHBOURS[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {1, 1}, {1, -1}, {-1, 1}};
        std::list<VertexChange> moves;

        for (const POINT& vertex : map.selectedVertices)
        {
            int32_t delta = -1;

            if (map.vertexElevation(vertex.x, vertex.y) == STEEP_LOWER_FROM)
            {
                for (const POINT& offset : NEIGHBOURS)
                {
                    const int32_t col = vertex.x + offset.x;
                    const int32_t row = vertex.y + offset.y;

                    if (map.vertexElevation(col, row) == 1 && isVertexSelected(map, col, row))
                    {
                        delta = -2;
                        break;
                    }
                }
            }

            moves.push_back({vertex.x, vertex.y, delta});
        }

        for (auto move = moves.rbegin(); move != moves.rend(); ++move)
        {
            adjustVertex(map, move->col, move->row, move->value, work);
        }
    }
    else
    {
        for (const POINT& vertex : map.selectedVertices)
        {
            adjustVertex(map, vertex.x, vertex.y, step, work);
        }
    }

    // The followers, from the back; a move adds its own at the front, which the walk then reaches.
    for (auto change = work.changes.end(); change != work.changes.begin();)
    {
        --change;
        const VertexChange move = *change;

        if (isVertexSelected(map, move.col, move.row) && vertexFits(map, move.col, move.row, move.value))
        {
            continue;
        }

        const bool moved = adjustVertex(map, move.col, move.row, move.value, work);
        result = moved && result;
    }

    repair(map, work);
    removeStrandedObjects(map);

    if (map.selectedVertices.size() == static_cast<size_t>((map.maxCol + 1) * (map.maxRow + 1)))
    {
        ownAction = true;
    }

    return result;
}

bool VertexTool::setSelectedElevation(EditorMap& map, int32_t elevation)
{
    if (map.selectedVertices.size() == static_cast<size_t>((map.maxCol + 1) * (map.maxRow + 1)))
    {
        setAllElevations(map, elevation);
        return true;
    }

    Work work;
    bool result = true;

    for (const POINT& vertex : map.selectedVertices)
    {
        adjustVertex(map, vertex.x, vertex.y, elevation - map.vertexElevation(vertex.x, vertex.y), work);
        result = processChanges(map, work, result);
        work.changes.clear();
    }

    repair(map, work);
    removeStrandedObjects(map);
    return result;
}

bool VertexTool::makeHilly(EditorMap& map)
{
    Work work;
    auto up = static_cast<uint32_t>(std::rand());
    auto down = static_cast<uint32_t>(std::rand());
    int32_t bit = 0;

    for (const POINT& vertex : map.selectedVertices)
    {
        auto delta = static_cast<int32_t>(up >> bit & 1);

        if ((down >> bit & 1) != 0)
        {
            delta = -delta;
        }

        bit++;
        adjustVertex(map, vertex.x, vertex.y, delta, work);

        if (bit >= HILL_BITS)
        {
            up = static_cast<uint32_t>(std::rand());
            down = static_cast<uint32_t>(std::rand());
            bit = 0;
        }

        processChanges(map, work, true);
        work.changes.clear();
    }

    repair(map, work);
    return true;
}

bool VertexTool::flattenTile(EditorMap& map, int32_t col, int32_t row)
{
    const POINT vertices[4] = {{col, row}, {col + 1, row}, {col, row + 1}, {col + 1, row + 1}};
    int32_t elevations[4];
    float sum = 0.0f;

    for (int32_t i = 0; i < 4; i++)
    {
        elevations[i] = map.vertexElevation(vertices[i].x, vertices[i].y);
        sum = static_cast<float>(elevations[i]) + sum;
    }

    // Exact in any precision: the elevations are small integers.
    const auto target = static_cast<int32_t>(std::floor(static_cast<double>(sum) * 0.25 + 0.5));
    Work work;
    bool result = true;

    for (int32_t i = 0; i < 4; i++)
    {
        const bool moved = adjustVertex(map, vertices[i].x, vertices[i].y, target - elevations[i], work);
        result = moved && result;
    }

    result = processChanges(map, work, result);
    repair(map, work);
    return result;
}

bool VertexTool::moveVertex(EditorMap& map, int32_t col, int32_t row, int32_t delta)
{
    Work work;
    const bool moved = adjustVertex(map, col, row, delta, work);
    const bool result = processChanges(map, work, moved);
    repair(map, work);
    return result;
}

int32_t VertexTool::removeStrandedObjects(EditorMap& map)
{
    int32_t removed = 0;

    for (std::list<EditorObject*>* list : {&map.objects, &map.dropZones})
    {
        for (auto it = list->begin(); it != list->end();)
        {
            EditorObject* object = *it;
            ++it;
            const ObjectFrame& frame = objectFrame(object->id);
            const EditorTile* tile =
                map.getTile(object->cellCol / MAP_CELLS_PER_TILE, object->cellRow / MAP_CELLS_PER_TILE);

            if (tile == nullptr)
            {
                continue;
            }

            bool stranded = (!frame.airTerrain && tile->hasTexture(TEXTURE_WATER)) || tile->hasTexture(TEXTURE_CLIFF);

            if (!stranded)
            {
                uint8_t heights[4];
                tile->getHeights(heights);
                stranded = std::any_of(std::begin(heights), std::end(heights),
                                       [&frame](uint8_t height) { return frame.slope < height; });
            }

            if (stranded)
            {
                if (heightAction != nullptr)
                {
                    heightAction->recordObject(*object);
                }

                if (!map.removeObject(*object, true))
                {
                    return -1;
                }

                removed++;
            }
        }
    }

    for (auto it = map.movers.begin(); it != map.movers.end();)
    {
        EditorObject* mover = *it;
        ++it;
        const EditorTile* tile = map.getTile(mover->cellCol / MAP_CELLS_PER_TILE, mover->cellRow / MAP_CELLS_PER_TILE);

        if (tile == nullptr)
        {
            continue;
        }

        if ((!objectFrame(mover->id).airTerrain && tile->hasTexture(TEXTURE_WATER)) || tile->hasTexture(TEXTURE_CLIFF))
        {
            if (heightAction != nullptr)
            {
                heightAction->recordObject(*mover);
            }

            map.removeMover(*mover, true);
            removed++;
        }
    }

    return removed;
}

bool VertexTool::adjustVertex(EditorMap& map, int32_t col, int32_t row, int32_t delta, Work& work)
{
    int32_t reach = TILE_REACH;
    const int32_t elevation = map.vertexElevation(col, row) + delta;

    if (elevation == 0)
    {
        reach = 1;
    }
    else if (elevation < 0 || elevation > MAX_ELEVATION)
    {
        return false;
    }

    if (ownAction && delta != 0 && heightAction != nullptr)
    {
        heightAction->recordVertex(col, row);
    }

    // The vertex is corner 2 of the tile above, 0 of its own, 1 of the one to the left and 3 of the one up-left.
    struct Corner
    {
        int32_t col;
        int32_t row;
        int32_t corner;
        int32_t firstChecked;
    };

    // The original checks its own tile's corners for water from the second one.
    const Corner corners[4] = {{col, row - 1, 2, 0}, {col, row, 0, 1}, {col - 1, row, 1, 0}, {col - 1, row - 1, 3, 0}};

    for (const Corner& at : corners)
    {
        EditorTile* tile = map.getTile(at.col, at.row);

        if (tile == nullptr)
        {
            continue;
        }

        noteTile(*tile, at.col, at.row, work);
        int32_t elevations[4];
        std::copy(std::begin(tile->elevations), std::end(tile->elevations), elevations);
        int32_t tileReach = reach;

        for (int32_t i = at.firstChecked; i < 4; i++)
        {
            if (elevations[i] == 0)
            {
                tileReach = 1;
            }
        }

        for (int32_t i = 0; i < 4; i++)
        {
            if (i == at.corner)
            {
                continue;
            }

            const int32_t apart = std::abs(elevations[i] - elevations[at.corner] - delta);

            if (apart > tileReach)
            {
                const POINT other = cornerVertex(at.col, at.row, i);
                const int32_t follow = delta < 0 ? -(apart - tileReach) : apart - tileReach;
                pushChange(work.changes, other.x, other.y, follow);
            }
        }

        elevations[at.corner] += delta;
        tile->setElevations(elevations);
    }

    return true;
}

bool VertexTool::processChanges(EditorMap& map, Work& work, bool result)
{
    for (auto change = work.changes.end(); change != work.changes.begin();)
    {
        --change;
        const VertexChange move = *change;
        const bool moved = adjustVertex(map, move.col, move.row, move.value, work);
        result = moved && result;
    }

    return result;
}

void VertexTool::repair(EditorMap& map, Work& work)
{
    repairCliffWater(map, work.cliffWater);
    repairOverlays(map, work.overlays);
    repairMountain(map, work.mountains);
    repairConcCrop(map, work.concCrop);
}

void VertexTool::setAllElevations(EditorMap& map, int32_t elevation)
{
    const int32_t elevations[4] = {elevation, elevation, elevation, elevation};
    // Original behaviour (EB-083): a tile of nothing but cliff and water takes the last other texture found, or before any the
    // elevation itself as a texture.
    int32_t fallback = elevation;

    for (int32_t row = 0; row < map.maxCol; row++)
    {
        for (int32_t col = 0; col < map.maxRow; col++)
        {
            EditorTile* tile = map.getTile(col, row);

            if (ownAction && heightAction != nullptr)
            {
                heightAction->appendRecord(col, row, *tile);
            }

            if (tile->setElevations(elevations))
            {
                continue;
            }

            if (!tile->hasTexture(TEXTURE_WATER) && !tile->hasTexture(TEXTURE_CLIFF))
            {
                continue;
            }

            uint8_t textures[4];
            shownTextures(*tile, textures);
            auto texture = static_cast<uint8_t>(fallback);

            for (const uint8_t shown : textures)
            {
                if (shown != TEXTURE_CLIFF && shown != TEXTURE_WATER)
                {
                    texture = shown;
                    fallback = shown;
                    break;
                }
            }

            const uint8_t flat[4] = {texture, texture, texture, texture};
            tile->setTextures(flat);
        }
    }
}

void VertexTool::noteTile(const EditorTile& tile, int32_t col, int32_t row, Work& work)
{
    if (tile.overlay != nullptr)
    {
        addTile(work.overlays, col, row);
    }

    if (tile.hasTexture(TEXTURE_CLIFF) || tile.hasTexture(TEXTURE_WATER))
    {
        addTile(work.cliffWater, col, row);
    }

    if (tile.hasTexture(TEXTURE_MOUN))
    {
        addTile(work.mountains, col, row);
    }

    if (tile.cropIndex != -1)
    {
        pushChange(work.concCrop, col, row, TEXTURE_CROP << LOOK_TEXTURE_SHIFT | tile.cropIndex);
    }

    if (tile.concIndex != -1)
    {
        pushChange(work.concCrop, col, row, TEXTURE_CONC << LOOK_TEXTURE_SHIFT | tile.concIndex);
    }
}

void VertexTool::pushChange(std::list<VertexChange>& changes, int32_t col, int32_t row, int32_t value)
{
    for (VertexChange& change : changes)
    {
        if (change.col == col && change.row == row)
        {
            if (std::abs(value) > std::abs(change.value))
            {
                change.value = value;
            }

            return;
        }
    }

    changes.push_front({col, row, value});
}

void VertexTool::addTile(std::list<POINT>& tiles, int32_t col, int32_t row)
{
    for (const POINT& tile : tiles)
    {
        if (tile.x == col && tile.y == row)
        {
            return;
        }
    }

    tiles.push_front({col, row});
}

bool VertexTool::vertexFits(EditorMap& map, int32_t col, int32_t row, int32_t delta)
{
    const EditorTile* tiles[4] = {map.getTile(col - 1, row - 1), map.getTile(col, row - 1), map.getTile(col - 1, row),
                                  map.getTile(col, row)};
    const int32_t elevation = map.vertexElevation(col, row);

    for (const EditorTile* tile : tiles)
    {
        if (tile == nullptr)
        {
            continue;
        }

        for (const int32_t corner : tile->elevations)
        {
            if ((corner - elevation > TILE_REACH && delta > 0) || (corner - elevation < -TILE_REACH && delta < 0))
            {
                return false;
            }
        }
    }

    return true;
}

void VertexTool::repairCliffWater(EditorMap& map, std::list<POINT>& tiles)
{
    const uint8_t fallback = TileLibrary::getDefaultTexture();

    for (auto it = tiles.rbegin(); it != tiles.rend(); ++it)
    {
        EditorTile* tile = map.getTile(it->x, it->y);
        uint8_t textures[4];
        shownTextures(*tile, textures);
        uint8_t heights[4];
        tile->getHeights(heights);
        const int32_t id = tileLibrary->pickTile(textures, heights, -1);
        tileLibrary->releaseTile(id);

        if (id != NO_TILE)
        {
            continue;
        }

        uint8_t ground = fallback;
        bool found = false;

        for (const uint8_t shown : textures)
        {
            if (shown == TEXTURE_CLIFF || shown == TEXTURE_WATER)
            {
                continue;
            }

            if (found)
            {
                const bool differs = shown != ground;
                ground = fallback;

                if (differs)
                {
                    break;
                }
            }

            found = true;
            ground = shown;
        }

        texture = ground;
        paintTile(map, it->x, it->y);
    }
}

void VertexTool::repairOverlays(EditorMap& map, std::list<POINT>& tiles)
{
    for (const POINT& at : tiles)
    {
        EditorTile* tile = map.getTile(at.x, at.y);

        // The original reads the overlay unchecked.
        if (tile == nullptr || tile->overlay == nullptr)
        {
            continue;
        }

        uint8_t heights[4];
        tile->getHeights(heights);
        int32_t kinds[4];
        std::copy(std::begin(tile->overlay->kinds), std::end(tile->overlay->kinds), kinds);
        const bool damaged = tile->overlay->damaged;

        if (tile->hasTexture(TEXTURE_WATER))
        {
            for (const int32_t kind : kinds)
            {
                if (kind != OVERLAY_ROUGH)
                {
                    delete tile->overlay;
                    tile->overlay = nullptr;
                }
            }
        }
        else if (tileLibrary->findOverlayTile(kinds, heights, damaged) == nullptr)
        {
            delete tile->overlay;
            tile->overlay = nullptr;
        }
        else
        {
            tile->setOverlay(kinds, damaged);
        }
    }
}

void VertexTool::repairMountain(EditorMap& map, std::list<POINT>& tiles)
{
    for (const POINT& at : tiles)
    {
        const EditorTile* tile = map.getTile(at.x, at.y);
        texture = TileLibrary::getDefaultTexture();

        if (std::all_of(std::begin(tile->elevations), std::end(tile->elevations),
                        [](int32_t elevation) { return elevation != 0; }))
        {
            continue;
        }

        uint8_t textures[4];
        shownTextures(*tile, textures);

        for (const uint8_t shown : textures)
        {
            if (shown != TEXTURE_MOUN)
            {
                texture = shown;
                break;
            }
        }

        paintTile(map, at.x, at.y);
    }
}

void VertexTool::repairConcCrop(EditorMap& map, std::list<VertexChange>& tiles)
{
    for (const VertexChange& at : tiles)
    {
        EditorTile* tile = map.getTile(at.col, at.row);
        const int32_t look = at.value & LOOK_INDEX_MASK;
        const auto lookTexture = static_cast<uint8_t>(at.value >> LOOK_TEXTURE_SHIFT);
        uint8_t heights[4];
        tile->getHeights(heights);

        if (!std::all_of(std::begin(heights), std::end(heights), [](uint8_t height) { return height == 0; }))
        {
            continue;
        }

        const uint8_t textures[4] = {lookTexture, lookTexture, lookTexture, lookTexture};
        const int32_t id = tileLibrary->pickTile(textures, heights, look);
        tileLibrary->releaseTile(id);

        // Original behaviour: the tile takes the pick even when there is none; its textures stay as they are.
        if (lookTexture == TEXTURE_CONC)
        {
            tile->tileId = id;
            std::fill(std::begin(tile->zoomArt), std::end(tile->zoomArt), nullptr);
            tile->concIndex = look;
            tile->cropIndex = -1;
        }
        else if (lookTexture == TEXTURE_CROP)
        {
            tile->tileId = id;
            std::fill(std::begin(tile->zoomArt), std::end(tile->zoomArt), nullptr);
            tile->cropIndex = look;
            tile->concIndex = -1;
        }
    }
}
