#include "stdafx.h"
#include "engine/MCPolygonElement.h"
#include "camera/MCCamera.h"
#include "vfx/MCVfxFunctions.h"

MCPolygonElement::MCPolygonElement(const MCPolyElementData& data, int32_t depth) : MCElement(depth), Data(data)
{
}

auto MCPolygonElement::Draw() -> void
{
    if (Data.StatusBar)
    {
        AGStatusBar(GlobalPane, Data.Vertices[0].X, Data.Vertices[0].Y, Data.Vertices[1].X, Data.Vertices[1].Y,
                    Data.BarColor, Data.BarPercent);
        return;
    }

    if (Data.NumVertices == 0)
    {
        return;
    }

    const std::span<MCScreenVertex> vertices(Data.Vertices.data(), static_cast<size_t>(Data.NumVertices));

    if (Data.Texture == nullptr)
    {
        if (Data.FadeTable != nullptr)
        {
            VfxTranslatePolygon(GlobalPane, vertices, Data.FadeTable);
            return;
        }

        VfxGouraudPolygon(GlobalPane, vertices);
        return;
    }

    if (Data.Translate && Data.FadeTable != nullptr)
    {
        VfxTranslatePolygon(GlobalPane, vertices, Data.FadeTable);
        return;
    }

    if (!Data.TextureMapOff)
    {
        // The texture mapper reads the texture as a window (its size - 1).
        MCWindow texture{};
        texture.XMax = Data.TextureWidth - 1;
        texture.YMax = Data.TextureHeight - 1;
        texture.Buffer = Data.Texture;
        texture.Texture = Data.TextureHandle;

        if (Data.FadeTable != nullptr)
        {
            VfxMapLookaside(Data.FadeTable);
            VfxMapPolygon(GlobalPane, vertices, &texture, VfxMapXlat | VfxMapTransparent);
            return;
        }

        VfxMapPolygon(GlobalPane, vertices, &texture, VfxMapTransparent);
    }
}
