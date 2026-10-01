#include "stdafx.h"
#include "logistics/misslog.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/cmponent.h"
#include "object/gvehicl.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/team.h"
#include "object/warrior.h"
#include "platform/MCFileSystem.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

char* componentComment[NUM_FIT_COMPONENTS] = {
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

uint8_t componentId[NUM_FIT_COMPONENTS] = {
    0x90, 0x99, 0x7b, 0x85, 0x8f, 0x7d, 0x87, 0x93, 0x9b, 0x66, 0x70, 0x8e, 0x98, 0x97, 0x8c, 0x65, 0x6f,
    0x91, 0x78, 0x82, 0x64, 0x67, 0x6e, 0x8d, 0x96, 0x92, 0x68, 0x71, 0x9a, 0x0d, 0x10, 0x0e, 0x0f, 0x11,
    0x25, 0x26, 0x2a, 0x2b, 0x62, 0x63, 0x6b, 0x6c, 0x6d, 0x74, 0x75, 0x76, 0x7e, 0x8b, 0xa0, 0xa1,
};

float totalScenarioTime = 0.0f;
float totalLogisticsTime = 0.0f;

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
    bool isWeaponForm(int32_t form)
    {
        return form == 6 || form == 7 || form == 8 || form == 9;
    }

    /// <summary>The CRT's <c>DeleteFileA</c> on a game path.</summary>
    void deleteFile(const char* fileName)
    {
        MCFileSystem::RemoveFile(fileName);
    }

    /// <summary>The name of profile <paramref name="index"/> ("tpak<i>n</i>").</summary>
    void profileName(char* name, size_t size, int32_t index)
    {
        std::snprintf(name, size, "tpak%d", index);
    }

    /// <summary>Starts block <paramref name="format"/> <paramref name="index"/> with its PacketNum.</summary>
    void writePacketBlock(FitIniFile& file, const char* format, int32_t index, uint32_t packetNum)
    {
        char block[32];
        std::snprintf(block, sizeof(block), format, index);
        file.writeBlock(block);
        file.writeIdULong("PacketNum", packetNum);
    }

    /// <summary>Writes the 50 "Componant" blocks: each one's comment line, id and the count from <paramref name="countOf"/>.</summary>
    template <typename CountFn> void writeComponents(FitIniFile& file, CountFn countOf)
    {
        file.writeBlock("Components");
        file.writeIdULong("NumComponents", NUM_FIT_COMPONENTS);

        for (int32_t i = 0; i < NUM_FIT_COMPONENTS; i++)
        {
            char block[32];
            std::snprintf(block, sizeof(block), "Componant%d", i);
            file.writeBlock(block);
            file.writeLine(componentComment[i]);
            file.writeIdUChar("ComponantId", componentId[i]);
            file.writeIdLong("NumAvailable", countOf(componentId[i]));
        }
    }

    /// <summary>
    /// Copies file <paramref name="fileName"/> into packet <paramref name="packet"/> of <paramref name="packFile"/>.
    /// Returns the open error, if any.
    /// </summary>
    int32_t packFileInto(PacketFile& packFile, File& file, const char* fileName, int32_t packet)
    {
        const int32_t result = file.open(fileName, READ, 0x32);

        if (result != 0)
        {
            return result;
        }

        const uint32_t size = file.fileSize();
        std::vector<uint8_t> buffer(size);
        file.read(buffer.data(), static_cast<int32_t>(size));
        packFile.writePacket(packet, buffer.data(), static_cast<int32_t>(size), 2);
        file.close();
        return 0;
    }

    /// <summary>Packs profile files tpak0 .. tpak<paramref name="count"/>-1 into packets 1 .. <paramref name="count"/>.</summary>
    int32_t packProfiles(PacketFile& packFile, File& file, int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            char name[32];
            profileName(name, sizeof(name), i);
            FullPathFileName fileName;
            fileName.init(saveTempPath, name, ".fit");
            const int32_t result = packFileInto(packFile, file, fileName, i + 1);

            if (result != 0)
            {
                return result;
            }
        }

        return 0;
    }

    /// <summary>Deletes profile files tpak0 .. tpak<paramref name="count"/>-1.</summary>
    void deleteProfiles(int32_t count)
    {
        for (int32_t i = 0; i < count; i++)
        {
            char name[32];
            profileName(name, sizeof(name), i);
            FullPathFileName fileName;
            fileName.init(saveTempPath, name, ".fit");
            deleteFile(fileName);
        }
    }

    /// <summary>The warrior at <paramref name="index"/> of <paramref name="list"/> (walked from the head).</summary>
    LogWarrior* warriorAt(LogWarriorList* list, int32_t index)
    {
        LogWarrior* warrior = list->warriors;

        for (int32_t i = index; i > 0; i--)
        {
            warrior = warrior->next;
        }

        return warrior;
    }

    /// <summary>How many warriors of <paramref name="list"/> are (or are not) assigned.</summary>
    uint32_t countWarriors(LogWarriorList* list, bool assigned)
    {
        uint32_t count = 0;
        LogWarrior* warrior = list->warriors;

        for (int32_t i = list->numWarriors; i > 0; i--)
        {
            if ((warrior->assigned != 0) == assigned)
            {
                ++count;
            }

            warrior = warrior->next;
        }

        return count;
    }

    /// <summary>
    /// A purchasable pilot's Status in the purchase file: 3 when a pilot of the player's with the same description
    /// was sold, 1 when one is alive, 2 when one is dead; otherwise the pilot's own status. The player's unassigned
    /// pilots are searched first, then the assigned ones; the last match of a list wins.
    /// </summary>
    int32_t purchasePilotStatus(Logistics* logistics, const PurPilotData* pilot)
    {
        auto search = [pilot](LogWarriorList* list)
        {
            int32_t status = -1;

            for (int32_t i = 0; i < list->numWarriors; i++)
            {
                LogWarrior* warrior = nullptr;
                list->getWarriorInfo(i, warrior);

                if (warrior->descIndex != pilot->descIndex)
                {
                    continue;
                }

                if (warrior->sold != 0)
                {
                    status = 3;
                }
                else
                {
                    status = warrior->health > 0.0f ? 1 : 2;
                }
            }

            return status;
        };

        int32_t status = search(logistics->warriorList);

        if (status == -1)
        {
            status = search(logistics->assignedWarriorList);
        }

        if (status == -1)
        {
            status = pilot->status;
        }

        return status;
    }

    /// <summary>Writes the purchase file: what the shop has left of each mech, vehicle, component and pilot.</summary>
    int32_t writePurchaseFile(const char* fileName)
    {
        Logistics* logistics = mission->logistics;
        FitIniFile file;
        const int32_t result = file.create(fileName);

        if (result != 0)
        {
            return result;
        }

        file.writeBlock("Header");
        file.writeIdLong("NumGifts", 0);
        file.writeIdLong("NumMechs", logistics->purMechList->count);
        file.writeIdLong("NumVehicles", logistics->purVehicleList->count);
        file.writeIdLong("NumComponants", logistics->purchaseComponents->numItems);
        file.writeIdLong("NumWarriors", logistics->purPilotList->count);
        char block[32];

        for (int32_t i = 0; i < logistics->purMechList->count; i++)
        {
            std::snprintf(block, sizeof(block), "Mech%d", i);
            file.writeBlock(block);
            PurMech* mech = nullptr;
            logistics->purMechList->getMechInfo(i, mech);
            file.writeIdLong("TypeAAvailable", mech->variants[0]->numAvailable);
            file.writeIdLong("TypeWAvailable", mech->variants[1]->numAvailable);
            file.writeIdLong("TypeJAvailable", mech->variants[2]->numAvailable);
            file.writeIdString("TypeAFile", mech->variants[0]->fileName);
            file.writeIdString("TypeWFile", mech->variants[1]->fileName);
            file.writeIdString("TypeJFile", mech->variants[2]->fileName);
        }

        for (int32_t i = 0; i < logistics->purVehicleList->count; i++)
        {
            std::snprintf(block, sizeof(block), "Vehicle%d", i);
            file.writeBlock(block);
            PurVehicle* vehicle = nullptr;
            logistics->purVehicleList->getVehicleInfo(i, vehicle);
            file.writeIdLong("NumAvailable", vehicle->data->numAvailable);
            file.writeIdString("Filename", vehicle->data->fileName);
        }

        for (int32_t i = 0; i < logistics->purchaseComponents->numItems; i++)
        {
            std::snprintf(block, sizeof(block), "Componant%d", i);
            file.writeBlock(block);
            _LogInventoryItem* item = logistics->purchaseComponents->getItemInfo(i);
            file.writeIdUChar("ComponantID", item->masterID);
            file.writeIdLong("NumAvailable", item->count);
        }

        for (int32_t i = 0; i < logistics->purPilotList->count; i++)
        {
            std::snprintf(block, sizeof(block), "Warrior%d", i);
            file.writeBlock(block);
            PurPilotData* pilot = nullptr;
            logistics->purPilotList->getPilotInfo(i, pilot);
            file.writeIdString("Profile", pilot->fileName);
            file.writeIdLong("Status", purchasePilotStatus(logistics, pilot));
        }

        file.close();
        return 0;
    }

    /// <summary>
    /// Writes the pilots of the player's list: first the unassigned ones as "Warriors", then the assigned ones as
    /// "AssWarriors", each list walked from its last pilot to its first. Returns the first error.
    /// </summary>
    int32_t writeWarriors(MissionLogisticsBridge* bridge, FitIniFile& file, uint32_t& numWarriors,
                          uint32_t& numAssigned)
    {
        Logistics* logistics = mission->logistics;
        file.writeBlock("Warriors");
        numWarriors = countWarriors(logistics->warriorList, false);
        file.writeIdULong("NumWarriors", numWarriors);
        uint32_t packet = 0;

        for (int32_t i = logistics->warriorList->numWarriors - 1; i >= 0; i--)
        {
            LogWarrior* warrior = warriorAt(logistics->warriorList, i);

            if (warrior->assigned != 0)
            {
                continue;
            }

            writePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
            char name[32];
            profileName(name, sizeof(name), static_cast<int32_t>(packet));
            const int32_t result = bridge->logisticsWarriorProfileWriter(name, warrior);

            if (result != 0)
            {
                return result;
            }

            ++packet;
        }

        file.writeBlock("AssWarriors");
        numAssigned = countWarriors(logistics->assignedWarriorList, true);
        file.writeIdULong("NumAssWarriors", numAssigned);
        packet = numWarriors;

        for (int32_t i = logistics->assignedWarriorList->numWarriors - 1; i >= 0; i--)
        {
            LogWarrior* warrior = warriorAt(logistics->assignedWarriorList, i);

            if (warrior->assigned == 0)
            {
                continue;
            }

            writePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
            char name[32];
            profileName(name, sizeof(name), static_cast<int32_t>(packet));
            const int32_t result = bridge->logisticsWarriorProfileWriter(name, warrior);

            if (result != 0)
            {
                return result;
            }

            ++packet;
        }

        return 0;
    }

    /// <summary>The scenario's warrior <paramref name="index"/> (1-based; null out of range).</summary>
    MechWarrior* scenarioWarrior(uint32_t index)
    {
        if (static_cast<int32_t>(index) < 1 || scenario->numWarriors < index)
        {
            return nullptr;
        }

        return scenario->warriors[index];
    }

    /// <summary>
    /// Whether a mission unit goes back to logistics: a player mech still under player control, not one that
    /// only joins on a win (NotMineYet) unless the mission was won.
    /// </summary>
    bool returnsFromMission(int32_t objectClass, int32_t alignment, int32_t netPlayerId, int notMineYet)
    {
        return objectClass == BATTLEMECH && alignment == homeTeam->alignment && netPlayerId != -1 &&
               (notMineYet == 0 || scenarioResult > 3);
    }
}

void destroyAllFITFiles(char* path)
{
    char pattern[0x1000];
    std::snprintf(pattern, sizeof(pattern), "%s*.fit", path);

    for (const std::string& name : MCFileSystem::FindFiles(pattern))
    {
        char fileName[0x1000];
        std::snprintf(fileName, sizeof(fileName), "%s%s", path, name.c_str());
        deleteFile(fileName);
    }
}

auto MissionLogisticsBridge::missionResultsStartingFitWriter(char* fileName) -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Planet");
    file.writeIdLong("Setting", CurPlanet);
    file.writeBlock("General");
    file.writeIdString("PurchaseFile", CurPlanet == 0 ? "purchase" : "xpur");
    file.writeBlock("ResourcePoints");
    file.writeIdULong("numPoints", static_cast<uint32_t>(scenario->calcResourcePointsEarned()));

    // The surviving pilots of the player's mechs.
    file.writeBlock("Warriors");
    auto warriorReturns = [](MechWarrior* warrior)
    {
        auto* vehicle = static_cast<Mover*>(warrior->vehicle);
        return returnsFromMission(vehicle->objectClass, warrior->alignment, vehicle->netPlayerId, warrior->notMineYet);
    };

    uint32_t numWarriors = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) <= static_cast<int32_t>(scenario->numWarriors); i++)
    {
        if (warriorReturns(scenarioWarrior(i)))
        {
            ++numWarriors;
        }
    }

    file.writeIdULong("NumWarriors", numWarriors);
    uint32_t packet = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) <= static_cast<int32_t>(scenario->numWarriors); i++)
    {
        MechWarrior* warrior = scenarioWarrior(i);

        if (!warriorReturns(warrior))
        {
            continue;
        }

        writePacketBlock(file, "Warrior%d", static_cast<int32_t>(packet), packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = missionResultsWarriorProfileWriter(name, warrior);

        if (result != 0)
        {
            return result;
        }

        ++packet;
    }

    // The player's mechs, then the salvaged ones.
    file.writeBlock("Mechs");
    auto mechReturns = [](BaseObject* object)
    {
        auto* mech = static_cast<BattleMech*>(object);
        return object->objectClass == BATTLEMECH &&
               returnsFromMission(object->objectClass, mech->getAlignment(), mech->netPlayerId, mech->notMineYet);
    };

    TacticalMap* tacMap = Terrain::terrainTacticalMap;
    uint32_t numMechs = 0;

    for (BaseObject* object = innerSphereMechList->head; object != nullptr; object = object->next)
    {
        if (mechReturns(object))
        {
            ++numMechs;
        }
    }

    for (int32_t i = 0; i < tacMap->numSalvage; i++)
    {
        if (tacMap->salvage[i] != nullptr && tacMap->salvage[i]->objectClass == BATTLEMECH)
        {
            ++numMechs;
        }
    }

    file.writeIdULong("NumMechs", numMechs);
    int32_t mechIndex = 0;
    packet = numWarriors;

    for (BaseObject* object = innerSphereMechList->head; object != nullptr; object = object->next)
    {
        if (!mechReturns(object))
        {
            continue;
        }

        writePacketBlock(file, "Mech%d", mechIndex, packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = missionResultsMechProfileWriter(name, static_cast<BattleMech*>(object), 0);

        if (result != 0)
        {
            return result;
        }

        ++mechIndex;
        ++packet;
    }

    for (int32_t i = 0; i < tacMap->numSalvage; i++)
    {
        GameObject* salvage = tacMap->salvage[i];

        if (salvage == nullptr || salvage->objectClass != BATTLEMECH)
        {
            continue;
        }

        writePacketBlock(file, "Mech%d", mechIndex, packet);
        auto* mech = static_cast<BattleMech*>(salvage);
        mech->notMineYet = 0;
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = missionResultsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++mechIndex;
        ++packet;
    }

    file.writeIdULong("NumVehicles", 0);

    // The components salvaged on the map.
    writeComponents(file,
                    [tacMap](uint8_t id)
                    {
                        int32_t count = 0;

                        for (int32_t i = 0; i < tacMap->numSalvage; i++)
                        {
                            GameObject* salvage = tacMap->salvage[i];

                            if (salvage == nullptr)
                            {
                                continue;
                            }

                            for (SalvageItem* item = salvage->getSalvage(); item != nullptr; item = item->next)
                            {
                                if (item->itemId == id)
                                {
                                    count += item->numItems;
                                }
                            }
                        }

                        return count;
                    });
    file.close();

    // Everything goes into one packet file: the starting fit, then the profiles.
    FullPathFileName packName;
    packName.init(savePath, fileName, ".pkk");
    PacketFile packFile;
    packFile.create(packName);
    const auto numProfiles = static_cast<int32_t>(numMechs + numWarriors);
    packFile.reserve(numProfiles + 1, 1);
    File source;
    result = packFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = packProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    packFile.close();
    deleteFile(fitName);
    deleteProfiles(numProfiles);
    FullPathFileName bridgeName;
    bridgeName.init(saveTempPath, "bridge", ".fit");
    deleteFile(bridgeName);

    for (int32_t i = 0; i < 12; i++)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "mech%04d", i);
        FullPathFileName mechName;
        mechName.init(saveTempPath, name, ".fit");
        deleteFile(mechName);
        std::snprintf(name, sizeof(name), "warr%04d", i);
        FullPathFileName warriorName;
        warriorName.init(saveTempPath, name, ".fit");
        deleteFile(warriorName);
    }

    return 0;
}

auto MissionLogisticsBridge::missionResultsMechProfileWriter(char* fileName, BattleMech* mech, int notAssigned)
    -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Header");
    file.writeIdString("FileType", "MechProfile");
    file.writeBlock("General");
    file.writeIdString("MechType", mech->ifaceName);
    file.writeIdString("Name", mech->debugStatus);
    file.writeIdFloat("CurTonnage", mech->getTonnage());
    file.writeIdString("icon", mech->iconName);
    file.writeIdChar("Status", static_cast<char>(mech->status));
    file.writeIdULong("Chassis", static_cast<uint32_t>(mech->getObjectType()->objTypeNum));
    file.writeIdLong("Pilot", mech->pilotId == -1 ? -8 : mech->pilotId);
    file.writeIdBoolean("Assigned", notAssigned == 0);
    file.writeIdBoolean("NotMineYet", mech->notMineYet);
    file.writeIdLong("DescIndex", mech->descIndex);
    file.writeIdLong("NameIndex", mech->nameIndex);
    file.writeIdLong("NameVariant", mech->nameVariant);
    file.writeBlock("Engine");
    file.writeIdFloat("Tonnage", mech->engineTonnage);
    file.writeIdULong("Rating", mech->engineRating);
    file.writeIdUChar("MaxRunSpeed", static_cast<uint8_t>(static_cast<int32_t>(mech->maxRunSpeed)));
    file.writeBlock("Armor");
    file.writeIdUChar("Type", mech->armorType);
    file.writeIdFloat("Tonnage", mech->armorTonnage);
    file.writeBlock("MaxArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.writeIdUChar(MechArmorNames[i], mech->armor[i].maxArmor);
    }

    file.writeBlock("CurArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.writeIdUChar(MechArmorNames[i], static_cast<uint8_t>(static_cast<int32_t>(mech->armor[i].curArmor)));
    }

    file.writeBlock("InventoryInfo");
    file.writeIdUChar("NumOther", mech->numOther);
    file.writeIdUChar("NumWeapons", mech->numWeapons);
    file.writeIdUChar("NumAmmo", mech->numAmmos);

    // Each location's CASE, structure, hot spot and critical spaces (component, hit).
    for (int32_t location = 0; location < 8; location++)
    {
        const BodyLocation& body = mech->bodyAt(location);
        file.writeBlock(MechLocationNames[location]);
        file.writeIdUChar("CASE", static_cast<uint8_t>(body.hasCASE));
        file.writeIdUChar("CurInternalStructure",
                          static_cast<uint8_t>(static_cast<int32_t>(body.curInternalStructure)));
        file.writeIdUChar("HotSpotNumber", body.hotSpotNumber);

        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; space++)
        {
            char id[32];
            std::snprintf(id, sizeof(id), "Component:%d", space);
            const CriticalSpace& critical = body.criticalSpaces[space];
            // Port: the original printed "hit a component" for hit spaces (a debug trace).
            const uint8_t values[2] = {critical.inventoryID, static_cast<uint8_t>(critical.hit)};
            file.writeIdUCharArray(id, values, 2);
        }
    }

    // The inventory: the other equipment, then the weapons with their facing, then the ammo with its amount.
    const int32_t numOther = mech->numOther;
    const int32_t numWeapons = mech->numWeapons;
    const int32_t numAmmo = mech->numAmmos;
    char block[32];
    int32_t item = 0;

    for (; item < numOther; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", mech->inventory[item].masterID);
    }

    for (; item < numOther + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", mech->inventory[item].masterID);
        file.writeIdUChar("FacesForward", mech->inventory[item].facesForward);
    }

    for (; item < numOther + numWeapons + numAmmo; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", mech->inventory[item].masterID);
        file.writeIdLong("Amount", mech->inventory[item].amount);
    }

    file.close();
    return 0;
}

auto MissionLogisticsBridge::missionResultsVehicleProfileWriter(char* fileName, GroundVehicle* vehicle) -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Header");
    file.writeIdString("FileType", "GroundVehicleProfile");
    file.writeBlock("General");
    file.writeIdString("Name", vehicle->debugStatus);
    file.writeIdFloat("CurTonnage", vehicle->getTonnage());
    file.writeIdString("icon", vehicle->iconName);
    file.writeIdChar("Status", static_cast<char>(vehicle->status));
    file.writeIdString("Crew", vehicle->crewName);
    file.writeIdULong("Chassis", static_cast<uint32_t>(vehicle->getObjectType()->objTypeNum));
    file.writeIdBoolean("Assigned", 1);
    file.writeIdBoolean("NotMineYet", vehicle->notMineYet);
    file.writeIdLong("DescIndex", vehicle->descIndex);
    file.writeIdLong("NameIndex", vehicle->nameIndex);
    file.writeBlock("Engine");
    file.writeIdFloat("Tonnage", vehicle->engineTonnage);
    file.writeIdULong("Rating", vehicle->engineRating);
    file.writeIdUChar("MaxMoveSpeed", static_cast<uint8_t>(static_cast<int32_t>(vehicle->maxRunSpeed)));
    file.writeBlock("Armor");
    file.writeIdUChar("Type", vehicle->armorType);
    file.writeIdFloat("Tonnage", vehicle->armorTonnage);

    for (int32_t location = 0; location < 5; location++)
    {
        file.writeBlock(VehicleLocationNames[location]);
        file.writeIdUChar("CurInternalStructure",
                          static_cast<uint8_t>(static_cast<int32_t>(vehicle->bodyAt(location).curInternalStructure)));
        file.writeIdUChar("MaxArmorPoints", vehicle->armor[location].maxArmor);
        file.writeIdUChar("CurArmorPoints",
                          static_cast<uint8_t>(static_cast<int32_t>(vehicle->armor[location].curArmor)));
    }

    file.writeBlock("InventoryInfo");
    const int32_t numOther = vehicle->numOther;
    file.writeIdUChar("NumOther", vehicle->numOther);
    file.writeIdUChar("NumWeapons", vehicle->numWeapons);
    const int32_t numAmmo = vehicle->numAmmos;
    file.writeIdUChar("NumAmmo", vehicle->numAmmos);
    const int32_t numWeapons = vehicle->numWeapons;
    char block[32];
    int32_t item = 0;

    for (; item < numOther; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", vehicle->inventory[item].masterID);
    }

    for (; item < numOther + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", vehicle->inventory[item].masterID);
        file.writeIdUChar("FacesForward", vehicle->inventory[item].facesForward);
    }

    for (; item < numOther + numAmmo + numWeapons; item++)
    {
        std::snprintf(block, sizeof(block), "Item:%d", item);
        file.writeBlock(block);
        file.writeIdUChar("MasterID", vehicle->inventory[item].masterID);
        file.writeIdLong("Amount", vehicle->inventory[item].amount);
    }

    file.close();
    return 0;
}

auto MissionLogisticsBridge::missionResultsWarriorProfileWriter(char* fileName, MechWarrior* warrior) -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("General");
    file.writeIdString("Name", warrior->name);
    file.writeIdString("Callsign", warrior->callsign);
    file.writeIdLong("paintScheme", warrior->paintScheme);
    file.writeIdString("pilotAudio", warrior->audioStr);
    file.writeIdString("pilotVideo", warrior->videoStr);
    file.writeIdString("Picture", warrior->picture);
    file.writeIdString("Brain", warrior->brainStr);
    file.writeIdBoolean("Assigned", 1);

    // Statuses 3, 5 and 6: the pilot got out.
    if (warrior->status == 3 || warrior->status == 5 || warrior->status == 6)
    {
        file.writeIdBoolean("Ejected", 1);
    }

    file.writeIdBoolean("NotMineYet", warrior->notMineYet);
    file.writeIdLong("DescIndex", warrior->descIndex);
    file.writeIdLong("NameIndex", warrior->nameIndex);
    file.writeBlock("PersonalityTraits");
    file.writeIdChar("Professionalism", warrior->professionalism);
    file.writeIdChar("Decorum", warrior->decorum);
    file.writeIdChar("Aggressiveness", static_cast<char>(warrior->getAggressiveness(1)));
    file.writeIdChar("Courage", warrior->courage);
    static constexpr const char* SkillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
    file.writeBlock("Skills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], static_cast<char>(static_cast<int32_t>(warrior->skillRank[i])));
    }

    file.writeBlock("OriginalSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], warrior->originalSkills[i]);
    }

    file.writeBlock("LatestSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], warrior->latestSkills[i]);
    }

    file.writeBlock("SkillPoints");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdFloat(SkillNames[i], warrior->skillPoints[i]);
    }

    file.writeBlock("Status");
    file.writeIdChar("Wounds", static_cast<char>(static_cast<int32_t>(warrior->wounds)));
    file.close();
    return 0;
}

auto MissionLogisticsBridge::logisticsStartingFitWriter(char* fileName, int skipFlagged) -> int32_t
{
    Logistics* logistics = mission->logistics;
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Planet");
    file.writeIdLong("Setting", CurPlanet);
    file.writeBlock("General");
    file.writeIdString("PurchaseFile", globalLogPtr->purchaseFile);
    file.writeBlock("ResourcePoints");
    file.writeIdULong("numPoints", static_cast<uint32_t>(ResourcePoints));

    uint32_t numWarriors = 0;
    uint32_t numAssWarriors = 0;
    result = writeWarriors(this, file, numWarriors, numAssWarriors);

    if (result != 0)
    {
        return result;
    }

    // The spare mechs.
    file.writeBlock("Mechs");
    uint32_t numMechs = 0;
    LogMech* mech = logistics->mechList->mechs;

    for (int32_t i = logistics->mechList->numMechs; i > 0; i--, mech = mech->next)
    {
        if (mech->assigned == 0)
        {
            ++numMechs;
        }
    }

    file.writeIdULong("NumMechs", numMechs);
    int32_t index = 0;
    mech = logistics->mechList->mechs;

    for (int32_t i = 0; i < logistics->mechList->numMechs; i++, mech = mech->next)
    {
        if (mech->assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssWarriors + numWarriors;
        writePacketBlock(file, "Mech%d", index, packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsMechProfileWriter(name, mech, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // The force's mechs (without the flagged ones when asked, and without the ones not the player's yet).
    auto forceMechWritten = [skipFlagged](LogMech* part)
    { return part->assigned != 0 && (part->deployed == 0 || skipFlagged == 0) && part->notMineYet == 0; };
    file.writeBlock("AssMechs");
    uint32_t numAssMechs = 0;
    mech = logistics->forceMechList->mechs;

    for (int32_t i = logistics->forceMechList->numMechs; i > 0; i--, mech = mech->next)
    {
        if (forceMechWritten(mech))
        {
            ++numAssMechs;
        }
    }

    file.writeIdULong("NumAssMechs", numAssMechs);
    index = 0;
    mech = logistics->forceMechList->mechs;

    for (int32_t i = 0; i < logistics->forceMechList->numMechs; i++, mech = mech->next)
    {
        if (!forceMechWritten(mech))
        {
            continue;
        }

        const uint32_t packet = index + numMechs + numAssWarriors + numWarriors;
        writePacketBlock(file, "Mech%d", static_cast<int32_t>(index + numMechs), packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsMechProfileWriter(name, mech, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // The vehicles not deployed: the spare ones, then the force's.
    file.writeBlock("Vehicles");
    uint32_t numVehicles = 0;
    LogVehicle* vehicle = logistics->vehicleList->vehicles;

    for (int32_t i = logistics->vehicleList->numVehicles; i > 0; i--, vehicle = vehicle->next)
    {
        if (vehicle->assigned == 0 && vehicle->deployed == 0)
        {
            ++numVehicles;
        }
    }

    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = logistics->forceVehicleList->numVehicles; i > 0; i--, vehicle = vehicle->next)
    {
        if (vehicle->assigned != 0 && vehicle->deployed == 0)
        {
            ++numVehicles;
        }
    }

    file.writeIdULong("NumVehicles", numVehicles);
    index = 0;
    auto writeVehicle = [&](LogVehicle* part) -> int32_t
    {
        const uint32_t packet = index + numAssMechs + numMechs + numAssWarriors + numWarriors;
        writePacketBlock(file, "Vehicle%d", index, packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        const int32_t written = logisticsVehicleProfileWriter(name, part, 0);

        if (written == 0)
        {
            ++index;
        }

        return written;
    };

    vehicle = logistics->vehicleList->vehicles;

    for (int32_t i = 0; i < logistics->vehicleList->numVehicles; i++, vehicle = vehicle->next)
    {
        if (vehicle->assigned != 0 || vehicle->deployed != 0)
        {
            continue;
        }

        result = writeVehicle(vehicle);

        if (result != 0)
        {
            return result;
        }
    }

    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = 0; i < logistics->forceVehicleList->numVehicles; i++, vehicle = vehicle->next)
    {
        if (vehicle->assigned == 0 || vehicle->deployed != 0)
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
    file.writeBlock("AssVehicles");
    uint32_t numAssVehicles = 0;
    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = logistics->forceVehicleList->numVehicles; i > 0; i--, vehicle = vehicle->next)
    {
        if (vehicle->assigned != 0 && vehicle->deployed != 0 && vehicle->notMineYet == 0)
        {
            ++numAssVehicles;
        }
    }

    file.writeIdULong("NumAssVehicles", numAssVehicles);
    index = 0;
    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = 0; i < logistics->forceVehicleList->numVehicles; i++, vehicle = vehicle->next)
    {
        if (vehicle->assigned == 0 || vehicle->deployed == 0 || vehicle->notMineYet != 0)
        {
            continue;
        }

        const uint32_t packet = index + numVehicles + numAssMechs + numMechs + numAssWarriors + numWarriors;
        writePacketBlock(file, "Vehicle%d", static_cast<int32_t>(index + numVehicles), packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsVehicleProfileWriter(name, vehicle, 0);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    writeComponents(file, [logistics](uint8_t id) { return logistics->componentInventory->getItemCount(id); });
    file.close();

    FullPathFileName purchaseName;
    purchaseName.init(savePath, fileName, ".pur");
    result = writePurchaseFile(purchaseName);

    if (result != 0)
    {
        return result;
    }

    // The starting fit, the profiles and the purchase file go into one packet file.
    FullPathFileName packName;
    packName.init(savePath, fileName, ".pkk");
    PacketFile packFile;
    packFile.create(packName);
    const auto numProfiles =
        static_cast<int32_t>(numAssVehicles + numVehicles + numAssMechs + numMechs + numAssWarriors + numWarriors);
    packFile.reserve(numProfiles + 2, 1);
    File source;
    result = packFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = packProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    result = packFileInto(packFile, source, purchaseName, numProfiles + 1);

    if (result != 0)
    {
        return result;
    }

    packFile.close();
    deleteFile(fitName);
    deleteFile(purchaseName);
    // Original behaviour: only the first vehicles + mechs + warriors profiles are deleted; the rest are left in
    // the temp folder (and overwritten next time).
    deleteProfiles(static_cast<int32_t>(numVehicles + numMechs + numWarriors));
    return 0;
}

auto MissionLogisticsBridge::logisticsMechProfileWriter(char* fileName, LogMech* mech, int writeRequired) -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Header");
    file.writeIdString("FileType", "MechProfile");
    file.writeBlock("General");
    file.writeIdString("MechType", mech->fileName);
    file.writeIdString("Name", mech->mechName);
    file.writeIdFloat("CurTonnage", mech->curTonnage);
    file.writeIdString("icon", mech->iconName);
    file.writeIdChar("Status", mech->status);
    file.writeIdULong("Chassis", mech->chassis);
    file.writeIdLong("NameIndex", mech->nameIndex);
    file.writeIdLong("NameVariant", mech->nameVariant);
    file.writeIdBoolean("Assigned", mech->assigned);
    file.writeIdBoolean("NotMineYet", mech->notMineYet);
    int32_t pilot = mech->pilotIndex;

    if (pilot == -8 && mech->notMineYet == 0)
    {
        pilot = -1;
    }

    file.writeIdLong("Pilot", pilot);

    if (writeRequired != 0)
    {
        file.writeIdBoolean("Required", mech->required);
    }

    file.writeIdLong("DescIndex", mech->descIndex);
    file.writeBlock("Engine");
    file.writeIdFloat("Tonnage", mech->engineTonnage);
    file.writeIdULong("Rating", mech->engineRating);
    file.writeIdUChar("MaxRunSpeed", mech->maxRunSpeed);
    file.writeBlock("Armor");
    file.writeIdUChar("Type", mech->armorType);
    file.writeIdFloat("Tonnage", mech->armorTonnage);
    file.writeBlock("MaxArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.writeIdUChar(MechArmorNames[i], mech->armor[i].maxArmor);
    }

    file.writeBlock("CurArmorPoints");

    for (int32_t i = 0; i < 11; i++)
    {
        file.writeIdUChar(MechArmorNames[i], mech->armor[i].curArmor);
    }

    // The critical slots are rebuilt from the inventory as it is written: other equipment, weapons, then ammo.
    mech->numOther = 0;
    mech->numWeapons = 0;
    mech->numAmmo = 0;

    for (auto& location : mech->itemSlots)
    {
        for (auto& slot : location)
        {
            slot = {0xff, 0, 0xff};
        }
    }

    char block[32];
    int32_t item = 0;

    for (_LogInventoryItem* entry = mech->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        const uint8_t masterID = entry->masterID;
        const int32_t form = MasterComponentList[masterID].form;

        if (isWeaponForm(form) || form == 10)
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            mech->placeItem(masterID, item, stat->hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", masterID);
            stat->itemNum = item;
            ++item;
            ++mech->numOther;
        }
    }

    for (_LogInventoryItem* entry = mech->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        const uint8_t masterID = entry->masterID;

        if (!isWeaponForm(MasterComponentList[masterID].form))
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            mech->placeItem(masterID, item, stat->hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", masterID);
            stat->itemNum = item;
            file.writeIdUChar("FacesForward", mech->inventory->getFacing(static_cast<uint8_t>(item)));
            ++item;
            ++mech->numWeapons;
        }
    }

    for (_LogInventoryItem* entry = mech->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        const uint8_t masterID = entry->masterID;

        if (MasterComponentList[masterID].form != 10)
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            mech->placeItem(masterID, item, stat->hits);
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", masterID);
            file.writeIdLong("Amount", -1);
            stat->itemNum = item;
            ++item;
            ++mech->numAmmo;
        }
    }

    file.writeBlock("InventoryInfo");
    file.writeIdUChar("NumOther", mech->numOther);
    file.writeIdUChar("NumWeapons", mech->numWeapons);
    file.writeIdUChar("NumAmmo", mech->numAmmo);

    for (int32_t location = 0; location < 8; location++)
    {
        file.writeBlock(MechLocationNames[location]);
        file.writeIdUChar("CASE", static_cast<uint8_t>(mech->hasCASE[location]));
        file.writeIdUChar("CurInternalStructure", mech->internals[location].curArmor);
        file.writeIdUChar("HotSpotNumber", mech->hotSpotNumber[location]);

        for (int32_t space = 0; space < NumLocationCriticalSpaces[location]; space++)
        {
            char id[32];
            std::snprintf(id, sizeof(id), "Component:%d", space);
            const LogMech::ItemSlot& slot = mech->itemSlots[location][space];
            const uint8_t values[2] = {slot.row, slot.column};
            file.writeIdUCharArray(id, values, 2);
        }
    }

    std::strncpy(mech->profileName, fileName, 9);
    file.close();
    return 0;
}

auto MissionLogisticsBridge::logisticsVehicleProfileWriter(char* fileName, LogVehicle* vehicle, int writeRequired)
    -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Header");
    file.writeIdString("FileType", "GroundVehicleProfile");
    file.writeBlock("General");
    file.writeIdString("Name", vehicle->fileName);
    file.writeIdFloat("CurTonnage", vehicle->curTonnage);
    file.writeIdLong("NameIndex", vehicle->nameIndex);
    file.writeIdString("icon", vehicle->iconName);
    file.writeIdChar("Status", vehicle->status);
    file.writeIdString("Crew", vehicle->crew);
    file.writeIdULong("Chassis", vehicle->chassis);
    file.writeIdBoolean("Assigned", vehicle->assigned);
    file.writeIdBoolean("NotMineYet", vehicle->notMineYet);
    file.writeIdBoolean("Deployed", vehicle->deployed);

    if (writeRequired != 0)
    {
        file.writeIdBoolean("Required", vehicle->required);
    }

    file.writeIdLong("DescIndex", vehicle->descIndex);
    file.writeBlock("Engine");
    file.writeIdFloat("Tonnage", vehicle->engineTonnage);
    file.writeIdULong("Rating", vehicle->engineRating);
    file.writeIdUChar("MaxMoveSpeed", vehicle->maxMoveSpeed);
    file.writeBlock("Armor");
    file.writeIdUChar("Type", vehicle->armorType);
    file.writeIdFloat("Tonnage", vehicle->armorTonnage);

    for (int32_t location = 0; location < 5; location++)
    {
        file.writeBlock(VehicleLocationNames[location]);
        file.writeIdUChar("CurInternalStructure", vehicle->curInternalStructure[location]);
        file.writeIdUChar("MaxArmorPoints", vehicle->maxArmorPoints[location]);
        file.writeIdUChar("CurArmorPoints", vehicle->curArmorPoints[location]);
    }

    file.writeBlock("InventoryInfo");
    file.writeIdUChar("NumOther", vehicle->numOther);
    file.writeIdUChar("NumWeapons", vehicle->numWeapons);
    file.writeIdUChar("NumAmmo", vehicle->numAmmo);

    char block[32];
    int32_t item = 0;

    for (_LogInventoryItem* entry = vehicle->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        const int32_t form = MasterComponentList[entry->masterID].form;

        if (isWeaponForm(form) || form == 10)
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", entry->masterID);
            ++item;
        }
    }

    for (_LogInventoryItem* entry = vehicle->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        if (!isWeaponForm(MasterComponentList[entry->masterID].form))
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", entry->masterID);
            file.writeIdUChar("FacesForward", stat->facing);
            ++item;
        }
    }

    for (_LogInventoryItem* entry = vehicle->inventory->getItemInfo(0); entry != nullptr; entry = entry->next)
    {
        if (MasterComponentList[entry->masterID].form != 10)
        {
            continue;
        }

        for (_LogInventoryStat* stat = entry->stats; stat != nullptr; stat = stat->next)
        {
            std::snprintf(block, sizeof(block), "Item:%d", item);
            file.writeBlock(block);
            file.writeIdUChar("MasterID", entry->masterID);
            file.writeIdLong("Amount", -1);
            ++item;
        }
    }

    std::strncpy(vehicle->profileName, fileName, 9);
    file.close();
    return 0;
}

auto MissionLogisticsBridge::logisticsWarriorProfileWriter(char* fileName, LogWarrior* warrior) -> int32_t
{
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    const int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("General");
    file.writeIdString("Name", warrior->name);
    file.writeIdString("Callsign", warrior->callsign);
    file.writeIdLong("paintScheme", warrior->paintScheme);
    file.writeIdString("pilotAudio", warrior->pilotAudio);
    file.writeIdString("pilotVideo", warrior->pilotVideo);
    file.writeIdString("Picture", warrior->picture);
    file.writeIdString("Brain", warrior->brain);
    file.writeIdBoolean("Assigned", warrior->assigned);
    file.writeIdBoolean("NotMineYet", warrior->notMineYet);
    file.writeIdLong("NameIndex", warrior->nameIndex);
    file.writeIdLong("DescIndex", warrior->descIndex);
    file.writeBlock("PersonalityTraits");
    file.writeIdChar("Professionalism", warrior->personality[0]);
    file.writeIdChar("Decorum", warrior->personality[1]);
    file.writeIdChar("Aggressiveness", warrior->personality[2]);
    file.writeIdChar("Courage", warrior->personality[3]);
    static constexpr const char* SkillNames[4] = {"Piloting", "Jumping", "Sensors", "Gunnery"};
    file.writeBlock("Skills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], warrior->skills[i]);
    }

    file.writeBlock("OriginalSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], warrior->originalSkills[i]);
    }

    file.writeBlock("LatestSkills");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdChar(SkillNames[i], warrior->startingSkills[i]);
    }

    file.writeBlock("SkillPoints");

    for (int32_t i = 0; i < 4; i++)
    {
        file.writeIdFloat(SkillNames[i], warrior->skillPoints[i]);
    }

    file.writeBlock("Status");
    file.writeIdChar("Wounds", static_cast<char>(static_cast<int32_t>(warrior->wounds)));
    std::strncpy(warrior->fileName, fileName, 9);
    file.close();
    return 0;
}

auto MissionLogisticsBridge::logisticsStartingFitReader(char*) -> int32_t
{
    return 0;
}

auto MissionLogisticsBridge::logisticsMechProfileReader(char*) -> int32_t
{
    return 0;
}

auto MissionLogisticsBridge::logisticsVehicleProfileReader(char*) -> int32_t
{
    return 0;
}

auto MissionLogisticsBridge::logisticsWarriorProfileReader(char*) -> int32_t
{
    return 0;
}

auto MissionLogisticsBridge::logisticsSaveGame(char* fileName) -> int32_t
{
    Logistics* logistics = mission->logistics;
    FullPathFileName fitName;
    fitName.init(saveTempPath, fileName, ".fit");
    FitIniFile file;
    int32_t result = file.create(fitName);

    if (result != 0)
    {
        return result;
    }

    file.writeBlock("Planet");
    file.writeIdLong("Setting", CurPlanet);
    file.writeBlock("General");
    file.writeIdString("PurchaseFile", CurPlanet == 0 ? "purchase" : "xpur");
    file.writeIdLong("MissionNumber", logistics->currentMission);
    file.writeIdFloat("LastScenarioTime", totalScenarioTime);
    file.writeIdFloat("LastLogisticsTime", totalLogisticsTime);
    file.writeBlock("ResourcePoints");
    file.writeIdULong("numPoints", static_cast<uint32_t>(ResourcePoints));

    uint32_t numWarriors = 0;
    uint32_t numAssWarriors = 0;
    result = writeWarriors(this, file, numWarriors, numAssWarriors);

    if (result != 0)
    {
        return result;
    }

    // Every mech: the spare ones, then the force.
    file.writeBlock("Mechs");
    uint32_t numMechs = 0;
    LogMech* mech = logistics->mechList->mechs;

    for (int32_t i = logistics->mechList->numMechs; i > 0; i--, mech = mech->next)
    {
        if (mech->assigned == 0)
        {
            ++numMechs;
        }
    }

    file.writeIdULong("NumMechs", numMechs);
    int32_t index = 0;
    mech = logistics->mechList->mechs;

    for (int32_t i = 0; i < logistics->mechList->numMechs; i++, mech = mech->next)
    {
        if (mech->assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssWarriors + numWarriors;
        writePacketBlock(file, "Mech%d", index, packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    file.writeBlock("AssMechs");
    uint32_t numAssMechs = 0;
    mech = logistics->forceMechList->mechs;

    for (int32_t i = logistics->forceMechList->numMechs; i > 0; i--, mech = mech->next)
    {
        if (mech->assigned != 0)
        {
            ++numAssMechs;
        }
    }

    file.writeIdULong("NumAssMechs", numAssMechs);
    index = 0;
    mech = logistics->forceMechList->mechs;

    for (int32_t i = 0; i < logistics->forceMechList->numMechs; i++, mech = mech->next)
    {
        if (mech->assigned == 0)
        {
            continue;
        }

        const uint32_t packet = index + numMechs + numAssWarriors + numWarriors;
        writePacketBlock(file, "Mech%d", static_cast<int32_t>(index + numMechs), packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsMechProfileWriter(name, mech, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    // Every vehicle: the spare ones, then the force.
    file.writeBlock("Vehicles");
    uint32_t numVehicles = 0;
    LogVehicle* vehicle = logistics->vehicleList->vehicles;

    for (int32_t i = logistics->vehicleList->numVehicles; i > 0; i--, vehicle = vehicle->next)
    {
        if (vehicle->assigned == 0)
        {
            ++numVehicles;
        }
    }

    file.writeIdULong("NumVehicles", numVehicles);
    index = 0;
    vehicle = logistics->vehicleList->vehicles;

    for (int32_t i = 0; i < logistics->vehicleList->numVehicles; i++, vehicle = vehicle->next)
    {
        if (vehicle->assigned != 0)
        {
            continue;
        }

        const uint32_t packet = index + numAssMechs + numMechs + numAssWarriors + numWarriors;
        writePacketBlock(file, "Vehicle%d", index, packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsVehicleProfileWriter(name, vehicle, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    file.writeBlock("AssVehicles");
    uint32_t numAssVehicles = 0;
    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = logistics->forceVehicleList->numVehicles; i > 0; i--, vehicle = vehicle->next)
    {
        if (vehicle->assigned != 0)
        {
            ++numAssVehicles;
        }
    }

    file.writeIdULong("NumAssVehicles", numAssVehicles);
    index = 0;
    vehicle = logistics->forceVehicleList->vehicles;

    for (int32_t i = 0; i < logistics->forceVehicleList->numVehicles; i++, vehicle = vehicle->next)
    {
        if (vehicle->assigned == 0)
        {
            continue;
        }

        const uint32_t packet = numVehicles + numWarriors + numAssWarriors + numMechs + numAssMechs + index;
        writePacketBlock(file, "Vehicle%d", static_cast<int32_t>(numVehicles + index), packet);
        char name[32];
        profileName(name, sizeof(name), static_cast<int32_t>(packet));
        result = logisticsVehicleProfileWriter(name, vehicle, 1);

        if (result != 0)
        {
            return result;
        }

        ++index;
    }

    writeComponents(file, [logistics](uint8_t id) { return logistics->componentInventory->getItemCount(id); });
    file.close();

    FullPathFileName purchaseName;
    purchaseName.init(savePath, fileName, ".pur");
    result = writePurchaseFile(purchaseName);

    if (result != 0)
    {
        return result;
    }

    FullPathFileName saveName;
    saveName.init(savePath, fileName, ".sav");
    PacketFile packFile;
    result = packFile.create(saveName);

    if (result != 0)
    {
        return result;
    }

    const auto numProfiles =
        static_cast<int32_t>(numVehicles + numWarriors + numAssWarriors + numMechs + numAssMechs + numAssVehicles);
    packFile.reserve(numProfiles + 2, 1);
    File source;
    result = packFileInto(packFile, source, fitName, 0);

    if (result != 0)
    {
        return result;
    }

    result = packProfiles(packFile, source, numProfiles);

    if (result != 0)
    {
        return result;
    }

    result = packFileInto(packFile, source, purchaseName, numProfiles + 1);

    if (result != 0)
    {
        return result;
    }

    packFile.close();
    deleteFile(fitName);
    deleteFile(purchaseName);
    deleteProfiles(numProfiles);
    return 0;
}

auto MissionLogisticsBridge::logisticsLoadGame(char*) -> int32_t
{
    return 0;
}

auto MissionLogisticsBridge::missionToLogisticsBridgeSave(char*) -> int32_t
{
    return 0;
}
