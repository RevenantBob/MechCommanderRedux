#pragma once

#include "VertexTool.h"

// The Make Hilly and Flatten tools (virtual table 0x004fc180). Their source file name wasn't kept; the names are the
// port's.

/// <summary>
/// The tool that flattens the tile under the cursor to its lowest corner, or makes its corners hilly at random; the
/// other one on the alternate apply.
/// </summary>
/// <remarks>0x838 bytes, virtual table 0x004fc180 (a vertex tool's 15 slots).</remarks>
class HillTool : public VertexTool
{
public:
    /// <summary>The Flatten tool with <paramref name="flatten"/>, else the Make Hilly tool.</summary>
    /// <remarks>Built inline by the view's commands (0x0041f640, 0x0041f0c0).</remarks>
    explicit HillTool(bool flatten);

    /// <summary>Begins as a vertex tool, its height action named "Flatten" or "Hills".</summary>
    /// <returns>true.</returns>
    /// <remarks>0x00409560.</remarks>
    bool begin(EditorMap& map) override;
    /// <summary>Flattens the tile under (<paramref name="x"/>, <paramref name="y"/>), or makes it hilly.</summary>
    /// <remarks>0x004097c0.</remarks>
    bool apply(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;
    /// <summary>The other of <see cref="apply"/>'s two.</summary>
    /// <remarks>0x004097f0.</remarks>
    bool applyAgain(EditorMap& map, int32_t x, int32_t y, ToolKeys keys) override;

private:
    /// <summary>
    /// Sets the four vertices of the tile under (<paramref name="x"/>, <paramref name="y"/>) to its lowest corner's
    /// elevation (the neighbours following).
    /// </summary>
    /// <returns>Whether every move was possible; false off the map.</returns>
    /// <remarks>0x00409680.</remarks>
    bool flattenAt(EditorMap& map, int32_t x, int32_t y);
    /// <summary>Moves the four vertices of the tile under (<paramref name="x"/>, <paramref name="y"/>) at random.</summary>
    /// <returns>true; false off the map.</returns>
    /// <remarks>0x00409820.</remarks>
    bool makeHillyAt(EditorMap& map, int32_t x, int32_t y);
    /// <summary>Selects the four vertices of tile (<paramref name="col"/>, <paramref name="row"/>).</summary>
    static void selectTileVertices(EditorMap& map, int32_t col, int32_t row);

    /// <summary>Flatten rather than Make Hilly (+0x834).</summary>
    bool flatten;
};
