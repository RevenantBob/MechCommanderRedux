#pragma once

/// <summary>The tactical map's fixed layout and the event types it handles (shared by its source files).</summary>
namespace MCTacmapLayout
{
    /// <summary>The event types of MCGuiEvent::Type the tactical map handles.</summary>
    inline constexpr int32_t EventLeftDown = 1;
    inline constexpr int32_t EventLeftUp = 4;
    inline constexpr int32_t EventMouseMove = 7;
    inline constexpr int32_t EventKeyUp = 8;
    inline constexpr int32_t EventResize = 0x12;
    inline constexpr int32_t EventTimer = 0x13;
    inline constexpr int32_t EventCallback = 0x14;
    inline constexpr int32_t EventZoomIn = 0x1a;
    inline constexpr int32_t EventZoomOut = 0x1b;

    /// <summary>The tactical map's centre in MFD pixels (the floats at 0x0078426c and 0x00784268).</summary>
    inline constexpr float MapCenterX = 71.0f;
    inline constexpr float MapCenterY = 99.0f;
    /// <summary>1/260: meters per pixel at zoom 1 from the map's diagonal (0x00784274).</summary>
    inline constexpr float DiagonalToPixels = 1.0f / 260.0f;
    /// <summary>1/130: the map picture's pixels per MFD pixel at zoom 1 (0x00784270).</summary>
    inline constexpr float PictureToPixels = 1.0f / 130.0f;
    /// <summary>cos 45 degrees (0x0077f0cc; 0x0077f0c4 holds its negative).</summary>
    inline constexpr float MapRotation = 0.70710677f;
    /// <summary>Half the map area's side, and the side (0x007849f8, 0x007849fc).</summary>
    inline constexpr float MapHalfSide = 65.0f;
    inline constexpr float MapPictureSide = 130.0f;
    /// <summary>The map area's top-left corner in MFD pixels (0x0077a538, 0x0078427c).</summary>
    inline constexpr float MapLeft = 6.0f;
    inline constexpr float MapTop = 34.0f;
}
