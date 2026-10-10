#include "stdafx.h"
#include "main/MCLogistics.h"
#include "ai/MCMoveGeometry.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "logistics/MCMissionLogisticsBridge.h"
#include "main/MCGamePaths.h"
#include "main/MCLogisticsShared.h"
#include "main/MCSystemConfig.h"
#include "main/MCMissionGlobals.h"
#include "main/MCGameStrings.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"

namespace
{
    /// <summary>The most of the mission's own inactive or capturable player-side parts that join the force (a game rule).</summary>
    constexpr int32_t MaxRavenParts = 4;

    /// <summary>
    /// Copies a mission's scenario file into a start file entry by entry. The values read go through one set of
    /// variables, as the original's did: an entry that can't be read writes what the last read of its type left.
    /// </summary>
    class MCScenarioCopier
    {
    public:
        MCScenarioCopier(MCFitIniFile& in, MCFitIniFile& out) : In(in), Out(out) {}

        /// <summary>Reports a failed step (the game goes on).</summary>
        static void Check(bool ok, std::string_view message) { Assert(ok, 0, message); }

        /// <summary>Copies block <paramref name="block"/>'s header (the entries are copied one by one).</summary>
        void Block(std::string_view block, std::string_view findMessage, std::string_view writeMessage)
        {
            Check(In.SeekBlock(block) == 0, findMessage);
            Check(Out.WriteBlock(block) > 0, writeMessage);
        }

        void Long(std::string_view name, std::string_view findMessage, std::string_view writeMessage)
        {
            Check(ReadEntry(In, name, LongValue), findMessage);
            Check(Out.WriteIdLong(name, LongValue) > 0, writeMessage);
        }

        void ULong(std::string_view name, std::string_view findMessage, std::string_view writeMessage)
        {
            Check(ReadEntry(In, name, ULongValue), findMessage);
            Check(Out.WriteIdULong(name, ULongValue) > 0, writeMessage);
        }

        void Float(std::string_view name, std::string_view findMessage, std::string_view writeMessage)
        {
            Check(ReadEntry(In, name, FloatValue), findMessage);
            Check(Out.WriteIdFloat(name, FloatValue) > 0, writeMessage);
        }

        void String(std::string_view name, size_t maxLength, std::string_view findMessage,
                    std::string_view writeMessage)
        {
            Check(ReadText(In, name, maxLength, Text), findMessage);
            Check(Out.WriteIdString(name, Text) > 0, writeMessage);
        }

        void Char(std::string_view name, std::string_view findMessage, std::string_view writeMessage)
        {
            char value = 0;
            Check(ReadEntry(In, name, value), findMessage);
            Check(Out.WriteIdChar(name, value) > 0, writeMessage);
        }

        MCFitIniFile& In;
        MCFitIniFile& Out;
        int32_t LongValue = 0;
        uint32_t ULongValue = 0;
        float FloatValue = 0.0f;
        std::string Text;
    };

    /// <summary>A salvaged part's place on the map.</summary>
    struct MCPartPlace
    {
        float X = 0;
        float Y = 0;
        float Rotation = 0;
    };
}

auto MCLogistics::PrepareScenario(std::string_view scenarioName, std::string_view startFile) -> int32_t
{
    LastLogisticsMissionState = CurrentMission;

    if (MultiplayerInitialized)
    {
        return PrepareMultiplayerScenario(scenarioName, startFile);
    }

    {
        // The automatic "before the mission" save, named after the operation and mission.
        MCFitIniFile masterFile;
        int32_t result = masterFile.Open(GamePath(MissionPath, MissionName, ".fit"));
        Assert(result == 0, 0, " could not open master mission file ");
        result = masterFile.SeekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file");
        const auto operationNumber =
            ReadRequired<int32_t>(masterFile, std::format("Scenario{}Operation", LastLogisticsMissionState),
                                  " could not find operation number in master mission file ");
        const auto missionNumber =
            ReadRequired<int32_t>(masterFile, std::format("Scenario{}Mission", LastLogisticsMissionState),
                                  " could not find mission number in master mission file ");

        if (!Solo)
        {
            // "Operation %d Mission %d" (a printf format from the string table).
            const std::string format = LoadGameString(CurPlanet == 0 ? 0x37b : 0x387, 199);
            SaveCampaign(MCFormatPrintf(format.c_str(), operationNumber, missionNumber));
        }

        // Write the deployed mechs (with their pilots) and vehicles as mech####/warr#### profiles.
        int32_t profileNumber = 0;

        for (const auto& lance : DeploySlots)
        {
            for (const DeploySlot& deploy : lance)
            {
                if (deploy.Unit < 0)
                {
                    MCLogVehicle* vehicle = nullptr;

                    if (deploy.Vehicle < 0 || ForceVehicleList->GetVehicleInfo(deploy.Vehicle, vehicle) != 0)
                    {
                        continue;
                    }

                    ForceVehicleList->SaveVehicleText(std::format("mech{:04}", profileNumber), deploy.Vehicle);
                    profileNumber++;
                    continue;
                }

                MCLogMech* mech = nullptr;

                if (ForceMechList->GetMechInfo(deploy.Unit, mech) != 0)
                {
                    continue;
                }

                ForceMechList->SaveMechText(std::format("mech{:04}", profileNumber), deploy.Unit);
                AssignedWarriorList->SaveWarriorText(std::format("warr{:04}", profileNumber),
                                                     ForceMechList->GetMechPilotIndex(deploy.Unit));
                profileNumber++;
            }
        }
    }

    // Copy the mission's scenario file into the start file, block by block, then add the player's force.
    // Port: the original allocated both FitIniFiles (asserting it got the memory).
    MCFitIniFile in;
    int32_t result = in.Open(GamePath(MissionPath, scenarioName, ".fit"));
    Assert(result == 0, static_cast<uint32_t>(result), " could not open logistics scenario file ");
    MCFitIniFile out;
    result = out.Create(GamePath(SaveTempPath, startFile, ".fit"));
    Assert(result == 0, static_cast<uint32_t>(result), " could not open scenario file ");
    MCScenarioCopier copy(in, out);
    auto check = &MCScenarioCopier::Check;

    copy.Block("ContactManager", " could not find ContactManager Block ", " could not write ContactManager Block ");
    copy.Long("MaxContacts", " could not find MaxContacts in ContactManager Block ",
              " could not write MaxContacts in ContactManager Block ");
    copy.Block("PotentialContactManager", " could not find PotentialContactManager Block ",
               " could not write PotentialContactManager Block ");
    copy.Long("MaxPotentialContacts", " could not find MaxPotentialContacts in PotentialContactManager Block ",
              " could not write MaxPotentialContacts in PotentialContactManager Block ");
    copy.Block("ABLibraries", " could not find ABLibraries Block ", " could not write ABLibraries Block ");

    for (int32_t index = 0;; index++)
    {
        const std::string name = std::format("Library{}", index);

        if (!ReadText(in, name, 0xff, copy.Text))
        {
            break;
        }

        check(out.WriteIdString(name, copy.Text) > 0, " could not write library string in ABLibraries Block ");
    }

    copy.Block("Smoke Manager", " could not find Smoke Manager Block ", " could not write Smoke Manager Block ");
    int32_t numSmokeTypes = 0;
    check(ReadEntry(in, "NumSmokeTypes", numSmokeTypes), " could not find NumSmokeTypes in Smoke Manager Block ");
    check(out.WriteIdLong("NumSmokeTypes", numSmokeTypes) > 0,
          " could not write NumSmokeTypes in Smoke Manager Block ");
    copy.Long("MaxSmokesPerType", " could not find MaxSmokesPerType in Smoke Manager Block ",
              " could not write MaxSmokesPerType in Smoke Manager Block ");
    copy.ULong("SmokeSphereHeapSize", " could not find SmokeSphereHeapSize in Smoke Manager Block ",
               " could not write SmokeSphereHeapSize in Smoke Manager Block ");

    for (int32_t index = 0; index < numSmokeTypes; index++)
    {
        copy.Block(std::format("Smoke{}", index), " could not find smoke Block ", " could not write smoke Block ");
        copy.Long("SmokeType", " could not find SmokeType in Smoke Block ",
                  " could not write SmokeType in Smoke Block ");
    }

    copy.Block("CollisionSystem", " could not find CollisionSystem Block ", " could not write CollisionSystem Block ");
    copy.ULong("XGridSize", " could not find XGridSize in CollisionSystem Block ",
               " could not write XGridSize in CollisionSystem Block ");
    copy.ULong("YGridSize", " could not find YGridSize in CollisionSystem Block ",
               " could not write YGridSize in CollisionSystem Block ");
    copy.ULong("GridRadius", " could not find GridRadius in CollisionSystem Block ",
               " could not write GridRadius in CollisionSystem Block ");
    copy.ULong("MaxObjects", " could not find MaxObjects in CollisionSystem Block ",
               " could not write MaxObjects in CollisionSystem Block ");
    copy.ULong("MaxCollisions", " could not find MaxCollisions in CollisionSystem Block ",
               " could not write MaxCollisions in CollisionSystem Block ");
    copy.ULong("MaxPending", " could not find MaxPending in CollisionSystem Block ",
               " could not write MaxPending in CollisionSystem Block ");
    copy.ULong("CollisionHeapSize", " could not find CollisionHeapSize in CollisionSystem Block ",
               " could not write CollisionHeapSize in CollisionSystem Block ");
    copy.Float("WarningDist", " could not find WarningDist in CollisionSystem Block ",
               " could not write WarningDist in CollisionSystem Block ");
    copy.Float("AlertTime", " could not find AlertTime in CollisionSystem Block ",
               " could not write AlertTime in CollisionSystem Block ");
    copy.ULong("NumAlerts", " could not find NumAlerts in CollisionSystem Block ",
               " could not write NumAlerts in CollisionSystem Block ");

    if (in.SeekBlock("StatusWindow") == 0)
    {
        check(out.WriteBlock("StatusWindow") > 0, " could not write StatusWindow Block ");
        copy.ULong("PosX", " could not find PosX in StatusWindow Block ",
                   " could not write PosX in StatusWindow Block ");
        copy.ULong("PosY", " could not find PosY in StatusWindow Block ",
                   " could not write PosY in StatusWindow Block ");
        copy.ULong("Width", " could not find Width in StatusWindow Block ",
                   " could not write Width in StatusWindow Block ");
        copy.ULong("Height", " could not find Height in StatusWindow Block ",
                   " could not write Height in StatusWindow Block ");
    }

    copy.Block("PaletteSystem", " could not find PaletteSystem Block ", " could not write PaletteSystem Block ");
    copy.String("PaletteSystem", 0xff, " could not find PaletteSystem in PaletteSystem Block ",
                " could not write PaletteSystem in PaletteSystem Block ");
    copy.Block("Music", " could not find Music block in Scenario File ",
               " could not write Music block in Scenario File ");
    uint8_t tuneNumber = 0;
    check(ReadEntry(in, "scenarioTuneNum", tuneNumber),
          " could not find ScenarioTuneNum in Music block in Scenario File ");
    check(out.WriteIdUChar("scenarioTuneNum", tuneNumber) > 0,
          " could not write ScenarioTuneNum in Music block in Scenario File ");
    copy.Block("Artillery", " could not find Artillery block in Scenario File ",
               " could not write Artillery block in Scenario File ");

    // Original behaviour: the old format (NumStrikes) is not copied at all; the new one is copied without checks
    // after the first count. Port fix: the counts start at 0 (the original wrote leftovers when one was missing).
    if (!ReadEntry(in, "NumStrikes", copy.ULongValue))
    {
        int32_t strikes = 0;
        Assert(ReadEntry(in, "NumLargeStrikes", strikes), 0, " Artillery is in neither of the two known states ");
        out.WriteIdLong("NumLargeStrikes", strikes);

        for (const std::string_view name : {"NumSmallStrikes", "NumSensorStrikes", "NumCameraStrikes"})
        {
            strikes = 0;
            ReadEntry(in, name, strikes);
            out.WriteIdLong(name, strikes);
        }
    }

    copy.Block("GameScale", " could not find GameScale block in Scenario File ",
               " could not write GameScale block in Scenario File ");
    copy.Float("WorldUnitsPerMeter", " could not find worldUnitsperMeter in GameScale block in Scenario File ",
               " could not write worldUnitsperMeter in GameScale block in Scenario File ");
    copy.Float("MetersPerWorldUnit", " could not find MetersperWorldUnit in GameScale block in Scenario File ",
               " could not write MetersperWorldUnit in GameScale block in Scenario File ");
    copy.ULong("Duration", " could not find Duration in GameScale block in Scenario File ",
               " could not write Duration in GameScale block in Scenario File ");
    copy.Float("CycleLength", " could not find CycleLength in GameScale block in Scenario File ",
               " could not write CycleLength in GameScale block in Scenario File ");
    // Original behaviour: SingleStep is copied unchecked (0 when missing).
    ReadEntry(in, "SingleStep", copy.ULongValue);
    out.WriteIdULong("SingleStep", copy.ULongValue);
    copy.Block("ElementSystem", " could not find ElementSystem block in Scenario File ",
               " could not write ElementSystem block in Scenario File ");
    copy.ULong("ElementHeapSize", " could not find ElementHeapSize in ElementSystem block in Scenario File ",
               " could not write ElementHeapSize in ElementSystem block in Scenario File ");
    copy.ULong("MaxElements", " could not find MaxElements in ElementSystem block in Scenario File ",
               " could not write MaxElements in ElementSystem block in Scenario File ");
    copy.ULong("MaxGroups", " could not find MaxGroups in ElementSystem block in Scenario File ",
               " could not write MaxGroups in ElementSystem block in Scenario File ");
    copy.Block("SensorContactShape", " could not find SensorContactShape block in Scenario File ",
               " could not write SensorContactShape block in Scenario File ");
    copy.String("shapeName", 0x4f, " could not find ShapeName in SensorContactShape block in Scenario File ",
                " could not write ShapeName in SensorContactShape block in Scenario File ");
    copy.Block("CraterSystem", " could not find CraterSystem Block in Scenario File ",
               " could not write CraterSystem Block in Scenario File ");
    copy.Long("NumCraters", " could not find NumCraters in CraterSystem Block in Scenario File ",
              " could not write NumCraters in CraterSystem Block in Scenario File ");
    copy.ULong("CraterShapeSize", " could not find CraterShapeSize in CraterSystem Block in Scenario File ",
               " could not write CraterShapeSize in CraterSystem Block in Scenario File ");
    copy.String("CraterFile", 0xf, " could not find CraterFile in CraterSystem Block in Scenario File ",
                " could not write CraterFile in CraterSystem Block in Scenario File ");
    copy.Block("CameraSystem", " could not Find CameraSystem Block ", " could not write CameraSystem Block ");
    copy.ULong("CameraHeapSize", " could not Find CameraHeapSize in CameraSystem Block ",
               " could not write CameraHeapSize in CameraSystem Block ");
    copy.String("CameraFileName", 0x4f, " could not Find CameraFileName in CameraSystem Block ",
                " could not write CameraFileName in CameraSystem Block ");
    copy.Block("ObjectSystem", " could not Find ObjectSystem Block ", " could not write ObjectSystem Block ");
    copy.ULong("ObjectHeapSize", " could not Find objectHeapSize in ObjectSystem Block ",
               " could not write objectHeapSize in ObjectSystem Block ");
    copy.ULong("ObjectTypeHeapSize", " could not Find ObjectTypeHeapSzize in ObjectSystem Block ",
               " could not write ObjectTypeHeapSzize in ObjectSystem Block ");
    copy.ULong("NumObjects", " could not Find NumObjects in ObjectSystem Block ",
               " could not write NumObjects in ObjectSystem Block ");
    copy.String("ObjectFileName", 0x4f, " could not Find ObjectFileName in ObjectSystem Block ",
                " could not write ObjectFileName in ObjectSystem Block ");
    copy.Block("SpriteSystem", " could not Find SpriteSystem Block ", " could not write SpriteSystem Block ");
    copy.ULong("SpriteHeapSize", " could not Find SpriteHeapSize in SpriteSystem Block ",
               " could not write SpriteHeapSize in SpriteSystem Block ");
    copy.ULong("SpriteManagerHeapSize", " could not Find SpriteManagerHeapSize in SpriteSystem Block ",
               " could not write SpriteManagerHeapSize in SpriteSystem Block ");
    copy.ULong("SpriteDataHeapSize", " could not Find SpriteDataHeapSize in SpriteSystem Block ",
               " could not write SpriteDataHeapSize in SpriteSystem Block ");
    copy.String("SpriteFileName", 0x4f, " could not Find SpriteFileName in SpriteSystem Block ",
                " could not write SpriteFileName in SpriteSystem Block ");
    copy.String("ShapeFileName", 0x4f, " could not Find ShapeFileName in SpriteSystem Block ",
                " could not write ShapeFileName in SpriteSystem Block ");
    copy.Block("SpriteManager", " could not Find SpriteManager Block ", " could not write SpriteManager Block ");
    copy.ULong("LegHeapSize", " could not Find LegHeapSize in SpriteManager Block ",
               " could not write LegHeapSize in SpriteManager Block ");
    copy.ULong("TorsoHeapSize", " could not Find TorsoHeapSize in SpriteManager Block ",
               " could not write TorsoHeapSize in SpriteManager Block ");
    copy.ULong("RightArmHeapSize", " could not Find RightArmHeapSize in SpriteManager Block ",
               " could not write RightArmHeapSize in SpriteManager Block ");
    copy.ULong("LeftArmHeapSize", " could not Find LeftArmHeapSize in SpriteManager Block ",
               " could not write LeftArmHeapSize in SpriteManager Block ");
    copy.ULong("TotalMechs", " could not Find TotalMechs in SpriteManager Block ",
               " could not write TotalMechs in SpriteManager Block ");
    // Original behaviour: Use90Pixel is copied unchecked (0 when missing).
    ReadEntry(in, "Use90Pixel", copy.ULongValue);
    out.WriteIdULong("Use90Pixel", copy.ULongValue);
    copy.Block("TerrainSystem", " could not find TerrainSystem block ", " could not write TerrainSystem block ");
    copy.String("TerrainFileName", 0x4f, " could not find TerrainFileName in TerrainSystem block ",
                " could not write TerrainFileName in TerrainSystem block ");
    // Original behaviour: without a TacMapGifName the terrain file name is written under its name.
    ReadText(in, "TacMapGifName", 0x4f, copy.Text);
    out.WriteIdString("TacMapGifName", copy.Text);
    copy.Block("Script", " could not find Script Block ", " could not write Script Block ");
    copy.String("ScenarioScript", 0x4f, " could not find ScenarioScript in Script Block ",
                " could not write ScenarioScript in Script Block ");

    // The computer-controlled parts and their pilots are copied renumbered from 1 (the player's parts are left
    // out; the force is added after them).
    result = in.SeekBlock("Warriors");
    Assert(result == 0, 0, " Could not find Warriors Block ");
    uint32_t numWarriors = 0;
    check(ReadEntry(in, "NumWarriors", numWarriors), " Could not find NumWarriors in Warriors Block ");
    // Per warrior number: the part that uses it (first half) and its new number (second half, from numWarriors).
    // Port fix: the buffer is always made and cleared (with no warriors the original used the start file's name).
    std::vector<char> pilotMap(static_cast<size_t>(numWarriors) * 2 + 2, 0);
    check(in.SeekBlock("Parts") == 0, " Could not find Parts Block ");
    uint32_t numParts = 0;
    check(ReadEntry(in, "NumParts", numParts), " Could not find NumParts in Parts Block ");

    // Whether part block <paramref name="part"/> (just made current) is one of the player's.
    auto playerPart = [&] { return in.Read<bool>("PlayerPart").value_or(false); };

    for (int32_t part = 1; part < static_cast<int32_t>(numParts + 1); part++)
    {
        check(in.SeekBlock(std::format("Part{}", part)) == 0, " Could not find PartNumber Block ");

        if (!playerPart())
        {
            check(ReadEntry(in, "Pilot", copy.ULongValue), " Could not find Pilot in PartNumber Block ");

            // Port fix: a pilot number past the scenario's warriors wrote past the map.
            if (copy.ULongValue < pilotMap.size())
            {
                pilotMap[copy.ULongValue] = static_cast<char>(part);
            }
        }
    }

    int32_t nextWarrior = 1;

    for (uint32_t warrior = 1; static_cast<int32_t>(warrior) < static_cast<int32_t>(numWarriors + 1); warrior++)
    {
        if (pilotMap[warrior] == 0)
        {
            continue;
        }

        pilotMap[numWarriors + warrior] = static_cast<char>(nextWarrior);
        result = in.SeekBlock(std::format("Warrior{}", warrior));
        Assert(result == 0, warrior, " Could not find Warrior Number Block ");
        result = out.WriteBlock(std::format("Warrior{}", nextWarrior++));
        Assert(result > 0, warrior, " Could not find Warrior Number Block ");
        Assert(ReadText(in, "Profile", 99, copy.Text), 0, " Could not find Warrior Profile in Warrior Number Block ");
        Assert(out.WriteIdString("Profile", copy.Text) > 0, 0,
               " Could not write Warrior Profile in Warrior Number Block ");
        copy.String("Brain", 0x7f, " Could not find Warrior Brain in Warrior Number Block ",
                    " Could not write Warrior Brain in Warrior Number Block ");
    }

    const int32_t firstForceWarrior = nextWarrior;

    // Old part number -> new part number (-1 for the player's parts, and for the numbers past the scenario's parts).
    std::vector<int32_t> partMap(std::max<size_t>(numParts + 1, 0x40), -1);
    auto newPartNumber = [&](int32_t part)
    { return part >= 0 && static_cast<size_t>(part) < partMap.size() ? partMap[static_cast<size_t>(part)] : -1; };
    int32_t nextPart = 1;

    for (int32_t part = 1; part < static_cast<int32_t>(numParts + 1); part++)
    {
        check(in.SeekBlock(std::format("Part{}", part)) == 0, " Could not find PartNumber Block ");

        if (playerPart())
        {
            continue;
        }

        partMap[static_cast<size_t>(part)] = nextPart;
        check(out.WriteBlock(std::format("Part{}", nextPart++)) > 0, " Could not write PartNumber Block ");
        copy.ULong("ObjectNumber", " Could not find ObjectNumber in PartNumber Block ",
                   " Could not write ObjectNumber in PartNumber Block ");
        copy.ULong("ControlType", " Could not find ControlType in PartNumber Block ",
                   " Could not write ControlType in PartNumber Block ");
        copy.ULong("ControlDataType", " Could not find ControlDataType in PartNumber Block ",
                   " Could not write ControlDataType in PartNumber Block ");
        copy.String("ObjectProfile", 9, " Could not find ObjectProfile in PartNumber Block ",
                    " Could not write ObjectProfile in PartNumber Block ");
        check(ReadEntry(in, "Pilot", copy.ULongValue), " Could not find Pilot in PartNumber Block ");
        const char newPilot =
            numWarriors + copy.ULongValue < pilotMap.size() ? pilotMap[numWarriors + copy.ULongValue] : 0;
        check(out.WriteIdULong("Pilot", static_cast<uint32_t>(static_cast<int32_t>(newPilot))) > 0,
              " Could not write Pilot in PartNumber Block ");
        copy.Float("PositionX", " Could not find PositionX in PartNumber Block ",
                   " Could not write PositionX in PartNumber Block ");
        copy.Float("PositionY", " Could not find PositionY in PartNumber Block ",
                   " Could not write PositionY in PartNumber Block ");
        copy.Float("PositionZ", " Could not find PositionZ in PartNumber Block ",
                   " Could not write PositionZ in PartNumber Block ");
        copy.Float("Rotation", " Could not find Rotation in PartNumber Block ",
                   " Could not write Rotation in PartNumber Block ");
        copy.Char("TeamId", " Could not find TeamId in PartNumber Block ",
                  " Could not write TeamId in PartNumber Block ");
        copy.Char("CommanderId", " Could not find CommanderId in PartNumber Block ",
                  " Could not write CommanderId in PartNumber Block ");
        copy.ULong("Gesture", " Could not find Gesture in PartNumber Block ",
                   " Could not write Gesture in PartNumber Block ");
        copy.Float("Velocity", " Could not find Velocity in PartNumber Block ",
                   " Could not write Velocity in PartNumber Block ");
        copy.Long("Active", " Could not find Active Flag in PartNumber Block ",
                  " Could not write Active Flag in PartNumber Block ");
        copy.Long("Exists", " Could not find Exists Flag in PartNumber Block ",
                  " Could not write Exists Flag in PartNumber Block ");
        copy.Char("MyIcon", " Could not find MyIcon in PartNumber Block ",
                  " Could not write MyIcon in PartNumber Block ");
    }

    const int32_t firstForcePart = nextPart;

    if (in.SeekBlock("Elemental Carriers") == 0)
    {
        // Original behaviour: the carrier part numbers are not renumbered.
        check(out.WriteBlock("Elemental Carriers") > 0, " Could not write Elemental Carriers Block ");
        int32_t numCarriers = 0;
        check(ReadEntry(in, "Carriers", numCarriers), " Could not read carriers in elemental carriers block");
        check(out.WriteIdLong("Carriers", numCarriers) > 0, " Could not write carriers in elemental carriers block");

        for (int32_t carrier = 0; carrier < numCarriers; carrier++)
        {
            copy.Block(std::format("ECarrier{}", carrier), " Could not find carrier block",
                       " Could not write carrier block");
            int32_t carrierPart = 0;
            check(ReadEntry(in, "Carrier", carrierPart), " Could not read carrier in carrier block");
            Assert(carrierPart < firstForcePart, static_cast<uint32_t>(carrierPart),
                   "Illegal part number for elemental carrier");
            check(out.WriteIdLong("Carrier", carrierPart) > 0, " Could not write carrier in carrier block");

            for (int32_t elemental = 0; elemental < 10; elemental++)
            {
                const std::string name = std::format("Elemental{}", elemental);

                if (!ReadEntry(in, name, copy.LongValue))
                {
                    break;
                }

                check(out.WriteIdLong(name, copy.LongValue) > 0, " Could not write elemental in carrier block");
            }
        }
    }

    check(in.SeekBlock("Objectives") == 0, " Could not find Objective Block ");
    check(out.WriteBlock("Objectives") > 0, " Could not write Objective Block ");
    check(out.WriteIdLong("TimeLeft", in.Read<int32_t>("TimeLeft").value_or(-1)) > 0,
          " Could not write TimeLeft in Objective Block ");
    uint32_t numObjectives = 0;
    check(ReadEntry(in, "NumObjectives", numObjectives), " Could not find numObjectives in Objective Block ");
    check(numObjectives < 9, " Too Many Objectives ");
    check(out.WriteIdULong("NumObjectives", numObjectives) > 0, " Could not write numObjectives in Objective Block ");
    const uint32_t numInnerSphereObjectives = in.Read<uint32_t>("NumInnerSphereObjectives").value_or(0);
    const uint32_t numClanObjectives = in.Read<uint32_t>("NumClanObjectives").value_or(0);
    // Original behaviour: NumObjectives is written a second time.
    check(out.WriteIdULong("NumObjectives", numObjectives) > 0, " Could not write numObjectives in Objective Block ");

    if (numInnerSphereObjectives != 0 || numClanObjectives != 0)
    {
        check(out.WriteIdULong("NumInnerSphereObjectives", numInnerSphereObjectives) > 0,
              " Could not write numInnerSphereObjectives in Objective Block ");
        check(out.WriteIdULong("NumClanObjectives", numClanObjectives) > 0,
              " Could not write numClanObjectives in Objective Block ");
    }

    for (uint32_t objective = 0; objective < numObjectives; objective++)
    {
        const std::string block = std::format("Objective{}", objective);
        Assert(in.SeekBlock(block) == 0, objective, " Could not find ObjectiveNumber Block ");
        Assert(out.WriteBlock(block) > 0, objective, " Could not write ObjectiveNumber Block ");
        copy.String("Name", 0xff, " Could not find Name in Objective Block ",
                    " Could not write Name in Objective Block ");
        copy.ULong("Type", " Could not find Type in Objective Block ", " Could not write Type in Objective Block ");
        copy.Float("TimeLeft", " Could not find TimeLeft in Objective Block ",
                   " Could not write TimeLeft in Objective Block ");
        copy.ULong("Status", " Could not find Status in Objective Block", " Could not write Status in Objective Block");
        check(out.WriteIdLong("Points", in.Read<int32_t>("Points").value_or(0)) > 0,
              " Could not write Points in Objective Block");
        check(out.WriteIdFloat("Radius", in.Read<float>("Radius").value_or(0.0f)) > 0,
              " Could not write Radius in Objective Block");
    }

    check(in.SeekBlock("Teams") == 0, " Could not find Teams Block in Scenario ");
    check(out.WriteBlock("Teams") > 0, " Could not write Teams Block");
    bool alliedTeam = false;
    check(ReadEntry(in, "AlliedTeam", alliedTeam), " Could not find Allied Team flag in Scenario ");
    check(out.WriteIdBoolean("AlliedTeam", alliedTeam) > 0, " Could not write AlliedTeam Flag");
    std::array<int32_t, 12> mates{};

    // The computer's commanders' groups, with their first five members renumbered.
    auto copyGroups = [&](std::string_view prefix, std::string_view writeMessage, std::string_view findMatesMessage,
                          std::string_view writeMatesMessage)
    {
        for (int32_t group = 0;; group++)
        {
            const std::string block = std::format("{}{}", prefix, group);

            if (in.SeekBlock(block) != 0)
            {
                break;
            }

            check(out.WriteBlock(block) > 0, writeMessage);
            mates.fill(0);
            check(in.ReadArray("Mates", std::span<int32_t>(mates)).has_value(), findMatesMessage);

            for (size_t mate = 0; mate < 5; mate++)
            {
                if (mates[mate] > 0)
                {
                    mates[mate] = newPartNumber(mates[mate]);
                }
            }

            check(out.WriteIdLongArray("Mates", mates) > 0, writeMatesMessage);
        }
    };

    copyGroups("Commander1Group:", " could not write Commander1Groupx in Scenario File ",
               " could not find Mates in ClanTeam in Scenario File ",
               " could not write Mates in ClanTeam in Scenario File ");
    copyGroups("Commander2Group:", " could not write Commander2Groupx in Scenario File ",
               " could not find Mates in AlliedTeam in Scenario File ",
               " could not write Mates in AlliedTeam in Scenario File ");

    // The "Raven system": the mission's own inactive or capturable player-side parts (at most four) join the force
    // as salvage (not the player's yet) in a free lance.
    check(in.SeekBlock("Parts") == 0, " Could not find Parts Block ");
    check(ReadEntry(in, "NumParts", copy.ULongValue), "Could not read Num Parts ");
    std::array<bool, MaxRavenParts> ravenIsMech{};
    std::array<MCPartPlace, MaxRavenParts> ravenMechPlaces{};
    std::array<MCPartPlace, MaxRavenParts> ravenVehiclePlaces{};
    int32_t numRaven = 0;
    int32_t numRavenMechs = 0;
    int32_t numRavenVehicles = 0;
    auto readPlace = [&](MCPartPlace& place)
    {
        check(ReadEntry(in, "PositionX", place.X), " Could not find Raven Part Position ");
        check(ReadEntry(in, "PositionY", place.Y), " Could not find Raven Part Position ");
        check(ReadEntry(in, "Rotation", place.Rotation), " Could not find Raven Part Rotation ");
    };

    for (int32_t part = 1; part <= static_cast<int32_t>(copy.ULongValue); part++)
    {
        check(in.SeekBlock(std::format("Part{}", part)) == 0, " Could not locate part block");
        char teamID = 0;
        check(ReadEntry(in, "TeamId", teamID), "Could not read alignment");

        if (teamID != 0)
        {
            continue;
        }

        int32_t active = 0;
        check(ReadEntry(in, "Active", active), " Could not read Active ");

        if (active != 0 && !in.Read<bool>("Capturable").value_or(false))
        {
            continue;
        }

        // Port fix: a fifth such part was written past the tables after the assert; it is left out.
        Assert(numRaven < MaxRavenParts, static_cast<uint32_t>(numRaven + 1),
               " Too Many Inactive parts.  Only allowed 4!! ");

        if (numRaven >= MaxRavenParts)
        {
            continue;
        }

        numRaven++;
        check(ReadText(in, "ObjectProfile", 9, copy.Text), " Could not find ObjectProfile in PartNumber Block ");
        const std::string profile = copy.Text;

        if (!profile.contains('v') && !profile.contains('V'))
        {
            // A mech, with its pilot.
            readPlace(ravenMechPlaces[static_cast<size_t>(numRavenMechs)]);
            ForceMechList->AddMech(profile, false, false, true);
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(0, mech);
            mech->Assigned = true;
            mech->Deployed = true;
            mech->NotMineYet = true;
            mech->ProfileName = profile.substr(0, 9);
            ravenIsMech[static_cast<size_t>(numRaven - 1)] = true;
            numRavenMechs++;
            uint32_t pilot = 0;
            check(ReadEntry(in, "Pilot", pilot), " No pilot for this part ");
            check(in.SeekBlock(std::format("Warrior{}", pilot)) == 0, " could not find pilot block for Raven System ");
            std::string pilotProfile;
            check(ReadText(in, "Profile", 0x31, pilotProfile), " could not find pilot profile for Raven System ");
            AssignedWarriorList->AddWarrior(pilotProfile, false);
            MCLogWarrior* warrior = nullptr;
            AssignedWarriorList->GetWarriorInfo(0, warrior);
            warrior->NotMineYet = true;
        }
        else
        {
            readPlace(ravenVehiclePlaces[static_cast<size_t>(numRavenVehicles)]);
            ForceVehicleList->AddVehicle(profile, false, false, true);
            MCLogVehicle* vehicle = nullptr;
            ForceVehicleList->GetVehicleInfo(0, vehicle);
            vehicle->Assigned = true;
            vehicle->Deployed = true;
            vehicle->NotMineYet = true;
            vehicle->ProfileName = profile.substr(0, 9);
            numRavenVehicles++;
        }
    }

    // The salvaged mechs take the first pilot indexes (their pilots went to the head of the list).
    // Original behaviour: the mech is looked up by its entry among the salvaged parts, not among the mechs.
    int32_t pilotIndex = numRavenMechs - 1;

    for (int32_t entry = numRaven - 1; entry >= 0; entry--)
    {
        if (ravenIsMech[static_cast<size_t>(entry)])
        {
            MCLogMech* mech = nullptr;
            ForceMechList->GetMechInfo(entry, mech);
            mech->PilotIndex = pilotIndex--;
        }
    }

    // They go in the third lance when it is empty.
    auto lanceUsed = [&](size_t lance)
    {
        int32_t sum = 0;

        for (const DeploySlot& slot : DeploySlots[lance])
        {
            sum += slot.Unit + slot.Vehicle;
        }

        return sum != -8;
    };

    const int32_t ravenLance = lanceUsed(2) ? -1 : 2;

    if (numRaven > 0)
    {
        Assert(ravenLance != -1, 0xffffffff, " No open lance for Raven System Vehicles/Mechs ");
    }

    // Port fix: with no open lance the original wrote the salvage's slots before the drop zone's.
    if (ravenLance >= 0)
    {
        int32_t mechIndex = numRaven - numRavenVehicles - 1;
        int32_t vehicleIndex = numRaven - numRavenMechs - 1;

        for (int32_t entry = numRaven - 1; entry >= 0; entry--)
        {
            DeploySlot& slot = DeploySlots[static_cast<size_t>(ravenLance)][static_cast<size_t>(entry)];

            if (!ravenIsMech[static_cast<size_t>(entry)])
            {
                slot.Vehicle = vehicleIndex--;
            }
            else
            {
                slot.Unit = mechIndex--;
            }
        }
    }

    // The force's pilots (vehicle crews from their profiles). The salvage went to the head of the lists, so the
    // other lances' indexes are past it.
    int32_t warriorNumber = firstForceWarrior;

    for (size_t lance = 0; lance < NumLances; lance++)
    {
        const bool raven = static_cast<int32_t>(lance) == ravenLance;

        for (const DeploySlot& deploy : DeploySlots[lance])
        {
            if (deploy.Unit < 0)
            {
                if (deploy.Vehicle < 0)
                {
                    continue;
                }

                Assert(out.WriteBlock(std::format("Warrior{}", warriorNumber++)) > 0, static_cast<uint32_t>(lance),
                       " Could not write Warrior Number Block ");
                MCLogVehicle* vehicle = nullptr;
                ForceVehicleList->GetVehicleInfo(deploy.Vehicle + (raven ? 0 : numRavenVehicles), vehicle);
                Assert(out.WriteIdString("Profile", vehicle->Crew) > 0, 0,
                       " Could not write Warrior Profile in Warrior Number Block ");
                MCFitIniFile crewFile;
                check(crewFile.Open(GamePath(WarriorPath, vehicle->Crew, ".fit")) == 0,
                      " Could not open vehicle profile");
                check(crewFile.SeekBlock("General") == 0, " Could not find General block in vehicle crew profile");
                check(ReadText(crewFile, "Brain", 0xff, copy.Text), " Could not read brain in vehicle crew profile");
                check(out.WriteIdString("Brain", copy.Text) > 0,
                      " Could not write Warrior Brain in Warrior Number Block ");
                crewFile.Close();
                continue;
            }

            Assert(out.WriteBlock(std::format("Warrior{}", warriorNumber++)) > 0, static_cast<uint32_t>(lance),
                   " Could not write Warrior Number Block ");
            const int32_t offset = raven ? 0 : numRavenMechs;
            const int32_t mech = deploy.Unit + offset;
            // The pilot's id, then the profile and brain of the pilot with that id (left as they were when none has).
            const int32_t id = AssignedWarriorList->GetID(ForceMechList->GetMechPilotIndex(mech) + offset);
            AssignedWarriorList->GetWarriorProfile(static_cast<uint32_t>(id), copy.Text);
            Assert(out.WriteIdString("Profile", copy.Text) > 0, 0,
                   " Could not write Warrior Profile in Warrior Number Block ");
            AssignedWarriorList->GetWarriorBrain(static_cast<uint32_t>(id), copy.Text);
            check(out.WriteIdString("Brain", copy.Text) > 0, " Could not write Warrior Brain in Warrior Number Block ");
            MCLogWarrior* warrior = nullptr;
            AssignedWarriorList->GetWarriorInfo(ForceMechList->GetMechPilotIndex(mech), warrior);
            out.WriteIdBoolean("NotMineYet", warrior->NotMineYet);
        }
    }

    // The force's parts: at the drop zone's slot offsets (the salvage where it stands, inactive).
    int32_t pilotNumber = 0;
    int32_t partNumber = firstForcePart;

    // The force's part placed in a drop slot (the salvage's lance counts from the head of the lists).
    auto deployedPart = [&](size_t lance, const DeploySlot& deploy) -> MCLogPart*
    {
        const bool raven = static_cast<int32_t>(lance) == ravenLance;

        if (deploy.Unit < 0)
        {
            MCLogVehicle* vehicle = nullptr;
            ForceVehicleList->GetVehicleInfo(deploy.Vehicle + (raven ? 0 : numRavenVehicles), vehicle);
            return vehicle;
        }

        MCLogMech* mech = nullptr;
        ForceMechList->GetMechInfo(deploy.Unit + (raven ? 0 : numRavenMechs), mech);
        return mech;
    };

    for (size_t lance = 0; lance < NumLances; lance++)
    {
        for (size_t slot = 0; slot < LanceSlots; slot++)
        {
            const DeploySlot& deploy = DeploySlots[lance][slot];

            if (deploy.Unit < 0 && deploy.Vehicle < 0)
            {
                continue;
            }

            check(out.WriteBlock(std::format("Part{}", partNumber)) > 0, " Could not write PartNumber Block ");
            check(out.WriteIdULong("ControlType", 2) > 0, " Could not write ControlType in PartNumber Block ");
            MCLogPart* part = deployedPart(lance, deploy);
            Assert(part != nullptr, 0,
                   deploy.Unit < 0 ? " Could not get vehicle pointer " : " Could not get mech pointer ");
            check(out.WriteIdULong("ControlDataType", deploy.Unit < 0 ? 2 : 1) > 0,
                  " Could not write ControlDataType in PartNumber Block ");
            // The part remembers its number (for the team's Mates below).
            part->PartNumber = partNumber++;
            check(out.WriteIdULong("ObjectNumber", part->Chassis) > 0,
                  " Could not write ObjectNumber in PartNumber Block ");
            check(out.WriteIdString("ObjectProfile", part->ProfileName) > 0,
                  " Could not write ObjectProfile in PartNumber Block ");
            check(out.WriteIdChar("TeamId", 0) > 0, " Could not write TeamId in PartNumber Block ");
            check(out.WriteIdChar("CommanderId", 0) > 0, " Could not write CommanderId in PartNumber Block ");
            check(out.WriteIdULong("Pilot", static_cast<uint32_t>(pilotNumber + firstForceWarrior)) > 0,
                  " Could not write Pilot in PartNumber Block ");
            const bool raven = static_cast<int32_t>(lance) == ravenLance;
            MCPartPlace place;

            if (raven)
            {
                place = deploy.Unit < 0 ? ravenVehiclePlaces[static_cast<size_t>(deploy.Vehicle)]
                                        : ravenMechPlaces[static_cast<size_t>(deploy.Unit)];
            }
            else
            {
                const DeploySlotInfo& info = DeploySlotPlacements[lance][slot];
                place = {info.OffsetX + DropZonePositions[lance].X, info.OffsetY + DropZonePositions[lance].Y,
                         info.Rotation};
            }

            check(out.WriteIdFloat("PositionX", place.X) > 0, " Could not write PositionX in PartNumber Block ");
            check(out.WriteIdFloat("PositionY", place.Y) > 0, " Could not write PositionY in PartNumber Block ");
            check(out.WriteIdFloat("PositionZ", -1.0f) > 0, " Could not write PositionZ in PartNumber Block ");
            check(out.WriteIdFloat("Rotation", place.Rotation) > 0, " Could not write Rotation in PartNumber Block ");
            check(out.WriteIdULong("Gesture", 2) > 0, " Could not write Gesture in PartNumber Block ");
            check(out.WriteIdFloat("Velocity", 0.0f) > 0, " Could not write Velocity in PartNumber Block ");
            check(out.WriteIdLong("Active", raven ? 0 : 1) > 0, " Could not write Active Flag in PartNumber Block ");
            check(out.WriteIdLong("Exists", 1) > 0, " Could not write Exists Flag in PartNumber Block ");
            check(out.WriteIdChar("MyIcon", 0) > 0, " Could not write MyIcon in PartNumber Block ");
            pilotNumber++;
        }
    }

    Assert(in.SeekBlock("Warriors") == 0, 0, " Could not find Warriors Block ");
    Assert(out.WriteBlock("Warriors") > 0, 0, " Could not write Warriors Block ");
    uint8_t captureChance = 0;
    Assert(ReadEntry(in, "CaptureChance", captureChance), 0, " Could not read captureChance in Warriors Block ");
    Assert(out.WriteIdUChar("CaptureChance", captureChance) > 0, 0,
           " Could not write captureChance in Warriors Block ");
    check(out.WriteIdULong("NumWarriors", static_cast<uint32_t>(warriorNumber - 1)) > 0,
          " Could not write NumWarriors in Warriors Block ");

    if (ReadText(in, "BrainParameterFile", 0xff, copy.Text))
    {
        check(out.WriteIdString("BrainParameterFile", copy.Text) > 0,
              " could not write BrainParameterFile in Warriors Block ");
    }

    check(out.WriteBlock("Parts") > 0, " Could not write Parts Block ");
    check(out.WriteIdULong("NumParts", static_cast<uint32_t>(partNumber - 1)) > 0,
          " Could not write NumParts in Parts Block ");

    // The player's commander groups: one per lance in use, from the first one.
    size_t firstLance = 0;

    while (firstLance < NumLances && !lanceUsed(firstLance))
    {
        firstLance++;
    }

    Assert(firstLance < NumLances, static_cast<uint32_t>(firstLance), " No Assigned Mechs for this Mission ");
    int32_t groupNumber = 0;

    for (size_t lance = firstLance; lance < NumLances; lance++)
    {
        if (!lanceUsed(lance))
        {
            continue;
        }

        Assert(out.WriteBlock(std::format("Commander0Group:{}", groupNumber)) > 0, static_cast<uint32_t>(lance),
               " could not write Commander0Groupx Team Block ");
        mates.fill(0);
        size_t numMates = 0;

        for (const DeploySlot& deploy : DeploySlots[lance])
        {
            if (deploy.Unit < 0 && deploy.Vehicle < 0)
            {
                continue;
            }

            if (const MCLogPart* part = deployedPart(lance, deploy); part != nullptr)
            {
                mates[numMates++] = part->PartNumber;
            }
        }

        check(out.WriteIdLongArray("Mates", mates) > 0, " could not write Mates in Inner Sphere Team Block ");
        groupNumber++;
    }

    if (in.SeekBlock("Trains") == 0)
    {
        int32_t numTrains = 0;
        check(ReadEntry(in, "NumTrains", numTrains), " Could not read numTrains ");
        out.WriteBlock("Trains");
        out.WriteIdLong("NumTrains", numTrains);

        for (int32_t train = 0; train < numTrains; train++)
        {
            const std::string block = std::format("Train{}", train);
            check(in.SeekBlock(block) == 0, " could not find trainBlock ");
            int32_t numCars = 0;
            check(ReadEntry(in, "NumCars", numCars), " could not find numCars ");
            out.WriteBlock(block);
            out.WriteIdLong("NumCars", numCars);

            for (int32_t car = 0; car < numCars; car++)
            {
                const std::string carName = std::format("Car{}", car);
                int32_t carPart = 0;
                check(ReadEntry(in, carName, carPart), " could not find carBlock ");
                out.WriteIdLong(carName, carPart);
            }
        }
    }

    in.Close();
    out.Close();

    // And the logistics state the mission reads back: start<n>.fit for the next mission.
    result = MCMissionLogisticsBridge::LogisticsStartingFitWriter(std::format("start{}", CurrentMission + 1), false);
    Assert(result == 0, 0, " Could not save logistics data ");
    return 0;
}
