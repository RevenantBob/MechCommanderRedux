#include "stdafx.h"
#include "GroundTools.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "PlaceObjectTool.h"
#include "TileLibrary.h"

namespace
{
    /// <summary>The ground tools' cursor (the palette's paint brush).</summary>
    inline constexpr UINT GROUND_CURSOR = IDB_PAINT_CURSOR;
    /// <summary>The cursor's hot spot.</summary>
    inline constexpr int32_t GROUND_HOT = 0x10;
    /// <summary>The strings the concrete and crop actions start with (the textures' names), replaced by the set's.</summary>
    inline constexpr UINT CONC_ACTION_NAME = IDS_CONCRETE;
    inline constexpr UINT CROP_ACTION_NAME = IDS_CROPS;
    /// <summary>A texture <see cref="EditorTile::setTextures"/> leaves.</summary>
    inline constexpr uint8_t K = EditorTile::KEEP_TEXTURE;
    /// <summary>An elevation <see cref="EditorTile::setElevations"/> leaves.</summary>
    inline constexpr int32_t KEEP = EditorTile::KEEP_ELEVATION;
    /// <summary>
    /// The tiles beside a cliff tile its edges face, by which of its corners are 2 up (bit 3 corner 0 ... bit 0 corner
    /// 3): bit 2 above, 0 right, 1 left, 3 below; all of them for no edge (0x004faa3c).
    /// </summary>
    inline constexpr int32_t CLIFF_FACES[16] = {-1, 9, 10, 3, 5, 12, -1, 6, 6, -1, 12, 5, 3, 10, 9, -1};
    inline constexpr int32_t FACE_ABOVE = 4;
    inline constexpr int32_t FACE_LEFT = 2;
    inline constexpr int32_t FACE_BELOW = 8;
    inline constexpr int32_t FACE_RIGHT = 1;

    /// <summary>The tile's textures, water where a corner is at elevation 0.</summary>
    void shownTextures(const EditorTile& tile, uint8_t* textures)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            textures[i] = tile.elevations[i] == 0 ? static_cast<uint8_t>(TEXTURE_WATER) : tile.textures[i];
        }
    }

    /// <summary>The vertex at corner <paramref name="corner"/> of tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    /// <remarks>0x00413280.</remarks>
    POINT cornerVertex(int32_t col, int32_t row, int32_t corner)
    {
        return {col + (corner & 1), row + (corner >> 1)};
    }

    /// <summary>Which corners of <paramref name="heights"/> are 2 up, corner 0 in bit 3.</summary>
    int32_t highCorners(const uint8_t* heights)
    {
        int32_t mask = 0;

        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] == 2)
            {
                mask |= 1 << (3 - i);
            }
        }

        return mask;
    }

    /// <summary>Moves <paramref name="source"/>'s records into <paramref name="target"/> and deletes it.</summary>
    void mergeRecords(EditorAction* source, EditorAction* target)
    {
        for (const TileAction::TileRecord& record : static_cast<TileAction*>(source)->records)
        {
            static_cast<TileAction*>(target)->recordTile(record.col, record.row, record.tile);
        }

        delete source;
    }
}

void abandonUse(EditorTool& tool, bool release)
{
    if (EditorAction* action = tool.end(); action != nullptr)
    {
        action->undo();
        delete action;
    }

    if (!release)
    {
        return;
    }

    if (auto* frame = static_cast<CFrameWnd*>(AfxGetMainWnd()); frame != nullptr)
    {
        if (CView* view = frame->GetActiveView(); view != nullptr)
        {
            ::SendMessage(view->m_hWnd, WM_LBUTTONUP, 0, 0);
        }
    }
}

WaterTool::WaterTool() : TextureTool(TEXTURE_WATER, GROUND_CURSOR), vertex(1, TEXTURE_NONE, true, 0)
{
}

bool WaterTool::begin(EditorMap& map)
{
    waterLevel = -1;
    vertex.begin(map);
    return TextureTool::begin(map);
}

EditorAction* WaterTool::end()
{
    EditorAction* painted = TextureTool::end();
    EditorAction* height = vertex.end();

    if (painted == nullptr && height == nullptr)
    {
        return nullptr;
    }

    if (painted != nullptr)
    {
        if (height == nullptr)
        {
            return painted;
        }

        mergeRecords(painted, height);
    }

    action = nullptr;

    if (height != nullptr && static_cast<TileAction*>(height)->records.empty())
    {
        delete height;
        height = nullptr;
    }

    return height;
}

bool WaterTool::apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return true;
    }

    if (waterLevel < 0)
    {
        const EditorTile* tile = map.getTile(col, row);
        const int32_t sum = tile->elevations[3] + tile->elevations[2] + tile->elevations[1] + tile->elevations[0];
        waterLevel = static_cast<int32_t>(std::floor(static_cast<double>(sum) * 0.25 + 0.5));
    }

    for (int32_t i = -1; i < 3; i++)
    {
        for (int32_t j = -1; j < 3; j++)
        {
            map.setVertexSelected(col + i, row + j, true);
        }
    }

    vertex.setSelectedElevation(map, waterLevel);
    map.selectedVertices.clear();
    const bool result = TextureTool::apply(map, x, y, keys);

    if (vertex.removeStrandedObjects(map) == -1)
    {
        abandonUse(*this, true);
        return false;
    }

    return result;
}

bool WaterTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    BrushTool::canApply(map, x, y, keys);
    int32_t col = 0;
    int32_t row = 0;
    // The original reads whatever the conversion leaves off the map.
    map.viewToTile(x, y, &col, &row);
    return tileClear(map, col, row);
}

bool WaterTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected && !tileClear(map, col, row))
            {
                return false;
            }
        }
    }

    return true;
}

bool WaterTool::applyToSelection(EditorMap& map, int32_t step)
{
    float sum = 0.0f;
    float count = 0.0f;
    std::list<POINT> added;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (!tile->selected)
            {
                continue;
            }

            for (const int32_t elevation : tile->elevations)
            {
                sum = sum + static_cast<float>(elevation);
                count = count + 1.0f;
            }

            for (int32_t vertexCol = col - 1; vertexCol < col + 3; vertexCol++)
            {
                for (int32_t vertexRow = row - 1; vertexRow < row + 3; vertexRow++)
                {
                    const bool selected =
                        std::any_of(map.selectedVertices.begin(), map.selectedVertices.end(),
                                    [=](const POINT& v) { return v.x == vertexCol && v.y == vertexRow; });

                    if (!selected)
                    {
                        map.setVertexSelected(vertexCol, vertexRow, true);
                        added.push_back({vertexCol, vertexRow});
                    }
                }
            }
        }
    }

    // Exact enough in any precision: the average's distance from a half is far above the rounding.
    waterLevel = static_cast<int32_t>(std::floor(static_cast<double>(sum) / count + 0.5));
    vertex.setSelectedElevation(map, waterLevel);

    for (const POINT& vertexAt : added)
    {
        map.setVertexSelected(vertexAt.x, vertexAt.y, false);
    }

    const bool result = TextureTool::applyToSelection(map, step);

    if (vertex.removeStrandedObjects(map) == -1)
    {
        abandonUse(*this, false);
        return false;
    }

    return result;
}

void WaterTool::paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures,
                             const int32_t* elevations, FailedTiles& failed)
{
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return;
    }

    uint8_t heights[4];
    tile->getHeights(heights);
    // The original changes the caller's textures.
    uint8_t wanted[4];
    std::copy(textures, textures + 4, wanted);

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] == 1 && wanted[i] == texture)
        {
            wanted[i] = K;
        }
    }

    TileAction::TileRecord record{col, row, *tile};
    bool changed = false;

    if (!tile->setTerrain(wanted, elevations, &changed))
    {
        if (action != nullptr)
        {
            action->recordTile(col, row, record.tile);
        }

        record.tile = *tile;
        failed.push_back(record);
    }
    else if (changed)
    {
        if (action != nullptr)
        {
            action->recordTile(col, row, record.tile);
        }

        if (tile->overlay != nullptr)
        {
            for (const int32_t kind : tile->overlay->kinds)
            {
                if (kind != OVERLAY_ROUGH)
                {
                    delete tile->overlay;
                    tile->overlay = nullptr;
                    break;
                }
            }
        }
    }
}

bool WaterTool::paintTile(EditorMap& map, int32_t col, int32_t row)
{
    if (col >= map.maxRow || row >= map.maxCol)
    {
        return false;
    }

    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    uint8_t textures[4];
    shownTextures(*tile, textures);
    uint8_t heights[4];
    tile->getHeights(heights);
    bool same = true;

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] == 0 && textures[i] != texture)
        {
            same = false;
            textures[i] = texture;
        }
    }

    if (same)
    {
        return true;
    }

    const EditorTile before = *tile;

    if (action != nullptr)
    {
        action->recordTile(col, row, before);
    }

    tile->setTextures(textures);
    return fixNeighbours(map, col, row);
}

bool WaterTool::tileClear(EditorMap& map, int32_t col, int32_t row)
{
    for (int32_t i = 0; i < MAP_CELLS_PER_TILE; i++)
    {
        for (int32_t j = 0; j < MAP_CELLS_PER_TILE; j++)
        {
            if (map.objectAt(col * MAP_CELLS_PER_TILE + i, row * MAP_CELLS_PER_TILE + j) != nullptr)
            {
                return false;
            }
        }
    }

    return true;
}

CliffTool::CliffTool() : TextureTool(TEXTURE_CLIFF, GROUND_CURSOR), vertex(1, TEXTURE_CLIFF, true, 0)
{
}

bool CliffTool::begin(EditorMap& map)
{
    ground = TEXTURE_NONE;
    const bool shaping = vertex.begin(map);
    const bool painting = TextureTool::begin(map);
    return painting && shaping;
}

EditorAction* CliffTool::end()
{
    EditorAction* painted = TextureTool::end();
    EditorAction* height = vertex.end();

    if (painted != nullptr)
    {
        if (height == nullptr)
        {
            return painted;
        }

        mergeRecords(painted, height);
    }

    if (height == nullptr)
    {
        return nullptr;
    }

    if (static_cast<TileAction*>(height)->records.empty())
    {
        delete height;
        return nullptr;
    }

    return height;
}

bool CliffTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row) || !raiseForCliff(map, col, row))
    {
        return false;
    }

    const bool result = paintTile(map, col, row);

    if (vertex.removeStrandedObjects(map) != -1)
    {
        return result;
    }

    abandonUse(*this, true);
    return false;
}

bool CliffTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return true;
    }

    const EditorTile* tile = map.getTile(col, row);
    const bool dry = tile == nullptr || !tile->hasTexture(TEXTURE_WATER);
    BrushTool::canApply(map, x, y, keys);
    return dry;
}

bool CliffTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (tile->selected && tile->hasTexture(TEXTURE_WATER))
            {
                return false;
            }
        }
    }

    return true;
}

bool CliffTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    action->map = &map;
    bool result = true;

    for (int32_t row = 0; row < map.maxCol; row++)
    {
        for (int32_t col = 0; col < map.maxRow; col++)
        {
            if (!map.getTile(col, row)->selected)
            {
                continue;
            }

            if (!raiseForCliff(map, col, row))
            {
                return false;
            }

            const bool painted = paintTile(map, col, row);
            result = painted && result;
        }
    }

    if (vertex.removeStrandedObjects(map) == -1)
    {
        abandonUse(*this, false);
        return false;
    }

    return result;
}

bool CliffTool::matchNeighbours(EditorMap& map, int32_t col, int32_t row, FailedTiles& failed)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return true;
    }

    uint8_t t[4];
    shownTextures(*tile, t);
    uint8_t heights[4];
    tile->getHeights(heights);
    const int32_t keep[4] = {KEEP, KEEP, KEEP, KEEP};
    const int32_t faces = CLIFF_FACES[highCorners(heights)];

    if ((faces & FACE_ABOVE) != 0)
    {
        const uint8_t above[4] = {K, K, t[0], t[1]};
        raiseForCliff(map, col, row - 1);
        paintTerrain(map, col, row - 1, above, keep, failed);
    }

    if ((faces & FACE_LEFT) != 0)
    {
        const uint8_t left[4] = {K, t[0], K, t[2]};
        raiseForCliff(map, col - 1, row);
        paintTerrain(map, col - 1, row, left, keep, failed);
    }

    if ((faces & FACE_BELOW) != 0)
    {
        const uint8_t below[4] = {t[2], t[3], K, K};
        raiseForCliff(map, col, row + 1);
        paintTerrain(map, col, row + 1, below, keep, failed);
    }

    if ((faces & FACE_RIGHT) != 0)
    {
        const uint8_t right[4] = {t[1], K, t[3], K};
        raiseForCliff(map, col + 1, row);
        paintTerrain(map, col + 1, row, right, keep, failed);
    }

    return true;
}

void CliffTool::paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures,
                             const int32_t* elevations, FailedTiles& failed)
{
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return;
    }

    TileAction::TileRecord record{col, row, *tile};
    bool changed = false;

    if (!tile->setTerrain(textures, elevations, &changed))
    {
        failed.push_back(record);
        return;
    }

    if (action != nullptr)
    {
        action->recordTile(col, row, record.tile);
    }

    if (changed)
    {
        matchNeighbours(map, col, row, failed);
    }
}

bool CliffTool::paintTile(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);
    uint8_t textures[4];

    if (ground == TEXTURE_NONE && tile != nullptr)
    {
        // The ground is the more common of dirt and grass on the first tile, the default on a tie.
        shownTextures(*tile, textures);
        const auto dirt = std::count(std::begin(textures), std::end(textures), TEXTURE_DIRT);
        const auto grass = std::count(std::begin(textures), std::end(textures), TEXTURE_GRASS);

        if (dirt == grass)
        {
            ground = TileLibrary::getDefaultTexture();
        }
        else
        {
            ground = dirt > grass ? TEXTURE_DIRT : TEXTURE_GRASS;
        }
    }

    if (col >= map.maxRow || row >= map.maxCol)
    {
        return true;
    }

    if (tile == nullptr)
    {
        return false;
    }

    shownTextures(*tile, textures);
    uint8_t heights[4];
    tile->getHeights(heights);
    bool same = true;

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] == highHeight)
        {
            if (textures[i] != texture)
            {
                same = false;
                textures[i] = texture;
            }
        }
        else if (heights[i] == lowHeight && textures[i] != ground)
        {
            textures[i] = ground;
            same = false;
            spreadGround(map, col, row, i);
        }
    }

    if (same)
    {
        return true;
    }

    const EditorTile before = *tile;

    if (action != nullptr)
    {
        action->recordTile(col, row, before);
    }

    tile->setTextures(textures);
    return fixCliffNeighbours(map, col, row);
}

bool CliffTool::raiseForCliff(EditorMap& map, int32_t col, int32_t row)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    if (hasCliffShape(map, col, row))
    {
        return true;
    }

    uint8_t heights[4];
    tile->getHeights(heights);
    const bool hasHigh = std::find(std::begin(heights), std::end(heights), 2) != std::end(heights);

    if (!hasHigh)
    {
        int32_t raised = 0;

        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] == 1)
            {
                raised++;
                const POINT at = cornerVertex(col, row, i);
                vertex.moveVertex(map, at.x, at.y, 1);
            }
        }

        if (raised == 0)
        {
            const POINT at = cornerVertex(col, row, 0);
            vertex.moveVertex(map, at.x, at.y, 2);
        }
    }
    else
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] == 1)
            {
                const POINT at = cornerVertex(col, row, i);
                vertex.moveVertex(map, at.x, at.y, 1);
            }
        }
    }

    tile->getHeights(heights);

    if (heights[0] == heights[3] && heights[1] == heights[2])
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] == 0)
            {
                const POINT at = cornerVertex(col, row, i);
                vertex.moveVertex(map, at.x, at.y, 2);
                break;
            }
        }
    }

    return true;
}

bool CliffTool::hasCliffShape(EditorMap& map, int32_t col, int32_t row)
{
    if (map.getTile(col, row) == nullptr)
    {
        return true;
    }

    std::list<POINT> pending;

    if (!checkCliffShape(map, col, row, pending, true))
    {
        return false;
    }

    // From the back; a check adds its tiles at the front, which the walk then reaches.
    for (auto at = pending.end(); at != pending.begin();)
    {
        --at;
        const POINT tile = *at;

        if ((tile.x != col || tile.y != row) && !checkCliffShape(map, tile.x, tile.y, pending, false))
        {
            return false;
        }
    }

    return true;
}

bool CliffTool::checkCliffShape(EditorMap& map, int32_t col, int32_t row, std::list<POINT>& pending, bool first)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return true;
    }

    uint8_t h[4];
    tile->getHeights(h);

    if (std::find(std::begin(h), std::end(h), 1) != std::end(h))
    {
        return false;
    }

    // Flat, or high on opposite corners.
    if (h[0] == h[3] && h[1] == h[2])
    {
        return false;
    }

    // A straight edge leads on by itself; the first tile and corner shapes add the tiles they face.
    const bool straight = (h[0] == h[1] && h[2] == h[3]) || (h[0] == h[2] && h[1] == h[3]);

    if (first || !straight)
    {
        const int32_t faces = CLIFF_FACES[highCorners(h)];

        if ((faces & FACE_ABOVE) != 0)
        {
            VertexTool::addTile(pending, col, row - 1);
        }

        if ((faces & FACE_RIGHT) != 0)
        {
            VertexTool::addTile(pending, col + 1, row);
        }

        if ((faces & FACE_LEFT) != 0)
        {
            VertexTool::addTile(pending, col - 1, row);
        }

        if ((faces & FACE_BELOW) != 0)
        {
            VertexTool::addTile(pending, col, row + 1);
        }

        if (faces == -1)
        {
            return false;
        }
    }

    return true;
}

bool CliffTool::spreadGround(EditorMap& map, int32_t col, int32_t row, int32_t corner)
{
    EditorTile* tile = map.getTile(col, row);

    // The original reads a tile off the map.
    if (tile == nullptr)
    {
        return true;
    }

    uint8_t textures[4];
    shownTextures(*tile, textures);
    const uint8_t old = textures[corner];
    uint8_t heights[4];
    tile->getHeights(heights);
    uint8_t wanted[4] = {K, K, K, K};
    wanted[corner] = ground;

    if (action != nullptr)
    {
        action->recordTile(col, row, *tile);
    }

    bool changed = false;
    bool follow = changed;

    if (tile->setTextures(wanted, &changed))
    {
        follow = changed;
    }
    else
    {
        for (int32_t i = corner; i < 4; i++)
        {
            // Original behaviour (EB-078): the test reads byte i of the corner number instead of corner i's texture,
            // which is 0 (dirt) for every corner after the first.
            const uint8_t compared = 0;

            if (heights[i] != 2 && i != corner && compared != TEXTURE_CLIFF && compared != ground)
            {
                spreadGround(map, col, row, i);
            }
        }

        follow = old != ground || changed;
    }

    if (!follow)
    {
        return true;
    }

    // The same vertex on the tiles sharing it.
    switch (corner)
    {
        case 0:
        {
            spreadGround(map, col - 1, row, 1);
            spreadGround(map, col, row - 1, 2);
            spreadGround(map, col - 1, row - 1, 3);
            break;
        }
        case 1:
        {
            spreadGround(map, col + 1, row, 0);
            spreadGround(map, col + 1, row - 1, 2);
            spreadGround(map, col, row - 1, 3);
            break;
        }
        case 2:
        {
            spreadGround(map, col, row + 1, 0);
            spreadGround(map, col - 1, row + 1, 1);
            spreadGround(map, col - 1, row, 3);
            break;
        }
        case 3:
        {
            spreadGround(map, col + 1, row + 1, 0);
            spreadGround(map, col, row + 1, 1);
            spreadGround(map, col + 1, row, 2);
            break;
        }
        default:
            break;
    }

    return true;
}

bool CliffTool::fixCliffNeighbours(EditorMap& map, int32_t col, int32_t row)
{
    FailedTiles failed;
    bool result = matchNeighbours(map, col, row, failed);

    // matchNeighbours adds to the list while it is walked; the new tiles are walked too.
    for (auto record = failed.begin(); record != failed.end(); ++record)
    {
        EditorTile* tile = map.getTile(record->col, record->row);
        uint8_t textures[4];
        shownTextures(*tile, textures);
        uint8_t heights[4];
        tile->getHeights(heights);
        const int32_t id = tileLibrary->pickTile(textures, heights, -1);
        tileLibrary->releaseTile(id);

        if (id != NO_TILE)
        {
            if (action != nullptr)
            {
                action->recordTile(record->col, record->row, record->tile);
            }

            tile->setTextures(textures);
            continue;
        }

        for (int32_t i = 0; i < 4; i++)
        {
            if (heights[i] == 0 && textures[i] != ground)
            {
                spreadGround(map, record->col, record->row, i);
            }

            textures[i] = heights[i] == 2 ? static_cast<uint8_t>(TEXTURE_CLIFF) : ground;
        }

        bool changed = false;
        tile->setTextures(textures, &changed);

        if (action != nullptr)
        {
            action->recordTile(record->col, record->row, record->tile);
        }

        const bool matched = matchNeighbours(map, record->col, record->row, failed);
        result = matched && result;
    }

    return result;
}

ConcreteTool::ConcreteTool(uint8_t texture, int32_t look)
    : BrushTool(true, GROUND_CURSOR, GROUND_HOT, GROUND_HOT)
    , texture(texture)
    , vertex(1, TEXTURE_NONE, true, 0)
    , look(look - 1)
{
}

bool ConcreteTool::begin(EditorMap& map)
{
    vertex.begin(map);
    const bool result = startAction(map, texture != TEXTURE_CONC ? CROP_ACTION_NAME : CONC_ACTION_NAME);

    if (action == nullptr)
    {
        return result;
    }

    // The name is replaced by the tile set's.
    action->name = static_cast<PureTileSet*>(tileLibrary->sets[texture][texture])->name;
    return result;
}

EditorAction* ConcreteTool::end()
{
    EditorAction* height = vertex.end();
    EditorAction* painted = BrushTool::end();

    if (painted != nullptr)
    {
        if (height == nullptr)
        {
            return painted;
        }

        mergeRecords(painted, height);
    }

    if (height == nullptr)
    {
        return nullptr;
    }

    if (static_cast<TileAction*>(height)->records.empty())
    {
        delete height;
        return nullptr;
    }

    return height;
}

bool ConcreteTool::apply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (map.viewToTile(x, y, &col, &row))
    {
        return layLook(map, col, row);
    }

    return true;
}

bool ConcreteTool::applyAgain(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return true;
    }

    EditorTile* tile = map.getTile(col, row);
    const TileAction::TileRecord record{col, row, *tile};
    int32_t* index = nullptr;

    if (texture == TEXTURE_CONC)
    {
        index = &tile->concIndex;
    }
    else if (texture == TEXTURE_CROP)
    {
        index = &tile->cropIndex;
    }

    if (index == nullptr || *index == -1)
    {
        return true;
    }

    *index = -1;
    uint8_t heights[4];
    tile->getHeights(heights);
    const int32_t id = tileLibrary->pickTile(tile->textures, heights, -1);
    tile->tileId = id;
    std::fill(std::begin(tile->zoomArt), std::end(tile->zoomArt), nullptr);
    tileLibrary->releaseTile(id);

    if (action != nullptr)
    {
        action->recordTile(col, row, record.tile);
    }

    return true;
}

bool ConcreteTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return true;
    }

    if (!canLay(map, col, row))
    {
        return false;
    }

    return BrushTool::canApply(map, x, y, keys);
}

bool ConcreteTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x,
                                 [[maybe_unused]] int32_t y, [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool ConcreteTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected && !canLay(map, col, row))
            {
                return false;
            }
        }
    }

    return true;
}

bool ConcreteTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    int32_t sum = 0;
    int32_t count = 0;

    for (const POINT& at : map.selectedVertices)
    {
        sum += map.vertexElevation(at.x, at.y);
        count++;
    }

    if (count != 0)
    {
        sum /= count;
    }

    vertex.setSelectedElevation(map, sum);
    bool result = true;

    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool laid = layLook(map, col, row);
                result = laid && result;
            }
        }
    }

    return result;
}

bool ConcreteTool::layLook(EditorMap& map, int32_t col, int32_t row)
{
    EditorTile* tile = map.getTile(col, row);
    uint8_t heights[4];
    tile->getHeights(heights);
    const uint8_t lookTexture =
        texture == TEXTURE_CROP ? static_cast<uint8_t>(TEXTURE_CROP) : static_cast<uint8_t>(TEXTURE_CONC);
    uint8_t textures[4];

    for (int32_t i = 0; i < 4; i++)
    {
        textures[i] = lookTexture;

        // Original behaviour: the tile is flattened once for each corner that was up.
        if (heights[i] != 0)
        {
            vertex.flattenTile(map, col, row);
        }

        heights[i] = 0;
    }

    const int32_t id = tileLibrary->pickTile(textures, heights, look);
    tileLibrary->releaseTile(id);

    if (id == NO_TILE)
    {
        return false;
    }

    if (action != nullptr)
    {
        action->recordTile(col, row, *tile);
    }

    tile->tileId = id;
    std::fill(std::begin(tile->zoomArt), std::end(tile->zoomArt), nullptr);

    if (lookTexture == TEXTURE_CROP)
    {
        tile->cropIndex = look;
        tile->concIndex = -1;
    }
    else
    {
        tile->concIndex = look;
        tile->cropIndex = -1;
    }

    return true;
}

bool ConcreteTool::canLay(EditorMap& map, int32_t col, int32_t row)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return false;
    }

    if (std::find(std::begin(tile->elevations), std::end(tile->elevations), 0) != std::end(tile->elevations) ||
        tile->hasTexture(TEXTURE_WATER))
    {
        return false;
    }

    for (int32_t i = 0; i < MAP_CELLS_PER_TILE; i++)
    {
        for (int32_t j = 0; j < MAP_CELLS_PER_TILE; j++)
        {
            const EditorObject* object = map.objectAt(col * MAP_CELLS_PER_TILE + i, row * MAP_CELLS_PER_TILE + j);

            if (object != nullptr && objectIdClass(object->id) == OBJECT_CLASS_SPRAY_CAN)
            {
                return false;
            }
        }
    }

    if (tile->overlay != nullptr)
    {
        for (const int32_t kind : tile->overlay->kinds)
        {
            if (kind == OVERLAY_FOREST)
            {
                return false;
            }
        }
    }

    return true;
}
