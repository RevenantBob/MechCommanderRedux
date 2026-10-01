#pragma once

class BattleMech;
class GroundVehicle;
class LogMech;
class LogVehicle;
class LogWarrior;
class MechWarrior;

/// <summary>
/// Passes the force between the mission and logistics as FIT files in the save folder: the starting fit (the
/// force and inventory), one profile per mech, vehicle and pilot, and the logistics save game.
/// </summary>
/// <remarks>
/// Original source: <c>logistics\misslog.cpp</c>. It has no data: callers make one on the stack just to call its
/// methods. The readers were never written (they return 0).
/// </remarks>
class MissionLogisticsBridge
{
public:
    /// <summary>After a mission: writes the surviving force, the salvage and the component counts to <paramref name="fileName"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007111e0</remarks>
    int32_t missionResultsStartingFitWriter(char* fileName);

    /// <summary>After a mission: writes <paramref name="mech"/>'s profile (<paramref name="notAssigned"/> = it has no pilot slot).</summary>
    /// <remarks>
    /// MCX.EXE @ 0x00711cd0 (unnamed in the symbols and compiled as a plain function: the name is inferred from its
    /// siblings and callers).
    /// </remarks>
    static int32_t missionResultsMechProfileWriter(char* fileName, BattleMech* mech, int notAssigned);

    /// <summary>After a mission: writes <paramref name="vehicle"/>'s profile.</summary>
    /// <remarks>MCX.EXE @ 0x00712380</remarks>
    int32_t missionResultsVehicleProfileWriter(char* fileName, GroundVehicle* vehicle);

    /// <summary>After a mission: writes <paramref name="warrior"/>'s profile.</summary>
    /// <remarks>MCX.EXE @ 0x007127b0</remarks>
    int32_t missionResultsWarriorProfileWriter(char* fileName, MechWarrior* warrior);

    /// <summary>
    /// Writes the force and inventory from logistics; when <paramref name="skipFlagged"/> is nonzero, units whose
    /// flag at <c>LogMech</c>/<c>LogVehicle</c> +0x78 is set are left out.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00712b30</remarks>
    int32_t logisticsStartingFitWriter(char* fileName, int skipFlagged);

    /// <summary>Writes <paramref name="mech"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    /// <remarks>MCX.EXE @ 0x00713ca0</remarks>
    int32_t logisticsMechProfileWriter(char* fileName, LogMech* mech, int writeRequired);

    /// <summary>Writes <paramref name="vehicle"/>'s profile from logistics (<paramref name="writeRequired"/> adds its Required flag).</summary>
    /// <remarks>MCX.EXE @ 0x007144c0</remarks>
    int32_t logisticsVehicleProfileWriter(char* fileName, LogVehicle* vehicle, int writeRequired);

    /// <summary>Writes <paramref name="warrior"/>'s profile from logistics.</summary>
    /// <remarks>MCX.EXE @ 0x00714990</remarks>
    int32_t logisticsWarriorProfileWriter(char* fileName, LogWarrior* warrior);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00714cb0</remarks>
    int32_t logisticsStartingFitReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00714cc0</remarks>
    int32_t logisticsMechProfileReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00714cd0</remarks>
    int32_t logisticsVehicleProfileReader(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00714ce0</remarks>
    int32_t logisticsWarriorProfileReader(char* fileName);

    /// <summary>Writes the logistics state (campaign, force, inventory, pilots) to save game <paramref name="fileName"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00714cf0</remarks>
    int32_t logisticsSaveGame(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00715d60</remarks>
    int32_t logisticsLoadGame(char* fileName);

    /// <summary>Never written: returns 0.</summary>
    /// <remarks>MCX.EXE @ 0x00715d70</remarks>
    int32_t missionToLogisticsBridgeSave(char* fileName);
};

/// <summary>Deletes every <c>*.fit</c> file in folder <paramref name="path"/>.</summary>
/// <remarks>MCX.EXE @ 0x00711140</remarks>
void destroyAllFITFiles(char* path);

/// <summary>The number of entries in <see cref="componentComment"/> and <see cref="componentId"/>.</summary>
inline constexpr int32_t NUM_FIT_COMPONENTS = 50;

/// <summary>The comment line written before each component block of a starting fit ("// Medium Pulse Laser (IS)", ...).</summary>
extern char* componentComment[NUM_FIT_COMPONENTS];

/// <summary>The master component id of each entry of <see cref="componentComment"/>.</summary>
extern uint8_t componentId[NUM_FIT_COMPONENTS];

/// <summary>Seconds spent in missions over the campaign.</summary>
extern float totalScenarioTime;

/// <summary>Seconds spent in logistics over the campaign.</summary>
extern float totalLogisticsTime;
