#pragma once

#include "lib/MCVector2D.h"
#include "lib/MCVector3D.h"
#include "camera/MCCameraList.h"
#include "mission/MCScenario.h"

class MCCamera;

/// <summary>Set (a debugging switch) to draw every static object's extent radius as an ellipse.</summary>
extern bool DrawExtents;

/// <summary>
/// Where <paramref name="position"/> lands on <paramref name="camera"/>'s screen: its projection onto the terrain,
/// from the camera's upper left, plus half the view.
/// </summary>
MCVector2D ProjectToScreen(const MCVector3D& position, const MCCamera& camera);

/// <summary>
/// Draws the debugging ellipse of an object's extent at <paramref name="position"/> with radii
/// <paramref name="size"/>, as an overlay that follows the object through the zoom.
/// </summary>
void DrawExtentEllipse(const MCVector3D& position, MCVector2D size);
