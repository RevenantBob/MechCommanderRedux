#pragma once

class MCBattleMech;
class MCLogMech;
class MCLogVehicle;
class MCLogWarrior;
class MCMechWarrior;

/// <summary>
/// Passes the force between the mission and logistics as FIT files in the save folder: the starting fit (the
/// force and inventory), one profile per mech, vehicle and pilot, and the logistics save game. Each writer returns 0,
/// or the first file error.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\misslog.cpp</c> (<c>MissionLogisticsBridge</c>, a class with no data that callers made
/// on the stack to call its methods). Its readers and the mission's vehicle profile writer were never called.
/// </remarks>
namespace MCMissionLogisticsBridge
{
    /// <summary>After a mission: writes the surviving force, the salvage and the component counts to <paramref name="fileName"/>.</summary>
    int32_t MissionResultsStartingFitWriter(std::string_view fileName);

    /// <summary>After a mission: writes <paramref name="mech"/>'s profile (<paramref name="notAssigned"/>: it has no pilot slot).</summary>
    int32_t MissionResultsMechProfileWriter(std::string_view fileName, MCBattleMech* mech, bool notAssigned);

    /// <summary>After a mission: writes <paramref name="warrior"/>'s profile.</summary>
    int32_t MissionResultsWarriorProfileWriter(std::string_view fileName, MCMechWarrior* warrior);

    /// <summary>
    /// Writes the force and inventory from logistics; with <paramref name="skipDeployed"/>, the force's deployed mechs
    /// are left out.
    /// </summary>
    int32_t LogisticsStartingFitWriter(std::string_view fileName, bool skipDeployed);

    /// <summary>Writes <paramref name="mech"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    int32_t LogisticsMechProfileWriter(std::string_view fileName, MCLogMech* mech, bool writeRequired);

    /// <summary>Writes <paramref name="vehicle"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    int32_t LogisticsVehicleProfileWriter(std::string_view fileName, MCLogVehicle* vehicle, bool writeRequired);

    /// <summary>Writes <paramref name="warrior"/>'s profile from logistics.</summary>
    int32_t LogisticsWarriorProfileWriter(std::string_view fileName, MCLogWarrior* warrior);

    /// <summary>Writes the logistics state (campaign, force, inventory, pilots) to save game <paramref name="fileName"/>.</summary>
    int32_t LogisticsSaveGame(std::string_view fileName);
}

/// <summary>Deletes every <c>*.fit</c> file in folder <paramref name="path"/>.</summary>
void DestroyAllFitFiles(std::string_view path);

/// <summary>A component block of a starting fit: its master component id and the comment line written before it.</summary>
struct MCFitComponent
{
    /// <summary>The master component id.</summary>
    uint8_t Id = 0;
    /// <summary>The comment line ("// Medium Pulse Laser (IS)", ...).</summary>
    std::string_view Comment;
};

/// <summary>
/// The 50 components a starting fit counts, in block order (<c>Componant0</c> .. <c>Componant49</c>; the count is the
/// file format's).
/// </summary>
inline constexpr std::array<MCFitComponent, 50> FitComponents = {{
    {0x90, "// Medium Pulse Laser (IS) "},
    {0x99, "// Medium Pulse Laser (Clan)"},
    {0x7b, "// Short-Range Missile/2 (IS) "},
    {0x85, "// Short-Range Missile/2 (CLAN)"},
    {0x8f, "// Medium Laser (IS)"},
    {0x7d, "// Streak Short-Range Missile/2 (IS) "},
    {0x87, "// Streak Short-Range Missile/2 (Clan)"},
    {0x93, "// Flamer (IS) "},
    {0x9b, "// Flamer (Clan)"},
    {0x66, "// Heavy Autocannon (IS) "},
    {0x70, "// Heavy Ultra AC (Clan)"},
    {0x8e, "// Large Pulse Laser (IS) "},
    {0x98, "// Medium ER Laser (Clan)"},
    {0x97, "// Large Pulse Laser (Clan)"},
    {0x8c, "// Large Laser (IS)"},
    {0x65, "// Medium Autocannon (IS) "},
    {0x6f, "// Medium Ultra AC (Clan)"},
    {0x91, "// Particle Projector Cannon (IS) "},
    {0x78, "// Long-Range Missile/5 (IS) "},
    {0x82, "// Long-Range Missile/5 (CLAN)"},
    {0x64, "// Light Autocannon (IS) "},
    {0x67, "// Light Ultra Autocannon (IS) "},
    {0x6e, "// Light Ultra AC (Clan)"},
    {0x8d, "// Large ER Laser (IS) "},
    {0x96, "// Large ER Laser (Clan)"},
    {0x92, "// ER Particle Projector Cannon (IS) "},
    {0x68, "// Gauss Rifle (IS) "},
    {0x71, "// Gauss Rifle (Clan)"},
    {0x9a, "// ER Particle Projector (Clan)"},
    {0x0d, "// Sensor (Basic) (IS) "},
    {0x10, "// Sensor (Basic) (Clan)"},
    {0x0e, "// Sensor (Intermediate) (IS) "},
    {0x0f, "// Sensor (Advanced) (IS) "},
    {0x11, "// Sensor (Advanced) (Clan)"},
    {0x25, "// Beagle Probe (IS) "},
    {0x26, "// Guardian ECM (IS) "},
    {0x2a, "// ECM Suite (Clan) "},
    {0x2b, "// Active Probe (Clan) "},
    {0x62, "// Heavy Gauss Cannon (IS) "},
    {0x63, "// Light Gauss Cannon (IS) "},
    {0x6b, "// Light LBX AutoCannon (IS) "},
    {0x6c, "// Medium LBX AutoCannon (IS) "},
    {0x6d, "// Heavy LBX AutoCannon (IS) "},
    {0x74, "// Light LBX AutoCannon (Clan) "},
    {0x75, "// Medium LBX AutoCannon (Clan) "},
    {0x76, "// Heavy LBX AutoCannon (Clan) "},
    {0x7e, "// Heavy Thunderbolt "},
    {0x8b, "// Large X Pulse Laser "},
    {0xa0, "// Long Tom "},
    {0xa1, "// Sniper Cannon "},
}};

/// <summary>Seconds spent in missions over the campaign.</summary>
extern float TotalScenarioTime;

/// <summary>Seconds spent in logistics over the campaign.</summary>
extern float TotalLogisticsTime;
