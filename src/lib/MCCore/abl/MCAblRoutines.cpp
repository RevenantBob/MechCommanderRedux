#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "ai/MCMoveSystem.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
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

// The helpers the routines share, and the dispatch from a routine key to its routine.

auto IsMover(MCBaseObject* object) -> bool
{
    MCObjectClass objectClass = object->ObjectClass;
    return objectClass == MCObjectClass::BattleMech || objectClass == MCObjectClass::GroundVehicle ||
           objectClass == MCObjectClass::Elemental || objectClass == MCObjectClass::Mover;
}

auto FindObject(MCAblRuntime& abl, int32_t partId) -> MCGameObject*
{
    if (partId == -1)
    {
        return abl.Brain.Object;
    }

    return static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(partId));
}

auto IsGroupId(int32_t partId) -> bool
{
    return partId >= 1 && partId <= 0x1ff;
}

auto GroupMovers(MCMoverGroup* group) -> std::vector<MCMover*>
{
    std::vector<MCMover*> movers(MCMoverGroup::MaxMovers);
    movers.resize(static_cast<size_t>(group->GetMovers(movers.data())));
    return movers;
}

namespace
{
    /// <summary>The movers on <paramref name="team"/>'s roster.</summary>
    auto TeamMovers(MCTeam* team) -> std::vector<MCMover*>
    {
        std::vector<MCMover*> movers(static_cast<size_t>(team->RosterSize()));
        movers.resize(static_cast<size_t>(team->GetRoster(reinterpret_cast<MCGameObject**>(movers.data()))));
        return movers;
    }
}

auto GetGroupMovers(int32_t groupId) -> std::vector<MCMover*>
{
    if (groupId < 0x21)
    {
        return GroupMovers(CommanderById(0)->GetGroup(groupId - 1));
    }

    if (groupId >= 0x149 && groupId < 0x169)
    {
        return GroupMovers(CommanderById(2)->GetGroup(groupId - 0x149));
    }

    if (groupId >= 0xa5 && groupId < 0xc5)
    {
        return GroupMovers(CommanderById(1)->GetGroup(groupId - 0xa5));
    }

    if (groupId == 500)
    {
        return TeamMovers(InnerSphereTeam());
    }

    if (groupId == 0x1f6)
    {
        if (AlliedTeam() == nullptr)
        {
            return {};
        }

        return TeamMovers(AlliedTeam());
    }

    if (groupId == 0x1f5)
    {
        return TeamMovers(ClanTeam());
    }

    return {};
}

auto FindWarrior(MCAblRuntime& abl, int32_t warriorIndex) -> MCMechWarrior*
{
    if (warriorIndex == -1)
    {
        return abl.Brain.Warrior;
    }

    if (warriorIndex < 1 || static_cast<uint32_t>(warriorIndex) > Scenario()->NumWarriors())
    {
        return nullptr;
    }

    return Scenario()->Warrior(warriorIndex);
}

auto GetDamageLevel(MCGameObject* object, uint32_t& damageLevel) -> bool
{
    MCObjectType* type = object->GetObjectType();

    switch (object->ObjectClass)
    {
        case MCObjectClass::Building:
        {
            damageLevel = static_cast<MCBuildingType*>(type)->DmgLevel;
            return true;
        }
        case MCObjectClass::Turret:
        {
            damageLevel = static_cast<MCTurretType*>(type)->DmgLevel;
            return true;
        }
        case MCObjectClass::TerrainObject:
        {
            damageLevel = static_cast<MCTerrainObjectType*>(type)->DmgLevel;
            return true;
        }
        case MCObjectClass::TreeBuilding:
        {
            damageLevel = static_cast<MCTreeBuildingType*>(type)->DmgLevel;
            return true;
        }
        case MCObjectClass::MiscTerrainObject:
        {
            MCMiscTerrainObjectType* miscType = static_cast<MCMiscTerrainObjectType*>(type);

            switch (static_cast<MCMiscTerrainObject*>(object)->Kind)
            {
                case MCMiscTerrainKind::Bridge:
                {
                    damageLevel = miscType->BridgeDmgLevel;
                    return true;
                }
                case MCMiscTerrainKind::Forest:
                {
                    damageLevel = miscType->ForestDmgLevel;
                    return true;
                }
                case MCMiscTerrainKind::Wall:
                {
                    damageLevel = miscType->WallDmgLevel;
                    return true;
                }
                case MCMiscTerrainKind::MediumWall:
                {
                    damageLevel = miscType->MediumWallDmgLevel;
                    return true;
                }
                case MCMiscTerrainKind::LightWall:
                {
                    damageLevel = miscType->LightWallDmgLevel;
                    return true;
                }
                default:
                    return false;
            }
        }

        default:
            return false;
    }
}

namespace
{
    /// <summary>Runs a routine and gives its result type.</summary>
    using MCAblRoutineHandler = MCAblType* (*)(MCAblRuntime&);

    /// <summary>A routine with a result type.</summary>
    template <auto Routine> auto Value(MCAblRuntime& abl) -> MCAblType*
    {
        return Routine(abl);
    }

    /// <summary>A routine the dispatch gives no result type (whatever it returns).</summary>
    template <auto Routine> auto Void(MCAblRuntime& abl) -> MCAblType*
    {
        Routine(abl);
        return nullptr;
    }

    /// <summary>getweaponsready, getweaponslocked, getweaponsinrange.</summary>
    template <MCAblRoutineKey Key> auto Weapons(MCAblRuntime& abl) -> MCAblType*
    {
        return ExecHbGetWeapons(abl, Key);
    }

    /// <summary>The routine of each key that has one (the original's execStandardRoutineCall switch).</summary>
    constexpr auto Handlers = []
    {
        std::array<MCAblRoutineHandler, static_cast<size_t>(MCAblRoutineKey::Count)> table{};
        auto set = [&](MCAblRoutineKey key, MCAblRoutineHandler handler) { table[static_cast<size_t>(key)] = handler; };
        set(MCAblRoutineKey::Return, Void<ExecStdReturn>);
        set(MCAblRoutineKey::Print, Void<ExecStdPrint>);
        set(MCAblRoutineKey::Concat, Value<ExecStdConcat>);
        set(MCAblRoutineKey::Abs, Value<ExecStdAbs>);
        set(MCAblRoutineKey::Round, Value<ExecStdRound>);
        set(MCAblRoutineKey::Sqrt, Value<ExecStdSqrt>);
        set(MCAblRoutineKey::Trunc, Value<ExecStdTrunc>);
        set(MCAblRoutineKey::Random, Value<ExecStdRandom>);
        set(MCAblRoutineKey::SetMaxLoops, Value<ExecStdSetMaxLoops>);
        set(MCAblRoutineKey::Fatal, Value<ExecStdFatal>);
        set(MCAblRoutineKey::Assert, Value<ExecStdAssert>);
        set(MCAblRoutineKey::GetModuleHandle, Value<ExecStdGetModHandle>);
        set(MCAblRoutineKey::GetId, Value<ExecHbGetId>);
        set(MCAblRoutineKey::GetTime, Value<ExecHbGetTime>);
        set(MCAblRoutineKey::GetTimeLeft, Value<ExecHbGetTimeLeft>);
        set(MCAblRoutineKey::GetWarriorStatus, Value<ExecHbGetWarriorStatus>);
        set(MCAblRoutineKey::SelectUnit, Value<ExecHbSelectUnit>);
        set(MCAblRoutineKey::SelectWarrior, Value<ExecHbSelectWarrior>);
        set(MCAblRoutineKey::SelectObject, Value<ExecHbSelectObject>);
        set(MCAblRoutineKey::GetContacts, Value<ExecHbGetContacts>);
        set(MCAblRoutineKey::GetEnemyCount, Value<ExecHbGetEnemyCount>);
        set(MCAblRoutineKey::SelectContact, Value<ExecHbSelectContact>);
        set(MCAblRoutineKey::GetContactId, Value<ExecHbGetContactId>);
        set(MCAblRoutineKey::IsContact, Value<ExecHbIsContact>);
        set(MCAblRoutineKey::GetContactStatus, Value<ExecHbGetContactStatus>);
        set(MCAblRoutineKey::GetContactRelativePosition, Value<ExecHbGetContactRelativePosition>);
        set(MCAblRoutineKey::GetTarget, Value<ExecHbGetTarget>);
        set(MCAblRoutineKey::SetTarget, Void<ExecHbSetTarget>);
        set(MCAblRoutineKey::GetWeaponsReady, Weapons<MCAblRoutineKey::GetWeaponsReady>);
        set(MCAblRoutineKey::GetWeaponsLocked, Weapons<MCAblRoutineKey::GetWeaponsLocked>);
        set(MCAblRoutineKey::GetWeaponsInRange, Weapons<MCAblRoutineKey::GetWeaponsInRange>);
        set(MCAblRoutineKey::GetWeaponShots, Value<ExecHbGetWeaponShots>);
        set(MCAblRoutineKey::GetWeaponRanges, Void<ExecHbGetWeaponRanges>);
        set(MCAblRoutineKey::GetObjectPosition, Value<ExecHbGetObjectPosition>);
        set(MCAblRoutineKey::GetIntegerMemory, Value<ExecHbGetMemoryInteger>);
        set(MCAblRoutineKey::GetRealMemory, Value<ExecHbGetMemoryReal>);
        set(MCAblRoutineKey::GetAlarmTriggers, Value<ExecHbGetAlarmTriggers>);
        set(MCAblRoutineKey::GetChallenger, Value<ExecHbGetChallenger>);
        set(MCAblRoutineKey::GetFireRanges, Value<ExecHbGetFireRanges>);
        set(MCAblRoutineKey::GetAttackers, Value<ExecHbGetAttackers>);
        set(MCAblRoutineKey::GetAttackerInfo, Value<ExecHbGetAttackerInfo>);
        set(MCAblRoutineKey::SetChallenger, Value<ExecHbSetChallenger>);
        set(MCAblRoutineKey::GetTimeWithoutOrders, Value<ExecHbGetTimeWithoutOrders>);
        set(MCAblRoutineKey::SetRadio, Void<ExecHbSetRadio>);
        set(MCAblRoutineKey::SetMoveGoal, Value<ExecHbSetMoveGoal>);
        set(MCAblRoutineKey::SetIntegerMemory, Void<ExecHbSetMemoryInteger>);
        set(MCAblRoutineKey::SetRealMemory, Void<ExecHbSetMemoryReal>);
        set(MCAblRoutineKey::HasMoveGoal, Value<ExecHbHasMoveGoal>);
        set(MCAblRoutineKey::HasMovePath, Value<ExecHbHasMovePath>);
        set(MCAblRoutineKey::SortWeapons, Void<ExecHbSortWeapons>);
        set(MCAblRoutineKey::GetVisualRange, Value<ExecHbGetVisualRange>);
        set(MCAblRoutineKey::GetUnitMates, Value<ExecHbGetUnitMates>);
        set(MCAblRoutineKey::GetTacOrder, Value<ExecHbGetTacOrder>);
        set(MCAblRoutineKey::GetLastTacOrder, Value<ExecHbGetLastTacOrder>);
        set(MCAblRoutineKey::SetOrderMode, Value<ExecHbSetOrderMode>);
        set(MCAblRoutineKey::OrderWait, Value<ExecHbWait>);
        set(MCAblRoutineKey::OrderMoveTo, Value<ExecHbMoveToPoint>);
        set(MCAblRoutineKey::OrderMoveToObject, Value<ExecHbMoveToObject>);
        set(MCAblRoutineKey::OrderMoveToContact, Value<ExecHbMoveToContact>);
        set(MCAblRoutineKey::OrderTraversePath, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::OrderPatrolPath, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::AttackClosestTarget, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::AttackPerOrders, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::Retreat, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::FireUponEnemyFireOnly, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::OrderPowerUp, Value<ExecHbOrderPowerUp>);
        set(MCAblRoutineKey::OrderPowerDown, Value<ExecHbOrderPowerDown>);
        set(MCAblRoutineKey::OrderAttackObject, Value<ExecHbOrderAttackObject>);
        set(MCAblRoutineKey::OrderAttackContact, Value<ExecHbOrderAttackContact>);
        set(MCAblRoutineKey::OrderWithdraw, Value<ExecHbObjWithdraw>);
        set(MCAblRoutineKey::DamageObject, Value<ExecHbDamageObject>);
        set(MCAblRoutineKey::SetAttackRadius, Value<ExecHbSetAttackRadius>);
        set(MCAblRoutineKey::OrderTest, Value<ExecHbOrderTest>);
        set(MCAblRoutineKey::PlaySmacker, Value<ExecHbPlaySmacker>);
        set(MCAblRoutineKey::ObjectChangeSides, Void<ExecHbObjectChangeSides>);
        set(MCAblRoutineKey::DistanceToObject, Value<ExecHbDistanceToObject>);
        set(MCAblRoutineKey::DistanceToPosition, Value<ExecHbDistanceToPosition>);
        set(MCAblRoutineKey::ObjectSuicide, Void<ExecHbObjectSuicide>);
        set(MCAblRoutineKey::ObjectCreate, Value<ExecHbObjectCreate>);
        set(MCAblRoutineKey::ObjectExists, Value<ExecHbObjectExists>);
        set(MCAblRoutineKey::ObjectStatus, Value<ExecHbObjectStatus>);
        set(MCAblRoutineKey::ObjectVisible, Value<ExecHbObjectVisible>);
        set(MCAblRoutineKey::ObjectClass, Value<ExecHbObjectClass>);
        set(MCAblRoutineKey::ObjectSide, Value<ExecHbObjectSide>);
        set(MCAblRoutineKey::ObjectCommander, Value<ExecHbObjectCommander>);
        set(MCAblRoutineKey::SetTimer, Value<ExecHbSetTimer>);
        set(MCAblRoutineKey::CheckTimer, Value<ExecHbChkTimer>);
        set(MCAblRoutineKey::EndTimer, Void<ExecHbEndTimer>);
        set(MCAblRoutineKey::SetObjectiveTimer, Value<ExecHbSetObjectiveTimer>);
        set(MCAblRoutineKey::CheckObjectiveTimer, Value<ExecHbCheckObjectiveTimer>);
        set(MCAblRoutineKey::SetObjectiveStatus, Value<ExecHbSetObjectiveStatus>);
        set(MCAblRoutineKey::CheckObjectiveStatus, Value<ExecHbCheckObjectiveStatus>);
        set(MCAblRoutineKey::SetObjectiveType, Value<ExecHbSetObjectiveType>);
        set(MCAblRoutineKey::CheckObjectiveType, Value<ExecHbCheckObjectiveType>);
        set(MCAblRoutineKey::PlayDigitalMusic, Value<ExecHbPlayDigitalMusic>);
        set(MCAblRoutineKey::StopMusic, Value<ExecHbStopMusic>);
        set(MCAblRoutineKey::PlaySoundEffect, Value<ExecHbPlaySoundEffect>);
        set(MCAblRoutineKey::PlayVideo, Value<ExecHbPlayVideo>);
        set(MCAblRoutineKey::PlaySpeech, Value<ExecHbPlaySpeech>);
        set(MCAblRoutineKey::PlayBetty, Value<ExecHbPlayBetty>);
        set(MCAblRoutineKey::SetObjectActive, Value<ExecHbSetObjActive>);
        set(MCAblRoutineKey::ObjectInWithdrawal, Value<ExecHbObjInWithdraw>);
        set(MCAblRoutineKey::ObjectTypeId, Value<ExecHbObjTypeId>);
        set(MCAblRoutineKey::GetTerrainObjectPartId, Value<ExecHbTerrainObjectId>);
        set(MCAblRoutineKey::GetVehiclePartId, Value<ExecHbVehicleId>);
        set(MCAblRoutineKey::GetWeaponAmmo, Value<ExecHbGetWeaponAmmo>);
        set(MCAblRoutineKey::ObjectStatusCount, Value<ExecHbObjectStatusCount>);
        set(MCAblRoutineKey::InArea, Value<ExecHbInArea>);
        set(MCAblRoutineKey::GetRelativePositionToPoint, Void<ExecHbRelPosPoint>);
        set(MCAblRoutineKey::GetRelativePositionToObject, Void<ExecHbRelPosObject>);
        set(MCAblRoutineKey::GetSensorsWorking, Value<ExecHbGetSensors>);
        set(MCAblRoutineKey::GetCurrentBRValue, Value<ExecHbGetBRValue>);
        set(MCAblRoutineKey::GetArmorPts, Value<ExecHbGetArmorPts>);
        set(MCAblRoutineKey::GetPilotId, Value<ExecHbGetPilotId>);
        set(MCAblRoutineKey::GetPilotWounds, Value<ExecHbGetPilotWounds>);
        set(MCAblRoutineKey::SetPilotWounds, Void<ExecHbSetPilotWounds>);
        set(MCAblRoutineKey::GetObjectActive, Value<ExecHbGetObjActive>);
        set(MCAblRoutineKey::GetObjectMaxDmg, Value<ExecHbGetObjDmgPts>);
        set(MCAblRoutineKey::GetObjectDamage, Value<ExecHbGetObjDamage>);
        set(MCAblRoutineKey::SetObjectDamage, Void<ExecHbSetObjDamage>);
        set(MCAblRoutineKey::GetGlobalValue, Value<ExecHbGetGlobalValue>);
        set(MCAblRoutineKey::SetGlobalValue, Void<ExecHbSetGlobalValue>);
        set(MCAblRoutineKey::SetObjectivePos, Void<ExecHbSetObjectivePos>);
        set(MCAblRoutineKey::SetPotentialContact, Value<ExecHbSetPotentialContact>);
        set(MCAblRoutineKey::SetSensorRange, Void<ExecHbSetSensorRange>);
        set(MCAblRoutineKey::SetTonnage, Void<ExecHbSetTonnage>);
        set(MCAblRoutineKey::PlayWaveFile, Void<ExecHbPlayWave>);
        set(MCAblRoutineKey::SetExplosionDamage, Void<ExecHbSetExplDmg>);
        set(MCAblRoutineKey::SetExplosionRadius, Void<ExecHbSetExplRad>);
        set(MCAblRoutineKey::GetSalvage, Void<ExecHbGetSalvage>);
        set(MCAblRoutineKey::SetSalvage, Void<ExecHbSetSalvage>);
        set(MCAblRoutineKey::SetSalvageStatus, Void<ExecHbSetSalvageStatus>);
        set(MCAblRoutineKey::SetAnimation, Void<ExecHbSetAnimation>);
        set(MCAblRoutineKey::SetRevealed, Void<ExecHbSetRevealed>);
        set(MCAblRoutineKey::OrderRefit, Void<ExecHbRefit>);
        set(MCAblRoutineKey::OrderCapture, Void<ExecHbCaptureObject>);
        set(MCAblRoutineKey::SetCaptured, Void<ExecHbSetCaptured>);
        set(MCAblRoutineKey::SetCaptureable, Void<ExecHbSetCaptureable>);
        set(MCAblRoutineKey::IsCaptured, Value<ExecHbIsCaptured>);
        set(MCAblRoutineKey::IsCapturable, Value<ExecHbIsCapturable>);
        set(MCAblRoutineKey::WasEverCapturable, Value<ExecHbWasEverCapturable>);
        set(MCAblRoutineKey::SetBuildingName, Void<ExecHbSetBuildingName>);
        set(MCAblRoutineKey::CallStrike, Void<ExecHbCallStrike>);
        set(MCAblRoutineKey::OrderLoadElementals, Void<ExecHbLoadElementals>);
        set(MCAblRoutineKey::OrderDeployElementals, Void<ExecHbDeployElementals>);
        set(MCAblRoutineKey::AddPrisoner, Value<ExecHbAddPrisoner>);
        set(MCAblRoutineKey::SetTrainSpeed, Void<ExecHbSetTrainSpeed>);
        set(MCAblRoutineKey::LockGateOpen, Void<ExecHbLockGateOpen>);
        set(MCAblRoutineKey::LockGateClosed, Void<ExecHbLockGateClosed>);
        set(MCAblRoutineKey::ReleaseGateLock, Void<ExecHbReleaseGateLock>);
        set(MCAblRoutineKey::IsGateOpen, Value<ExecHbIsGateOpen>);
        set(MCAblRoutineKey::CallStrikeEx, Void<ExecHbCallStrikeEx>);
        set(MCAblRoutineKey::GetUnitStatus, Value<ExecHbGetUnitStatus>);
        set(MCAblRoutineKey::Repair, Void<ExecHbRepair>);
        set(MCAblRoutineKey::GetFixed, Value<ExecHbGetFixed>);
        set(MCAblRoutineKey::GetRepairState, Value<ExecHbGetRepairState>);
        set(MCAblRoutineKey::IsTeamTargeting, Value<ExecHbIsTeamTargeting>);
        set(MCAblRoutineKey::SendMessage, Void<ExecHbSendMessage>);
        set(MCAblRoutineKey::GetMessage, Value<ExecHbGetMessage>);
        set(MCAblRoutineKey::GetHomeTeam, Value<ExecHbGetHomeTeam>);
        set(MCAblRoutineKey::SetStrikes, Void<ExecHbSetStrikes>);
        set(MCAblRoutineKey::GetStrikes, Value<ExecHbGetStrikes>);
        set(MCAblRoutineKey::IsServer, Value<ExecHbIsServer>);
        set(MCAblRoutineKey::AddStrikes, Void<ExecHbAddStrikes>);
        return table;
    }();
}

auto ExecStandardRoutineCall(MCAblRuntime& abl, MCAblRoutineKey key) -> MCAblType*
{
    if (const MCAblRoutineHandler handler = Handlers[static_cast<size_t>(key)])
    {
        return handler(abl);
    }

    // Original behaviour (OB-050): among others the module name and mode routines, the guard routines, the scans,
    // getmaxarmor, getobjectdmgpts and setcurrentbrvalue compile but have no runtime routine.
    Fatal(0, std::format(" ABL: Undefined ABL RoutineKey in {}:{}", abl.CurrentModule()->Name(), abl.LineNumber()));
}
