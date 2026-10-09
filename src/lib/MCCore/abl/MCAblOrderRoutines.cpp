#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "iface/iface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/MCMasterComponent.h"
#include "object/MCForces.h"
#include "object/MCContactSystem.h"
#include "object/MCBigGameObject.h"
#include "object/gate.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/tbldng.h"
#include "object/terrobj.h"
#include "object/train.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"
#include "object/MCMechWarrior.h"
#include "sound/radio.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

// Tactical orders and their state.

namespace
{
    /// <summary>gettacorder / getlasttacorder: the order's time stamp and parameters of a mover's pilot.</summary>
    auto GetTacOrderData(MCAblRuntime& abl, bool last) -> void
    {
        abl.GetCodeToken();
        abl.GetCodeToken();
        abl.ExecExpression();
        int32_t partId = abl.Top().Integer;
        abl.GetCodeToken();
        float* timeStamp = reinterpret_cast<float*>(abl.NextReference());
        abl.Pop();
        abl.GetCodeToken();
        int32_t* paramList = reinterpret_cast<int32_t*>(abl.NextReference());
        abl.Top().Integer = 0;

        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(abl, partId);

            if (object && IsMover(object))
            {
                MCMechWarrior* pilot = object->GetPilot();

                if (pilot)
                {
                    MCTacticalOrder& order = last ? pilot->LastTacOrder : pilot->CurTacOrder;
                    abl.Top().Integer = order.GetParamData(timeStamp, paramList);
                }
            }
        }

        abl.GetCodeToken();
    }
}

auto ExecHbGetTacOrder(MCAblRuntime& abl) -> MCAblType*
{
    GetTacOrderData(abl, false);
    return IntegerTypePtr;
}

auto ExecHbGetLastTacOrder(MCAblRuntime& abl) -> MCAblType*
{
    GetTacOrderData(abl, true);
    return IntegerTypePtr;
}

auto ExecHbSetOrderMode(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    // Original behaviour: the argument is ignored; the mode is always reset to pilot orders.
    int wasUnitOrder = abl.Brain.IsUnitOrder != 0;
    abl.Brain.IsUnitOrder = 0;
    abl.Top().Integer = wasUnitOrder;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbWait(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    float seconds = abl.Top().Real;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int clearLastTarget = abl.Top().Integer == 1;
    int32_t result = 0;

    if (abl.Brain.IsUnitOrder == 0)
    {
        // The original rounds with the 1.5 * 2^52 addition trick: to nearest, ties to even.
        result = abl.Brain.Warrior->OrderWait(0, MCOrderOrigin::Commander,
                                              static_cast<int32_t>(std::nearbyint(seconds)), clearLastTarget);
    }
    else
    {
        Fatal(0, " Team orderwait needs support ");
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetAttackRadius(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    float radius = abl.Top().Real;
    abl.Top().Real = abl.Brain.Warrior->AttackRadius;
    abl.Brain.Warrior->AttackRadius = radius;
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbMoveToPoint(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    float* location = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCVector3D goal;
    goal.X = location[0];
    goal.Y = location[1];
    goal.Z = location[2];
    uint32_t params = abl.Top().Integer == 1 ? 1 : 0;
    int32_t result;

    if (abl.Brain.IsUnitOrder == 0)
    {
        result = abl.Brain.Warrior->OrderMoveToPoint(0, 1, MCOrderOrigin::Commander, goal, -1, params);
    }
    else
    {
        result = abl.Brain.Group->OrderMoveToPoint(1, MCOrderOrigin::Commander, goal, params);
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbMoveToObject(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t flag = abl.Top().Integer;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            if (abl.Brain.IsUnitOrder == 0)
            {
                abl.Top().Integer =
                    abl.Brain.Warrior->OrderMoveToObject(0, 1, MCOrderOrigin::Commander, object, -1, flag == 1 ? 1 : 0);
            }
            else
            {
                abl.Top().Integer = abl.Brain.Group->OrderMoveToObject(1, MCOrderOrigin::Commander, object, 1);
            }

            abl.GetCodeToken();
            return IntegerTypePtr;
        }
    }

    abl.Top().Integer = 1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbMoveToContact(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t result = -1;

    if (abl.Brain.Contact)
    {
        if (abl.Brain.IsUnitOrder != 0)
        {
            result = abl.Brain.Group->OrderMoveToObject(1, MCOrderOrigin::Commander, abl.Brain.Contact, 1);
        }
        else
        {
            result = abl.Brain.Warrior->OrderMoveToObject(0, 1, MCOrderOrigin::Commander, abl.Brain.Contact, -1,
                                                          abl.Top().Integer == 1 ? 1 : 0);
        }
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderPowerDown(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(0);

    if (abl.Brain.IsUnitOrder != 0)
    {
        abl.Top().Integer = abl.Brain.Group->OrderPowerDown(MCOrderOrigin::Commander);
    }
    else
    {
        abl.Top().Integer = abl.Brain.Warrior->OrderPowerDown(0, MCOrderOrigin::Commander);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderPowerUp(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(0);

    if (abl.Brain.IsUnitOrder != 0)
    {
        abl.Top().Integer = abl.Brain.Group->OrderPowerUp(MCOrderOrigin::Commander);
    }
    else
    {
        abl.Top().Integer = abl.Brain.Warrior->OrderPowerUp(0, MCOrderOrigin::Commander);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderAttackObject(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t partId = static_cast<uint32_t>(abl.Top().Integer);
    abl.Pop();
    int32_t attackType = abl.NextInteger();
    int32_t attackMethod = abl.NextInteger();
    int32_t attackRange = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t params = abl.Top().Integer != 0 ? 0x10 : 0;

    if (partId != 0 && partId < 0x200)
    {
        abl.Top().Integer = 1;
        abl.GetCodeToken();
        return IntegerTypePtr;
    }

    // Unlike the other routines, -1 is looked up as a part id rather than meaning the current object.
    MCGameObject* target = nullptr;

    if (partId != 0)
    {
        target = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(static_cast<int32_t>(partId)));
    }

    if (abl.Brain.IsUnitOrder != 0)
    {
        abl.Top().Integer = abl.Brain.Group->OrderAttackObject(MCOrderOrigin::Commander, target, attackType,
                                                               attackMethod, attackRange, -1, params);
    }
    else
    {
        abl.Top().Integer = abl.Brain.Warrior->OrderAttackObject(0, MCOrderOrigin::Commander, target, attackType,
                                                                 attackMethod, attackRange, -1, params);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderAttackContact(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t attackType = abl.Top().Integer;
    abl.Pop();
    int32_t attackMethod = abl.NextInteger();
    int32_t attackRange = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t params = abl.Top().Integer != 0 ? 0x10 : 0;
    int32_t result = -2;

    if (abl.Brain.Contact)
    {
        result = abl.Brain.Warrior->OrderAttackObject(0, MCOrderOrigin::Commander, abl.Brain.Contact, attackType,
                                                      attackMethod, attackRange, -1, params);
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderTest(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlaySmacker(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectChangeSides(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t alignment = abl.NextInteger();

    if (IsGroupId(partId))
    {
        Fatal(0, " Cannot ABL:ObjectChangeSides for Mover Units ");
    }

    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && object->GetObjectType())
    {
        static_cast<MCGameObject*>(object)->SetAlignment(alignment);
    }

    abl.GetCodeToken();
}
