#pragma once

#include "platform/MCRenderer.h"

/// <summary>
/// The software renderer: the original's pixel loops (from vfx/*.cpp), drawing into the windows' memory. It is the
/// renderer of every window today, the headless renderer of the tests, and the reference a hardware renderer is
/// compared with.
/// </summary>
/// <remarks>
/// Defined in MCSoftwareRenderer.cpp (fills, copies, pixels, lines, ellipses, status bars, glyphs, sprites),
/// MCSoftwareRendererShapes.cpp (run-length shapes, fast shapes, tiles) and MCSoftwareRendererPolygons.cpp
/// (polygons and mapped quadrilaterals).
/// </remarks>
class MCSoftwareRenderer final : public MCRenderer
{
public:
    /// <summary>The one software renderer.</summary>
    static MCSoftwareRenderer& Instance();

    void Clear(_window* target, const MCRect& rect, uint8_t color) override;
    void Hash(_window* target, const MCRect& rect, uint8_t color) override;
    void Copy(_window* target, const MCCopyCommand& command) override;
    void AlphaBlit(_window* target, const MCAlphaBlitCommand& command) override;
    void Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count) override;
    void Pixel(_window* target, int32_t x, int32_t y, uint8_t color) override;
    void Shape(_window* target, const MCShapeCommand& command) override;
    void FastShape(_window* target, const MCFastShapeCommand& command) override;
    void Tile(_window* target, const MCTileCommand& command) override;
    void Polygon(_window* target, const MCPolygonCommand& command) override;
    void MapQuad(_window* target, const MCMapQuadCommand& command) override;
    void Line(_window* target, const MCLineCommand& command) override;
    void Ellipse(_window* target, const MCEllipseCommand& command) override;
    void StatusBar(_window* target, const MCStatusBarCommand& command) override;
    void Glyph(_window* target, const MCGlyphCommand& command) override;

protected:
    /// <summary>Nothing to do: every draw reads AlphaTable as it goes.</summary>
    void OnAlphaTableChanged() override {}

    /// <summary>Nothing to do: shapes are read straight from the game's memory.</summary>
    void OnShapesForgotten(const void*, size_t) override {}

private:
    /// <summary>
    /// The span slope the last Gouraud or dithered span computed (0x007a9b54 in MCX.EXE): a one-pixel span reuses it
    /// (without effect on its pixel).
    /// </summary>
    int32_t _SpanSlope = 0;
    /// <summary>VFX_map_polygon's texel step table and span slopes (0x007a9b7c..): they too survive between spans.</summary>
    int32_t _MapSteps[4] = {};
    int32_t _MapDu = 0;
    int32_t _MapDv = 0;
    /// <summary>VFX_shape_transform's texel step table (0x007a9aac), kept between calls as the asm's global was.</summary>
    int32_t _QuadSteps[4] = {};
};
