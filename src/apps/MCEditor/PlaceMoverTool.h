#pragma once

#include "PlaceObjectTool.h"

struct EditorTile;

// The tool of the Map Features palette's Units tab (virtual table 0x004faea8, 0x00413520-0x004138d0). Its source file
// name wasn't kept; the names are the port's.

/// <summary>The tool that places mechs and vehicles of one id (or erases them) where it is applied.</summary>
/// <remarks>0x258bc bytes, virtual table 0x004faea8.</remarks>
class PlaceMoverTool : public PlaceObjectTool
{
public:
    /// <summary>The most mechs and vehicles a map may hold.</summary>
    static constexpr int32_t MAX_MOVERS = 0x2d;

    /// <summary>
    /// A tool placing the mech or vehicle <paramref name="id"/> for <paramref name="side"/>, a mech as
    /// <paramref name="variant"/>.
    /// </summary>
    /// <remarks>0x00413520.</remarks>
    PlaceMoverTool(uint32_t id, int32_t side, int32_t variant);
    /// <summary>Takes the highlight off the tile it was last over.</summary>
    /// <remarks>0x00413580.</remarks>
    ~PlaceMoverTool() override;

    /// <summary>Starts the use as the place tool does, its error box allowed again.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x004135a0.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>
    /// Places a new mech or vehicle on the cell under (<paramref name="x"/>, <paramref name="y"/>), or erases the one
    /// there.
    /// </summary>
    /// <returns>false off the grid, or when there is nothing to erase.</returns>
    /// <remarks>0x00413750.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>
    /// Highlights the tile under (<paramref name="x"/>, <paramref name="y"/>) and tells whether the mover can be
    /// placed there as the place tool does; with the map's movers at <see cref="MAX_MOVERS"/>, never (the first time
    /// in a use, the error box says why).
    /// </summary>
    /// <remarks>0x004135c0.</remarks>
    bool canApply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

private:
    /// <summary>The variant a placed mech gets (+0x258b4).</summary>
    int32_t variant;
    /// <summary>The tile highlighted last (+0x258b8).</summary>
    EditorTile* highlightedTile = nullptr;
};
