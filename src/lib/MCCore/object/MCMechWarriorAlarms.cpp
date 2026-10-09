#include "stdafx.h"
#include "object/MCMechWarrior.h"
#include "abl/MCAblRuntime.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectSystem.h"
#include "sound/MCRadio.h"

// The pilot's alarms: raising them, and the handlers the game runs before the brain's.

const std::array<std::string_view, NumPilotAlarms> PilotAlarmFunctionName = {"handletargetofweaponfire",
                                                                             "handlehitbyweaponfire",
                                                                             "handledamagetakenrate",
                                                                             "handledeathofmate",
                                                                             "handlecripplingoffriendlyvehicle",
                                                                             "handledestructionoffriendlyvehicle",
                                                                             "handleincapacitationofvehicle",
                                                                             "handledestructionofvehicle",
                                                                             "handlewithdraw",
                                                                             "handlemoralebreak",
                                                                             "handlecollision",
                                                                             "handleguardbreach",
                                                                             "handlekilledtarget",
                                                                             "handlematefiredweapon",
                                                                             "handleplayerorder",
                                                                             "handlenomovepath",
                                                                             "handlegateclosing"};

auto MCMechWarrior::TriggerAlarm(MCPilotAlarmType alarm, uint32_t triggerId) -> int32_t
{
    MCPilotAlarm& pilotAlarm = AlarmOf(alarm);

    if (pilotAlarm.NumTriggers == MCPilotAlarm::MaxTriggers)
    {
        return -1;
    }

    pilotAlarm.Trigger[pilotAlarm.NumTriggers] = triggerId;
    pilotAlarm.NumTriggers++;
    return 0;
}

auto MCMechWarrior::HandleAlarm(MCPilotAlarmType alarm, uint32_t triggerId) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " bad vehicle for pilot ");

    if (mover->GetAwake() == 0)
    {
        return 0;
    }

    if (alarm == MCPilotAlarmType::VehicleIncapacitated)
    {
        HandleOwnVehicleIncapacitation(triggerId);
    }
    else if (alarm == MCPilotAlarmType::VehicleDestroyed)
    {
        HandleOwnVehicleDestruction(triggerId);
    }
    else if (alarm == MCPilotAlarmType::VehicleWithdrawn)
    {
        HandleOwnVehicleWithdrawn();
    }

    MCAblSymbol* callback = BrainAlarmCallback[static_cast<size_t>(alarm)];

    if ((MPlayer == nullptr || MPlayer->IsServer != 0) && callback != nullptr)
    {
        MCAblBrainScope brain(GetGroup(), Vehicle, static_cast<int32_t>(Vehicle->ObjectClass), this);
        AblRuntime()->Brain.Alarm = std::to_underlying(alarm);
        Brain->Execute({}, callback);
    }

    return 0;
}

auto MCMechWarrior::GetAlarmTriggers(MCPilotAlarmType alarm, uint32_t* triggerList) -> int32_t
{
    const MCPilotAlarm& pilotAlarm = AlarmOf(alarm);
    std::copy_n(pilotAlarm.Trigger.begin(), pilotAlarm.NumTriggers, triggerList);
    return pilotAlarm.NumTriggers;
}

auto MCMechWarrior::CheckAlarms() -> int32_t
{
    std::optional<MCAblBrainScope> brain;

    if (Brain != nullptr)
    {
        brain.emplace(GetGroup(), Vehicle, static_cast<int32_t>(Vehicle->ObjectClass), this);
    }

    for (int32_t code = 0; code < NumPilotAlarms; code++)
    {
        const auto alarm = static_cast<MCPilotAlarmType>(code);

        if (AlarmOf(alarm).NumTriggers == 0)
        {
            continue;
        }

        switch (alarm)
        {
            case MCPilotAlarmType::TargetOfWeaponFire:
                HandleTargetOfWeaponFire();
                break;
            case MCPilotAlarmType::HitByWeaponFire:
                HandleHitByWeaponFire();
                break;
            case MCPilotAlarmType::DamageTakenRate:
                HandleDamageTakenRate();
                break;
            case MCPilotAlarmType::DeathOfMate:
                HandleUnitMateDeath();
                break;
            case MCPilotAlarmType::FriendlyVehicleCrippled:
                HandleFriendlyVehicleCrippled();
                break;
            case MCPilotAlarmType::FriendlyVehicleDestroyed:
                HandleFriendlyVehicleDestruction();
                break;
            case MCPilotAlarmType::VehicleIncapacitated:
                HandleOwnVehicleIncapacitation(0);
                break;
            case MCPilotAlarmType::VehicleDestroyed:
                HandleOwnVehicleDestruction(0);
                break;
            case MCPilotAlarmType::VehicleWithdrawn:
                HandleOwnVehicleWithdrawn();
                break;
            case MCPilotAlarmType::MoraleBreak:
                HandleMoraleBreak();
                break;
            case MCPilotAlarmType::Collision:
                HandleCollision();
                break;
            case MCPilotAlarmType::KilledTarget:
                HandleKilledTarget();
                break;
            case MCPilotAlarmType::MateFiredWeapon:
                HandleUnitMateFiredWeapon();
                break;
            case MCPilotAlarmType::PlayerOrder:
                HandlePlayerOrder();
                break;
            case MCPilotAlarmType::NoMovePath:
                HandleNoMovePath();
                break;
            case MCPilotAlarmType::GateClosing:
                HandleGateClosing();
                break;
            default:
                break;
        }

        MCAblSymbol* callback = BrainAlarmCallback[static_cast<size_t>(code)];

        if ((MPlayer == nullptr || MPlayer->IsServer != 0) && Brain != nullptr && callback != nullptr)
        {
            AblRuntime()->Brain.Alarm = code;
            Brain->Execute({}, callback);
        }

        ClearAlarm(alarm);
    }

    return 0;
}

auto MCMechWarrior::HandleTargetOfWeaponFire() -> int32_t
{
    if (Vehicle != nullptr)
    {
        TheInterface->ObjectAttacked(Vehicle->PartId);
    }

    return 0;
}

auto MCMechWarrior::HandleHitByWeaponFire() -> int32_t
{
    if (AlarmOf(MCPilotAlarmType::HitByWeaponFire).Trigger[0] != 0)
    {
        RadioMessage(MCRadioMessageType::UnderAttack, 1);
    }

    return 0;
}

auto MCMechWarrior::HandleCollision() -> int32_t
{
    ObjectList()->FindObjectFromPart(static_cast<int32_t>(AlarmOf(MCPilotAlarmType::Collision).Trigger[0]));
    return 0;
}

auto MCMechWarrior::HandleDamageTakenRate() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleUnitMateDeath() -> int32_t
{
    const int32_t mateId = static_cast<int32_t>(AlarmOf(MCPilotAlarmType::DeathOfMate).Trigger[0]);

    if (Vehicle->PartId == mateId)
    {
        return 0;
    }

    return GetMoverFromPartId(mateId) != nullptr ? 0 : -1;
}

auto MCMechWarrior::HandleFriendlyVehicleCrippled() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleFriendlyVehicleDestruction() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleIncapacitation(uint32_t cause) -> int32_t
{
    MCMover* mover = static_cast<MCMover*>(Vehicle);
    Assert(mover != nullptr, 0, " pilot has no vehicle ");

    if (cause < 2 || cause == 0x42)
    {
        mover->HandleEjection();
    }

    ClearCurTacOrder(0, 0);
    OrderState = MCOrderState::General;

    for (std::unique_ptr<MCMovePath>& path : MoveOrders.Path)
    {
        path->NumSteps = 0;
    }

    MoveOrders.Reset();
    PathManager()->Remove(this);
    AttackOrders.Reset();
    SetLastTarget(nullptr, 0, 0);
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleDestruction(uint32_t cause) -> int32_t
{
    Assert(Vehicle != nullptr, 0, "handleOwnVehicleDestruction:pilot has no vehicle ");
    return 0;
}

auto MCMechWarrior::HandleOwnVehicleWithdrawn() -> int32_t
{
    Assert(Vehicle != nullptr, 0, "handleOwnVehicleWithdrawn:pilot has no vehicle ");
    Status = 2;
    return 0;
}

auto MCMechWarrior::HandleMoraleBreak() -> int32_t
{
    MCTacticalOrder order;
    order.Reset();
    order.Reset(MCOrderOrigin::Self, MCTacticalOrderCode::Withdraw, 0);
    SetAlarmTacOrder(order, 10);
    return 0;
}

auto MCMechWarrior::HandleKilledTarget() -> int32_t
{
    MCBaseObject* target =
        ObjectList()->FindObjectFromPart(static_cast<int32_t>(AlarmOf(MCPilotAlarmType::KilledTarget).Trigger[0]));

    if (target == nullptr)
    {
        return 0;
    }

    // Count the kill and score gunnery points by what it was.
    int32_t killType = -1;
    float points = 10.0f;
    MCRadioMessageType message;

    switch (target->ObjectClass)
    {
        case MCObjectClass::BattleMech:
        {
            killType = static_cast<int32_t>(static_cast<MCGameObject*>(target)->GetMechClass());
            points = KillSkill[killType];
            NumKilled[killType][1]++;
            message = MCRadioMessageType::MechDestroyed;
            break;
        }
        case MCObjectClass::GroundVehicle:
        case MCObjectClass::Turret:
        {
            killType = 5;
            points = KillSkill[4];
            NumKilled[5][1]++;
            message = MCRadioMessageType::VehicleDestroyed;
            break;
        }
        case MCObjectClass::Elemental:
        {
            killType = 6;
            points = KillSkill[5];
            NumKilled[6][1]++;
            message = MCRadioMessageType::ObjectDestroyed;
            break;
        }
        default:
            message = MCRadioMessageType::ObjectDestroyed;
            break;
    }

    RadioMessage(message, 0);

    // A tenth for killing one of our own (or an ally's).
    if (std::abs(static_cast<MCGameObject*>(target)->GetAlignment() - Alignment) < 2)
    {
        points = points * 0.1f;
    }

    SkillPoints[SkillGunnery] = points + SkillPoints[SkillGunnery];

    if (MPlayer != nullptr && MPlayer->IsServer != 0 && killType != -1)
    {
        MPlayer->AddPilotKillStat(static_cast<MCMover*>(Vehicle), killType);
    }

    return 0;
}

auto MCMechWarrior::HandleUnitMateFiredWeapon() -> int32_t
{
    return 0;
}

auto MCMechWarrior::HandlePlayerOrder() -> int32_t
{
    if (GetVehicleStatus() == 5 && CurTacOrder.Code != MCTacticalOrderCode::PowerDown)
    {
        OrderPowerUp(0, MCOrderOrigin::Self);
    }

    return 0;
}

auto MCMechWarrior::HandleNoMovePath() -> int32_t
{
    if (CurTacOrder.Code == MCTacticalOrderCode::GetFixed)
    {
        ClearCurTacOrder(1, 0);
        RadioMessage(MCRadioMessageType::MoveBlocked, 0);
    }

    return 0;
}

auto MCMechWarrior::HandleGateClosing() -> int32_t
{
    return 0;
}
