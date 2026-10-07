#include "stdafx.h"
#include "vfx/vfxint.h"

// VFX_shape_transform (vfxa.asm): draws a shape rotated and scaled about its hot spot. The shape is first drawn
// upright into a caller-supplied work buffer (a bitmap of the shape's size, cleared to 255), then the buffer is
// texture-mapped onto the quadrilateral the rotated and scaled corners make, with 255 as the transparent texel.

int32_t VfxShapeTransform(MCPane* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                          int32_t rot, int32_t xScale, int32_t yScale, uint32_t flags)
{
    // Neither rotated nor scaled: an ordinary draw.
    if (xScale == 0x10000 && yScale == 0x10000 && rot == 0)
    {
        if (flags & ST_XLAT)
        {
            return VfxShapeTranslateDraw(pane, shapeTable, shapeNum, hotX, hotY);
        }

        return VfxShapeDraw(pane, shapeTable, shapeNum, hotX, hotY);
    }

    // The work buffer: a window the size of the shape, with a pane over all of it.
    const int32_t resolution = VfxShapeResolution(shapeTable, shapeNum);
    const int32_t lastColumn = static_cast<int32_t>(static_cast<uint32_t>(resolution) >> 16) - 1;
    const int32_t lastRow = (resolution & 0xffff) - 1;
    MCWindow work{};
    work.Buffer = static_cast<uint8_t*>(buffer);
    work.XMax = lastColumn;
    work.YMax = lastRow;
    MCPane workPane;
    workPane.Window = &work;
    workPane.X0 = 0;
    workPane.Y0 = 0;
    workPane.X1 = lastColumn;
    workPane.Y1 = lastRow;

    // The quadrilateral's corners (MCX.EXE 0x007a9a5c), clockwise from the top left, with their texture coordinates.
    MCMapQuadCommand command;
    command.Corners[0].U = 0;
    command.Corners[0].V = 0;
    command.Corners[1].U = lastColumn;
    command.Corners[1].V = 0;
    command.Corners[2].U = lastColumn;
    command.Corners[2].V = lastRow;
    command.Corners[3].U = 0;
    command.Corners[3].V = lastRow;

    // The hot spot's position in the work buffer.
    const int32_t minXY = VfxShapeMinxy(shapeTable, shapeNum);
    MCVfxPoint origin;
    origin.X = -(minXY >> 16);
    origin.Y = -static_cast<int32_t>(static_cast<int16_t>(minXY & 0xffff));

    if (!(flags & ST_REUSE))
    {
        VfxPaneWipe(&workPane, 0xff);

        if (flags & ST_XLAT)
        {
            VfxShapeTranslateDraw(&workPane, shapeTable, shapeNum, origin.X, origin.Y);
        }
        else
        {
            VfxShapeDraw(&workPane, shapeTable, shapeNum, origin.X, origin.Y);
        }
    }

    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    // Rotate and scale the corners about the hot spot, then place them in window coordinates.
    const int32_t offsetX = hotX - origin.X;
    const int32_t offsetY = hotY - origin.Y;
    const MCVfxPoint corners[4] = {{0, 0}, {lastColumn, 0}, {lastColumn, lastRow}, {0, lastRow}};

    for (int i = 0; i < 4; ++i)
    {
        MCVfxPoint in = corners[i];
        MCVfxPoint out;
        VfxPointTransform(&in, &out, &origin, rot, xScale, yScale);
        command.Corners[i].X = out.X + offsetX + clip.PaneX;
        command.Corners[i].Y = out.Y + offsetY + clip.PaneY;
    }

    // The work buffer is texture-mapped onto the quadrilateral, 255 transparent. (The game never calls this; the work
    // buffer is the caller's, so its texture lasts for the draw.)
    command.Clip = MCRect{clip.X0, clip.Y0, clip.X1, clip.Y1};
    command.Texture = &work;
    MCRenderer::CreateTexture(&work, MCTextureUse::Dynamic);
    MCRenderer::For(pane->Window).MapQuad(pane->Window, command);
    MCRenderer::DestroyTexture(&work);
    return 0;
}
