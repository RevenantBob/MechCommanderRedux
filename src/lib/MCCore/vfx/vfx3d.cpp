#include "stdafx.h"
#include "vfx/vfxint.h"

// vfx3d.asm: VFX's convex polygon fillers. Every routine clips the pane to its window and takes its vertices relative
// to the clipped corner (max(0, x0), max(0, y0)); the renderer walks the edges and fills the spans (the scheme is
// described in platform/MCSoftwareRendererPolygons.cpp).

namespace
{
    /// <summary>The map lookaside table (0x007a9b8c), set by VFX_map_lookaside.</summary>
    uint8_t mapLookaside[256];

    /// <summary>
    /// The vfx3d prologue: clips the pane to its window and draws <paramref name="command"/> there (vertices relative
    /// to the clipped corner, clipped to 0..xmax, 0..ymax from it).
    /// </summary>
    void DrawPolygon(PANE* pane, MCPolygonCommand& command)
    {
        const WINDOW* window = pane->window;
        const int32_t x1 = window->x_max < pane->x1 ? window->x_max : pane->x1;
        const int32_t x0 = 0 > pane->x0 ? 0 : pane->x0;
        command.XMax = x1 - x0;

        if (command.XMax < 0)
        {
            return;
        }

        const int32_t y1 = window->y_max < pane->y1 ? window->y_max : pane->y1;
        const int32_t y0 = 0 > pane->y0 ? 0 : pane->y0;
        command.YMax = y1 - y0;

        if (command.YMax < 0)
        {
            return;
        }

        command.OriginX = x0;
        command.OriginY = y0;
        MCRenderer::For(pane->window).Polygon(pane->window, command);
    }

    /// <summary>A polygon command of <paramref name="kind"/> over the vertices.</summary>
    MCPolygonCommand MakePolygon(MCPolygonKind kind, int32_t vcnt, const SCRNVERTEX* vlist)
    {
        MCPolygonCommand command{};
        command.Kind = kind;
        command.VertexCount = vcnt;
        command.Vertices = vlist;
        return command;
    }
}

void VFX_flat_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Flat, vcnt, vlist);
    DrawPolygon(pane, command);
}

void VFX_Gouraud_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Gouraud, vcnt, vlist);
    DrawPolygon(pane, command);
}

void VFX_dithered_Gouraud_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::DitheredGouraud, vcnt, vlist);
    command.DitherAmount = ditherAmount;
    DrawPolygon(pane, command);
}

void VFX_translate_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, void* lookaside)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Translate, vcnt, vlist);
    command.Table = static_cast<const uint8_t*>(lookaside);
    DrawPolygon(pane, command);
}

void VFX_illuminate_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Illuminate, vcnt, vlist);
    command.DitherAmount = ditherAmount;
    DrawPolygon(pane, command);
}

void VFX_map_lookaside(uint8_t* table)
{
    std::memcpy(mapLookaside, table, sizeof(mapLookaside));
}

void VFX_map_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, WINDOW* texture, uint32_t flags)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Map, vcnt, vlist);
    command.Table = mapLookaside;
    command.Texture = texture;
    command.MapFlags = flags;
    DrawPolygon(pane, command);
}
