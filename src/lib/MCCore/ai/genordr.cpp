#include "stdafx.h"
#include "ai/genordr.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "abl/ablxstd.h"
#include "lib/heap.h"
#include "object/gameobj.h"
#include "object/warrior.h"

ABLParam* GeneralOrder::orderParams = nullptr;

auto GeneralOrder::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto GeneralOrder::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto GeneralOrder::init() -> void
{
    object = nullptr;
    objectClass = 0;
    warrior = nullptr;
    brain = nullptr;

    if (orderParams == nullptr)
    {
        orderParams = ABLi_createParamList(3);
    }
}

auto GeneralOrder::init(GameObject* obj, int32_t moduleHandle) -> int32_t
{
    object = obj;
    warrior = nullptr;
    objectClass = static_cast<int32_t>(obj->objectClass);

    if (objectClass == 2 || objectClass == 3 || objectClass == 4 || objectClass == 8)
    {
        warrior = obj->getPilot();
    }

    if (moduleHandle >= 0)
    {
        brain = new ABLModule;
        const int32_t result = brain->init(moduleHandle);

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto GeneralOrder::execute() -> int32_t
{
    CurObjectClass = objectClass;
    CurObject = object;
    CurWarrior = warrior;
    // Original behaviour (OB-036): getGroup and execute are called through a null warrior (non-mover) or brain
    // (moduleHandle < 0). Nothing in MCX.EXE creates a GeneralOrder.
    // Port fix: skip both when null.
    CurGroup = (CurWarrior != nullptr) ? CurWarrior->getGroup() : nullptr;

    if (brain != nullptr)
    {
        brain->execute(orderParams);
    }

    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurGroup = nullptr;
    return 0;
}

auto GeneralOrder::destroy() -> void
{
    if (brain != nullptr)
    {
        brain->destroy();
        delete brain;
        brain = nullptr;
    }
}
