#pragma once

#include "engine/celement.h"

/// <summary>
/// What a <see cref="PolygonElement"/> draws: up to 6 screen vertices, filled in one colour, through a fade table,
/// texture-mapped from a bitmap, or (as a special case) a status bar.
/// </summary>
/// <remarks>Original source: <c>engine\cepoly.h</c>, 0xbc bytes.</remarks>
struct PolyElementData
{
    /// <summary>Clears the flags and pointers (the vertices are left alone).</summary>
    /// <remarks>MCX.EXE @ 0x00654610</remarks>
    void init()
    {
        numVertices = 0;
        textureMapOff = 0;
        unknown98 = 0;
        statusBar = 0;
        texture = nullptr;
        textureWidth = 0;
        textureHeight = 0;
        fadeTable = nullptr;
        translate = 0;
    }

    PolyElementData() { init(); }

    /// <summary>The number of vertices used (0 draws nothing).</summary>
    int32_t numVertices; // +0x00
    /// <summary>The vertices, in pane coordinates (u, v are texture coordinates when mapped).</summary>
    SCRNVERTEX vertices[6]; // +0x04
    /// <summary>When nonzero a textured polygon isn't drawn.</summary>
    int32_t textureMapOff; // +0x94
    /// <summary>Cleared by <see cref="init"/>; never read in MCX.EXE.</summary>
    int32_t unknown98; // +0x98
    /// <summary>When set (with a fade table) a textured polygon is drawn translated instead.</summary>
    int32_t translate; // +0x9c
    /// <summary>When set the element draws a status bar from vertex 0 to vertex 1 instead.</summary>
    int32_t statusBar; // +0xa0
    /// <summary>The status bar's second argument (the last one of <c>AG_StatusBar</c>).</summary>
    int32_t barPercent; // +0xa4
    /// <summary>The status bar's colour.</summary>
    int32_t barColor; // +0xa8
    /// <summary>The texture's pixels, or null for a flat polygon (colour in the vertices).</summary>
    uint8_t* texture; // +0xac
    /// <summary>The texture's width.</summary>
    int32_t textureWidth; // +0xb0
    /// <summary>The texture's height.</summary>
    int32_t textureHeight; // +0xb4
    /// <summary>The fade table the polygon is translated through, or null.</summary>
    uint8_t* fadeTable; // +0xb8
};

/// <summary>A polygon (see <see cref="PolyElementData"/>).</summary>
/// <remarks>Original source: <c>engine\cepoly.cpp</c>, 0xc8 bytes.</remarks>
class PolygonElement : public Element
{
public:
    /// <summary>A copy of <paramref name="_data"/> at depth <paramref name="_depth"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006b1d40</remarks>
    PolygonElement(PolyElementData* _data, int32_t _depth);

    /// <remarks>MCX.EXE @ 0x006b1db0; slot 0</remarks>
    void draw() override;

    /// <summary>What to draw.</summary>
    PolyElementData data; // +0x0c
};

/// <summary>The window <see cref="PolygonElement::draw"/> hands the texture mapper (the texture, its size - 1).</summary>
extern _window textureWindow;
