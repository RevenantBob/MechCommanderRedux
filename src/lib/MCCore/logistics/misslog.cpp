#include "stdafx.h"
#include "logistics/misslog.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "object/MCMasterComponent.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/MCForces.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"
#include "platform/MCFileSystem.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"

char* ComponentComment[NUM_FIT_COMPONENTS] = {
    const_cast<char*>("// Medium Pulse Laser (IS) "),
    const_cast<char*>("// Medium Pulse Laser (Clan)"),
    const_cast<char*>("// Short-Range Missile/2 (IS) "),
    const_cast<char*>("// Short-Range Missile/2 (CLAN)"),
    const_cast<char*>("// Medium Laser (IS)"),
    const_cast<char*>("// Streak Short-Range Missile/2 (IS) "),
    const_cast<char*>("// Streak Short-Range Missile/2 (Clan)"),
    const_cast<char*>("// Flamer (IS) "),
    const_cast<char*>("// Flamer (Clan)"),
    const_cast<char*>("// Heavy Autocannon (IS) "),
    const_cast<char*>("// Heavy Ultra AC (Clan)"),
    const_cast<char*>("// Large Pulse Laser (IS) "),
    const_cast<char*>("// Medium ER Laser (Clan)"),
    const_cast<char*>("// Large Pulse Laser (Clan)"),
    const_cast<char*>("// Large Laser (IS)"),
    const_cast<char*>("// Medium Autocannon (IS) "),
    const_cast<char*>("// Medium Ultra AC (Clan)"),
    const_cast<char*>("// Particle Projector Cannon (IS) "),
    const_cast<char*>("// Long-Range Missile/5 (IS) "),
    const_cast<char*>("// Long-Range Missile/5 (CLAN)"),
    const_cast<char*>("// Light Autocannon (IS) "),
    const_cast<char*>("// Light Ultra Autocannon (IS) "),
    const_cast<char*>("// Light Ultra AC (Clan)"),
    const_cast<char*>("// Large ER Laser (IS) "),
    const_cast<char*>("// Large ER Laser (Clan)"),
    const_cast<char*>("// ER Particle Projector Cannon (IS) "),
    const_cast<char*>("// Gauss Rifle (IS) "),
    const_cast<char*>("// Gauss Rifle (Clan)"),
    const_cast<char*>("// ER Particle Projector (Clan)"),
    const_cast<char*>("// Sensor (Basic) (IS) "),
    const_cast<char*>("// Sensor (Basic) (Clan)"),
    const_cast<char*>("// Sensor (Intermediate) (IS) "),
    const_cast<char*>("// Sensor (Advanced) (IS) "),
    const_cast<char*>("// Sensor (Advanced) (Clan)"),
    const_cast<char*>("// Beagle Probe (IS) "),
    const_cast<char*>("// Guardian ECM (IS) "),
    const_cast<char*>("// ECM Suite (Clan) "),
    const_cast<char*>("// Active Probe (Clan) "),
    const_cast<char*>("// Heavy Gauss Cannon (IS) "),
    const_cast<char*>("// Light Gauss Cannon (IS) "),
    const_cast<char*>("// Light LBX AutoCannon (IS) "),
    const_cast<char*>("// Medium LBX AutoCannon (IS) "),
    const_cast<char*>("// Heavy LBX AutoCannon (IS) "),
    const_cast<char*>("// Light LBX AutoCannon (Clan) "),
    const_cast<char*>("// Medium LBX AutoCannon (Clan) "),
    const_cast<char*>("// Heavy LBX AutoCannon (Clan) "),
    const_cast<char*>("// Heavy Thunderbolt "),
    const_cast<char*>("// Large X Pulse Laser "),
    const_cast<char*>("// Long Tom "),
    const_cast<char*>("// Sniper Cannon "),
};

uint8_t ComponentId[NUM_FIT_COMPONENTS] = {
    0x90, 0x99, 0x7b, 0x85, 0x8f, 0x7d, 0x87, 0x93, 0x9b, 0x66, 0x70, 0x8e, 0x98, 0x97, 0x8c, 0x65, 0x6f,
    0x91, 0x78, 0x82, 0x64, 0x67, 0x6e, 0x8d, 0x96, 0x92, 0x68, 0x71, 0x9a, 0x0d, 0x10, 0x0e, 0x0f, 0x11,
    0x25, 0x26, 0x2a, 0x2b, 0x62, 0x63, 0x6b, 0x6c, 0x6d, 0x74, 0x75, 0x76, 0x7e, 0x8b, 0xa0, 0xa1,
};

float TotalScenarioTime = 0.0f;
float TotalLogisticsTime = 0.0f;

namespace
{
    /// <summary>The block names of a mech's eight body locations, in location order.</summary>
    constexpr const char* MechLocationNames[8] = {"Head",    "CenterTorso", "LeftTorso", "RightTorso",
                                                  "LeftArm", "RightArm",    "LeftLeg",   "RightLeg"};

    /// <summary>The names of a mech's eleven armor facings, in armor order.</summary>
    constexpr const char* MechArmorNames[11] = {
        "Head",    "CenterTorso", "LeftTorso",       "RightTorso",    "LeftArm",       "RightArm",
        "LeftLeg", "RightLeg",    "RearCenterTorso", "RearLeftTorso", "RearRightTorso"};

    /// <summary>The block names of a vehicle's five locations.</summary>
    constexpr const char* VehicleLocationNames[5] = {"Front", "Left", "Right", "Rear", "Turret"};

    /// <summary>The master component forms a profile lists as weapons (FacesForward); form 10 is ammo.</summary>
    bool IsWeaponForm(MCComponentForm form)
    {
        return form == MCComponentForm::Weapon || form == MCComponentForm::WeaponEnergy ||
               form == MCComponentForm::WeaponBallistic || form == MCComponentForm::WeaponMissile;
    }

    /// <summary>The CRT's <c>DeleteFileA</c> on a game path.</summary>
    void DeleteFile(std::string_view fileName)
    {
        MCFileSystem::RemoveFile(fileName);
    }

    /// <summary>The name of profile <paramref name="index"/> ("tpak<i>n</i>").</summary>
    void ProfileName(char* name, size_t size, int32_t index)
    {
        std::snprintf(name, size, "tpak%d", index);
    }

    /// <summary>Starts block <paramref name="format"/> <paramref name="index"/> with its PacketNum.</summary>
    void WritePacketBlock(MCFitIniFile& file, const char* format, int32_t index, uint32_t packetNum)
    {
        char block[32];
        std::snprintf(block, sizeof(block), format, index);
        file.WriteBlock(block);
        file.WriteIdULong("PacketNum", packetNum);
    }

    /// <summary>Writes the 50 "Componant" blocks: each one's comment line, id and the count from <paramref name="countOf"/>.</summary>
    template <typename CountFn> void WriteComponents(MCFitIniFile& file, CountFn countOf)
    {
        file.WriteBlock("Components");
        file.WriteIdULong("NumComponents", NUM_FIT_COMPONENTS);

        for (int32_t i = 0; i < NUM_FIT_COMPONENTS; i++)
        {
            char block[32];
            std::snprintf(block, sizeof(block), "Componant%d", i);
            file.WriteBlock(block);
            file.WriteLine(ComponentComment[i]);
            file.WriteIdUChar("ComponantId", ComponentId[i]);
            file.WriteIdLong("NumAvailable", countOf(ComponentId[i]));
        }
    }

    /// <summary>
    /// Copies file <paramref name="fileName"/> into packet <paramref name="packet"/> of <paramref name="packFile"/>.
    /// Returns the open error, if any.
    /// </summary>
    int32_t PackFileInto(MCPacketFile& packFile, MCFile& file, std::string_view fileName, int32_t packet)
    {
        const int32_t result = file.Open(fileName);

        if (result != 0)
        {
            return result;
        }

        const uint32_t size = file.FileSize();
        std::vector<uint8_t> buffer(size);
        file.Read(buffer.data(), static_cast<int32_t>(size));
        packFile.WritePacket(packet, buffer, MCPacketStorage::Lzd);
        file.Close();
        return 0;
    }

    /// <summary>Packs profile files tpak0 .. tpak<paramref name="count"/>-1 into packets 1 .. <paramref name="count"/>.</summary>
    int32_t PackProfiles(MCPacketFile& packFile, MCFile& file, int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            char name[32];
            ProfileName(name, sizeof(name), i);
            std::string fileName;
            fileName = GamePath(SaveTempPath, name, ".fit");
            const int32_t result = PackFileInto(packFile, file, fileName, i + 1);

            if (result != 0)
            {
                return result;
            }
        }

        return 0;
    }

    /// <summary>Deletes profile files tpak0 .. tpak<paramref name="count"/>-1.</summary>
    void DeleteProfiles(int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            char name[32];
            ProfileName(name, sizeof(name), i);
            std::string fileName;
            fileName = GamePath(SaveTempPath, name, ".fit");
            DeleteFile(fileName);
        }
    }

    /// <summary>The warrior at <paramref name="index"/> of <paramref name="list"/> (walked from the head).</summary>
    MCLogWarrior* WarriorAt(MCLogWarriorList* list, int32_t index)
    {
        MCLogWarrior* warrior = list->Warriors;

        for (int32_t i = index; i > 0; i--)
        {
            warrior = warrior->Next;
        }

        return warrior;
    }

    /// <summary>How many warriors of <paramref name="list"/> are (or are not) assigned.</summary>
    uint32_t CountWarriors(MCLogWarriorList* list, bool assigned)
    {
        uint32_t count = 0;
        MCLogWarrior* warrior = list->Warriors;

        for (int32_t i = list->NumWarriors; i > 0; i--)
        {
            if ((warrior->Assigned != 0) == assigned)
            {
                ++count;
            }

            warrior = warrior->Next;
        }

        return count;
    }

    /// <summary>
    /// A purchasable pilot's Status in the purchase file: 3 when a pilot of the player's with the same description
    /// was sold, 1 when one is alive, 2 when one is dead; otherwise the pilot's own status. The player's unassigned
    /// pilots are searched first, then the assigned ones; the last match of a list wins.
    /// </summary>
    int32_t PurchasePilotStatus(MCLogistics* logistics, const MCPurPilotData* pilot)
    {
        auto search = [pilot](MCLogWarriorList* list)
        {
            int32_t status = -1;

            for (int32_t i = 0; i < list->NumWarriors; i++)
            {
                MCLogWarrior* warrior = nullptr;
                list->GetWarriorInfo(i, warrior);

                if (warrior->DescIndex != pilot->DescIndex)
                {
                    continue;
                }

                if (warrior->Sold != 0)
                {
                    status = 3;
                }
                else
                {
                    status = warrior->Health > 0.0f ? 1 : 2;
                }
            }

            return status;
        };

        int32_t status = search(logistics->WarriorList);

        if (status == -1)
        {
            status = search(logistics->AssignedWarriorList);
        }

        if (status == -1)
        {
            status = pilot->Status;
        }

        return status;
    }

    /// <summary>Writes the purchase file: what the shop has left of each mech, vehicle, component and pilot.</summary>
    int32_t WritePurchaseFile(const char* fileName)
    {
        MCLogistics* logistics = Mission()->Logistics.get();
        MCFitIniFile file;
        const int32_t result = file.Create(fileName);

        if (result != 0)
        {
            return result;
        }

        file.WriteBlock("Header");
        file.WriteIdLong("NumGifts", 0);
        file.WriteIdLong("NumMechs", logistics->PurMechList->Count);
        file.WriteIdLong("NumVehicles", logistics->PurVehicleList->Count);
        file.WriteIdLong("NumComponants", logistics->PurchaseComponents->NumItems);
        file.WriteIdLong("NumWarriors", logistics->PurPilotList->Count);
        char block[32];

        for (int32_t i = 0; i < logistics->PurMechList->Count; i++)
        {
            std::snprintf(block, sizeof(block), "Mech%d", i);
            file.WriteBlock(block);
            MCPurMech* mech = nullptr;
            logistics->PurMechList->GetMechInfo(i, mech);
            file.WriteIdLong("TypeAAvailable", mech->Variants[0]->NumAvailable);
            file.WriteIdLong("TypeWAvailable", mech->Variants[1]->NumAvailable);
            file.WriteIdLong("TypeJAvailable", mech->Variants[2]->NumAvailable);
            file.WriteIdString("TypeAFile", mech->Variants[0]->FileName);
            file.WriteIdString("TypeWFile", mech->Variants[1]->FileName);
            file.WriteIdString("TypeJFile", mech->Variants[2]->FileName);
        }

        for (int32_t i = 0; i < logistics->PurVehicleList->Count; i++)
        {
            std::snprintf(block, sizeof(block), "Vehicle%d", i);
            file.WriteBlock(block);
            MCPurVehicle* vehicle = nullptr;
            logistics->PurVehicleList->GetVehicleInfo(i, vehicle);
            file.WriteIdLong("NumAvailable", vehicle->Data->NumAvailable);
            file.WriteIdString("Filename", vehicle->Data->FileName);
        }

        for (int32_t i = 0; i < logistics->PurchaseComponents->NumItems; i++)
        {
            std::snprintf(block, sizeof(block), "Componant%d", i);
            file.WriteBlock(block);
            MCLogInventoryItem* item = logistics->PurchaseComponents->GetItemInfo(i);
            file.WriteIdUChar("ComponantID", item->MasterID);
            file.WriteIdLong("NumAvailable", item->Count);
        }

        for (int32_t i = 0; i < logistics->PurPilotList->Count; i++)
        {
            std::snprintf(block, sizeof(block), "Warrior%d", i);
            file.WriteBlock(block);
            MCPurPilotData* pilot = nullptr;
            logistics->PurPilotList->GetPilotInfo(i, pilot);
            file.WriteIdString("Profile", pilot->FileName);
            file.WriteIdLong("Status", PurchasePilotStatus(logistics, pilot));
        }

        file.Close();
        return 0;
    }

    /// <summary>
    /// Writes the pilots of the player's list: first the unassigned ones as "Warriors", then the assigned ones as
    /// "AssWarriors", each list walked from its last pilot to its first. Returns the first error.
    /// </summary>
    int32_t WriteWarriors(MCMissionLogisticsBridge* bridge, MCFitIniFile& file, uint32_t& numWarriors,
                          uint32_t& numAssigned)
    {
        MCLogistics* logistics = Mission()->Logistics.get();
        file.WriteBlock("Warriors");
        numWarriors = CountWarriors(logistics->WarriorList, false);
        file.WriteIdULong("NumWarriors", numWarriors);
        uint32_t packet = 0;

        for (int32_t i = logistics->WarriorList->NumWarriors - 1; i >= 0; i--)
        {
            MCLogWarrior* warrior = WarriorAt(logistics->WarriorList, i);

            if (warrior->Assigned != 0)
            {
                continue;
            }

            WritePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
            char name[32];
            ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
            const int32_t result = bridge->LogisticsWarriorProfileWriter(name, warrior);

            if (result != 0)
            {
                return result;
            }

            ++packet;
        }

        file.WriteBlock("AssWarriors");
        numAssigned = CountWarriors(logistics->AssignedWarriorList, true);
        file.WriteIdULong("NumAssWarriors", numAssigned);
        packet = numWarriors;

        for (int32_t i = logistics->AssignedWarriorList->NumWarriors - 1; i >= 0; i--)
        {
            MCLogWarrior* warrior = WarriorAt(logistics->AssignedWarriorList, i);

            if (warrior->Assigned == 0)
            {
                continue;
            }

            WritePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
            char name[32];
            ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
            const int32_t result = bridge->LogisticsWarriorProfileWriter(name, warrior);

            if (result != 0)
            {
                return result;
            }

            ++packet;
        }

        return 0;
    }

    /// <summary>The scenario's warrior <paramref name="index"/> (1-based; null out of range).</summary>
    MCMechWarrior* ScenarioWarrior(uint32_t index)
    {
        if (static_cast<int32_t>(index) < 1 || Scenario()->NumWarriors() < index)
        {
            return nullptr;
        }

        return Scenario()->Warrior(index);
    }

    /// <summary>
    /// Whether a mission unit goes back to logistics: a player mech still under player control, not one that
    /// only joins on a win (NotMineYet) unless the mission was won.
    /// </summary>
    bool ReturnsFromMission(MCObjectClass objectClass, int32_t alignment, int32_t netPlayerId, int notMineYet)
    {
        return objectClass == MCObjectClass::BattleMech && alignment == HomeTeam()->Alignment && netPlayerId != -1 &&
               (notMineYet == 0 || ScenarioResult > 3);
    }
}

void DestroyAllFitFiles(char* path)
{
    char pattern[0x1000];
    std::snprintf(pattern, sizeof(pattern), "%s*.fit", path);

    for (const std::string& name : MCFileSystem::FindFiles(pattern))
    {
        char fileName[0x1000];
        std::snprintf(fileName, sizeof(fileName), "%s%s", path, name.c_str());
        DeleteFile(fileName);
    }
}

auto MCMissionLogisticsBridge::MissionResultsStartingFitWriter(char* fileName) -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Planet");
    file.WriteIdLong("Setting", CurPlanet);
    file.WriteBlock("General");
    file.WriteIdString("PurchaseFile", CurPlanet == 0 ? "purchase" : "xpur");
    file.WriteBlock("ResourcePoints");
    file.WriteIdULong("numPoints", static_cast<uint32_t>(Scenario()->CalcResourcePointsEarned()));

    // The surviving pilots of the player's mechs.
    file.WriteBlock("Warriors");
    auto warriorReturns = [](MCMechWarrior* warrior)
    {
        auto* vehicle = static_cast<MCMover*>(warrior->Vehicle);
        return ReturnsFromMission(vehicle->ObjectClass, warrior->Alignment, vehicle->NetPlayerId, warrior->NotMineYet);
    };

    uint32_t numWarriors = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) <= static_cast<int32_t>(Scenario()->NumWarriors()); i++)
    {
        if (warriorReturns(ScenarioWarrior(i)))
        {
            ++numWarriors;
        }
    }

    file.WriteIdULong("NumWarriors", numWarriors);
    uint32_t packet = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) <= static_cast<int32_t>(Scenario()->NumWarriors()); i++)
    {
        MCMechWarrior* warrior = ScenarioWarrior(i);

        if (!warriorReturns(warrior))
        {
            continue;
        }

        WritePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = MissionResultsWarriorProfileWriter(name, warrior);

        if (result != 0)
        {
            return result;
        }

        ++packet;
    }

    // The player's mechs, then the salvaged ones.
    file.WriteBlock("Mechs");
    auto mechReturns = [](MCBaseObject* object)
    {
        auto* mech = static_cast<MCBattleMech*>(object);
        return object->ObjectClass == MCObjectClass::BattleMech &&
               ReturnsFromMission(object->ObjectClass, mech->GetAlignment(), mech->NetPlayerId, mech->NotMineYet);
    };

    MCTacticalMap* tacMap = TacticalMap();
    uint32_t numMechs = 0;

    for (MCBaseObject* object : *InnerSphereMechList())
    {
        if (mechReturns(object))
        {
            ++numMechs;
        }
    }

    for (size_t i = 0; i < tacMap->Salvage.size(); i++)
    {
        if (tacMap->Salvage[i] != nullptr && tacMap->Salvage[i]->ObjectClass == MCObjectClass::BattleMech)
        {
            ++numMechs;
        }
    }

    file.WriteIdULong("NumMechs", numMechs);
    int32_t mechIndex = 0;
    packet = numWarriors;

    for (MCBaseObject* object : *InnerSphereMechList())
    {
        if (!mechReturns(object))
        {
            continue;
        }

        WritePacketBlock(file, "Mech%d", mechIndex, packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = MissionResultsMechProfileWriter(name, static_cast<MCBattleMech*>(object), 0);

        if (result != 0)
        {
            return result;
        }

        ++mechIndex;
        ++packet;
    }

    for (size_t i = 0; i < tacMap->Salvage.size(); i++)
    {
        MCGameObject* salvage = tacMap->Salvage[i];

        if (salvage == nullptr || salvage->ObjectClass != MCObjectClass::BattleMech)
        {
            continue;
        }

        WritePacketBlock(file, "Mech%d", mechIndex, packet);
        auto* mech = static_cast<MCBattleMech*>(salvage);
        mech->NotMineYet = 0;
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = MissionResultsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++mechIndex;
        ++packet;
    }

    file.WriteIdULong("NumVehicles", 0);

    // The components salvaged on the map.
    WriteComponents(file,
                    [tacMap](uint8_t id)
                    {
                        int32_t count = 0;

                        for (size_t i = 0; i < tacMap->Salvage.size(); i++)
                        {
                            MCGameObject* salvage = tacMap->Salvage[i];

                            if (salvage == nullptr)
                            {
                                continue;
                            }

                            for (const MCSalvageItem& item : salvage->GetSalvage())
                            {
                                if (item.ItemId == id)
                                {
                                    count += item.NumItems;
                                }
                            }
                        }

                        return count;
                    });
    file.Close();

    // Everything goes into one packet file: the starting fit, then the profiles.
    std::string packName;
    packName = GamePath(SavePath, fileName, ".pkk");
    MCPacketFile packFile;
    packFile.Create(packName);
    const auto numProfiles = static_cast<int32_t>(numMechs + numWarriors);
    packFile.Reserve(numProfiles + 1, 1);
    MCFile source;
    result = PackFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = PackProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    packFile.Close();
    DeleteFile(fitName);
    DeleteProfiles(numProfiles);
    std::string bridgeName;
    bridgeName = GamePath(SaveTempPath, "bridge", ".fit");
    DeleteFile(bridgeName);

    for (int32_t i = 0; i < 12; i++)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "mech%04d", i);
        std::string mechName;
        mechName = GamePath(SaveTempPath, name, ".fit");
        DeleteFile(mechName);
        std::snprintf(name, sizeof(name), "warr%04d", i);
        std::string warriorName;
        warriorName = GamePath(SaveTempPath, name, ".fit");
        DeleteFile(warriorName);
    }

    return 0;
}

auto MCMissionLogisticsBridge::MissionResultsMechProfileWriter(char* fileName, MCBattleMech* mech, int notAssigned)
    -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Header");
    file.WriteIdString("FileType", "MechProfile");
    file.WriteBlock("General");
    file.WriteIdString("MechType", mech->IfaceName.c_str());
    file.WriteIdString("Name", mech->DebugStatus.c_str());
    file.WriteIdFloat("CurTonnage", mech->GetTonnage());
    file.WriteIdString("icon", mech->IconName);
    file.WriteIdChar("Status", static_cast<char>(mech->Status));
    file.WriteIdULong("Chassis", static_cast<uint32_t>(mech->GetObjectType()->ObjTypeNum));
    file.WriteIdLong("Pilot", mech->PilotId == -1 ? -8 : mech->PilotId);
    file.WriteIdBoolean("Assigned", notAssigned == 0);
    file.WriteIdBoolean("NotMineYet", mech->NotMineYet);
    file.WriteIdLong("DescIndex", mech->DescIndex);
    file.WriteIdLong("NameIndex", mech->NameIndex);
    file.WriteIdLong("NameVariant", mech->NameVariant);
    file.WriteBlock("Engine");
    file.WriteIdFloat("Tonnage", mech->EngineTonnage);
    file.WriteIdULong("Rating", mech->EngineRating);
    file.WriteIdUChar("MaxRunSpeed", static_cast<uint8_t>(static_cast<int32_t>(mech->MaxRunSpeed)));
    file.WriteBlock("Armor");
    file.WriteIdUChar("Type", mech->ArmorType);
    file.WriteIdFloat("Tonnage", mech->ArmorTonnage);
    file.WriteBlock("MaxArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.WriteIdUChar(MechArmorNames[i], mech->Armor[i].MaxArmor);
    }

    file.WriteBlock("CurArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.WriteIdUChar(MechArmorNames[i], static_cast<uint8_t>(static_cast<int32_t>(mech->Armor[i].CurArmor)));
    }

    file.WriteBlock("InventoryInfo");
    file.WriteIdUChar("NumOther", mech->NumOther);
    file.WriteIdUChar("NumWeapons", mech->NumWeapons);
    file.WriteIdUChar("NumAmmo", mech->NumAmmos);

    // Each location's CASE, structure, hot spot and critical spaces (component, hit).
    for (int32_t location = 0; location < 8; location++)
    {
        const MCBodyLocation& body = mech->BodyAt(location);
        file.WriteBlock(MechLocationNames[location]);
        file.WriteIdUChar("CASE", static_cast<uint8_t>(body.HasCase));
        file.WriteIdUChar("CurInternalStructure",
                          static_cast<uint8_t>(static_cast<int32_t>(body.CurInternalStructure)));
        file.WriteIdUChar("HotSpotNumber", body.HotSpotNumber);

        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; space++)
        {
            char id[32];
            std::snprintf(id, sizeof(id), "Component:%d", space);
            const MCCriticalSpace& critical = body.CriticalSpaces[space];
            // Port: the original printed "hit a component" for hit spaces (a debug trace).
            const uint8_t values[2] = {critical.InventoryID, static_cast<uint8_t>(critical.Hit)};
            file.WriteIdUCharArray(id, std::span(values, 2));
        }
    }

    // The inventory: the other equipment, then the weapons with their facing, then the ammo with its amount.
    const int32_t numOther = mech->NumOther;
    const int32_t numWeapons = mech->NumWeapons;
    const int32_t numAmmo = mech->NumAmmos;
    char block[32];
    int32_t item = 0;

    for (; item < numOther; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", mech->Inventory[item].MasterID);
    }

    for (; item < numOther + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", mech->Inventory[item].MasterID);
        file.WriteIdUChar("FacesForward", mech->Inventory[item].FacesForward);
    }

    for (; item < numOther + numWeapons + numAmmo; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", mech->Inventory[item].MasterID);
        file.WriteIdLong("Amount", mech->Inventory[item].Amount);
    }

    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::MissionResultsVehicleProfileWriter(char* fileName, MCGroundVehicle* vehicle) -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Header");
    file.WriteIdString("FileType", "GroundVehicleProfile");
    file.WriteBlock("General");
    file.WriteIdString("Name", vehicle->DebugStatus.c_str());
    file.WriteIdFloat("CurTonnage", vehicle->GetTonnage());
    file.WriteIdString("icon", vehicle->IconName);
    file.WriteIdChar("Status", static_cast<char>(vehicle->Status));
    file.WriteIdString("Crew", vehicle->CrewName.c_str());
    file.WriteIdULong("Chassis", static_cast<uint32_t>(vehicle->GetObjectType()->ObjTypeNum));
    file.WriteIdBoolean("Assigned", 1);
    file.WriteIdBoolean("NotMineYet", vehicle->NotMineYet);
    file.WriteIdLong("DescIndex", vehicle->DescIndex);
    file.WriteIdLong("NameIndex", vehicle->NameIndex);
    file.WriteBlock("Engine");
    file.WriteIdFloat("Tonnage", vehicle->EngineTonnage);
    file.WriteIdULong("Rating", vehicle->EngineRating);
    file.WriteIdUChar("MaxMoveSpeed", static_cast<uint8_t>(static_cast<int32_t>(vehicle->MaxRunSpeed)));
    file.WriteBlock("Armor");
    file.WriteIdUChar("Type", vehicle->ArmorType);
    file.WriteIdFloat("Tonnage", vehicle->ArmorTonnage);

    for (int32_t location = 0; location < 5; location++)
    {
        file.WriteBlock(VehicleLocationNames[location]);
        file.WriteIdUChar("CurInternalStructure",
                          static_cast<uint8_t>(static_cast<int32_t>(vehicle->BodyAt(location).CurInternalStructure)));
        file.WriteIdUChar("MaxArmorPoints", vehicle->Armor[location].MaxArmor);
        file.WriteIdUChar("CurArmorPoints",
                          static_cast<uint8_t>(static_cast<int32_t>(vehicle->Armor[location].CurArmor)));
    }

    file.WriteBlock("InventoryInfo");
    const int32_t numOther = vehicle->NumOther;
    file.WriteIdUChar("NumOther", vehicle->NumOther);
    file.WriteIdUChar("NumWeapons", vehicle->NumWeapons);
    const int32_t numAmmo = vehicle->NumAmmos;
    file.WriteIdUChar("NumAmmo", vehicle->NumAmmos);
    const int32_t numWeapons = vehicle->NumWeapons;
    char block[32];
    int32_t item = 0;

    for (; item < numOther; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", vehicle->Inventory[item].MasterID);
    }

    for (; item < numOther + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", vehicle->Inventory[item].MasterID);
        file.WriteIdUChar("FacesForward", vehicle->Inventory[item].FacesForward);
    }

    for (; item < numOther + numAmmo + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.WriteBlock(block);
        file.WriteIdUChar("MasterID", vehicle->Inventory[item].MasterID);
        file.WriteIdLong("Amount", vehicle->Inventory[item].Amount);
    }

    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::MissionResultsWarriorProfileWriter(char* fileName, MCMechWarrior* warrior) -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("General");
    file.WriteIdString("Name", warrior->Name);
    file.WriteIdString("Callsign", warrior->Callsign);
    file.WriteIdLong("paintScheme", warrior->PaintScheme);
    file.WriteIdString("pilotAudio", warrior->AudioStr);
    file.WriteIdString("pilotVideo", warrior->VideoStr);
    file.WriteIdString("Picture", warrior->Picture);
    file.WriteIdString("Brain", warrior->BrainStr);
    file.WriteIdBoolean("Assigned", 1);

    // Statuses 3, 5 and 6: the pilot got out.
    if (warrior->Status == 3 || warrior->Status == 5 || warrior->Status == 6)
    {
        file.WriteIdBoolean("Ejected", 1);
    }

    file.WriteIdBoolean("NotMineYet", warrior->NotMineYet);
    file.WriteIdLong("DescIndex", warrior->DescIndex);
    file.WriteIdLong("NameIndex", warrior->NameIndex);
    file.WriteBlock("PersonalityTraits");
    file.WriteIdChar("Professionalism", warrior->Professionalism);
    file.WriteIdChar("Decorum", warrior->Decorum);
    file.WriteIdChar("Aggressiveness", static_cast<char>(warrior->GetAggressiveness(1)));
    file.WriteIdChar("Courage", warrior->Courage);
    static constexpr const char* skillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
    file.WriteBlock("Skills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], static_cast<char>(static_cast<int32_t>(warrior->SkillRank[i])));
    }

    file.WriteBlock("OriginalSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], warrior->OriginalSkills[i]);
    }

    file.WriteBlock("LatestSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], warrior->LatestSkills[i]);
    }

    file.WriteBlock("SkillPoints");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdFloat(skillNames[i], warrior->SkillPoints[i]);
    }

    file.WriteBlock("Status");
    file.WriteIdChar("Wounds", static_cast<char>(static_cast<int32_t>(warrior->Wounds)));
    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsStartingFitWriter(char* fileName, int skipFlagged) -> int32_t
{
    MCLogistics* logistics = Mission()->Logistics.get();
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Planet");
    file.WriteIdLong("Setting", CurPlanet);
    file.WriteBlock("General");
    file.WriteIdString("PurchaseFile", GlobalLogPtr->PurchaseFile);
    file.WriteBlock("ResourcePoints");
    file.WriteIdULong("numPoints", static_cast<uint32_t>(ResourcePoints));

    uint32_t numWarriors = 0;
    uint32_t numAssWarriors = 0;
    result = WriteWarriors(this, file, numWarriors, numAssWarriors);

    if (result != 0)
    {
        return result;
    }

    // The spare mechs.
    file.WriteBlock("Mechs");
    uint32_t numMechs = 0;
    MCLogMech* mech = logistics->MechList->Mechs;

    for (int32_t i = logistics->MechList->NumMechs; i > 0; i--, mech = mech->Next)
    {
        if (mech->Assigned == 0)
        {
            ++numMechs;
        }
    }

    file.WriteIdULong("NumMechs", numMechs);
    int32_t index = 0;
    mech = logistics->MechList->Mechs;

    for (int32_t i = 0; i < logistics->MechList->NumMechs; i++, mech = mech->Next)
    {
        if (mech->Assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Mech%d", index, packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsMechProfileWriter(name, mech, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // The force's mechs (without the flagged ones when asked, and without the ones not the player's yet).
    auto forceMechWritten = [skipFlagged](MCLogMech* part)
    { return part->Assigned != 0 && (part->Deployed == 0 || skipFlagged == 0) && part->NotMineYet == 0; };
    file.WriteBlock("AssMechs");
    uint32_t numAssMechs = 0;
    mech = logistics->ForceMechList->Mechs;

    for (int32_t i = logistics->ForceMechList->NumMechs; i > 0; i--, mech = mech->Next)
    {
        if (forceMechWritten(mech))
        {
            ++numAssMechs;
        }
    }

    file.WriteIdULong("NumAssMechs", numAssMechs);
    index = 0;
    mech = logistics->ForceMechList->Mechs;

    for (int32_t i = 0; i < logistics->ForceMechList->NumMechs; i++, mech = mech->Next)
    {
        if (!forceMechWritten(mech))
        {
            continue;
        }

        const uint32_t packet = index + numMechs + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Mech%d", static_cast<int32_t>(index + numMechs), packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsMechProfileWriter(name, mech, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // The vehicles not deployed: the spare ones, then the force's.
    file.WriteBlock("Vehicles");
    uint32_t numVehicles = 0;
    MCLogVehicle* vehicle = logistics->VehicleList->Vehicles;

    for (int32_t i = logistics->VehicleList->NumVehicles; i > 0; i--, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned == 0 && vehicle->Deployed == 0)
        {
            ++numVehicles;
        }
    }

    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = logistics->ForceVehicleList->NumVehicles; i > 0; i--, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned != 0 && vehicle->Deployed == 0)
        {
            ++numVehicles;
        }
    }

    file.WriteIdULong("NumVehicles", numVehicles);
    index = 0;
    auto writeVehicle = [&](MCLogVehicle* part) -> int32_t
    {
        const uint32_t packet = index + numAssMechs + numMechs + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Vehicle%d", index, packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        const int32_t written = LogisticsVehicleProfileWriter(name, part, 0);

        if (written == 0)
        {
            ++index;
        }

        return written;
    };

    vehicle = logistics->VehicleList->Vehicles;

    for (int32_t i = 0; i < logistics->VehicleList->NumVehicles; i++, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned != 0 || vehicle->Deployed != 0)
        {
            continue;
        }

        result = writeVehicle(vehicle);

        if (result != 0)
        {
            return result;
        }
    }

    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = 0; i < logistics->ForceVehicleList->NumVehicles; i++, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned == 0 || vehicle->Deployed != 0)
        {
            continue;
        }

        result = writeVehicle(vehicle);

        if (result != 0)
        {
            return result;
        }
    }

    // The deployed vehicles of the force.
    file.WriteBlock("AssVehicles");
    uint32_t numAssVehicles = 0;
    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = logistics->ForceVehicleList->NumVehicles; i > 0; i--, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned != 0 && vehicle->Deployed != 0 && vehicle->NotMineYet == 0)
        {
            ++numAssVehicles;
        }
    }

    file.WriteIdULong("NumAssVehicles", numAssVehicles);
    index = 0;
    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = 0; i < logistics->ForceVehicleList->NumVehicles; i++, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned == 0 || vehicle->Deployed == 0 || vehicle->NotMineYet != 0)
        {
            continue;
        }

        const uint32_t packet = index + numVehicles + numAssMechs + numMechs + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Vehicle%d", static_cast<int32_t>(index + numVehicles), packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsVehicleProfileWriter(name, vehicle, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    WriteComponents(file, [logistics](uint8_t id) { return logistics->ComponentInventory->GetItemCount(id); });
    file.Close();

    std::string purchaseName;
    purchaseName = GamePath(SavePath, fileName, ".pur");
    result = WritePurchaseFile(purchaseName.c_str());

    if (result != 0)
    {
        return result;
    }

    // The starting fit, the profiles and the purchase file go into one packet file.
    std::string packName;
    packName = GamePath(SavePath, fileName, ".pkk");
    MCPacketFile packFile;
    packFile.Create(packName);
    const auto numProfiles =
        static_cast<int32_t>(numAssVehicles + numVehicles + numAssMechs + numMechs + numAssWarriors + numWarriors);
    packFile.Reserve(numProfiles + 2, 1);
    MCFile source;
    result = PackFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = PackProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    result = PackFileInto(packFile, source, purchaseName, numProfiles + 1);

    if (result != 0)
    {
        return result;
    }

    packFile.Close();
    DeleteFile(fitName);
    DeleteFile(purchaseName);
    // Original behaviour: only the first vehicles + mechs + warriors profiles are deleted; the rest are left in
    // the temp folder (and overwritten next time).
    DeleteProfiles(static_cast<int32_t>(numVehicles + numMechs + numWarriors));
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsMechProfileWriter(char* fileName, MCLogMech* mech, int writeRequired) -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Header");
    file.WriteIdString("FileType", "MechProfile");
    file.WriteBlock("General");
    file.WriteIdString("MechType", mech->FileName);
    file.WriteIdString("Name", mech->MechName);
    file.WriteIdFloat("CurTonnage", mech->CurTonnage);
    file.WriteIdString("icon", mech->IconName);
    file.WriteIdChar("Status", mech->Status);
    file.WriteIdULong("Chassis", mech->Chassis);
    file.WriteIdLong("NameIndex", mech->NameIndex);
    file.WriteIdLong("NameVariant", mech->NameVariant);
    file.WriteIdBoolean("Assigned", mech->Assigned);
    file.WriteIdBoolean("NotMineYet", mech->NotMineYet);
    int32_t pilot = mech->PilotIndex;

    if (pilot == -8 && mech->NotMineYet == 0)
    {
        pilot = -1;
    }

    file.WriteIdLong("Pilot", pilot);

    if (writeRequired != 0)
    {
        file.WriteIdBoolean("Required", mech->Required);
    }

    file.WriteIdLong("DescIndex", mech->DescIndex);
    file.WriteBlock("Engine");
    file.WriteIdFloat("Tonnage", mech->EngineTonnage);
    file.WriteIdULong("Rating", mech->EngineRating);
    file.WriteIdUChar("MaxRunSpeed", mech->MaxRunSpeed);
    file.WriteBlock("Armor");
    file.WriteIdUChar("Type", mech->ArmorType);
    file.WriteIdFloat("Tonnage", mech->ArmorTonnage);
    file.WriteBlock("MaxArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.WriteIdUChar(MechArmorNames[i], mech->Armor[i].MaxArmor);
    }

    file.WriteBlock("CurArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.WriteIdUChar(MechArmorNames[i], mech->Armor[i].CurArmor);
    }

    // The critical slots are rebuilt from the inventory as it is written: other equipment, weapons, then ammo.
    mech->NumOther = 0;
    mech->NumWeapons = 0;
    mech->NumAmmo = 0;

    for (auto& location : mech->ItemSlots)
    {
        for (auto& slot : location)
        {
            slot = {0xff, 0, 0xff};
        }
    }

    char block[32];
    int32_t item = 0;

    for (MCLogInventoryItem* entry = mech->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        const uint8_t masterID = entry->MasterID;
        const MCComponentForm form = MasterComponentList[masterID].Form;

        if (IsWeaponForm(form) || form == MCComponentForm::Ammo)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            mech->PlaceItem(masterID, item, stat->Hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", masterID);
            stat->ItemNum = item;
            ++item;
            ++mech->NumOther;
        }
    }

    for (MCLogInventoryItem* entry = mech->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        const uint8_t masterID = entry->MasterID;

        if (!IsWeaponForm(MasterComponentList[masterID].Form))
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            mech->PlaceItem(masterID, item, stat->Hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", masterID);
            stat->ItemNum = item;
            file.WriteIdUChar("FacesForward", mech->Inventory->GetFacing(static_cast<uint8_t>(item)));
            ++item;
            ++mech->NumWeapons;
        }
    }

    for (MCLogInventoryItem* entry = mech->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        const uint8_t masterID = entry->MasterID;

        if (MasterComponentList[masterID].Form != MCComponentForm::Ammo)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            mech->PlaceItem(masterID, item, stat->Hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", masterID);
            file.WriteIdLong("Amount", -1);
            stat->ItemNum = item;
            ++item;
            ++mech->NumAmmo;
        }
    }

    file.WriteBlock("InventoryInfo");
    file.WriteIdUChar("NumOther", mech->NumOther);
    file.WriteIdUChar("NumWeapons", mech->NumWeapons);
    file.WriteIdUChar("NumAmmo", mech->NumAmmo);

    for (int32_t location = 0; location < 8; location++)
    {
        file.WriteBlock(MechLocationNames[location]);
        file.WriteIdUChar("CASE", static_cast<uint8_t>(mech->HasCase[location]));
        file.WriteIdUChar("CurInternalStructure", mech->Internals[location].CurArmor);
        file.WriteIdUChar("HotSpotNumber", mech->HotSpotNumber[location]);

        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; space++)
        {
            char id[32];
            std::snprintf(id, sizeof(id), "Component:%d", space);
            const MCLogMech::ItemSlot& slot = mech->ItemSlots[location][space];
            const uint8_t values[2] = {slot.Row, slot.Column};
            file.WriteIdUCharArray(id, std::span(values, 2));
        }
    }

    std::strncpy(mech->ProfileName, fileName, 9);
    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsVehicleProfileWriter(char* fileName, MCLogVehicle* vehicle, int writeRequired)
    -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Header");
    file.WriteIdString("FileType", "GroundVehicleProfile");
    file.WriteBlock("General");
    file.WriteIdString("Name", vehicle->FileName);
    file.WriteIdFloat("CurTonnage", vehicle->CurTonnage);
    file.WriteIdLong("NameIndex", vehicle->NameIndex);
    file.WriteIdString("icon", vehicle->IconName);
    file.WriteIdChar("Status", vehicle->Status);
    file.WriteIdString("Crew", vehicle->Crew);
    file.WriteIdULong("Chassis", vehicle->Chassis);
    file.WriteIdBoolean("Assigned", vehicle->Assigned);
    file.WriteIdBoolean("NotMineYet", vehicle->NotMineYet);
    file.WriteIdBoolean("Deployed", vehicle->Deployed);

    if (writeRequired != 0)
    {
        file.WriteIdBoolean("Required", vehicle->Required);
    }

    file.WriteIdLong("DescIndex", vehicle->DescIndex);
    file.WriteBlock("Engine");
    file.WriteIdFloat("Tonnage", vehicle->EngineTonnage);
    file.WriteIdULong("Rating", vehicle->EngineRating);
    file.WriteIdUChar("MaxMoveSpeed", vehicle->MaxMoveSpeed);
    file.WriteBlock("Armor");
    file.WriteIdUChar("Type", vehicle->ArmorType);
    file.WriteIdFloat("Tonnage", vehicle->ArmorTonnage);

    for (int32_t location = 0; location < 5; location++)
    {
        file.WriteBlock(VehicleLocationNames[location]);
        file.WriteIdUChar("CurInternalStructure", vehicle->CurInternalStructure[location]);
        file.WriteIdUChar("MaxArmorPoints", vehicle->MaxArmorPoints[location]);
        file.WriteIdUChar("CurArmorPoints", vehicle->CurArmorPoints[location]);
    }

    file.WriteBlock("InventoryInfo");
    file.WriteIdUChar("NumOther", vehicle->NumOther);
    file.WriteIdUChar("NumWeapons", vehicle->NumWeapons);
    file.WriteIdUChar("NumAmmo", vehicle->NumAmmo);

    char block[32];
    int32_t item = 0;

    for (MCLogInventoryItem* entry = vehicle->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        const MCComponentForm form = MasterComponentList[entry->MasterID].Form;

        if (IsWeaponForm(form) || form == MCComponentForm::Ammo)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", entry->MasterID);
            ++item;
        }
    }

    for (MCLogInventoryItem* entry = vehicle->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        if (!IsWeaponForm(MasterComponentList[entry->MasterID].Form))
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", entry->MasterID);
            file.WriteIdUChar("FacesForward", stat->Facing);
            ++item;
        }
    }

    for (MCLogInventoryItem* entry = vehicle->Inventory->GetItemInfo(0); entry != nullptr; entry = entry->Next)
    {
        if (MasterComponentList[entry->MasterID].Form != MCComponentForm::Ammo)
        {
            continue;
        }

        for (MCLogInventoryStat* stat = entry->Stats; stat != nullptr; stat = stat->Next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.WriteBlock(block);
            file.WriteIdUChar("MasterID", entry->MasterID);
            file.WriteIdLong("Amount", -1);
            ++item;
        }
    }

    std::strncpy(vehicle->ProfileName, fileName, 9);
    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsWarriorProfileWriter(char* fileName, MCLogWarrior* warrior) -> int32_t
{
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    const int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("General");
    file.WriteIdString("Name", warrior->Name);
    file.WriteIdString("Callsign", warrior->Callsign);
    file.WriteIdLong("paintScheme", warrior->PaintScheme);
    file.WriteIdString("pilotAudio", warrior->PilotAudio);
    file.WriteIdString("pilotVideo", warrior->PilotVideo);
    file.WriteIdString("Picture", warrior->Picture);
    file.WriteIdString("Brain", warrior->Brain);
    file.WriteIdBoolean("Assigned", warrior->Assigned);
    file.WriteIdBoolean("NotMineYet", warrior->NotMineYet);
    file.WriteIdLong("NameIndex", warrior->NameIndex);
    file.WriteIdLong("DescIndex", warrior->DescIndex);
    file.WriteBlock("PersonalityTraits");
    file.WriteIdChar("Professionalism", warrior->Personality[0]);
    file.WriteIdChar("Decorum", warrior->Personality[1]);
    file.WriteIdChar("Aggressiveness", warrior->Personality[2]);
    file.WriteIdChar("Courage", warrior->Personality[3]);
    static constexpr const char* skillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
    file.WriteBlock("Skills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], warrior->Skills[i]);
    }

    file.WriteBlock("OriginalSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], warrior->OriginalSkills[i]);
    }

    file.WriteBlock("LatestSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdChar(skillNames[i], warrior->StartingSkills[i]);
    }

    file.WriteBlock("SkillPoints");

    for (int32_t i = 0; i < 4; i++)
    {
        file.WriteIdFloat(skillNames[i], warrior->SkillPoints[i]);
    }

    file.WriteBlock("Status");
    file.WriteIdChar("Wounds", static_cast<char>(static_cast<int32_t>(warrior->Wounds)));
    std::strncpy(warrior->FileName, fileName, 9);
    file.Close();
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsStartingFitReader(char*) -> int32_t
{
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsMechProfileReader(char*) -> int32_t
{
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsVehicleProfileReader(char*) -> int32_t
{
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsWarriorProfileReader(char*) -> int32_t
{
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsSaveGame(char* fileName) -> int32_t
{
    MCLogistics* logistics = Mission()->Logistics.get();
    std::string fitName;
    fitName = GamePath(SaveTempPath, fileName, ".fit");
    MCFitIniFile file;
    int32_t result = file.Create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.WriteBlock("Planet");
    file.WriteIdLong("Setting", CurPlanet);
    file.WriteBlock("General");
    file.WriteIdString("PurchaseFile", CurPlanet == 0 ? "purchase" : "xpur");
    file.WriteIdLong("MissionNumber", logistics->CurrentMission);
    file.WriteIdFloat("LastScenarioTime", TotalScenarioTime);
    file.WriteIdFloat("LastLogisticsTime", TotalLogisticsTime);
    file.WriteBlock("ResourcePoints");
    file.WriteIdULong("numPoints", static_cast<uint32_t>(ResourcePoints));

    uint32_t numWarriors = 0;
    uint32_t numAssWarriors = 0;
    result = WriteWarriors(this, file, numWarriors, numAssWarriors);

    if (result != 0)
    {
        return result;
    }

    // Every mech: the spare ones, then the force.
    file.WriteBlock("Mechs");
    uint32_t numMechs = 0;
    MCLogMech* mech = logistics->MechList->Mechs;

    for (int32_t i = logistics->MechList->NumMechs; i > 0; i--, mech = mech->Next)
    {
        if (mech->Assigned == 0)
        {
            ++numMechs;
        }
    }

    file.WriteIdULong("NumMechs", numMechs);
    int32_t index = 0;
    mech = logistics->MechList->Mechs;

    for (int32_t i = 0; i < logistics->MechList->NumMechs; i++, mech = mech->Next)
    {
        if (mech->Assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Mech%d", index, packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    file.WriteBlock("AssMechs");
    uint32_t numAssMechs = 0;
    mech = logistics->ForceMechList->Mechs;

    for (int32_t i = logistics->ForceMechList->NumMechs; i > 0; i--, mech = mech->Next)
    {
        if (mech->Assigned != 0)
        {
            ++numAssMechs;
        }
    }

    file.WriteIdULong("NumAssMechs", numAssMechs);
    index = 0;
    mech = logistics->ForceMechList->Mechs;

    for (int32_t i = 0; i < logistics->ForceMechList->NumMechs; i++, mech = mech->Next)
    {
        if (mech->Assigned == 0)
        {
            continue;
        }

        const uint32_t packet = index + numMechs + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Mech%d", static_cast<int32_t>(index + numMechs), packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // Every vehicle: the spare ones, then the force.
    file.WriteBlock("Vehicles");
    uint32_t numVehicles = 0;
    MCLogVehicle* vehicle = logistics->VehicleList->Vehicles;

    for (int32_t i = logistics->VehicleList->NumVehicles; i > 0; i--, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned == 0)
        {
            ++numVehicles;
        }
    }

    file.WriteIdULong("NumVehicles", numVehicles);
    index = 0;
    vehicle = logistics->VehicleList->Vehicles;

    for (int32_t i = 0; i < logistics->VehicleList->NumVehicles; i++, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssMechs + numMechs + numAssWarriors + numWarriors;
        WritePacketBlock(file, "Vehicle%d", index, packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsVehicleProfileWriter(name, vehicle, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    file.WriteBlock("AssVehicles");
    uint32_t numAssVehicles = 0;
    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = logistics->ForceVehicleList->NumVehicles; i > 0; i--, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned != 0)
        {
            ++numAssVehicles;
        }
    }

    file.WriteIdULong("NumAssVehicles", numAssVehicles);
    index = 0;
    vehicle = logistics->ForceVehicleList->Vehicles;

    for (int32_t i = 0; i < logistics->ForceVehicleList->NumVehicles; i++, vehicle = vehicle->Next)
    {
        if (vehicle->Assigned == 0)
        {
            continue;
        }

        const uint32_t packet = numVehicles + numWarriors + numAssWarriors + numMechs + numAssMechs + index;
        WritePacketBlock(file, "Vehicle%d", static_cast<int32_t>(numVehicles + index), packet);
        char name[32];
        ProfileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = LogisticsVehicleProfileWriter(name, vehicle, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    WriteComponents(file, [logistics](uint8_t id) { return logistics->ComponentInventory->GetItemCount(id); });
    file.Close();

    std::string purchaseName;
    purchaseName = GamePath(SavePath, fileName, ".pur");
    result = WritePurchaseFile(purchaseName.c_str());

    if (result != 0)
    {
        return result;
    }

    std::string saveName;
    saveName = GamePath(SavePath, fileName, ".sav");
    MCPacketFile packFile;
    result = packFile.Create(saveName);

    if (result != 0)
    {
        return result;
    }

    const auto numProfiles =
        static_cast<int32_t>(numVehicles + numWarriors + numAssWarriors + numMechs + numAssMechs + numAssVehicles);
    packFile.Reserve(numProfiles + 2, 1);
    MCFile source;
    result = PackFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = PackProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    result = PackFileInto(packFile, source, purchaseName, numProfiles + 1);

    if (result != 0)
    {
        return result;
    }

    packFile.Close();
    DeleteFile(fitName);
    DeleteFile(purchaseName);
    DeleteProfiles(numProfiles);
    return 0;
}

auto MCMissionLogisticsBridge::LogisticsLoadGame(char*) -> int32_t
{
    return 0;
}

auto MCMissionLogisticsBridge::MissionToLogisticsBridgeSave(char*) -> int32_t
{
    return 0;
}
