#include "stdafx.h"
#include "object/cmponent.h"
#include "lib/aerror.h"
#include "lib/file.h"

const char* ComponentFormString[21] = {
    "Simple",       "Cockpit",         "Sensors",       "Actuator", "Engine",  "HeatSink", "Weapon",
    "EnergyWeapon", "BallisticWeapon", "MissileWeapon", "Ammo",     "JumpJet", "Case",     "LifeSupport",
    "Gyroscope",    "PowerAmplifier",  "ECM",           "Probe",    "Jammer",  "Bulk",     nullptr};
std::unique_ptr<MasterComponent[]> MasterComponentList;
int32_t NumMasterComponents = 0;
int32_t MasterArmActuatorID = -1;
int32_t MasterLegActuatorID = -1;
int32_t MasterClanAntiMissileSystemID = -1;
int32_t MasterInnerSphereAntiMissileSystemID = -1;

namespace
{
    /// <summary>The next comma-separated field of the row being read.</summary>
    char* NextField()
    {
        return std::strtok(nullptr, ",");
    }

    /// <summary>A weapon's heat column as the original stores it: truncated, then read back as unsigned.</summary>
    float TruncatedHeat(const char* field)
    {
        return static_cast<float>(static_cast<uint32_t>(static_cast<int32_t>(std::atof(field))));
    }

    /// <summary>The "miss type" column of an energy or missile weapon: 1 SRM, 2 LRM, 3 ST, else 0.</summary>
    uint8_t MissileTypeOf(const char* field)
    {
        if (std::strcmp(field, "SRM") == 0)
        {
            return 1;
        }

        if (std::strcmp(field, "LRM") == 0)
        {
            return 2;
        }

        if (std::strcmp(field, "ST") == 0)
        {
            return 3;
        }

        return 0;
    }
}

auto MasterComponent::destroy() -> void
{
}

auto MasterComponent::initEXCEL(char* dataLine, uint8_t, float weaponRangeFactor, float sensorRangeFactor) -> int32_t
{
    masterID = std::atoi(std::strtok(dataLine, ","));
    char* field = NextField();

    if (std::strcmp(field, "undefined") == 0)
    {
        masterID = -1;
        return 0;
    }

    std::strncpy(name, field, 0x1e);
    abbreviation[0] = 0; // the original terminates the name by clearing the byte after it (+0x2a)
    std::strncpy(abbreviation, NextField(), 0xf);
    abbreviation[0xf] = 0;

    field = NextField();
    int32_t formIndex = 0;

    while (ComponentFormString[formIndex] != nullptr && std::strcmp(field, ComponentFormString[formIndex]) != 0)
    {
        formIndex++;
    }

    if (ComponentFormString[formIndex] == nullptr)
    {
        return -1;
    }

    form = formIndex;

    criticalSpacesReq = static_cast<uint8_t>(std::atoi(NextField()));
    health = static_cast<uint8_t>(std::atoi(NextField()));
    tonnage = static_cast<float>(std::atof(NextField()));
    resourcePoints = std::atoi(NextField());

    field = NextField();

    for (int32_t location = 0; location < 8; location++)
    {
        if (std::strcmp(field, "No") == 0)
        {
            criticalSpacesLocation[location] = -1;
        }
        else if (std::strcmp(field, "Yes") == 0)
        {
            criticalSpacesLocation[location] = 0;
        }
        else
        {
            criticalSpacesLocation[location] = static_cast<int8_t>(std::atoi(field));
        }

        field = NextField();
    }

    flags = 0;

    if (std::strcmp(field, "Yes") == 0)
    {
        flags = 2;
    }

    if (std::strcmp(NextField(), "Yes") == 0)
    {
        flags |= 1;
    }

    field = NextField();

    if (std::strcmp(field, "Both") == 0)
    {
        techBase = 3;
    }
    else if (std::strcmp(field, "Clan") == 0)
    {
        techBase = 1;
    }
    else if (std::strcmp(field, "IS") == 0)
    {
        techBase = 2;
    }

    if (std::strcmp(NextField(), "Yes") == 0)
    {
        flags |= 0x30;
    }
    else if ((techBase & 1) == 0)
    {
        flags |= 0x20;
    }
    else
    {
        flags |= 0x10;
    }

    art = std::atoi(NextField());
    disableLevel = static_cast<uint8_t>(std::atoi(NextField()));
    battleRating = static_cast<float>(std::atof(NextField()));

    field = NextField();

    switch (form)
    {
        case COMPONENT_FORM_SIMPLE:
        case COMPONENT_FORM_COCKPIT:
        case COMPONENT_FORM_ACTUATOR:
        case COMPONENT_FORM_CASE:
        case COMPONENT_FORM_LIFESUPPORT:
        case COMPONENT_FORM_GYROSCOPE:
        case COMPONENT_FORM_POWER_AMPLIFIER:
        case COMPONENT_FORM_BULK:
            return 0;
        case COMPONENT_FORM_SENSOR:
        {
            rangeOrHeat = static_cast<float>(std::atof(field) * sensorRangeFactor);
            return 0;
        }
        case COMPONENT_FORM_ENGINE:
        case COMPONENT_FORM_HEATSINK:
        {
            shortValue = static_cast<int16_t>(std::atoi(field));
            return 0;
        }
        case COMPONENT_FORM_WEAPON_ENERGY:
        case COMPONENT_FORM_WEAPON_BALLISTIC:
        case COMPONENT_FORM_WEAPON_MISSILE:
        {
            damage = static_cast<float>(std::atof(field));
            recycleTime = static_cast<float>(std::atof(NextField()));
            rangeOrHeat = TruncatedHeat(NextField());
            numMissiles = std::atoi(NextField());
            field = NextField();
            missileType = 0;

            if (form == COMPONENT_FORM_WEAPON_BALLISTIC && std::strcmp(field, "1") == 0)
            {
                missileType = 4;
            }
            else
            {
                missileType = MissileTypeOf(field);
            }
            break;
        }
        case COMPONENT_FORM_AMMO:
        {
            longValue = std::atoi(field);
            damage = static_cast<float>(std::atof(NextField()));
            return 0;
        }
        case COMPONENT_FORM_JUMPJET:
        {
            longValue = std::atoi(field);
            return 0;
        }
        case COMPONENT_FORM_ECM:
        {
            damage = static_cast<float>(std::atof(field));
            rangeOrHeat = static_cast<float>(std::atof(NextField()));
            return 0;
        }
        case COMPONENT_FORM_PROBE:
        case COMPONENT_FORM_JAMMER:
        {
            rangeOrHeat = static_cast<float>(std::atof(field));
            return 0;
        }
        default:
            return -2;
    }

    for (float& range : weaponRange)
    {
        range = static_cast<float>(std::atof(NextField()) * weaponRangeFactor);
    }

    weaponType = static_cast<int16_t>(std::atoi(NextField()));
    weaponEffect = static_cast<uint8_t>(std::atoi(NextField()));
    ammoMasterId = static_cast<uint8_t>(std::atoi(NextField()));
    weaponFlags = static_cast<uint8_t>(std::atoi(NextField()));
    return 0;
}

auto MasterComponent::isOffensiveWeapon() -> int
{
    return masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID ? 1 : 0;
}

auto MasterComponent::isDefensiveWeapon() -> int
{
    return masterID != MasterClanAntiMissileSystemID && masterID != MasterInnerSphereAntiMissileSystemID ? 0 : 1;
}

auto MasterComponent::multiplyWeaponRanges(float factor) -> void
{
    // x87: the product at double precision, truncated to a 32-bit int, of which only the low 16 bits are kept.
    for (float& range : weaponRange)
    {
        range = static_cast<float>(static_cast<int16_t>(static_cast<int32_t>(static_cast<double>(factor) * range)));
    }
}

auto initMasterComponentListEXCEL(char* fileName, int32_t numComponents, float weaponRangeFactor,
                                  float sensorRangeFactor) -> int32_t
{
    MasterComponentList = std::make_unique<MasterComponent[]>(static_cast<size_t>(numComponents));
    NumMasterComponents = numComponents;

    for (int32_t i = 0; i < numComponents; i++)
    {
        MasterComponent* component = &MasterComponentList[i];
        component->masterID = -1;
        component->name[0] = 0;
        component->abbreviation[0] = 0;
    }

    MasterArmActuatorID = -1;
    MasterLegActuatorID = -1;
    MasterClanAntiMissileSystemID = -1;
    MasterInnerSphereAntiMissileSystemID = -1;

    File componentFile;
    int32_t result = componentFile.open(fileName, READ, 0x32);

    if (result != 0)
    {
        return result;
    }

    uint8_t dataLine[512];

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterArmActuatorID = %d", &MasterArmActuatorID);

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterLegActuatorID = %d", &MasterLegActuatorID);

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterClanAntiMissileSystemID = %d",
                &MasterClanAntiMissileSystemID);

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterInnerSphereAntiMissileSystemID = %d",
                &MasterInnerSphereAntiMissileSystemID);
    componentFile.readLine(dataLine, 0x1ff);

    if (componentFile.readLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    for (int32_t i = 0; i < numComponents; i++)
    {
        if (componentFile.readLine(dataLine, 0x1ff) == 0)
        {
            return -1;
        }

        MasterComponentList[i].initEXCEL(reinterpret_cast<char*>(dataLine), static_cast<uint8_t>(i), weaponRangeFactor,
                                         sensorRangeFactor);
    }

    componentFile.close();
    return 0;
}

auto multiplyMasterWeaponRanges(float factor) -> void
{
    for (int32_t i = 0; i < NumMasterComponents; i++)
    {
        if (MasterComponentList[i].form > COMPONENT_FORM_WEAPON && MasterComponentList[i].form < COMPONENT_FORM_AMMO)
        {
            MasterComponentList[i].multiplyWeaponRanges(factor);
        }
    }
}
