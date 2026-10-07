#pragma once

#include "object/gameobj.h"
#include "object/objtype.h"

class MCAppearance;
class MCFile;
class MCGameObject;
class MCTrain;
struct MCWeaponShotInfo;

/// <summary>
/// The type of a <see cref="MCTrainCar"/>: its name, speed limits, hit points and what happens when it blows up.
/// </summary>
/// <remarks>
/// Original source: <c>object\train.cpp</c>, 0x50 bytes. Read from the "Train" block of its FIT. The constructor was
/// inline in <c>ObjectTypeManager::load</c> (object type class 0x14).
/// </remarks>
class MCTrainCarType : public MCObjectType
{
public:
    /// <summary>Clears every field; TonnageClass starts at -1.</summary>
    /// <remarks>Inline in ObjectTypeManager::load.</remarks>
    MCTrainCarType();
    ~MCTrainCarType() override { Destroy(); }

    /// <summary>Makes a <see cref="MCTrainCar"/> of this type and gives it the next object id.</summary>
    MCBaseObject* CreateInstance() override;
    void Destroy() override;
    /// <summary>
    /// Reads Name, Explosion Chance, Explosion Damage, Velocity Multiplier, Acceleration, Deceleration, TopSpeed,
    /// Damage and TonnageClass from the "Train" block, then the common type data.
    /// </summary>
    int32_t Init(MCFile* objFile, uint32_t fileSize) override;
    /// <summary>
    /// A car running into something: a derailed car hurts movers it lands on; a running one stops the train on a
    /// heavy obstacle and trades damage (scaled by the train's tonnage) with movers, buildings and trees.
    /// </summary>
    int HandleCollision(MCGameObject* collidee, MCGameObject* collider) override;
    /// <summary>Makes this type's explosion at the car, sized by <see cref="ExplosionDamage"/>.</summary>
    int HandleDestruction(MCGameObject* collidee, MCGameObject* collider) override;

    /// <summary>String resource id of the car's name (FIT "Name").</summary>
    int32_t NameId = 0;
    /// <summary>FIT "Explosion Chance".</summary>
    uint8_t ExplosionChance = 0;
    /// <summary>FIT "Explosion Damage"; also the size of the explosion made by handleDestruction.</summary>
    uint8_t ExplosionDamage = 0;
    /// <summary>FIT "Velocity Multiplier".</summary>
    uint8_t VelocityMultiplier = 0;
    /// <summary>Top speed (FIT "TopSpeed"); <see cref="MCTrainCar::GetMaxSpeed"/>.</summary>
    float TopSpeed = 0;
    /// <summary>FIT "Acceleration"; <see cref="MCTrainCar::GetMaxAccel"/>.</summary>
    float Acceleration = 0;
    /// <summary>FIT "Deceleration"; <see cref="MCTrainCar::GetMaxDecel"/>.</summary>
    float Deceleration = 0;
    /// <summary>Hit points of a car (FIT "Damage"); a car that has taken half of them may derail.</summary>
    int32_t Damage = 0;
    /// <summary>FIT "TonnageClass", given to the car's tonnage (-1 until read).</summary>
    float TonnageClass = 0;
};

/// <summary>
/// One car of a <see cref="MCTrain"/>. The train moves its cars along the track; a car hit hard enough, or running
/// onto a mine or off the rails, derails and leaves the train, splitting it.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>, <c>object\train.h</c>; 0xd4 bytes. Object class 0x1d.</remarks>
class MCTrainCar : public MCBigGameObject
{
public:
    MCTrainCar() { Init(); }
    ~MCTrainCar() override { Destroy(); }

    using MCBigGameObject::SetPartId;

    /// <summary>Clears the fields: no appearance, name or train; on the map; just created; no sound.</summary>
    void Init() override;
    /// <summary>Copies the type's name, hit points and tonnage, and makes the car's GV appearance.</summary>
    int32_t Init(MCObjectType* objType) override;
    /// <summary>Frees the name.</summary>
    void Destroy() override;
    int32_t Kill() override { return 0; }
    /// <summary>Derails a car that has taken enough damage, blows it on a mine tile, and advances the appearance.</summary>
    int32_t Update() override;
    /// <summary>Draws the car (or its destruction effect) and plays or stops its moving sound.</summary>
    void Render() override;
    MCAppearance* GetAppearance() override { return Appearance; }
    /// <summary>Checks the car against the static objects of its terrain block.</summary>
    void HandleStaticCollision() override;
    int OnScreen() override;
    /// <summary>Takes the damage; at zero hit points the car derails and leaves its train.</summary>
    int32_t HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) override;
    /// <summary>A position at <paramref name="angle"/> and <paramref name="radius"/> relative to the car's frame.</summary>
    MCVector3D RelativePosition(float angle, float radius, uint32_t flags) override;
    /// <summary>Returns the car's frame of reference.</summary>
    MCFrameOfRef GetFrame() override;
    /// <summary>Sets the car's frame of reference.</summary>
    void SetFrame(MCFrameOfRef& newFrame) override;
    /// <summary>The angle from the car's facing to <paramref name="goal"/>.</summary>
    float RelFacingTo(MCVector3D goal, int32_t bodyLocation) override;
    /// <summary>Whether the car's tile is seen by the home team.</summary>
    int IsRevealed() override;

    /// <summary>Sets the part id from the car's train number and its place in the train.</summary>
    virtual void SetPartId(int32_t trainNumber, int32_t carNumber);

    /// <summary>Knocks the car off the rails, turning it by <paramref name="angle"/>; it may fall in or be destroyed.</summary>
    void Derail(float angle);
    /// <summary>Sets off a mine on the car's cell, damaging the car.</summary>
    void MineCheck();
    float GetMaxAccel();
    float GetMaxDecel();
    float GetMaxSpeed();

    /// <summary>The car's name, loaded from the type's string resource.</summary>
    std::string Name;
    /// <summary>The car's GV appearance.</summary>
    MCAppearance* Appearance = nullptr;
    /// <summary>The car's orientation.</summary>
    MCFrameOfRef Frame; // (0x24 bytes)
    /// <summary>The car's speed, copied from its train every update.</summary>
    float Speed = 0;
    /// <summary>1 once the car is off the rails; the train then stops moving it.</summary>
    int32_t Derailed = 0;
    /// <summary>Set when the car is wrecked for good (on a mine-marked tile in update, or derailed onto a tile of terrain type 0x2b): no update or draw.</summary>
    int32_t Wrecked = 0;
    /// <summary>Whether the car's cell is inside the map.</summary>
    int32_t OnMap = 0;
    /// <summary>Set by init; the first update clears it.</summary>
    int32_t JustCreated = 0;
    /// <summary>Damage taken so far; past half the type's hit points the car may derail.</summary>
    float DamageTaken = 0;
    /// <summary>The entry angle of the last hit; the car derails by it.</summary>
    float LastHitAngle = 0;
    /// <summary>The train the car belongs to.</summary>
    MCTrain* Train = nullptr;
};

/// <summary>A node of a train's list of cars.</summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0xc bytes.</remarks>
class MCTrainListEntry
{
public:
    MCTrainListEntry();
    /// <summary>Clears the car and the links.</summary>
    void Init();

    MCTrainCar* Car = nullptr;
    /// <summary>The car behind.</summary>
    MCTrainListEntry* Next = nullptr;
    /// <summary>The car ahead.</summary>
    MCTrainListEntry* Prev = nullptr;
};

/// <summary>
/// A train: a list of cars moved together along the track, with a speed limited by its slowest car.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0x2c bytes.</remarks>
class MCTrain
{
public:
    MCTrain();
    /// <summary>Frees the car list.</summary>
    ~MCTrain();

    void Init();
    /// <summary>Frees the car list.</summary>
    void Destroy();
    /// <summary>
    /// Accelerates or brakes toward the desired speed (stopping if any car derailed), then moves every car along
    /// the track, moving the cars' passability marks on the move map with them.
    /// </summary>
    void Update();
    /// <summary>Appends a car (placing it <c>carOffset</c> behind the last one).</summary>
    /// <returns>The number of cars, or an error when the object is not a train car.</returns>
    int32_t AddCar(MCTrainCar* car);
    /// <summary>
    /// Removes a car. With <paramref name="justUnlink"/> 0 the car and those behind it become new trains; a train
    /// left with no cars removes and deletes itself.
    /// </summary>
    /// <returns>The number of cars left, or -1 when the car isn't in the train.</returns>
    int32_t RemoveCar(MCTrainCar* car, int justUnlink);
    /// <summary>Recomputes the limits from the cars and the lead car's position, and clamps the desired speed.</summary>
    void RecalcInfo();
    /// <summary>The sum of the cars' tonnage.</summary>
    float GetTotalTonnage();

    /// <summary>The lead car's list entry.</summary>
    MCTrainListEntry* Cars = nullptr;
    int32_t NumCars = 0;
    /// <summary>The current speed (negative when backing).</summary>
    float Speed = 0;
    /// <summary>The best acceleration of the cars.</summary>
    float MaxAccel = 0;
    /// <summary>The best deceleration of the cars.</summary>
    float MaxDecel = 0;
    /// <summary>The lowest top speed of the cars.</summary>
    float MaxSpeed = 0;
    /// <summary>The speed the train drives toward (set by the scenario; clamped to +/- maxSpeed).</summary>
    float DesiredSpeed = 0;
    /// <summary>
    /// The track direction in degrees, set by the scenario; -45 and 135 mark cells along one axis, anything else the
    /// other, when the cars' move-map marks are moved.
    /// </summary>
    int32_t TrackDirection = 0;
    /// <summary>The lead car's position at the last RecalcInfo.</summary>
    MCVector3D LeadPosition;
};

/// <summary>Keeps every train of the mission (up to 64) and updates them each frame.</summary>
/// <remarks>Original source: <c>object\train.cpp</c>, 0x104 bytes.</remarks>
class MCTrainManager
{
public:
    static constexpr int32_t MAX_TRAINS = 64;

    /// <summary>Clears the train list.</summary>
    void Init();
    /// <summary>Destroys and frees every train.</summary>
    void Destroy();
    /// <summary>Makes a new empty train, or returns null when the list is full.</summary>
    MCTrain* CreateTrain();
    /// <summary>Takes a train out of the list (it isn't freed).</summary>
    void RemoveTrain(MCTrain* train);
    /// <summary>Updates every train.</summary>
    void UpdateTrains();

    MCTrain* Trains[MAX_TRAINS]{};
    int32_t NumTrains = 0;
};

/// <summary>The distance between two cars of a train (added along the track axis by <see cref="MCTrain::AddCar"/>).</summary>
extern float CarOffset;

/// <summary>The mission's train manager.</summary>
extern MCTrainManager* TrainManager;
