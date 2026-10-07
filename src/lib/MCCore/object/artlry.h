#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"
#include "platform/MCRegisteredBlock.h"

class MCAppearance;
class MCBaseObject;
class MCCamera;
class MCFile;
class MCGameObject;
class MCObjectEvent;
class MCSensorSystem;
class MCTeam;
struct MCWeaponShotInfo;

/// <summary>
/// Calls an artillery strike (or a sensor probe) for commander <paramref name="commanderId"/>: spends one of the
/// commander's strikes of that kind, creates the strike object at <paramref name="location"/> and, in a multiplayer
/// game run by the server, sends it to the others.
/// </summary>
/// <param name="strikeType">
/// 0-3 (and 4-7, the same kinds): which of the commander's four artillery counters is spent and which strike object
/// type is made. In multiplayer 4-6 are turned into 0-2.
/// </param>
/// <param name="seconds">Seconds until impact; -1 keeps the type's nominal time, under 3 means -1.</param>
/// <param name="randomOffset">Stored in the strike (<see cref="MCArtillery::RandomOffset"/>).</param>
void CallArtillery(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds, int randomOffset);

/// <summary>
/// A multiplayer message announcing an artillery strike: the commander, the strike type, the map cell and the
/// seconds to impact, packed into one 32-bit word.
/// </summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x14 bytes. <c>data</c> is what goes over the network:
/// bits 0-2 commanderId, 3-5 strikeType, 6-15 cellCol, 16-25 cellRow, 26-31 seconds + 1.
/// </remarks>
class MCArtilleryChunk
{
public:
    /// <summary>Fills the chunk for a strike at <paramref name="location"/> (converted to a map cell).</summary>
    void Build(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds);
    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();
    /// <summary>Unpacks <see cref="Data"/> into the fields.</summary>
    void Unpack();
    /// <summary>Whether the unpacked fields of both chunks match.</summary>
    int EqualTo(MCArtilleryChunk* chunk);

    int8_t CommanderId = 0;
    int8_t StrikeType = 0;
    int32_t CellRow = 0;
    int32_t CellCol = 0;
    /// <summary>Seconds to impact (-1 = the type's nominal time).</summary>
    int8_t Seconds = 0;
    /// <summary>The packed word sent over the network.</summary>
    uint32_t Data = 0;
};

/// <summary>
/// The type of an <see cref="MCArtillery"/> strike: its countdown sprite, timing, damage, the ranges and hit counts of
/// its major and minor blast, its sensor probe and the pattern of explosions it sets off.
/// </summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x94 bytes. Read from the "Artillery" block of its FIT. Constructed
/// inline in ObjectTypeManager::load (only the ObjectType defaults).
/// </remarks>
class MCArtilleryType : public MCObjectType
{
public:
    ~MCArtilleryType() override { Destroy(); }

    /// <summary>Makes an <see cref="MCArtillery"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    /// <summary>Frees the sprite and the explosion tables.</summary>
    void Destroy() override;
    /// <summary>Reads the "Artillery" block and the explosion pattern, and loads the countdown sprite.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// Damages <paramref name="collider"/> once the strike (<paramref name="collidee"/>) has hit: the major hit count
    /// inside the major range, else the minor one.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>The countdown sprite (a VFX shape file), in the sprite manager's shape RAM.</summary>
    MCRegisteredBlock ShapeData;
    /// <summary>FIT "FrameCount".</summary>
    uint32_t FrameCount = 0;
    /// <summary>FIT "StartFrame".</summary>
    uint32_t StartFrame = 0;
    /// <summary>FIT "FrameRate", frames per second.</summary>
    float FrameRate = 0;
    /// <summary>FIT "NominalTimeToImpact", seconds.</summary>
    float NominalTimeToImpact = 0;
    /// <summary>FIT "NominalTimeToLaunch", seconds (default: time to impact - 10).</summary>
    float NominalTimeToLaunch = 0;
    /// <summary>FIT "NominalDamage": damage per hit; 0 for a sensor probe.</summary>
    float NominalDamage = 0;
    /// <summary>FIT "NominalMajorRange", meters.</summary>
    float NominalMajorRange = 0;
    /// <summary>FIT "NominalMajorHits".</summary>
    float NominalMajorHits = 0;
    /// <summary>FIT "NominalMinorRange", meters.</summary>
    float NominalMinorRange = 0;
    /// <summary>FIT "NominalMinorHits".</summary>
    float NominalMinorHits = 0;
    /// <summary>FIT "NominalSensorTime", seconds the sensor probe lasts.</summary>
    float NominalSensorTime = 0;
    /// <summary>FIT "NominalSensorRange", meters.</summary>
    float NominalSensorRange = 0;
    /// <summary>FIT "fontScale".</summary>
    float FontScale = 0;
    /// <summary>FIT "fontXOffset": where the countdown text goes, relative to the sprite.</summary>
    float FontXOffset = 0;
    /// <summary>FIT "fontYOffset".</summary>
    float FontYOffset = 0;
    /// <summary>FIT "fontColor".</summary>
    uint32_t FontColor = 0;
    /// <summary>FIT "NumExplosions": entries of the three tables below (only read when there is damage).</summary>
    int32_t NumExplosions = 0;
    /// <summary>FIT "ExplosionOffsetX%d", world units from the strike point.</summary>
    std::unique_ptr<float[]> ExplosionOffsetX;
    /// <summary>FIT "ExplosionOffsetY%d".</summary>
    std::unique_ptr<float[]> ExplosionOffsetY;
    /// <summary>FIT "ExplosionDelay%d", seconds after impact.</summary>
    std::unique_ptr<float[]> ExplosionDelay;
    /// <summary>FIT "ExplosionsPerExplosion": explosions made at each entry.</summary>
    int32_t ExplosionsPerExplosion = 0;
    /// <summary>FIT "ExplosionRandomOffsetX": random spread of each explosion.</summary>
    int32_t ExplosionRandomOffsetX = 0;
    /// <summary>FIT "ExplosionRandomOffsetY".</summary>
    int32_t ExplosionRandomOffsetY = 0;
    /// <summary>
    /// FIT "MinArtilleryHeadRange" (default 5), meters: beyond it a hit lands on hit-location table 4, else 2.
    /// </summary>
    int32_t MinArtilleryHeadRange = 0;
};

/// <summary>
/// An artillery strike or sensor probe on its way: shows a countdown at the target, then sets off its explosion
/// pattern (or, for a probe, opens a shrinking sensor and launches a <see cref="MCCameraDrone"/>).
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>, <c>object\artlry.h</c>; 0xd8 bytes.</remarks>
class MCArtillery : public MCBigGameObject
{
public:
    /// <summary>
    /// Calls <see cref="init()"/>, then sets the defaults (times -1, justCreated and randomOffset 1, the rest 0).
    /// </summary>
    /// <remarks>Inline in ArtilleryType::createInstance.</remarks>
    MCArtillery();
    ~MCArtillery() override { Destroy(); }

    /// <summary>Records the scenario time the strike was made.</summary>
    void Init() override;
    /// <summary>Sets up the object and, for a damaging strike, the table of explosions already set off.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the sensor and the explosion table.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Counts down, animates, plays the incoming sound, sets off the explosions and runs the probe.</summary>
    /// <returns>0 when the strike is over.</returns>
    int32_t Update() override;
    /// <summary>Draws the countdown sprite and the time left.</summary>
    void Render() override;
    /// <summary>Tracks the mouse-over (0x1c/0x1d) and select (0x1e/0x1f) events.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    /// <summary>
    /// After impact: sets off the mines in the 3x3 map cells around the strike and runs collision checks with the
    /// terrain objects of the 3x3 terrain blocks around it.
    /// </summary>
    void HandleStaticCollision() override;
    /// <summary>Projects the strike to the screen; true when its sprite is inside the main camera.</summary>
    int OnScreen() override;

    /// <summary>
    /// First update/render: takes the type's times and start frame, and for a probe opens its sensor for the
    /// strike's side.
    /// </summary>
    void SetJustCreated();
    /// <summary>Recomputes the sprite's screen bounds; true when they overlap the camera's view.</summary>
    int RecalcBounds(MCCamera* camera);
    /// <summary>Gives the sensor probe its team, time and range (-1 keeps the current value).</summary>
    void SetSensorData(MCTeam* team, float sensorTime, float sensorRange);
    /// <summary>Does nothing.</summary>
    void DrawSelectBox(uint8_t color);

    /// <summary>Set until the first update or render has run <see cref="SetJustCreated"/>.</summary>
    int32_t JustCreated = 0;
    /// <summary>The sprite frame drawn (wraps at the type's frame count).</summary>
    uint32_t CurrentFrame = 0;
    /// <summary>Seconds the sprite has been animating.</summary>
    float FrameTime = 0;
    /// <summary>floor(frameTime * frameRate) at the last frame advance.</summary>
    int32_t FrameCount = 0;
    /// <summary>Screen bounds of the sprite: left, top.</summary>
    float BoundsLeft = 0;
    float BoundsTop = 0;
    /// <summary>Screen bounds of the sprite: right, bottom.</summary>
    float BoundsRight = 0;
    float BoundsBottom = 0;
    /// <summary>Seconds to impact (negative after it; -1 until set).</summary>
    float TimeToImpact = 0;
    /// <summary>Seconds to launch (counts down with timeToImpact).</summary>
    float TimeToLaunch = 0;
    /// <summary>The sensor probe's current range, world units.</summary>
    float SensorRange = 0;
    /// <summary>Seconds of sensor time left.</summary>
    float SensorTime = 0;
    /// <summary>The sensor probe's sensor (from the SensorSystemManager).</summary>
    MCSensorSystem* SensorSystem = nullptr;
    /// <summary>Scenario time the strike was made.</summary>
    float StartTime = 0;
    /// <summary>Set once the strike has hit (from then on it collides and explodes).</summary>
    int32_t HasImpacted = 0;
    /// <summary>Set once the sensor probe's sensor is running.</summary>
    int32_t SensorActive = 0;
    /// <summary>The countdown text, "%01d:%02d".</summary>
    char TimeString[8]{};
    /// <summary>
    /// Whether the impact is scattered: the last argument of <see cref="CallArtillery"/> (default 1; every caller
    /// passes 0). The scatter itself is gone from the original: when nonzero, two
    /// RandomNumber(500) draws are made (and discarded) at impact.
    /// </summary>
    int32_t RandomOffset = 0;
    /// <summary>Set once the incoming-shell sound has played.</summary>
    int32_t ImpactSoundPlayed = 0;
    /// <summary>One flag per entry of the type's explosion tables: set once that explosion went off.</summary>
    std::unique_ptr<int32_t[]> ExplosionsDone;
};

/// <summary>The type of a <see cref="MCCameraDrone"/>: speed, hit points and battle value.</summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x3c bytes. Read from the "General" block of its FIT; the extent
/// radius is forced to -1. Constructed inline in ObjectTypeManager::load (only the ObjectType defaults).
/// </remarks>
class MCCameraDroneType : public MCObjectType
{
public:
    ~MCCameraDroneType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCCameraDrone"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>Reads maxVelocity, maxDamage and BRValue from the "General" block.</summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    int HandleCollision(MCGameObject* /*collidee*/, MCGameObject* /*collider*/) override { return 0; }
    /// <summary>Marks the drone destroyed (status 2).</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>FIT "maxVelocity", meters per second.</summary>
    float MaxVelocity = 0;
    /// <summary>FIT "maxDamage": hit points.</summary>
    int32_t MaxDamage = 0;
    /// <summary>FIT "BRValue" (default 0): the drone's max and current CV.</summary>
    int32_t BrValue = 0;
};

/// <summary>
/// The spotter drone a sensor probe launches: flies an outward square spiral of map tiles around its start,
/// revealing the terrain it passes over.
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>, <c>object\artlry.h</c>; 0xc4 bytes.</remarks>
class MCCameraDrone : public MCBigGameObject
{
public:
    /// <summary>Starts in the world frame, then calls <see cref="init()"/>.</summary>
    /// <remarks>Inline in CameraDroneType::createInstance.</remarks>
    MCCameraDrone();
    ~MCCameraDrone() override { Destroy(); }

    /// <summary>Resets the spiral, the target tile and the appearance.</summary>
    void Init() override;
    /// <summary>Makes the GV appearance, takes the type's speed, hit points and CV, and sets the frame.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Flies toward the target tile, reveals the terrain around it and picks the next tile on arrival.</summary>
    int32_t Update() override;
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Tracks the mouse-over (0x1c/0x1d) and select (0x1e/0x1f) events.</summary>
    int32_t HandleEvent(MCObjectEvent* event) override;
    int OnScreen() override;
    /// <summary>Takes the damage off the hit points; at none left the drone is destroyed and explodes.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    MCFrameOfRef GetFrame() override { return Frame; }

    /// <summary>Turns the frame and steps the spiral: the next leg's direction and, every other leg, its length.</summary>
    void FindNextTargetTile();

    /// <summary>The spiral leg's direction, 0-3 (-1 before the first).</summary>
    int8_t SpiralDirection = 0;
    /// <summary>The spiral leg's length in tiles.</summary>
    int8_t SpiralLength = 0;
    /// <summary>The map tile being flown to.</summary>
    int32_t TargetTileRow = 0;
    int32_t TargetTileCol = 0;
    /// <summary>Meters per second (from the type).</summary>
    float MaxVelocity = 0;
    /// <summary>Hit points left.</summary>
    int32_t HitPoints = 0;
    /// <summary>The drone's orientation.</summary>
    MCFrameOfRef Frame;
    /// <summary>Scenario time the drone was launched (-1 until then).</summary>
    float LaunchTime = 0;
    /// <summary>The drone's GV appearance.</summary>
    MCAppearance* Appearance = nullptr;
};

/// <summary>The object type numbers made by <see cref="CallArtillery"/>, one per strike type.</summary>
/// <remarks>
/// At 0x0078e4f8 (no symbol; the name is the port's), indexed by strike type. Initial values:
/// 249, 248, 250, 516, 508, 507, 509, 516.
/// </remarks>
extern int32_t ArtilleryTypeTable[8];
/// <summary>How many camera drones have been launched; their part ids start at 0x802c8 (limit 1000).</summary>
/// <remarks>At 0x007dd018 (no symbol; the name is the port's).</remarks>
extern int32_t NumCameraDrones;
