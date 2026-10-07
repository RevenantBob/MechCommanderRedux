#pragma once

/// <summary>A component's kind: the "type" column of compbas.csv, an index into <see cref="ComponentFormString"/>.</summary>
enum MCComponentForm : int32_t
{
    COMPONENT_FORM_SIMPLE = 0,
    COMPONENT_FORM_COCKPIT = 1,
    COMPONENT_FORM_SENSOR = 2,
    COMPONENT_FORM_ACTUATOR = 3,
    COMPONENT_FORM_ENGINE = 4,
    COMPONENT_FORM_HEATSINK = 5,
    COMPONENT_FORM_WEAPON = 6,
    COMPONENT_FORM_WEAPON_ENERGY = 7,
    COMPONENT_FORM_WEAPON_BALLISTIC = 8,
    COMPONENT_FORM_WEAPON_MISSILE = 9,
    COMPONENT_FORM_AMMO = 10,
    COMPONENT_FORM_JUMPJET = 11,
    COMPONENT_FORM_CASE = 12,
    COMPONENT_FORM_LIFESUPPORT = 13,
    COMPONENT_FORM_GYROSCOPE = 14,
    COMPONENT_FORM_POWER_AMPLIFIER = 15,
    COMPONENT_FORM_ECM = 16,
    COMPONENT_FORM_PROBE = 17,
    COMPONENT_FORM_JAMMER = 18,
    COMPONENT_FORM_BULK = 19,
};

/// <summary>
/// One row of the master component table (data\objects\compbas.csv): a mech or vehicle part, weapon or ammo, its
/// weight, critical spaces and, by <see cref="Form"/>, its stats. <see cref="MasterComponentList"/> holds them by
/// id.
/// </summary>
/// <remarks>Original source: <c>object\cmponent.cpp</c>; 0x84 bytes.</remarks>
class MCMasterComponent
{
public:
    void Destroy();
    /// <summary>
    /// Reads one CSV row (tokenized in place with strtok). An "undefined" row leaves the id -1. Weapon ranges are
    /// scaled by <paramref name="weaponRangeFactor"/>, a sensor's range by <paramref name="sensorRangeFactor"/>.
    /// Returns 0, -1 for a form name not in ComponentFormString, -2 for a form with no stats (plain "Weapon").
    /// </summary>
    int32_t InitExcel(char* dataLine, uint8_t index, float weaponRangeFactor, float sensorRangeFactor);
    /// <summary>Whether this is anything but an anti-missile system.</summary>
    int IsOffensiveWeapon();
    /// <summary>Whether this is an anti-missile system.</summary>
    int IsDefensiveWeapon();
    /// <summary>Scales the four weapon ranges by <paramref name="factor"/>, truncating each to a short.</summary>
    void MultiplyWeaponRanges(float factor);

    /// <summary>Master id; -1 for an unused ("undefined") row.</summary>
    int32_t MasterID = -1;
    /// <summary>"RP": resource points.</summary>
    int32_t ResourcePoints = 0;
    /// <summary>"type": the <see cref="MCComponentForm"/>.</summary>
    int32_t Form = 0;
    /// <summary>Full name (29 characters).</summary>
    char Name[30] = {};
    /// <summary>"abbr" (15 characters).</summary>
    char Abbreviation[16] = {};
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
    int8_t CriticalSpacesLocation[8] = {};
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
    float WeaponRange[4] = {};
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
int32_t InitMasterComponentListExcel(char* fileName, int32_t numComponents, float weaponRangeFactor,
                                     float sensorRangeFactor);
/// <summary>Scales every energy, ballistic and missile weapon's ranges.</summary>
void MultiplyMasterWeaponRanges(float factor);

/// <summary>The form names, by <see cref="MCComponentForm"/>, null-terminated.</summary>
extern const char* ComponentFormString[21];
/// <summary>The master component table.</summary>
extern std::unique_ptr<MCMasterComponent[]> MasterComponentList;
/// <summary>Rows in <see cref="MasterComponentList"/>.</summary>
extern int32_t NumMasterComponents;
/// <summary>The arm actuator's master id (from the file's header).</summary>
extern int32_t MasterArmActuatorID;
/// <summary>The leg actuator's master id.</summary>
extern int32_t MasterLegActuatorID;
/// <summary>The clan anti-missile system's master id.</summary>
extern int32_t MasterClanAntiMissileSystemID;
/// <summary>The Inner Sphere anti-missile system's master id.</summary>
extern int32_t MasterInnerSphereAntiMissileSystemID;
