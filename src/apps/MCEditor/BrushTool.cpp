#include "stdafx.h"
#include "BrushTool.h"
#include "EditorMap.h"
#include "TileLibrary.h"

namespace
{
    /// <summary>The brushes' cursor hot spot.</summary>
    inline constexpr int32_t BRUSH_HOT_X = 0x10;
    inline constexpr int32_t BRUSH_HOT_Y = 0x10;
    /// <summary>
    /// The string a texture paint's action name starts with; the editor has none, so the name is the texture's.
    /// </summary>
    inline constexpr UINT PAINT_ACTION_NAME = 0x8014;
    /// <summary>The first texture name string; the GTiles set's are <c>tileSetStringOffset</c> on.</summary>
    inline constexpr UINT TEXTURE_NAME_STRINGS = IDS_DIRT;
    /// <summary>The longest string the brushes load.</summary>
    inline constexpr int NAME_LENGTH = 0x100;
    /// <summary>An elevation <see cref="EditorTile::setElevations"/> leaves.</summary>
    inline constexpr int32_t KEEP = EditorTile::KEEP_ELEVATION;
    /// <summary>A texture <see cref="EditorTile::setTextures"/> leaves.</summary>
    inline constexpr uint8_t K = EditorTile::KEEP_TEXTURE;

    /// <summary>The tile's textures, water where a corner is at elevation 0.</summary>
    void shownTextures(const EditorTile& tile, uint8_t* textures)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            textures[i] = tile.elevations[i] == 0 ? static_cast<uint8_t>(TEXTURE_WATER) : tile.textures[i];
        }
    }
}

BrushTool::BrushTool(bool highlight, UINT bitmapId, int32_t hotX, int32_t hotY)
    : CursorTool(bitmapId, hotX, hotY), highlight(highlight)
{
}

BrushTool::~BrushTool()
{
    if (highlightedTile != nullptr)
    {
        highlightedTile->highlighted = false;
    }
}

EditorAction* BrushTool::end()
{
    TileAction* result = action;
    action = nullptr;

    if (result->records.empty())
    {
        delete result;
        result = nullptr;
    }

    return result;
}

bool BrushTool::apply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                      [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool BrushTool::canApply(EditorMap& map, int32_t x, int32_t y, [[maybe_unused]] ToolKeys keys)
{
    if (highlight)
    {
        int32_t col = 0;
        int32_t row = 0;

        if (map.viewToTile(x, y, &col, &row))
        {
            EditorTile* tile = map.getTile(col, row);
            map.clearHighlight();
            tile->highlighted = true;
            highlightedTile = tile;
        }
    }

    return true;
}

bool BrushTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    return BrushTool::canApply(map, x, y, keys);
}

bool BrushTool::matchNeighbours(EditorMap& map, int32_t col, int32_t row, FailedTiles& failed)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return true;
    }

    uint8_t t[4];
    shownTextures(*tile, t);
    const int32_t keep[4] = {KEEP, KEEP, KEEP, KEEP};
    // Each neighbour's corners on the tile's: above, up-left, left, down-left, below, down-right, right, up-right.
    const uint8_t above[4] = {K, K, t[0], t[1]};
    paintTerrain(map, col, row - 1, above, keep, failed);
    const uint8_t upLeft[4] = {K, K, K, t[0]};
    paintTerrain(map, col - 1, row - 1, upLeft, keep, failed);
    const uint8_t left[4] = {K, t[0], K, t[2]};
    paintTerrain(map, col - 1, row, left, keep, failed);
    const uint8_t downLeft[4] = {K, t[2], K, K};
    paintTerrain(map, col - 1, row + 1, downLeft, keep, failed);
    const uint8_t below[4] = {t[2], t[3], K, K};
    paintTerrain(map, col, row + 1, below, keep, failed);
    const uint8_t downRight[4] = {t[3], K, K, K};
    paintTerrain(map, col + 1, row + 1, downRight, keep, failed);
    const uint8_t right[4] = {t[1], K, t[3], K};
    paintTerrain(map, col + 1, row, right, keep, failed);
    const uint8_t upRight[4] = {K, K, t[1], K};
    paintTerrain(map, col + 1, row - 1, upRight, keep, failed);
    return true;
}

void BrushTool::paintTerrain(EditorMap& map, int32_t col, int32_t row, const uint8_t* textures,
                             const int32_t* elevations, FailedTiles& failed)
{
    EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return;
    }

    TileAction::TileRecord before{col, row, *tile};
    bool changed = false;

    if (!tile->setTerrain(textures, elevations, &changed))
    {
        if (action != nullptr)
        {
            action->recordTile(col, row, before.tile);
        }

        failed.push_back(before);
    }
    else if (changed && action != nullptr)
    {
        action->recordTile(col, row, before.tile);
    }
}

bool BrushTool::startAction(EditorMap& map, UINT nameId)
{
    delete action;
    char name[NAME_LENGTH];

    if (LoadStringA(GetModuleHandleA(nullptr), nameId, name, NAME_LENGTH) == 0)
    {
        name[0] = '\0';
    }

    action = new TileAction(name, &map);
    return true;
}

TextureTool::TextureTool(uint8_t texture, UINT bitmapId)
    : BrushTool(true, bitmapId, BRUSH_HOT_X, BRUSH_HOT_Y), texture(texture)
{
}

bool TextureTool::begin(EditorMap& map)
{
    startAction(map, PAINT_ACTION_NAME);
    const char* name = nullptr;

    if (texture == TEXTURE_CLIFF)
    {
        // Original behaviour: cliff has no pure set, so its name is loaded into the library's own name.
        if (tileLibrary->name[0] == '\0')
        {
            LoadStringA(GetModuleHandleA(nullptr), texture + TEXTURE_NAME_STRINGS + tileSetStringOffset,
                        tileLibrary->name, NAME_LENGTH);
        }

        name = tileLibrary->name;
    }
    else
    {
        name = static_cast<PureTileSet*>(tileLibrary->sets[texture][texture])->name;
    }

    action->name += name;
    return true;
}

EditorAction* TextureTool::end()
{
    TileAction* result = action;

    if (result != nullptr && result->records.empty())
    {
        delete result;
        result = nullptr;
    }

    action = nullptr;
    return result;
}

bool TextureTool::apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    BrushTool::apply(map, x, y, keys);
    int32_t col = 0;
    int32_t row = 0;

    if (map.viewToTile(x, y, &col, &row))
    {
        return paintTile(map, col, row);
    }

    return true;
}

bool TextureTool::applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    const uint8_t saved = texture;
    texture = TileLibrary::getDefaultTexture();
    const bool result = apply(map, x, y, keys);
    texture = saved;
    return result;
}

bool TextureTool::canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    int32_t col = 0;
    int32_t row = 0;

    if (!map.viewToTile(x, y, &col, &row))
    {
        return false;
    }

    if (texture == TEXTURE_MOUN)
    {
        if (mountainFits(map, col, row, true))
        {
            return BrushTool::canApply(map, x, y, keys);
        }

        return false;
    }

    // Original behaviour: other textures don't light the tile up.
    const EditorTile* tile = map.getTile(col, row);
    return std::any_of(std::begin(tile->elevations), std::end(tile->elevations),
                       [](int32_t elevation) { return elevation != 0; });
}

bool TextureTool::canApplyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys)
{
    const uint8_t saved = texture;
    texture = TileLibrary::getDefaultTexture();
    const bool result = canApply(map, x, y, keys);
    texture = saved;
    return result;
}

bool TextureTool::hasTarget(EditorMap& map)
{
    for (int32_t col = 0; col < map.maxRow; col++)
    {
        for (int32_t row = 0; row < map.maxCol; row++)
        {
            const EditorTile* tile = map.getTile(col, row);

            if (!tile->selected)
            {
                continue;
            }

            if (texture == TEXTURE_MOUN)
            {
                if (!mountainFits(map, col, row, true))
                {
                    return false;
                }
            }
            else if (std::any_of(std::begin(tile->elevations), std::end(tile->elevations),
                                 [](int32_t elevation) { return elevation != 0; }))
            {
                return true;
            }
        }
    }

    return texture == TEXTURE_MOUN;
}

bool TextureTool::applyToSelection(EditorMap& map, [[maybe_unused]] int32_t step)
{
    bool result = true;

    if (action != nullptr)
    {
        action->map = &map;
    }

    for (int32_t row = 0; row < map.maxCol; row++)
    {
        for (int32_t col = 0; col < map.maxRow; col++)
        {
            if (map.getTile(col, row)->selected)
            {
                const bool painted = paintTile(map, col, row);
                result = painted && result;
            }
        }
    }

    return result;
}

bool TextureTool::paintTile(EditorMap& map, int32_t col, int32_t row)
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
    int32_t elevations[4];
    std::copy(std::begin(tile->elevations), std::end(tile->elevations), elevations);

    if (tile->concIndex != -1 || tile->cropIndex != -1)
    {
        const EditorTile before = *tile;

        if (action != nullptr)
        {
            action->recordTile(col, row, before);
        }

        tile->setTextures(textures);
    }

    bool same = true;

    for (int32_t i = 0; i < 4; i++)
    {
        if (textures[i] != texture && elevations[i] != 0)
        {
            same = false;
        }

        if (elevations[i] != 0)
        {
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

bool TextureTool::mountainFits(EditorMap& map, int32_t col, int32_t row, bool checkAround)
{
    const EditorTile* tile = map.getTile(col, row);

    if (tile == nullptr)
    {
        return true;
    }

    bool low = false;

    for (const int32_t elevation : tile->elevations)
    {
        if (elevation == 0)
        {
            return false;
        }

        if (elevation == 1)
        {
            low = true;
        }
    }

    if (checkAround && low)
    {
        for (int32_t aroundCol = col - 1; aroundCol < col + 2; aroundCol++)
        {
            for (int32_t aroundRow = row - 1; aroundRow < row + 2; aroundRow++)
            {
                if (!mountainFits(map, aroundCol, aroundRow, false))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool TextureTool::fixNeighbours(EditorMap& map, int32_t col, int32_t row)
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

        uint8_t resolved[4];

        if (tileLibrary->resolveTextures(texture, textures, heights, resolved))
        {
            bool changed = false;
            tile->setTextures(resolved, &changed);

            if (action != nullptr)
            {
                action->recordTile(record->col, record->row, record->tile);
            }

            const bool matched = matchNeighbours(map, record->col, record->row, failed);
            result = matched && result;
        }
        else
        {
            const bool painted = paintTile(map, record->col, record->row);
            result = painted && result;
        }
    }

    return result;
}
