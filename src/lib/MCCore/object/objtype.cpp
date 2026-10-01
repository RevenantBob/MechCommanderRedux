#include "stdafx.h"
#include "object/objtype.h"
#include "appear/apprtype.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "object/artlry.h"
#include "object/baseobj.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/bullet.h"
#include "object/debris.h"
#include "object/elemntl.h"
#include "object/explode.h"
#include "object/fire.h"
#include "object/gameobj.h"
#include "object/gate.h"
#include "object/gvehicl.h"
#include "object/jet.h"
#include "object/laser.h"
#include "object/light.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/prjlase.h"
#include "object/smoke.h"
#include "object/tbldng.h"
#include "object/terrobj.h"
#include "object/train.h"
#include "object/tree.h"
#include "object/turret.h"
#include "sprite/bactor.h"

uint32_t NextIdNumber = 0x30000001;
char objectPath[80] = "data\\objects\\";
PacketFile* ObjectTypeManager::objectFile = nullptr;
UserHeap* ObjectTypeManager::objectTypeCache = nullptr;
UserHeap* ObjectTypeManager::objectCache = nullptr;

//---------------------------------------------------------------------------
// ObjectType
//---------------------------------------------------------------------------

auto ObjectType::operator new(size_t size) noexcept -> void*
{
    return ObjectTypeManager::objectTypeCache->malloc(static_cast<uint32_t>(size));
}

auto ObjectType::operator delete(void* ptr) -> void
{
    // A type still in use stays allocated.
    if (static_cast<ObjectType*>(ptr)->numUsers < 1)
    {
        ObjectTypeManager::objectTypeCache->free(ptr);
    }
}

auto ObjectType::createInstance() -> BaseObject*
{
    auto* object = new BaseObject;
    object->init(this);
    object->idNumber = NextIdNumber++;
    return object;
}

auto ObjectType::init(FitIniFile* typeFile) -> int32_t
{
    int32_t result = typeFile->seekBlock("ObjectType");

    if (result != 0)
    {
        return result;
    }

    numUsers = 0;
    result = typeFile->readIdLong("Type", typeClass);

    if (result != 0)
    {
        return result;
    }

    int32_t appearance = 0;
    result = typeFile->readIdLong("Appearance", appearance);
    appearName = static_cast<uint32_t>(appearance);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->readIdLong("ExplosionObject", explosionObject);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->readIdLong("DestroyedObject", destroyedObject);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->readIdFloat("ExtentRadius", extentRadius);

    if (result != 0)
    {
        return result;
    }

    if (typeFile->readIdLong("KeepMe", keepMe) != 0)
    {
        keepMe = 0;
    }

    if (typeFile->readIdLong("IconNumber", iconNumber) != 0)
    {
        iconNumber = -1;
    }

    if (typeFile->readIdLong("Alignment", teamId) != 0)
    {
        teamId = 0;
    }

    return 0;
}

auto ObjectType::createExplosion(vector_3d& position, float damage, float radius) -> void
{
    if (explosionObject == -1)
    {
        return;
    }

    GameObject* explosion = createObject(explosionObject);

    if (explosion == nullptr)
    {
        return;
    }

    explosion->setPosition(position);

    if (radius != 0.0)
    {
        explosion->setExplRad(radius);
        explosion->setExplDmg(damage);
    }

    if (objectList->head != nullptr)
    {
        objectList->head->addNode(explosion);
    }
}

auto ObjectType::handleDestruction(GameObject* collidee, GameObject*) -> int
{
    if (explosionObject != -1)
    {
        vector_3d position = collidee->getPosition();
        createExplosion(position, 0.0f, 0.0f);
    }

    return 1;
}

//---------------------------------------------------------------------------
// ObjectTypeManager
//---------------------------------------------------------------------------

auto ObjectTypeManager::init(char* objectFileName, int32_t objectTypeCacheSize, int32_t objectCacheSize) -> int32_t
{
    FullPathFileName fileName;
    fileName.init(objectPath, objectFileName, ".pak");

    objectFile = new PacketFile;

    if (objectFile == nullptr)
    {
        return static_cast<int32_t>(0xbeef0008);
    }

    int32_t result = objectFile->open(fileName, READ, 50);

    if (result != 0)
    {
        return result;
    }

    objectTypeCache = new UserHeap;

    if (objectTypeCache == nullptr)
    {
        return static_cast<int32_t>(0xbeef0009);
    }

    result = objectTypeCache->init(static_cast<uint32_t>(objectTypeCacheSize), "ObjectTypeHeap");

    if (result != 0)
    {
        return result;
    }

    objectCache = new UserHeap;

    if (objectCache == nullptr)
    {
        return static_cast<int32_t>(0xbeef000a);
    }

    result = objectCache->init(static_cast<uint32_t>(objectCacheSize), "ObjectHeap");

    if (result != 0)
    {
        return result;
    }

    return 0;
}

auto ObjectTypeManager::destroy() -> void
{
    if (objectFile != nullptr)
    {
        objectFile->close();
    }

    delete objectFile;
    objectFile = nullptr;
    delete objectTypeCache;
    objectTypeCache = nullptr;
    delete objectCache;
    objectCache = nullptr;
}

auto ObjectTypeManager::add(ObjectType* objType) -> void
{
    auto* node = new ObjectTypeNode;
    node->objType = objType;
    AddToTail(node);
}

auto ObjectTypeManager::remove(int32_t index) -> void
{
    Link* link = nullptr;
    int more = Traverse(link);
    int32_t i = 0;

    while (more != 0 && i < index)
    {
        more = Traverse(link);
        i++;
    }

    if (link != nullptr)
    {
        remove(static_cast<ObjectTypeNode*>(link));
    }
}

auto ObjectTypeManager::remove(ObjectType* objType) -> void
{
    Link* link = nullptr;
    int more = Traverse(link);

    while (more != 0 && static_cast<ObjectTypeNode*>(link)->objType != objType)
    {
        more = Traverse(link);
    }

    if (link != nullptr)
    {
        remove(static_cast<ObjectTypeNode*>(link));
    }
}

auto ObjectTypeManager::remove(ObjectTypeNode* node) -> void
{
    if (node == nullptr)
    {
        return;
    }

    ObjectType* objType = node->objType;
    objType->numUsers--;

    if (objType->numUsers < 1 && objType->keepMe == 0)
    {
        delete objType;
        Destroy(node);
    }
}

auto ObjectTypeManager::find(int32_t objTypeNum) -> ObjectType*
{
    Link* link = nullptr;

    for (int more = Traverse(link); more != 0; more = Traverse(link))
    {
        ObjectType* objType = static_cast<ObjectTypeNode*>(link)->objType;

        if (objType->objTypeNum == objTypeNum)
        {
            return objType;
        }
    }

    return nullptr;
}

auto ObjectTypeManager::element(int32_t index) -> ObjectType*
{
    Link* link = nullptr;
    int32_t i = 0;

    for (int more = Traverse(link); more != 0; more = Traverse(link))
    {
        if (i == index)
        {
            return static_cast<ObjectTypeNode*>(link)->objType;
        }

        i++;
    }

    return nullptr;
}

auto ObjectTypeManager::get(int32_t objTypeNum) -> BaseObject*
{
    ObjectType* objType = find(objTypeNum);

    if (objType == nullptr)
    {
        objType = load(objTypeNum, 0);
    }

    if (objType == nullptr)
    {
        return nullptr;
    }

    BaseObject* object = objType->createInstance();

    if (object == nullptr)
    {
        return nullptr;
    }

    objType->numUsers++;
    return object;
}

auto ObjectTypeManager::load(int32_t objTypeNum, int keepMe) -> ObjectType*
{
    dynamicFrameTiming = 0;

    // Original behaviour: a type already loaded isn't returned; the caller gets null.
    if (objTypeNum < 1 || find(objTypeNum) != nullptr || objectFile->seekPacket(objTypeNum) != 0)
    {
        return nullptr;
    }

    // The packet's "ObjectClass" block names the class to make.
    int32_t objectClassNum = -1;
    {
        FitIniFile classFile;

        if (classFile.open(objectFile, static_cast<uint32_t>(objectFile->getPacketSize()), 50) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006), nullptr);
        }

        if (classFile.seekBlock("ObjectClass") != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006), nullptr);
        }

        if (classFile.readIdLong("ObjectTypeNum", objectClassNum) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006), nullptr);
        }

        classFile.close();
    }

    objectFile->seekPacket(objTypeNum);

    ObjectType* objType = nullptr;

    switch (objectClassNum)
    {
        case 0:
            objType = new TreeType;
            break;
        case 1:
            objType = new BuildingType;
            break;
        case 2:
        {
            auto* mechType = new BattleMechType;

            if (mechType != nullptr)
            {
                mechType->init();
            }

            objType = mechType;
            break;
        }

        case 3:
        {
            auto* vehicleType = new GroundVehicleType;

            if (vehicleType != nullptr)
            {
                vehicleType->init();
            }

            objType = vehicleType;
            break;
        }

        case 4:
            objType = new ExplosionType;
            break;
        case 5:
            objType = new FireType;
            break;
        case 6:
            objType = new LaserType;
            break;
        case 7:
            objType = new SmokeType;
            break;
        case 8:
            objType = new BulletType;
            break;
        case 9:
            objType = new DebrisType;
            break;
        case 0xb:
            objType = new TerrainObjectType;
            break;
        case 0xc:
            objType = new ArtilleryType;
            break;
        case 0xd:
            // Port fix: the original returns the type number as a pointer (and writes through it for a kept type).
            return nullptr;
        case 0xe:
            objType = new ElementalType;
            break;
        case 0xf:
            objType = new MiscTerrainObjectType;
            break;
        case 0x10:
            objType = new JetType;
            break;
        case 0x11:
            objType = new ProjectileLaserType;
            break;
        case 0x12:
            objType = new TreeBuildingType;
            break;
        case 0x13:
            objType = new CameraDroneType;
            break;
        case 0x14:
            objType = new TrainCarType;
            break;
        case 0x15:
            objType = new TurretType;
            break;
        case 0x16:
            objType = new GateType;
            break;
        case 0x17:
            objType = new LightType;
            break;
        case -1:
            Fatal(static_cast<int32_t>(0xbeef0001), nullptr);
        default:
            Fatal(static_cast<int32_t>(0xbeef0003), nullptr);
    }

    // Port fix: the original calls through a null type when the type heap is full.
    if (objType == nullptr)
    {
        return nullptr;
    }

    if (objType->init(objectFile, static_cast<uint32_t>(objectFile->getPacketSize())) != 0)
    {
        Fatal(static_cast<int32_t>(0xbeef0006), nullptr);
    }

    if (keepMe != 0)
    {
        // A kept type preloads its appearance and explosion.
        objType->keepMe = 1;
        appearanceTypeList->getAppearance(objType->appearName, 0);
        load(objType->explosionObject, 0);
    }

    objType->objTypeNum = objTypeNum;
    add(objType);
    return objType;
}
