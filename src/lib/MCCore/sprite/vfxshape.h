#pragma once

class MCAppearanceType;

/// <summary>
/// A shape table loaded into the sprite manager's cache: one packet of a sprite PAK, kept in an LRU list and freed
/// when the cache needs room or its type goes.
/// </summary>
/// <remarks>
/// Original source: <c>sprite\vfxshape.cpp</c>, 0x14 bytes (allocated from the sprite manager's data heap). A
/// packet is either a VFX shape table (it starts with the "1.10" version tag) or a 6-byte
/// <c>SpriteGestureHeader</c> followed by one.
/// </remarks>
class MCShape
{
public:
    /// <summary>Tells the owner type the shape goes, then frees the packet.</summary>
    void Destroy();

    /// <summary>
    /// Takes the packet <paramref name="shapeData"/> (<paramref name="dataSize"/> bytes) for
    /// <paramref name="owner"/>, checking its shape table.
    /// </summary>
    /// <returns>0; -1 for an empty table, -3 when the first frame lies past the packet, -4 when the offset table
    /// doesn't match the frame count.</returns>
    int32_t Init(uint8_t* shapeData, MCAppearanceType* owner, int32_t dataSize);

    /// <summary>The VFX shape table.</summary>
    uint8_t* FrameList;
    /// <summary>The packet when it has a gesture header (the one to free), else null.</summary>
    uint8_t* StupidHeader;
    /// <summary>The turn the shape was last used (the LRU key).</summary>
    int32_t LastTurnUsed;
    /// <summary>The next shape in the sprite manager's list.</summary>
    MCShape* Next;
    /// <summary>The type that uses the shape (told by <c>removeShape</c> when it goes).</summary>
    MCAppearanceType* Owner;
};
