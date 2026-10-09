#include "stdafx.h"
#include "mission/MCMissionResults.h"
#include "lib/MCFatal.h"
#include "mission/MCMission.h"
#include "object/MCSensorSystem.h"
#include "lib/MCMsvcSort.h"
#include "mission/MCScenario.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"

const std::array<int32_t, 4> ResultsSkillOrder = {SkillGunnery, SkillPiloting, SkillJumping, SkillSensors};

namespace
{
    /// <summary>Warrior status of a dead pilot.</summary>
    constexpr int32_t PilotDead = 4;

    /// <summary>The kinds of kill a warrior counts.</summary>
    constexpr int32_t KillKinds = 7;
}

auto PilotSortKey(int32_t rank, std::string_view callsign) -> int32_t
{
    int32_t key = (3 - rank) * 10000;

    // Original behaviour (OB-058): meant as letter * 10^n, the callsign's letters are XORed with n. A callsign shorter
    // than three letters counts its end as 0 (the original read its terminator, then whatever followed).
    for (size_t letter = 0, power = 2; letter < 3; letter++, power--)
    {
        const int32_t value = letter < callsign.size() ? callsign[letter] : 0;
        key += (value * 10) ^ static_cast<int32_t>(power);
    }

    return key;
}

auto ApplySkillUps(float& rank, float& points, float maxSkill) -> int32_t
{
    int32_t gains = 0;

    if (rank == 0.0f)
    {
        return 0;
    }

    do
    {
        if (points < rank)
        {
            break;
        }

        points -= rank;

        if (rank < maxSkill && gains < 3)
        {
            gains++;
            rank += 1.0f;
        }
    } while (rank != 0.0f);

    return gains;
}

auto SortPilotResults(std::span<MCMissionPilotResult> pilots) -> void
{
    MCMsvcSort(pilots, [](const MCMissionPilotResult& a, const MCMissionPilotResult& b)
               { return a.SortKey == b.SortKey ? 0 : (b.SortKey < a.SortKey ? 1 : -1); });
}

auto SortCommanderScores(std::span<MCMissionCommanderScore> scores) -> void
{
    MCMsvcSort(scores, [](const MCMissionCommanderScore& a, const MCMissionCommanderScore& b)
               { return a.Score == b.Score ? 0 : (a.Score < b.Score ? 1 : -1); });
}

auto HomeSideLost(uint32_t scenarioResult, int32_t homeAlignment) -> bool
{
    return scenarioResult == 3 || (scenarioResult == 1 && homeAlignment == -1) ||
           (scenarioResult == 2 && homeAlignment == 1);
}

auto GatherSinglePlayerResults(const MCScenario& scenario, uint32_t scenarioResult) -> MCMissionResults
{
    MCMissionResults results;
    MCMissionStatistics& statistics = results.Statistics;

    for (MCBaseObject* current : *ClanMechList())
    {
        auto* object = static_cast<MCGameObject*>(current);

        if ((object->IsDestroyed() || object->IsDisabled()) && !object->IsMarine())
        {
            statistics.EnemyUnitsHit++;

            if (object->ObjectClass == MCObjectClass::BattleMech)
            {
                statistics.EnemyMechsDestroyed++;
            }
        }
    }

    for (MCBaseObject* current : *InnerSphereMechList())
    {
        auto* object = static_cast<MCGameObject*>(current);

        if ((object->IsDestroyed() || object->IsDisabled()) && !object->IsMarine())
        {
            statistics.PlayerUnitsHit++;

            if (object->ObjectClass == MCObjectClass::BattleMech && static_cast<MCMover*>(object)->NetPlayerId != -1)
            {
                statistics.PlayerMechsDestroyed++;
            }
        }
    }

    // The lines the screen has room for: the home side's awake 'Mechs that have a network player id.
    size_t lines = 0;

    for (uint32_t i = 1; i <= scenario.NumWarriors(); i++)
    {
        MCMechWarrior* warrior = scenario.Warrior(i);

        if (warrior == nullptr)
        {
            continue;
        }

        auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

        if (vehicle != nullptr && vehicle->GetAwake() && vehicle->ObjectClass == MCObjectClass::BattleMech &&
            warrior->Alignment == HomeTeam()->Alignment && vehicle->NetPlayerId != -1)
        {
            lines++;
        }
    }

    // Fill the lines and apply the skill-ups.
    for (uint32_t i = 1; i <= scenario.NumWarriors(); i++)
    {
        MCMechWarrior* warrior = scenario.Warrior(i);

        if (warrior == nullptr)
        {
            continue;
        }

        auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

        if (vehicle == nullptr || vehicle->ObjectClass != MCObjectClass::BattleMech || !vehicle->GetAwake())
        {
            continue;
        }

        if (!warrior->OnHomeTeam())
        {
            if (warrior->Status == PilotDead && warrior->Alignment != HomeTeam()->Alignment)
            {
                statistics.EnemyPilotsKilled++;
            }

            continue;
        }

        // Port fix: the fill loop doesn't test the network player id or the alignment the count did, so it can find
        // more pilots than the original allocated for; the extra ones are left off the screen.
        if (results.Pilots.size() >= lines)
        {
            continue;
        }

        MCMissionPilotResult& entry = results.Pilots.emplace_back();
        entry.Warrior = warrior;

        if (vehicle->SensorSystem != nullptr)
        {
            warrior->SkillPoints[SkillSensors] = static_cast<float>(vehicle->SensorSystem->TotalContacts) * SensorSkill;
        }

        warrior->SkillPoints[SkillPiloting] += warrior->SkillRank[SkillPiloting];
        entry.OldRank = static_cast<int8_t>(warrior->Rank);

        for (int32_t skill = 0; skill < 4; skill++)
        {
            entry.Skills[static_cast<size_t>(skill)] = static_cast<int32_t>(warrior->SkillRank[skill]);

            if (scenarioResult > 3)
            {
                ApplySkillUps(warrior->SkillRank[skill], warrior->SkillPoints[skill], MaxPilotSkill);
            }
        }

        warrior->CalcRank();
        Assert(entry.OldRank <= static_cast<int8_t>(warrior->Rank), 0, "Hey, how'd we drop in rank???");
        entry.SortKey = PilotSortKey(static_cast<int8_t>(warrior->Rank), warrior->Callsign);
    }

    SortPilotResults(results.Pilots);
    // Only the first nine objectives' points count.
    results.ResourcePointsEarned = scenario.Objectives.SucceededPoints();
    return results;
}

auto GatherMultiplayerResults(const MCScenario& scenario) -> MCMissionResults
{
    MCMissionResults results;
    MCMissionStatistics& statistics = results.Statistics;

    for (int32_t i = 0; i < static_cast<int32_t>(results.Commanders.size()); i++)
    {
        results.Commanders[static_cast<size_t>(i)] = {i, -1};
    }

    for (uint32_t i = 1; i <= scenario.NumWarriors(); i++)
    {
        MCMechWarrior* warrior = scenario.Warrior(i);

        if (warrior == nullptr)
        {
            continue;
        }

        auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);

        if (vehicle == nullptr || !vehicle->GetAwake())
        {
            continue;
        }

        if (vehicle->IsDisabled())
        {
            const bool home = warrior->Team == HomeTeam();
            (home ? statistics.PlayerUnitsHit : statistics.EnemyUnitsHit)++;

            if (vehicle->ObjectClass == MCObjectClass::BattleMech)
            {
                (home ? statistics.PlayerMechsDestroyed : statistics.EnemyMechsDestroyed)++;
            }
        }

        MCMissionPilotResult& entry = results.Pilots.emplace_back();
        entry.Warrior = warrior;

        if (warrior->Status == PilotDead && warrior->Team != HomeTeam())
        {
            statistics.EnemyPilotsKilled++;
        }

        for (int32_t kind = 0; kind < KillKinds; kind++)
        {
            MCMissionCommanderScore& score = results.Commanders[static_cast<size_t>(vehicle->GetCommanderId())];

            if (score.Score == -1)
            {
                score.Score = 0;
            }

            const int32_t kills = warrior->NumKilled[kind][1];
            score.Score += kills;
            entry.SortKey += kills * -10000;
            entry.OldRank += kills;
        }

        // Damage taken counts against the pilot: armor lost, and internal structure lost twice over.
        for (int32_t j = 0; j < vehicle->NumArmorLocations(); j++)
        {
            const MCArmorLocation& armor = vehicle->Armor[j];
            entry.SortKey = static_cast<int32_t>(static_cast<double>(armor.MaxArmor) - armor.CurArmor + entry.SortKey);
        }

        for (int32_t j = 0; j < vehicle->NumBodyLocations(); j++)
        {
            const MCBodyLocation& body = vehicle->BodyAt(j);
            entry.SortKey = static_cast<int32_t>(
                (static_cast<double>(body.MaxInternalStructure) - body.CurInternalStructure) * 2 + entry.SortKey);
        }
    }

    SortCommanderScores(results.Commanders);
    SortPilotResults(results.Pilots);
    return results;
}
