#include "stdafx.h"
#include "engine/cepoly.h"
#include "camera/camera.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

_window textureWindow{};

PolygonElement::PolygonElement(PolyElementData* _data, int32_t _depth) : Element(_depth)
{
    data = *_data;
}

auto PolygonElement::draw() -> void
{
    if (data.statusBar != 0)
    {
        AG_StatusBar(globalPane, data.vertices[0].x, data.vertices[0].y, data.vertices[1].x, data.vertices[1].y,
                     data.barColor, data.barPercent);
        return;
    }

    if (data.numVertices == 0)
    {
        return;
    }

    if (data.texture == nullptr)
    {
        if (data.fadeTable != nullptr)
        {
            VFX_translate_polygon(globalPane, data.numVertices, data.vertices, data.fadeTable);
            return;
        }

        VFX_Gouraud_polygon(globalPane, data.numVertices, data.vertices);
        return;
    }

    if (data.translate != 0 && data.fadeTable != nullptr)
    {
        VFX_translate_polygon(globalPane, data.numVertices, data.vertices, data.fadeTable);
        return;
    }

    if (data.textureMapOff == 0)
    {
        textureWindow.x_max = data.textureWidth - 1;
        textureWindow.y_max = data.textureHeight - 1;
        textureWindow.buffer = data.texture;
        textureWindow.Texture = data.textureHandle;

        if (data.fadeTable != nullptr)
        {
            VFX_map_lookaside(data.fadeTable);
            VFX_map_polygon(globalPane, data.numVertices, data.vertices, &textureWindow, 3);
            return;
        }

        VFX_map_polygon(globalPane, data.numVertices, data.vertices, &textureWindow, 2);
    }
}
