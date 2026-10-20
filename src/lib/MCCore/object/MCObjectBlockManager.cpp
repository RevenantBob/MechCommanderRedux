#include "stdafx.h"
#include "object/MCObjectBlockManager.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
#include "main/MCGamePaths.h"
#include "network/MCMultiPlayer.h"
#include "object/MCObjectSystem.h"
#include "object/MCSensorSystem.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCGate.h"
#include "object/MCGateType.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCTerrainObject.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"
#include "sprite/MCVfxAppearance.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>The block packet size of the old object data the game refuses.</summary>
    constexpr uint32_t OldStyleObjectDataSize = 0x898;
    /// <summary>The object type of misc terrain objects (bridges, walls, forests) in the .bdg file.</summary>
    constexpr int32_t MiscTerrainObjectType = 0x1c0;
    /// <summary>Size of one .bdg record: block, vertex, kind and destroyed, four 32-bit values.</summary>
    constexpr size_t MiscTerrainRecordSize = 16;

    /// <summary>Whether <paramref name="block"/> and <paramref name="vertex"/> (from a map file) name a vertex of the map.</summary>
    bool IsMapVertex(int64_t block, int64_t vertex)
    {
        const int64_t blocks = static_cast<int64_t>(MCTerrain::BlocksMapSide) * MCTerrain::BlocksMapSide;
        const int64_t vertices = static_cast<int64_t>(MCTerrain::VerticesBlockSide) * MCTerrain::VerticesBlockSide;
        return block >= 0 && block < blocks && vertex >= 0 && vertex < vertices;
    }

    /// <summary>Nothing, or which .bdg record names a block or vertex off the map.</summary>
    std::expected<void, std::string> CheckMiscTerrainRecords(std::span<const uint8_t> records)
    {
        for (size_t offset = 0; offset + MiscTerrainRecordSize <= records.size(); offset += MiscTerrainRecordSize)
        {
            std::array<int32_t, 2> place{};
            std::memcpy(place.data(), records.data() + offset, sizeof(place));

            if (!IsMapVertex(place[0], place[1]))
            {
                return std::unexpected(std::format("terrain object {} is at block {}, vertex {}, off the map",
                                                   offset / MiscTerrainRecordSize, place[0], place[1]));
            }
        }

        return {};
    }

    /// <summary>A placed-destroyed object's damage: its type's DmgLevel.</summary>
    template <typename T> float DestroyedDamage(MCGameObject& object)
    {
        return static_cast<float>(static_cast<int32_t>(object.TypeAs<T>().DmgLevel));
    }

    /// <summary>The <paramref name="index"/>th object of <paramref name="list"/>, or null past its end.</summary>
    std::shared_ptr<MCBaseObject> ObjectAt(const MCObjectList& list, size_t index)
    {
        for (std::shared_ptr<MCBaseObject> object : list)
        {
            if (index-- == 0)
            {
                return object;
            }
        }

        return nullptr;
    }

    /// <summary>Updates every object of the list, finding each by its index from the start.</summary>
    void UpdateListObjects(const MCObjectList& list)
    {
        const size_t count = list.Size();

        for (size_t i = 0; i < count; i++)
        {
            if (const std::shared_ptr<MCBaseObject> object = ObjectAt(list, i))
            {
                object->Update();
            }
        }
    }
}

MCObjectBlockManager::MCObjectBlockManager(Key)
{
}

MCObjectBlockManager::~MCObjectBlockManager()
{
    for (const std::weak_ptr<MCObjectList>& block : _BlockLists)
    {
        if (const std::shared_ptr<MCObjectList> list = block.lock(); list != nullptr && ObjectList() != nullptr)
        {
            ObjectList()->DeleteList(*list);
        }
    }
}

auto MCObjectBlockManager::Create(std::string_view fileName)
    -> std::expected<std::unique_ptr<MCObjectBlockManager>, std::string>
{
    auto manager = std::make_unique<MCObjectBlockManager>(Key{});
    manager->_ObjectFile = std::make_unique<MCPacketFile>();
    const std::string objPath = GamePath(TerrainPath, fileName, ".obj");

    if (const int32_t result = manager->_ObjectFile->Open(objPath); result != 0)
    {
        return std::unexpected(std::format("could not open {} ({:#x})", objPath, static_cast<uint32_t>(result)));
    }

    manager->_ObjectFile->SeekPacket(0);

    if (static_cast<uint32_t>(manager->_ObjectFile->GetPacketSize()) == OldStyleObjectDataSize)
    {
        Fatal(-1, " Tried to use old Style Object Data ");
    }

    const int32_t mapSide = MCTerrain::BlocksMapSide * MCTerrain::VerticesBlockSide;
    manager->_VertexObjectCount.assign(static_cast<size_t>(mapSide) * static_cast<size_t>(mapSide), 0);

    if (std::expected<void, std::string> loaded = manager->LoadBlocks(); !loaded)
    {
        return std::unexpected(std::format("{}: {}", objPath, loaded.error()));
    }

    MCFile bdgFile;

    if (bdgFile.Open(GamePath(TerrainPath, fileName, ".bdg")) == 0)
    {
        // Issue #25: the count and each record's block and vertex come from the file; a count the file doesn't hold
        // (or a negative one) asked for a huge allocation, and a block or vertex off the map wrote past the vertex
        // counts. Such a file is refused.
        const int32_t numRecords = bdgFile.ReadLong();
        const uint32_t bdgLength = bdgFile.GetLength();

        if (numRecords < 0 || bdgLength < sizeof(int32_t) ||
            static_cast<uint64_t>(numRecords) * MiscTerrainRecordSize > bdgLength - sizeof(int32_t))
        {
            return std::unexpected(
                std::format("{}.bdg holds {} bytes, not {} terrain objects", fileName, bdgLength, numRecords));
        }

        std::vector<uint8_t> records(static_cast<size_t>(numRecords) * MiscTerrainRecordSize);

        if (!records.empty())
        {
            bdgFile.Read(records);

            if (const std::expected<void, std::string> checked = CheckMiscTerrainRecords(records); !checked)
            {
                return std::unexpected(std::format("{}.bdg: {}", fileName, checked.error()));
            }

            manager->LoadMiscTerrainObjects(records);
        }

        bdgFile.Close();
        manager->_VertexObjectCount = {};
    }

    return manager;
}

auto MCObjectBlockManager::NextPartId(int32_t blockNumber, int32_t vertexNumber) -> int32_t
{
    const int32_t blocksMapSide = MCTerrain::BlocksMapSide;
    const int32_t verticesBlockSide = MCTerrain::VerticesBlockSide;
    const int32_t index =
        (((blockNumber % blocksMapSide) * verticesBlockSide + vertexNumber % verticesBlockSide) * blocksMapSide +
         blockNumber / blocksMapSide) *
            verticesBlockSide +
        vertexNumber / verticesBlockSide;
    const int32_t partId =
        static_cast<int8_t>(_VertexObjectCount[index]) + 0x1000 + (blockNumber * 400 + vertexNumber) * 8;
    const auto count = static_cast<int8_t>(_VertexObjectCount[index] + 1);
    _VertexObjectCount[index] = static_cast<uint8_t>(count);

    if (7 < count)
    {
        _VertexObjectCount[index] = 7;
    }

    return partId;
}

auto MCObjectBlockManager::LoadMiscTerrainObjects(std::span<const uint8_t> records) -> void
{
    for (size_t offset = 0; offset + MiscTerrainRecordSize <= records.size(); offset += MiscTerrainRecordSize)
    {
        std::array<int32_t, 4> record{};
        std::memcpy(record.data(), records.data() + offset, MiscTerrainRecordSize);
        const int32_t blockNumber = record[0];
        const int32_t vertexNumber = record[1];
        const int32_t kind = record[2];
        std::shared_ptr<MCMiscTerrainObject> object = CreateObjectAs<MCMiscTerrainObject>(MiscTerrainObjectType);
        object->VertexNumber = vertexNumber;
        object->BlockNumber = blockNumber;
        object->Kind = static_cast<MCMiscTerrainKind>(kind);
        object->SetPartId(NextPartId(blockNumber, vertexNumber));

        if (record[3] != 0)
        {
            // Placed already destroyed: damaged one point past its kind's level.
            const auto& type = object->TypeAs<MCMiscTerrainObjectType>();
            uint32_t level = 0;
            bool known = true;

            switch (kind)
            {
                case 5:
                {
                    level = type.BridgeDmgLevel;
                    break;
                }
                case 6:
                {
                    level = type.ForestDmgLevel;
                    break;
                }
                case 7:
                {
                    level = type.WallDmgLevel;
                    break;
                }
                case 8:
                {
                    level = type.MediumWallDmgLevel;
                    break;
                }
                case 9:
                {
                    level = type.LightWallDmgLevel;
                    break;
                }
                default:
                {
                    known = false;
                    break;
                }
            }

            if (known)
            {
                object->Destroyed = true;
                object->Damage = static_cast<float>(static_cast<int32_t>(level + 1));
                object->OverlayDestroyed = true;
            }
        }

        // Light walls go in the block's TBlk list, the rest in its RBlk list.
        const std::shared_ptr<MCObjectList> list =
            ObjectList()->FindList(std::format("{}{}", kind == 9 ? "TBlk" : "RBlk", blockNumber));

        if (list == nullptr)
        {
            Fatal(-1, "objectLists are SNAFU");
        }

        list->Add(std::move(object));
    }
}

auto MCObjectBlockManager::SetupObjectQueue(uint32_t blockNumber, std::span<const uint8_t> packet)
    -> std::expected<void, std::string>
{
    // The block's two lists.
    const std::shared_ptr<MCObjectList> treeList = ObjectList()->AddList(
        std::make_unique<MCObjectList>(std::format("TBlk{}", blockNumber), static_cast<int32_t>(blockNumber)));
    const std::shared_ptr<MCObjectList> restList = ObjectList()->AddList(
        std::make_unique<MCObjectList>(std::format("RBlk{}", blockNumber), static_cast<int32_t>(blockNumber)));
    _BlockLists[blockNumber * 2] = treeList;
    _BlockLists[blockNumber * 2 + 1] = restList;

    for (size_t offset = 0; offset + sizeof(MCObjData) <= packet.size(); offset += sizeof(MCObjData))
    {
        MCObjData record;
        std::memcpy(&record, packet.data() + offset, sizeof(MCObjData));
        const int16_t typeId = record.ObjTypeNum;

        // Types the game never places from the map.
        if (typeId == 0x3c || typeId == 0xfb || typeId == 0x209 || typeId == 0x20a || typeId == 0x262 ||
            typeId == 0x263 || typeId == 0x264)
        {
            continue;
        }

        const auto vertexNumber = static_cast<uint16_t>(record.VertexNumber);
        const auto recordBlock = static_cast<uint16_t>(record.BlockNumber);

        // Issue #25: a vertex off the map wrote past the vertex counts; the original was fatal on a type it doesn't
        // have. Such a map is refused.
        if (!IsMapVertex(recordBlock, vertexNumber))
        {
            return std::unexpected(std::format("object block {}: an object at block {}, vertex {}, off the map",
                                               blockNumber, recordBlock, vertexNumber));
        }

        std::shared_ptr<MCGameObject> object = CreateObject(typeId);

        if (object == nullptr)
        {
            return std::unexpected(std::format("object block {}: object type {} doesn't exist", blockNumber, typeId));
        }

        MCVector2D offsetOnVertex(static_cast<float>(record.PixelOffsetX), static_cast<float>(record.PixelOffsetY));
        MCVector2D numbers(static_cast<float>(vertexNumber), static_cast<float>(recordBlock));
        object->SetPartId(NextPartId(recordBlock, vertexNumber));
        object->SetTerrainPosition(offsetOnVertex, numbers);

        // Placed already destroyed: damaged to its type's level.
        const MCObjectClass objectClass = object->ObjectClass;
        const bool destroyed = (record.Damage & 0xf) != 0;

        switch (objectClass)
        {
            case MCObjectClass::Building:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCBuildingType>(*object));
                }

                static_cast<MCBuilding*>(object.get())->TileNum = static_cast<uint8_t>(record.Damage >> 4);
                object->SetTerrainPosition(offsetOnVertex, numbers);
                break;
            }
            case MCObjectClass::TerrainObject:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCTerrainObjectType>(*object));
                }
                break;
            }
            case MCObjectClass::Turret:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCTurretType>(*object));
                }
                break;
            }
            case MCObjectClass::TreeBuilding:
            {
                if (destroyed)
                {
                    auto& building = static_cast<MCTreeBuilding&>(*object);
                    object->SetDamage(DestroyedDamage<MCTreeBuildingType>(*object));
                    building.HitOnce = true;
                    building.Collapsed = true;
                    building.CollisionsOn = 0;
                    building.Status = 2;

                    if (building.SensorSystem.lock() != nullptr)
                    {
                        building.SensorSystem.lock()->Disable();
                    }

                    building.Appearance->SetTypeId(MCActorState::FallenDamaged, 0xff);
                }
                break;
            }
            case MCObjectClass::Gate:
            {
                if (destroyed)
                {
                    object->SetDamage(DestroyedDamage<MCGateType>(*object));
                    static_cast<MCGate*>(object.get())->DestroyGate(true);
                }
                break;
            }
            default:
                break;
        }

        // Turrets and gates go in the first object list (and turrets on the multiplayer roster); trees in TBlk,
        // everything else in RBlk.
        if (objectClass == MCObjectClass::Turret || objectClass == MCObjectClass::Gate)
        {
            std::shared_ptr<MCGameObject> placed = AddToDefaultList(std::move(object));

            if (MultiPlayer() != nullptr && objectClass == MCObjectClass::Turret)
            {
                MultiPlayer()->AddToTurretRoster(std::static_pointer_cast<MCTurret>(placed));
            }
        }
        else
        {
            (objectClass == MCObjectClass::Tree ? treeList : restList)->Add(std::move(object));
        }
    }

    return {};
}

auto MCObjectBlockManager::LoadBlocks() -> std::expected<void, std::string>
{
    if (_ObjectFile == nullptr || _ObjectFile->IsOpen() == 0)
    {
        return {};
    }

    const int32_t numPackets = _ObjectFile->GetNumPackets();
    _BlockLists.assign(static_cast<size_t>(numPackets) * 2, {});

    for (int32_t block = 0; block < numPackets; block++)
    {
        if (_ObjectFile->SeekPacket(block) != 0)
        {
            continue;
        }

        // The original read every packet into one 22000-byte buffer.
        std::vector<uint8_t> packet(static_cast<size_t>(_ObjectFile->GetPacketSize()), 0xff);
        _ObjectFile->ReadPacket(block, packet);

        if (std::expected<void, std::string> queued = SetupObjectQueue(static_cast<uint32_t>(block), packet); !queued)
        {
            return queued;
        }
    }

    return {};
}

auto MCObjectBlockManager::UpdateAllObjects() -> void
{
    for (const std::weak_ptr<MCObjectList>& block : _BlockLists)
    {
        if (const std::shared_ptr<MCObjectList> list = block.lock())
        {
            UpdateListObjects(*list);
        }
    }
}
