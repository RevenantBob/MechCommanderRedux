#include "stdafx.h"
#include "object/objblck.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/contact.h"
#include "object/gate.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/tbldng.h"
#include "object/terrobj.h"
#include "object/turret.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"

namespace
{
    /// <summary>Size of the block packet buffer.</summary>
    constexpr uint32_t OBJECT_DATA_SIZE = 22000;
    /// <summary>Size of one terrain object record in a block packet.</summary>
    constexpr uint32_t OBJECT_RECORD_SIZE = 11;
    /// <summary>The object type of misc terrain objects (bridges, walls, forests) in the .bdg file.</summary>
    constexpr int32_t MISC_TERRAIN_OBJECT_TYPE = 0x1c0;

    /// <summary>
    /// Objects placed on each map vertex so far (up to 7), for their part ids; made by init and freed once the
    /// .bdg file is read (unnamed global DAT_007e3708).
    /// </summary>
    std::vector<uint8_t> vertexObjectCount;

    /// <summary>
    /// The next part id for an object on (<paramref name="blockNumber"/>, <paramref name="vertexNumber"/>): the
    /// vertex's base id plus how many are already on it, which is then counted (up to 7).
    /// </summary>
    int32_t nextPartId(int32_t blockNumber, int32_t vertexNumber)
    {
        const int32_t blocksMapSide = Terrain::blocksMapSide;
        const int32_t verticesBlockSide = Terrain::verticesBlockSide;
        const int32_t index =
            (((blockNumber % blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide) * blocksMapSide +
             blockNumber / blocksMapSide) *
                verticesBlockSide +
            vertexNumber / verticesBlockSide;
        const int32_t partId =
            static_cast<int8_t>(vertexObjectCount[index]) + 0x1000 + (blockNumber * 400 + vertexNumber) * 8;
        const auto count = static_cast<int8_t>(vertexObjectCount[index] + 1);
        vertexObjectCount[index] = static_cast<uint8_t>(count);

        if (7 < count)
        {
            vertexObjectCount[index] = 7;
        }

        return partId;
    }

    /// <summary>A placed-destroyed object's damage: its type's DmgLevel (every such type keeps it at +0x30).</summary>
    template <typename T> float destroyedDamage(GameObject* object)
    {
        return static_cast<float>(static_cast<int32_t>(static_cast<T*>(object->getObjectType())->dmgLevel));
    }

    /// <summary>Unlinks <paramref name="node"/> from objectList and destroys it.</summary>
    void removeList(ObjectQueueNode* node)
    {
        ObjectQueueNode* previous = nullptr;

        for (ObjectQueueNode* list = objectList->head; list != nullptr; list = list->next)
        {
            if (list == node)
            {
                if (list == objectList->head)
                {
                    objectList->head = list->next;
                }

                if (list == objectList->tail)
                {
                    objectList->tail = previous;
                }

                if (previous != nullptr)
                {
                    previous->next = list->next;
                }
                break;
            }

            previous = list;
        }

        node->destroy();
        delete node;
    }

    /// <summary>Updates every object of the list, finding each by its index from the head.</summary>
    void updateListObjects(ObjectQueueNode* list)
    {
        int32_t count = 0;

        for (BaseObject* object = list->head; object != nullptr; object = object->next)
        {
            count++;
        }

        for (int32_t i = 0; i < count; i++)
        {
            BaseObject* object = nullptr;
            int32_t skip = i;

            do
            {
                object = object == nullptr ? list->head : object->next;
            } while (object != nullptr && 0 < skip--);

            object->update();
        }
    }
} // namespace

auto ObjectBlockManager::destroy() -> void
{
    destroyAllObjects();
    objectLists.reset();
    objectData.reset();

    if (objectFile != nullptr)
    {
        objectFile->close();
        delete objectFile;
        objectFile = nullptr;
    }
}

auto ObjectBlockManager::init(const char* fileName) -> int32_t
{
    objectFile = new PacketFile;

    if (objectFile == nullptr)
    {
        return static_cast<int32_t>(0xbaaa0014);
    }

    FullPathFileName objPath;
    objPath.init(terrainPath, fileName, ".obj");
    int32_t result = objectFile->open(objPath, READ, 50);

    if (result != 0)
    {
        return result;
    }

    objectFile->seekPacket(0);

    if (objectFile->getPacketSize() == 0x898)
    {
        Fatal(-1, " Tried to use old Style Object Data ");
    }

    // The lists table and the block packet buffer (heapSize is the original's heap size, kept for reference).
    const int32_t numPackets = objectFile->getNumPackets();
    heapSize = numPackets * 0x18 + 0x5600;
    objectLists = std::make_unique<ObjectQueueNode*[]>(static_cast<size_t>(numPackets) * 2);
    objectData = std::make_unique<uint8_t[]>(OBJECT_DATA_SIZE);
    *reinterpret_cast<int32_t*>(objectData.get()) = -1;
    const int32_t mapSide = Terrain::blocksMapSide * Terrain::verticesBlockSide;
    const auto countSize = static_cast<size_t>(mapSide) * static_cast<size_t>(mapSide);
    vertexObjectCount.assign(countSize, 0);

    if ((result = update(1)) != 0)
    {
        return result;
    }

    // The misc terrain objects: 16-byte records of block, vertex, kind and whether it starts destroyed.
    FullPathFileName bdgPath;
    bdgPath.init(terrainPath, fileName, ".bdg");
    File bdgFile;

    if (bdgFile.open(bdgPath, READ, 50) == 0)
    {
        const int32_t numRecords = bdgFile.readLong();

        if (numRecords != 0)
        {
            auto* records = new uint8_t[static_cast<size_t>(numRecords) << 4];

            if (records == nullptr)
            {
                return static_cast<int32_t>(0xbaaa0018);
            }

            bdgFile.read(records, numRecords << 4);
            const auto* record = reinterpret_cast<const int32_t*>(records);

            for (int32_t i = 0; i < numRecords; i++, record += 4)
            {
                const int32_t blockNumber = record[0];
                const int32_t vertexNumber = record[1];
                const int32_t kind = record[2];
                auto* object = static_cast<MiscTerrainObject*>(createObject(MISC_TERRAIN_OBJECT_TYPE));
                object->vertexNumber = vertexNumber;
                object->blockNumber = blockNumber;
                object->terrainObjectKind = kind;
                object->setPartId(nextPartId(blockNumber, vertexNumber));

                if (record[3] != 0)
                {
                    // Placed already destroyed: damaged one point past its kind's level.
                    const auto* type = static_cast<MiscTerrainObjectType*>(object->objType);
                    uint32_t level = 0;
                    bool known = true;

                    switch (kind)
                    {
                        case 5:
                            level = type->bridgeDmgLevel;
                            break;
                        case 6:
                            level = type->forestDmgLevel;
                            break;
                        case 7:
                            level = type->wallDmgLevel;
                            break;
                        case 8:
                            level = type->mediumWallDmgLevel;
                            break;
                        case 9:
                            level = type->lightWallDmgLevel;
                            break;
                        default:
                            known = false;
                            break;
                    }

                    if (known)
                    {
                        object->destroyed = 1;
                        object->damage = static_cast<float>(static_cast<int32_t>(level + 1));
                        object->overlayDestroyed = 1;
                    }
                }

                // Light walls go in the block's TBlk list, the rest in its RBlk list.
                char listName[12];
                std::sprintf(listName, kind == 9 ? "TBlk%d" : "RBlk%d", blockNumber);
                ObjectQueueNode* list = objectList->head;

                while (list != nullptr && list->operator==(listName) == 0)
                {
                    list = list->next;
                }

                if (list == nullptr)
                {
                    Fatal(-1, "objectLists are SNAFU");
                }

                list->addNode(object);
            }

            delete[] records;
        }

        bdgFile.close();
        vertexObjectCount = {};
    }

    return 0;
}

auto ObjectBlockManager::setupObjectQueue(uint32_t listIndex, uint32_t packetSize) -> int32_t
{
    if (objectLists[listIndex] != nullptr || objectLists[listIndex + 1] != nullptr)
    {
        return static_cast<int32_t>(0xbaaa001d);
    }

    // The block's two lists.
    auto* treeList = new ObjectQueueNode;
    objectLists[listIndex] = treeList;
    auto* restList = new ObjectQueueNode;
    objectLists[listIndex + 1] = restList;
    const uint32_t blockNumber = listIndex >> 1;
    char listName[20];
    std::sprintf(listName, "TBlk%d", blockNumber);
    treeList->init(listName, static_cast<int32_t>(blockNumber));
    objectList->addList(treeList);
    std::sprintf(listName, "RBlk%d", blockNumber);
    restList->init(listName, static_cast<int32_t>(blockNumber));
    objectList->addList(restList);

    // Each record: type, pixel offset, vertex, block, then damage (low nibble) and commander (high nibble).
    const uint8_t* record = objectData.get();

    for (uint32_t n = packetSize / OBJECT_RECORD_SIZE; n != 0; n--, record += OBJECT_RECORD_SIZE)
    {
        int16_t typeId;
        int16_t offsetX;
        int16_t offsetY;
        uint16_t vertexNumber;
        uint16_t recordBlock;
        std::memcpy(&typeId, record, 2);
        std::memcpy(&offsetX, record + 2, 2);
        std::memcpy(&offsetY, record + 4, 2);
        std::memcpy(&vertexNumber, record + 6, 2);
        std::memcpy(&recordBlock, record + 8, 2);
        const uint8_t flags = record[10];

        // Types the game never places from the map.
        if (typeId == 0x3c || typeId == 0xfb || typeId == 0x209 || typeId == 0x20a || typeId == 0x262 ||
            typeId == 0x263 || typeId == 0x264)
        {
            continue;
        }

        GameObject* object = createObject(typeId);

        if (object == nullptr)
        {
            Fatal(typeId, " This object number is BAD ");
        }

        vector_2d offset(static_cast<float>(offsetX), static_cast<float>(offsetY));
        vector_2d numbers(static_cast<float>(vertexNumber), static_cast<float>(recordBlock));
        object->setPartId(nextPartId(recordBlock, vertexNumber));
        object->setTerrainPosition(offset, numbers);

        // Placed already destroyed: damaged to its type's level.
        const int32_t objectClass = object->objectClass;
        const bool destroyed = (flags & 0xf) != 0;
        bool isTree = false;

        switch (objectClass)
        {
            case BUILDING:
            {
                if (destroyed)
                {
                    object->setDamage(destroyedDamage<BuildingType>(object));
                }

                static_cast<Building*>(object)->tileNum = static_cast<uint8_t>(flags >> 4);
                object->setTerrainPosition(offset, numbers);
                break;
            }
            case TREE:
                isTree = true;
                break;
            case TERRAINOBJECT:
            {
                if (destroyed)
                {
                    object->setDamage(destroyedDamage<TerrainObjectType>(object));
                }
                break;
            }
            case TURRET:
            {
                if (destroyed)
                {
                    object->setDamage(destroyedDamage<TurretType>(object));
                }
                break;
            }
            case TREEBUILDING:
            {
                if (destroyed)
                {
                    auto* building = static_cast<TreeBuilding*>(object);
                    object->setDamage(destroyedDamage<TreeBuildingType>(object));
                    building->hitOnce = 1;
                    building->collapsed = 1;
                    building->collisionsOn = 0;
                    building->status = 2;

                    if (building->sensorSystem != nullptr)
                    {
                        building->sensorSystem->disable();
                    }

                    static_cast<VFXAppearance*>(building->appearance)->setTypeId(static_cast<ActorState>(5), 0xff);
                }
                break;
            }
            case GATE:
            {
                if (destroyed)
                {
                    object->setDamage(destroyedDamage<GateType>(object));
                    static_cast<Gate*>(object)->destroyGate(1);
                }
                break;
            }
            default:
                break;
        }

        // Turrets and gates go in the first object list (and turrets on the multiplayer roster); trees in TBlk,
        // everything else in RBlk.
        if (objectClass == TURRET || objectClass == GATE)
        {
            if (objectList->head != nullptr)
            {
                objectList->head->addNode(object);
            }

            if (MPlayer != nullptr && objectClass == TURRET)
            {
                MPlayer->addToTurretRoster(static_cast<Turret*>(object));
            }
        }
        else
        {
            ObjectQueueNode* list = isTree ? objectLists[listIndex] : objectLists[listIndex + 1];

            if (list != nullptr)
            {
                list->addNode(object);
            }
        }
    }

    return 0;
}

auto ObjectBlockManager::update(int reload) -> int32_t
{
    if (reload == 0)
    {
        return 0;
    }

    if (objectData == nullptr)
    {
        objectData = std::make_unique<uint8_t[]>(OBJECT_DATA_SIZE);
    }

    std::memset(objectData.get(), 0xff, OBJECT_DATA_SIZE);

    if (objectFile == nullptr || objectFile->isOpen() == 0)
    {
        return 0;
    }

    const int32_t numPackets = objectFile->getNumPackets();

    for (int32_t listIndex = 0; listIndex < numPackets * 2; listIndex += 2)
    {
        if (objectFile->seekPacket(listIndex / 2) != 0)
        {
            continue;
        }

        objectFile->readPacket(listIndex / 2, objectData.get());
        const int32_t result =
            setupObjectQueue(static_cast<uint32_t>(listIndex), static_cast<uint32_t>(objectFile->getPacketSize()));

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto ObjectBlockManager::updateAllObjects() -> void
{
    for (int32_t listIndex = 0; listIndex < objectFile->getNumPackets() * 2; listIndex += 2)
    {
        if (objectLists[listIndex] != nullptr)
        {
            updateListObjects(objectLists[listIndex]);
        }

        if (objectLists[listIndex + 1] != nullptr)
        {
            updateListObjects(objectLists[listIndex + 1]);
        }
    }
}

auto ObjectBlockManager::destroyAllObjects() -> void
{
    for (int32_t listIndex = 0; listIndex < objectFile->getNumPackets() * 2; listIndex += 2)
    {
        if (objectLists[listIndex] != nullptr)
        {
            removeList(objectLists[listIndex]);
            objectLists[listIndex] = nullptr;
        }

        if (objectLists[listIndex + 1] != nullptr)
        {
            removeList(objectLists[listIndex + 1]);
            objectLists[listIndex + 1] = nullptr;
        }
    }
}
