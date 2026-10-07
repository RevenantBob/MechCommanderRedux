#include "stdafx.h"
#include "object/objblck.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
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
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"

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
    /// .bdg file is read (a global at 0x007e3708 in the original, with no symbol).
    /// </summary>
    std::vector<uint8_t> VertexObjectCount;

    /// <summary>
    /// The next part id for an object on (<paramref name="blockNumber"/>, <paramref name="vertexNumber"/>): the
    /// vertex's base id plus how many are already on it, which is then counted (up to 7).
    /// </summary>
    int32_t NextPartId(int32_t blockNumber, int32_t vertexNumber)
    {
        const int32_t blocksMapSide = MCTerrain::BlocksMapSide;
        const int32_t verticesBlockSide = MCTerrain::VerticesBlockSide;
        const int32_t index =
            (((blockNumber % blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide) * blocksMapSide +
             blockNumber / blocksMapSide) *
                verticesBlockSide +
            vertexNumber / verticesBlockSide;
        const int32_t partId =
            static_cast<int8_t>(VertexObjectCount[index]) + 0x1000 + (blockNumber * 400 + vertexNumber) * 8;
        const auto count = static_cast<int8_t>(VertexObjectCount[index] + 1);
        VertexObjectCount[index] = static_cast<uint8_t>(count);

        if (7 < count)
        {
            VertexObjectCount[index] = 7;
        }

        return partId;
    }

    /// <summary>A placed-destroyed object's damage: its type's DmgLevel (every such type keeps it at +0x30).</summary>
    template <typename T> float DestroyedDamage(MCGameObject* object)
    {
        return static_cast<float>(static_cast<int32_t>(static_cast<T*>(object->GetObjectType())->DmgLevel));
    }

    /// <summary>Unlinks <paramref name="node"/> from objectList and destroys it.</summary>
    void RemoveList(MCObjectQueueNode* node)
    {
        MCObjectQueueNode* previous = nullptr;

        for (MCObjectQueueNode* list = ObjectList->Head; list != nullptr; list = list->Next)
        {
            if (list == node)
            {
                if (list == ObjectList->Head)
                {
                    ObjectList->Head = list->Next;
                }

                if (list == ObjectList->Tail)
                {
                    ObjectList->Tail = previous;
                }

                if (previous != nullptr)
                {
                    previous->Next = list->Next;
                }
                break;
            }

            previous = list;
        }

        node->Destroy();
        delete node;
    }

    /// <summary>Updates every object of the list, finding each by its index from the head.</summary>
    void UpdateListObjects(MCObjectQueueNode* list)
    {
        int32_t count = 0;

        for (MCBaseObject* object = list->Head; object != nullptr; object = object->Next)
        {
            count++;
        }

        for (int32_t i = 0; i < count; i++)
        {
            MCBaseObject* object = nullptr;
            int32_t skip = i;

            do
            {
                object = object == nullptr ? list->Head : object->Next;
            } while (object != nullptr && 0 < skip--);

            object->Update();
        }
    }
} // namespace

auto MCObjectBlockManager::Destroy() -> void
{
    DestroyAllObjects();
    ObjectLists.reset();
    ObjectData.reset();

    if (ObjectFile != nullptr)
    {
        ObjectFile->Close();
        delete ObjectFile;
        ObjectFile = nullptr;
    }
}

auto MCObjectBlockManager::Init(const char* fileName) -> int32_t
{
    ObjectFile = new MCPacketFile;

    if (ObjectFile == nullptr)
    {
        return static_cast<int32_t>(0xbaaa0014);
    }

    std::string objPath;
    objPath = GamePath(TerrainPath, fileName, ".obj");
    int32_t result = ObjectFile->Open(objPath);

    if (result != 0)
    {
        return result;
    }

    ObjectFile->SeekPacket(0);

    if (ObjectFile->GetPacketSize() == 0x898)
    {
        Fatal(-1, " Tried to use old Style Object Data ");
    }

    // The lists table and the block packet buffer (heapSize is the original's heap size, kept for reference).
    const int32_t numPackets = ObjectFile->GetNumPackets();
    HeapSize = numPackets * 0x18 + 0x5600;
    ObjectLists = std::make_unique<MCObjectQueueNode*[]>(static_cast<size_t>(numPackets) * 2);
    ObjectData = std::make_unique<uint8_t[]>(OBJECT_DATA_SIZE);
    *reinterpret_cast<int32_t*>(ObjectData.get()) = -1;
    const int32_t mapSide = MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide;
    const auto countSize = static_cast<size_t>(mapSide) * static_cast<size_t>(mapSide);
    VertexObjectCount.assign(countSize, 0);

    if ((result = Update(1)) != 0)
    {
        return result;
    }

    // The misc terrain objects: 16-byte records of block, vertex, kind and whether it starts destroyed.
    std::string bdgPath;
    bdgPath = GamePath(TerrainPath, fileName, ".bdg");
    MCFile bdgFile;

    if (bdgFile.Open(bdgPath) == 0)
    {
        const int32_t numRecords = bdgFile.ReadLong();

        if (numRecords != 0)
        {
            auto* records = new uint8_t[static_cast<size_t>(numRecords) << 4];

            if (records == nullptr)
            {
                return static_cast<int32_t>(0xbaaa0018);
            }

            bdgFile.Read(records, numRecords << 4);
            const auto* record = reinterpret_cast<const int32_t*>(records);

            for (int32_t i = 0; i < numRecords; i++, record += 4)
            {
                const int32_t blockNumber = record[0];
                const int32_t vertexNumber = record[1];
                const int32_t kind = record[2];
                auto* object = static_cast<MCMiscTerrainObject*>(CreateObject(MISC_TERRAIN_OBJECT_TYPE));
                object->VertexNumber = vertexNumber;
                object->BlockNumber = blockNumber;
                object->TerrainObjectKind = kind;
                object->SetPartId(NextPartId(blockNumber, vertexNumber));

                if (record[3] != 0)
                {
                    // Placed already destroyed: damaged one point past its kind's level.
                    const auto* type = static_cast<MCMiscTerrainObjectType*>(object->ObjType);
                    uint32_t level = 0;
                    bool known = true;

                    switch (kind)
                    {
                        case 5:
                            level = type->BridgeDmgLevel;
                            break;
                        case 6:
                            level = type->ForestDmgLevel;
                            break;
                        case 7:
                            level = type->WallDmgLevel;
                            break;
                        case 8:
                            level = type->MediumWallDmgLevel;
                            break;
                        case 9:
                            level = type->LightWallDmgLevel;
                            break;
                        default:
                            known = false;
                            break;
                    }

                    if (known)
                    {
                        object->Destroyed = 1;
                        object->Damage = static_cast<float>(static_cast<int32_t>(level + 1));
                        object->OverlayDestroyed = 1;
                    }
                }

                // Light walls go in the block's TBlk list, the rest in its RBlk list.
                char listName[12];
                std::sprintf(listName, kind == 9 ? "TBlk%d" : "RBlk%d", blockNumber);
                MCObjectQueueNode* list = ObjectList->Head;

                while (list != nullptr && list->operator==(listName) == 0)
                {
                    list = list->Next;
                }

                if (list == nullptr)
                {
                    Fatal(-1, "objectLists are SNAFU");
                }

                list->AddNode(object);
            }

            delete[] records;
        }

        bdgFile.Close();
        VertexObjectCount = {};
    }

    return 0;
}

auto MCObjectBlockManager::SetupObjectQueue(uint32_t listIndex, uint32_t packetSize) -> int32_t
{
    if (ObjectLists[listIndex] != nullptr || ObjectLists[listIndex + 1] != nullptr)
    {
        return static_cast<int32_t>(0xbaaa001d);
    }

    // The block's two lists.
    auto* treeList = new MCObjectQueueNode;
    ObjectLists[listIndex] = treeList;
    auto* restList = new MCObjectQueueNode;
    ObjectLists[listIndex + 1] = restList;
    const uint32_t blockNumber = listIndex >> 1;
    char listName[20];
    std::sprintf(listName, "TBlk%d", blockNumber);
    treeList->Init(listName, static_cast<int32_t>(blockNumber));
    ObjectList->AddList(treeList);
    std::sprintf(listName, "RBlk%d", blockNumber);
    restList->Init(listName, static_cast<int32_t>(blockNumber));
    ObjectList->AddList(restList);

    // Each record: type, pixel offset, vertex, block, then damage (low nibble) and commander (high nibble).
    const uint8_t* record = ObjectData.get();

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

        MCGameObject* object = CreateObject(typeId);

        if (object == nullptr)
        {
            Fatal(typeId, " This object number is BAD ");
        }

        MCVector2D offset(static_cast<float>(offsetX), static_cast<float>(offsetY));
        MCVector2D numbers(static_cast<float>(vertexNumber), static_cast<float>(recordBlock));
        object->SetPartId(NextPartId(recordBlock, vertexNumber));
        object->SetTerrainPosition(offset, numbers);

        // Placed already destroyed: damaged to its type's level.
        const int32_t objectClass = object->ObjectClass;
        const bool destroyed = (flags & 0xf) != 0;
        bool isTree = false;

        switch (objectClass)
        {
            case BUILDING:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCBuildingType>(object));
                }

                static_cast<MCBuilding*>(object)->TileNum = static_cast<uint8_t>(flags >> 4);
                object->SetTerrainPosition(offset, numbers);
                break;
            }
            case TREE:
                isTree = true;
                break;
            case TERRAINOBJECT:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCTerrainObjectType>(object));
                }
                break;
            }
            case TURRET:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCTurretType>(object));
                }
                break;
            }
            case TREEBUILDING:
            {
                if (destroyed)
                {
                    auto* building = static_cast<MCTreeBuilding*>(object);
                    object->SetDamage(DestroyedDamage<MCTreeBuildingType>(object));
                    building->HitOnce = 1;
                    building->Collapsed = 1;
                    building->CollisionsOn = 0;
                    building->Status = 2;

                    if (building->SensorSystem != nullptr)
                    {
                        building->SensorSystem->Disable();
                    }

                    static_cast<MCVfxAppearance*>(building->Appearance)->SetTypeId(static_cast<MCActorState>(5), 0xff);
                }
                break;
            }
            case GATE:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCGateType>(object));
                    static_cast<MCGate*>(object)->DestroyGate(1);
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
            if (ObjectList->Head != nullptr)
            {
                ObjectList->Head->AddNode(object);
            }

            if (MPlayer != nullptr && objectClass == TURRET)
            {
                MPlayer->AddToTurretRoster(static_cast<MCTurret*>(object));
            }
        }
        else
        {
            MCObjectQueueNode* list = isTree ? ObjectLists[listIndex] : ObjectLists[listIndex + 1];

            if (list != nullptr)
            {
                list->AddNode(object);
            }
        }
    }

    return 0;
}

auto MCObjectBlockManager::Update(int reload) -> int32_t
{
    if (reload == 0)
    {
        return 0;
    }

    if (ObjectData == nullptr)
    {
        ObjectData = std::make_unique<uint8_t[]>(OBJECT_DATA_SIZE);
    }

    std::memset(ObjectData.get(), 0xff, OBJECT_DATA_SIZE);

    if (ObjectFile == nullptr || ObjectFile->IsOpen() == 0)
    {
        return 0;
    }

    const int32_t numPackets = ObjectFile->GetNumPackets();

    for (int32_t listIndex = 0; listIndex < numPackets * 2; listIndex += 2)
    {
        if (ObjectFile->SeekPacket(listIndex / 2) != 0)
        {
            continue;
        }

        ObjectFile->ReadPacket(listIndex / 2, ObjectData.get());
        const int32_t result =
            SetupObjectQueue(static_cast<uint32_t>(listIndex), static_cast<uint32_t>(ObjectFile->GetPacketSize()));

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto MCObjectBlockManager::UpdateAllObjects() -> void
{
    for (int32_t listIndex = 0; listIndex < ObjectFile->GetNumPackets() * 2; listIndex += 2)
    {
        if (ObjectLists[listIndex] != nullptr)
        {
            UpdateListObjects(ObjectLists[listIndex]);
        }

        if (ObjectLists[listIndex + 1] != nullptr)
        {
            UpdateListObjects(ObjectLists[listIndex + 1]);
        }
    }
}

auto MCObjectBlockManager::DestroyAllObjects() -> void
{
    for (int32_t listIndex = 0; listIndex < ObjectFile->GetNumPackets() * 2; listIndex += 2)
    {
        if (ObjectLists[listIndex] != nullptr)
        {
            RemoveList(ObjectLists[listIndex]);
            ObjectLists[listIndex] = nullptr;
        }

        if (ObjectLists[listIndex + 1] != nullptr)
        {
            RemoveList(ObjectLists[listIndex + 1]);
            ObjectLists[listIndex + 1] = nullptr;
        }
    }
}
