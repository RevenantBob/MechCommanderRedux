#pragma once

#include "engine/celement.h"

/// <summary>
/// One frame of a VFX shape table at a screen point: drawn as is, through a fade table, mirrored, or scaled to the
/// camera's zoom (<c>scaleDraw</c>).
/// </summary>
/// <remarks>
/// Original source: <c>engine\cevfx.cpp</c>, 0x3c bytes. The depth is -y (lower on screen draws later).
/// </remarks>
class MCVfxElement : public MCElement
{
public:
    /// <summary>
    /// Frame <paramref name="frame"/> (clamped to the table) of <paramref name="shape"/> at
    /// (<paramref name="x"/>, <paramref name="y"/>).
    /// </summary>
    MCVfxElement(uint8_t* shape, int32_t x, int32_t y, int32_t frame, int reverse, uint8_t* fadeTbl, int noScaleDraw,
                 int scaleUp);

    /// <summary>The same at a float position (rounded down).</summary>
    MCVfxElement(uint8_t* shape, float x, float y, int32_t frame, int reverse, uint8_t* fadeTbl, int noScaleDraw,
                 int scaleUp);

    /// <summary>
    /// Copies the debug names to <see cref="CurrentVfx"/>/<see cref="CurrentVfx2"/>, then draws: through
    /// <c>scaleDraw</c> unless <see cref="NoScaleDraw"/>, else directly (mirrored/transformed when
    /// <see cref="Reverse"/>).
    /// </summary>
    void Draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* ShapeTable;
    /// <summary>The frame drawn.</summary>
    int32_t FrameNum;
    int32_t X;
    int32_t Y;
    /// <summary>Nonzero to draw mirrored (passed to the transform draw).</summary>
    int Reverse;
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* FadeTable;
    /// <summary>Nonzero to draw at 1:1 instead of through <c>scaleDraw</c>.</summary>
    int NoScaleDraw;
    /// <summary>Passed to <c>scaleDraw</c> (which recomputes it from the camera's zoom).</summary>
    int ScaleUp;
    /// <summary>A name for crash reports, set by the creator (copied to <see cref="CurrentVfx"/> when drawn).</summary>
    char Name[8];
    /// <summary>A second name, copied to <see cref="CurrentVfx2"/>.</summary>
    char Name2[8];
};

/// <summary>The name of the VFX element being drawn (for crash reports).</summary>
extern char CurrentVfx[8];
/// <summary>The second name of the VFX element being drawn.</summary>
extern char CurrentVfx2[8];
/// <summary>The size of <see cref="TempBuffer"/>.</summary>
inline constexpr size_t TEMP_BUFFER_SIZE = 0x1fa40;
/// <summary>The scratch buffer the scaled and transformed shape draws use.</summary>
extern std::array<uint8_t, TEMP_BUFFER_SIZE> TempBuffer;
