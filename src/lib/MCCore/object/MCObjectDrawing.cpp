#include "stdafx.h"
#include "object/MCObjectDrawing.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "camera/MCViewWindow.h"
#include "color/MCPalette.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCEllipseElement.h"
#include "mission/MCScenario.h"
#include "terrain/MCTerrain.h"

bool DrawExtents = false;

MCCamera* ActiveMainCamera()
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);
    return camera != nullptr && camera->Active != 0 ? camera : nullptr;
}

MCVector2D ProjectToScreen(const MCVector3D& position, const MCCamera& camera)
{
    MCVector2D screen100;
    MCVector2D screen50;

    if (Terrain() != nullptr)
    {
        MCTerrain::ProjectTerrain(position, screen100, screen50);
    }

    MCVector2D screen;
    float screenY;

    if (MCCamera::CameraScale == 1)
    {
        screen.X = (screen50.X - camera.ScreenUL50.X) + camera.HalfWidth;
        screenY = screen50.Y - camera.ScreenUL50.Y;
    }
    else
    {
        screen.X = (screen100.X - camera.ScreenUL.X) + camera.HalfWidth;
        screenY = screen100.Y - camera.ScreenUL.Y;
    }

    screen.Y = screenY + camera.HalfHeight;
    return screen;
}

uint8_t* HazePaletteFor(int32_t numVisible)
{
    const int32_t hazeLevel = Eye->HazeLevel;
    int32_t level;

    if (hazeLevel < 0 && 0 < Eye->HazeInc * numVisible + hazeLevel)
    {
        level = 0;
    }
    else
    {
        level = hazeLevel + Eye->HazeInc * numVisible;
    }

    return GamePalette()->GetHazePalette(level);
}

void DrawExtentEllipse(const MCVector3D& position, MCVector2D size)
{
    if (MCCamera::CameraScale == 1)
    {
        size.X *= 0.5f;
        size.Y *= 0.5f;
    }

    const float scale = MCCamera::CameraScale != 1 ? 1.0f : 0.5f;
    const float sx = (position.X - Eye->Position.X) * scale;
    const float sy = (position.Y - Eye->Position.Y) * scale;
    MCVector2D center;
    center.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
    center.Y = ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * (position.Z - Eye->Position.Z);
    ElementList()->OpenGroup(-50000, true);
    // Port: an overlay, on the screen over the view: it follows the object through the zoom.
    center = MCOverlayPoint(center);
    size.X *= MCOverlay.ScaleX;
    size.Y *= MCOverlay.ScaleY;
    ElementList()->Add(ElementList()->Make<MCEllipseElement>(center, size, 0xfe, -50000));
}

uint8_t* SensorBlipShape(float tonnage)
{
    if (50.0f < tonnage)
    {
        return Scenario()->SensorContactShape(0);
    }

    if (35.0f < tonnage)
    {
        return Scenario()->SensorContactShape(2);
    }

    return Scenario()->SensorContactShape(4);
}
