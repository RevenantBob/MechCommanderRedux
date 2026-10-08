#pragma once

#include "object/MCCommander.h"
#include "object/MCTeam.h"

/// <summary>
/// The mission's sides: the Inner Sphere, clan and (when the scenario has one) allied teams, the commanders that lead
/// them, and which of each the player is. A game system of <see cref="MCGameContext"/> (Forces()), made by the
/// scenario's Teams block and removed at its end.
/// </summary>
/// <remarks>The pieces were globals in the original (clanTeam, alliedTeam, ISTeam, homeTeam, TeamTable,
/// commanders, homeCommander, numCommanders).</remarks>
class MCForces
{
public:
    /// <summary>Teams by id: Inner Sphere, clan, allied.</summary>
    static constexpr int32_t NumTeams = 3;
    /// <summary>Commanders of a multiplayer game (kept: the players' check-in ids go up to 5 on the wire).</summary>
    static constexpr int32_t MaxCommanders = 6;
    /// <summary>The Inner Sphere team's id.</summary>
    static constexpr int32_t InnerSphereId = 0;
    /// <summary>The clan team's id.</summary>
    static constexpr int32_t ClanId = 1;
    /// <summary>The allied team's id.</summary>
    static constexpr int32_t AlliedId = 2;

    /// <summary>The Inner Sphere and clan teams, and an allied one when <paramref name="allied"/>.</summary>
    explicit MCForces(bool allied);
    ~MCForces();
    MCForces(const MCForces&) = delete;
    MCForces& operator=(const MCForces&) = delete;

    /// <summary>Makes commanders 0..<paramref name="count"/>-1 (dropping any made before).</summary>
    void MakeCommanders(int32_t count);
    /// <summary>Team <paramref name="id"/> (null when the scenario has no such team).</summary>
    MCTeam* Team(int32_t id) const { return Teams[id].get(); }

    /// <summary>The teams by id; the allied one is null without an allied team.</summary>
    std::array<std::unique_ptr<MCTeam>, NumTeams> Teams;
    /// <summary>The commanders by id.</summary>
    std::vector<std::unique_ptr<MCCommander>> Commanders;
    /// <summary>The player's team.</summary>
    MCTeam* PlayerTeam = nullptr;
    /// <summary>The player's commander.</summary>
    MCCommander* PlayerCommander = nullptr;
};

/// <summary>The mission's forces (null outside a mission).</summary>
MCForces* Forces();
/// <summary>The clan team (null without forces).</summary>
MCTeam* ClanTeam();
/// <summary>The allied team (null without one).</summary>
MCTeam* AlliedTeam();
/// <summary>The Inner Sphere team (null without forces).</summary>
MCTeam* InnerSphereTeam();
/// <summary>The player's team (null without forces).</summary>
MCTeam* HomeTeam();
/// <summary>Team <paramref name="id"/>: 0 Inner Sphere, 1 clan, 2 allied (the original's TeamTable).</summary>
MCTeam* TeamById(int32_t id);
/// <summary>Commanders in play.</summary>
int32_t NumCommanders();
/// <summary>Commander <paramref name="id"/> (the original's commanders table); null past the commanders in play.</summary>
MCCommander* CommanderById(int32_t id);
/// <summary>The player's commander (null without forces).</summary>
MCCommander* HomeCommander();
