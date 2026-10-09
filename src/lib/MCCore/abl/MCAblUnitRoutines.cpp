#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
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
#include "object/MCMasterComponent.h"
#include "object/MCForces.h"
#include "object/MCContactSystem.h"
#include "object/MCBigGameObject.h"
#include "object/MCGate.h"
#include "object/MCGateType.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCTerrainObject.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCTrain.h"
#include "object/MCTrainCar.h"
#include "object/MCTrainCarType.h"
#include "object/MCTrainManager.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"
#include "object/MCMechWarrior.h"
#include "sound/MCRadio.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

// The running unit: its pilot, targets, contacts, weapons, alarms and memory cells.

auto ExecHbGetId(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(0);

    if (abl.Brain.Object)
    {
        abl.Top().Integer = abl.Brain.Object->PartId;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetTime(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushReal(ActualTime);
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTimeLeft(MCAblRuntime& abl) -> MCAblType*
{
    float timeLeft;

    if (Scenario()->TimeLimit < 0)
    {
        timeLeft = -1.0f;
    }
    else
    {
        timeLeft = static_cast<float>(Scenario()->TimeLimit) - ActualTime;

        if (timeLeft <= 0.0f)
        {
            timeLeft = 0.0f;
        }
    }

    abl.PushReal(timeLeft);
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTarget(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (abl.Brain.IsUnitOrder == 0)
    {
        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(abl, partId);

            if (object && IsMover(object))
            {
                MCMechWarrior* pilot = object->GetPilot();
                Assert(pilot != nullptr, 0, " execHbGetTarget:No pilot in mover! ");
                MCGameObject* target = pilot->GetLastTarget();

                if (target)
                {
                    abl.Top().Integer = target->PartId;
                }
            }
        }
    }
    else
    {
        MCGameObject* target = abl.Brain.Group->GetPointPilot()->GetLastTarget();

        // Port fix: the original reads the part id of a null target.
        if (target)
        {
            abl.Top().Integer = target->PartId;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetTarget(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t targetId = abl.NextInteger();
    MCGameObject* target = FindObject(abl, targetId);

    if (IsGroupId(partId))
    {
        const std::vector<MCMover*> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMechWarrior* pilot = movers[i]->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
            }
        }
    }
    else
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            MCMechWarrior* pilot = object->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSelectUnit(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = -1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectObject(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t previousId = 0;
    abl.ExecExpression();

    if (abl.Brain.Object)
    {
        previousId = abl.Brain.Object->PartId;
    }

    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object)
    {
        abl.Brain.Object = static_cast<MCGameObject*>(object);
        abl.Top().Integer = previousId;
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectWarrior(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Top().Integer = -1;
    int32_t previousIndex = 0;

    if (abl.Brain.Warrior)
    {
        previousIndex = abl.Brain.Warrior->Index;
    }

    abl.Top().Integer = previousIndex;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario()->NumWarriors())
    {
        abl.Brain.Warrior = Scenario()->Warrior(warriorIndex);
    }
    else
    {
        abl.Brain.Warrior = nullptr;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWarriorStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario()->NumWarriors())
    {
        MCMechWarrior* warrior = Scenario()->Warrior(warriorIndex);

        if (warrior)
        {
            abl.Top().Integer = warrior->Status;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContacts(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t* contacts = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();
    int32_t contactCriteria = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t sortType = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(abl.Brain.Object))
    {
        abl.Top().Integer =
            abl.Brain.Object->GetTeam()->GetContacts(abl.Brain.Object, contacts, contactCriteria, sortType);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetEnemyCount(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            if (IsMover(object))
            {
                abl.Top().Integer = object->GetTeam()->NumLosContacts();
            }
            else if (object->ObjectClass == MCObjectClass::Artillery ||
                     object->ObjectClass == MCObjectClass::Building ||
                     object->ObjectClass == MCObjectClass::TreeBuilding)
            {
                int32_t alignment = object->GetAlignment();

                if (alignment == -1)
                {
                    abl.Top().Integer = ClanTeam()->NumLosContacts();
                }
                else if (alignment == 1)
                {
                    abl.Top().Integer = InnerSphereTeam()->NumLosContacts();
                }
            }
        }
    }
    else if (partId == 500)
    {
        abl.Top().Integer = InnerSphereTeam()->NumLosContacts();
    }
    else if (partId == 0x1f5)
    {
        abl.Top().Integer = ClanTeam()->NumLosContacts();
    }
    else if (partId == 0x1f6 && AlliedTeam())
    {
        abl.Top().Integer = AlliedTeam()->NumLosContacts();
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectContact(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(abl.Brain.Object))
    {
        MCGameObject* contact = FindObject(abl, partId);

        if (contact && abl.Brain.Object->GetTeam()->GetContactType(contact) != 0)
        {
            abl.Brain.Contact = contact;
            abl.Top().Integer = 0;
        }
        else
        {
            abl.Top().Integer = 1;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsContact(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t contactCriteria = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t select = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(abl.Brain.Object))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (abl.Brain.Object->GetTeam()->IsContact(object, contactCriteria) == 0)
        {
            abl.Top().Integer = 0;
        }
        else
        {
            abl.Top().Integer = partId;

            if (select)
            {
                abl.Brain.Contact = object;
            }
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactId(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(abl.Brain.Contact ? abl.Brain.Contact->PartId : 0);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t* tagged = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Top().Integer = 0;

    *tagged = 0;
    if (abl.Brain.Contact)
    {
        int taggedFlag;
        abl.Top().Integer = abl.Brain.Contact->GetContactType(abl.Brain.Object->GetTeam()->Id, taggedFlag);
        *tagged = (taggedFlag == 1) ? 1 : 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactRelativePosition(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    float* range = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    abl.GetCodeToken();
    float* angle = reinterpret_cast<float*>(abl.NextReference());
    *range = -1.0f;
    abl.Top().Integer = 1;

    *angle = 0.0f;
    if (abl.Brain.Contact && abl.Brain.Object)
    {
        MCVector3D contactPosition = abl.Brain.Contact->GetPosition();
        *range = static_cast<float>(abl.Brain.Object->DistanceFrom(contactPosition));
        *angle = abl.Brain.Object->RelFacingTo(abl.Brain.Contact->GetPosition(), -1);
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetPotentialContact(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t contactType = abl.Top().Integer;

    // Original behaviour: the contact type is left on the stack as the result.
    if (IsGroupId(partId))
    {
        const std::vector<MCMover*> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            movers[i]->SetPotentialContact(contactType);
        }
    }
    else
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            object->SetPotentialContact(contactType);
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeapons(MCAblRuntime& abl, MCAblRoutineKey key) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t listSize = abl.Top().Integer;
    MCGameObject* target = abl.Brain.Warrior->GetLastTarget();
    abl.Top().Integer = -1;

    if (IsMover(abl.Brain.Object))
    {
        MCMover* mover = static_cast<MCMover*>(abl.Brain.Object);

        if (key == MCAblRoutineKey::GetWeaponsReady)
        {
            abl.Top().Integer = mover->GetWeaponsReady(weaponList, listSize);
        }
        else if (key == MCAblRoutineKey::GetWeaponsInRange && target)
        {
            MCVector3D targetPosition = target->GetPosition();
            abl.Top().Integer =
                mover->GetWeaponsInRange(weaponList, listSize, static_cast<float>(mover->DistanceFrom(targetPosition)));
        }
        else
        {
            abl.Top().Integer = mover->GetWeaponsLocked(weaponList, listSize);
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponShots(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t weaponIndex = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(abl.Brain.Object))
    {
        abl.Top().Integer = static_cast<MCMover*>(abl.Brain.Object)->GetWeaponShots(weaponIndex);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponRanges(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    float* ranges = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    MCGameObject* object = FindObject(abl, partId);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);

        if (mover->ShortestRangeWeapon == 0xff)
        {
            ranges[0] = -1.0f;
        }
        else
        {
            ranges[0] = MasterComponentList[mover->Inventory[mover->ShortestRangeWeapon].MasterID].WeaponRange[1];
        }

        ranges[1] = mover->GetFireRange(-1);

        if (ranges[1] == -1.0f)
        {
            mover->CalcOptimalRange(nullptr);
            ranges[1] = mover->GetFireRange(-1);
        }

        ranges[2] = mover->GetFireRange(-2);
    }
    else
    {
        ranges[2] = 0.0f;
        ranges[1] = 0.0f;
        ranges[0] = 0.0f;
    }

    abl.GetCodeToken();
}

auto ExecHbSetMoveGoal(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t goalType = static_cast<uint32_t>(abl.Top().Integer);
    abl.Pop();
    abl.GetCodeToken();
    float* location = reinterpret_cast<float*>(abl.NextReference());

    if (abl.Brain.Warrior)
    {
        MCVector3D goal;
        goal.X = location[0];
        goal.Y = location[1];
        goal.Z = location[2];
        abl.Brain.Warrior->SetMoveGoal(goalType, &goal, nullptr);
        location[0] = goal.X;
        location[1] = goal.Y;
        location[2] = goal.Z;
        abl.Brain.Warrior->MoveOrders.ScriptGoal = 1;
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetChallenger(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object && IsMover(object))
        {
            MCGameObject* challenger = static_cast<MCMover*>(object)->GetChallenger();

            if (challenger)
            {
                abl.Top().Integer = challenger->PartId;
            }
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetFireRanges(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    float* ranges = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    ranges[0] = WeaponRange[0];
    ranges[1] = WeaponRange[1];
    ranges[2] = WeaponRange[2];
    ranges[3] = Scenario()->MaxWeaponRange;
    abl.GetCodeToken();
    return nullptr;
}

auto ExecHbGetAttackers(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    uint32_t* attackerList = reinterpret_cast<uint32_t*>(abl.NextReference());
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    float seconds = abl.Top().Real;
    abl.Top().Integer = 0;

    if (abl.Brain.Warrior)
    {
        abl.Top().Integer = abl.Brain.Warrior->GetAttackers(attackerList, seconds);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetAttackerInfo(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t attackerId = static_cast<uint32_t>(abl.Top().Integer);
    abl.Top().Real = 1000000.0f;

    if ((attackerId == 0 || attackerId > 0x1ff) && abl.Brain.Warrior)
    {
        MCAttackerRec* attackerRec = abl.Brain.Warrior->GetAttackerInfo(attackerId);

        if (attackerRec)
        {
            abl.Top().Real = ScenarioTime - attackerRec->LastTime;
        }
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTimeWithoutOrders(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushReal(0.0f);

    if (abl.Brain.Warrior && abl.Brain.Warrior->TimeOfLastOrders >= 0.0f)
    {
        abl.Top().Real = ScenarioTime - abl.Brain.Warrior->TimeOfLastOrders;
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetChallenger(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t challengerId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (IsGroupId(challengerId))
    {
        abl.Top().Integer = -1;
    }
    else
    {
        MCGameObject* challenger = FindObject(abl, challengerId);
        MCGameObject* object = FindObject(abl, partId);

        if (object && IsMover(object))
        {
            static_cast<MCMover*>(object)->SetChallenger(challenger);
        }
        else
        {
            abl.Top().Integer = -2;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetMemoryInteger(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t cell = abl.Top().Integer;
    abl.Pop();
    int32_t value = abl.NextInteger();
    abl.Brain.Warrior->Memory[cell].Integer = value;
    abl.GetCodeToken();
}

auto ExecHbSetMemoryReal(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t cell = abl.Top().Integer;
    abl.Pop();
    float value = abl.NextReal();
    abl.Brain.Warrior->Memory[cell].Real = value;
    abl.GetCodeToken();
}

auto ExecHbHasMoveGoal(MCAblRuntime& abl) -> MCAblType*
{
    if (abl.Brain.Warrior && abl.Brain.Warrior->MoveOrders.ScriptGoal != 0 &&
        abl.Brain.Warrior->MoveOrders.GoalType != -1)
    {
        abl.PushInteger(1);
    }
    else
    {
        abl.PushInteger(0);
    }

    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbHasMovePath(MCAblRuntime& abl) -> MCAblType*
{
    if (abl.Brain.Warrior && abl.Brain.Warrior->GetMovePath() && abl.Brain.Warrior->MoveOrders.ScriptGoal == 0)
    {
        abl.PushInteger(1);
    }
    else
    {
        abl.PushInteger(0);
    }

    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSortWeapons(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();
    int32_t listSize = abl.NextInteger();
    int32_t sortType = abl.NextInteger();
    int32_t valueList[48];

    if (abl.Brain.Object && IsMover(abl.Brain.Object))
    {
        static_cast<MCMover*>(abl.Brain.Object)->SortWeapons(weaponList, valueList, listSize, sortType, 1);
    }

    abl.GetCodeToken();
}

auto ExecHbGetObjectPosition(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    float* position = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    abl.PushInteger(0);

    if (IsGroupId(partId))
    {
        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;
    }
    else
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            MCVector3D objectPosition = object->GetPosition();
            position[0] = objectPosition.X;
            position[1] = objectPosition.Y;
            position[2] = objectPosition.Z;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetVisualRange(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;

    if (IsGroupId(partId))
    {
        abl.Top().Real = -1.0f;
    }
    else
    {
        // Original behaviour: for an object that isn't a mover the part id stays as the (real) result.
        MCGameObject* object = FindObject(abl, partId);

        if (object && IsMover(object))
        {
            abl.Top().Real = static_cast<MCMover*>(object)->GetVisualRange();
        }
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetMemoryInteger(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = abl.Brain.Warrior->Memory[abl.Top().Integer].Integer;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMemoryReal(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Real = abl.Brain.Warrior->Memory[abl.Top().Integer].Real;
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetAlarmTriggers(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    uint32_t* triggerList = reinterpret_cast<uint32_t*>(abl.NextReference());
    abl.Top().Integer =
        abl.Brain.Warrior->GetAlarmTriggers(static_cast<MCPilotAlarmType>(abl.Brain.Alarm), triggerList);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetUnitMates(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.GetCodeToken();
    int32_t* mateList = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Top().Integer = 0;
    std::vector<MCMover*> movers;

    if (IsGroupId(partId))
    {
        movers = GetGroupMovers(partId);
    }
    else
    {
        MCGameObject* object = FindObject(abl, partId);

        if (!object || !IsMover(object) || !static_cast<MCMover*>(object)->Group)
        {
            abl.GetCodeToken();
            return IntegerTypePtr;
        }

        movers = GroupMovers(static_cast<MCMover*>(object)->Group);
    }

    for (size_t i = 0; i < movers.size(); i++)
    {
        mateList[i] = movers[i]->PartId;
    }

    abl.Top().Integer = static_cast<int32_t>(movers.size());
    abl.GetCodeToken();
    return IntegerTypePtr;
}
