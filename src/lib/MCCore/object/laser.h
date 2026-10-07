#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"
#include "object/objwtch.h"

class MCFile;
class MCGameObject;
struct MCPane;
struct MCWindow;

/// <summary>
/// The type of a <see cref="MCLaser"/> beam: its width and colour stages (one set for friendly, one for enemy
/// shooters), damage, sound and hit effects, and for a PPC the effect shape played along the beam.
/// </summary>
/// <remarks>
/// Original source: <c>object\laser.cpp</c>, <c>object\laser.h</c>; 0x74 bytes. Read from the "LaserData" block, the
/// "FLaser%d"/"ELaser%d" stage blocks and, with a LaserEffectShape, the "PPCData" block. The stage arrays come
/// from ObjectTypeManager::objectTypeCache.
/// </remarks>
class MCLaserType : public MCObjectType
{
public:
    MCLaserType() { Init(); }
    ~MCLaserType() override { Destroy(); }

    /// <summary>Resets the common type data and this type's fields.</summary>
    void Init();
    /// <summary>Makes a <see cref="MCLaser"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    /// <summary>Frees the stage arrays (when the type cache is up).</summary>
    void Destroy() override;
    /// <summary>
    /// Reads the laser data, loads the effect shape (from the sprite path) and its PPC data, reads numStages friendly
    /// then numStages enemy stages, the common type data, and loads the hit and miss types.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>Beam width in pixels (FIT "PixelWidth").</summary>
    uint8_t PixelWidth = 0;
    /// <summary>Stages per beam (FIT "NumStages"); the stage arrays hold 2 * numStages entries (friendly, then enemy).</summary>
    uint8_t NumStages = 0;
    /// <summary>Per stage: its duration in seconds (FIT "StageDuration").</summary>
    float* StageDuration = nullptr;
    /// <summary>Per stage: the outer (cool) palette colour (FIT "StageCool").</summary>
    uint8_t* StageCool = nullptr;
    /// <summary>Per stage: the core (hot) palette colour (FIT "StageHot").</summary>
    uint8_t* StageHot = nullptr;
    /// <summary>FIT "DmgLevel".</summary>
    uint32_t DmgLevel = 0;
    /// <summary>Sample played when the laser fires (FIT "SoundEffectId").</summary>
    uint32_t SoundEffectId = 0;
    /// <summary>Object type created where the beam hits its target (FIT "LaserHitEffect").</summary>
    uint32_t LaserHitEffect = 0;
    /// <summary>Object type created where a beam without a target lands (FIT "LaserMissEffect").</summary>
    uint32_t LaserMissEffect = 0;
    /// <summary>The PPC effect shape file's data (FIT "LaserEffectShape"), or null for a plain beam.</summary>
    uint8_t* LaserEffectShape = nullptr;
    /// <summary>Frames of the PPC effect (FIT "numPPCFrames"); the beam ends after the last.</summary>
    uint32_t NumPpcFrames = 0;
    /// <summary>Left edge of the effect in its shape (FIT "lPPC").</summary>
    uint32_t Lppc = 0;
    /// <summary>Top edge (FIT "tPPC").</summary>
    uint32_t Tppc = 0;
    /// <summary>Right edge (FIT "rPPC").</summary>
    uint32_t Rppc = 0;
    /// <summary>Bottom edge (FIT "bPPC").</summary>
    uint32_t Bppc = 0;
    /// <summary>The frame at which the PPC hits and deals its damage (FIT "hitPPC").</summary>
    uint32_t HitPpc = 0;
    /// <summary>Seconds per PPC frame (FIT "lengthPPC").</summary>
    float LengthPpc = 0;
    /// <summary>Animation period of the PPC effect (FIT "animPPC").</summary>
    float AnimPpc = 0;
};

/// <summary>
/// A laser (or PPC) beam drawn from its source's hot spot to its target, through the type's colour stages; when the
/// stages (or the PPC's hit frame) are reached it applies its shot to the target and creates the hit or miss effect.
/// </summary>
/// <remarks>Original source: <c>object\laser.cpp</c>, <c>object\laser.h</c>; 0xd4 bytes.</remarks>
class MCLaser : public MCBigGameObject
{
public:
    MCLaser() { Init(); }
    /// <summary>Frees the target position and stops watching the target and source.</summary>
    ~MCLaser() override
    {
        Destroy();
        Target.Free();
        Source.Free();
    }

    /// <summary>Resets the base object fields and this beam's state (stage 0xff: not started).</summary>
    void Init() override;
    /// <summary>Resets (vtable init()), then initialises from the type; class 0xe.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the target position.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>
    /// Steps the beam through its colour stages (the enemy set when the source's alignment isn't 1) or, for a PPC,
    /// its effect frames; applies the shot to the target once (host only in multiplayer).
    /// </summary>
    /// <returns>1 while the beam lasts, 0 when done.</returns>
    int32_t Update() override;
    /// <summary>
    /// Draws the beam (polygons in the stage colours, or the PPC shape stretched along it through the laser pane);
    /// creates the hit or miss effect (and a crater on a miss) once.
    /// </summary>
    void Render() override;

    /// <summary>Sets (allocating it the first time) the point the beam ends at.</summary>
    void SetTargetPosition(MCVector3D position);
    /// <summary>
    /// Watches <paramref name="source"/>, fires from its hot spot <paramref name="sourceHotSpot"/> at
    /// <paramref name="targetPos"/>, and copies <paramref name="shotInfo"/> (if any) as the shot to apply.
    /// </summary>
    void Connect(MCGameObject* source, MCVector3D targetPos, MCWeaponShotInfo* shotInfo, int32_t sourceHotSpot);

    /// <summary>The current colour stage; 0xff before the first update.</summary>
    uint8_t CurrentStage = 0;
    /// <summary>Seconds left in the current stage.</summary>
    float StageTimeLeft = 0;
    /// <summary>The current stage's outer (cool) colour.</summary>
    uint32_t CoolColor = 0;
    /// <summary>The current stage's core (hot) colour; the core is drawn only when it differs from the outer.</summary>
    uint32_t HotColor = 0;
    /// <summary>The object that fired.</summary>
    MCBaseObjectWatcher Source;
    /// <summary>The source's hot spot the beam starts at.</summary>
    int32_t SourceHotSpot = 0;
    /// <summary>The object hit; the shot is applied to it.</summary>
    MCBaseObjectWatcher Target;
    /// <summary>
    /// The target's hot spot that was hit, set by the launchers. The original never read it (OB-017); the beam ends there.
    /// </summary>
    int32_t TargetHotSpot = 0;
    /// <summary>Where the beam ends, allocated by <see cref="SetTargetPosition"/>.</summary>
    MCVector3D* TargetPosition = nullptr;
    /// <summary>The shot applied to the target.</summary>
    MCWeaponShotInfo ShotInfo{};
    /// <summary>Set once the hit or miss effect has been created.</summary>
    int32_t HitEffectCreated = 0;
    /// <summary>The current frame of the PPC effect.</summary>
    int32_t PpcFrame = 0;
    /// <summary>Time left in the PPC effect's animation period (restarts at the type's animPPC).</summary>
    float PpcAnimTimeLeft = 0;
    /// <summary>Time left in the current PPC frame (restarts at the type's lengthPPC).</summary>
    float PpcFrameTimeLeft = 0;
    /// <summary>Set by init; the first update of a PPC clears it and plays the sound.</summary>
    int32_t JustCreated = 0;
    /// <summary>Set once the shot has been applied to the target.</summary>
    int32_t DamageApplied = 0;
};

/// <summary>The 256x256 8-bit buffer the PPC effect shape is drawn into (allocated on first use).</summary>
extern std::unique_ptr<uint8_t[]> LaserEffectBuffer;
/// <summary>The pane over laserEffectBuffer.</summary>
extern std::unique_ptr<MCPane> LaserPane;
/// <summary>The window over laserEffectBuffer.</summary>
extern std::unique_ptr<MCWindow> LaserWindow;
