#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class Appearance;
class File;
class GameObject;
class Train;
struct _WeaponShotInfo;

/// <summary>
/// The type of a <see cref="TrainCar"/>: its name, speed limits, hit points and what happens when it blows up.
/// </summary>
/// <remarks>
/// Original source: <c>object\train.cpp</c>, 0x50 bytes. Read from the "Train" block of its FIT. The constructor was
/// inline in <c>ObjectTypeManager::load</c> (object type class 0x14).
/// </remarks>
class TrainCarType : public ObjectType
{
public:
    /// <summary>Clears every field; TonnageClass starts at -1 and <see cref="unknown48"/> at -1.</summary>
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    TrainCarType();
    /// <remarks>MCX.EXE @ 0x00690a40 (vector deleting destructor)</remarks>
    ~TrainCarType() override { destroy(); }

    /// <summary>Makes a <see cref="TrainCar"/> of this type and gives it the next object id.</summary>
    /// <remarks>MCX.EXE @ 0x0069a0e0</remarks>
    BaseObject* createInstance() override;
    /// <remarks>MCX.EXE @ 0x0069a360</remarks>
    void destroy() override;
    /// <summary>
    /// Reads Name, Explosion Chance, Explosion Damage, Velocity Multiplier, Acceleration, Deceleration, TopSpeed,
    /// Damage and TonnageClass from the "Train" block, then the common type data.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069a370</remarks>
    int32_t init(File* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A car running into something: a derailed car hurts movers it lands on; a running one stops the train on a
    /// heavy obstacle and trades damage (scaled by the train's tonnage) with movers, buildings and trees.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0069a580</remarks>
    int handleCollision(GameObject* collidee, GameObject* collider) override;
    /// <summary>Makes this type's explosion at the car, sized by <see cref="explosionDamage"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0069a7f0</remarks>
    int handleDestruction(GameObject* collidee, GameObject* collider) override;

    /// <summary>String resource id of the car's name (FIT "Name").</summary>
    int32_t nameId; // +0x30
    /// <summary>FIT "Explosion Chance".</summary>
    uint8_t explosionChance; // +0x34
    /// <summary>FIT "Explosion Damage"; also the size of the explosion made by handleDestruction.</summary>
    uint8_t explosionDamage; // +0x35
    /// <summary>FIT "Velocity Multiplier".</summary>
    uint8_t velocityMultiplier; // +0x36
    /// <summary>Top speed (FIT "TopSpeed"); <see cref="TrainCar::GetMaxSpeed"/>.</summary>
    float topSpeed; // +0x38
    /// <summary>FIT "Acceleration"; <see cref="TrainCar::GetMaxAccel"/>.</summary>
    float acceleration; // +0x3c
    /// <summary>FIT "Deceleration"; <see cref="TrainCar::GetMaxDecel"/>.</summary>
    float deceleration; // +0x40
    /// <summary>Hit points of a car (FIT "Damage"); a car that has taken half of them may derail.</summary>
    int32_t damage; // +0x44
    /// <summary>Set to -1 by the constructor and never read in train.cpp.</summary>
    int32_t unknown48; // +0x48
    /// <summary>FIT "TonnageClass", given to the car's tonnage (-1 until read).</summary>
    float tonnageClass; // +0x4c
};

/// <summary>
/// One car of a <see cref="Train"/>. The train moves its cars along the track; a car hit hard enough, or running
/// onto a mine or off the rails, derails and leaves the train, splitting it.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>, <c>object\train.h</c>; 0xd4 bytes. Object class 0x1d.</remarks>
class TrainCar : public BigGameObject
{
public:
    TrainCar() { init(); }
    /// <remarks>MCX.EXE @ 0x0069a310 (vector deleting destructor)</remarks>
    ~TrainCar() override { destroy(); }

    using BigGameObject::setPartId;

    /// <summary>Clears the fields: no appearance, name or train; on the map; just created; no sound.</summary>
    /// <remarks>MCX.EXE @ 0x0069a1e0 (inline in <c>object\train.h</c>)</remarks>
    void init() override;
    /// <summary>Copies the type's name, hit points and tonnage, and makes the car's GV appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0069a980</remarks>
    int32_t init(ObjectType* objType) override;
    /// <summary>Frees the name.</summary>
    /// <remarks>MCX.EXE @ 0x0069ab20</remarks>
    void destroy() override;
    /// <remarks>MCX.EXE @ 0x0069a300</remarks>
    int32_t kill() override { return 0; }
    /// <summary>Derails a car that has taken enough damage, blows it on a mine tile, and advances the appearance.</summary>
    /// <remarks>MCX.EXE @ 0x0069ad10</remarks>
    int32_t update() override;
    /// <summary>Draws the car (or its destruction effect) and plays or stops its moving sound.</summary>
    /// <remarks>MCX.EXE @ 0x0069af10</remarks>
    void render() override;
    /// <remarks>MCX.EXE @ 0x0069a2f0</remarks>
    Appearance* getAppearance() override { return appearance; }
    /// <summary>Checks the car against the static objects of its terrain block.</summary>
    /// <remarks>MCX.EXE @ 0x0069a830</remarks>
    void handleStaticCollision() override;
    /// <remarks>MCX.EXE @ 0x0069ac60</remarks>
    int onScreen() override;
    /// <summary>Takes the damage; at zero hit points the car derails and leaves its train.</summary>
    /// <remarks>MCX.EXE @ 0x0069b300</remarks>
    int32_t handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>A position at <paramref name="angle"/> and <paramref name="radius"/> relative to the car's frame.</summary>
    /// <remarks>MCX.EXE @ 0x0069be10</remarks>
    vector_3d relativePosition(float angle, float radius, uint32_t flags) override;
    /// <summary>Returns the car's frame of reference.</summary>
    /// <remarks>MCX.EXE @ 0x0069a230 (inline in <c>object\train.h</c>)</remarks>
    frame_of_ref getFrame() override;
    /// <summary>Sets the car's frame of reference.</summary>
    /// <remarks>MCX.EXE @ 0x0069a290 (inline in <c>object\train.h</c>)</remarks>
    void setFrame(frame_of_ref& newFrame) override;
    /// <summary>The angle from the car's facing to <paramref name="goal"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0069bc50</remarks>
    float relFacingTo(vector_3d goal, int32_t bodyLocation) override;
    /// <summary>Whether the car's tile is seen by the home team.</summary>
    /// <remarks>MCX.EXE @ 0x0069ab70</remarks>
    int isRevealed() override;

    /// <summary>Sets the part id from the car's train number and its place in the train.</summary>
    /// <remarks>MCX.EXE @ 0x0069ab50</remarks>
    virtual void setPartId(int32_t trainNumber, int32_t carNumber);

    /// <summary>Knocks the car off the rails, turning it by <paramref name="angle"/>; it may fall in or be destroyed.</summary>
    /// <remarks>MCX.EXE @ 0x0069b3e0</remarks>
    void derail(float angle);
    /// <summary>Sets off a mine on the car's cell, damaging the car.</summary>
    /// <remarks>MCX.EXE @ 0x0069baa0</remarks>
    void mineCheck();
    /// <remarks>MCX.EXE @ 0x0069c310</remarks>
    float GetMaxAccel();
    /// <remarks>MCX.EXE @ 0x0069c320</remarks>
    float GetMaxDecel();
    /// <remarks>MCX.EXE @ 0x0069c330</remarks>
    float GetMaxSpeed();

    /// <summary>The car's name, loaded from the type's string resource (heap copy).</summary>
    char* name; // +0x84
    /// <summary>The car's GV appearance.</summary>
    Appearance* appearance; // +0x88
    /// <summary>The car's orientation.</summary>
    frame_of_ref frame; // +0x8c (0x24 bytes)
    /// <summary>The car's speed, copied from its train every update.</summary>
    float speed; // +0xb0
    /// <summary>1 once the car is off the rails; the train then stops moving it.</summary>
    int32_t derailed; // +0xb4
    /// <summary>Set when the car is wrecked for good (on a mine-marked tile in update, or derailed onto a tile of terrain type 0x2b): no update or draw.</summary>
    int32_t wrecked; // +0xb8
    /// <summary>Whether the car's cell is inside the map.</summary>
    int32_t onMap; // +0xbc
    /// <summary>Set by init; the first update clears it.</summary>
    int32_t justCreated; // +0xc0
    /// <summary>Damage taken so far; past half the type's hit points the car may derail.</summary>
    float damageTaken; // +0xc4
    /// <summary>The entry angle of the last hit; the car derails by it.</summary>
    float lastHitAngle; // +0xc8
    /// <summary>The moving sound's handle, or -1.</summary>
    uint32_t soundHandle; // +0xcc
    /// <summary>The train the car belongs to.</summary>
    Train* train; // +0xd0
};

/// <summary>A node of a train's list of cars.</summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0xc bytes. Allocated from the system heap.</remarks>
class TrainListEntry
{
public:
    /// <remarks>MCX.EXE @ 0x0069c340</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0069c360</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x0069c380</remarks>
    TrainListEntry();
    /// <summary>Clears the car and the links.</summary>
    /// <remarks>MCX.EXE @ 0x0069c390</remarks>
    void init();

    TrainCar* car; // +0x0
    /// <summary>The car behind.</summary>
    TrainListEntry* next; // +0x4
    /// <summary>The car ahead.</summary>
    TrainListEntry* prev; // +0x8
};

/// <summary>
/// A train: a list of cars moved together along the track, with a speed limited by its slowest car.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0x2c bytes. Allocated from the system heap.</remarks>
class Train
{
public:
    /// <remarks>MCX.EXE @ 0x006991b0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x006991d0</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x006991f0</remarks>
    Train();
    /// <summary>Frees the car list.</summary>
    /// <remarks>MCX.EXE @ 0x00699200</remarks>
    ~Train();

    /// <remarks>MCX.EXE @ 0x00699210</remarks>
    void init();
    /// <summary>Frees the car list.</summary>
    /// <remarks>MCX.EXE @ 0x00699230</remarks>
    void destroy();
    /// <summary>
    /// Accelerates or brakes toward the desired speed (stopping if any car derailed), then moves every car along
    /// the track, moving the cars' passability marks on the move map with them.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00699250</remarks>
    void Update();
    /// <summary>Appends a car (placing it <c>carOffset</c> behind the last one).</summary>
    /// <returns>The number of cars, or an error when the object is not a train car.</returns>
    /// <remarks>MCX.EXE @ 0x00699c20</remarks>
    int32_t AddCar(TrainCar* car);
    /// <summary>
    /// Removes a car. With <paramref name="justUnlink"/> 0 the car and those behind it become new trains; a train
    /// left with no cars removes and deletes itself.
    /// </summary>
    /// <returns>The number of cars left, or -1 when the car isn't in the train.</returns>
    /// <remarks>MCX.EXE @ 0x00699ea0</remarks>
    int32_t RemoveCar(TrainCar* car, int justUnlink);
    /// <summary>Recomputes the limits from the cars and the lead car's position, and clamps the desired speed.</summary>
    /// <remarks>MCX.EXE @ 0x00699fc0</remarks>
    void RecalcInfo();
    /// <summary>The sum of the cars' tonnage.</summary>
    /// <remarks>MCX.EXE @ 0x0069a0a0</remarks>
    float GetTotalTonnage();

    /// <summary>The lead car's list entry.</summary>
    TrainListEntry* cars; // +0x0
    int32_t numCars;      // +0x4
    /// <summary>The current speed (negative when backing).</summary>
    float speed; // +0x8
    /// <summary>The best acceleration of the cars.</summary>
    float maxAccel; // +0xc
    /// <summary>The best deceleration of the cars.</summary>
    float maxDecel; // +0x10
    /// <summary>The lowest top speed of the cars.</summary>
    float maxSpeed; // +0x14
    /// <summary>The speed the train drives toward (set by the scenario; clamped to +/- maxSpeed).</summary>
    float desiredSpeed; // +0x18
    /// <summary>
    /// The track direction in degrees, set by the scenario; -45 and 135 mark cells along one axis, anything else the
    /// other, when the cars' move-map marks are moved.
    /// </summary>
    int32_t trackDirection; // +0x1c
    /// <summary>The lead car's position at the last RecalcInfo.</summary>
    vector_3d leadPosition; // +0x20
};

/// <summary>Keeps every train of the mission (up to 64) and updates them each frame.</summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0x104 bytes. Allocated from the system heap.</remarks>
class TrainManager
{
public:
    static constexpr int32_t MAX_TRAINS = 64;

    /// <remarks>MCX.EXE @ 0x0069c3a0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <remarks>MCX.EXE @ 0x0069c3c0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Clears the train list.</summary>
    /// <remarks>MCX.EXE @ 0x0069c3e0</remarks>
    void init();
    /// <summary>Destroys and frees every train.</summary>
    /// <remarks>MCX.EXE @ 0x0069c400</remarks>
    void destroy();
    /// <summary>Makes a new empty train, or returns null when the list is full.</summary>
    /// <remarks>MCX.EXE @ 0x0069c440</remarks>
    Train* CreateTrain();
    /// <summary>Takes a train out of the list (it isn't freed).</summary>
    /// <remarks>MCX.EXE @ 0x0069c480</remarks>
    void RemoveTrain(Train* train);
    /// <summary>Updates every train.</summary>
    /// <remarks>MCX.EXE @ 0x0069c4e0</remarks>
    void UpdateTrains();

    Train* trains[MAX_TRAINS]; // +0x0
    int32_t numTrains;         // +0x100
};

/// <summary>The distance between two cars of a train (added along the track axis by <see cref="Train::AddCar"/>).</summary>
/// <remarks>MCX.EXE @ 0x00792d9c</remarks>
extern float carOffset;

/// <summary>The mission's train manager.</summary>
/// <remarks>MCX.EXE @ 0x007e4958 (globals_by_file.md files it under mission\scenario.cpp, its other user).</remarks>
extern TrainManager* trainManager;
