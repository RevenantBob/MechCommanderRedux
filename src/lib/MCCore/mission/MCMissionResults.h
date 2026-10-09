#pragma once

class MCMechWarrior;
class MCScenario;

/// <summary>The skills a results line shows, in its order (gunnery, piloting, jumping, sensors).</summary>
extern const std::array<int32_t, 4> ResultsSkillOrder;

/// <summary>One pilot's line on the results screen.</summary>
/// <remarks>
/// Single player fills it as below. Multiplayer leaves <see cref="Skills"/> at 0, keeps the pilot's kills in
/// <see cref="OldRank"/>, and sorts on kills * -10000 plus the armor and (twice) the internal structure the pilot's
/// 'Mech lost.
/// </remarks>
struct MCMissionPilotResult
{
    /// <summary>The pilot.</summary>
    MCMechWarrior* Warrior = nullptr;
    /// <summary>The pilot's four skills at the end of the scenario (truncated), before any skill-ups are applied.</summary>
    std::array<int32_t, 4> Skills{};
    /// <summary>The pilot's rank before this scenario (multiplayer: the pilot's kills).</summary>
    int32_t OldRank = 0;
    /// <summary>Sort key, ascending (see <see cref="PilotSortKey"/>).</summary>
    int32_t SortKey = 0;
};

/// <summary>A commander's kill total on the multiplayer results screen.</summary>
struct MCMissionCommanderScore
{
    /// <summary>The commander (the session's player number).</summary>
    int32_t CommanderId = 0;
    /// <summary>The kills of the commander's pilots; -1 when the commander has no pilots.</summary>
    int32_t Score = -1;
};

/// <summary>The kill and loss statistics of the results screen, in the order it lists them.</summary>
struct MCMissionStatistics
{
    /// <summary>
    /// Enemy units destroyed or disabled (single player: the clan list, marines left out; multiplayer: the disabled
    /// units of the other teams' pilots).
    /// </summary>
    int32_t EnemyUnitsHit = 0;
    /// <summary>Of those, the BattleMechs.</summary>
    int32_t EnemyMechsDestroyed = 0;
    /// <summary>Enemy pilots killed (warrior status 4).</summary>
    int32_t EnemyPilotsKilled = 0;
    /// <summary>Player units destroyed or disabled.</summary>
    int32_t PlayerUnitsHit = 0;
    /// <summary>Of those, the BattleMechs (single player: only ones with a network player id).</summary>
    int32_t PlayerMechsDestroyed = 0;

    /// <summary>The five values in the screen's order.</summary>
    std::array<int32_t, 5> Values() const
    {
        return {EnemyUnitsHit, EnemyMechsDestroyed, EnemyPilotsKilled, PlayerUnitsHit, PlayerMechsDestroyed};
    }
};

/// <summary>What the results screen shows: worked out once, when the scenario ends.</summary>
struct MCMissionResults
{
    /// <summary>The statistics.</summary>
    MCMissionStatistics Statistics;
    /// <summary>The pilot lines, sorted.</summary>
    std::vector<MCMissionPilotResult> Pilots;
    /// <summary>Multiplayer: every commander's score, best first.</summary>
    std::array<MCMissionCommanderScore, 6> Commanders{};
    /// <summary>Single player: the points of the objectives that succeeded.</summary>
    int32_t ResourcePointsEarned = 0;
};

/// <summary>
/// The single-player sort key of a pilot of rank <paramref name="rank"/>: (3 - rank) * 10000, plus, for each of the
/// first three letters of <paramref name="callsign"/>, (letter * 10) XOR (2, 1, 0) (OB-058: meant as letter * 10^n).
/// </summary>
int32_t PilotSortKey(int32_t rank, std::string_view callsign);

/// <summary>
/// A won scenario's skill-up of one skill: each rank's worth of <paramref name="points"/> buys one rank, at most three
/// and not past <paramref name="maxSkill"/>; the points are spent even when no rank is left to buy.
/// </summary>
/// <returns>The ranks gained.</returns>
int32_t ApplySkillUps(float& rank, float& points, float maxSkill);

/// <summary>Sorts pilot lines by their key, ascending, as MCX.EXE's qsort did (equal keys included).</summary>
void SortPilotResults(std::span<MCMissionPilotResult> pilots);

/// <summary>Sorts commander scores best first, as MCX.EXE's qsort did (equal scores included).</summary>
void SortCommanderScores(std::span<MCMissionCommanderScore> scores);

/// <summary>Whether the home side (alignment <paramref name="homeAlignment"/>) lost a multiplayer game.</summary>
bool HomeSideLost(uint32_t scenarioResult, int32_t homeAlignment);

/// <summary>
/// Single player: counts the kills and losses, builds the home side's pilot lines (applying the skill-ups of a won
/// scenario), sorts them and totals the objectives' points.
/// </summary>
MCMissionResults GatherSinglePlayerResults(const MCScenario& scenario, uint32_t scenarioResult);

/// <summary>Multiplayer: counts the kills and losses, every pilot's line and every commander's score.</summary>
MCMissionResults GatherMultiplayerResults(const MCScenario& scenario);
