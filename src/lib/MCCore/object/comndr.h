#pragma once

class Mover;
class MoverGroup;
class Team;

/// <summary>Groups a commander has.</summary>
constexpr int32_t MAX_GROUPS_PER_COMMANDER = 32;
/// <summary>Entries of <see cref="CommanderTable"/>.</summary>
constexpr int32_t MAX_COMMANDERS = 6;

/// <summary>
/// A side's commander: its team, its 32 groups (lances), and the support it can call in (air strikes, sensor
/// probes, camera drones).
/// </summary>
/// <remarks>Original source: <c>object\comndr.cpp</c>, <c>object\comndr.h</c>; 0x9c bytes.</remarks>
class Commander
{
public:
    /// <summary>No id or team; makes the 32 groups (ids 0..31); no support.</summary>
    /// <remarks>MCX.EXE @ 0x00658340</remarks>
    virtual void init();
    /// <summary>Deletes the groups.</summary>
    /// <remarks>MCX.EXE @ 0x00658430</remarks>
    virtual void destroy();
    /// <remarks>MCX.EXE @ 0x00736860 (inline in <c>object\comndr.h</c>)</remarks>
    virtual int32_t getId() { return id; }
    /// <remarks>MCX.EXE @ 0x00736870 (inline in <c>object\comndr.h</c>)</remarks>
    virtual void setId(int32_t newId) { id = newId; }
    /// <remarks>MCX.EXE @ 0x00736880 (inline in <c>object\comndr.h</c>)</remarks>
    virtual Team* getTeam() { return team; }
    /// <remarks>MCX.EXE @ 0x00736890 (inline in <c>object\comndr.h</c>)</remarks>
    virtual void setTeam(Team* newTeam) { team = newTeam; }
    /// <remarks>MCX.EXE @ 0x007368a0 (inline in <c>object\comndr.h</c>)</remarks>
    virtual MoverGroup* getGroup(int32_t groupId) { return groups[groupId]; }

    /// <summary>
    /// Disbands group <paramref name="groupId"/> and fills it with <paramref name="moverList"/> (each leaving its
    /// old group); member <paramref name="point"/> becomes the point.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00658470</remarks>
    int32_t setGroup(int32_t groupId, int32_t numMovers, Mover** moverList, int32_t point);
    /// <summary>Sets <c>Mover::netPlayerId</c> of every mover in every group.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x00658520. Unnamed in the binary (called once, from <c>Scenario::init</c> with 0 in single player);
    /// the name is the port's.
    /// </remarks>
    void setNetPlayerId(int32_t playerId);
    /// <summary>Adds the first four groups to the interface's mech list.</summary>
    /// <remarks>MCX.EXE @ 0x00658560</remarks>
    void addToGUI(int visible);
    /// <summary>Sets the small air strikes left (the home commander's redraws the tactical map).</summary>
    /// <remarks>MCX.EXE @ 0x00658590</remarks>
    void setNumSmallStrikes(int32_t strikes);
    /// <summary>Sets the large air strikes left.</summary>
    /// <remarks>MCX.EXE @ 0x006585c0</remarks>
    void setNumLargeStrikes(int32_t strikes);
    /// <summary>Sets the sensor probes left.</summary>
    /// <remarks>MCX.EXE @ 0x006585f0</remarks>
    void setNumSensorStrikes(int32_t strikes);
    /// <summary>Sets the camera drones left.</summary>
    /// <remarks>MCX.EXE @ 0x00658620</remarks>
    void setNumCameraDrones(int32_t drones);

    /// <summary>The commander's id; -1 for none.</summary>
    int32_t id = -1; // +0x04
    /// <summary>Its team.</summary>
    Team* team = nullptr; // +0x08
    /// <summary>Its groups, by id.</summary>
    MoverGroup* groups[MAX_GROUPS_PER_COMMANDER] = {}; // +0x0c
    /// <summary>Small air strikes left.</summary>
    int32_t numSmallStrikes = 0; // +0x8c
    /// <summary>Large air strikes left.</summary>
    int32_t numLargeStrikes = 0; // +0x90
    /// <summary>Sensor probes left.</summary>
    int32_t numSensorStrikes = 0; // +0x94
    /// <summary>Camera drones left.</summary>
    int32_t numCameraDrones = 0; // +0x98
};

/// <summary>Commanders in <see cref="CommanderTable"/>.</summary>
extern int32_t NumCommanders;
/// <summary>The commanders by id.</summary>
extern Commander* CommanderTable[MAX_COMMANDERS];
/// <summary>The player's commander.</summary>
extern Commander* HomeCommander;
