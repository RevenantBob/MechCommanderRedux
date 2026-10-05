#pragma once

#include "lib/cvmath.h"

class PacketFile;

/// <summary>A crater or footprint on the ground (0x14 bytes).</summary>
struct CraterData
{
    /// <summary>The crater shape (packet of the crater PAK), -1 for a free slot.</summary>
    int32_t craterShapeId; // +0x00
    /// <summary>Where it is in the world.</summary>
    vector_3d position; // +0x04
    /// <summary>The frame of the shape drawn (the shapes hold one frame per facing).</summary>
    int32_t rotation; // +0x10
};

/// <summary>
/// The craters and footprints left on the ground: a ring of <see cref="maxCraters"/> slots (the oldest is
/// overwritten), drawn under everything else with shapes from a PAK.
/// </summary>
/// <remarks>
/// Original source: <c>engine\crater.cpp</c>, 0x2c bytes. The PAK holds the full-size shapes in its first half
/// and the zoomed-out ones in its second half.
/// </remarks>
class CraterManager
{
public:
    /// <summary>
    /// Makes room for <paramref name="numCraters"/> craters and loads every shape of the PAK
    /// <paramref name="craterFileName"/> (from the sprite path, or the CD's). <paramref name="unused"/> is ignored.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b4000</remarks>
    int32_t init(int32_t numCraters, uint32_t unused, char* craterFileName);

    /// <summary>Frees the PAK, the shapes and the crater list.</summary>
    /// <remarks>MCX.EXE @ 0x006b4280</remarks>
    void destroy();

    /// <summary>
    /// Adds crater <paramref name="craterType"/> at <paramref name="position"/> facing <paramref name="rotation"/>,
    /// when the tile there is seen, isn't water or a road-like overlay, and isn't a bridge tile.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006b4360</remarks>
    int32_t addCrater(int32_t craterType, vector_3d& position, int32_t rotation);

    /// <summary>Nothing to update (returns 1).</summary>
    /// <remarks>MCX.EXE @ 0x006b4570</remarks>
    int32_t update();

    /// <summary>Adds a VFX element for every crater on screen, in a group at the bottom of the draw order.</summary>
    /// <remarks>MCX.EXE @ 0x006b4580</remarks>
    void render();

protected:
    /// <summary>Shape <paramref name="craterId"/>, loading it if it isn't; null for -1.</summary>
    /// <remarks>MCX.EXE @ 0x006b42f0</remarks>
    uint8_t* getCrater(int32_t craterId);

    /// <summary>Reads shape <paramref name="craterId"/> from the PAK (seeked to it) and registers it.</summary>
    void loadShape(int32_t craterId);

public:
    /// <summary>The crater PAK.</summary>
    PacketFile* craterFile; // +0x10
    /// <summary>The number of crater slots.</summary>
    int32_t maxCraters; // +0x14
    /// <summary>The slot the next crater goes in.</summary>
    int32_t currentCrater; // +0x18
    /// <summary>The slots.</summary>
    std::vector<CraterData> craterList; // +0x1c
    /// <summary>The number of shapes (packets).</summary>
    int32_t numCraterShapes; // +0x20
    /// <summary>Half of that: added to the shape id when the camera is zoomed out.</summary>
    int32_t numCraterTypes; // +0x24
    /// <summary>The shapes (numCraterShapes of them, at least 11; null until loaded).</summary>
    std::vector<std::unique_ptr<uint8_t[]>> craterShapes; // +0x28
};

/// <summary>The game's crater manager.</summary>
extern CraterManager* craterManager;
