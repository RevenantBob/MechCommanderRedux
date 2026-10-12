#pragma once

class MCGameObject;

/// <summary>
/// The facing of <paramref name="obj"/> in degrees from the world's x axis: positive to the left, negative to the
/// right (from its frame's i axis).
/// </summary>
double MCActorFacing(MCGameObject& obj);

/// <summary>A shape frame's box: the top-left corner's offset from the hot spot, and the size.</summary>
struct MCFrameBounds
{
    float MinX = 0.0f;
    float MinY = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

/// <summary>The box of frame <paramref name="frame"/> of <paramref name="shapeTable"/> (VFX_shape_minxy, _resolution).</summary>
MCFrameBounds MCShapeFrameBounds(std::span<const uint8_t> shapeTable, int32_t frame);

/// <summary><paramref name="frame"/> limited to the frames of <paramref name="shapeTable"/> (from 0 to the last).</summary>
int32_t MCClampShapeFrame(std::span<const uint8_t> shapeTable, int32_t frame);

/// <summary>
/// Grows the box (<paramref name="minX"/>, <paramref name="minY"/>, <paramref name="width"/>,
/// <paramref name="height"/>) to take in <paramref name="bounds"/>: each value only moves outward.
/// </summary>
void MCGrowBounds(const MCFrameBounds& bounds, float& minX, float& minY, float& width, float& height);
