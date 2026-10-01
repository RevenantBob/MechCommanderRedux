#pragma once

#include "lib/cvmath.h"

class FitIniFile;
class GameObject;
class Mover;
class Scenario;
class SensorSystem;
class SortList;
struct _PotentialContact;

/// <summary>Maximum contacts a team's LOS list and its sensor list each hold.</summary>
constexpr int32_t MAX_TEAM_CONTACTS = 500;

/// <summary>
/// A jammer or ECM a team carries (<see cref="Team::addJammer"/>, <see cref="Team::addECM"/>): kept in a list sorted
/// by strength, strongest first.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>; 0x14 bytes, allocated from systemHeap.</remarks>
struct _SystemTracker
{
    /// <summary>The object carrying it; cleared when removed.</summary>
    GameObject* owner; // +0x00
    /// <summary>Its component, in MasterComponentList.</summary>
    int32_t masterId; // +0x04
    /// <summary>The component's range/strength (MasterComponent +0x54), the list's sort key.</summary>
    float effect; // +0x08
    /// <summary>Previous (stronger) entry.</summary>
    _SystemTracker* prev; // +0x0c
    /// <summary>Next (weaker) entry.</summary>
    _SystemTracker* next; // +0x10
};

/// <summary>
/// One side of the battle (Inner Sphere, clan or allied): its roster of movers (by part id), its sensors, the
/// contacts it sees by line of sight and by sensors, and its jammers and ECM.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>, <c>object\team.h</c>; 0x818 bytes.</remarks>
class Team
{
public:
    /// <summary>Clears everything; three sensors updated a frame.</summary>
    /// <remarks>MCX.EXE @ 0x00697050</remarks>
    virtual void init();
    /// <summary>Sets the team id and makes room for <paramref name="maxSensors"/> sensors (from systemHeap).</summary>
    /// <remarks>MCX.EXE @ 0x006970b0</remarks>
    virtual int32_t init(int32_t newId, int32_t maxSensors);
    /// <remarks>MCX.EXE @ 0x007367d0 (inline in <c>object\team.h</c>)</remarks>
    virtual int32_t init(FitIniFile* file) { return 0; }
    /// <summary>Fills <paramref name="objects"/> with the roster's movers that still exist; returns how many.</summary>
    /// <remarks>MCX.EXE @ 0x00697980</remarks>
    virtual int32_t getRoster(GameObject** objects);
    /// <summary>Counts an enemy contact; the home team is then in contact.</summary>
    /// <remarks>MCX.EXE @ 0x00697740</remarks>
    virtual void incNumEnemyContacts();
    /// <summary>Uncounts an enemy contact; fatal when it goes negative.</summary>
    /// <remarks>MCX.EXE @ 0x00697770</remarks>
    virtual void decNumEnemyContacts();
    /// <summary>Adds a sensor to the team's list; fatal when full.</summary>
    /// <remarks>MCX.EXE @ 0x00697250</remarks>
    virtual void addSensor(SensorSystem* sensor);
    /// <summary>Takes a sensor off the list (the last one fills its slot).</summary>
    /// <remarks>MCX.EXE @ 0x006972a0</remarks>
    virtual void removeSensor(SensorSystem* sensor);
    /// <summary>Updates every sensor's scan, then re-checks a few sensors' contacts in turn.</summary>
    /// <remarks>MCX.EXE @ 0x00697300</remarks>
    virtual void updateSensors();
    /// <summary>
    /// Fills <paramref name="contacts"/> with the part ids of the team's LOS contacts (or sensor contacts, flag
    /// 0x10) that pass the filter flags (1 enemies only, 8 not already challenged), sorted by
    /// <paramref name="sortType"/> (0 none, 1 by the target's value, 2 by distance from <paramref name="looker"/>).
    /// Returns how many.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00697400</remarks>
    virtual int32_t getContacts(GameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType);
    /// <summary>How the team knows <paramref name="object"/> (<see cref="ContactStatus"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00697650</remarks>
    virtual int32_t getContactType(GameObject* object);
    /// <summary>Forces every sensor to scan now.</summary>
    /// <remarks>MCX.EXE @ 0x00697710</remarks>
    virtual void scanBattlefield();
    /// <summary>Frees the sensors list, the jammer and ECM lists and the roster.</summary>
    /// <remarks>MCX.EXE @ 0x00698260</remarks>
    virtual void destroy();

    /// <summary>Counts the scenario's mover parts on this team and keeps their part ids.</summary>
    /// <remarks>MCX.EXE @ 0x006970f0</remarks>
    void buildRoster(Scenario* scenario);
    /// <summary>Fills <paramref name="objects"/> with the LOS contacts; returns how many.</summary>
    /// <remarks>MCX.EXE @ 0x00697360</remarks>
    int32_t getLOSContacts(GameObject** objects);
    /// <summary>Fills <paramref name="objects"/> with the sensor contacts; returns how many.</summary>
    /// <remarks>MCX.EXE @ 0x006973b0</remarks>
    int32_t getSensorContacts(GameObject** objects);
    /// <summary>
    /// Whether <paramref name="object"/> is a contact that passes the filter flags (1 enemies only, 2 visual only,
    /// 8 not already challenged).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00697670</remarks>
    int isContact(GameObject* object, int32_t contactCriteria);
    /// <summary>
    /// Adds a LOS contact. The original checks the sensor list's count against the limit (sic).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006977b0</remarks>
    void addLOSContact(_PotentialContact* contact);
    /// <summary>Drops the LOS contact in slot <paramref name="index"/> (the last fills the gap).</summary>
    /// <remarks>MCX.EXE @ 0x006977f0</remarks>
    void removeLOSContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> from the LOS list if there.</summary>
    /// <remarks>MCX.EXE @ 0x00697860</remarks>
    void removeLOSContact(_PotentialContact* contact);
    /// <summary>Adds a sensor contact.</summary>
    /// <remarks>MCX.EXE @ 0x00697890</remarks>
    void addSensorContact(_PotentialContact* contact);
    /// <summary>Drops the sensor contact in slot <paramref name="index"/> (the last fills the gap).</summary>
    /// <remarks>MCX.EXE @ 0x006978d0</remarks>
    void removeSensorContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> from the sensor list if there.</summary>
    /// <remarks>MCX.EXE @ 0x00697950</remarks>
    void removeSensorContact(_PotentialContact* contact);
    /// <summary>Debug: every mover's current target (if a mover) is sent message 0x42.</summary>
    /// <remarks>MCX.EXE @ 0x006979d0</remarks>
    void disableTargets();
    /// <summary>Debug: every mover's current target (if a mover) takes a hundred heavy hits.</summary>
    /// <remarks>MCX.EXE @ 0x00697a50</remarks>
    void destroyTargets();
    /// <summary>
    /// Whether any of the roster (other than part <paramref name="exceptPartId"/>, when nonzero) targets part
    /// <paramref name="targetPartId"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00697b60</remarks>
    int isTargeting(uint32_t targetPartId, uint32_t exceptPartId);
    /// <summary>Adds a jammer (component <paramref name="masterId"/> of <paramref name="owner"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00697c20</remarks>
    _SystemTracker* addJammer(GameObject* owner, int32_t masterId);
    /// <summary>Removes and frees a jammer.</summary>
    /// <remarks>MCX.EXE @ 0x00697d00</remarks>
    void removeJammer(_SystemTracker* tracker);
    /// <summary>The strongest working jammer's effect; 1 for none.</summary>
    /// <remarks>MCX.EXE @ 0x00697d50</remarks>
    float getJammerEffect();
    /// <summary>Adds an ECM (component <paramref name="masterId"/> of <paramref name="owner"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00697d80</remarks>
    _SystemTracker* addECM(GameObject* owner, int32_t masterId);
    /// <summary>Removes and frees an ECM.</summary>
    /// <remarks>MCX.EXE @ 0x00697e60</remarks>
    void removeECM(_SystemTracker* tracker);
    /// <summary>
    /// The effect (MasterComponent +0x58) of the first ECM (components 0x26, 0x2a) whose carrier is alive, awake,
    /// status 0 and within its range of <paramref name="position"/>; 1 for none.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00697eb0</remarks>
    float getECMEffect(vector_3d position);
    /// <summary>
    /// The direction away from the team's movers within <paramref name="range"/> of <paramref name="mover"/>,
    /// normalized.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00697f50</remarks>
    vector_3d calcEscapeVector(Mover* mover, float range);
    /// <summary>
    /// Adds the roster to <paramref name="counts"/>: [status 0..5] by status, [6] pilot ejected, [7] asleep, [8]
    /// gone.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00698180</remarks>
    void statusCount(int32_t* counts);
    /// <summary>Whether <paramref name="position"/> is visible to the team (its terrain visibility bits).</summary>
    /// <remarks>MCX.EXE @ 0x006982f0</remarks>
    int lineOfSight(vector_3d position);

    /// <summary>0 Inner Sphere, 1 clan, 2 allied: the index into the per-team arrays.</summary>
    int32_t id = 0; // +0x04
    /// <summary>-1 Inner Sphere, 1 clan (and allied).</summary>
    int32_t alignment = 0; // +0x08
    /// <summary>Movers on the roster.</summary>
    int32_t rosterSize = 0; // +0x0c
    /// <summary>The roster's part ids (systemHeap).</summary>
    int32_t* roster = nullptr; // +0x10
    /// <summary>The team's first objective in <c>Scenario::objectives</c> (the tactical map's mission page).</summary>
    int32_t firstObjective = 0; // +0x14
    /// <summary>How many objectives are the team's (set by Scenario::init).</summary>
    uint32_t numObjectives = 0; // +0x18
    /// <summary>Set to 1 by init.</summary>
    int32_t unknown1C = 1; // +0x1c
    /// <summary>Sensors whose contacts updateSensors re-checks per frame (up to 3).</summary>
    int32_t sensorsPerUpdate = 3; // +0x20
    /// <summary>The next sensor updateSensors re-checks.</summary>
    int32_t nextSensorUpdate = 0; // +0x24
    /// <summary>The LOS contacts, by _PotentialContact::id.</summary>
    int16_t losContacts[MAX_TEAM_CONTACTS] = {}; // +0x28
    /// <summary>How many.</summary>
    int32_t numLOSContacts = 0; // +0x410
    /// <summary>The sensor contacts, by _PotentialContact::id.</summary>
    int16_t sensorContacts[MAX_TEAM_CONTACTS] = {}; // +0x414
    /// <summary>How many.</summary>
    int32_t numSensorContacts = 0; // +0x7fc
    /// <summary>Enemy contacts (incNumEnemyContacts / decNumEnemyContacts).</summary>
    int32_t numEnemyContacts = 0; // +0x800
    /// <summary>The team's sensors (systemHeap).</summary>
    SensorSystem** sensors = nullptr; // +0x804
    /// <summary>Room in <see cref="sensors"/>.</summary>
    int32_t maxSensors = 0; // +0x808
    /// <summary>Sensors on the list.</summary>
    int32_t numSensors = 0; // +0x80c
    /// <summary>The ECM list, strongest first.</summary>
    _SystemTracker* ecmList = nullptr; // +0x810
    /// <summary>The jammer list, strongest first.</summary>
    _SystemTracker* jammerList = nullptr; // +0x814
};

/// <summary>Debug key: <see cref="Team::disableTargets"/> for the home team.</summary>
/// <remarks>MCX.EXE @ 0x006983e0</remarks>
void disableHomeTeamTargets();
/// <summary>Debug key: <see cref="Team::destroyTargets"/> for the home team.</summary>
/// <remarks>MCX.EXE @ 0x006983f0</remarks>
void killHomeTeamTargets();

/// <summary>The clan team.</summary>
extern Team* clanTeam;
/// <summary>The allied team.</summary>
extern Team* alliedTeam;
/// <summary>The Inner Sphere team.</summary>
extern Team* innerSphereTeam;
/// <summary>The player's team.</summary>
extern Team* homeTeam;
/// <summary>Sorts Team::getContacts' results (made on first use).</summary>
extern SortList* ContactSortList;
/// <summary>Set while the home team has an enemy contact.</summary>
extern int inContact;
