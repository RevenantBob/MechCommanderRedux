#pragma once

#pragma pack(push, 1)
/// <summary>
/// The 6-byte header in front of the VFX shape table of a gesture packet (mech sprite PAKs).
/// </summary>
struct SpriteGestureHeader
{
    /// <summary>Overwritten with the gesture number when loaded.</summary>
    int16_t gestureNum; // +0x00
    /// <summary>The number of frames (must equal the shape table's count).</summary>
    uint16_t numFrames; // +0x02
    /// <summary>Zero in every retail gesture packet; never read.</summary>
    uint16_t padding; // +0x04 // Fixed layout: sprite gesture packet
};
#pragma pack(pop)
static_assert(sizeof(SpriteGestureHeader) == 6);

/// <summary>A gesture's frames: a packet with a <see cref="SpriteGestureHeader"/> and its shape table.</summary>
/// <remarks>Original source: <c>sprite\sprite.cpp</c>, 8 bytes.</remarks>
class SpriteGesture
{
public:
    /// <summary>Forgets the data.</summary>
    /// <remarks>MCX.EXE @ 0x00641610</remarks>
    void destroy();

    /// <summary>
    /// Takes the packet <paramref name="data"/>, checks the header's frame count against the shape table and stamps
    /// <paramref name="gestureNum"/> into the header.
    /// </summary>
    /// <returns>0, or 0xBEEF0005 (negative) on a count mismatch.</returns>
    /// <remarks>MCX.EXE @ 0x00641620</remarks>
    int32_t init(uint8_t* data, int gestureNum);

    /// <summary>The packet (its header).</summary>
    uint8_t* gestureData; // +0x00
    /// <summary>The shape table after the header.</summary>
    uint8_t* shapeTable; // +0x04
};
