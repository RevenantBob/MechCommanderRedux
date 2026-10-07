#pragma once

#include "engine/celement.h"

/// <summary>
/// What a <see cref="MCPolygonElement"/> draws: up to 6 screen vertices, filled in one colour, through a fade table,
/// texture-mapped from a bitmap, or (as a special case) a status bar.
/// </summary>
/// <remarks>Original source: <c>engine\cepoly.h</c>, 0xbc bytes.</remarks>
struct MCPolyElementData
{
    /// <summary>Clears the flags and pointers (the vertices are left alone).</summary>
    void Init()
    {
        NumVertices = 0;
        TextureMapOff = 0;
        StatusBar = 0;
        Texture = nullptr;
        TextureWidth = 0;
        TextureHeight = 0;
        FadeTable = nullptr;
        Translate = 0;
        TextureHandle = nullptr;
    }

    MCPolyElementData() { Init(); }

    /// <summary>The number of vertices used (0 draws nothing).</summary>
    int32_t NumVertices;
    /// <summary>The vertices, in pane coordinates (u, v are texture coordinates when mapped).</summary>
    MCScreenVertex Vertices[6];
    /// <summary>When nonzero a textured polygon isn't drawn.</summary>
    int32_t TextureMapOff;
    /// <summary>When set (with a fade table) a textured polygon is drawn translated instead.</summary>
    int32_t Translate;
    /// <summary>When set the element draws a status bar from vertex 0 to vertex 1 instead.</summary>
    int32_t StatusBar;
    /// <summary>The status bar's second argument (the last one of <c>AG_StatusBar</c>).</summary>
    int32_t BarPercent;
    /// <summary>The status bar's colour.</summary>
    int32_t BarColor;
    /// <summary>The texture's pixels, or null for a flat polygon (colour in the vertices).</summary>
    uint8_t* Texture;
    /// <summary>The texture's width.</summary>
    int32_t TextureWidth;
    /// <summary>The texture's height.</summary>
    int32_t TextureHeight;
    /// <summary>The fade table the polygon is translated through, or null.</summary>
    uint8_t* FadeTable;
    /// <summary>Port: the texture's handle (<see cref="MCTexture"/>, the renderers read the texture through it).</summary>
    MCTexture* TextureHandle;
};

/// <summary>A polygon (see <see cref="MCPolyElementData"/>).</summary>
/// <remarks>Original source: <c>engine\cepoly.cpp</c>, 0xc8 bytes.</remarks>
class MCPolygonElement : public MCElement
{
public:
    /// <summary>A copy of <paramref name="data"/> at depth <paramref name="depth"/>.</summary>
    MCPolygonElement(MCPolyElementData* data, int32_t depth);

    void Draw() override;

    /// <summary>What to draw.</summary>
    MCPolyElementData Data;
};

/// <summary>The window <see cref="MCPolygonElement::Draw"/> hands the texture mapper (the texture, its size - 1).</summary>
extern MCWindow TextureWindow;
