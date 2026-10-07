#pragma once

// Potential contacts and sensor systems (original source: object\contact.cpp): every object that can be seen is a
// _PotentialContact on one of three lists; each team keeps which it sees by line of sight or by sensors.

class MCBigGameObject;
class MCFitIniFile;
class MCGameObject;
class MCSortList;
class MCTeam;

/// <summary>Which of the potential contact manager's lists an object is on (by alignment).</summary>
enum MCPotentialContactType : int32_t
{
    /// <summary>Inner Sphere objects.</summary>
    POTENTIAL_CONTACT_INNER_SPHERE = 0,
    /// <summary>Clan objects.</summary>
    POTENTIAL_CONTACT_CLAN = 1,
    /// <summary>Allied objects.</summary>
    POTENTIAL_CONTACT_ALLIED = 2,
};

/// <summary>How a team knows a contact (<see cref="MCPotentialContact::ContactStatus"/>).</summary>
enum MCContactStatus : uint8_t
{
    CONTACT_NONE = 0,
    /// <summary>In the team's line of sight.</summary>
    CONTACT_VISUAL = 1,
    /// <summary>Only on the team's sensors.</summary>
    CONTACT_SENSOR = 2,
};

/// <summary>Maximum number of sensor systems (<see cref="MCSensorSystemManager"/>).</summary>
constexpr int32_t MAX_SENSORS = 0x41;
/// <summary>Maximum number of contacts one sensor holds.</summary>
constexpr int32_t MAX_SENSOR_CONTACTS = 200;

/// <summary>
/// An object that can be seen, and how each team sees it. Pooled by the <see cref="MCPotentialContactManager"/>.
/// </summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x60 bytes, packed (the object pointer is at +0x02).</remarks>
struct MCPotentialContact
{
    /// <summary>Clears the per-team state and sensor slots; list type 2, visibility 0.</summary>
    void Init();
    /// <summary>
    /// Works out whether <paramref name="team"/> sees the object by line of sight, by sensors only, or not, and
    /// moves it between the team's LOS and sensor contact lists when that changes.
    /// </summary>
    void UpdateStatus(MCTeam* team);

    /// <summary>Index in the manager's pool (and in the sensors' contact lists).</summary>
    uint16_t Id = 0;
    /// <summary>The object.</summary>
    MCBigGameObject* Object = nullptr;
    /// <summary>The manager list it is on (<see cref="MCPotentialContactType"/>).</summary>
    int8_t ContactType = 0;
    /// <summary>How visible it is: 2 can't be sensed at all, 3 can't be seen by line of sight.</summary>
    int8_t Visibility = 0;
    /// <summary>Per team: <see cref="MCContactStatus"/>.</summary>
    uint8_t ContactStatus[3]{};
    /// <summary>Per team: how many of its sensors hold the contact.</summary>
    int8_t NumSensors[3]{};
    /// <summary>Per team: it went from visual to sensor contact (BigGameObject::getContactType's tagged).</summary>
    uint8_t LostVisual[3]{};
    /// <summary>Per sensor: the contact's slot in that sensor's list, 0xff for none.</summary>
    uint8_t SensorSlot[MAX_SENSORS]{};
    /// <summary>Per team: the contact's slot in that team's LOS or sensor list, -1 for none.</summary>
    int16_t TeamSlot[3]{};
    /// <summary>Previous on the manager list.</summary>
    MCPotentialContact* Prev = nullptr;
    /// <summary>Next on the manager list (or the free list).</summary>
    MCPotentialContact* Next = nullptr;
};

/// <summary>The pool of potential contacts and the three lists they are on.</summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x1c bytes.</remarks>
class MCPotentialContactManager
{
public:
    /// <summary>
    /// Reads "MaxPotentialContacts" from the "PotentialContactManager" block (fatal below 2) and chains that many
    /// contacts into the free list.
    /// </summary>
    int32_t Init(MCFitIniFile* file);
    /// <summary>Takes a free contact for <paramref name="object"/> onto list <paramref name="type"/>; fatal when
    /// none is left.</summary>
    MCPotentialContact* Add(int32_t type, MCBigGameObject* object, char visibility);
    /// <summary>
    /// Counts what team <paramref name="teamId"/> knows of the enemy list (and, unless
    /// <paramref name="enemiesOnly"/>, of every list): counts[0] tagged, [1] visual, [2] sensor contacts.
    /// </summary>
    int32_t GetContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly);
    /// <summary>Takes <paramref name="contact"/> off every sensor and team list and back to the free list.</summary>
    void Remove(MCPotentialContact* contact);
    /// <summary>Moves <paramref name="contact"/> to list <paramref name="type"/> with a new visibility.</summary>
    void Move(MCPotentialContact* contact, int32_t type, char visibility);
    /// <summary>Updates every contact's status for the two teams it isn't on.</summary>
    void UpdateStatus();
    /// <summary>Frees the pool.</summary>
    void Destroy();

    /// <summary>Pool size (FIT "MaxPotentialContacts").</summary>
    int32_t MaxContacts = 0;
    /// <summary>Contacts on the free list.</summary>
    int32_t NumFree = 0;
    /// <summary>The first contact of each <see cref="MCPotentialContactType"/> list.</summary>
    MCPotentialContact* ContactList[3] = {};
    /// <summary>The pool.</summary>
    std::unique_ptr<MCPotentialContact[]> Contacts;
    /// <summary>The first free contact.</summary>
    MCPotentialContact* FreeList = nullptr;
};

/// <summary>
/// One object's sensors: a range (by the owner's speed and its pilot's sensor skill), scaled by the enemy's
/// jammers and ECM, and the contacts it currently holds. Pooled by the <see cref="MCSensorSystemManager"/>.
/// </summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0x1ec bytes.</remarks>
class MCSensorSystem
{
public:
    /// <summary>
    /// Takes the next sensor id, no owner, team or range; staggers the first scan by id (0.1 s each, from 0.25 s);
    /// scans every ContactUpdateFrequency seconds; makes the shared sort list with the first sensor.
    /// </summary>
    void Init();
    /// <summary>Drops the shared sort list with the last sensor.</summary>
    void Destroy();
    /// <summary>
    /// Sets the range; a mover's is split into three by speed state, reduced by its pilot's sensor skill
    /// (SensorSkillMoveRange / SensorSkillMoveFactor).
    /// </summary>
    void SetRange(float newRange);
    /// <summary>
    /// The range now: a mover's for its speed state, eased over six turns when that changes; times the team
    /// multiplier.
    /// </summary>
    float GetSkilledRange();
    /// <summary>Leaves the old team's sensors and joins <paramref name="newTeam"/>'s (fatal for a team id with no team).</summary>
    void SetTeam(MCTeam* newTeam);
    /// <summary>
    /// Whether the sensors work: on a team, the owner alive and awake, and (for a mover) its sensor component
    /// present and undamaged.
    /// </summary>
    int Enabled();
    /// <summary>Drops every contact and leaves the team.</summary>
    void Disable();
    /// <summary>The weaker of <paramref name="team"/>'s jamming and its ECM at the owner's position.</summary>
    float CalcTeamEffect(MCTeam* team);
    /// <summary>Once per scenario time: each team's effect on these sensors, and the one that applies.</summary>
    void CalcTeamMultipliers();
    /// <summary>Adds <paramref name="contact"/> unless full or already held.</summary>
    void AddSensorContact(MCPotentialContact* contact);
    /// <summary>Drops the contact in slot <paramref name="index"/> (the last one fills the gap).</summary>
    void RemoveSensorContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> if held.</summary>
    void RemoveSensorContact(MCPotentialContact* contact);
    /// <summary>Drops every contact.</summary>
    void ClearSensorContacts();
    /// <summary>
    /// Re-checks the contacts held (once per scenario time): drops the disabled and those out of range. The
    /// original's name is lost.
    /// </summary>
    void UpdateContacts();
    /// <summary>
    /// When due (every scanFrequency seconds) or <paramref name="forceScan"/>: scans the battlefield, and has the
    /// pilot report new contacts.
    /// </summary>
    void UpdateScan(int forceScan);
    /// <summary>Scans list <paramref name="type"/> for other teams' objects; returns the newly sensed ones.</summary>
    int32_t ScanBattlefield(MCPotentialContactType type);
    /// <summary>Scans the lists the owner's alignment can see; returns the newly sensed.</summary>
    int32_t ScanBattlefield();
    /// <summary>
    /// Whether <paramref name="target"/> is on these sensors: in range, sensable, and (for a mover) the sensor
    /// working; a probe extends the range for hidden (status 5) targets.
    /// </summary>
    int OnSensors(MCGameObject* target);

    /// <summary>Index in the manager's pool (and in _PotentialContact::sensorSlot).</summary>
    int32_t Id = 0;
    /// <summary>The object carrying the sensors.</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>The owner's team.</summary>
    MCTeam* Team = nullptr;
    /// <summary>0 Inner Sphere, 1 clan, 2 allied; -1 for none.</summary>
    int32_t TeamIndex = -1;
    /// <summary>Slot in the team's sensor list (set by Team::addSensor); -1 when not on a team.</summary>
    int32_t TeamSensorSlot = -1;
    /// <summary>The base range; -1 for no sensors.</summary>
    float Range = -1.0f;
    /// <summary>A mover's range by speed state (still, moving, running).</summary>
    float SpeedRange[3] = {};
    /// <summary>The range last settled on (getSkilledRange eases toward the new one).</summary>
    float CurrentRange = 0.0f;
    /// <summary>The turn the easing ends; -1 when settled.</summary>
    int32_t RangeChangeTurn = -1;
    /// <summary>Each team's jamming/ECM effect on these sensors (Inner Sphere, clan, allied).</summary>
    float TeamMultiplier[3] = {1.0f, 1.0f, 1.0f};
    /// <summary>The effect that applies (the enemy's).</summary>
    float Multiplier = 1.0f;
    /// <summary>Scenario time of the next scan.</summary>
    float NextScanTime = 0.0f;
    /// <summary>Scenario time of the last scan.</summary>
    float LastScanTime = 0.0f;
    /// <summary>Scenario time calcTeamMultipliers last ran.</summary>
    float LastMultiplierTime = 0.0f;
    /// <summary>Seconds between scans (4).</summary>
    float ScanFrequency = 0.0f;
    /// <summary>The contacts held, by _PotentialContact::id.</summary>
    uint16_t Contacts[MAX_SENSOR_CONTACTS] = {};
    /// <summary>How many.</summary>
    int32_t NumContacts = 0;
    /// <summary>Contacts newly sensed, over the mission.</summary>
    int32_t TotalContacts = 0;
    /// <summary>Previous in the manager's free list.</summary>
    MCSensorSystem* Prev = nullptr;
    /// <summary>Next in the manager's free list.</summary>
    MCSensorSystem* Next = nullptr;

    /// <summary>Sensors made (the next id).</summary>
    static int32_t NumSensors;
    /// <summary>Shared by every sensor: made by the first init, freed by the last destroy.</summary>
    static MCSortList* SortList;
};

/// <summary>The pool of <see cref="MAX_SENSORS"/> sensor systems.</summary>
/// <remarks>Original source: <c>object\contact.cpp</c>; 0xc bytes.</remarks>
class MCSensorSystemManager
{
public:
    /// <summary>Makes the sensors and chains them into the free list; fatal without memory.</summary>
    int32_t Init(MCFitIniFile* file);
    /// <summary>Takes a free sensor; fatal when none is left.</summary>
    MCSensorSystem* NewSensor();
    /// <summary>Returns <paramref name="sensor"/> to the free list.</summary>
    void FreeSensor(MCSensorSystem* sensor);
    /// <summary>
    /// Meant to delete every sensor, but its duplicate check nulls each entry before testing it, so it only frees
    /// the table: the sensors stay in objectCache and SensorSystem::numSensors keeps counting.
    /// </summary>
    void Destroy();

    /// <summary>Sensors on the free list.</summary>
    int32_t NumFree = 0;
    /// <summary>Every sensor, by id.</summary>
    std::unique_ptr<MCSensorSystem*[]> Sensors;
    /// <summary>The first free sensor.</summary>
    MCSensorSystem* FreeList = nullptr;
};

/// <summary>The potential contacts.</summary>
extern MCPotentialContactManager* PotentialContactManager;
/// <summary>The sensor systems.</summary>
extern MCSensorSystemManager* SensorSystemManager;
/// <summary>Debug: every target is on every sensor.</summary>
extern int SensorAutomaticSuccess;
/// <summary>Sensor skill thresholds (45, 59, 69, 80) for SensorSkillMoveFactor's rows.</summary>
extern char SensorSkillMoveRange[4];
/// <summary>Per skill row: the moving and running range factors.</summary>
extern float SensorSkillMoveFactor[4][2];
/// <summary>Sensors "SensorModifiers" (loadMoverGameSystem).</summary>
extern float SensorModifier[8];
