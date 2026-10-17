#include "stdafx.h"
#include "EditorAction.h"
#include "EditorMap.h"

namespace
{
    /// <summary>The recorded-tile bits are laid out for the largest map: this many tiles per row.</summary>
    inline constexpr int32_t RECORD_ROW = 0xf0;
}

EditorAction::EditorAction(const std::string& name) : name(name)
{
}

TileAction::TileAction(const std::string& name, EditorMap* map) : EditorAction(name), map(map)
{
}

bool TileAction::redo()
{
    for (TileRecord& record : records)
    {
        const EditorTile saved = record.tile;
        EditorTile* tile = map->getTile(record.col, record.row);

        // Original behaviour: a record outside the map is swapped with a null tile (a crash); the port skips it.
        if (tile == nullptr)
        {
            continue;
        }

        record.tile = *tile;
        *tile = saved;
    }

    return true;
}

bool TileAction::undo()
{
    // The tile swap itself (slot 2 jumps to 0x004128f0), not a derived action's redo.
    return TileAction::redo();
}

bool TileAction::isRecorded(int32_t col, int32_t row) const
{
    const int32_t bit = row * RECORD_ROW + col;
    return (recorded[bit >> 3] & (1u << (bit & 7))) != 0;
}

void TileAction::recordTile(int32_t col, int32_t row, const EditorTile& tile)
{
    const int32_t bit = row * RECORD_ROW + col;

    if (!isRecorded(col, row))
    {
        records.push_back({col, row, tile});
    }

    recorded[bit >> 3] |= static_cast<uint8_t>(1u << (bit & 7));
}

void TileAction::appendRecord(int32_t col, int32_t row, const EditorTile& tile)
{
    records.push_back({col, row, tile});
}

void TileAction::recordVertex(int32_t col, int32_t row)
{
    // The tiles up-left, up, left and at the vertex, as far as they exist.
    const int32_t cols[4] = {col - 1, col, col - 1, col};
    const int32_t rows[4] = {row - 1, row - 1, row, row};

    for (int32_t i = 0; i < 4; i++)
    {
        if (const EditorTile* tile = map->getTile(cols[i], rows[i]); tile != nullptr)
        {
            recordTile(cols[i], rows[i], *tile);
        }
    }
}
