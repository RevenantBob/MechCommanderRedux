#include "stdafx.h"
#include "engine/cepoly.h"
#include "camera/camera.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

MCWindow TextureWindow{};

MCPolygonElement::MCPolygonElement(MCPolyElementData* data, int32_t depth) : MCElement(depth)
{
    Data = *data;
}

auto MCPolygonElement::Draw() -> void
{
    if (Data.StatusBar != 0)
    {
        AGStatusBar(GlobalPane, Data.Vertices[0].X, Data.Vertices[0].Y, Data.Vertices[1].X, Data.Vertices[1].Y,
                    Data.BarColor, Data.BarPercent);
        return;
    }

    if (Data.NumVertices == 0)
    {
        return;
    }

    if (Data.Texture == nullptr)
    {
        if (Data.FadeTable != nullptr)
        {
            VfxTranslatePolygon(GlobalPane, Data.NumVertices, Data.Vertices, Data.FadeTable);
            return;
        }

        VfxGouraudPolygon(GlobalPane, Data.NumVertices, Data.Vertices);
        return;
    }

    if (Data.Translate != 0 && Data.FadeTable != nullptr)
    {
        VfxTranslatePolygon(GlobalPane, Data.NumVertices, Data.Vertices, Data.FadeTable);
        return;
    }

    if (Data.TextureMapOff == 0)
    {
        TextureWindow.XMax = Data.TextureWidth - 1;
        TextureWindow.YMax = Data.TextureHeight - 1;
        TextureWindow.Buffer = Data.Texture;
        TextureWindow.Texture = Data.TextureHandle;

        if (Data.FadeTable != nullptr)
        {
            VfxMapLookaside(Data.FadeTable);
            VfxMapPolygon(GlobalPane, Data.NumVertices, Data.Vertices, &TextureWindow, 3);
            return;
        }

        VfxMapPolygon(GlobalPane, Data.NumVertices, Data.Vertices, &TextureWindow, 2);
    }
}
