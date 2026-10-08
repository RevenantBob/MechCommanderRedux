#pragma once

/// <summary>A component's kind: the "type" column of compbas.csv, an index into <see cref="ComponentFormString"/>.</summary>
enum class MCComponentForm : int32_t
{
    Simple = 0,
    Cockpit = 1,
    Sensor = 2,
    Actuator = 3,
    Engine = 4,
    HeatSink = 5,
    Weapon = 6,
    WeaponEnergy = 7,
    WeaponBallistic = 8,
    WeaponMissile = 9,
    Ammo = 10,
    JumpJet = 11,
    Case = 12,
    LifeSupport = 13,
    Gyroscope = 14,
    PowerAmplifier = 15,
    Ecm = 16,
    Probe = 17,
    Jammer = 18,
    Bulk = 19,
};

/// <summary>
/// One row of the master component table (data\objects\compbas.csv): a mech or vehicle part, weapon or ammo, its
/// weight, critical spaces and, by <see cref="Form"/>, its stats. <see cref="MasterComponentList"/> holds them by
/// id.
/// </summary>
/// <remarks>Original source: <c>object\cmponent.cpp</c>.</remarks>
class MCMasterComponent
{
public:
    /// <summary>
    /// Reads one CSV row. Fields are split as strtok split them: empty fields are skipped. An "undefined" row leaves
    /// the id -1. Weapon ranges are scaled by <paramref name="weaponRangeFactor"/>, a sensor's range by
    /// <paramref name="sensorRangeFactor"/>. Returns 0, -1 for a form name not in ComponentFormString, -2 for a form
    /// with no stats (plain "Weapon").
    /// </summary>
    int32_t InitExcel(std::string_view dataLine, float weaponRangeFactor, float sensorRangeFactor);
    /// <summary>Whether this is anything but an anti-missile system.</summary>
    int IsOffensiveWeapon() const;
    /// <summary>Whether this is an anti-missile system.</summary>
    int IsDefensiveWeapon() const;
    /// <summary>Scales the four weapon ranges by <paramref name="factor"/>, truncating each to a short.</summary>
    void MultiplyWeaponRanges(float factor);

    /// <summary>Master id; -1 for an unused ("undefined") row.</summary>
    int32_t MasterID = -1;
    /// <summary>"RP": resource points.</summary>
    int32_t ResourcePoints = 0;
    /// <summary>"type".</summary>
    MCComponentForm Form = MCComponentForm::Simple;
    /// <summary>Full name (the first 30 characters).</summary>
    std::string Name;
    /// <summary>"abbr" (the first 15 characters).</summary>
    std::string Abbreviation;
    /// <summary>"tons".</summary>
    float Tonnage = 0.0f;
    /// <summary>"crits": critical spaces it takes.</summary>
    uint8_t CriticalSpacesReq = 0;
    /// <summary>The column after crits ("?").</summary>
    uint8_t Health = 0;
    /// <summary>
    /// Per location (head, CT, LT, RT, LA, RA, LL, RL): "No" -1 (can't go there), "Yes" 0, else the critical spaces
    /// it must take there.
    /// </summary>
    std::array<int8_t, 8> CriticalSpacesLocation{};
    /// <summary>"disable".</summary>
    uint8_t DisableLevel = 0;
    /// <summary>
    /// Bits: 2 fits vehicles ("Vehicle?"), 1 fits both ("Fit both?"), 0x30 "Fit IS?" yes, else 0x10 (clan tech) or
    /// 0x20.
    /// </summary>
    uint8_t Flags = 0;
    /// <summary>"Side": 1 clan, 2 Inner Sphere, 3 both.</summary>
    uint8_t TechBase = 0;
    /// <summary>"BR": battle rating.</summary>
    float BattleRating = 0.0f;
    /// <summary>
    /// By form: a sensor's, probe's, jammer's or ECM's range (float); an energy/ballistic/missile weapon's heat
    /// (float); an engine's or heat sink's value (short); ammo per ton or jump jet value (int).
    /// </summary>
    union
    {
        float RangeOrHeat;
        int16_t ShortValue;
        int32_t LongValue = 0;
    };

    /// <summary>A weapon's damage; an ECM's effect; ammo's second value.</summary>
    float Damage = 0.0f;
    /// <summary>A weapon's recycle time.</summary>
    float RecycleTime = 0.0f;
    /// <summary>"#miss": missiles per salvo.</summary>
    int32_t NumMissiles = 0;
    /// <summary>"miss type": 1 SRM, 2 LRM, 3 ST, 4 ballistic "1".</summary>
    uint8_t MissileType = 0;
    /// <summary>"Ammo": the master id of the weapon's ammo.</summary>
    uint8_t AmmoMasterId = 0;
    /// <summary>"min", "short", "med", "long" ranges (scaled).</summary>
    std::array<float, 4> WeaponRange{};
    /// <summary>"Flag": 1 streak, 2 inferno, 4 LBX, 8 artillery.</summary>
    uint8_t WeaponFlags = 0;
    /// <summary>"Weapon": the weapon's graphic type.</summary>
    int16_t WeaponType = 0;
    /// <summary>"Effect".</summary>
    uint8_t WeaponEffect = 0;
    /// <summary>"art".</summary>
    int32_t Art = 0;
};

/// <summary>
/// Reads the master component table from <paramref name="fileName"/> (the four special ids, then
/// <paramref name="numComponents"/> rows); 0, the File error, or -1 when the file is short.
/// </summary>
int32_t InitMasterComponentListExcel(std::string_view fileName, int32_t numComponents, float weaponRangeFactor,
                                     float sensorRangeFactor);
/// <summary>Scales every energy, ballistic and missile weapon's ranges.</summary>
void MultiplyMasterWeaponRanges(float factor);
/// <summary>Rows in <see cref="MasterComponentList"/>.</summary>
inline int32_t NumMasterComponents();

/// <summary>The form names, by <see cref="MCComponentForm"/>.</summary>
extern const std::array<std::string_view, 20> ComponentFormString;
/// <summary>The master component table, by id (loaded with the mission and read by logistics too).</summary>
extern std::vector<MCMasterComponent> MasterComponentList;
/// <summary>The arm actuator's master id (from the file's header).</summary>
extern int32_t MasterArmActuatorID;
/// <summary>The leg actuator's master id.</summary>
extern int32_t MasterLegActuatorID;
/// <summary>The clan anti-missile system's master id.</summary>
extern int32_t MasterClanAntiMissileSystemID;
/// <summary>The Inner Sphere anti-missile system's master id.</summary>
extern int32_t MasterInnerSphereAntiMissileSystemID;

inline int32_t NumMasterComponents()
{
    return static_cast<int32_t>(MasterComponentList.size());
}
