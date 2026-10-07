#pragma once

#pragma pack(push, 1)
/// <summary>
/// The 6-byte header in front of the VFX shape table of a gesture packet (mech sprite PAKs).
/// </summary>
struct MCSpriteGestureHeader
{
    /// <summary>Overwritten with the gesture number when loaded.</summary>
    int16_t GestureNum;
    /// <summary>The number of frames (must equal the shape table's count).</summary>
    uint16_t NumFrames;
    /// <summary>Zero in every retail gesture packet; never read.</summary>
    uint16_t Padding; // Fixed layout: sprite gesture packet
};
#pragma pack(pop)
static_assert(sizeof(MCSpriteGestureHeader) == 6);

/// <summary>A gesture's frames: a packet with a <see cref="MCSpriteGestureHeader"/> and its shape table.</summary>
/// <remarks>Original source: <c>sprite\sprite.cpp</c>, 8 bytes.</remarks>
class MCSpriteGesture
{
public:
    /// <summary>Forgets the data.</summary>
    void Destroy();

    /// <summary>
    /// Takes the packet <paramref name="data"/>, checks the header's frame count against the shape table and stamps
    /// <paramref name="gestureNum"/> into the header.
    /// </summary>
    /// <returns>0, or 0xBEEF0005 (negative) on a count mismatch.</returns>
    int32_t Init(uint8_t* data, int gestureNum);

    /// <summary>The packet (its header).</summary>
    uint8_t* GestureData;
    /// <summary>The shape table after the header.</summary>
    uint8_t* ShapeTable;
};
