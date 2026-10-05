#pragma once

#include "engine/celement.h"

/// <summary>
/// One frame of a VFX shape table at a screen point: drawn as is, through a fade table, mirrored, or scaled to the
/// camera's zoom (<c>scaleDraw</c>).
/// </summary>
/// <remarks>
/// Original source: <c>engine\cevfx.cpp</c>, 0x3c bytes. The depth is -y (lower on screen draws later).
/// </remarks>
class VFXElement : public Element
{
public:
    /// <summary>
    /// Frame <paramref name="frame"/> (clamped to the table) of <paramref name="_shape"/> at
    /// (<paramref name="_x"/>, <paramref name="_y"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1ee0</remarks>
    VFXElement(uint8_t* _shape, int32_t _x, int32_t _y, int32_t frame, int _reverse, uint8_t* fadeTbl, int _noScaleDraw,
               int _scaleUp);

    /// <summary>The same at a float position (rounded down).</summary>
    /// <remarks>MCX.EXE @ 0x006b1f40</remarks>
    VFXElement(uint8_t* _shape, float _x, float _y, int32_t frame, int _reverse, uint8_t* fadeTbl, int _noScaleDraw,
               int _scaleUp);

    /// <summary>
    /// Copies the debug names to <see cref="CurrentVFX"/>/<see cref="CurrentVFX2"/>, then draws: through
    /// <c>scaleDraw</c> unless <see cref="noScaleDraw"/>, else directly (mirrored/transformed when
    /// <see cref="reverse"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b1fd0; slot 0</remarks>
    void draw() override;

    /// <summary>The shape table.</summary>
    uint8_t* shapeTable; // +0x0c
    /// <summary>The frame drawn.</summary>
    int32_t frameNum; // +0x10
    int32_t x;        // +0x14
    int32_t y;        // +0x18
    /// <summary>Nonzero to draw mirrored (passed to the transform draw).</summary>
    int reverse; // +0x1c
    /// <summary>The fade table the shape is translated through, or null.</summary>
    uint8_t* fadeTable; // +0x20
    /// <summary>Nonzero to draw at 1:1 instead of through <c>scaleDraw</c>.</summary>
    int noScaleDraw; // +0x24
    /// <summary>Passed to <c>scaleDraw</c> (which recomputes it from the camera's zoom).</summary>
    int scaleUp; // +0x28
    /// <summary>A name for crash reports, set by the creator (copied to <see cref="CurrentVFX"/> when drawn).</summary>
    char name[8]; // +0x2c
    /// <summary>A second name, copied to <see cref="CurrentVFX2"/>.</summary>
    char name2[8]; // +0x34
};

/// <summary>The name of the VFX element being drawn (for crash reports).</summary>
extern char CurrentVFX[8];
/// <summary>The second name of the VFX element being drawn.</summary>
extern char CurrentVFX2[8];
/// <summary>The size of <see cref="tempBuffer"/>.</summary>
inline constexpr size_t TEMP_BUFFER_SIZE = 0x1fa40;
/// <summary>The scratch buffer the scaled and transformed shape draws use.</summary>
extern std::array<uint8_t, TEMP_BUFFER_SIZE> tempBuffer;
