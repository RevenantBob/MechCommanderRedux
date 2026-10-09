#pragma once

#include "lib/MCVector2D.h"
#include "lib/MCVector3D.h"

class MCCamera;

/// <summary>Set (a debugging switch) to draw every static object's extent radius as an ellipse.</summary>
extern bool DrawExtents;

/// <summary>The main camera (id 1) when it exists and is active, else null.</summary>
MCCamera* ActiveMainCamera();

/// <summary>
/// Where <paramref name="position"/> lands on <paramref name="camera"/>'s screen: its projection onto the terrain at
/// the full or half zoom (by the camera's scale), from the camera's upper left, plus half the view.
/// </summary>
MCVector2D ProjectToScreen(const MCVector3D& position, const MCCamera& camera);

/// <summary>
/// The haze palette of a static object of which <paramref name="numVisible"/> corners the home team sees: the main
/// camera's haze level, lifted by its haze step for each seen corner (clamped at 0 from below zero).
/// </summary>
uint8_t* HazePaletteFor(int32_t numVisible);

/// <summary>
/// Draws the debugging ellipse of an object's extent at <paramref name="position"/> with radii
/// <paramref name="size"/> (halved at the half zoom), as an overlay that follows the object through the zoom.
/// </summary>
void DrawExtentEllipse(const MCVector3D& position, MCVector2D size);

/// <summary>
/// The sensor blip shape a contact of <paramref name="tonnage"/> shows: the large one over 50 tons, the medium one over
/// 35, else the small one (null when the scenario has none).
/// </summary>
uint8_t* SensorBlipShape(float tonnage);
