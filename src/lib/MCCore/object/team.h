#pragma once

#include "lib/cvmath.h"

class MCFitIniFile;
class MCGameObject;
class MCMover;
class MCScenario;
class MCSensorSystem;
class MCSortList;
struct MCPotentialContact;

/// <summary>Maximum contacts a team's LOS list and its sensor list each hold.</summary>
constexpr int32_t MAX_TEAM_CONTACTS = 500;

/// <summary>
/// A jammer or ECM a team carries (<see cref="MCTeam::AddJammer"/>, <see cref="MCTeam::AddEcm"/>): kept in a list sorted
/// by strength, strongest first.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>; 0x14 bytes.</remarks>
struct MCSystemTracker
{
    /// <summary>The object carrying it; cleared when removed.</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>Its component, in MasterComponentList.</summary>
    int32_t MasterId = 0;
    /// <summary>The component's range/strength (MasterComponent +0x54), the list's sort key.</summary>
    float Effect = 0;
    /// <summary>Previous (stronger) entry.</summary>
    MCSystemTracker* Prev = nullptr;
    /// <summary>Next (weaker) entry.</summary>
    MCSystemTracker* Next = nullptr;
};

/// <summary>
/// One side of the battle (Inner Sphere, clan or allied): its roster of movers (by part id), its sensors, the
/// contacts it sees by line of sight and by sensors, and its jammers and ECM.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>, <c>object\team.h</c>; 0x818 bytes.</remarks>
class MCTeam
{
public:
    /// <summary>Clears everything; three sensors updated a frame.</summary>
    virtual void Init();
    /// <summary>Sets the team id and makes room for <paramref name="maxSensors"/> sensors.</summary>
    virtual int32_t Init(int32_t newId, int32_t maxSensors);
    virtual int32_t Init(MCFitIniFile* file) { return 0; }
    /// <summary>Fills <paramref name="objects"/> with the roster's movers that still exist; returns how many.</summary>
    virtual int32_t GetRoster(MCGameObject** objects);
    /// <summary>Counts an enemy contact; the home team is then in contact.</summary>
    virtual void IncNumEnemyContacts();
    /// <summary>Uncounts an enemy contact; fatal when it goes negative.</summary>
    virtual void DecNumEnemyContacts();
    /// <summary>Adds a sensor to the team's list; fatal when full.</summary>
    virtual void AddSensor(MCSensorSystem* sensor);
    /// <summary>Takes a sensor off the list (the last one fills its slot).</summary>
    virtual void RemoveSensor(MCSensorSystem* sensor);
    /// <summary>Updates every sensor's scan, then re-checks a few sensors' contacts in turn.</summary>
    virtual void UpdateSensors();
    /// <summary>
    /// Fills <paramref name="contacts"/> with the part ids of the team's LOS contacts (or sensor contacts, flag
    /// 0x10) that pass the filter flags (1 enemies only, 8 not already challenged), sorted by
    /// <paramref name="sortType"/> (0 none, 1 by the target's value, 2 by distance from <paramref name="looker"/>).
    /// Returns how many.
    /// </summary>
    virtual int32_t GetContacts(MCGameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType);
    /// <summary>How the team knows <paramref name="object"/> (<see cref="MCContactStatus"/>).</summary>
    virtual int32_t GetContactType(MCGameObject* object);
    /// <summary>Forces every sensor to scan now.</summary>
    virtual void ScanBattlefield();
    /// <summary>Frees the sensors list, the jammer and ECM lists and the roster.</summary>
    virtual void Destroy();

    /// <summary>Counts the scenario's mover parts on this team and keeps their part ids.</summary>
    void BuildRoster(MCScenario* scenario);
    /// <summary>Fills <paramref name="objects"/> with the LOS contacts; returns how many.</summary>
    int32_t GetLosContacts(MCGameObject** objects);
    /// <summary>Fills <paramref name="objects"/> with the sensor contacts; returns how many.</summary>
    int32_t GetSensorContacts(MCGameObject** objects);
    /// <summary>
    /// Whether <paramref name="object"/> is a contact that passes the filter flags (1 enemies only, 2 visual only,
    /// 8 not already challenged).
    /// </summary>
    int IsContact(MCGameObject* object, int32_t contactCriteria);
    /// <summary>
    /// Adds a LOS contact. The original checks the sensor list's count against the limit (sic).
    /// </summary>
    void AddLosContact(MCPotentialContact* contact);
    /// <summary>Drops the LOS contact in slot <paramref name="index"/> (the last fills the gap).</summary>
    void RemoveLosContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> from the LOS list if there.</summary>
    void RemoveLosContact(MCPotentialContact* contact);
    /// <summary>Adds a sensor contact.</summary>
    void AddSensorContact(MCPotentialContact* contact);
    /// <summary>Drops the sensor contact in slot <paramref name="index"/> (the last fills the gap).</summary>
    void RemoveSensorContact(int32_t index);
    /// <summary>Drops <paramref name="contact"/> from the sensor list if there.</summary>
    void RemoveSensorContact(MCPotentialContact* contact);
    /// <summary>Debug: every mover's current target (if a mover) is sent message 0x42.</summary>
    void DisableTargets();
    /// <summary>Debug: every mover's current target (if a mover) takes a hundred heavy hits.</summary>
    void DestroyTargets();
    /// <summary>
    /// Whether any of the roster (other than part <paramref name="exceptPartId"/>, when nonzero) targets part
    /// <paramref name="targetPartId"/>.
    /// </summary>
    int IsTargeting(uint32_t targetPartId, uint32_t exceptPartId);
    /// <summary>Adds a jammer (component <paramref name="masterId"/> of <paramref name="owner"/>).</summary>
    MCSystemTracker* AddJammer(MCGameObject* owner, int32_t masterId);
    /// <summary>Removes and frees a jammer.</summary>
    void RemoveJammer(MCSystemTracker* tracker);
    /// <summary>The strongest working jammer's effect; 1 for none.</summary>
    float GetJammerEffect();
    /// <summary>Adds an ECM (component <paramref name="masterId"/> of <paramref name="owner"/>).</summary>
    MCSystemTracker* AddEcm(MCGameObject* owner, int32_t masterId);
    /// <summary>Removes and frees an ECM.</summary>
    void RemoveEcm(MCSystemTracker* tracker);
    /// <summary>
    /// The effect (MasterComponent +0x58) of the first ECM (components 0x26, 0x2a) whose carrier is alive, awake,
    /// status 0 and within its range of <paramref name="position"/>; 1 for none.
    /// </summary>
    float GetEcmEffect(MCVector3D position);
    /// <summary>
    /// The direction away from the team's movers within <paramref name="range"/> of <paramref name="mover"/>,
    /// normalized.
    /// </summary>
    MCVector3D CalcEscapeVector(MCMover* mover, float range);
    /// <summary>
    /// Adds the roster to <paramref name="counts"/>: [status 0..5] by status, [6] pilot ejected, [7] asleep, [8]
    /// gone.
    /// </summary>
    void StatusCount(int32_t* counts);
    /// <summary>Whether <paramref name="position"/> is visible to the team (its terrain visibility bits).</summary>
    int LineOfSight(MCVector3D position);

    /// <summary>0 Inner Sphere, 1 clan, 2 allied: the index into the per-team arrays.</summary>
    int32_t Id = 0;
    /// <summary>-1 Inner Sphere, 1 clan (and allied).</summary>
    int32_t Alignment = 0;
    /// <summary>Movers on the roster.</summary>
    int32_t RosterSize = 0;
    /// <summary>The roster's part ids.</summary>
    std::unique_ptr<int32_t[]> Roster;
    /// <summary>The team's first objective in <c>Scenario::objectives</c> (the tactical map's mission page).</summary>
    int32_t FirstObjective = 0;
    /// <summary>How many objectives are the team's (set by Scenario::init).</summary>
    uint32_t NumObjectives = 0;
    /// <summary>Sensors whose contacts updateSensors re-checks per frame (up to 3).</summary>
    int32_t SensorsPerUpdate = 3;
    /// <summary>The next sensor updateSensors re-checks.</summary>
    int32_t NextSensorUpdate = 0;
    /// <summary>The LOS contacts, by _PotentialContact::id.</summary>
    int16_t LosContacts[MAX_TEAM_CONTACTS] = {};
    /// <summary>How many.</summary>
    int32_t NumLosContacts = 0;
    /// <summary>The sensor contacts, by _PotentialContact::id.</summary>
    int16_t SensorContacts[MAX_TEAM_CONTACTS] = {};
    /// <summary>How many.</summary>
    int32_t NumSensorContacts = 0;
    /// <summary>Enemy contacts (incNumEnemyContacts / decNumEnemyContacts).</summary>
    int32_t NumEnemyContacts = 0;
    /// <summary>The team's sensors.</summary>
    std::unique_ptr<MCSensorSystem*[]> Sensors;
    /// <summary>Room in <see cref="Sensors"/>.</summary>
    int32_t MaxSensors = 0;
    /// <summary>Sensors on the list.</summary>
    int32_t NumSensors = 0;
    /// <summary>The ECM list, strongest first.</summary>
    MCSystemTracker* EcmList = nullptr;
    /// <summary>The jammer list, strongest first.</summary>
    MCSystemTracker* JammerList = nullptr;
};

/// <summary>Debug key: <see cref="MCTeam::DisableTargets"/> for the home team.</summary>
void DisableHomeTeamTargets();
/// <summary>Debug key: <see cref="MCTeam::DestroyTargets"/> for the home team.</summary>
void KillHomeTeamTargets();

/// <summary>The clan team.</summary>
extern MCTeam* ClanTeam;
/// <summary>The allied team.</summary>
extern MCTeam* AlliedTeam;
/// <summary>The Inner Sphere team.</summary>
extern MCTeam* InnerSphereTeam;
/// <summary>The player's team.</summary>
extern MCTeam* HomeTeam;
/// <summary>Sorts Team::getContacts' results (made on first use).</summary>
extern MCSortList* ContactSortList;
/// <summary>Set while the home team has an enemy contact.</summary>
extern int InContact;
