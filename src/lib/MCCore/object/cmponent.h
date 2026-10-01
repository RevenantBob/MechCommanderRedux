#pragma once

/// <summary>A component's kind: the "type" column of compbas.csv, an index into <see cref="ComponentFormString"/>.</summary>
enum ComponentForm : int32_t
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
/// weight, critical spaces and, by <see cref="form"/>, its stats. <see cref="MasterComponentList"/> holds them by
/// id.
/// </summary>
/// <remarks>Original source: <c>object\cmponent.cpp</c>; 0x84 bytes. Allocated from systemHeap.</remarks>
class MasterComponent
{
public:
    /// <summary>Allocates from systemHeap.</summary>
    /// <remarks>MCX.EXE @ 0x00655d80</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into systemHeap.</summary>
    /// <remarks>MCX.EXE @ 0x00655da0</remarks>
    static void operator delete(void* ptr);

    /// <remarks>MCX.EXE @ 0x00655dc0</remarks>
    void destroy();
    /// <summary>
    /// Reads one CSV row (tokenized in place with strtok). An "undefined" row leaves the id -1. Weapon ranges are
    /// scaled by <paramref name="weaponRangeFactor"/>, a sensor's range by <paramref name="sensorRangeFactor"/>.
    /// Returns 0, -1 for an unknown form, -2 for a form with no stats (plain "Weapon").
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00655dd0</remarks>
    int32_t initEXCEL(char* dataLine, uint8_t index, float weaponRangeFactor, float sensorRangeFactor);
    /// <summary>Whether this is anything but an anti-missile system.</summary>
    /// <remarks>MCX.EXE @ 0x00656760</remarks>
    int isOffensiveWeapon();
    /// <summary>Whether this is an anti-missile system.</summary>
    /// <remarks>MCX.EXE @ 0x00656780</remarks>
    int isDefensiveWeapon();
    /// <summary>Scales the four weapon ranges by <paramref name="factor"/>, truncating each to a short.</summary>
    /// <remarks>MCX.EXE @ 0x006567a0</remarks>
    void multiplyWeaponRanges(float factor);

    /// <summary>Master id; -1 for an unused ("undefined") row.</summary>
    int32_t masterID = -1; // +0x00
    /// <summary>"RP": resource points.</summary>
    int32_t resourcePoints = 0; // +0x04
    /// <summary>"type": the <see cref="ComponentForm"/>.</summary>
    int32_t form = 0; // +0x08
    /// <summary>Full name (29 characters).</summary>
    char name[30] = {}; // +0x0c
    /// <summary>"abbr" (15 characters).</summary>
    char abbreviation[16] = {}; // +0x2a
    /// <summary>"tons".</summary>
    float tonnage = 0.0f; // +0x3c
    /// <summary>"crits": critical spaces it takes.</summary>
    uint8_t criticalSpacesReq = 0; // +0x40
    /// <summary>The column after crits ("?").</summary>
    uint8_t health = 0; // +0x41
    /// <summary>
    /// Per location (head, CT, LT, RT, LA, RA, LL, RL): "No" -1 (can't go there), "Yes" 0, else the critical spaces
    /// it must take there.
    /// </summary>
    int8_t criticalSpacesLocation[8] = {}; // +0x42
    /// <summary>"disable".</summary>
    uint8_t disableLevel = 0; // +0x4a
    /// <summary>
    /// Bits: 2 fits vehicles ("Vehicle?"), 1 fits both ("Fit both?"), 0x30 "Fit IS?" yes, else 0x10 (clan tech) or
    /// 0x20.
    /// </summary>
    uint8_t flags = 0; // +0x4b
    /// <summary>"Side": 1 clan, 2 Inner Sphere, 3 both.</summary>
    uint8_t techBase = 0; // +0x4c
    /// <summary>"BR": battle rating.</summary>
    float battleRating = 0.0f; // +0x50
    /// <summary>
    /// By form: a sensor's, probe's, jammer's or ECM's range (float); an energy/ballistic/missile weapon's heat
    /// (float); an engine's or heat sink's value (short); ammo per ton or jump jet value (int).
    /// </summary>
    union
    {
        float rangeOrHeat;
        int16_t shortValue;
        int32_t longValue = 0;
    }; // +0x54

    /// <summary>A weapon's damage; an ECM's effect; ammo's second value.</summary>
    float damage = 0.0f; // +0x58
    /// <summary>A weapon's recycle time.</summary>
    float recycleTime = 0.0f; // +0x5c
    /// <summary>"#miss": missiles per salvo.</summary>
    int32_t numMissiles = 0; // +0x60
    /// <summary>"miss type": 1 SRM, 2 LRM, 3 ST, 4 ballistic "1".</summary>
    uint8_t missileType = 0; // +0x64
    /// <summary>"Ammo": the master id of the weapon's ammo.</summary>
    uint8_t ammoMasterId = 0; // +0x65
    /// <summary>"min", "short", "med", "long" ranges (scaled).</summary>
    float weaponRange[4] = {}; // +0x68
    /// <summary>"Flag": 1 streak, 2 inferno, 4 LBX, 8 artillery.</summary>
    uint8_t weaponFlags = 0; // +0x78
    /// <summary>"Weapon": the weapon's graphic type.</summary>
    int16_t weaponType = 0; // +0x7a
    /// <summary>"Effect".</summary>
    uint8_t weaponEffect = 0; // +0x7c
    /// <summary>"art".</summary>
    int32_t art = 0; // +0x80
};

/// <summary>
/// Reads the master component table from <paramref name="fileName"/> (the four special ids, then
/// <paramref name="numComponents"/> rows); 0, the File error, or -1 when the file is short.
/// </summary>
/// <remarks>MCX.EXE @ 0x00656810</remarks>
int32_t initMasterComponentListEXCEL(char* fileName, int32_t numComponents, float weaponRangeFactor,
                                     float sensorRangeFactor);
/// <summary>Scales every energy, ballistic and missile weapon's ranges.</summary>
/// <remarks>MCX.EXE @ 0x00656a80</remarks>
void multiplyMasterWeaponRanges(float factor);

/// <summary>The form names, by <see cref="ComponentForm"/>, null-terminated.</summary>
extern const char* ComponentFormString[21];
/// <summary>The master component table.</summary>
extern MasterComponent* MasterComponentList;
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
