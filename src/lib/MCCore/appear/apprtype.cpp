#include "stdafx.h"
#include "appear/apprtype.h"
#include "appear/lineappr.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/heap.h"
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

UserHeap* AppearanceTypeList::appearanceHeap = nullptr;
AppearanceTypeList* appearanceTypeList = nullptr;

auto AppearanceType::operator new(size_t size) noexcept -> void*
{
    void* block = nullptr;

    if (AppearanceTypeList::appearanceHeap != nullptr && AppearanceTypeList::appearanceHeap->heapSize != 0)
    {
        block = AppearanceTypeList::appearanceHeap->malloc(static_cast<uint32_t>(size));
    }

    return block;
}

auto AppearanceType::operator delete(void* block) -> void
{
    if (AppearanceTypeList::appearanceHeap != nullptr && AppearanceTypeList::appearanceHeap->heapSize != 0)
    {
        AppearanceTypeList::appearanceHeap->free(block);
    }
}

auto AppearanceType::initType(File* apprFile, uint32_t fileSize) -> int32_t
{
    FitIniFile iniFile;
    int32_t result = iniFile.open(apprFile, fileSize, 0x32);

    if (result != 0)
    {
        return result;
    }

    if (iniFile.seekBlock("Bounds") == 0)
    {
        result = iniFile.readIdLong("UpperLeftX", boundsUpperLeftX);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("UpperLeftY", boundsUpperLeftY);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("LowerRightX", boundsLowerRightX);

        if (result != 0)
        {
            return result;
        }

        result = iniFile.readIdLong("LowerRightY", boundsLowerRightY);

        if (result != 0)
        {
            return result;
        }
    }

    iniFile.close();
    return 0;
}

auto AppearanceType::addUsers(void* user) -> void
{
    auto* node = static_cast<AppearanceUser*>(spriteManager->mallocDataRAM(sizeof(AppearanceUser)));
    Assert(node != nullptr, 0, " Too much sprite data ");
    node->next = nullptr;
    node->user = user;

    if (userList == nullptr)
    {
        userList = node;
        lastUser = node;
        return;
    }

    AppearanceUser* previous = lastUser;
    lastUser = node;
    previous->next = node;
}

auto AppearanceType::destroy() -> void
{
    AppearanceUser* node = userList;

    while (node != nullptr)
    {
        userList = node->next;

        if (node == lastUser)
        {
            lastUser = nullptr;
            userList = nullptr;
        }

        spriteManager->freeDataRAM(node);
        node = userList;
    }
}

auto AppearanceType::removeUsers(void* user) -> void
{
    AppearanceUser* previous = nullptr;
    AppearanceUser* node = userList;

    if (node == nullptr)
    {
        return;
    }
    while (node->user != user)
    {
        previous = node;
        node = node->next;

        if (node == nullptr)
        {
            return;
        }
    }

    if (previous == nullptr)
    {
        userList = node->next;
    }
    else
    {
        previous->next = node->next;
    }

    if (node == lastUser)
    {
        lastUser = previous;
    }

    spriteManager->freeDataRAM(node);
}

auto AppearanceTypeList::init(char* fileName, uint32_t heapSize) -> int32_t
{
    appearanceHeap = new UserHeap();

    if (appearanceHeap == nullptr)
    {
        return -0x5225ffff;
    }

    int32_t result = appearanceHeap->init(heapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    FullPathFileName spriteName;
    spriteName.init(spritePath, fileName, ".pak");
    appearanceFile = new PacketFile();

    if (appearanceFile == nullptr)
    {
        return -0x5225fffe;
    }

    if (appearanceFile->open(spriteName, READ, 0x32) != 0)
    {
        FullPathFileName cdName;
        cdName.init(CDspritePath, fileName, ".pak");
        result = appearanceFile->open(cdName, READ, 0x32);

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto AppearanceTypeList::getAppearance(uint32_t appearanceId, uint32_t loadFlags) -> AppearanceType*
{
    const auto packetNum = static_cast<int32_t>(appearanceId & 0xffffff);
    const uint32_t appearanceClass = appearanceId >> 24;

    if (appearanceClass == 0)
    {
        return nullptr;
    }

    for (AppearanceType* type = head; type != nullptr; type = type->next)
    {
        if (type->appearanceNum == appearanceId)
        {
            type->numUsers++;
            return type;
        }
    }

    PacketFile* packetFile = appearanceFile;

    if (packetFile->seekPacket(packetNum) != 0)
    {
        return nullptr;
    }

    const auto packetSize = static_cast<uint32_t>(packetFile->getPacketSize());
    AppearanceType* type = nullptr;

    switch (appearanceClass)
    {
        case SPRITE_TREE:
            type = new SpriteTree();
            break;
        case VFX_APPEAR:
            type = new VFXAppearanceType();
            break;
        case LINE_APPEAR:
            type = new LineAppearanceType();
            break;
        case GV_APPEAR:
            type = new GVAppearanceType();
            break;
        case ARM_APPEAR:
            type = new ArmAppearanceType();
            break;
        case BUILD_APPEAR:
            type = new VFXBuildingAppearanceType();
            break;
        case ELM_TREE:
            type = new ElementalTree();
            break;
        case PU_APPEAR:
            type = new PUAppearanceType();
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

    type->appearanceNum = appearanceId;

    if (type->init(packetFile, packetSize, loadFlags) != 0)
    {
        return nullptr;
    }

    packetFile->seekPacket(packetNum);

    if (type->initType(packetFile, packetSize) != 0)
    {
        return nullptr;
    }

    type->numUsers = 1;
    type->next = nullptr;

    if (head == nullptr)
    {
        head = type;
        last = type;
        return type;
    }

    AppearanceType* previous = last;
    last = type;
    previous->next = type;
    return type;
}

auto AppearanceTypeList::removeAppearance(AppearanceType* which) -> int32_t
{
    AppearanceType* previous = nullptr;
    AppearanceType* type = head;

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
        type = type->next;
    }

    type->numUsers--;

    if (type->numUsers == 0 && type->keepLoaded == 0)
    {
        if (previous == nullptr)
        {
            head = type->next;
        }
        else
        {
            previous->next = type->next;
        }

        if (type == last)
        {
            last = previous;
        }

        delete type;
    }

    return 0;
}

auto AppearanceTypeList::destroy() -> void
{
    if (appearanceFile != nullptr)
    {
        appearanceFile->close();
        delete appearanceFile;
    }

    appearanceFile = nullptr;
    // The types are only destroyed, not deleted: they go with the heap.
    AppearanceType* type = head;

    while (type != nullptr)
    {
        AppearanceType* next = type->next;
        type->destroy();
        type = next;
    }

    last = nullptr;
    head = nullptr;
    delete appearanceHeap;
    appearanceHeap = nullptr;
}
