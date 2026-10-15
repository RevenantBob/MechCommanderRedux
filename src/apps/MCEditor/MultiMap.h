#pragma once

#include <array>

#include "EditorMap.h"

// The multiplayer map (0x00468c60-0x0046a640, virtual table 0x00502224). Its source file name wasn't kept; the names
// are the port's. Team 1 is side 2 (the Inner Sphere, the NewMissions row's plain artillery and the "is" dispatch),
// team 2 side 1 (the Clans, the Clan columns and the "c" dispatch).

/// <summary>The teams of a multiplayer mission.</summary>
inline constexpr size_t MULTI_TEAMS = 2;

/// <summary>The multiplayer map: two teams, each with its settings and dispatch, and the mission's description.</summary>
/// <remarks>0x716c bytes.</remarks>
class MultiMap : public EditorMap
{
public:
    /// <summary>
    /// An <see cref="EditorMap"/> with both teams' settings at their defaults and a second "Destroy All Enemies"
    /// objective, for side 1.
    /// </summary>
    /// <remarks>0x00468c60.</remarks>
    MultiMap(int32_t maxRow, int32_t maxCol);

    /// <summary>Team 1's settings.</summary>
    /// <remarks>0x0046a600.</remarks>
    MissionSettings team1Settings() const;
    /// <summary>Team 2's settings.</summary>
    /// <remarks>0x0046a620.</remarks>
    MissionSettings team2Settings() const;
    /// <summary>Replaces team 1's settings with <paramref name="settings"/>.</summary>
    /// <remarks>0x0046a5c0.</remarks>
    void setTeam1Settings(const MissionSettings& settings);
    /// <summary>Replaces team 2's settings with <paramref name="settings"/>.</summary>
    /// <remarks>0x0046a5e0.</remarks>
    void setTeam2Settings(const MissionSettings& settings);

    /// <summary>
    /// Writes mission <paramref name="name"/>: both teams' dispatches (Data\Missions\is&lt;name&gt;.txt and
    /// c&lt;name&gt;.txt), its NewMissions row (added or updated, with each team's artillery), its units', structures',
    /// objectives' and drop zones' rows and its save.
    /// </summary>
    /// <returns>What writing the save returned.</returns>
    /// <remarks>0x004691b0.</remarks>
    bool saveMission(DaoDatabase& database, CString& name) override;
    /// <summary>
    /// Checks each team has an objective and three drop zones, then the mission's size
    /// (<see cref="EditorMap::checkMissionSize"/>).
    /// </summary>
    /// <remarks>0x00468de0.</remarks>
    bool checkMission(DaoDatabase& database, const CString& name) override;
    /// <summary>
    /// Reads mission <paramref name="name"/> onto the map: its NewMissions settings, both teams' dispatches, the time
    /// limit and the teams' resource points from its save (Data\Savegame\<paramref name="name"/>.mpk) and the
    /// description from the save's third packet, then its units, objectives, structure links and drop zones.
    /// </summary>
    /// <returns>Whether the mission is in the database.</returns>
    /// <remarks>0x00469660.</remarks>
    bool loadMission(DaoDatabase& database, const CString& name) override;
    /// <returns>64.</returns>
    /// <remarks>0x00468d50.</remarks>
    int32_t maxTurrets() override;
    /// <returns>6.</returns>
    /// <remarks>0x00468d30.</remarks>
    int32_t maxDropZones() override;
    /// <returns>6.</returns>
    /// <remarks>0x00468d40.</remarks>
    int32_t maxPlayers() override;
    /// <summary>
    /// Writes the save game Data\Savegame\&lt;<paramref name="saveName"/> or <paramref name="name"/>&gt;.mpk, a
    /// packet file made by hand: Teditor\MPK.txt filled with the names, the time limit and the teams' resource points
    /// as the second packet, the description as the third, and in place of a signature the sum of every byte after it.
    /// </summary>
    /// <returns>false when MPK.txt or the save doesn't open.</returns>
    /// <remarks>0x0046a0b0.</remarks>
    bool writeSave(const CString& name, const char* saveName);

    /// <summary>Each team's settings (+0x7114, team 1 first).</summary>
    std::array<MissionSettings, MULTI_TEAMS> teams;
    /// <summary>Team 2's dispatch (+0x7154); team 1's is <see cref="EditorMap::missionText"/>.</summary>
    MissionText clanText;
};
