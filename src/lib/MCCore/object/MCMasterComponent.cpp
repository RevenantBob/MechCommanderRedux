#include "stdafx.h"
#include "object/MCMasterComponent.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"

const std::array<std::string_view, 20> ComponentFormString = {
    "Simple",       "Cockpit",         "Sensors",       "Actuator", "Engine",  "HeatSink", "Weapon",
    "EnergyWeapon", "BallisticWeapon", "MissileWeapon", "Ammo",     "JumpJet", "Case",     "LifeSupport",
    "Gyroscope",    "PowerAmplifier",  "ECM",           "Probe",    "Jammer",  "Bulk"};
std::vector<MCMasterComponent> MasterComponentList;
int32_t MasterArmActuatorID = -1;
int32_t MasterLegActuatorID = -1;
int32_t MasterClanAntiMissileSystemID = -1;
int32_t MasterInnerSphereAntiMissileSystemID = -1;

namespace
{
    /// <summary>The fields of a row as strtok splits it at commas: runs of commas separate, empty fields vanish.</summary>
    class MCCsvFields
    {
    public:
        explicit MCCsvFields(std::string_view line) : _Rest(line) {}

        /// <summary>The next field (empty at the end of the row).</summary>
        std::string Next()
        {
            const size_t start = _Rest.find_first_not_of(',');

            if (start == std::string_view::npos)
            {
                _Rest = {};
                return {};
            }

            _Rest.remove_prefix(start);
            const size_t end = std::min(_Rest.find(','), _Rest.size());
            std::string field(_Rest.substr(0, end));
            _Rest.remove_prefix(end);
            return field;
        }

    private:
        std::string_view _Rest;
    };

    /// <summary>atoi of a field.</summary>
    int32_t ToInt(const std::string& field)
    {
        return std::atoi(field.c_str());
    }

    /// <summary>atof of a field.</summary>
    double ToDouble(const std::string& field)
    {
        return std::atof(field.c_str());
    }

    /// <summary>A weapon's heat column as the original stores it: truncated, then read back as unsigned.</summary>
    float TruncatedHeat(const std::string& field)
    {
        return static_cast<float>(static_cast<uint32_t>(static_cast<int32_t>(ToDouble(field))));
    }

    /// <summary>The "miss type" column of an energy or missile weapon: 1 SRM, 2 LRM, 3 ST, else 0.</summary>
    uint8_t MissileTypeOf(std::string_view field)
    {
        if (field == "SRM")
        {
            return 1;
        }

        if (field == "LRM")
        {
            return 2;
        }

        if (field == "ST")
        {
            return 3;
        }

        return 0;
    }
}

auto MCMasterComponent::InitExcel(std::string_view dataLine, float weaponRangeFactor, float sensorRangeFactor)
    -> int32_t
{
    MCCsvFields fields(dataLine);
    MasterID = ToInt(fields.Next());
    std::string field = fields.Next();

    if (field == "undefined")
    {
        MasterID = -1;
        return 0;
    }

    Name = field.substr(0, 30);
    Abbreviation = fields.Next().substr(0, 15);
    field = fields.Next();
    const auto form = std::ranges::find(ComponentFormString, field);

    if (form == ComponentFormString.end())
    {
        return -1;
    }

    Form = static_cast<MCComponentForm>(form - ComponentFormString.begin());
    CriticalSpacesReq = static_cast<uint8_t>(ToInt(fields.Next()));
    Health = static_cast<uint8_t>(ToInt(fields.Next()));
    Tonnage = static_cast<float>(ToDouble(fields.Next()));
    ResourcePoints = ToInt(fields.Next());
    field = fields.Next();

    for (int8_t& location : CriticalSpacesLocation)
    {
        if (field == "No")
        {
            location = -1;
        }
        else if (field == "Yes")
        {
            location = 0;
        }
        else
        {
            location = static_cast<int8_t>(ToInt(field));
        }

        field = fields.Next();
    }

    Flags = field == "Yes" ? 2 : 0;

    if (fields.Next() == "Yes")
    {
        Flags |= 1;
    }

    field = fields.Next();

    if (field == "Both")
    {
        TechBase = 3;
    }
    else if (field == "Clan")
    {
        TechBase = 1;
    }
    else if (field == "IS")
    {
        TechBase = 2;
    }

    if (fields.Next() == "Yes")
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

    Art = ToInt(fields.Next());
    DisableLevel = static_cast<uint8_t>(ToInt(fields.Next()));
    BattleRating = static_cast<float>(ToDouble(fields.Next()));
    field = fields.Next();

    switch (Form)
    {
        case MCComponentForm::Simple:
        case MCComponentForm::Cockpit:
        case MCComponentForm::Actuator:
        case MCComponentForm::Case:
        case MCComponentForm::LifeSupport:
        case MCComponentForm::Gyroscope:
        case MCComponentForm::PowerAmplifier:
        case MCComponentForm::Bulk:
            return 0;
        case MCComponentForm::Sensor:
        {
            RangeOrHeat = static_cast<float>(ToDouble(field) * sensorRangeFactor);
            return 0;
        }
        case MCComponentForm::Engine:
        case MCComponentForm::HeatSink:
        {
            ShortValue = static_cast<int16_t>(ToInt(field));
            return 0;
        }
        case MCComponentForm::WeaponEnergy:
        case MCComponentForm::WeaponBallistic:
        case MCComponentForm::WeaponMissile:
        {
            Damage = static_cast<float>(ToDouble(field));
            RecycleTime = static_cast<float>(ToDouble(fields.Next()));
            RangeOrHeat = TruncatedHeat(fields.Next());
            NumMissiles = ToInt(fields.Next());
            field = fields.Next();
            MissileType = Form == MCComponentForm::WeaponBallistic && field == "1" ? 4 : MissileTypeOf(field);
            break;
        }
        case MCComponentForm::Ammo:
        {
            LongValue = ToInt(field);
            Damage = static_cast<float>(ToDouble(fields.Next()));
            return 0;
        }
        case MCComponentForm::JumpJet:
        {
            LongValue = ToInt(field);
            return 0;
        }
        case MCComponentForm::Ecm:
        {
            Damage = static_cast<float>(ToDouble(field));
            RangeOrHeat = static_cast<float>(ToDouble(fields.Next()));
            return 0;
        }
        case MCComponentForm::Probe:
        case MCComponentForm::Jammer:
        {
            RangeOrHeat = static_cast<float>(ToDouble(field));
            return 0;
        }
        default:
            return -2;
    }

    for (float& range : WeaponRange)
    {
        range = static_cast<float>(ToDouble(fields.Next()) * weaponRangeFactor);
    }

    WeaponType = static_cast<int16_t>(ToInt(fields.Next()));
    WeaponEffect = static_cast<uint8_t>(ToInt(fields.Next()));
    AmmoMasterId = static_cast<uint8_t>(ToInt(fields.Next()));
    WeaponFlags = static_cast<uint8_t>(ToInt(fields.Next()));
    return 0;
}

auto InitMasterComponentListExcel(std::string_view fileName, int32_t numComponents, float weaponRangeFactor,
                                  float sensorRangeFactor) -> int32_t
{
    MasterComponentList.assign(static_cast<size_t>(numComponents), MCMasterComponent{});
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

    std::array<uint8_t, 512> dataLine{};
    const auto lineText = [&dataLine] { return reinterpret_cast<const char*>(dataLine.data()); };
    const auto line = [&lineText] { return std::string_view(lineText()); };
    const auto readHeaderLine = [&]
    {
        if (componentFile.ReadLine(dataLine.data(), 0x1ff) == 0)
        {
            Fatal(0, "Bad MasterComponent File");
        }
    };

    // A header line, the four special ids, a line skipped, and the column titles.
    readHeaderLine();
    readHeaderLine();
    std::sscanf(lineText(), "MasterArmActuatorID = %d", &MasterArmActuatorID);
    readHeaderLine();
    std::sscanf(lineText(), "MasterLegActuatorID = %d", &MasterLegActuatorID);
    readHeaderLine();
    std::sscanf(lineText(), "MasterClanAntiMissileSystemID = %d", &MasterClanAntiMissileSystemID);
    readHeaderLine();
    std::sscanf(lineText(), "MasterInnerSphereAntiMissileSystemID = %d", &MasterInnerSphereAntiMissileSystemID);
    componentFile.ReadLine(dataLine.data(), 0x1ff);
    readHeaderLine();

    for (MCMasterComponent& component : MasterComponentList)
    {
        if (componentFile.ReadLine(dataLine.data(), 0x1ff) == 0)
        {
            return -1;
        }

        component.InitExcel(line(), weaponRangeFactor, sensorRangeFactor);
    }

    componentFile.Close();
    return 0;
}
