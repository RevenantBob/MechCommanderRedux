#include "stdafx.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MapOverlay.h"
#include "ObjectLibrary.h"
#include "TileLibrary.h"
#include "X87Extended.h"

// The map's file writers (0x004564d0, 0x00456770, 0x00456f60, 0x00457200): the terrain (.elv), the objects (.obj),
// the overlays' buildings (.bdg) and the tiles to preload (.pre).

namespace
{
    /// <summary>The files' signature.</summary>
    inline constexpr uint32_t MAP_FILE_SIGNATURE = 0xfeedface;
    /// <summary>The header before a file's block table: the signature and the table's end.</summary>
    inline constexpr uint32_t BLOCK_TABLE_START = 8;
    /// <summary>A block of the .elv: 20 x 20 vertices of 8 bytes.</summary>
    inline constexpr uint32_t ELV_BLOCK_BYTES = MAP_BLOCK_TILES * MAP_BLOCK_TILES * 8;
    /// <summary>The overlay number of no overlay in a .elv file.</summary>
    inline constexpr uint16_t ELV_NO_OVERLAY = 0x29;
    /// <summary>The game's tile size, which the objects' positions are written in.</summary>
    inline constexpr int32_t GAME_TILE_SIZE = 0x9e;
    /// <summary>The flags byte of an object record: set bits and the object's look (bit 0, destroyed).</summary>
    inline constexpr uint8_t OBJECT_RECORD_FLAGS = 0xa0;
    /// <summary>Where a block's offset in the .obj's table is.</summary>
    inline constexpr LONG OBJECT_TABLE_OFFSET = 8;
    /// <summary>The game's building type of each overlay kind, as the .bdg gives it (0x005028e8).</summary>
    inline constexpr int32_t OVERLAY_BUILDING_TYPES[OVERLAY_KINDS] = {6, 8, 0, 9, 5, 0, 0, 5, 0, 0, 7, 0};
    /// <summary>The tiles the .pre may name (the ids below it) and the most art they may come to.</summary>
    inline constexpr int32_t PRELOAD_TILES = 0xe7f;
    inline constexpr uint32_t PRELOAD_LIMIT = 0x1f4000;

    /// <summary>The building type of an overlay: its first corner's kind's; 0 when it has none.</summary>
    /// <remarks>0x00476380.</remarks>
    int32_t overlayBuildingType(const int32_t* kinds)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (kinds[i] != OVERLAY_NONE)
            {
                return OVERLAY_BUILDING_TYPES[kinds[i]];
            }
        }

        return 0;
    }

    /// <summary>The map block of an object's cell: its row, then column (60 cells to a block side).</summary>
    bool beforeByBlock(const EditorObject* a, const EditorObject* b)
    {
        constexpr int32_t BLOCK_CELLS = MAP_CELLS_PER_TILE * MAP_BLOCK_TILES;
        const int32_t rowA = a->cellRow / BLOCK_CELLS;
        const int32_t rowB = b->cellRow / BLOCK_CELLS;
        return rowA < rowB || (rowA == rowB && a->cellCol / BLOCK_CELLS < b->cellCol / BLOCK_CELLS);
    }
}

bool EditorMap::writeElevations(CArchive& archive)
{
    archive << MAP_FILE_SIGNATURE;
    const int32_t blocks = (maxRow + 1) / MAP_BLOCK_TILES;
    const int32_t blockCount = blocks * blocks;
    // The block table: each block's offset, the blocks following the table.
    uint32_t offset = static_cast<uint32_t>(blockCount) * 4 + BLOCK_TABLE_START;
    archive << offset;

    for (int32_t i = 0; i < blockCount; i++, offset += ELV_BLOCK_BYTES)
    {
        archive << offset;
    }

    // Original behaviour: a vertex past the tiles (the last column and row) writes the terrain type of the tile
    // written before it.
    uint8_t type = 0;

    for (int32_t blockRow = 0; blockRow < blocks; blockRow++)
    {
        for (int32_t blockCol = 0; blockCol < blocks; blockCol++)
        {
            for (int32_t r = 0; r < MAP_BLOCK_TILES; r++)
            {
                const int32_t row = blockRow * MAP_BLOCK_TILES + r;

                for (int32_t c = 0; c < MAP_BLOCK_TILES; c++)
                {
                    const int32_t col = blockCol * MAP_BLOCK_TILES + c;
                    const EditorTile* tile = getTile(col, row);

                    if (tile != nullptr)
                    {
                        type = static_cast<uint8_t>(TileLibrary::tileGroup(tile->tileId));
                    }

                    archive << static_cast<uint8_t>(vertexElevation(col, row) + 1) << type << static_cast<uint16_t>(0);
                    archive << static_cast<uint16_t>(tile != nullptr ? tile->tileId : 0);
                    archive << static_cast<uint16_t>(tile != nullptr && tile->overlay != nullptr ? tile->overlay->tileId
                                                                                                 : ELV_NO_OVERLAY);
                }
            }
        }
    }

    return true;
}

bool EditorMap::writeObjects(CArchive& archive, CFile& file)
{
    using X = X87Extended;

    // The objects go in map block order (a stable sort) and back in row and column order after.
    if (objects.size() >= 2)
    {
        objects.sort(beforeByBlock);
    }

    // The positions are written in the game's tile size.
    const int32_t savedWidth = tileWorldWidth;
    const int32_t savedHeight = tileWorldHeight;
    tileWorldWidth = GAME_TILE_SIZE;
    tileWorldHeight = GAME_TILE_SIZE;
    archive << MAP_FILE_SIGNATURE;
    const auto blocks = static_cast<uint32_t>(maxRow + 1) / MAP_BLOCK_TILES;
    archive << blocks * blocks * 4 + BLOCK_TABLE_START;
    // The table is written now and again once the blocks' offsets are known.
    std::vector<uint32_t> table(static_cast<size_t>(blocks) * blocks);
    archive.Write(table.data(), static_cast<UINT>(table.size() * sizeof(uint32_t)));
    // The cells' half size and size, in the game's units.
    const X half = X(GAME_TILE_SIZE) / X(MAP_CELLS_PER_TILE * 2);
    const float halfCell = half.toFloat();
    const float cell = (half + half).toFloat();
    auto next = objects.begin();
    size_t slot = 0;
    // Original behaviour: an object whose tile's vertex isn't on the map keeps the last vertex position found.
    int32_t vertexViewX = 0;
    int32_t vertexViewY = 0;

    for (int32_t blockRow = 0; blockRow < static_cast<int32_t>(blocks); blockRow++)
    {
        for (int32_t blockCol = 0; blockCol < static_cast<int32_t>(blocks); blockCol++)
        {
            archive.Flush();
            table[slot++] = static_cast<uint32_t>(file.GetPosition());

            for (; next != objects.end(); ++next)
            {
                const EditorObject& object = **next;
                const int32_t tileCol = object.cellCol / MAP_CELLS_PER_TILE;
                const int32_t tileRow = object.cellRow / MAP_CELLS_PER_TILE;
                const int32_t objectBlockCol = tileCol / MAP_BLOCK_TILES;
                const int32_t objectBlockRow = tileRow / MAP_BLOCK_TILES;
                // The cell's centre, and its view position on the terrain; the tile's vertex's view position.
                const auto x = static_cast<int32_t>((X(object.cellCol) * X(cell) + X(halfCell)).truncate());
                const auto y = static_cast<int32_t>((X(object.cellRow) * X(cell) + X(halfCell)).truncate());
                int32_t viewX = 0;
                int32_t viewY = 0;
                mapToView(x, y, terrainHeight(x, y), &viewX, &viewY);
                int32_t vertexX = 0;
                int32_t vertexY = 0;
                int32_t vertexZ = 0;

                if (getVertex(tileCol, tileRow, &vertexX, &vertexY, &vertexZ))
                {
                    mapToView(vertexX, vertexY, vertexZ, &vertexViewX, &vertexViewY);
                }

                if (objectBlockCol != blockCol || objectBlockRow != blockRow)
                {
                    break;
                }

                // The record: FIT id, offset from the vertex in the view, the tile in the block, the block, flags.
                const ObjectFrame& frame = objectLibrary->objectGroups[objectIdGroup(object.id)]
                                               .types[objectIdType(object.id)]
                                               .frames[objectIdFrame(object.id)];
                const int32_t tileInBlockCol = tileCol - objectBlockCol * MAP_BLOCK_TILES;
                const int32_t tileInBlockRow = tileRow - objectBlockRow * MAP_BLOCK_TILES;
                archive << static_cast<uint16_t>(frame.fitId);
                archive << static_cast<uint16_t>(viewX - vertexViewX);
                archive << static_cast<uint16_t>(viewY - vertexViewY);
                archive << static_cast<uint16_t>(tileInBlockCol + tileInBlockRow * MAP_BLOCK_TILES);
                archive << static_cast<uint16_t>(blocks * objectBlockRow + objectBlockCol);
                archive << static_cast<uint8_t>(objectIdSlot(object.id) | OBJECT_RECORD_FLAGS);
            }
        }
    }

    // Back in row and column order.
    std::list<EditorObject*> byBlock;
    byBlock.splice(byBlock.end(), objects);

    for (EditorObject* object : byBlock)
    {
        insertSorted(objects, object);
    }

    archive.Flush();
    file.Seek(OBJECT_TABLE_OFFSET, CFile::begin);
    archive.Write(table.data(), static_cast<UINT>(table.size() * sizeof(uint32_t)));
    tileWorldWidth = savedWidth;
    tileWorldHeight = savedHeight;
    return true;
}

bool EditorMap::writeBuildings(CArchive& archive, CFile& file)
{
    // The count goes first, written again once known.
    archive << static_cast<int32_t>(0);
    int32_t count = 0;
    const int32_t blocks = (maxRow + 1) / MAP_BLOCK_TILES;

    for (int32_t row = 0; row < maxCol; row++)
    {
        for (int32_t col = 0; col < maxRow; col++)
        {
            const MapOverlay* overlay = getTile(col, row)->overlay;

            if (overlay == nullptr)
            {
                continue;
            }

            if (!overlay->damaged)
            {
                // An overlay that can't be damaged makes no building.
                const uint8_t flat[4] = {};

                if (tileLibrary->findOverlayTile(overlay->kinds, flat, true) == nullptr)
                {
                    continue;
                }
            }

            archive << static_cast<int32_t>((row / MAP_BLOCK_TILES) * blocks + col / MAP_BLOCK_TILES);
            archive << static_cast<int32_t>(col % MAP_BLOCK_TILES + (row % MAP_BLOCK_TILES) * MAP_BLOCK_TILES);
            archive << static_cast<int32_t>(overlayBuildingType(overlay->kinds));
            archive << static_cast<int32_t>(overlay->damaged ? 1 : 0);
            count++;
        }
    }

    archive.Flush();
    file.Seek(0, CFile::begin);
    archive << count;
    return true;
}

bool EditorMap::writePreload(CFile& file)
{
    // The tiles in use, then as many of them as fit.
    std::list<std::pair<int32_t, uint32_t>> used;

    for (int32_t id = 0; id < PRELOAD_TILES; id++)
    {
        const TileEntry& entry = tileLibrary->tiles[static_cast<size_t>(id)];

        if (entry.refCount > 0)
        {
            used.emplace_back(id, entry.unpackedSize);
        }
    }

    int32_t total = 0;

    for (const auto& [id, size] : used)
    {
        total += static_cast<int32_t>(size);

        if (total >= static_cast<int32_t>(PRELOAD_LIMIT))
        {
            break;
        }

        file.Write(&id, sizeof(id));
    }

    return true;
}
