#include "stdafx.h"
#include "ai/genordr.h"
#include "abl/ablenv.h"
#include "abl/ablrtn.h"
#include "abl/ablxstd.h"
#include "object/gameobj.h"
#include "object/warrior.h"

MCAblParam* MCGeneralOrder::OrderParams = nullptr;

auto MCGeneralOrder::Init() -> void
{
    Object = nullptr;
    ObjectClass = 0;
    Warrior = nullptr;
    Brain = nullptr;

    if (OrderParams == nullptr)
    {
        OrderParams = AblCreateParamList(3);
    }
}

auto MCGeneralOrder::Init(MCGameObject* obj, int32_t moduleHandle) -> int32_t
{
    Object = obj;
    Warrior = nullptr;
    ObjectClass = static_cast<int32_t>(obj->ObjectClass);

    if (ObjectClass == 2 || ObjectClass == 3 || ObjectClass == 4 || ObjectClass == 8)
    {
        Warrior = obj->GetPilot();
    }

    if (moduleHandle >= 0)
    {
        Brain = new MCAblModule;
        const int32_t result = Brain->Init(moduleHandle);

        if (result != 0)
        {
            return result;
        }
    }

    return 0;
}

auto MCGeneralOrder::Execute() -> int32_t
{
    CurObjectClass = ObjectClass;
    CurObject = Object;
    CurWarrior = Warrior;
    // Original behaviour (OB-036): getGroup and execute are called through a null warrior (non-mover) or brain
    // (moduleHandle < 0). Nothing in MCX.EXE creates a GeneralOrder.
    // Port fix: skip both when null.
    CurGroup = (CurWarrior != nullptr) ? CurWarrior->GetGroup() : nullptr;

    if (Brain != nullptr)
    {
        Brain->Execute(OrderParams);
    }

    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurGroup = nullptr;
    return 0;
}

auto MCGeneralOrder::Destroy() -> void
{
    if (Brain != nullptr)
    {
        Brain->Destroy();
        delete Brain;
        Brain = nullptr;
    }
}
