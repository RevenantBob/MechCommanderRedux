#include "stdafx.h"
#include "ToolFactory.h"
#include "GroundTools.h"
#include "OverlayTools.h"
#include "TileLibrary.h"

BrushTool* makePaletteTool(UINT command, int32_t variant)
{
    if (command >= OVERLAY_COMMANDS && command <= OVERLAY_COMMANDS_LAST)
    {
        const auto kind = static_cast<int32_t>(command - OVERLAY_COMMANDS);

        if ((kind & OVERLAY_TURNED) != 0 || kind == OVERLAY_BRIDGE || kind == OVERLAY_RAIL_BRIDGE)
        {
            return new BridgeTool(kind & OVERLAY_KIND_MASK, (kind & OVERLAY_TURNED) != 0);
        }

        if (kind == OVERLAY_FOREST)
        {
            return new ForestTool();
        }

        if (kind == OVERLAY_ROUGH)
        {
            return new RoughTool();
        }

        return new OverlayTool(kind);
    }

    if (command < TERRAIN_COMMANDS || command >= TERRAIN_COMMANDS_END)
    {
        return nullptr;
    }

    const auto texture = static_cast<uint8_t>(command & TERRAIN_TEXTURE_MASK);

    if (texture == TEXTURE_CONC || texture == TEXTURE_CROP)
    {
        return new ConcreteTool(texture, variant);
    }

    if (texture == TEXTURE_CLIFF)
    {
        return new CliffTool();
    }

    if (texture == TEXTURE_WATER)
    {
        return new WaterTool();
    }

    if (texture < TEXTURE_CLIFF)
    {
        return new TextureTool(texture, IDB_PAINT_CURSOR);
    }

    return nullptr;
}
