#pragma once

#include <list>
#include <string>

#include "EditorTile.h"

class EditorMap;

// The undo records of the editor's tools (virtual tables 0x004fa9b8, 0x004fa9a8 and their derived ones). Their source
// file name wasn't kept; the names are the port's.

/// <summary>A change a tool made, which the undo list can undo and redo.</summary>
/// <remarks>0x14 bytes with the virtual table (0x004fa9b8); the name is a VC6 std::string (+4).</remarks>
class EditorAction
{
public:
    /// <summary>An action named <paramref name="name"/> (the Undo and Redo menu items show it).</summary>
    /// <remarks>0x00402270.</remarks>
    explicit EditorAction(const std::string& name);
    /// <summary>Frees the name.</summary>
    /// <remarks>0x00402220.</remarks>
    virtual ~EditorAction() = default;

    /// <summary>Does the change again.</summary>
    /// <returns>Whether it could be done.</returns>
    /// <remarks>Virtual table slot 1.</remarks>
    virtual bool redo() = 0;
    /// <summary>Takes the change back.</summary>
    /// <returns>Whether it could be undone.</returns>
    /// <remarks>Virtual table slot 2.</remarks>
    virtual bool undo() = 0;

    /// <summary>The action's name.</summary>
    std::string name;
};

/// <summary>A change to some of the map's tiles: each changed tile's state before the change, swapped in and out.</summary>
/// <remarks>0x1c44 bytes, virtual table 0x004fa9a8.</remarks>
class TileAction : public EditorAction
{
public:
    /// <summary>A tile action on <paramref name="map"/> with no tiles recorded.</summary>
    TileAction(const std::string& name, EditorMap* map);
    /// <summary>Frees the records.</summary>
    /// <remarks>0x004023f0.</remarks>
    ~TileAction() override = default;

    /// <summary>Swaps each recorded tile with the map's.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004128f0 (slot 1 jumps there).</remarks>
    bool redo() override;
    /// <summary>Swaps each recorded tile with the map's (the same swap as <see cref="redo"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004128f0 (slot 2 jumps there).</remarks>
    bool undo() override;

    /// <summary>Records tile (<paramref name="col"/>, <paramref name="row"/>) as <paramref name="tile"/> is, once.</summary>
    /// <remarks>0x004129d0.</remarks>
    void recordTile(int32_t col, int32_t row, const EditorTile& tile);
    /// <summary>Records the four tiles around vertex (<paramref name="col"/>, <paramref name="row"/>) that exist.</summary>
    /// <remarks>0x00412a70.</remarks>
    void recordVertex(int32_t col, int32_t row);
    /// <summary>Whether tile (<paramref name="col"/>, <paramref name="row"/>) is recorded.</summary>
    bool isRecorded(int32_t col, int32_t row) const;
    /// <summary>
    /// Adds tile (<paramref name="col"/>, <paramref name="row"/>) as <paramref name="tile"/> is to the records without
    /// marking it recorded, as the height tools do for the whole map.
    /// </summary>
    void appendRecord(int32_t col, int32_t row, const EditorTile& tile);

    /// <summary>One recorded tile: where and how it was.</summary>
    /// <remarks>0x3c bytes (a 0x44-byte list node).</remarks>
    struct TileRecord
    {
        int32_t col;
        int32_t row;
        EditorTile tile;
    };

    /// <summary>The recorded tiles, in the order recorded (+0x18).</summary>
    std::list<TileRecord> records;
    /// <summary>The map (+0x20).</summary>
    EditorMap* map;
    /// <summary>Which tiles are recorded: bit <c>row * 240 + col</c> (+0x24).</summary>
    uint8_t recorded[0x1c20] = {};
};
