#include "stdafx.h"
#include "vfx/vfxint.h"

// vfx3d.asm: VFX's convex polygon fillers. Every routine clips the pane to its window and takes its vertices relative
// to the clipped corner (max(0, x0), max(0, y0)); the renderer walks the edges and fills the spans (the scheme is
// described in platform/MCSoftwareRendererPolygons.cpp).

namespace
{
    /// <summary>The map lookaside table (0x007a9b8c), set by VFX_map_lookaside.</summary>
    uint8_t MapLookaside[256];

    /// <summary>
    /// The vfx3d prologue: clips the pane to its window and draws <paramref name="command"/> there (vertices relative
    /// to the clipped corner, clipped to 0..xmax, 0..ymax from it).
    /// </summary>
    void DrawPolygon(MCPane* pane, MCPolygonCommand& command)
    {
        const MCWindow* window = pane->Window;
        const int32_t x1 = window->XMax < pane->X1 ? window->XMax : pane->X1;
        const int32_t x0 = 0 > pane->X0 ? 0 : pane->X0;
        command.XMax = x1 - x0;

        if (command.XMax < 0)
        {
            return;
        }

        const int32_t y1 = window->YMax < pane->Y1 ? window->YMax : pane->Y1;
        const int32_t y0 = 0 > pane->Y0 ? 0 : pane->Y0;
        command.YMax = y1 - y0;

        if (command.YMax < 0)
        {
            return;
        }

        command.OriginX = x0;
        command.OriginY = y0;

        if (window->View != nullptr)
        {
            // Port: a view's scissor can cut the clip rectangle from the left or the top, which the fillers can't
            // express (they clip at 0 relative to the corner the vertices are given from). The vertices are moved to
            // be relative to the cut corner instead.
            int32_t cutX0 = x0;
            int32_t cutY0 = y0;
            int32_t cutX1 = x1;
            int32_t cutY1 = y1;
            MCClipToView(window, cutX0, cutY0, cutX1, cutY1);

            if (cutX1 < cutX0 || cutY1 < cutY0)
            {
                return;
            }

            if (cutX0 != x0 || cutY0 != y0)
            {
                static std::vector<MCScreenVertex> moved;
                moved.assign(command.Vertices, command.Vertices + command.VertexCount);

                for (MCScreenVertex& vertex : moved)
                {
                    vertex.X -= cutX0 - x0;
                    vertex.Y -= cutY0 - y0;
                }

                command.Vertices = moved.data();
            }

            command.OriginX = cutX0;
            command.OriginY = cutY0;
            command.XMax = cutX1 - cutX0;
            command.YMax = cutY1 - cutY0;
        }

        MCRenderer::For(pane->Window).Polygon(pane->Window, command);
    }

    /// <summary>A polygon command of <paramref name="kind"/> over the vertices.</summary>
    MCPolygonCommand MakePolygon(MCPolygonKind kind, int32_t vcnt, const MCScreenVertex* vlist)
    {
        MCPolygonCommand command{};
        command.Kind = kind;
        command.VertexCount = vcnt;
        command.Vertices = vlist;
        return command;
    }
}

void VfxFlatPolygon(MCPane* pane, int32_t vcnt, MCScreenVertex* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Flat, vcnt, vlist);
    DrawPolygon(pane, command);
}

void VfxGouraudPolygon(MCPane* pane, int32_t vcnt, MCScreenVertex* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Gouraud, vcnt, vlist);
    DrawPolygon(pane, command);
}

void VfxDitheredGouraudPolygon(MCPane* pane, MCFixed16 ditherAmount, int32_t vcnt, MCScreenVertex* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::DitheredGouraud, vcnt, vlist);
    command.DitherAmount = ditherAmount;
    DrawPolygon(pane, command);
}

void VfxTranslatePolygon(MCPane* pane, int32_t vcnt, MCScreenVertex* vlist, void* lookaside)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Translate, vcnt, vlist);
    command.Table = static_cast<const uint8_t*>(lookaside);
    DrawPolygon(pane, command);
}

void VfxIlluminatePolygon(MCPane* pane, MCFixed16 ditherAmount, int32_t vcnt, MCScreenVertex* vlist)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Illuminate, vcnt, vlist);
    command.DitherAmount = ditherAmount;
    DrawPolygon(pane, command);
}

void VfxMapLookaside(uint8_t* table)
{
    std::memcpy(MapLookaside, table, sizeof(MapLookaside));
    // The copy is the table draws read: registered (again) as new bytes.
    MCRenderer::RegisterData(MapLookaside, sizeof(MapLookaside), MCDataKind::Tables);
}

void VfxMapPolygon(MCPane* pane, int32_t vcnt, MCScreenVertex* vlist, MCWindow* texture, uint32_t flags)
{
    MCPolygonCommand command = MakePolygon(MCPolygonKind::Map, vcnt, vlist);
    command.Table = MapLookaside;
    command.Texture = texture;
    command.MapFlags = flags;
    DrawPolygon(pane, command);
}
