#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class BaseObject;
class Camera;
class File;
class GameObject;
class ObjectEvent;
class SensorSystem;
class Team;
struct _WeaponShotInfo;

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
/// <param name="randomOffset">Stored in the strike (<see cref="Artillery::unknownCC"/>).</param>
/// <remarks>MCX.EXE @ 0x0064dda0</remarks>
void CallArtillery(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds, int randomOffset);

/// <summary>
/// A multiplayer message announcing an artillery strike: the commander, the strike type, the map cell and the
/// seconds to impact, packed into one 32-bit word.
/// </summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x14 bytes. <c>data</c> is what goes over the network:
/// bits 0-2 commanderId, 3-5 strikeType, 6-15 cellCol, 16-25 cellRow, 26-31 seconds + 1.
/// </remarks>
class ArtilleryChunk
{
public:
    /// <remarks>MCX.EXE @ 0x0064dfa0 (allocates from systemHeap)</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0064dfd0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Fills the chunk for a strike at <paramref name="location"/> (converted to a map cell).</summary>
    /// <remarks>MCX.EXE @ 0x0064e000</remarks>
    void build(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds);
    /// <summary>Packs the fields into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0064e050</remarks>
    void pack();
    /// <summary>Unpacks <see cref="data"/> into the fields.</summary>
    /// <remarks>MCX.EXE @ 0x0064e080</remarks>
    void unpack();
    /// <summary>Whether the unpacked fields of both chunks match.</summary>
    /// <remarks>MCX.EXE @ 0x0064e0c0</remarks>
    int equalTo(ArtilleryChunk* chunk);

    int8_t commanderId; // +0x0
    int8_t strikeType;  // +0x1
    int32_t cellRow;    // +0x4
    int32_t cellCol;    // +0x8
    /// <summary>Seconds to impact (-1 = the type's nominal time).</summary>
    int8_t seconds; // +0xc
    /// <summary>The packed word sent over the network.</summary>
    uint32_t data; // +0x10
};

/// <summary>
/// The type of an <see cref="Artillery"/> strike: its countdown sprite, timing, damage, the ranges and hit counts of
/// its major and minor blast, its sensor probe and the pattern of explosions it sets off.
/// </summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x94 bytes. Read from the "Artillery" block of its FIT. Constructed
/// inline in ObjectTypeManager::load (only the ObjectType defaults).
/// </remarks>
class ArtilleryType : public ObjectType
{
public:
    /// <remarks>MCX.EXE @ 0x00690850 (vector deleting destructor)</remarks>
    ~ArtilleryType() override { destroy(); }

    /// <summary>Makes an <see cref="Artillery"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0064e120</remarks>
    BaseObject* createInstance() override;
    /// <summary>Frees the sprite and the explosion tables.</summary>
    /// <remarks>MCX.EXE @ 0x0064ef20</remarks>
    void destroy() override;
    /// <summary>Reads the "Artillery" block and the explosion pattern, and loads the countdown sprite.</summary>
    /// <remarks>MCX.EXE @ 0x0064ef80</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// Damages <paramref name="collider"/> once the strike (<paramref name="collidee"/>) has hit: the major hit count
    /// inside the major range, else the minor one.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064f5b0</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <remarks>MCX.EXE @ 0x0064f8f0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>The countdown sprite (a VFX shape file), in the sprite manager's shape RAM.</summary>
    uint8_t* shapeData; // +0x30
    /// <summary>FIT "FrameCount".</summary>
    uint32_t frameCount; // +0x34
    /// <summary>FIT "StartFrame".</summary>
    uint32_t startFrame; // +0x38
    /// <summary>FIT "FrameRate", frames per second.</summary>
    float frameRate; // +0x3c
    /// <summary>FIT "NominalTimeToImpact", seconds.</summary>
    float nominalTimeToImpact; // +0x40
    /// <summary>FIT "NominalTimeToLaunch", seconds (default: time to impact - 10).</summary>
    float nominalTimeToLaunch; // +0x44
    /// <summary>FIT "NominalDamage": damage per hit; 0 for a sensor probe.</summary>
    float nominalDamage; // +0x48
    /// <summary>FIT "NominalMajorRange", meters.</summary>
    float nominalMajorRange; // +0x4c
    /// <summary>FIT "NominalMajorHits".</summary>
    float nominalMajorHits; // +0x50
    /// <summary>FIT "NominalMinorRange", meters.</summary>
    float nominalMinorRange; // +0x54
    /// <summary>FIT "NominalMinorHits".</summary>
    float nominalMinorHits; // +0x58
    /// <summary>FIT "NominalSensorTime", seconds the sensor probe lasts.</summary>
    float nominalSensorTime; // +0x5c
    /// <summary>FIT "NominalSensorRange", meters.</summary>
    float nominalSensorRange; // +0x60
    /// <summary>FIT "fontScale".</summary>
    float fontScale; // +0x64
    /// <summary>FIT "fontXOffset": where the countdown text goes, relative to the sprite.</summary>
    float fontXOffset; // +0x68
    /// <summary>FIT "fontYOffset".</summary>
    float fontYOffset; // +0x6c
    /// <summary>FIT "fontColor".</summary>
    uint32_t fontColor; // +0x70
    /// <summary>FIT "NumExplosions": entries of the three tables below (only read when there is damage).</summary>
    int32_t numExplosions; // +0x74
    /// <summary>FIT "ExplosionOffsetX%d", world units from the strike point (systemHeap).</summary>
    float* explosionOffsetX; // +0x78
    /// <summary>FIT "ExplosionOffsetY%d".</summary>
    float* explosionOffsetY; // +0x7c
    /// <summary>FIT "ExplosionDelay%d", seconds after impact.</summary>
    float* explosionDelay; // +0x80
    /// <summary>FIT "ExplosionsPerExplosion": explosions made at each entry.</summary>
    int32_t explosionsPerExplosion; // +0x84
    /// <summary>FIT "ExplosionRandomOffsetX": random spread of each explosion.</summary>
    int32_t explosionRandomOffsetX; // +0x88
    /// <summary>FIT "ExplosionRandomOffsetY".</summary>
    int32_t explosionRandomOffsetY; // +0x8c
    /// <summary>
    /// FIT "MinArtilleryHeadRange" (default 5), meters: beyond it a hit lands on hit-location table 4, else 2.
    /// </summary>
    int32_t minArtilleryHeadRange; // +0x90
};

/// <summary>
/// An artillery strike or sensor probe on its way: shows a countdown at the target, then sets off its explosion
/// pattern (or, for a probe, opens a shrinking sensor and launches a <see cref="CameraDrone"/>).
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>, <c>object\artlry.h</c>; 0xd8 bytes.</remarks>
class Artillery : public BigGameObject
{
public:
    /// <summary>
    /// Calls <see cref="init()"/>, then sets the defaults (times -1, justCreated and unknownCC 1, the rest 0).
    /// </summary>
    /// <remarks>Inline in ArtilleryType::createInstance (MCX.EXE @ 0x0064e120).</remarks>
    Artillery();
    /// <remarks>MCX.EXE @ 0x0064eed0 (vector deleting destructor)</remarks>
    ~Artillery() override { destroy(); }

    /// <summary>Records the scenario time the strike was made.</summary>
    /// <remarks>MCX.EXE @ 0x0064fe10</remarks>
    void init() override;
    /// <summary>Sets up the object and, for a damaging strike, the table of explosions already set off.</summary>
    /// <remarks>MCX.EXE @ 0x00650b20</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the sensor and the explosion table.</summary>
    /// <remarks>MCX.EXE @ 0x00650ad0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0064eec0</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Counts down, animates, plays the incoming sound, sets off the explosions and runs the probe.</summary>
    /// <returns>0 when the strike is over.</returns>
    /// <remarks>MCX.EXE @ 0x0064ff90</remarks>
    int32_t update() override;
    /// <summary>Draws the countdown sprite and the time left.</summary>
    /// <remarks>MCX.EXE @ 0x00650830</remarks>
    void render() override;
    /// <summary>Tracks the mouse-over (0x1c/0x1d) and select (0x1e/0x1f) events.</summary>
    /// <remarks>MCX.EXE @ 0x006507d0 (unnamed in the symbols; vtable slot 9 of Artillery)</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <summary>
    /// After impact: sets off the mines in the 3x3 map cells around the strike and runs collision checks with the
    /// terrain objects of the 3x3 terrain blocks around it.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064f900</remarks>
    void handleStaticCollision() override;
    /// <summary>Projects the strike to the screen; true when its sprite is inside the main camera.</summary>
    /// <remarks>MCX.EXE @ 0x0064fe20</remarks>
    int onScreen() override;

    /// <summary>
    /// First update/render: takes the type's times and start frame, and for a probe opens its sensor for the
    /// strike's side.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0064fec0</remarks>
    void setJustCreated();
    /// <summary>Recomputes the sprite's screen bounds; true when they overlap the camera's view.</summary>
    /// <remarks>MCX.EXE @ 0x00650610</remarks>
    int recalcBounds(Camera* camera);
    /// <summary>Gives the sensor probe its team, time and range (-1 keeps the current value).</summary>
    /// <remarks>MCX.EXE @ 0x00650750</remarks>
    void setSensorData(Team* team, float sensorTime, float sensorRange);
    /// <summary>Does nothing.</summary>
    /// <remarks>MCX.EXE @ 0x006507c0</remarks>
    void drawSelectBox(uint8_t color);

    /// <summary>Set until the first update or render has run <see cref="setJustCreated"/>.</summary>
    int32_t justCreated; // +0x84
    /// <summary>The sprite frame drawn (wraps at the type's frame count).</summary>
    uint32_t currentFrame; // +0x88
    /// <summary>Seconds the sprite has been animating.</summary>
    float frameTime; // +0x8c
    /// <summary>floor(frameTime * frameRate) at the last frame advance.</summary>
    int32_t frameCount; // +0x90
    /// <summary>Screen bounds of the sprite: left, top.</summary>
    float boundsLeft; // +0x94
    float boundsTop;  // +0x98
    /// <summary>Screen bounds of the sprite: right, bottom.</summary>
    float boundsRight;  // +0x9c
    float boundsBottom; // +0xa0
    /// <summary>Seconds to impact (negative after it; -1 until set).</summary>
    float timeToImpact; // +0xa4
    /// <summary>Seconds to launch (counts down with timeToImpact).</summary>
    float timeToLaunch; // +0xa8
    /// <summary>The sensor probe's current range, world units.</summary>
    float sensorRange; // +0xac
    /// <summary>Seconds of sensor time left.</summary>
    float sensorTime; // +0xb0
    /// <summary>The sensor probe's sensor (from the SensorSystemManager).</summary>
    SensorSystem* sensorSystem; // +0xb4
    /// <summary>Scenario time the strike was made.</summary>
    float startTime; // +0xb8
    /// <summary>Set once the strike has hit (from then on it collides and explodes).</summary>
    int32_t hasImpacted; // +0xbc
    /// <summary>Set once the sensor probe's sensor is running.</summary>
    int32_t sensorActive; // +0xc0
    /// <summary>The countdown text, "%01d:%02d".</summary>
    char timeString[8]; // +0xc4
    /// <summary>
    /// The last argument of <see cref="CallArtillery"/> (default 1). Its only use: when nonzero, two
    /// RandomNumber(500) draws are made (and discarded) at impact.
    /// </summary>
    int32_t unknownCC; // +0xcc
    /// <summary>Set once the incoming-shell sound has played.</summary>
    int32_t impactSoundPlayed; // +0xd0
    /// <summary>One flag per entry of the type's explosion tables: set once that explosion went off.</summary>
    int32_t* explosionsDone; // +0xd4
};

/// <summary>The type of a <see cref="CameraDrone"/>: speed, hit points and battle value.</summary>
/// <remarks>
/// Original source: <c>object\artlry.cpp</c>, 0x3c bytes. Read from the "General" block of its FIT; the extent
/// radius is forced to -1. Constructed inline in ObjectTypeManager::load (only the ObjectType defaults).
/// </remarks>
class CameraDroneType : public ObjectType
{
public:
    /// <remarks>MCX.EXE @ 0x006908a0 (vector deleting destructor)</remarks>
    ~CameraDroneType() override { destroy(); }

    /// <summary>Makes a <see cref="CameraDrone"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x00650bc0</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x00650e80</remarks>
    void destroy() override;
    /// <summary>Reads maxVelocity, maxDamage and BRValue from the "General" block.</summary>
    /// <remarks>MCX.EXE @ 0x00650dd0</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <remarks>MCX.EXE @ 0x00690890</remarks>
    int handleCollision(GameObject* /*collidee*/, GameObject* /*collider*/) override { return 0; }
    /// <summary>Marks the drone destroyed (status 2).</summary>
    /// <remarks>MCX.EXE @ 0x00650e90</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>FIT "maxVelocity", meters per second.</summary>
    float maxVelocity; // +0x30
    /// <summary>FIT "maxDamage": hit points.</summary>
    int32_t maxDamage; // +0x34
    /// <summary>FIT "BRValue" (default 0): the drone's max and current CV.</summary>
    int32_t brValue; // +0x38
};

/// <summary>
/// The spotter drone a sensor probe launches: flies an outward square spiral of map tiles around its start,
/// revealing the terrain it passes over.
/// </summary>
/// <remarks>Original source: <c>object\artlry.cpp</c>, <c>object\artlry.h</c>; 0xc4 bytes.</remarks>
class CameraDrone : public BigGameObject
{
public:
    /// <summary>Starts in the world frame, then calls <see cref="init()"/>.</summary>
    /// <remarks>Inline in CameraDroneType::createInstance (MCX.EXE @ 0x00650bc0).</remarks>
    CameraDrone();
    /// <remarks>MCX.EXE @ 0x00650d80 (vector deleting destructor)</remarks>
    ~CameraDrone() override { destroy(); }

    /// <summary>Resets the spiral, the target tile and the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x00650cc0 (inline in <c>object\artlry.h</c>)</remarks>
    void init() override;
    /// <summary>Makes the GV appearance, takes the type's speed, hit points and CV, and sets the frame.</summary>
    /// <remarks>MCX.EXE @ 0x00650eb0</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Destroys the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x006512d0</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x00650d10</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Flies toward the target tile, reveals the terrain around it and picks the next tile on arrival.</summary>
    /// <remarks>MCX.EXE @ 0x006513a0</remarks>
    int32_t update() override;
    /// <remarks>MCX.EXE @ 0x006516a0</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x00650d00</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Tracks the mouse-over (0x1c/0x1d) and select (0x1e/0x1f) events.</summary>
    /// <remarks>MCX.EXE @ 0x00651640</remarks>
    int32_t handleEvent(ObjectEvent* event) override;
    /// <remarks>MCX.EXE @ 0x006512f0</remarks>
    int onScreen() override;
    /// <summary>Takes the damage off the hit points; at none left the drone is destroyed and explodes.</summary>
    /// <remarks>MCX.EXE @ 0x006516f0</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <remarks>MCX.EXE @ 0x00650d20 (inline in <c>object\artlry.h</c>)</remarks>
    frame_of_ref getFrame() override { return frame; }

    /// <summary>Turns the frame and steps the spiral: the next leg's direction and, every other leg, its length.</summary>
    /// <remarks>MCX.EXE @ 0x00651790</remarks>
    void findNextTargetTile();

    /// <summary>The spiral leg's direction, 0-3 (-1 before the first).</summary>
    int8_t spiralDirection; // +0x84
    /// <summary>The spiral leg's length in tiles.</summary>
    int8_t spiralLength; // +0x85
    /// <summary>The map tile being flown to.</summary>
    int32_t targetTileRow; // +0x88
    int32_t targetTileCol; // +0x8c
    /// <summary>Meters per second (from the type).</summary>
    float maxVelocity; // +0x90
    /// <summary>Hit points left.</summary>
    int32_t hitPoints; // +0x94
    /// <summary>The drone's orientation.</summary>
    frame_of_ref frame; // +0x98
    /// <summary>Scenario time the drone was launched (-1 until then).</summary>
    float launchTime; // +0xbc
    /// <summary>The drone's GV appearance.</summary>
    Appearance* appearance; // +0xc0
};

/// <summary>The object type numbers made by <see cref="CallArtillery"/>, one per strike type.</summary>
/// <remarks>
/// At 0x0078e4f8 (unnamed in the symbols; <c>DAT_0078e4f8</c>), indexed by strike type. Initial values:
/// 249, 248, 250, 516, 508, 507, 509, 516.
/// </remarks>
extern int32_t artilleryTypeTable[8];
/// <summary>How many camera drones have been launched; their part ids start at 0x802c8 (limit 1000).</summary>
/// <remarks>At 0x007dd018 (unnamed in the symbols; <c>DAT_007dd018</c>).</remarks>
extern int32_t numCameraDrones;
