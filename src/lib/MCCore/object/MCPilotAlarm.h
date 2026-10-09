#pragma once

/// <summary>
/// The pilot alarms, in <see cref="PilotAlarmFunctionName"/> order (the brain's handler for each). Names follow
/// MechCommander 2's, which match the handler names; ABL brains see them as the numbers 0..16.
/// </summary>
enum class MCPilotAlarmType : int32_t
{
    TargetOfWeaponFire = 0,
    HitByWeaponFire = 1,
    DamageTakenRate = 2,
    DeathOfMate = 3,
    FriendlyVehicleCrippled = 4,
    FriendlyVehicleDestroyed = 5,
    VehicleIncapacitated = 6,
    VehicleDestroyed = 7,
    VehicleWithdrawn = 8,
    MoraleBreak = 9,
    Collision = 10,
    GuardRadiusBreach = 11,
    KilledTarget = 12,
    MateFiredWeapon = 13,
    PlayerOrder = 14,
    NoMovePath = 15,
    GateClosing = 16,
    Count
};

/// <summary>How many pilot alarms there are.</summary>
inline constexpr int32_t NumPilotAlarms = std::to_underlying(MCPilotAlarmType::Count);

/// <summary>An alarm raised on a pilot: its triggers since it was last handled.</summary>
/// <remarks>The original name isn't known (MC2: PilotAlarm).</remarks>
struct MCPilotAlarm
{
    /// <summary>
    /// Triggers an alarm remembers until it is handled; more are dropped. Kept: the brains copy the triggers into
    /// ABL arrays of their own (getalarmtriggers) sized for this many.
    /// </summary>
    static constexpr int32_t MaxTriggers = 10;

    /// <summary>Triggers remembered; 0 when not raised.</summary>
    uint8_t NumTriggers = 0;
    /// <summary>What raised it (a part id, a cause code, a path error).</summary>
    std::array<uint32_t, MaxTriggers> Trigger{};
};

/// <summary>The brain's alarm handler names, by <see cref="MCPilotAlarmType"/>.</summary>
extern const std::array<std::string_view, NumPilotAlarms> PilotAlarmFunctionName;
