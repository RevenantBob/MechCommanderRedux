#pragma once

#include "lib/MCVector3D.h"

class MCGameObject;
class MCMover;
class MCScenario;
class MCSensorSystem;
struct MCPotentialContact;

/// <summary>
/// A jammer or ECM a team carries (<see cref="MCTeam::AddJammer"/>, <see cref="MCTeam::AddEcm"/>): kept in a list sorted
/// by strength, strongest first.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>.</remarks>
struct MCSystemTracker
{
    /// <summary>The object carrying it.</summary>
    MCGameObject* Owner = nullptr;
    /// <summary>Its component, in MasterComponentList.</summary>
    int32_t MasterId = 0;
    /// <summary>The component's range (a jammer's) or effect (an ECM's), the list's sort key.</summary>
    float Effect = 0;
};

/// <summary>
/// One side of the battle (Inner Sphere, clan or allied): its roster of movers (by part id), its sensors, the
/// contacts it sees by line of sight and by sensors, and its jammers and ECM.
/// </summary>
/// <remarks>Original source: <c>object\team.cpp</c>, <c>object\team.h</c>. The forces own the teams
/// (<see cref="MCForces"/>).</remarks>
class MCTeam
{
public:
    /// <summary>
    /// Team <paramref name="id"/> (0 Inner Sphere, 1 clan, 2 allied) of side <paramref name="alignment"/> (-1 clan,
    /// 1 Inner Sphere and allied), with no roster, sensors or contacts.
    /// </summary>
    MCTeam(int32_t id, int32_t alignment);
    MCTeam(const MCTeam&) = delete;
    MCTeam& operator=(const MCTeam&) = delete;

    /// <summary>Fills <paramref name="objects"/> with the roster's movers that still exist; returns how many.</summary>
    int32_t GetRoster(MCGameObject** objects);
    /// <summary>Counts an enemy contact; the home team is then in contact.</summary>
    void IncNumEnemyContacts();
    /// <summary>Uncounts an enemy contact; fatal when it goes negative.</summary>
    void DecNumEnemyContacts();
    /// <summary>Adds a sensor to the team's list.</summary>
    void AddSensor(MCSensorSystem* sensor);
    /// <summary>Takes a sensor off the list (the last one fills its slot).</summary>
    void RemoveSensor(MCSensorSystem* sensor);
    /// <summary>Updates every sensor's scan, then re-checks a few sensors' contacts in turn.</summary>
    void UpdateSensors();
    /// <summary>
    /// Fills <paramref name="contacts"/> with the part ids of the team's LOS contacts (or sensor contacts, flag
    /// 0x10) that pass the filter flags (1 enemies only, 8 not already challenged), sorted by
    /// <paramref name="sortType"/> (0 none, 1 by the target's value, 2 by distance from <paramref name="looker"/>).
    /// Returns how many.
    /// </summary>
    int32_t GetContacts(MCGameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType);
    /// <summary>How the team knows <paramref name="object"/> (<see cref="MCContactStatus"/>).</summary>
    int32_t GetContactType(MCGameObject* object);
    /// <summary>Forces every sensor to scan now.</summary>
    void ScanBattlefield();
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
    /// <summary>Adds a LOS contact.</summary>
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
    /// <summary>Removes a jammer.</summary>
    void RemoveJammer(MCSystemTracker* tracker);
    /// <summary>The strongest jammer's effect; 1 for none.</summary>
    float GetJammerEffect();
    /// <summary>Adds an ECM (component <paramref name="masterId"/> of <paramref name="owner"/>).</summary>
    MCSystemTracker* AddEcm(MCGameObject* owner, int32_t masterId);
    /// <summary>Removes an ECM.</summary>
    void RemoveEcm(MCSystemTracker* tracker);
    /// <summary>
    /// The effect (the component's damage column) of the first ECM (components 0x26, 0x2a) whose carrier is alive,
    /// awake, status 0 and within its range of <paramref name="position"/>; 1 for none.
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
    /// <summary>The team's sensors.</summary>
    std::span<MCSensorSystem* const> Sensors() const { return _Sensors; }
    /// <summary>The LOS contacts, by <c>MCPotentialContact::Id</c>.</summary>
    std::span<const uint16_t> LosContacts() const { return _LosContacts; }
    /// <summary>The sensor contacts, by <c>MCPotentialContact::Id</c>.</summary>
    std::span<const uint16_t> SensorContacts() const { return _SensorContacts; }
    /// <summary>LOS contacts.</summary>
    int32_t NumLosContacts() const { return static_cast<int32_t>(_LosContacts.size()); }
    /// <summary>Sensor contacts.</summary>
    int32_t NumSensorContacts() const { return static_cast<int32_t>(_SensorContacts.size()); }
    /// <summary>The movers on the roster.</summary>
    int32_t RosterSize() const { return static_cast<int32_t>(Roster.size()); }

    /// <summary>0 Inner Sphere, 1 clan, 2 allied: the index into the per-team arrays.</summary>
    int32_t Id = 0;
    /// <summary>-1 Inner Sphere, 1 clan (and allied).</summary>
    int32_t Alignment = 0;
    /// <summary>The roster's part ids.</summary>
    std::vector<int32_t> Roster;
    /// <summary>The team's first objective in <c>Scenario::objectives</c> (the tactical map's mission page).</summary>
    int32_t FirstObjective = 0;
    /// <summary>How many objectives are the team's (set by Scenario::init).</summary>
    uint32_t NumObjectives = 0;
    /// <summary>Sensors whose contacts updateSensors re-checks per frame (up to 3).</summary>
    int32_t SensorsPerUpdate = 3;
    /// <summary>The next sensor updateSensors re-checks.</summary>
    int32_t NextSensorUpdate = 0;
    /// <summary>Enemy contacts (incNumEnemyContacts / decNumEnemyContacts).</summary>
    int32_t NumEnemyContacts = 0;

private:
    std::vector<MCSensorSystem*> _Sensors;
    std::vector<uint16_t> _LosContacts;
    std::vector<uint16_t> _SensorContacts;
    /// <summary>The ECMs, strongest first (the owners keep pointers to their entries).</summary>
    std::list<MCSystemTracker> _Ecms;
    /// <summary>The jammers, strongest first.</summary>
    std::list<MCSystemTracker> _Jammers;
};

/// <summary>Debug key: <see cref="MCTeam::DisableTargets"/> for the home team.</summary>
void DisableHomeTeamTargets();
/// <summary>Debug key: <see cref="MCTeam::DestroyTargets"/> for the home team.</summary>
void KillHomeTeamTargets();

/// <summary>Set while the home team has an enemy contact.</summary>
extern int InContact;
