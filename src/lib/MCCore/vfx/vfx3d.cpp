#include "stdafx.h"
#include "vfx/vfxint.h"

// vfx3d.asm: VFX's convex polygon fillers. Every routine shares one scheme, reproduced here once:
//
// - The pane is clipped to its window; vertices are relative to the clipped corner (max(0, x0), max(0, y0)).
// - Each vertex gets four outcode bits (x < 0, x > xmax, y < 0, y > ymax); a bit every vertex has rejects the
//   polygon. The x bits of any vertex mark the spans as needing horizontal clipping.
// - The top vertex is the one with the smallest y (the last of equals). A left edge walks the vertex list backwards
//   from it, a right edge forwards. Edge x is 16.16 with +0.5 added at the vertex (x << 16 | 0x8000), stepped by
//   dx * 65536 / dy; interpolated values (colour, u, v) start at the vertex's value + 0x8000.
// - Rows run from the top vertex's y to the bottom vertex's y inclusive (clipped to the pane). On the last row the
//   edges are only stepped, never switched. Spans are filled from the smaller edge x >> 16 to the larger inclusive.
//
// The asm kept its walk state in globals at 0x007a9abc..0x007a9b88; the port keeps it in locals, except the
// Gouraud span slope, which the clipped path reuses from the previous span for one-pixel spans (without effect on the
// pixels), and the map lookaside table.

namespace
{
    /// <summary>The map lookaside table (0x007a9b8c), set by VFX_map_lookaside.</summary>
    uint8_t mapLookaside[256];

    /// <summary>The pane resolved as the vfx3d prologue does it.</summary>
    struct PolyTarget
    {
        /// <summary>The window pixel at the clipped pane's top-left corner (0x007a9ac4).</summary>
        uint8_t* Base;
        /// <summary>The window's row length (0x007a9ac8).</summary>
        int32_t Stride;
        /// <summary>The clipped pane's last column and row, relative to its corner (0x007a9abc, 0x007a9ac0).</summary>
        int32_t XMax;
        int32_t YMax;
    };

    /// <summary>The vfx3d prologue: clips the pane to its window.</summary>
    /// <returns>False when the clipped pane is empty.</returns>
    bool PolyPrologue(const PANE* pane, PolyTarget& target)
    {
        const WINDOW* window = pane->window;
        target.Stride = window->x_max + 1;
        const int32_t x1 = window->x_max < pane->x1 ? window->x_max : pane->x1;
        const int32_t x0 = 0 > pane->x0 ? 0 : pane->x0;
        target.XMax = x1 - x0;

        if (target.XMax < 0)
        {
            return false;
        }

        const int32_t y1 = window->y_max < pane->y1 ? window->y_max : pane->y1;
        const int32_t y0 = 0 > pane->y0 ? 0 : pane->y0;
        target.YMax = y1 - y0;

        if (target.YMax < 0)
        {
            return false;
        }

        target.Base = window->buffer + static_cast<intptr_t>(y0) * target.Stride + x0;
        return true;
    }

    /// <summary>
    /// An edge's x slope: <c>((int16)dx &lt;&lt; 32) / (dy &lt;&lt; 16)</c>, as the asm divides it (only dx's low 16 bits
    /// take part).
    /// </summary>
    int32_t EdgeXSlope(int32_t dx, int32_t dy)
    {
        const int64_t numerator = static_cast<int64_t>(static_cast<int16_t>(dx)) * 4294967296LL;
        const int32_t divisor = static_cast<int32_t>(static_cast<uint32_t>(dy) << 16);
        return static_cast<int32_t>(numerator / divisor);
    }

    /// <summary>A 16.16 value's slope over <paramref name="steps"/>: <c>(d &lt;&lt; 16) / (steps &lt;&lt; 16)</c> in 64 bits.</summary>
    int32_t ValueSlope(int32_t d, int32_t steps)
    {
        const int64_t numerator = static_cast<int64_t>(d) * 65536;
        const int32_t divisor = static_cast<int32_t>(static_cast<uint32_t>(steps) << 16);
        // Port fix: the asm's idiv faults when the quotient doesn't fit 32 bits; the port truncates.
        return static_cast<int32_t>(numerator / divisor);
    }

    /// <summary><c>imul</c> then <c>shrd 16</c>: the low 32 bits of the 64-bit product shifted right by 16.</summary>
    int32_t MulShift16(int32_t a, int32_t b)
    {
        return static_cast<int32_t>(static_cast<uint64_t>(static_cast<int64_t>(a) * b) >> 16);
    }

    /// <summary>Shifts left by 16 in 32 bits, as the asm's <c>shl reg, 16</c>.</summary>
    int32_t Shl16(int32_t a)
    {
        return static_cast<int32_t>(static_cast<uint32_t>(a) << 16);
    }

    /// <summary>One side's edge while a polygon is walked.</summary>
    template <int N> struct Edge
    {
        /// <summary>The vertex the edge starts at (0x007a9aec / 0x007a9af0).</summary>
        const SCRNVERTEX* Start;
        /// <summary>The vertex it runs to (0x007a9af4 / 0x007a9af8).</summary>
        const SCRNVERTEX* Next;
        /// <summary>Rows left on the edge.</summary>
        int32_t Count;
        int32_t X;
        int32_t DX;
        int32_t A[N > 0 ? N : 1];
        int32_t DA[N > 0 ? N : 1];
    };

    /// <summary>The value at byte offset <paramref name="offset"/> of a vertex (8 colour, 12 u, 16 v).</summary>
    int32_t VertexValue(const SCRNVERTEX* v, int offset)
    {
        switch (offset)
        {
            case 8:
                return v->c;
            case 12:
                return v->u;
            case 16:
                return v->v;
            default:
                return v->w;
        }
    }

    /// <summary>The walk's options, which differ slightly between the routines.</summary>
    struct WalkOptions
    {
        /// <summary>Whether a first edge that runs upwards ends the polygon (every routine but the dithered Gouraud).</summary>
        bool RejectUpwardFirstEdge;
    };

    /// <summary>
    /// Walks a convex polygon row by row, calling <paramref name="span"/> with the row's pixels, the two edges' x
    /// (16.16), their interpolated values, the number of rows clipped off the top and the row's index.
    /// </summary>
    /// <typeparam name="N">The number of interpolated values (0 to 2).</typeparam>
    /// <returns>False when the polygon was rejected before any row.</returns>
    template <int N, typename Span>
    void WalkPolygon(const PolyTarget& target, int32_t vcnt, const SCRNVERTEX* vlist,
                     const int (&offsets)[N > 0 ? N : 1], const WalkOptions& options, Span&& span)
    {
        // Port fix: the asm loops forever on an empty vertex list.
        if (vcnt <= 0)
        {
            return;
        }

        const SCRNVERTEX* first = vlist;
        const SCRNVERTEX* end = vlist + vcnt;

        uint32_t xClip = 0;
        uint32_t allOut = 0xf;
        int32_t minY = 0x7fff;
        int32_t maxY = -0x8000;
        const SCRNVERTEX* top = nullptr;

        for (const SCRNVERTEX* v = first; v != end; ++v)
        {
            uint32_t code = 0;
            code = code << 1 | static_cast<uint32_t>(v->x) >> 31;
            code = code << 1 | static_cast<uint32_t>(target.XMax - v->x) >> 31;
            xClip |= code;
            code = code << 1 | static_cast<uint32_t>(v->y) >> 31;
            code = code << 1 | static_cast<uint32_t>(target.YMax - v->y) >> 31;

            if (v->y <= minY)
            {
                minY = v->y;
                top = v;
            }

            if (v->y >= maxY)
            {
                maxY = v->y;
            }

            allOut &= code;
        }

        if (allOut != 0)
        {
            return;
        }

        // Port fix: every vertex above 0x7fff leaves no top vertex (the asm would read through a null pointer).
        if (top == nullptr)
        {
            return;
        }

        if (maxY == minY)
        {
            return;
        }

        auto setupEdge = [&](Edge<N>& edge, const SCRNVERTEX* start, const SCRNVERTEX* next, int32_t dy)
        {
            edge.Count = dy;
            edge.DX = EdgeXSlope(next->x - start->x, dy);

            for (int i = 0; i < N; ++i)
            {
                edge.DA[i] = ValueSlope(VertexValue(next, offsets[i]) - VertexValue(start, offsets[i]), dy);
            }

            edge.X = Shl16(start->x) + 0x8000;

            for (int i = 0; i < N; ++i)
            {
                edge.A[i] = VertexValue(start, offsets[i]) + 0x8000;
            }
        };

        auto stepBack = [&](const SCRNVERTEX* v) { return v - 1 < first ? end - 1 : v - 1; };
        auto stepForward = [&](const SCRNVERTEX* v) { return v + 1 >= end ? first : v + 1; };

        // The first edges: skip those wholly above the pane and the horizontal ones. The guard is the port's: the asm
        // relies on the polygon reaching y >= 0, which the outcodes guarantee, to end these loops.
        Edge<N> left{};
        Edge<N> right{};
        left.Next = top;
        right.Next = top;
        int guard = 0;

        for (;;)
        {
            if (++guard > 2 * vcnt + 2)
            {
                return;
            }

            left.Start = left.Next;
            left.Next = stepBack(left.Start);
            const int32_t y0 = left.Start->y;
            const int32_t y1 = left.Next->y;

            if (y0 < 0 && y1 <= 0)
            {
                continue;
            }

            const int32_t dy = y1 - y0;

            if (dy == 0)
            {
                continue;
            }

            // Original behaviour: the dithered Gouraud filler walks on along an edge running upwards (a non-convex
            // polygon) with a negative row count; the others give up.
            if (dy < 0 && options.RejectUpwardFirstEdge)
            {
                return;
            }

            setupEdge(left, left.Start, left.Next, dy);
            break;
        }

        guard = 0;

        for (;;)
        {
            if (++guard > 2 * vcnt + 2)
            {
                return;
            }

            right.Start = right.Next;
            right.Next = stepForward(right.Start);
            const int32_t y0 = right.Start->y;
            const int32_t y1 = right.Next->y;

            if (y0 < 0 && y1 <= 0)
            {
                continue;
            }

            const int32_t dy = y1 - y0;

            if (dy == 0)
            {
                continue;
            }

            if (dy < 0 && options.RejectUpwardFirstEdge)
            {
                return;
            }

            setupEdge(right, right.Start, right.Next, dy);
            break;
        }

        int32_t rows = target.YMax - minY;

        if (maxY - target.YMax <= 0)
        {
            rows += maxY - target.YMax;
        }

        int32_t clippedTop = 0;

        if (minY < 0)
        {
            clippedTop = -minY;
            rows -= clippedTop;
            minY = 0;

            for (Edge<N>* edge : {&left, &right})
            {
                const int32_t n = 0 - edge->Start->y;
                edge->Count -= n;
                edge->X += MulShift16(Shl16(n), edge->DX);

                for (int i = 0; i < N; ++i)
                {
                    edge->A[i] += MulShift16(Shl16(n), edge->DA[i]);
                }
            }
        }

        // A later edge: one running upwards ends the polygon; a horizontal one counts as one row.
        auto nextEdge = [&](Edge<N>& edge, bool backwards) -> bool
        {
            edge.Start = edge.Next;
            edge.Next = backwards ? stepBack(edge.Start) : stepForward(edge.Start);
            int32_t dy = edge.Next->y - edge.Start->y;

            if (dy < 0)
            {
                return false;
            }

            if (dy == 0)
            {
                dy = 1;
            }

            setupEdge(edge, edge.Start, edge.Next, dy);
            return true;
        };

        uint8_t* row = target.Base + static_cast<intptr_t>(minY) * target.Stride;

        for (int32_t rowIndex = 0;; ++rowIndex)
        {
            span(row, left, right, xClip != 0, clippedTop, rowIndex);
            row += target.Stride;
            --rows;

            if (rows < 0)
            {
                return;
            }

            if (rows == 0)
            {
                left.X += left.DX;

                for (int i = 0; i < N; ++i)
                {
                    left.A[i] += left.DA[i];
                }

                right.X += right.DX;

                for (int i = 0; i < N; ++i)
                {
                    right.A[i] += right.DA[i];
                }

                continue;
            }

            if (--left.Count == 0)
            {
                if (!nextEdge(left, true))
                {
                    return;
                }
            }
            else
            {
                left.X += left.DX;

                for (int i = 0; i < N; ++i)
                {
                    left.A[i] += left.DA[i];
                }
            }

            if (--right.Count == 0)
            {
                if (!nextEdge(right, false))
                {
                    return;
                }
            }
            else
            {
                right.X += right.DX;

                for (int i = 0; i < N; ++i)
                {
                    right.A[i] += right.DA[i];
                }
            }
        }
    }

    /// <summary>
    /// A span's extent, clipped to 0..xmax: the pixels [<paramref name="l"/>, <paramref name="l"/> +
    /// <paramref name="count"/>), and how many were cut off its left end.
    /// </summary>
    /// <returns>False when the span is wholly outside.</returns>
    /// <remarks>
    /// The asm has an unclipped path for polygons whose vertices are all inside horizontally; it writes the same
    /// pixels whenever the spans stay inside, which such spans do, so the port always takes the clipped one.
    /// </remarks>
    bool ClipSpan(int32_t xa, int32_t xb, int32_t xmax, int32_t& l, int32_t& r, int32_t& count, int32_t& cut)
    {
        l = xa >> 16;

        if (l > xmax)
        {
            return false;
        }

        r = xb >> 16;

        if (r < 0)
        {
            return false;
        }

        count = r - l + 1;
        cut = 0;

        if (l < 0)
        {
            cut = -l;
            count -= cut;
            l = 0;
        }

        if (r > xmax)
        {
            count -= r - xmax;
        }

        return true;
    }

    /// <summary>The slope the last clipped Gouraud/dithered span computed (0x007a9b54), reused by one-pixel spans.</summary>
    int32_t spanSlope = 0;

    /// <summary>
    /// The dithered Gouraud span shared by VFX_dithered_Gouraud_polygon and VFX_illuminate_polygon: pixel values
    /// alternate between two accumulators (colour + dither, colour + 0, swapped every row), each stepped by the slope
    /// over half the span per pixel pair.
    /// </summary>
    template <bool Add>
    void DitheredSpan(uint8_t* row, int32_t xa, int32_t xb, int32_t ca, int32_t cb, int32_t xmax, int32_t ditherA,
                      int32_t ditherB)
    {
        if (!(xb > xa))
        {
            std::swap(xa, xb);
            std::swap(ca, cb);
        }

        int32_t l = xa >> 16;

        if (l > xmax)
        {
            return;
        }

        int32_t r = xb >> 16;

        if (r < 0)
        {
            return;
        }

        const int32_t n = r - l;
        int32_t count = n + 1;

        if (count != 1)
        {
            int32_t half = n >> 1;

            if (half == 0)
            {
                half = 1;
            }

            spanSlope = ValueSlope(cb - ca, half);
        }

        const int32_t slope = spanSlope;
        int32_t cut = 0;
        int32_t c = ca;
        uint8_t* p = row + l;

        if (l < 0)
        {
            cut = -l;
            p += cut;
            count -= cut;
            c += MulShift16(static_cast<int32_t>(static_cast<uint32_t>(cut & ~1) << 15), slope);
        }

        if (r > xmax)
        {
            count -= r - xmax;
        }

        uint32_t accA = static_cast<uint32_t>(c + ditherA);
        uint32_t accB = static_cast<uint32_t>(c + ditherB);
        // The first pixel is A's unless an odd number of pixels was cut off the left, which puts B first.
        uint32_t first = accA;
        uint32_t second = accB;

        if (cut & 1)
        {
            first = accB;
            second = accA + static_cast<uint32_t>(slope);
        }

        auto pixel = [&](int32_t j) -> uint8_t
        { return static_cast<uint8_t>((j & 1) == 0 ? first >> 16 : second >> 16); };
        auto advance = [&](int32_t j)
        {
            if (j & 1)
            {
                first += static_cast<uint32_t>(slope);
                second += static_cast<uint32_t>(slope);
            }
        };

        if constexpr (!Add)
        {
            for (int32_t j = 0; j < count; ++j)
            {
                p[j] = pixel(j);
                advance(j);
            }
        }
        else
        {
            // The asm adds the values with byte adds for a lone pixel and 16-bit adds for pairs, so a pair's low byte
            // carries into its high byte. Pairs start at the first even address, or at the first pixel when an odd
            // number was cut off the left.
            int32_t j = 0;

            if ((cut & 1) == 0 && (reinterpret_cast<uintptr_t>(p) & 1) != 0)
            {
                p[0] = static_cast<uint8_t>(p[0] + pixel(0));
                advance(0);
                j = 1;
            }

            for (; j + 1 < count; j += 2)
            {
                const uint8_t lo = pixel(j);
                advance(j);
                const uint8_t hi = pixel(j + 1);
                advance(j + 1);
                const uint16_t word = static_cast<uint16_t>(p[j] | p[j + 1] << 8);
                const uint16_t sum = static_cast<uint16_t>(word + (lo | hi << 8));
                p[j] = static_cast<uint8_t>(sum);
                p[j + 1] = static_cast<uint8_t>(sum >> 8);
            }

            if (j < count)
            {
                p[j] = static_cast<uint8_t>(p[j] + pixel(j));
            }
        }
    }

    /// <summary>VFX_dithered_Gouraud_polygon and VFX_illuminate_polygon, which differ only in storing or adding.</summary>
    template <bool Add> void DitheredPolygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist)
    {
        PolyTarget target;

        if (!PolyPrologue(pane, target))
        {
            return;
        }

        static const int offsets[1] = {8};
        WalkPolygon<1>(
            target, vcnt, vlist, offsets, WalkOptions{Add},
            [&](uint8_t* row, const Edge<1>& left, const Edge<1>& right, bool, int32_t clippedTop, int32_t rowIndex)
            {
                // The dither goes on A for the rows at an even distance from the top vertex.
                const bool swapped = ((clippedTop + rowIndex) & 1) != 0;
                const int32_t ditherA = swapped ? 0 : ditherAmount;
                const int32_t ditherB = swapped ? ditherAmount : 0;
                DitheredSpan<Add>(row, left.X, right.X, left.A[0], right.A[0], target.XMax, ditherA, ditherB);
            });
    }
}

void VFX_flat_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist)
{
    PolyTarget target;

    if (!PolyPrologue(pane, target))
    {
        return;
    }

    if (vcnt <= 0)
    {
        return;
    }

    // Original behaviour: the asm builds a dword of the colour from (c + 0x8000) >> 16 and fills with it, so colours
    // above 255 leak their high bits into some pixels of the dword stores; the port fills with the low byte.
    const uint8_t color = static_cast<uint8_t>((static_cast<uint32_t>(vlist[0].c) + 0x8000) >> 16);
    static const int offsets[1] = {0};
    WalkPolygon<0>(target, vcnt, vlist, offsets, WalkOptions{true},
                   [&](uint8_t* row, const Edge<0>& left, const Edge<0>& right, bool, int32_t, int32_t)
                   {
                       int32_t xa = left.X;
                       int32_t xb = right.X;

                       if (!(xb > xa))
                       {
                           std::swap(xa, xb);
                       }

                       int32_t l, r, count, cut;

                       if (!ClipSpan(xa, xb, target.XMax, l, r, count, cut))
                       {
                           return;
                       }

                       std::memset(row + l, color, static_cast<size_t>(count));
                   });
}

void VFX_Gouraud_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist)
{
    PolyTarget target;

    if (!PolyPrologue(pane, target))
    {
        return;
    }

    static const int offsets[1] = {8};
    WalkPolygon<1>(target, vcnt, vlist, offsets, WalkOptions{true},
                   [&](uint8_t* row, const Edge<1>& left, const Edge<1>& right, bool, int32_t, int32_t)
                   {
                       int32_t xa = left.X;
                       int32_t xb = right.X;
                       int32_t ca = left.A[0];
                       int32_t cb = right.A[0];

                       if (!(xb > xa))
                       {
                           std::swap(xa, xb);
                           std::swap(ca, cb);
                       }

                       int32_t l = xa >> 16;

                       if (l > target.XMax)
                       {
                           return;
                       }

                       int32_t r = xb >> 16;

                       if (r < 0)
                       {
                           return;
                       }

                       int32_t count = r - l + 1;

                       if (count != 1)
                       {
                           spanSlope = ValueSlope(cb - ca, r - l);
                       }

                       const int32_t slope = spanSlope;
                       uint32_t c = static_cast<uint32_t>(ca);

                       if (l < 0)
                       {
                           const int32_t cut = -l;
                           count -= cut;
                           l = 0;
                           c += static_cast<uint32_t>(MulShift16(Shl16(cut), slope));
                       }

                       if (r > target.XMax)
                       {
                           count -= r - target.XMax;
                       }

                       // The asm keeps the colour as an 8-bit integer and a 16-bit fraction; bits above 23 never reach a pixel,
                       // so a 32-bit accumulator gives the same bytes.
                       uint8_t* p = row + l;

                       for (int32_t j = 0; j < count; ++j)
                       {
                           p[j] = static_cast<uint8_t>(c >> 16);
                           c += static_cast<uint32_t>(slope);
                       }
                   });
}

void VFX_dithered_Gouraud_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist)
{
    DitheredPolygon<false>(pane, ditherAmount, vcnt, vlist);
}

void VFX_translate_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, void* lookaside)
{
    PolyTarget target;

    if (!PolyPrologue(pane, target))
    {
        return;
    }

    const uint8_t* table = static_cast<const uint8_t*>(lookaside);
    static const int offsets[1] = {0};
    WalkPolygon<0>(target, vcnt, vlist, offsets, WalkOptions{true},
                   [&](uint8_t* row, const Edge<0>& left, const Edge<0>& right, bool, int32_t, int32_t)
                   {
                       int32_t xa = left.X;
                       int32_t xb = right.X;

                       if (!(xb > xa))
                       {
                           std::swap(xa, xb);
                       }

                       int32_t l = xa >> 16;

                       if (l > target.XMax)
                       {
                           return;
                       }

                       int32_t r = xb >> 16;

                       if (r < 0)
                       {
                           return;
                       }

                       if (r != l)
                       {
                           if (l < 0)
                           {
                               l = 0;
                           }

                           if (r > target.XMax)
                           {
                               r = target.XMax;
                           }
                       }

                       for (uint8_t* p = row + l; p <= row + r; ++p)
                       {
                           *p = table[*p];
                       }
                   });
}

void VFX_illuminate_polygon(PANE* pane, FIXED16 ditherAmount, int32_t vcnt, SCRNVERTEX* vlist)
{
    DitheredPolygon<true>(pane, ditherAmount, vcnt, vlist);
}

void VFX_map_lookaside(uint8_t* table)
{
    std::memcpy(mapLookaside, table, sizeof(mapLookaside));
}

void VFX_map_polygon(PANE* pane, int32_t vcnt, SCRNVERTEX* vlist, WINDOW* texture, uint32_t flags)
{
    PolyTarget target;

    if (!PolyPrologue(pane, target))
    {
        return;
    }

    const uint8_t* texels = texture->buffer;
    const int32_t texStride = texture->x_max + 1;
    const int64_t texSize = static_cast<int64_t>(texStride) * (texture->y_max + 1);
    const bool xlat = (flags & MP_XLAT) != 0;
    const bool transparent = (flags & MP_XP) != 0;

    // The texel step table (0x007a9b7c): the offset to add per pixel, indexed by the carries out of the u and v
    // fractions (u carry * 2 + v carry). Like the Gouraud slope it survives from span to span.
    static int32_t stepTable[4] = {};
    static int32_t duSpan = 0;
    static int32_t dvSpan = 0;

    static const int offsets[2] = {12, 16};
    WalkPolygon<2>(target, vcnt, vlist, offsets, WalkOptions{true},
                   [&](uint8_t* row, const Edge<2>& left, const Edge<2>& right, bool, int32_t, int32_t)
                   {
                       int32_t xa = left.X;
                       int32_t xb = right.X;
                       int32_t ua = left.A[0];
                       int32_t ub = right.A[0];
                       int32_t va = left.A[1];
                       int32_t vb = right.A[1];

                       if (!(xb > xa))
                       {
                           std::swap(xa, xb);
                           std::swap(ua, ub);
                           std::swap(va, vb);
                       }

                       int32_t l = xa >> 16;

                       if (l > target.XMax)
                       {
                           return;
                       }

                       int32_t r = xb >> 16;

                       if (r < 0)
                       {
                           return;
                       }

                       int32_t u = ua;
                       int32_t v = va;

                       if (r != l)
                       {
                           const int32_t n = r - l;
                           duSpan = ValueSlope(ub - ua, n);
                           // The whole texel steps of u and v, truncated towards zero, and the extra step a fraction's carry
                           // (or borrow, for a negative slope) adds.
                           int32_t uInt = static_cast<int16_t>(static_cast<uint32_t>(duSpan) >> 16);
                           int32_t uCarry = 1;

                           if (uInt < 0)
                           {
                               uCarry = -1;

                               if ((duSpan & 0xffff) != 0)
                               {
                                   ++uInt;
                               }
                           }

                           uCarry += uInt;
                           dvSpan = ValueSlope(vb - va, n);
                           int32_t vInt = static_cast<int32_t>(static_cast<uint32_t>(dvSpan) >> 16) & 0xffff;
                           int32_t vCarry = texStride;

                           if (vInt & 0x8000)
                           {
                               vCarry = -vCarry;

                               if ((dvSpan & 0xffff) != 0)
                               {
                                   ++vInt;
                               }
                           }

                           // Original behaviour: the row step is a 16-bit product (imul dx; cwde).
                           const int32_t rowStep = static_cast<int16_t>(
                               static_cast<uint16_t>(static_cast<int16_t>(texStride) * static_cast<int16_t>(vInt)));
                           stepTable[0] = uInt + rowStep;
                           stepTable[1] = stepTable[0] + vCarry;
                           stepTable[2] = uCarry + rowStep;
                           stepTable[3] = stepTable[2] + vCarry;

                           if (l < 0)
                           {
                               const int32_t cut = -l;
                               l = 0;
                               u += MulShift16(Shl16(cut), duSpan);
                               v += MulShift16(Shl16(cut), dvSpan);
                           }

                           if (r > target.XMax)
                           {
                               r = target.XMax;
                           }
                       }

                       int64_t offset = static_cast<int64_t>(static_cast<uint32_t>(v) >> 16) * texStride +
                                        (static_cast<uint32_t>(u) >> 16);
                       // The fractions: kept complemented for a negative slope, so that a borrow shows as a carry.
                       uint32_t uFrac = static_cast<uint32_t>(u);
                       uint32_t uStepFrac = static_cast<uint32_t>(duSpan);

                       if (duSpan < 0)
                       {
                           uStepFrac = static_cast<uint32_t>(-duSpan);
                           uFrac = ~uFrac;
                       }

                       uStepFrac <<= 16;
                       uFrac <<= 16;
                       uint32_t vFrac = static_cast<uint32_t>(v);
                       uint32_t vStepFrac = static_cast<uint32_t>(dvSpan);

                       if (dvSpan < 0)
                       {
                           vStepFrac = static_cast<uint32_t>(-dvSpan);
                           vFrac = ~vFrac;
                       }

                       vStepFrac <<= 16;
                       vFrac <<= 16;

                       uint8_t* p = row + l;

                       for (int32_t x = l; x <= r; ++x, ++p)
                       {
                           // Port fix: the asm reads wherever the texture coordinates point; the port skips texels outside it.
                           if (offset >= 0 && offset < texSize)
                           {
                               uint8_t texel = texels[offset];

                               if (xlat)
                               {
                                   texel = mapLookaside[texel];
                               }

                               if (!transparent || texel != 0xff)
                               {
                                   *p = texel;
                               }
                           }

                           const uint32_t uSum = uFrac + uStepFrac;
                           const uint32_t carryU = uSum < uFrac ? 1 : 0;
                           uFrac = uSum;
                           const uint32_t vSum = vFrac + vStepFrac;
                           const uint32_t carryV = vSum < vFrac ? 1 : 0;
                           vFrac = vSum;
                           offset += stepTable[carryU * 2 + carryV];
                       }
                   });
}
