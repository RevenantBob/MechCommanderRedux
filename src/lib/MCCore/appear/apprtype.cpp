#include "stdafx.h"
#include "appear/apprtype.h"
#include "appear/lineappr.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "sprite/actor.h"
#include "sprite/armactor.h"
#include "sprite/bactor.h"
#include "sprite/elmtree.h"
#include "sprite/gvactor.h"
#include "sprite/puactor.h"
#include "sprite/spritree.h"
#include "sprite/sprtmgr.h"

MCAppearanceTypeList* AppearanceTypeList = nullptr;

auto MCAppearanceType::InitType(MCFile* apprFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile iniFile;
    int32_t result = iniFile.Open(apprFile, fileSize, 0x32);

    if (result != 0)
    {
        return result;
    }

    if (iniFile.SeekBlock("Bounds") == 0)
    {
        result = iniFile.ReadIdLong("UpperLeftX", BoundsUpperLeftX);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("UpperLeftY", BoundsUpperLeftY);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("LowerRightX", BoundsLowerRightX);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.ReadIdLong("LowerRightY", BoundsLowerRightY);

        if (result != 0)
        {
            return result;
        }
    }

    iniFile.Close();
    return 0;
}

auto MCAppearanceType::AddUsers(void* user) -> void
{
    auto* node = static_cast<MCAppearanceUser*>(SpriteManager->MallocDataRam(sizeof(MCAppearanceUser)));
    Assert(node != nullptr, 0, " Too much sprite data ");
    node->Next = nullptr;
    node->User = user;

    if (UserList == nullptr)
    {
        UserList = node;
        LastUser = node;
        return;
    }

    MCAppearanceUser* previous = LastUser;
    LastUser = node;
    previous->Next = node;
}

auto MCAppearanceType::Destroy() -> void
{
    MCAppearanceUser* node = UserList;

    while (node != nullptr)
    {
        UserList = node->Next;

        if (node == LastUser)
        {
            LastUser = nullptr;
            UserList = nullptr;
        }

        SpriteManager->FreeDataRam(node);
        node = UserList;
    }
}

auto MCAppearanceType::RemoveUsers(void* user) -> void
{
    MCAppearanceUser* previous = nullptr;
    MCAppearanceUser* node = UserList;

    if (node == nullptr)
    {
        return;
    }
    while (node->User != user)
    {
        previous = node;
        node = node->Next;

        if (node == nullptr)
        {
            return;
        }
    }

    if (previous == nullptr)
    {
        UserList = node->Next;
    }
    else
    {
        previous->Next = node->Next;
    }

    if (node == LastUser)
    {
        LastUser = previous;
    }

    SpriteManager->FreeDataRam(node);
}

auto MCAppearanceTypeList::Init(char* fileName) -> int32_t
{
    MCFullPathFileName spriteName;
    spriteName.Init(SpritePath, fileName, ".pak");
    AppearanceFile = new MCPacketFile();

    if (AppearanceFile == nullptr)
    {
        return -0x5225fffe;
    }

    if (AppearanceFile->Open(spriteName, READ, 0x32) != 0)
    {
        MCFullPathFileName cdName;
        cdName.Init(CDspritePath, fileName, ".pak");
        const int32_t result = AppearanceFile->Open(cdName, READ, 0x32);

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto MCAppearanceTypeList::GetAppearance(uint32_t appearanceId, uint32_t loadFlags) -> MCAppearanceType*
{
    const auto packetNum = static_cast<int32_t>(appearanceId & 0xffffff);
    const uint32_t appearanceClass = appearanceId >> 24;

    if (appearanceClass == 0)
    {
        return nullptr;
    }

    for (MCAppearanceType* type = Head; type != nullptr; type = type->Next)
    {
        if (type->AppearanceNum == appearanceId)
        {
            type->NumUsers++;
            return type;
        }
    }

    MCPacketFile* packetFile = AppearanceFile;

    if (packetFile->SeekPacket(packetNum) != 0)
    {
        return nullptr;
    }

    const auto packetSize = static_cast<uint32_t>(packetFile->GetPacketSize());
    MCAppearanceType* type = nullptr;

    switch (appearanceClass)
    {
        case SPRITE_TREE:
            type = new MCSpriteTree();
            break;
        case VFX_APPEAR:
            type = new MCVfxAppearanceType();
            break;
        case LINE_APPEAR:
            type = new MCLineAppearanceType();
            break;
        case GV_APPEAR:
            type = new MCGVAppearanceType();
            break;
        case ARM_APPEAR:
            type = new MCArmAppearanceType();
            break;
        case BUILD_APPEAR:
            type = new MCVfxBuildingAppearanceType();
            break;
        case ELM_TREE:
            type = new MCElementalTree();
            break;
        case PU_APPEAR:
            type = new MCPUAppearanceType();
            break;
        default:
            return nullptr;
    }

    // Port fix: a failed allocation returns null. The original wrote the id through the null pointer (class 1) or
    // called the null type's init (the others).
    if (type == nullptr)
    {
        return nullptr;
    }

    type->AppearanceNum = appearanceId;

    if (type->Init(packetFile, packetSize, loadFlags) != 0)
    {
        return nullptr;
    }

    packetFile->SeekPacket(packetNum);

    if (type->InitType(packetFile, packetSize) != 0)
    {
        return nullptr;
    }

    type->NumUsers = 1;
    type->Next = nullptr;

    if (Head == nullptr)
    {
        Head = type;
        Last = type;
        return type;
    }

    MCAppearanceType* previous = Last;
    Last = type;
    previous->Next = type;
    return type;
}

auto MCAppearanceTypeList::RemoveAppearance(MCAppearanceType* which) -> int32_t
{
    MCAppearanceType* previous = nullptr;
    MCAppearanceType* type = Head;

    while (true)
    {
        if (type == nullptr)
        {
            return -0x5225fffd;
        }

        if (type == which)
        {
            break;
        }

        previous = type;
        type = type->Next;
    }

    type->NumUsers--;

    if (type->NumUsers == 0 && type->KeepLoaded == 0)
    {
        if (previous == nullptr)
        {
            Head = type->Next;
        }
        else
        {
            previous->Next = type->Next;
        }

        if (type == Last)
        {
            Last = previous;
        }

        delete type;
    }

    return 0;
}

auto MCAppearanceTypeList::Destroy() -> void
{
    if (AppearanceFile != nullptr)
    {
        AppearanceFile->Close();
        delete AppearanceFile;
    }

    AppearanceFile = nullptr;
    // The original only destroyed the types and let them go with its appearance heap.
    MCAppearanceType* type = Head;

    while (type != nullptr)
    {
        MCAppearanceType* next = type->Next;
        type->Destroy();
        delete type;
        type = next;
    }

    Last = nullptr;
    Head = nullptr;
}
