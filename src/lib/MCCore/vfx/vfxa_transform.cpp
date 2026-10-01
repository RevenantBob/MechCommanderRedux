#include "stdafx.h"
#include "vfx/vfxint.h"

// VFX_shape_transform (vfxa.asm): draws a shape rotated and scaled about its hot spot. The shape is first drawn
// upright into a caller-supplied work buffer (a bitmap of the shape's size, cleared to 255), then the buffer is
// texture-mapped onto the quadrilateral the rotated and scaled corners make, with 255 as the transparent texel.

namespace
{
    /// <summary>One corner of the mapped quadrilateral (the asm's 5-dword vertex records at 0x007a9a5c).</summary>
    struct TransformVertex
    {
        int32_t x;        // +0x00
        int32_t y;        // +0x04
        int32_t unused08; // +0x08 (the colour slot of VFX's vertices; never set here)
        int32_t u;        // +0x0c
        int32_t v;        // +0x10
    };

    /// <summary>The quadrilateral's corners (MCX.EXE 0x007a9a5c, 4 x 0x14 bytes).</summary>
    TransformVertex transformVertices[4];

    /// <summary>
    /// The texel-pointer steps of the current span (MCX.EXE 0x007a9aac): indexed by (u carry * 2 + v carry) of the
    /// fractional accumulators, each the integer u step plus the integer v step times the texture's row length,
    /// with one more texel/row where a carry occurred.
    /// </summary>
    int32_t spanSteps[4];

    /// <summary>
    /// The asm's 16.16 divide: <c>(int64)numerator &lt;&lt; 16</c> by <c>(int32)(count &lt;&lt; 16)</c>, quotient kept to
    /// 32 bits. (idiv faulted on overflow in the original; the port truncates.)
    /// </summary>
    int32_t StepDivide(int32_t numerator, int32_t count)
    {
        const int64_t dividend = static_cast<int64_t>(numerator) * 65536;
        const int32_t divisor = static_cast<int32_t>(static_cast<uint32_t>(count) << 16);

        if (divisor == 0)
        {
            return 0; // Port fix: the original faulted.
        }

        return static_cast<int32_t>(dividend / divisor);
    }

    /// <summary>An edge's step: the difference of two integer coordinates, as 16.16 per row.</summary>
    int32_t EdgeStep(int32_t from, int32_t to, int32_t rows)
    {
        return StepDivide(static_cast<int32_t>(static_cast<uint32_t>(to - from) << 16), rows);
    }

    /// <summary>An integer coordinate as the 16.16 start of an edge (the pixel's centre).</summary>
    int32_t EdgeStart(int32_t value)
    {
        return static_cast<int32_t>(static_cast<uint32_t>(value) << 16) + 0x8000;
    }

    /// <summary>A 16.16 step times an integer count: <c>(step * (count &lt;&lt; 16)) &gt;&gt; 16</c>, low 32 bits.</summary>
    int32_t StepTimes(int32_t step, int32_t count)
    {
        const int64_t product = static_cast<int64_t>(step) * static_cast<int32_t>(static_cast<uint32_t>(count) << 16);
        return static_cast<int32_t>(static_cast<uint64_t>(product) >> 16);
    }

    /// <summary>One side of the polygon being walked down: the 16.16 x, u, v and their per-row steps.</summary>
    struct Edge
    {
        int32_t x;
        int32_t u;
        int32_t v;
        int32_t dx;
        int32_t du;
        int32_t dv;
        /// <summary>Rows left on this edge.</summary>
        int32_t count;
        /// <summary>The vertex the edge starts at, and the one it runs to.</summary>
        int32_t from;
        int32_t to;
    };

    /// <summary>Sets up <paramref name="edge"/> from vertex <paramref name="from"/> to <paramref name="to"/>.</summary>
    void SetupEdge(Edge& edge, int32_t from, int32_t to, int32_t rows)
    {
        const TransformVertex& a = transformVertices[from];
        const TransformVertex& b = transformVertices[to];
        edge.count = rows;
        edge.dx = EdgeStep(a.x, b.x, rows);
        edge.du = EdgeStep(a.u, b.u, rows);
        edge.dv = EdgeStep(a.v, b.v, rows);
        edge.x = EdgeStart(a.x);
        edge.u = EdgeStart(a.u);
        edge.v = EdgeStart(a.v);
    }

    /// <summary>
    /// The integer part (towards zero) of a 16.16 span step, and the extra step (+1 or -1) a carry of its fraction
    /// adds, as the asm splits it (and <paramref name="negative"/> when the step is below zero).
    /// </summary>
    void SplitStep(int32_t step, int32_t& whole, bool& negative)
    {
        const uint32_t fraction = static_cast<uint32_t>(step) & 0xffff;
        int32_t integer = static_cast<int32_t>((static_cast<uint32_t>(step) >> 16) & 0xffff);
        negative = (integer & 0x8000) != 0;

        if (negative)
        {
            integer |= static_cast<int32_t>(0xffff0000);

            if (fraction != 0)
            {
                integer += 1;
            }
        }

        whole = integer;
    }
}

int32_t VFX_shape_transform(PANE* pane, void* shapeTable, int32_t shapeNum, int32_t hotX, int32_t hotY, void* buffer,
                            int32_t rot, int32_t x_scale, int32_t y_scale, uint32_t flags)
{
    // Neither rotated nor scaled: an ordinary draw.
    if (x_scale == 0x10000 && y_scale == 0x10000 && rot == 0)
    {
        if (flags & ST_XLAT)
        {
            return VFX_shape_translate_draw(pane, shapeTable, shapeNum, hotX, hotY);
        }

        return VFX_shape_draw(pane, shapeTable, shapeNum, hotX, hotY);
    }

    // The work buffer: a window the size of the shape, with a pane over all of it.
    const int32_t resolution = VFX_shape_resolution(shapeTable, shapeNum);
    const int32_t lastColumn = static_cast<int32_t>(static_cast<uint32_t>(resolution) >> 16) - 1;
    const int32_t lastRow = (resolution & 0xffff) - 1;
    WINDOW work;
    work.buffer = static_cast<uint8_t*>(buffer);
    work.x_max = lastColumn;
    work.y_max = lastRow;
    PANE workPane;
    workPane.window = &work;
    workPane.x0 = 0;
    workPane.y0 = 0;
    workPane.x1 = lastColumn;
    workPane.y1 = lastRow;

    // Texture coordinates of the corners, clockwise from the top left.
    transformVertices[0].u = 0;
    transformVertices[0].v = 0;
    transformVertices[1].u = lastColumn;
    transformVertices[1].v = 0;
    transformVertices[2].u = lastColumn;
    transformVertices[2].v = lastRow;
    transformVertices[3].u = 0;
    transformVertices[3].v = lastRow;

    // The hot spot's position in the work buffer.
    const int32_t minXY = VFX_shape_minxy(shapeTable, shapeNum);
    VFX_POINT origin;
    origin.x = -(minXY >> 16);
    origin.y = -static_cast<int32_t>(static_cast<int16_t>(minXY & 0xffff));

    if (!(flags & ST_REUSE))
    {
        VFX_pane_wipe(&workPane, 0xff);

        if (flags & ST_XLAT)
        {
            VFX_shape_translate_draw(&workPane, shapeTable, shapeNum, origin.x, origin.y);
        }
        else
        {
            VFX_shape_draw(&workPane, shapeTable, shapeNum, origin.x, origin.y);
        }
    }

    MCVfxClip clip;
    const int32_t status = MCVfxClipPane(pane, clip);

    if (status != 0)
    {
        return status;
    }

    // Rotate and scale the corners about the hot spot, then place them in window coordinates.
    const int32_t offsetX = hotX - origin.x;
    const int32_t offsetY = hotY - origin.y;
    const VFX_POINT corners[4] = {{0, 0}, {lastColumn, 0}, {lastColumn, lastRow}, {0, lastRow}};

    for (int i = 0; i < 4; ++i)
    {
        VFX_POINT in = corners[i];
        VFX_POINT out;
        VFX_point_transform(&in, &out, &origin, rot, x_scale, y_scale);
        transformVertices[i].x = out.x + offsetX + clip.PaneX;
        transformVertices[i].y = out.y + offsetY + clip.PaneY;
    }

    const uint8_t* texture = work.buffer;
    const int32_t textureStride = lastColumn + 1;
    // Port fix: texel reads outside the work buffer (the original read whatever lay there) count as transparent.
    const size_t textureSize =
        static_cast<size_t>(std::max(textureStride, 0)) * static_cast<size_t>(std::max(lastRow + 1, 0));

    // Outcodes, and the top (the last vertex with the smallest y) and bottom.
    int32_t yTop = 0x7fff;
    int32_t yBottom = -0x8000;
    int32_t top = 0;
    uint32_t allOut = 0xf;

    for (int i = 0; i < 4; ++i)
    {
        const TransformVertex& vertex = transformVertices[i];
        uint32_t code = 0;
        code = (code << 1) | ((vertex.x - clip.X0) < 0 ? 1u : 0u);
        code = (code << 1) | ((clip.X1 - vertex.x) < 0 ? 1u : 0u);
        code = (code << 1) | ((vertex.y - clip.Y0) < 0 ? 1u : 0u);
        code = (code << 1) | ((clip.Y1 - vertex.y) < 0 ? 1u : 0u);

        if (vertex.y <= yTop)
        {
            yTop = vertex.y;
            top = i;
        }

        if (vertex.y >= yBottom)
        {
            yBottom = vertex.y;
        }

        allOut &= code;
    }

    if (allOut != 0)
    {
        return 0;
    }

    if (yBottom == yTop)
    {
        return 0;
    }

    // The left side walks the vertices backwards from the top, the right side forwards (which is really left is
    // settled per row). Edges wholly above the pane, and flat ones, are passed over.
    Edge left;
    Edge right;
    int32_t leftEnd = top;

    for (;;)
    {
        const int32_t from = leftEnd;
        const int32_t to = from == 0 ? 3 : from - 1;
        leftEnd = to;
        const int32_t fromY = transformVertices[from].y;
        const int32_t toY = transformVertices[to].y;

        if (fromY < clip.Y0 && toY <= clip.Y0)
        {
            continue;
        }

        if (toY - fromY == 0)
        {
            continue;
        }

        left.from = from;
        left.to = to;
        SetupEdge(left, from, to, toY - fromY);
        break;
    }

    int32_t rightEnd = top;

    for (;;)
    {
        const int32_t from = rightEnd;
        const int32_t to = from == 3 ? 0 : from + 1;
        rightEnd = to;
        const int32_t fromY = transformVertices[from].y;
        const int32_t toY = transformVertices[to].y;

        if (fromY < clip.Y0 && toY <= clip.Y0)
        {
            continue;
        }

        if (toY - fromY == 0)
        {
            continue;
        }

        right.from = from;
        right.to = to;
        SetupEdge(right, from, to, toY - fromY);
        break;
    }

    // Rows to draw after the first, clipped to the pane's bottom and then its top.
    int32_t rowsLeft = clip.Y1 - yTop;

    if (yBottom - clip.Y1 <= 0)
    {
        rowsLeft += yBottom - clip.Y1;
    }

    int32_t y = yTop;

    if (clip.Y0 - yTop > 0)
    {
        rowsLeft -= clip.Y0 - yTop;
        y = clip.Y0;
        int32_t skip = clip.Y0 - transformVertices[left.from].y;
        left.count -= skip;
        left.x += StepTimes(left.dx, skip);
        left.u += StepTimes(left.du, skip);
        left.v += StepTimes(left.dv, skip);
        skip = clip.Y0 - transformVertices[right.from].y;
        right.count -= skip;
        right.x += StepTimes(right.dx, skip);
        right.u += StepTimes(right.du, skip);
        right.v += StepTimes(right.dv, skip);
    }

    uint8_t* row = clip.Buffer + static_cast<intptr_t>(y) * clip.Stride;
    // The span's steps persist between rows: a one-pixel span reuses the previous row's (as the asm's did).
    int32_t spanDu = 0;
    int32_t spanDv = 0;

    for (;;)
    {
        // The span, ordered left to right.
        int32_t lx = left.x;
        int32_t lu = left.u;
        int32_t lv = left.v;
        int32_t rx = right.x;
        int32_t ru = right.u;
        int32_t rv = right.v;

        if (rx <= lx)
        {
            std::swap(lx, rx);
            std::swap(lu, ru);
            std::swap(lv, rv);
        }

        int32_t xl = lx >> 16;
        int32_t xr = rx >> 16;

        if (xl <= clip.X1 && xr >= clip.X0)
        {
            const int32_t width = xr - xl;

            if (width != 0)
            {
                spanDu = StepDivide(ru - lu, width);
                int32_t uWhole;
                bool uNegative;
                SplitStep(spanDu, uWhole, uNegative);
                const int32_t uCarry = uWhole + (uNegative ? -1 : 1);

                spanDv = StepDivide(rv - lv, width);
                int32_t vWhole;
                bool vNegative;
                SplitStep(spanDv, vWhole, vNegative);
                const int32_t vCarry = vNegative ? -textureStride : textureStride;
                // imul dx / cwde: the row offset is formed in 16 bits.
                const int32_t vOffset =
                    static_cast<int16_t>(static_cast<int16_t>(textureStride) * static_cast<int16_t>(vWhole));

                spanSteps[0] = uWhole + vOffset;
                spanSteps[1] = spanSteps[0] + vCarry;
                spanSteps[2] = uCarry + vOffset;
                spanSteps[3] = spanSteps[2] + vCarry;

                const int32_t clipLeft = clip.X0 - xl;

                if (clipLeft > 0)
                {
                    xl += clipLeft;
                    lu += StepTimes(spanDu, clipLeft);
                    lv += StepTimes(spanDv, clipLeft);
                }

                const int32_t clipRight = xr - clip.X1;

                if (clipRight > 0)
                {
                    xr -= clipRight;
                }
            }

            // Texel pointer, and the fractional accumulators (the complement when stepping backwards).
            intptr_t texel = static_cast<intptr_t>(static_cast<uint32_t>(lv) >> 16) * textureStride +
                             (static_cast<uint32_t>(lu) >> 16);
            uint32_t uStep = static_cast<uint32_t>(spanDu);
            uint32_t uFraction = static_cast<uint32_t>(lu);

            if (spanDu < 0)
            {
                uStep = 0u - uStep;
                uFraction = ~uFraction;
            }

            uStep <<= 16;
            uFraction <<= 16;
            uint32_t vStep = static_cast<uint32_t>(spanDv);
            uint32_t vFraction = static_cast<uint32_t>(lv);

            if (spanDv < 0)
            {
                vStep = 0u - vStep;
                vFraction = ~vFraction;
            }

            vStep <<= 16;
            vFraction <<= 16;

            uint8_t* out = row + xl;

            for (int32_t n = xr - xl; n >= 0; --n)
            {
                if (texel >= 0 && static_cast<size_t>(texel) < textureSize)
                {
                    const uint8_t color = texture[texel];

                    if (color != 0xff)
                    {
                        *out = color;
                    }
                }

                ++out;
                uint32_t index = 0;
                const uint32_t newU = uFraction + uStep;
                index = (index << 1) | (newU < uFraction ? 1u : 0u);
                uFraction = newU;
                const uint32_t newV = vFraction + vStep;
                index = (index << 1) | (newV < vFraction ? 1u : 0u);
                vFraction = newV;
                texel += spanSteps[index];
            }
        }

        row += clip.Stride;
        --rowsLeft;

        if (rowsLeft < 0)
        {
            break;
        }

        if (rowsLeft == 0)
        {
            // The last row: both sides step without looking at their counts.
            left.x += left.dx;
            left.u += left.du;
            left.v += left.dv;
            right.x += right.dx;
            right.u += right.du;
            right.v += right.dv;
            continue;
        }

        if (--left.count == 0)
        {
            const int32_t from = leftEnd;
            const int32_t to = from == 0 ? 3 : from - 1;
            leftEnd = to;
            int32_t rows = transformVertices[to].y - transformVertices[from].y;

            if (static_cast<uint32_t>(rows) < 1)
            {
                rows = 1;
            }

            left.from = from;
            left.to = to;
            SetupEdge(left, from, to, rows);
        }
        else
        {
            left.x += left.dx;
            left.u += left.du;
            left.v += left.dv;
        }

        if (--right.count == 0)
        {
            const int32_t from = rightEnd;
            const int32_t to = from == 3 ? 0 : from + 1;
            rightEnd = to;
            int32_t rows = transformVertices[to].y - transformVertices[from].y;

            if (static_cast<uint32_t>(rows) < 1)
            {
                rows = 1;
            }

            right.from = from;
            right.to = to;
            SetupEdge(right, from, to, rows);
        }
        else
        {
            right.x += right.dx;
            right.u += right.du;
            right.v += right.dv;
        }
    }

    return 0;
}
