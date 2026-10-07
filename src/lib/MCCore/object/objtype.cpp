#include "stdafx.h"
#include "object/objtype.h"
#include "appear/apprtype.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
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
char ObjectPath[80] = "data\\objects\\";
MCPacketFile* MCObjectTypeManager::ObjectFile = nullptr;
MCBlockStore MCObjectTypeManager::ObjectTypeCache;
MCBlockStore MCObjectTypeManager::ObjectCache;

//---------------------------------------------------------------------------
// ObjectType
//---------------------------------------------------------------------------

auto MCObjectType::CreateInstance() -> MCBaseObject*
{
    auto* object = new MCBaseObject;
    object->Init(this);
    object->IdNumber = NextIdNumber++;
    return object;
}

auto MCObjectType::Init(MCFitIniFile* typeFile) -> int32_t
{
    int32_t result = typeFile->SeekBlock("ObjectType");

    if (result != 0)
    {
        return result;
    }

    NumUsers = 0;
    result = typeFile->ReadIdLong("Type", TypeClass);

    if (result != 0)
    {
        return result;
    }

    int32_t appearance = 0;
    result = typeFile->ReadIdLong("Appearance", appearance);
    AppearName = static_cast<uint32_t>(appearance);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->ReadIdLong("ExplosionObject", ExplosionObject);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->ReadIdLong("DestroyedObject", DestroyedObject);

    if (result != 0)
    {
        return result;
    }

    result = typeFile->ReadIdFloat("ExtentRadius", ExtentRadius);

    if (result != 0)
    {
        return result;
    }

    if (typeFile->ReadIdLong("KeepMe", KeepMe) != 0)
    {
        KeepMe = 0;
    }

    if (typeFile->ReadIdLong("IconNumber", IconNumber) != 0)
    {
        IconNumber = -1;
    }

    if (typeFile->ReadIdLong("Alignment", TeamId) != 0)
    {
        TeamId = 0;
    }

    return 0;
}

auto MCObjectType::CreateExplosion(MCVector3D& position, float damage, float radius) -> void
{
    if (ExplosionObject == -1)
    {
        return;
    }

    MCGameObject* explosion = CreateObject(ExplosionObject);

    if (explosion == nullptr)
    {
        return;
    }

    explosion->SetPosition(position);

    if (radius != 0.0)
    {
        explosion->SetExplRad(radius);
        explosion->SetExplDmg(damage);
    }

    if (ObjectList->Head != nullptr)
    {
        ObjectList->Head->AddNode(explosion);
    }
}

auto MCObjectType::HandleDestruction(MCGameObject* collidee, MCGameObject*) -> int
{
    if (ExplosionObject != -1)
    {
        MCVector3D position = collidee->GetPosition();
        CreateExplosion(position, 0.0f, 0.0f);
    }

    return 1;
}

//---------------------------------------------------------------------------
// ObjectTypeManager
//---------------------------------------------------------------------------

auto MCObjectTypeManager::Init(char* objectFileName, int32_t objectTypeCacheSize, int32_t objectCacheSize) -> int32_t
{
    std::string fileName;
    fileName = GamePath(ObjectPath, objectFileName, ".pak");

    ObjectFile = new MCPacketFile;

    if (ObjectFile == nullptr)
    {
        return static_cast<int32_t>(0xbeef0008);
    }

    int32_t result = ObjectFile->Open(fileName);

    if (result != 0)
    {
        return result;
    }

    return 0;
}

auto MCObjectTypeManager::Destroy() -> void
{
    if (ObjectFile != nullptr)
    {
        ObjectFile->Close();
    }

    delete ObjectFile;
    ObjectFile = nullptr;

    // The original's heap went with the types still loaded (kept or still used) without running their destructors;
    // the port deletes them, then frees the blocks nothing freed.
    while (_Head != nullptr)
    {
        auto* node = static_cast<MCObjectTypeNode*>(_Head);
        delete node->ObjType;
        MCLinkedList::Destroy(node);
    }

    ObjectTypeCache.Clear();
    ObjectCache.Clear();
}

auto MCObjectTypeManager::Add(MCObjectType* objType) -> void
{
    auto* node = new MCObjectTypeNode;
    node->ObjType = objType;
    AddToTail(node);
}

auto MCObjectTypeManager::Remove(int32_t index) -> void
{
    MCLink* link = nullptr;
    int more = Traverse(link);
    int32_t i = 0;

    while (more != 0 && i < index)
    {
        more = Traverse(link);
        i++;
    }

    if (link != nullptr)
    {
        Remove(static_cast<MCObjectTypeNode*>(link));
    }
}

auto MCObjectTypeManager::Remove(MCObjectType* objType) -> void
{
    MCLink* link = nullptr;
    int more = Traverse(link);

    while (more != 0 && static_cast<MCObjectTypeNode*>(link)->ObjType != objType)
    {
        more = Traverse(link);
    }

    if (link != nullptr)
    {
        Remove(static_cast<MCObjectTypeNode*>(link));
    }
}

auto MCObjectTypeManager::Remove(MCObjectTypeNode* node) -> void
{
    if (node == nullptr)
    {
        return;
    }

    MCObjectType* objType = node->ObjType;
    objType->NumUsers--;

    if (objType->NumUsers < 1 && objType->KeepMe == 0)
    {
        delete objType;
        MCLinkedList::Destroy(node);
    }
}

auto MCObjectTypeManager::Find(int32_t objTypeNum) -> MCObjectType*
{
    MCLink* link = nullptr;

    for (int more = Traverse(link); more != 0; more = Traverse(link))
    {
        MCObjectType* objType = static_cast<MCObjectTypeNode*>(link)->ObjType;

        if (objType->ObjTypeNum == objTypeNum)
        {
            return objType;
        }
    }

    return nullptr;
}

auto MCObjectTypeManager::Element(int32_t index) -> MCObjectType*
{
    MCLink* link = nullptr;
    int32_t i = 0;

    for (int more = Traverse(link); more != 0; more = Traverse(link))
    {
        if (i == index)
        {
            return static_cast<MCObjectTypeNode*>(link)->ObjType;
        }

        i++;
    }

    return nullptr;
}

auto MCObjectTypeManager::Get(int32_t objTypeNum) -> MCBaseObject*
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

    MCBaseObject* object = objType->CreateInstance();

    if (object == nullptr)
    {
        return nullptr;
    }

    objType->NumUsers++;
    return object;
}

auto MCObjectTypeManager::Load(int32_t objTypeNum, int keepMe) -> MCObjectType*
{
    DynamicFrameTiming = 0;

    // Original behaviour: a type already loaded isn't returned; the caller gets null.
    if (objTypeNum < 1 || Find(objTypeNum) != nullptr || ObjectFile->SeekPacket(objTypeNum) != 0)
    {
        return nullptr;
    }

    // The packet's "ObjectClass" block names the class to make.
    int32_t objectClassNum = -1;
    {
        MCFitIniFile classFile;

        if (classFile.Open(ObjectFile, static_cast<uint32_t>(ObjectFile->GetPacketSize())) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        if (classFile.SeekBlock("ObjectClass") != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        if (classFile.ReadIdLong("ObjectTypeNum", objectClassNum) != 0)
        {
            Fatal(static_cast<int32_t>(0xbeef0006));
        }

        classFile.Close();
    }

    ObjectFile->SeekPacket(objTypeNum);

    MCObjectType* objType = nullptr;

    switch (objectClassNum)
    {
        case 0:
            objType = new MCTreeType;
            break;
        case 1:
            objType = new MCBuildingType;
            break;
        case 2:
        {
            auto* mechType = new MCBattleMechType;

            if (mechType != nullptr)
            {
                mechType->Init();
            }

            objType = mechType;
            break;
        }

        case 3:
        {
            auto* vehicleType = new MCGroundVehicleType;

            if (vehicleType != nullptr)
            {
                vehicleType->Init();
            }

            objType = vehicleType;
            break;
        }

        case 4:
            objType = new MCExplosionType;
            break;
        case 5:
            objType = new MCFireType;
            break;
        case 6:
            objType = new MCLaserType;
            break;
        case 7:
            objType = new MCSmokeType;
            break;
        case 8:
            objType = new MCBulletType;
            break;
        case 9:
            objType = new MCDebrisType;
            break;
        case 0xb:
            objType = new MCTerrainObjectType;
            break;
        case 0xc:
            objType = new MCArtilleryType;
            break;
        case 0xd:
            // Port fix: the original returns the type number as a pointer (and writes through it for a kept type).
            return nullptr;
        case 0xe:
            objType = new MCElementalType;
            break;
        case 0xf:
            objType = new MCMiscTerrainObjectType;
            break;
        case 0x10:
            objType = new MCJetType;
            break;
        case 0x11:
            objType = new MCProjectileLaserType;
            break;
        case 0x12:
            objType = new MCTreeBuildingType;
            break;
        case 0x13:
            objType = new MCCameraDroneType;
            break;
        case 0x14:
            objType = new MCTrainCarType;
            break;
        case 0x15:
            objType = new MCTurretType;
            break;
        case 0x16:
            objType = new MCGateType;
            break;
        case 0x17:
            objType = new MCLightType;
            break;
        case -1:
            Fatal(static_cast<int32_t>(0xbeef0001));
        default:
            Fatal(static_cast<int32_t>(0xbeef0003));
    }

    // Port fix: the original calls through a null type when the type heap is full.
    if (objType == nullptr)
    {
        return nullptr;
    }

    if (objType->Init(ObjectFile, static_cast<uint32_t>(ObjectFile->GetPacketSize())) != 0)
    {
        Fatal(static_cast<int32_t>(0xbeef0006));
    }

    if (keepMe != 0)
    {
        // A kept type preloads its appearance and explosion.
        objType->KeepMe = 1;
        AppearanceTypeList->GetAppearance(objType->AppearName, 0);
        Load(objType->ExplosionObject, 0);
    }

    objType->ObjTypeNum = objTypeNum;
    Add(objType);
    return objType;
}
