#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
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

auto ExecHbGetId(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.PushInteger(0);

    if (abl.Brain.Object())
    {
        abl.Top().Integer = abl.Brain.Object()->PartId;
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetTime(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.PushReal(ActualTime);
    abl.GetCodeToken();
    return MCAblTypeId::Real;
}

auto ExecHbGetTimeLeft(MCAblRuntime& abl) -> MCAblTypeId
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
    return MCAblTypeId::Real;
}

auto ExecHbGetTarget(MCAblRuntime& abl) -> MCAblTypeId
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
            std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

            if (object && IsMover(*object))
            {
                std::shared_ptr<MCMechWarrior> pilot = object->GetPilot();
                Assert(pilot != nullptr, 0, " execHbGetTarget:No pilot in mover! ");
                std::shared_ptr<MCGameObject> target = pilot->GetLastTarget();

                if (target)
                {
                    abl.Top().Integer = target->PartId;
                }
            }
        }
    }
    else
    {
        std::shared_ptr<MCGameObject> target = abl.Brain.Group()->GetPointPilot()->GetLastTarget();

        // Port fix: the original reads the part id of a null target.
        if (target)
        {
            abl.Top().Integer = target->PartId;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbSetTarget(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t targetId = abl.NextInteger();
    std::shared_ptr<MCGameObject> target = FindObject(abl, targetId);

    if (IsGroupId(partId))
    {
        const std::vector<std::shared_ptr<MCMover>> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            std::shared_ptr<MCMechWarrior> pilot = movers[i]->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                std::static_pointer_cast<MCMover>(LockObject(pilot->Vehicle))->CalcOptimalRange(nullptr);
            }
        }
    }
    else
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object)
        {
            std::shared_ptr<MCMechWarrior> pilot = object->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                std::static_pointer_cast<MCMover>(LockObject(pilot->Vehicle))->CalcOptimalRange(nullptr);
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSelectUnit(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = -1;
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbSelectObject(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t previousId = 0;
    abl.ExecExpression();

    if (abl.Brain.Object())
    {
        previousId = abl.Brain.Object()->PartId;
    }

    std::shared_ptr<MCBaseObject> object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object)
    {
        abl.Brain.SetObject(std::static_pointer_cast<MCGameObject>(object));
        abl.Top().Integer = previousId;
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbSelectWarrior(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Top().Integer = -1;
    int32_t previousIndex = 0;

    if (abl.Brain.Warrior())
    {
        previousIndex = abl.Brain.Warrior()->Index;
    }

    abl.Top().Integer = previousIndex;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario()->NumWarriors())
    {
        abl.Brain.SetWarrior(Scenario()->Warrior(warriorIndex));
    }
    else
    {
        abl.Brain.SetWarrior({});
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetWarriorStatus(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario()->NumWarriors())
    {
        std::shared_ptr<MCMechWarrior> warrior = Scenario()->Warrior(warriorIndex);

        if (warrior)
        {
            abl.Top().Integer = warrior->Status;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetContacts(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<int32_t> contacts = abl.NextReference<int32_t>();
    abl.Pop();
    int32_t contactCriteria = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t sortType = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(*abl.Brain.Object()))
    {
        // The script's list takes as many as the team's contact list holds (the original wrote it unchecked).
        const std::shared_ptr<MCTeam> team = abl.Brain.Object()->GetTeam();
        const int32_t maxContacts = (contactCriteria & 0x10) != 0 ? team->NumSensorContacts() : team->NumLosContacts();
        abl.Top().Integer = team->GetContacts(
            *abl.Brain.Object(), ArrayArgument(contacts, static_cast<size_t>(maxContacts)), contactCriteria, sortType);
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetEnemyCount(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (!IsGroupId(partId))
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object)
        {
            if (IsMover(*object))
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
    return MCAblTypeId::Integer;
}

auto ExecHbSelectContact(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(*abl.Brain.Object()))
    {
        std::shared_ptr<MCGameObject> contact = FindObject(abl, partId);

        if (contact && abl.Brain.Object()->GetTeam()->GetContactType(*contact) != 0)
        {
            abl.Brain.SetContact(contact);
            abl.Top().Integer = 0;
        }
        else
        {
            abl.Top().Integer = 1;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbIsContact(MCAblRuntime& abl) -> MCAblTypeId
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

    if (IsMover(*abl.Brain.Object()))
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (abl.Brain.Object()->GetTeam()->IsContact(*object, contactCriteria) == 0)
        {
            abl.Top().Integer = 0;
        }
        else
        {
            abl.Top().Integer = partId;

            if (select)
            {
                abl.Brain.SetContact(object);
            }
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetContactId(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.PushInteger(abl.Brain.Contact() ? abl.Brain.Contact()->PartId : 0);
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetContactStatus(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<int32_t> tagged = abl.NextReference<int32_t>();
    abl.Top().Integer = 0;

    tagged[0] = 0;

    if (abl.Brain.Contact())
    {
        int taggedFlag;
        abl.Top().Integer = abl.Brain.Contact()->GetContactType(abl.Brain.Object()->GetTeam()->Id, taggedFlag);
        tagged[0] = (taggedFlag == 1) ? 1 : 0;
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetContactRelativePosition(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<float> range = abl.NextReference<float>();
    abl.Pop();
    abl.GetCodeToken();
    std::span<float> angle = abl.NextReference<float>();
    range[0] = -1.0f;
    abl.Top().Integer = 1;

    angle[0] = 0.0f;

    if (abl.Brain.Contact() && abl.Brain.Object())
    {
        MCVector3D contactPosition = abl.Brain.Contact()->GetPosition();
        range[0] = static_cast<float>(abl.Brain.Object()->DistanceFrom(contactPosition));
        angle[0] = abl.Brain.Object()->RelFacingTo(abl.Brain.Contact()->GetPosition(), -1);
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbSetPotentialContact(MCAblRuntime& abl) -> MCAblTypeId
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
        const std::vector<std::shared_ptr<MCMover>> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            movers[i]->SetPotentialContact(contactType);
        }
    }
    else
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object)
        {
            object->SetPotentialContact(contactType);
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetWeapons(MCAblRuntime& abl, MCAblRoutineKey key) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<int32_t> weaponList = abl.NextReference<int32_t>();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t listSize = abl.Top().Integer;
    std::shared_ptr<MCGameObject> target = abl.Brain.Warrior()->GetLastTarget();
    abl.Top().Integer = -1;

    if (IsMover(*abl.Brain.Object()))
    {
        auto& mover = static_cast<MCMover&>(*abl.Brain.Object());
        const std::span<int32_t> weapons =
            ArrayArgument(weaponList, static_cast<size_t>(std::max<int32_t>(listSize, mover.NumWeapons)));

        if (key == MCAblRoutineKey::GetWeaponsReady)
        {
            abl.Top().Integer = mover.GetWeaponsReady(weapons, listSize);
        }
        else if (key == MCAblRoutineKey::GetWeaponsInRange && target)
        {
            MCVector3D targetPosition = target->GetPosition();
            abl.Top().Integer =
                mover.GetWeaponsInRange(weapons, listSize, static_cast<float>(mover.DistanceFrom(targetPosition)));
        }
        else
        {
            abl.Top().Integer = mover.GetWeaponsLocked(weapons, listSize);
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetWeaponShots(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t weaponIndex = abl.Top().Integer;
    abl.Top().Integer = -1;

    if (IsMover(*abl.Brain.Object()))
    {
        abl.Top().Integer = static_cast<MCMover&>(*abl.Brain.Object()).GetWeaponShots(weaponIndex);
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetWeaponRanges(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    std::span<float> ranges = abl.NextReference<float>();
    abl.Pop();
    std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

    if (object && IsMover(*object))
    {
        const auto mover = std::static_pointer_cast<MCMover>(object);

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

auto ExecHbSetMoveGoal(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t goalType = static_cast<uint32_t>(abl.Top().Integer);
    abl.Pop();
    abl.GetCodeToken();
    std::span<float> location = abl.NextReference<float>();

    if (abl.Brain.Warrior())
    {
        MCVector3D goal;
        goal.X = location[0];
        goal.Y = location[1];
        goal.Z = location[2];
        abl.Brain.Warrior()->SetMoveGoal(goalType, goal, nullptr);
        location[0] = goal.X;
        location[1] = goal.Y;
        location[2] = goal.Z;
        abl.Brain.Warrior()->MoveOrders.ScriptGoal = 1;
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetChallenger(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (!IsGroupId(partId))
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object && IsMover(*object))
        {
            std::shared_ptr<MCGameObject> challenger = static_cast<MCMover&>(*object).GetChallenger();

            if (challenger)
            {
                abl.Top().Integer = challenger->PartId;
            }
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetFireRanges(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<float> ranges = abl.NextReference<float>();
    abl.Pop();
    ranges[0] = WeaponRange[0];
    ranges[1] = WeaponRange[1];
    ranges[2] = WeaponRange[2];
    ranges[3] = Scenario()->MaxWeaponRange;
    abl.GetCodeToken();
    return MCAblTypeId::None;
}

auto ExecHbGetAttackers(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<uint32_t> attackerList = abl.NextReference<uint32_t>();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    float seconds = abl.Top().Real;
    abl.Top().Integer = 0;

    if (abl.Brain.Warrior())
    {
        const std::vector<uint32_t> attackers = abl.Brain.Warrior()->GetAttackers(seconds);
        // Only as many as the script's array holds (OB-173).
        std::ranges::copy(std::span(attackers).first(std::min(attackers.size(), attackerList.size())),
                          attackerList.begin());
        abl.Top().Integer = static_cast<int32_t>(attackers.size());
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetAttackerInfo(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t attackerId = static_cast<uint32_t>(abl.Top().Integer);
    abl.Top().Real = 1000000.0f;

    if ((attackerId == 0 || attackerId > 0x1ff) && abl.Brain.Warrior())
    {
        const std::optional<MCAttackerRec> attackerRec = abl.Brain.Warrior()->GetAttackerInfo(attackerId);

        if (attackerRec)
        {
            abl.Top().Real = ScenarioTime - attackerRec->LastTime;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Real;
}

auto ExecHbGetTimeWithoutOrders(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.PushReal(0.0f);

    if (abl.Brain.Warrior() && abl.Brain.Warrior()->TimeOfLastOrders >= 0.0f)
    {
        abl.Top().Real = ScenarioTime - abl.Brain.Warrior()->TimeOfLastOrders;
    }

    abl.GetCodeToken();
    return MCAblTypeId::Real;
}

auto ExecHbSetChallenger(MCAblRuntime& abl) -> MCAblTypeId
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
        std::shared_ptr<MCGameObject> challenger = FindObject(abl, challengerId);
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object && IsMover(*object))
        {
            static_cast<MCMover&>(*object).SetChallenger(challenger);
        }
        else
        {
            abl.Top().Integer = -2;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbSetMemoryInteger(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t cell = abl.Top().Integer;
    abl.Pop();
    int32_t value = abl.NextInteger();
    abl.Brain.Warrior()->Memory[cell].Integer = value;
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
    abl.Brain.Warrior()->Memory[cell].Real = value;
    abl.GetCodeToken();
}

auto ExecHbHasMoveGoal(MCAblRuntime& abl) -> MCAblTypeId
{
    if (abl.Brain.Warrior() && abl.Brain.Warrior()->MoveOrders.ScriptGoal != 0 &&
        abl.Brain.Warrior()->MoveOrders.GoalType != -1)
    {
        abl.PushInteger(1);
    }
    else
    {
        abl.PushInteger(0);
    }

    abl.GetCodeToken();
    return MCAblTypeId::Boolean;
}

auto ExecHbHasMovePath(MCAblRuntime& abl) -> MCAblTypeId
{
    if (abl.Brain.Warrior() && abl.Brain.Warrior()->PollMovePath() && abl.Brain.Warrior()->MoveOrders.ScriptGoal == 0)
    {
        abl.PushInteger(1);
    }
    else
    {
        abl.PushInteger(0);
    }

    abl.GetCodeToken();
    return MCAblTypeId::Boolean;
}

auto ExecHbSortWeapons(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<int32_t> weaponList = abl.NextReference<int32_t>();
    abl.Pop();
    int32_t listSize = abl.NextInteger();
    int32_t sortType = abl.NextInteger();
    int32_t valueList[48];

    if (abl.Brain.Object() && IsMover(*abl.Brain.Object()))
    {
        auto& mover = static_cast<MCMover&>(*abl.Brain.Object());
        mover.SortWeapons(ArrayArgument(weaponList, static_cast<size_t>(std::max<int32_t>(listSize, mover.NumWeapons))),
                          valueList, listSize, sortType, 1);
    }

    abl.GetCodeToken();
}

auto ExecHbGetObjectPosition(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    std::span<float> position = abl.NextReference<float>();
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
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object)
        {
            MCVector3D objectPosition = object->GetPosition();
            position[0] = objectPosition.X;
            position[1] = objectPosition.Y;
            position[2] = objectPosition.Z;
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetVisualRange(MCAblRuntime& abl) -> MCAblTypeId
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
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (object && IsMover(*object))
        {
            abl.Top().Real = static_cast<MCMover&>(*object).GetVisualRange();
        }
    }

    abl.GetCodeToken();
    return MCAblTypeId::Real;
}

auto ExecHbGetMemoryInteger(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = abl.Brain.Warrior()->Memory[abl.Top().Integer].Integer;
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetMemoryReal(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Real = abl.Brain.Warrior()->Memory[abl.Top().Integer].Real;
    abl.GetCodeToken();
    return MCAblTypeId::Real;
}

auto ExecHbGetAlarmTriggers(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    std::span<uint32_t> triggerList = abl.NextReference<uint32_t>();
    abl.Top().Integer = abl.Brain.Warrior()->GetAlarmTriggers(static_cast<MCPilotAlarmType>(abl.Brain.Alarm),
                                                              ArrayArgument(triggerList, MCPilotAlarm::MaxTriggers));
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}

auto ExecHbGetUnitMates(MCAblRuntime& abl) -> MCAblTypeId
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.GetCodeToken();
    std::span<int32_t> mateList = abl.NextReference<int32_t>();
    abl.Top().Integer = 0;
    std::vector<std::shared_ptr<MCMover>> movers;

    if (IsGroupId(partId))
    {
        movers = GetGroupMovers(partId);
    }
    else
    {
        std::shared_ptr<MCGameObject> object = FindObject(abl, partId);

        if (!object || !IsMover(*object) || !static_cast<MCMover&>(*object).Group.lock())
        {
            abl.GetCodeToken();
            return MCAblTypeId::Integer;
        }

        movers = GroupMovers(*static_cast<MCMover&>(*object).Group.lock());
    }

    for (size_t i = 0; i < movers.size(); i++)
    {
        mateList[i] = movers[i]->PartId;
    }

    abl.Top().Integer = static_cast<int32_t>(movers.size());
    abl.GetCodeToken();
    return MCAblTypeId::Integer;
}
