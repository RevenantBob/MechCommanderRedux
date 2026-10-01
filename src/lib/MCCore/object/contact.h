#pragma once

// Potential contacts and sensor systems (original source: object\contact.cpp): every object that can be seen is a
// _PotentialContact on one of three lists; each team keeps which it sees by line of sight or by sensors.

class BigGameObject;
class FitIniFile;
class GameObject;
class SortList;
class Team;

/// <summary>Which of the potential contact manager's lists an object is on (by alignment).</summary>
enum PotentialContactType : int32_t
{
    /// <summary>Inner Sphere objects.</summary>
    POTENTIAL_CONTACT_INNER_SPHERE = 0,
    /// <summary>Clan objects.</summary>
    POTENTIAL_CONTACT_CLAN = 1,
    /// <summary>Allied objects.</summary>
    POTENTIAL_CONTACT_ALLIED = 2,
};

/// <summary>How a team knows a contact (<see cref="_PotentialContact::contactStatus"/>).</summary>
enum ContactStatus : uint8_t
{
    CONTACT_NONE = 0,
    /// <summary>In the team's line of sight.</summary>
    CONTACT_VISUAL = 1,
    /// <summary>Only on the team's sensors.</summary>
    CONTACT_SENSOR = 2,
};

/// <summary>Maximum number of sensor systems (<see cref="SensorSystemManager"/>).</summary>
constexpr int32_t MAX_SENSORS = 0x41;
/// <summary>Maximum number of contacts one sensor holds.</summary>
constexpr int32_t MAX_SENSOR_CONTACTS = 200;

/// <summary>
/// An object that can be seen, and how each team sees it. Pooled by the <see cref="PotentialContactManager"/>.
/// </summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x60 bytes, packed (the object pointer is at +0x02).</remarks>
struct _PotentialContact
{
    /// <summary>Clears the per-team state and sensor slots; list type 2, visibility 0.</summary>
    /// <remarks>MCX.EXE @ 0x00658650</remarks>
    void init();
    /// <summary>
    /// Works out whether <paramref name="team"/> sees the object by line of sight, by sensors only, or not, and
    /// moves it between the team's LOS and sensor contact lists when that changes.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006586a0</remarks>
    void updateStatus(Team* team);

    /// <summary>Index in the manager's pool (and in the sensors' contact lists).</summary>
    uint16_t id; // +0x00
    /// <summary>The object.</summary>
    BigGameObject* object; // +0x02
    /// <summary>The manager list it is on (<see cref="PotentialContactType"/>).</summary>
    int8_t contactType; // +0x06
    /// <summary>How visible it is: 2 can't be sensed at all, 3 can't be seen by line of sight.</summary>
    int8_t visibility; // +0x07
    /// <summary>Per team: <see cref="ContactStatus"/>.</summary>
    uint8_t contactStatus[3]; // +0x08
    /// <summary>Per team: how many of its sensors hold the contact.</summary>
    int8_t numSensors[3]; // +0x0b
    /// <summary>Per team: it went from visual to sensor contact (BigGameObject::getContactType's tagged).</summary>
    uint8_t lostVisual[3]; // +0x0e
    /// <summary>Per sensor: the contact's slot in that sensor's list, 0xff for none.</summary>
    uint8_t sensorSlot[MAX_SENSORS]; // +0x11
    /// <summary>Per team: the contact's slot in that team's LOS or sensor list, -1 for none.</summary>
    int16_t teamSlot[3]; // +0x52
    /// <summary>Previous on the manager list.</summary>
    _PotentialContact* prev; // +0x58
    /// <summary>Next on the manager list (or the free list).</summary>
    _PotentialContact* next; // +0x5c
};

/// <summary>The pool of potential contacts and the three lists they are on.</summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x1c bytes. Allocated from
/// <c>ObjectTypeManager::objectCache</c>.</remarks>
class PotentialContactManager
{
public:
    /// <summary>Allocates from <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x00658780</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006587a0</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Reads "MaxPotentialContacts" from the "PotentialContactManager" block (fatal below 2) and chains that many
    /// contacts into the free list.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006587c0</remarks>
    int32_t init(FitIniFile* file);
    /// <summary>Takes a free contact for <paramref name="object"/> onto list <paramref name="type"/>; fatal when
    /// none is left.</summary>
    /// <remarks>MCX.EXE @ 0x00658900</remarks>
    _PotentialContact* add(int32_t type, BigGameObject* object, char visibility);
    /// <summary>
    /// Counts what team <paramref name="teamId"/> knows of the enemy list (and, unless
    /// <paramref name="enemiesOnly"/>, of every list): counts[0] tagged, [1] visual, [2] sensor contacts.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658970</remarks>
    int32_t getContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly);
    /// <summary>Takes <paramref name="contact"/> off every sensor and team list and back to the free list.</summary>
    /// <remarks>MCX.EXE @ 0x00658a80</remarks>
    void remove(_PotentialContact* contact);
    /// <summary>Moves <paramref name="contact"/> to list <paramref name="type"/> with a new visibility.</summary>
    /// <remarks>MCX.EXE @ 0x00658b50</remarks>
    void move(_PotentialContact* contact, int32_t type, char visibility);
    /// <summary>Updates every contact's status for the two teams it isn't on.</summary>
    /// <remarks>MCX.EXE @ 0x00658bb0</remarks>
    void updateStatus();
    /// <summary>Frees the pool.</summary>
    /// <remarks>MCX.EXE @ 0x00658c30</remarks>
    void destroy();

    /// <summary>Pool size (FIT "MaxPotentialContacts").</summary>
    int32_t maxContacts = 0; // +0x00
    /// <summary>Contacts on the free list.</summary>
    int32_t numFree = 0; // +0x04
    /// <summary>The first contact of each <see cref="PotentialContactType"/> list.</summary>
    _PotentialContact* contactList[3] = {}; // +0x08
    /// <summary>The pool.</summary>
    _PotentialContact* contacts = nullptr; // +0x14
    /// <summary>The first free contact.</summary>
    _PotentialContact* freeList = nullptr; // +0x18
};

/// <summary>
/// One object's sensors: a range (by the owner's speed and its pilot's sensor skill), scaled by the enemy's
/// jammers and ECM, and the contacts it currently holds. Pooled by the <see cref="SensorSystemManager"/>.
/// </summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x1ec bytes. Allocated from
/// <c>ObjectTypeManager::objectCache</c>.</remarks>
class SensorSystem
{
public:
    /// <summary>Allocates from <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x00658c60</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x00658c80</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// Takes the next sensor id, no owner, team or range; staggers the first scan by id (0.1 s each, from 0.25 s);
    /// scans every ContactUpdateFrequency seconds; makes the shared sort list with the first sensor.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658ca0</remarks>
    void init();
    /// <summary>Drops the shared sort list with the last sensor.</summary>
    /// <remarks>MCX.EXE @ 0x00658d60</remarks>
    void destroy();
    /// <summary>
    /// Sets the range; a mover's is split into three by speed state, reduced by its pilot's sensor skill
    /// (SensorSkillMoveRange / SensorSkillMoveFactor).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658da0</remarks>
    void setRange(float newRange);
    /// <summary>
    /// The range now: a mover's for its speed state, eased over six turns when that changes; times the team
    /// multiplier.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658e70</remarks>
    float getSkilledRange();
    /// <summary>Leaves the old team's sensors and joins <paramref name="newTeam"/>'s (fatal for an unknown team).</summary>
    /// <remarks>MCX.EXE @ 0x00658f10</remarks>
    void setTeam(Team* newTeam);
    /// <summary>
    /// Whether the sensors work: on a team, the owner alive and awake, and (for a mover) its sensor component
    /// present and undamaged.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658fa0</remarks>
    int enabled();
    /// <summary>Drops every contact and leaves the team.</summary>
    /// <remarks>MCX.EXE @ 0x00659010</remarks>
    void disable();
    /// <summary>The weaker of <paramref name="team"/>'s jamming and its ECM at the owner's position.</summary>
    /// <remarks>MCX.EXE @ 0x00659030</remarks>
    float calcTeamEffect(Team* team);
    /// <summary>Once per scenario time: each team's effect on these sensors, and the one that applies.</summary>
    /// <remarks>MCX.EXE @ 0x00659090</remarks>
    void calcTeamMultipliers();
    /// <summary>Adds <paramref name="contact"/> unless full or already held.</summary>
    /// <remarks>MCX.EXE @ 0x006591b0</remarks>
    void addSensorContact(_PotentialContact* contact);
    /// <summary>Drops the contact in slot <paramref name="index"/> (the last one fills the gap).</summary>
    /// <remarks>MCX.EXE @ 0x00659200</remarks>
    void removeSensorContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> if held.</summary>
    /// <remarks>MCX.EXE @ 0x00659270</remarks>
    void removeSensorContact(_PotentialContact* contact);
    /// <summary>Drops every contact.</summary>
    /// <remarks>MCX.EXE @ 0x006592a0</remarks>
    void clearSensorContacts();
    /// <summary>
    /// Re-checks the contacts held (once per scenario time): drops the disabled and those out of range. The
    /// original's name is lost.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006592d0</remarks>
    void updateContacts();
    /// <summary>
    /// When due (every scanFrequency seconds) or <paramref name="forceScan"/>: scans the battlefield, and has the
    /// pilot report new contacts.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006593c0</remarks>
    void updateScan(int forceScan);
    /// <summary>Scans list <paramref name="type"/> for other teams' objects; returns the newly sensed ones.</summary>
    /// <remarks>MCX.EXE @ 0x006594c0</remarks>
    int32_t scanBattlefield(PotentialContactType type);
    /// <summary>Scans the lists the owner's alignment can see; returns the newly sensed.</summary>
    /// <remarks>MCX.EXE @ 0x00659590</remarks>
    int32_t scanBattlefield();
    /// <summary>
    /// Whether <paramref name="target"/> is on these sensors: in range, sensable, and (for a mover) the sensor
    /// working; a probe extends the range for hidden (status 5) targets.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00659600</remarks>
    int onSensors(GameObject* target);

    /// <summary>Index in the manager's pool (and in _PotentialContact::sensorSlot).</summary>
    int32_t id = 0; // +0x00
    /// <summary>The object carrying the sensors.</summary>
    GameObject* owner = nullptr; // +0x04
    /// <summary>The owner's team.</summary>
    Team* team = nullptr; // +0x08
    /// <summary>0 Inner Sphere, 1 clan, 2 allied; -1 for none.</summary>
    int32_t teamIndex = -1; // +0x0c
    /// <summary>Slot in the team's sensor list (set by Team::addSensor); -1 when not on a team.</summary>
    int32_t teamSensorSlot = -1; // +0x10
    /// <summary>The base range; -1 for no sensors.</summary>
    float range = -1.0f; // +0x14
    /// <summary>A mover's range by speed state (still, moving, running).</summary>
    float speedRange[3] = {}; // +0x18
    /// <summary>The range last settled on (getSkilledRange eases toward the new one).</summary>
    float currentRange = 0.0f; // +0x24
    /// <summary>The turn the easing ends; -1 when settled.</summary>
    int32_t rangeChangeTurn = -1; // +0x28
    /// <summary>Each team's jamming/ECM effect on these sensors (Inner Sphere, clan, allied).</summary>
    float teamMultiplier[3] = {1.0f, 1.0f, 1.0f}; // +0x2c
    /// <summary>The effect that applies (the enemy's).</summary>
    float multiplier = 1.0f; // +0x38
    /// <summary>Scenario time of the next scan.</summary>
    float nextScanTime = 0.0f; // +0x3c
    /// <summary>Scenario time of the last scan.</summary>
    float lastScanTime = 0.0f; // +0x40
    /// <summary>Scenario time calcTeamMultipliers last ran.</summary>
    float lastMultiplierTime = 0.0f; // +0x44
    /// <summary>Seconds between scans (4).</summary>
    float scanFrequency = 0.0f; // +0x48
    /// <summary>The contacts held, by _PotentialContact::id.</summary>
    uint16_t contacts[MAX_SENSOR_CONTACTS] = {}; // +0x4c
    /// <summary>How many.</summary>
    int32_t numContacts = 0; // +0x1dc
    /// <summary>Contacts newly sensed, over the mission.</summary>
    int32_t totalContacts = 0; // +0x1e0
    /// <summary>Previous in the manager's free list.</summary>
    SensorSystem* prev = nullptr; // +0x1e4
    /// <summary>Next in the manager's free list.</summary>
    SensorSystem* next = nullptr; // +0x1e8

    /// <summary>Sensors made (the next id).</summary>
    static int32_t numSensors;
    /// <summary>Shared by every sensor: made by the first init, freed by the last destroy.</summary>
    static SortList* sortList;
};

/// <summary>The pool of <see cref="MAX_SENSORS"/> sensor systems.</summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0xc bytes. Allocated from
/// <c>ObjectTypeManager::objectCache</c>.</remarks>
class SensorSystemManager
{
public:
    /// <summary>Allocates from <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006597b0</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into <c>ObjectTypeManager::objectCache</c>.</summary>
    /// <remarks>MCX.EXE @ 0x006597d0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Makes the sensors and chains them into the free list; fatal without memory.</summary>
    /// <remarks>MCX.EXE @ 0x006597f0</remarks>
    int32_t init(FitIniFile* file);
    /// <summary>Takes a free sensor; fatal when none is left.</summary>
    /// <remarks>MCX.EXE @ 0x006598e0</remarks>
    SensorSystem* newSensor();
    /// <summary>Returns <paramref name="sensor"/> to the free list.</summary>
    /// <remarks>MCX.EXE @ 0x00659930</remarks>
    void freeSensor(SensorSystem* sensor);
    /// <summary>
    /// Meant to delete every sensor, but its duplicate check nulls each entry before testing it, so it only frees
    /// the table: the sensors stay in objectCache and SensorSystem::numSensors keeps counting.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00659960</remarks>
    void destroy();

    /// <summary>Sensors on the free list.</summary>
    int32_t numFree = 0; // +0x00
    /// <summary>Every sensor, by id.</summary>
    SensorSystem** sensors = nullptr; // +0x04
    /// <summary>The first free sensor.</summary>
    SensorSystem* freeList = nullptr; // +0x08
};

/// <summary>The potential contacts.</summary>
extern PotentialContactManager* potentialContactManager;
/// <summary>The sensor systems.</summary>
extern SensorSystemManager* sensorSystemManager;
/// <summary>Debug: every target is on every sensor.</summary>
extern int SensorAutomaticSuccess;
/// <summary>Sensor skill thresholds (45, 59, 69, 80) for SensorSkillMoveFactor's rows.</summary>
extern char SensorSkillMoveRange[4];
/// <summary>Per skill row: the moving and running range factors.</summary>
extern float SensorSkillMoveFactor[4][2];
/// <summary>Sensors "SensorModifiers" (loadMoverGameSystem).</summary>
/// <remarks>MCX.EXE @ 0x0078eefc</remarks>
extern float SensorModifier[8];
