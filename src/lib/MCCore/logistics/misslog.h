#pragma once

class MCBattleMech;
class MCGroundVehicle;
class MCLogMech;
class MCLogVehicle;
class MCLogWarrior;
class MCMechWarrior;

/// <summary>
/// Passes the force between the mission and logistics as FIT files in the save folder: the starting fit (the
/// force and inventory), one profile per mech, vehicle and pilot, and the logistics save game.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\misslog.cpp</c>. It has no data: callers make one on the stack just to call its
/// methods. The readers were never written (they return 0).
/// </remarks>
class MCMissionLogisticsBridge
{
public:
    /// <summary>After a mission: writes the surviving force, the salvage and the component counts to <paramref name="fileName"/>.</summary>
    int32_t MissionResultsStartingFitWriter(char* fileName);

    /// <summary>After a mission: writes <paramref name="mech"/>'s profile (<paramref name="notAssigned"/> = it has no pilot slot).</summary>
    static int32_t MissionResultsMechProfileWriter(char* fileName, MCBattleMech* mech, int notAssigned);

    /// <summary>After a mission: writes <paramref name="vehicle"/>'s profile.</summary>
    int32_t MissionResultsVehicleProfileWriter(char* fileName, MCGroundVehicle* vehicle);

    /// <summary>After a mission: writes <paramref name="warrior"/>'s profile.</summary>
    int32_t MissionResultsWarriorProfileWriter(char* fileName, MCMechWarrior* warrior);

    /// <summary>
    /// Writes the force and inventory from logistics; when <paramref name="skipFlagged"/> is nonzero, units whose
    /// flag at <c>LogMech</c>/<c>LogVehicle</c> +0x78 is set are left out.
    /// </summary>
    int32_t LogisticsStartingFitWriter(char* fileName, int skipFlagged);

    /// <summary>Writes <paramref name="mech"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    int32_t LogisticsMechProfileWriter(char* fileName, MCLogMech* mech, int writeRequired);

    /// <summary>Writes <paramref name="vehicle"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    int32_t LogisticsVehicleProfileWriter(char* fileName, MCLogVehicle* vehicle, int writeRequired);

    /// <summary>Writes <paramref name="warrior"/>'s profile from logistics.</summary>
    int32_t LogisticsWarriorProfileWriter(char* fileName, MCLogWarrior* warrior);

    /// <summary>Never written: returns 0.</summary>
    int32_t LogisticsStartingFitReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    int32_t LogisticsMechProfileReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    int32_t LogisticsVehicleProfileReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    int32_t LogisticsWarriorProfileReader(char* fileName);

    /// <summary>Writes the logistics state (campaign, force, inventory, pilots) to save game <paramref name="fileName"/>.</summary>
    int32_t LogisticsSaveGame(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    int32_t LogisticsLoadGame(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    int32_t MissionToLogisticsBridgeSave(char* fileName);
};

/// <summary>Deletes every <c>*.fit</c> file in folder <paramref name="path"/>.</summary>
void DestroyAllFitFiles(char* path);

/// <summary>The number of entries in <see cref="ComponentComment"/> and <see cref="ComponentId"/>.</summary>
inline constexpr int32_t NUM_FIT_COMPONENTS = 50;

/// <summary>The comment line written before each component block of a starting fit ("// Medium Pulse Laser (IS)", ...).</summary>
extern char* ComponentComment[NUM_FIT_COMPONENTS];

/// <summary>The master component id of each entry of <see cref="ComponentComment"/>.</summary>
extern uint8_t ComponentId[NUM_FIT_COMPONENTS];

/// <summary>Seconds spent in missions over the campaign.</summary>
extern float TotalScenarioTime;

/// <summary>Seconds spent in logistics over the campaign.</summary>
extern float TotalLogisticsTime;
