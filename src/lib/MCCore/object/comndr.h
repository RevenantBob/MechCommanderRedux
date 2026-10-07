#pragma once

class MCMover;
class MCMoverGroup;
class MCTeam;

/// <summary>Groups a commander has.</summary>
constexpr int32_t MAX_GROUPS_PER_COMMANDER = 32;
/// <summary>Entries of <see cref="CommanderTable"/>.</summary>
constexpr int32_t MAX_COMMANDERS = 6;

/// <summary>
/// A side's commander: its team, its 32 groups (lances), and the support it can call in (air strikes, sensor
/// probes, camera drones).
/// </summary>
/// <remarks>Original source: <c>object\comndr.cpp</c>, <c>object\comndr.h</c>; 0x9c bytes.</remarks>
class MCCommander
{
public:
    /// <summary>No id or team; makes the 32 groups (ids 0..31); no support.</summary>
    virtual void Init();
    /// <summary>Deletes the groups.</summary>
    virtual void Destroy();
    virtual int32_t GetId() { return Id; }
    virtual void SetId(int32_t newId) { Id = newId; }
    virtual MCTeam* GetTeam() { return Team; }
    virtual void SetTeam(MCTeam* newTeam) { Team = newTeam; }
    virtual MCMoverGroup* GetGroup(int32_t groupId) { return Groups[groupId]; }

    /// <summary>
    /// Disbands group <paramref name="groupId"/> and fills it with <paramref name="moverList"/> (each leaving its
    /// old group); member <paramref name="point"/> becomes the point.
    /// </summary>
    int32_t SetGroup(int32_t groupId, int32_t numMovers, MCMover** moverList, int32_t point);
    /// <summary>Sets <c>Mover::netPlayerId</c> of every mover in every group.</summary>
    /// <remarks>Called once, from <c>MCScenario::Init</c> with 0 in single player.</remarks>
    void SetNetPlayerId(int32_t playerId);
    /// <summary>Adds the first four groups to the interface's mech list.</summary>
    void AddToGui(int visible);
    /// <summary>Sets the small air strikes left (the home commander's redraws the tactical map).</summary>
    void SetNumSmallStrikes(int32_t strikes);
    /// <summary>Sets the large air strikes left.</summary>
    void SetNumLargeStrikes(int32_t strikes);
    /// <summary>Sets the sensor probes left.</summary>
    void SetNumSensorStrikes(int32_t strikes);
    /// <summary>Sets the camera drones left.</summary>
    void SetNumCameraDrones(int32_t drones);

    /// <summary>The commander's id; -1 for none.</summary>
    int32_t Id = -1;
    /// <summary>Its team.</summary>
    MCTeam* Team = nullptr;
    /// <summary>Its groups, by id.</summary>
    MCMoverGroup* Groups[MAX_GROUPS_PER_COMMANDER] = {};
    /// <summary>Small air strikes left.</summary>
    int32_t NumSmallStrikes = 0;
    /// <summary>Large air strikes left.</summary>
    int32_t NumLargeStrikes = 0;
    /// <summary>Sensor probes left.</summary>
    int32_t NumSensorStrikes = 0;
    /// <summary>Camera drones left.</summary>
    int32_t NumCameraDrones = 0;
};

/// <summary>Commanders in <see cref="CommanderTable"/>.</summary>
extern int32_t NumCommanders;
/// <summary>The commanders by id.</summary>
extern MCCommander* CommanderTable[MAX_COMMANDERS];
/// <summary>The player's commander.</summary>
extern MCCommander* HomeCommander;
