#include "stdafx.h"
#include "object/MCObjectTypeManager.h"
#include "appear/MCAppearanceTypeList.h"
#include "lib/MCFatal.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "object/MCBaseObject.h"
#include "object/MCObjectType.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCBullet.h"
#include "object/MCBulletType.h"
#include "object/MCDebris.h"
#include "object/MCDebrisType.h"
#include "object/MCElemental.h"
#include "object/MCElementalType.h"
#include "object/MCElementalGameSystem.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCFire.h"
#include "object/MCFireType.h"
#include "object/MCEffectSystem.h"
#include "object/MCGate.h"
#include "object/MCGateType.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCJet.h"
#include "object/MCJetType.h"
#include "object/MCLaser.h"
#include "object/MCLaserType.h"
#include "object/MCLight.h"
#include "object/MCLightType.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCProjectileLaser.h"
#include "object/MCProjectileLaserType.h"
#include "object/MCSmoke.h"
#include "object/MCSmokeType.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCTerrainObject.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCTrain.h"
#include "object/MCTrainCar.h"
#include "object/MCTrainCarType.h"
#include "object/MCTrainManager.h"
#include "object/MCTree.h"
#include "object/MCTreeType.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"

char ObjectPath[80] = "data\\objects\\";

namespace
{
    /// <summary>A new, empty type of the class an "ObjectClass" block numbers; null for 0xd, which has none.</summary>
    std::unique_ptr<MCObjectType> NewType(int32_t objectClassNum)
    {
        switch (objectClassNum)
        {
            case 0:
                return std::make_unique<MCTreeType>();
            case 1:
                return std::make_unique<MCBuildingType>();
            case 2:
                return std::make_unique<MCBattleMechType>();
            case 3:
                return std::make_unique<MCGroundVehicleType>();
            case 4:
                return std::make_unique<MCExplosionType>();
            case 5:
                return std::make_unique<MCFireType>();
            case 6:
                return std::make_unique<MCLaserType>();
            case 7:
                return std::make_unique<MCSmokeType>();
            case 8:
                return std::make_unique<MCBulletType>();
            case 9:
                return std::make_unique<MCDebrisType>();
            case 0xb:
                return std::make_unique<MCTerrainObjectType>();
            case 0xc:
                return std::make_unique<MCArtilleryType>();
            case 0xd:
                // Port fix: the original returns the type number as a pointer (and writes through it for a kept type).
                return nullptr;
            case 0xe:
                return std::make_unique<MCElementalType>();
            case 0xf:
                return std::make_unique<MCMiscTerrainObjectType>();
            case 0x10:
                return std::make_unique<MCJetType>();
            case 0x11:
                return std::make_unique<MCProjectileLaserType>();
            case 0x12:
                return std::make_unique<MCTreeBuildingType>();
            case 0x13:
                return std::make_unique<MCCameraDroneType>();
            case 0x14:
                return std::make_unique<MCTrainCarType>();
            case 0x15:
                return std::make_unique<MCTurretType>();
            case 0x16:
                return std::make_unique<MCGateType>();
            case 0x17:
                return std::make_unique<MCLightType>();
            case -1:
                Fatal(static_cast<int32_t>(0xbeef0001));
            default:
                Fatal(static_cast<int32_t>(0xbeef0003));
        }
    }
}

MCObjectTypeManager::MCObjectTypeManager() = default;

MCObjectTypeManager::~MCObjectTypeManager()
{
    // The original's heap went with the types still loaded (kept or still used) without running their destructors;
    // the port deletes them, first to last, then frees the blocks nothing freed.
    while (!_Types.empty())
    {
        std::unique_ptr<MCObjectType> type = std::move(_Types.front());
        _Types.erase(_Types.begin());
    }

    TypeData.Clear();
    ObjectData.Clear();
}

auto MCObjectTypeManager::Create(std::string_view objectFileName)
    -> std::expected<std::unique_ptr<MCObjectTypeManager>, std::string>
{
    auto manager = std::make_unique<MCObjectTypeManager>();
    manager->_ObjectFile = std::make_unique<MCPacketFile>();
    const std::string fileName = GamePath(ObjectPath, objectFileName, ".pak");

    if (const int32_t result = manager->_ObjectFile->Open(fileName); result != 0)
    {
        return std::unexpected(std::format("could not open {} ({:#x})", fileName, static_cast<uint32_t>(result)));
    }

    return manager;
}

auto MCObjectTypeManager::Add(std::unique_ptr<MCObjectType> objType) -> MCObjectType*
{
    return _Types.emplace_back(std::move(objType)).get();
}

auto MCObjectTypeManager::Remove(MCObjectType* objType) -> void
{
    const auto position = std::ranges::find(_Types, objType, &std::unique_ptr<MCObjectType>::get);

    if (position == _Types.end())
    {
        return;
    }

    objType->NumUsers--;

    if (objType->NumUsers < 1 && objType->KeepMe == 0)
    {
        // Out of the list before it is deleted.
        std::unique_ptr<MCObjectType> type = std::move(*position);
        _Types.erase(position);
    }
}

auto MCObjectTypeManager::Find(int32_t objTypeNum) const -> MCObjectType*
{
    for (const std::unique_ptr<MCObjectType>& type : _Types)
    {
        if (type->ObjTypeNum == objTypeNum)
        {
            return type.get();
        }
    }

    return nullptr;
}

auto MCObjectTypeManager::Get(int32_t objTypeNum) -> std::unique_ptr<MCBaseObject>
{
    MCObjectType* objType = Find(objTypeNum);

    if (objType == nullptr)
    {
        objType = Load(objTypeNum, 0);
    }

    if (objType == nullptr)
    {
        return nullptr;
    }

    std::unique_ptr<MCBaseObject> object = objType->CreateInstance();

    if (object == nullptr)
    {
        return nullptr;
    }

    objType->NumUsers++;
    return object;
}

auto MCObjectTypeManager::Load(int32_t objTypeNum, int keepMe) -> MCObjectType*
{
    // Original behaviour: a type already loaded isn't returned; the caller gets null.
    if (objTypeNum < 1 || Find(objTypeNum) != nullptr || _ObjectFile->SeekPacket(objTypeNum) != 0)
    {
        return nullptr;
    }

    // The packet's "ObjectClass" block names the class to make.
    int32_t objectClassNum = -1;
    {
        MCFitIniFile classFile;

        if (classFile.Open(_ObjectFile.get(), static_cast<uint32_t>(_ObjectFile->GetPacketSize())) != 0 ||
            classFile.SeekBlock("ObjectClass") != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        const MCFitResult<int32_t> classNum = classFile.Read<int32_t>("ObjectTypeNum");

        if (!classNum)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        objectClassNum = *classNum;
        classFile.Close();
    }

    _ObjectFile->SeekPacket(objTypeNum);
    std::unique_ptr<MCObjectType> objType = NewType(objectClassNum);

    if (objType == nullptr)
    {
        return nullptr;
    }

    if (objType->Init(_ObjectFile.get(), static_cast<uint32_t>(_ObjectFile->GetPacketSize())) != 0)
    {
        Fatal(static_cast<int32_t>(0xbeef0006));
    }

    if (keepMe != 0)
    {
        // A kept type preloads its appearance and explosion.
        objType->KeepMe = 1;
        AppearanceTypeList()->GetAppearance(objType->AppearName);
        Load(objType->ExplosionObject, 0);
    }

    objType->ObjTypeNum = objTypeNum;
    return Add(std::move(objType));
}
