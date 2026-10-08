#pragma once

#include "object/MCMoverGroup.h"

class MCMover;
class MCTeam;

/// <summary>
/// A side's commander: its team, its 32 groups (lances), and the support it can call in (air strikes, sensor
/// probes, camera drones).
/// </summary>
/// <remarks>Original source: <c>object\comndr.cpp</c>, <c>object\comndr.h</c>. The forces own the commanders
/// (<see cref="MCForces"/>).</remarks>
class MCCommander
{
public:
    /// <summary>Groups a commander has (kept: ABL addresses them as groups 1..32 of each side).</summary>
    static constexpr int32_t MaxGroups = 32;

    /// <summary>Commander <paramref name="id"/>, with no team, its groups 0..31 empty, and no support.</summary>
    explicit MCCommander(int32_t id);
    MCCommander(const MCCommander&) = delete;
    MCCommander& operator=(const MCCommander&) = delete;

    int32_t GetId() const { return Id; }
    MCTeam* GetTeam() const { return Team; }
    void SetTeam(MCTeam* newTeam) { Team = newTeam; }
    MCMoverGroup* GetGroup(int32_t groupId) { return &Groups[groupId]; }

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

    /// <summary>The commander's id.</summary>
    int32_t Id = -1;
    /// <summary>Its team.</summary>
    MCTeam* Team = nullptr;
    /// <summary>Its groups, by id.</summary>
    std::array<MCMoverGroup, MaxGroups> Groups;
    /// <summary>Small air strikes left.</summary>
    int32_t NumSmallStrikes = 0;
    /// <summary>Large air strikes left.</summary>
    int32_t NumLargeStrikes = 0;
    /// <summary>Sensor probes left.</summary>
    int32_t NumSensorStrikes = 0;
    /// <summary>Camera drones left.</summary>
    int32_t NumCameraDrones = 0;
};
