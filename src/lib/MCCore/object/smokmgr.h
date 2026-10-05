#pragma once

#include "platform/MCBlockStore.h"

#include "lib/cvmath.h"

class FitIniFile;

/// <summary>One puff of a <see cref="Smoke"/>: a VFX shape drifting with its own velocity and animation.</summary>
/// <remarks>Original source: <c>object\smokmgr.cpp</c> / <c>object\smoke.cpp</c>, 0x38 bytes (runtime only; 0x40 on x64, so sizes use sizeof).</remarks>
struct SmokeSphere
{
    /// <summary>The smoke shape (the type's VFX shape file).</summary>
    uint8_t* shape = nullptr; // +0x0
    /// <summary>World position.</summary>
    vector_3d position; // +0x4
    /// <summary>World units per second.</summary>
    vector_3d velocity; // +0x10
    /// <summary>Nonzero while the puff is alive and visible.</summary>
    int32_t active = 0; // +0x1c
    /// <summary>Screen position (from the last visibility check).</summary>
    float screenX = 0; // +0x20
    float screenY = 0; // +0x24
    /// <summary>The shape frame drawn.</summary>
    int32_t frame = 0; // +0x28
    /// <summary>floor(frameTime * frameRate) at the last frame advance.</summary>
    int32_t frameCount = 0; // +0x2c
    /// <summary>Seconds the puff has been animating.</summary>
    float frameTime = 0; // +0x30
    /// <summary>Set once the puff has touched the ground (it then spreads sideways).</summary>
    int32_t onGround = 0; // +0x34
};

/// <summary>
/// Owns the smoke spheres (and the smoke shapes) and hands out blocks of spheres, up to the number the scenario
/// sized it for.
/// </summary>
/// <remarks>
/// Original source: <c>object\smokmgr.cpp</c>, 0x24 bytes (with the heaps for <see cref="Smoke"/> objects and
/// spheres). Made by the scenario; set up from the "Smoke Manager" block of the scenario's FIT.
/// </remarks>
class SmokeManager
{
public:
    /// <summary>
    /// Reads the "Smoke Manager" block, makes the two heaps and preloads the smoke object types.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00694a80</remarks>
    int32_t init(FitIniFile* scenarioFile);
    /// <summary>Destroys both heaps.</summary>
    /// <remarks>MCX.EXE @ 0x00694c30</remarks>
    void destroy();
    /// <summary>
    /// Allocates <paramref name="numSpheres"/> spheres, or sets it to 0 and returns null when not that many are left.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00694c80</remarks>
    SmokeSphere* getSpheres(int32_t& numSpheres);
    /// <summary>Gives back a block of <paramref name="numSpheres"/> spheres.</summary>
    /// <remarks>MCX.EXE @ 0x00694cd0</remarks>
    void freeSpheres(SmokeSphere* spheres, int32_t numSpheres);

    /// <summary>FIT "NumSmokeTypes".</summary>
    int32_t numSmokeTypes = 0; // +0x0
    /// <summary>FIT "MaxSmokesPerType".</summary>
    int32_t maxSmokesPerType = 0; // +0x4
    /// <summary>The spheres handed out and the smoke shapes (the original's sphere heap).</summary>
    MCBlockStore sphereBlocks; // +0x1c
    /// <summary>Spheres not handed out yet.</summary>
    int32_t numFreeSpheres = 0; // +0x20
};

/// <summary>How many smoke spheres the manager hands out (set by the scenario).</summary>
extern int32_t totalSmokeSpheres;
/// <summary>Bytes of smoke shapes (set by the scenario; unused since the sphere heap is gone).</summary>
extern int32_t totalSmokeShapeSize;
