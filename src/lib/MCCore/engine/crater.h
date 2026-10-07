#pragma once

#include "lib/cvmath.h"

class MCPacketFile;

/// <summary>A crater or footprint on the ground (0x14 bytes).</summary>
struct MCCraterData
{
    /// <summary>The crater shape (packet of the crater PAK), -1 for a free slot.</summary>
    int32_t CraterShapeId;
    /// <summary>Where it is in the world.</summary>
    MCVector3D Position;
    /// <summary>The frame of the shape drawn (the shapes hold one frame per facing).</summary>
    int32_t Rotation;
};

/// <summary>
/// The craters and footprints left on the ground: a ring of <see cref="MaxCraters"/> slots (the oldest is
/// overwritten), drawn under everything else with shapes from a PAK.
/// </summary>
/// <remarks>
/// Original source: <c>engine\crater.cpp</c>, 0x2c bytes. The PAK holds the full-size shapes in its first half
/// and the zoomed-out ones in its second half.
/// </remarks>
class MCCraterManager
{
public:
    /// <summary>
    /// Makes room for <paramref name="numCraters"/> craters and loads every shape of the PAK
    /// <paramref name="craterFileName"/> (from the sprite path, or the CD's). <paramref name="unused"/> is ignored.
    /// </summary>
    int32_t Init(int32_t numCraters, uint32_t unused, char* craterFileName);

    /// <summary>Frees the PAK, the shapes and the crater list.</summary>
    void Destroy();

    /// <summary>
    /// Adds crater <paramref name="craterType"/> at <paramref name="position"/> facing <paramref name="rotation"/>,
    /// when the tile there is seen, isn't water or a road-like overlay, and isn't a bridge tile.
    /// </summary>
    int32_t AddCrater(int32_t craterType, MCVector3D& position, int32_t rotation);

    /// <summary>Nothing to update (returns 1).</summary>
    int32_t Update();

    /// <summary>Adds a VFX element for every crater on screen, in a group at the bottom of the draw order.</summary>
    void Render();

protected:
    /// <summary>Shape <paramref name="craterId"/>, loading it if it isn't; null for -1.</summary>
    uint8_t* GetCrater(int32_t craterId);

    /// <summary>Reads shape <paramref name="craterId"/> from the PAK (seeked to it) and registers it.</summary>
    void LoadShape(int32_t craterId);

public:
    /// <summary>The crater PAK.</summary>
    MCPacketFile* CraterFile;
    /// <summary>The number of crater slots.</summary>
    int32_t MaxCraters;
    /// <summary>The slot the next crater goes in.</summary>
    int32_t CurrentCrater;
    /// <summary>The slots.</summary>
    std::vector<MCCraterData> CraterList;
    /// <summary>The number of shapes (packets).</summary>
    int32_t NumCraterShapes;
    /// <summary>Half of that: added to the shape id when the camera is zoomed out.</summary>
    int32_t NumCraterTypes;
    /// <summary>The shapes (numCraterShapes of them, at least 11; null until loaded).</summary>
    std::vector<std::unique_ptr<uint8_t[]>> CraterShapes;
};

/// <summary>The game's crater manager.</summary>
extern MCCraterManager* CraterManager;
