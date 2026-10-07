#pragma once

#include "platform/MCBlockStore.h"

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"

class MCFitIniFile;

/// <summary>One puff of a <see cref="MCSmoke"/>: a VFX shape drifting with its own velocity and animation.</summary>
/// <remarks>Original source: <c>object\smokmgr.cpp</c> / <c>object\smoke.cpp</c>, 0x38 bytes (runtime only; 0x40 on x64, so sizes use sizeof).</remarks>
struct MCSmokeSphere
{
    /// <summary>The smoke shape (the type's VFX shape file).</summary>
    uint8_t* Shape = nullptr;
    /// <summary>World position.</summary>
    MCVector3D Position;
    /// <summary>World units per second.</summary>
    MCVector3D Velocity;
    /// <summary>Nonzero while the puff is alive and visible.</summary>
    int32_t Active = 0;
    /// <summary>Screen position (from the last visibility check).</summary>
    float ScreenX = 0;
    float ScreenY = 0;
    /// <summary>The shape frame drawn.</summary>
    int32_t Frame = 0;
    /// <summary>floor(frameTime * frameRate) at the last frame advance.</summary>
    int32_t FrameCount = 0;
    /// <summary>Seconds the puff has been animating.</summary>
    float FrameTime = 0;
    /// <summary>Set once the puff has touched the ground (it then spreads sideways).</summary>
    int32_t OnGround = 0;
};

/// <summary>
/// Owns the smoke spheres (and the smoke shapes) and hands out blocks of spheres, up to the number the scenario
/// sized it for.
/// </summary>
/// <remarks>
/// Original source: <c>object\smokmgr.cpp</c>, 0x24 bytes (with the heaps for <see cref="MCSmoke"/> objects and
/// spheres). Made by the scenario; set up from the "Smoke Manager" block of the scenario's FIT.
/// </remarks>
class MCSmokeManager
{
public:
    /// <summary>
    /// Reads the "Smoke Manager" block, makes the two heaps and preloads the smoke object types.
    /// </summary>
    int32_t Init(MCFitIniFile* scenarioFile);
    /// <summary>Destroys both heaps.</summary>
    void Destroy();
    /// <summary>
    /// Allocates <paramref name="numSpheres"/> spheres, or sets it to 0 and returns null when not that many are left.
    /// </summary>
    MCSmokeSphere* GetSpheres(int32_t& numSpheres);
    /// <summary>Gives back a block of <paramref name="numSpheres"/> spheres.</summary>
    void FreeSpheres(MCSmokeSphere* spheres, int32_t numSpheres);

    /// <summary>FIT "NumSmokeTypes".</summary>
    int32_t NumSmokeTypes = 0;
    /// <summary>FIT "MaxSmokesPerType".</summary>
    int32_t MaxSmokesPerType = 0;
    /// <summary>The spheres handed out and the smoke shapes (the original's sphere heap).</summary>
    MCBlockStore SphereBlocks;
    /// <summary>Spheres not handed out yet.</summary>
    int32_t NumFreeSpheres = 0;
};

/// <summary>How many smoke spheres the manager hands out (set by the scenario).</summary>
extern int32_t TotalSmokeSpheres;
/// <summary>Bytes of smoke shapes (set by the scenario; unused since the sphere heap is gone).</summary>
extern int32_t TotalSmokeShapeSize;
