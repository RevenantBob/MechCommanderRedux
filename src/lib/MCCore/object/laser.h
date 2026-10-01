#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"
#include "object/objwtch.h"

class File;
class GameObject;
struct _pane;
struct _window;

/// <summary>
/// The type of a <see cref="Laser"/> beam: its width and colour stages (one set for friendly, one for enemy
/// shooters), damage, sound and hit effects, and for a PPC the effect shape played along the beam.
/// </summary>
/// <remarks>
/// Original source: <c>object\laser.cpp</c>, <c>object\laser.h</c>; 0x74 bytes. Read from the "LaserData" block, the
/// "FLaser%d"/"ELaser%d" stage blocks and, with a LaserEffectShape, the "PPCData" block. The stage arrays come
/// from ObjectTypeManager::objectTypeCache.
/// </remarks>
class LaserType : public ObjectType
{
public:
    LaserType() { init(); }
    /// <remarks>MCX.EXE @ 0x00690440 (vector deleting destructor)</remarks>
    ~LaserType() override { destroy(); }

    /// <summary>Resets the common type data and this type's fields.</summary>
    /// <remarks>MCX.EXE @ 0x006903e0 (inline in <c>object\laser.h</c>)</remarks>
    void init();
    /// <summary>Makes a <see cref="Laser"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00672f70</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the stage arrays (when the type cache is up).</summary>
    /// <remarks>MCX.EXE @ 0x006731e0</remarks>
    void destroy() override;
    /// <summary>
    /// Reads the laser data, loads the effect shape (from the sprite path) and its PPC data, reads numStages friendly
    /// then numStages enemy stages, the common type data, and loads the hit and miss types.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00673240</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00673750</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x00673760</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>Beam width in pixels (FIT "PixelWidth").</summary>
    uint8_t pixelWidth; // +0x30
    /// <summary>Stages per beam (FIT "NumStages"); the stage arrays hold 2 * numStages entries (friendly, then enemy).</summary>
    uint8_t numStages; // +0x31
    /// <summary>Per stage: its duration in seconds (FIT "StageDuration").</summary>
    float* stageDuration; // +0x34
    /// <summary>Per stage: the outer (cool) palette colour (FIT "StageCool").</summary>
    uint8_t* stageCool; // +0x38
    /// <summary>Per stage: the core (hot) palette colour (FIT "StageHot").</summary>
    uint8_t* stageHot; // +0x3c
    /// <summary>FIT "DmgLevel".</summary>
    uint32_t dmgLevel; // +0x40
    /// <summary>Sample played when the laser fires (FIT "SoundEffectId").</summary>
    uint32_t soundEffectId; // +0x44
    /// <summary>Object type created where the beam hits its target (FIT "LaserHitEffect").</summary>
    uint32_t laserHitEffect; // +0x48
    /// <summary>Object type created where a beam without a target lands (FIT "LaserMissEffect").</summary>
    uint32_t laserMissEffect; // +0x4c
    /// <summary>The PPC effect shape file's data (FIT "LaserEffectShape"), or null for a plain beam.</summary>
    uint8_t* laserEffectShape; // +0x50
    /// <summary>Frames of the PPC effect (FIT "numPPCFrames"); the beam ends after the last.</summary>
    uint32_t numPPCFrames; // +0x54
    /// <summary>Left edge of the effect in its shape (FIT "lPPC").</summary>
    uint32_t lPPC; // +0x58
    /// <summary>Top edge (FIT "tPPC").</summary>
    uint32_t tPPC; // +0x5c
    /// <summary>Right edge (FIT "rPPC").</summary>
    uint32_t rPPC; // +0x60
    /// <summary>Bottom edge (FIT "bPPC").</summary>
    uint32_t bPPC; // +0x64
    /// <summary>The frame at which the PPC hits and deals its damage (FIT "hitPPC").</summary>
    uint32_t hitPPC; // +0x68
    /// <summary>Seconds per PPC frame (FIT "lengthPPC").</summary>
    float lengthPPC; // +0x6c
    /// <summary>Animation period of the PPC effect (FIT "animPPC").</summary>
    float animPPC; // +0x70
};

/// <summary>
/// A laser (or PPC) beam drawn from its source's hot spot to its target, through the type's colour stages; when the
/// stages (or the PPC's hit frame) are reached it applies its shot to the target and creates the hit or miss effect.
/// </summary>
/// <remarks>Original source: <c>object\laser.cpp</c>, <c>object\laser.h</c>; 0xd4 bytes.</remarks>
class Laser : public BigGameObject
{
public:
    Laser() { init(); }
    /// <summary>Frees the target position and stops watching the target and source.</summary>
    /// <remarks>MCX.EXE @ 0x00673160 (vector deleting destructor)</remarks>
    ~Laser() override
    {
        destroy();
        target.free();
        source.free();
    }

    /// <summary>Resets the base object fields and this beam's state (stage 0xff: not started).</summary>
    /// <remarks>MCX.EXE @ 0x00673070 (inline in <c>object\laser.h</c>)</remarks>
    void init() override;
    /// <summary>Resets (vtable init()), then initialises from the type; class 0xe.</summary>
    /// <remarks>MCX.EXE @ 0x00674580</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the target position.</summary>
    /// <remarks>MCX.EXE @ 0x00673120 (inline in <c>object\laser.h</c>)</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00673150</remarks>
    int32_t kill() override { return 0; }
    /// <summary>
    /// Steps the beam through its colour stages (the enemy set when the source's alignment isn't 1) or, for a PPC,
    /// its effect frames; applies the shot to the target once (host only in multiplayer).
    /// </summary>
    /// <returns>1 while the beam lasts, 0 when done.</returns>
    /// <remarks>MCX.EXE @ 0x00673770</remarks>
    int32_t update() override;
    /// <summary>
    /// Draws the beam (polygons in the stage colours, or the PPC shape stretched along it through the laser pane);
    /// creates the hit or miss effect (and a crater on a miss) once.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00673b90</remarks>
    void render() override;

    /// <summary>Sets (allocating it the first time) the point the beam ends at.</summary>
    /// <remarks>MCX.EXE @ 0x006745c0</remarks>
    void setTargetPosition(vector_3d position);
    /// <summary>
    /// Watches <paramref name="source"/>, fires from its hot spot <paramref name="sourceHotSpot"/> at
    /// <paramref name="targetPos"/>, and copies <paramref name="shotInfo"/> (if any) as the shot to apply.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0065f5e0 (inline in <c>object\laser.h</c>)</remarks>
    void connect(GameObject* source, vector_3d targetPos, _WeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>The current colour stage; 0xff before the first update.</summary>
    uint8_t currentStage; // +0x84
    /// <summary>Seconds left in the current stage.</summary>
    float stageTimeLeft; // +0x88
    /// <summary>The current stage's outer (cool) colour.</summary>
    uint32_t coolColor; // +0x8c
    /// <summary>The current stage's core (hot) colour; the core is drawn only when it differs from the outer.</summary>
    uint32_t hotColor; // +0x90
    /// <summary>The object that fired.</summary>
    BaseObjectWatcher source; // +0x94
    /// <summary>The source's hot spot the beam starts at.</summary>
    int32_t sourceHotSpot; // +0x98
    /// <summary>The object hit; the shot is applied to it.</summary>
    BaseObjectWatcher target; // +0x9c
    /// <summary>Zeroed by init; not otherwise used in laser.cpp.</summary>
    int32_t unknownA0; // +0xa0
    /// <summary>Where the beam ends, allocated by <see cref="setTargetPosition"/>.</summary>
    vector_3d* targetPosition; // +0xa4
    /// <summary>The shot applied to the target.</summary>
    _WeaponShotInfo shotInfo; // +0xa8
    /// <summary>Set once the hit or miss effect has been created.</summary>
    int32_t hitEffectCreated; // +0xbc
    /// <summary>The current frame of the PPC effect.</summary>
    int32_t ppcFrame; // +0xc0
    /// <summary>Time left in the PPC effect's animation period (restarts at the type's animPPC).</summary>
    float ppcAnimTimeLeft; // +0xc4
    /// <summary>Time left in the current PPC frame (restarts at the type's lengthPPC).</summary>
    float ppcFrameTimeLeft; // +0xc8
    /// <summary>Set by init; the first update of a PPC clears it and plays the sound.</summary>
    int32_t justCreated; // +0xcc
    /// <summary>Set once the shot has been applied to the target.</summary>
    int32_t damageApplied; // +0xd0
};

/// <summary>The 256x256 8-bit buffer the PPC effect shape is drawn into (allocated on first use).</summary>
extern uint8_t* laserEffectBuffer;
/// <summary>The pane over laserEffectBuffer.</summary>
extern _pane* laserPane;
/// <summary>The window over laserEffectBuffer.</summary>
extern _window* laserWindow;
