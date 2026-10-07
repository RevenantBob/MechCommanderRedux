#include "stdafx.h"
#include "object/cmponent.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"

const char* ComponentFormString[21] = {
    "Simple",       "Cockpit",         "Sensors",       "Actuator", "Engine",  "HeatSink", "Weapon",
    "EnergyWeapon", "BallisticWeapon", "MissileWeapon", "Ammo",     "JumpJet", "Case",     "LifeSupport",
    "Gyroscope",    "PowerAmplifier",  "ECM",           "Probe",    "Jammer",  "Bulk",     nullptr};
std::unique_ptr<MCMasterComponent[]> MasterComponentList;
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

auto MCMasterComponent::Destroy() -> void
{
}

auto MCMasterComponent::InitExcel(char* dataLine, uint8_t, float weaponRangeFactor, float sensorRangeFactor) -> int32_t
{
    MasterID = std::atoi(std::strtok(dataLine, ","));
    char* field = NextField();

    if (std::strcmp(field, "undefined") == 0)
    {
        MasterID = -1;
        return 0;
    }

    std::strncpy(Name, field, 0x1e);
    Abbreviation[0] = 0; // the original terminates the name by clearing the byte after it (+0x2a)
    std::strncpy(Abbreviation, NextField(), 0xf);
    Abbreviation[0xf] = 0;

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

    Form = formIndex;

    CriticalSpacesReq = static_cast<uint8_t>(std::atoi(NextField()));
    Health = static_cast<uint8_t>(std::atoi(NextField()));
    Tonnage = static_cast<float>(std::atof(NextField()));
    ResourcePoints = std::atoi(NextField());

    field = NextField();

    for (int32_t location = 0; location < 8; location++)
    {
        if (std::strcmp(field, "No") == 0)
        {
            CriticalSpacesLocation[location] = -1;
        }
        else if (std::strcmp(field, "Yes") == 0)
        {
            CriticalSpacesLocation[location] = 0;
        }
        else
        {
            CriticalSpacesLocation[location] = static_cast<int8_t>(std::atoi(field));
        }

        field = NextField();
    }

    Flags = 0;

    if (std::strcmp(field, "Yes") == 0)
    {
        Flags = 2;
    }

    if (std::strcmp(NextField(), "Yes") == 0)
    {
        Flags |= 1;
    }

    field = NextField();

    if (std::strcmp(field, "Both") == 0)
    {
        TechBase = 3;
    }
    else if (std::strcmp(field, "Clan") == 0)
    {
        TechBase = 1;
    }
    else if (std::strcmp(field, "IS") == 0)
    {
        TechBase = 2;
    }

    if (std::strcmp(NextField(), "Yes") == 0)
    {
        Flags |= 0x30;
    }
    else if ((TechBase & 1) == 0)
    {
        Flags |= 0x20;
    }
    else
    {
        Flags |= 0x10;
    }

    Art = std::atoi(NextField());
    DisableLevel = static_cast<uint8_t>(std::atoi(NextField()));
    BattleRating = static_cast<float>(std::atof(NextField()));

    field = NextField();

    switch (Form)
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
            RangeOrHeat = static_cast<float>(std::atof(field) * sensorRangeFactor);
            return 0;
        }
        case COMPONENT_FORM_ENGINE:
        case COMPONENT_FORM_HEATSINK:
        {
            ShortValue = static_cast<int16_t>(std::atoi(field));
            return 0;
        }
        case COMPONENT_FORM_WEAPON_ENERGY:
        case COMPONENT_FORM_WEAPON_BALLISTIC:
        case COMPONENT_FORM_WEAPON_MISSILE:
        {
            Damage = static_cast<float>(std::atof(field));
            RecycleTime = static_cast<float>(std::atof(NextField()));
            RangeOrHeat = TruncatedHeat(NextField());
            NumMissiles = std::atoi(NextField());
            field = NextField();
            MissileType = 0;

            if (Form == COMPONENT_FORM_WEAPON_BALLISTIC && std::strcmp(field, "1") == 0)
            {
                MissileType = 4;
            }
            else
            {
                MissileType = MissileTypeOf(field);
            }
            break;
        }
        case COMPONENT_FORM_AMMO:
        {
            LongValue = std::atoi(field);
            Damage = static_cast<float>(std::atof(NextField()));
            return 0;
        }
        case COMPONENT_FORM_JUMPJET:
        {
            LongValue = std::atoi(field);
            return 0;
        }
        case COMPONENT_FORM_ECM:
        {
            Damage = static_cast<float>(std::atof(field));
            RangeOrHeat = static_cast<float>(std::atof(NextField()));
            return 0;
        }
        case COMPONENT_FORM_PROBE:
        case COMPONENT_FORM_JAMMER:
        {
            RangeOrHeat = static_cast<float>(std::atof(field));
            return 0;
        }
        default:
            return -2;
    }

    for (float& range : WeaponRange)
    {
        range = static_cast<float>(std::atof(NextField()) * weaponRangeFactor);
    }

    WeaponType = static_cast<int16_t>(std::atoi(NextField()));
    WeaponEffect = static_cast<uint8_t>(std::atoi(NextField()));
    AmmoMasterId = static_cast<uint8_t>(std::atoi(NextField()));
    WeaponFlags = static_cast<uint8_t>(std::atoi(NextField()));
    return 0;
}

auto MCMasterComponent::IsOffensiveWeapon() -> int
{
    return MasterID != MasterClanAntiMissileSystemID && MasterID != MasterInnerSphereAntiMissileSystemID ? 1 : 0;
}

auto MCMasterComponent::IsDefensiveWeapon() -> int
{
    return MasterID != MasterClanAntiMissileSystemID && MasterID != MasterInnerSphereAntiMissileSystemID ? 0 : 1;
}

auto MCMasterComponent::MultiplyWeaponRanges(float factor) -> void
{
    // x87: the product at double precision, truncated to a 32-bit int, of which only the low 16 bits are kept.
    for (float& range : WeaponRange)
    {
        range = static_cast<float>(static_cast<int16_t>(static_cast<int32_t>(static_cast<double>(factor) * range)));
    }
}

auto InitMasterComponentListExcel(char* fileName, int32_t numComponents, float weaponRangeFactor,
                                  float sensorRangeFactor) -> int32_t
{
    MasterComponentList = std::make_unique<MCMasterComponent[]>(static_cast<size_t>(numComponents));
    NumMasterComponents = numComponents;

    for (int32_t i = 0; i < numComponents; i++)
    {
        MCMasterComponent* component = &MasterComponentList[i];
        component->MasterID = -1;
        component->Name[0] = 0;
        component->Abbreviation[0] = 0;
    }

    MasterArmActuatorID = -1;
    MasterLegActuatorID = -1;
    MasterClanAntiMissileSystemID = -1;
    MasterInnerSphereAntiMissileSystemID = -1;

    MCFile componentFile;
    int32_t result = componentFile.Open(fileName);

    if (result != 0)
    {
        return result;
    }

    uint8_t dataLine[512];

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterArmActuatorID = %d", &MasterArmActuatorID);

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterLegActuatorID = %d", &MasterLegActuatorID);

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterClanAntiMissileSystemID = %d",
                &MasterClanAntiMissileSystemID);

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    std::sscanf(reinterpret_cast<char*>(dataLine), "MasterInnerSphereAntiMissileSystemID = %d",
                &MasterInnerSphereAntiMissileSystemID);
    componentFile.ReadLine(dataLine, 0x1ff);

    if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
    {
        Fatal(0, "Bad MasterComponent File");
    }

    for (int32_t i = 0; i < numComponents; i++)
    {
        if (componentFile.ReadLine(dataLine, 0x1ff) == 0)
        {
            return -1;
        }

        MasterComponentList[i].InitExcel(reinterpret_cast<char*>(dataLine), static_cast<uint8_t>(i), weaponRangeFactor,
                                         sensorRangeFactor);
    }

    componentFile.Close();
    return 0;
}

auto MultiplyMasterWeaponRanges(float factor) -> void
{
    for (int32_t i = 0; i < NumMasterComponents; i++)
    {
        if (MasterComponentList[i].Form > COMPONENT_FORM_WEAPON && MasterComponentList[i].Form < COMPONENT_FORM_AMMO)
        {
            MasterComponentList[i].MultiplyWeaponRanges(factor);
        }
    }
}
