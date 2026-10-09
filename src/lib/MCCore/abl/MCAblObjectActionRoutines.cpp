#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "gui/MCGuiSystem.h"
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

// Changing objects: objective markers, tonnage, sensors, explosions, salvage, animation, capture, names, strikes, elementals, prisoners, trains, gates, unit status, repair.

auto ExecHbSetObjectivePos(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t objectiveNumber = abl.Top().Integer;
    abl.Pop();
    float x = abl.NextReal();
    float y = abl.NextReal();
    float z = abl.NextReal();
    Scenario()->Objectives.SetPosition(objectiveNumber, x, y, z);
    abl.GetCodeToken();
}

auto ExecHbSetTonnage(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float tonnage = abl.NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            object->SetTonnage(tonnage);
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSetSensorRange(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    float range = abl.Top().Real;
    abl.Top().Integer = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            switch (object->ObjectClass)
            {
                case MCObjectClass::BattleMech:
                case MCObjectClass::GroundVehicle:
                case MCObjectClass::Elemental:
                {
                    if (static_cast<MCMover*>(object)->SensorSystem)
                    {
                        static_cast<MCMover*>(object)->SensorSystem->SetRange(range);
                    }
                    break;
                }
                case MCObjectClass::Artillery:
                {
                    static_cast<MCArtillery*>(object)->SensorRange = range;
                    static_cast<MCArtillery*>(object)->SensorSystem->SetRange(range);
                    break;
                }
                case MCObjectClass::Building:
                {
                    if (static_cast<MCBuilding*>(object)->SensorSystem)
                    {
                        static_cast<MCBuilding*>(object)->SensorSystem->SetRange(range);
                    }
                    else
                    {
                        abl.Top().Integer = -1;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSetExplDmg(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float damage = abl.NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            object->SetExplDmg(damage);
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSetExplRad(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float radius = abl.NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            object->SetExplRad(radius);
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSetSalvage(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t itemId = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t numItems = abl.Top().Integer;
    int added = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        // An object without a salvage list (not a big object) takes nothing, yet the call still succeeds.
        if (object)
        {
            object->AddSalvage(static_cast<uint8_t>(itemId), static_cast<uint8_t>(numItems));
            added = 1;
        }
    }

    abl.Top().Integer = added;
    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetSalvageStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t status = abl.Top().Integer;
    int result = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object && TacticalMap() && (IsMover(object) || object->IsBuilding()))
        {
            if (status == 1)
            {
                result = TacticalMap()->AddSalvage(object);
            }
            else
            {
                result = TacticalMap()->RemoveSalvage(object, 1);
            }
        }
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetAnimation(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    uint32_t state = static_cast<uint32_t>(abl.NextInteger());
    int32_t subState = abl.NextInteger();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            if (object->ObjectClass == MCObjectClass::Building)
            {
                MCVfxBuildingAppearance* buildingAppearance = static_cast<MCBuilding*>(object)->Appearance.get();

                if (state >= buildingAppearance->BuildType->AnimStates.size())
                {
                    buildingAppearance->AnimState = -1;
                }
                else
                {
                    buildingAppearance->AnimState = static_cast<int32_t>(state);
                }

                buildingAppearance->CurrentFrame = 0;
            }
            else if (object->ObjectClass == MCObjectClass::TreeBuilding)
            {
                static_cast<MCTreeBuilding*>(object)->Appearance->SetTypeId(static_cast<MCActorState>(state),
                                                                            static_cast<uint8_t>(subState));
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbPlayWave(MCAblRuntime& abl) -> void
{
    // Original behaviour (OB-045): only the first of the two arguments is read; the code pointer is left on the
    // comma before the second.
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Pop();
    abl.GetCodeToken();
}

auto ExecHbSetRevealed(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t teamId = abl.Top().Integer;
    abl.Pop();
    // Original behaviour: the radius is read as a real even when the script passed an integer.
    float radius = abl.NextReal();
    abl.GetCodeToken();
    float* position = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    MCVector3D looker;
    looker.X = position[0];
    looker.Y = position[1];
    looker.Z = 0.0f;
    MCVector3D lookVector;
    lookVector.X = 0.0f;
    lookVector.Y = 0.0f;
    lookVector.Z = 0.0f;
    Terrain()->MarkRadiusSeen(looker, lookVector, 360.0f, radius, static_cast<uint8_t>(teamId));

    if (teamId == 1)
    {
        InnerSphereTeam()->ScanBattlefield();
    }
    else
    {
        ClanTeam()->ScanBattlefield();
    }

    if (AlliedTeam())
    {
        AlliedTeam()->ScanBattlefield();
    }

    abl.GetCodeToken();
}

auto ExecHbGetSalvage(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t listSize = abl.NextInteger();
    abl.GetCodeToken();
    int32_t* itemIds = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();
    abl.GetCodeToken();
    int32_t* itemCounts = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();

    for (int32_t i = 0; i < listSize; i++)
    {
        itemIds[i] = -1;
        itemCounts[i] = -1;
    }

    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object)
    {
        const std::span<const MCSalvageItem> salvage = static_cast<MCGameObject*>(object)->GetSalvage();

        for (size_t i = 0; i < salvage.size() && i < static_cast<size_t>(listSize); i++)
        {
            itemIds[i] = salvage[i].ItemId;
            itemCounts[i] = salvage[i].NumItems;
        }
    }

    abl.GetCodeToken();
}

auto ExecHbRefit(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t targetId = abl.Top().Integer;
    abl.Pop();
    uint32_t params = static_cast<uint32_t>(abl.NextInteger());

    if (abl.Brain.Object && IsMover(abl.Brain.Object))
    {
        MCMechWarrior* pilot = abl.Brain.Object->GetPilot();

        if (pilot)
        {
            MCBaseObject* target = ObjectList()->FindObjectFromPart(targetId);

            if (target && target->ObjectClass == MCObjectClass::BattleMech)
            {
                pilot->OrderRefit(MCOrderOrigin::Commander, static_cast<MCGameObject*>(target), params);
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbSetCaptured(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object)
    {
        static_cast<MCGameObject*>(object)->SetCaptured();
    }

    abl.GetCodeToken();
}

auto ExecHbCaptureObject(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t targetId = abl.Top().Integer;
    abl.Pop();
    uint32_t params = static_cast<uint32_t>(abl.NextInteger());
    // Port fix: the original leaves the target register unset when the current object isn't a mover (and then
    // orders its pilot anyway).
    MCBaseObject* target = nullptr;

    if (abl.Brain.Object && IsMover(abl.Brain.Object))
    {
        target = ObjectList()->FindObjectFromPart(targetId);
    }

    if (target)
    {
        abl.Brain.Object->GetPilot()->OrderCapture(MCOrderOrigin::Commander, static_cast<MCGameObject*>(target),
                                                   params);
    }

    abl.GetCodeToken();
}

auto ExecHbSetCaptureable(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t captureable = abl.NextInteger() == 1 ? 1 : 0;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object)
    {
        if (MPlayer)
        {
            static_cast<MCGameObject*>(object)->ClearCaptured();
        }

        switch (object->ObjectClass)
        {
            case MCObjectClass::GroundVehicle:
                static_cast<MCGroundVehicle*>(object)->Captureable = captureable;
                break;
            case MCObjectClass::Building:
                static_cast<MCBuilding*>(object)->Captureable = captureable;
                break;
            case MCObjectClass::TreeBuilding:
                static_cast<MCTreeBuilding*>(object)->Captureable = captureable;
                break;
            case MCObjectClass::Turret:
                // Original behaviour (OB-044): a turret's flag goes where tree buildings keep theirs, +0x110,
                // which is the turret's lastFireTime.
                static_cast<MCTurret*>(object)->LastFireTime = std::bit_cast<float>(captureable);
                break;
            default:
                break;
        }
    }

    abl.GetCodeToken();
}

auto ExecHbIsCaptured(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    int32_t numCaptured = 0;

    if (IsGroupId(partId))
    {
        const std::vector<MCMover*> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i]->IsCaptured())
            {
                numCaptured++;
            }
        }
    }
    else
    {
        MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

        if (object && static_cast<MCGameObject*>(object)->IsCaptured())
        {
            numCaptured = 1;
        }
    }

    abl.Top().Integer = numCaptured;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsCapturable(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int captureable = 0;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object)
    {
        captureable = static_cast<MCGameObject*>(object)->IsCaptureable();
    }

    abl.Top().Integer = captureable != 0 ? 1 : 0;
    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbWasEverCapturable(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t captureable = 0;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object)
    {
        switch (object->ObjectClass)
        {
            case MCObjectClass::GroundVehicle:
                captureable = static_cast<MCGroundVehicle*>(object)->Captureable;
                break;
            case MCObjectClass::Building:
                captureable = static_cast<MCBuilding*>(object)->Captureable;
                break;
            case MCObjectClass::TreeBuilding:
                captureable = static_cast<MCTreeBuilding*>(object)->Captureable;
                break;
            case MCObjectClass::Turret:
                // Original behaviour (OB-044): reads the turret's lastFireTime bits.
                captureable = std::bit_cast<int32_t>(static_cast<MCTurret*>(object)->LastFireTime);
                break;
            default:
                break;
        }
    }

    abl.Top().Integer = captureable != 0 ? 1 : 0;
    abl.GetCodeToken();
    return BooleanTypePtr;
}

namespace
{
    /// <summary>Replaces <paramref name="name"/> with string resource <paramref name="stringId"/>.</summary>
    auto SetNameFromResource(std::string& name, uint32_t stringId) -> void
    {
        char buffer[256];
        CLoadString(ThisInstance, stringId, buffer, 0xfe);
        name = buffer;
    }

    /// <summary>What <c>__ftol</c> gives: the value truncated, or 0x80000000 for NaN or out of range.</summary>
    auto X87Ftol(double value) -> int32_t
    {
        if (!(value > -2147483649.0 && value < 2147483648.0))
        {
            return INT32_MIN;
        }

        return static_cast<int32_t>(value);
    }
}

auto ExecHbSetBuildingName(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    uint32_t stringId = static_cast<uint32_t>(abl.NextInteger());
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && static_cast<MCGameObject*>(object)->IsBuilding())
    {
        if (object->ObjectClass == MCObjectClass::Building)
        {
            SetNameFromResource(static_cast<MCBuilding*>(object)->Name, stringId);
        }

        if (object->ObjectClass == MCObjectClass::TreeBuilding)
        {
            SetNameFromResource(static_cast<MCTreeBuilding*>(object)->Name, stringId);
        }

        if (object->ObjectClass == MCObjectClass::Turret)
        {
            SetNameFromResource(static_cast<MCTurret*>(object)->Name, stringId);
        }
    }

    abl.GetCodeToken();
}

namespace
{
    /// <summary>callstrike / callstrikeex: an artillery strike on an object, or on a point at ground level.</summary>
    auto CallStrike(int32_t strikeType, int32_t targetId, MCVector3D& position, int forClansOnPoint,
                    int forClansOnTarget, float delay) -> void
    {
        MCGameObject* target = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(targetId));

        if (!target)
        {
            position.Z = Terrain()->GetTerrainElevation(position);
            TacticalInterface()->CallStrike(strikeType, &position, nullptr, false, forClansOnPoint != 0, delay);
        }
        else
        {
            TacticalInterface()->CallStrike(strikeType, nullptr, target, false, forClansOnTarget != 0, delay);
        }
    }
}

auto ExecHbCallStrike(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t strikeType = abl.Top().Integer;
    abl.Pop();
    int32_t targetId = abl.NextInteger();
    MCVector3D position;
    position.X = abl.NextReal();
    position.Y = abl.NextReal();
    position.Z = abl.NextReal();
    int forClans = abl.NextInteger() == 1 ? 1 : 0;
    // Original behaviour: the clan flag only counts for a strike on a point.
    CallStrike(strikeType, targetId, position, forClans, 0, -1.0f);
    abl.GetCodeToken();
}

auto ExecHbCallStrikeEx(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t strikeType = abl.Top().Integer;
    abl.Pop();
    int32_t targetId = abl.NextInteger();
    MCVector3D position;
    position.X = abl.NextReal();
    position.Y = abl.NextReal();
    position.Z = abl.NextReal();
    int forClans = abl.NextInteger() == 1 ? 1 : 0;
    float delay = abl.NextReal();

    if (delay < 0.0f)
    {
        delay = 0.0f;
    }

    CallStrike(strikeType, targetId, position, forClans, forClans, delay);
    abl.GetCodeToken();
}

auto ExecHbLoadElementals(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t carrierId = abl.Top().Integer;
    abl.Pop();

    if (abl.Brain.Object && abl.Brain.Object->ObjectClass == MCObjectClass::Elemental)
    {
        MCBaseObject* carrier = ObjectList()->FindObjectFromPart(carrierId);

        if (carrier && carrier->ObjectClass == MCObjectClass::GroundVehicle &&
            static_cast<MCGroundVehicle*>(carrier)->ElementalCarrier != 0)
        {
            abl.Brain.Object->GetPilot()->OrderLoadIntoCarrier(MCOrderOrigin::Commander,
                                                               static_cast<MCGameObject*>(carrier), 0);
        }
    }

    abl.GetCodeToken();
}

auto ExecHbDeployElementals(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t params = static_cast<uint32_t>(abl.Top().Integer);
    abl.Pop();

    if (abl.Brain.Object && abl.Brain.Object->ObjectClass == MCObjectClass::GroundVehicle &&
        static_cast<MCGroundVehicle*>(abl.Brain.Object)->ElementalCarrier != 0)
    {
        abl.Brain.Object->GetPilot()->OrderDeployElementals(MCOrderOrigin::Commander, params);
    }

    abl.GetCodeToken();
}

auto ExecHbAddPrisoner(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t buildingId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t pilotIndex = abl.Top().Integer;
    int32_t result = -1;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(buildingId);

    if (object && static_cast<MCGameObject*>(object)->IsBuilding() && Scenario())
    {
        // Port fix: with no warriors at all the original fills the prison with the pointer -1.
        MCMechWarrior* prisoner = nullptr;

        for (uint32_t i = 1; i <= Scenario()->NumWarriors(); i++)
        {
            MCMechWarrior* warrior = Scenario()->Warrior(i);

            if (warrior && warrior->Index == pilotIndex)
            {
                prisoner = warrior;
                break;
            }
        }

        if (prisoner)
        {
            // Original behaviour (OB-046): the prisoner goes into every empty slot, not just the first.
            std::span<MCMechWarrior*> prisonSlots;

            if (object->ObjectClass == MCObjectClass::Building)
            {
                prisonSlots = static_cast<MCBuilding*>(object)->PrisonSlots;
            }
            else if (object->ObjectClass == MCObjectClass::TreeBuilding)
            {
                prisonSlots = static_cast<MCTreeBuilding*>(object)->PrisonSlots;
            }

            for (MCMechWarrior*& slot : prisonSlots)
            {
                if (slot == nullptr)
                {
                    slot = prisoner;
                    result = 0;
                }
            }
        }
    }

    abl.Top().Integer = result;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetTrainSpeed(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float speed = abl.NextReal();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && object->ObjectClass == MCObjectClass::TrainCar)
    {
        MCTrain* train = static_cast<MCTrainCar*>(object)->Train;

        if (std::fabs(speed) > train->MaxSpeed)
        {
            speed = speed > 0.0f ? train->MaxSpeed : -train->MaxSpeed;
        }

        train->DesiredSpeed = speed;
    }

    abl.GetCodeToken();
}

namespace
{
    /// <summary>lockgateopen / lockgateclosed / releasegatelock: sets a gate's two lock flags.</summary>
    auto SetGateLocks(MCAblRuntime& abl, int32_t blownOpen, int32_t lockedClosed) -> void
    {
        abl.GetCodeToken();
        abl.GetCodeToken();
        abl.ExecExpression();
        int32_t partId = abl.Top().Integer;
        abl.Pop();
        MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

        if (object && object->ObjectClass == MCObjectClass::Gate)
        {
            static_cast<MCGate*>(object)->BlownOpen = blownOpen;
            static_cast<MCGate*>(object)->LockedClosed = lockedClosed;
        }

        abl.GetCodeToken();
    }
}

auto ExecHbLockGateOpen(MCAblRuntime& abl) -> void
{
    SetGateLocks(abl, 1, 0);
}

auto ExecHbLockGateClosed(MCAblRuntime& abl) -> void
{
    SetGateLocks(abl, 0, 1);
}

auto ExecHbReleaseGateLock(MCAblRuntime& abl) -> void
{
    SetGateLocks(abl, 0, 0);
}

auto ExecHbIsGateOpen(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (!IsGroupId(partId))
    {
        MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

        if (object && object->ObjectClass == MCObjectClass::Gate)
        {
            abl.Top().Integer = static_cast<MCGate*>(object)->IsOpen != 0 ? 1 : 0;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>How much of a pilot's worth is left by wounds (0 to 6).</summary>
    constexpr float WoundEffectiveness[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};

    /// <summary>An armor location's share left, scaled into 0.4 .. 1.0.</summary>
    auto ArmorFactor(const MCArmorLocation& location) -> double
    {
        return static_cast<double>(location.CurArmor) / static_cast<double>(location.MaxArmor) * 0.6 + 0.4;
    }

    /// <summary>getunitstatus of a mech: its armor state times its weapon effectiveness (without the pilot).</summary>
    auto MechStatus(MCMover* mech) -> float
    {
        // Armor locations: 0 head, 1 center torso, 2 / 3 arms, 4 / 5 side torsos, 8 rear center torso, 9 / 10 legs.
        MCArmorLocation* armor = mech->Armor.data();
        float centerArmor = armor[1].CurArmor;
        uint8_t centerMax = armor[1].MaxArmor;

        if (centerArmor > armor[8].CurArmor)
        {
            centerArmor = armor[8].CurArmor;
            centerMax = armor[8].MaxArmor;
        }

        double head = ArmorFactor(armor[0]);
        double sides = static_cast<double>(armor[5].CurArmor + armor[4].CurArmor) /
                           static_cast<double>(armor[5].MaxArmor + armor[4].MaxArmor) * 0.25 +
                       0.75;
        float sidesFactor = static_cast<float>(sides);
        int32_t limbMax = armor[10].MaxArmor + armor[9].MaxArmor + armor[3].MaxArmor + armor[2].MaxArmor;
        double limbs =
            static_cast<double>(armor[10].CurArmor + armor[9].CurArmor + armor[3].CurArmor + armor[2].CurArmor) /
                static_cast<double>(limbMax) * 0.25 +
            0.75;
        double center = (static_cast<double>(centerArmor) / static_cast<double>(centerMax) + 1.0) * 0.5;
        return static_cast<float>(center * limbs * sidesFactor * sidesFactor * static_cast<float>(head));
    }

    /// <summary>getunitstatus of a ground vehicle: the product of its five armor locations' factors.</summary>
    auto VehicleStatus(MCMover* vehicle) -> float
    {
        MCArmorLocation* armor = vehicle->Armor.data();
        double turret = 1.0;

        if (armor[4].MaxArmor != 0)
        {
            turret = ArmorFactor(armor[4]);
        }

        return static_cast<float>(turret * ArmorFactor(armor[0]) * ArmorFactor(armor[1]) * ArmorFactor(armor[2]) *
                                  ArmorFactor(armor[3]));
    }

    /// <summary>The share of <paramref name="damageLevel"/> the damage leaves (at least 0).</summary>
    auto HealthLeft(MCGameObject* object, uint32_t damageLevel) -> double
    {
        float maxDamage = static_cast<float>(static_cast<int32_t>(damageLevel));
        float left = maxDamage - object->GetDamage();

        if (left < 0.0f)
        {
            left = 0.0f;
        }

        return static_cast<double>(left) / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    /// <summary>The damage taken as a share of <paramref name="damageLevel"/>, both truncated, capped at 1.</summary>
    auto DamageTaken(MCGameObject* object, int32_t damageLevel) -> double
    {
        int32_t damage = X87Ftol(object->GetDamage());

        if (damage > damageLevel)
        {
            damage = damageLevel;
        }

        return static_cast<double>(damage) / static_cast<double>(damageLevel);
    }
}

auto ExecHbGetUnitStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* baseObject = ObjectList()->FindObjectFromPart(abl.Top().Integer);
    abl.Top().Integer = 0;

    if (baseObject)
    {
        MCGameObject* object = static_cast<MCGameObject*>(baseObject);
        // Port fix: other classes scale an uninitialized local in the original.
        double status = 0.0;

        switch (object->ObjectClass)
        {
            case MCObjectClass::BattleMech:
            case MCObjectClass::GroundVehicle:
            {
                MCMover* mover = static_cast<MCMover*>(object);
                float weaponShare;
                float armorStatus;

                if (object->ObjectClass == MCObjectClass::BattleMech)
                {
                    weaponShare = mover->WeaponEffectiveness / mover->MaxWeaponEffectiveness;
                    armorStatus = MechStatus(mover);
                }
                else
                {
                    weaponShare = mover->MaxWeaponEffectiveness == 0.0f
                                      ? 1.0f
                                      : mover->WeaponEffectiveness / mover->MaxWeaponEffectiveness;
                    armorStatus = VehicleStatus(mover);
                }

                // Port fix: wounds past 6 index past the table in the original.
                int32_t wounds = std::clamp(X87Ftol(object->GetPilot()->Wounds), 0, 6);
                float pilotShare = WoundEffectiveness[wounds];

                if (object->IsDestroyed() || object->IsDisabled())
                {
                    status = 0.0f * armorStatus * weaponShare;
                }
                else
                {
                    status = pilotShare * armorStatus * weaponShare;
                }
                break;
            }

            case MCObjectClass::Building:
                status = HealthLeft(object, static_cast<MCBuildingType*>(object->GetObjectType())->DmgLevel);
                break;
            case MCObjectClass::TreeBuilding:
                status = HealthLeft(object, static_cast<MCTreeBuildingType*>(object->GetObjectType())->DmgLevel);
                break;
            case MCObjectClass::MiscTerrainObject:
            {
                uint32_t damageLevel = 0;
                // Original behaviour: a kind getDamageLevel doesn't list divides 0 by 0.
                GetDamageLevel(object, damageLevel);
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(damageLevel));
                break;
            }

            case MCObjectClass::TrainCar:
                // Original behaviour (OB-047): a train car reports the damage taken, not the health left.
                status = DamageTaken(object, static_cast<MCTrainCarType*>(object->GetObjectType())->Damage);
                break;
            case MCObjectClass::Turret:
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(
                                                       static_cast<MCTurretType*>(object->GetObjectType())->DmgLevel));
                break;
            case MCObjectClass::Gate:
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(
                                                       static_cast<MCGateType*>(object->GetObjectType())->DmgLevel));
                break;
            default:
                break;
        }

        abl.Top().Real = static_cast<float>(status * 100.0);
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbRelPosPoint(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    float* point = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    float angle = abl.NextReal();
    float distance = abl.NextReal();
    uint32_t flags = static_cast<uint32_t>(abl.NextInteger());
    abl.GetCodeToken();
    float* result = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    MCVector3D start;
    start.X = point[0];
    start.Y = point[1];
    start.Z = 0.0f;
    MCVector3D position = RelativePositionToPoint(start, angle, distance, flags);
    result[0] = position.X;
    result[1] = position.Y;
    result[2] = position.Z;
    abl.GetCodeToken();
}

auto ExecHbRelPosObject(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float angle = abl.NextReal();
    float distance = abl.NextReal();
    uint32_t flags = static_cast<uint32_t>(abl.NextInteger());
    abl.GetCodeToken();
    float* result = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    MCGameObject* object = FindObject(abl, partId);

    if (object)
    {
        MCVector3D position = object->RelativePosition(angle, distance, flags);
        result[0] = position.X;
        result[1] = position.Y;
        result[2] = position.Z;
    }

    abl.GetCodeToken();
}

namespace
{
    /// <summary>The damage state of the body location an armor location covers (the rear locations, from
    /// numBodyLocations on, cover the torsos from 1 on).</summary>
    auto ArmorLocationState(MCMover* mover, int32_t armorIndex) -> uint8_t
    {
        if (armorIndex < mover->NumBodyLocations())
        {
            return mover->BodyAt(armorIndex).DamageState;
        }

        return mover->BodyAt(armorIndex - mover->NumBodyLocations() + 1).DamageState;
    }
}

auto ExecHbRepair(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    float points = abl.NextReal();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && object->ObjectClass == MCObjectClass::BattleMech)
    {
        // Fills internal structure first, then armor, location by location, skipping destroyed ones.
        MCMover* mech = static_cast<MCMover*>(object);

        for (int32_t i = 0; i < mech->NumBodyLocations(); i++)
        {
            MCBodyLocation& location = mech->BodyAt(i);
            float needed = static_cast<float>(location.MaxInternalStructure) - location.CurInternalStructure;

            if (location.DamageState != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.CurInternalStructure += points;
                    points = 0.0f;
                    break;
                }

                points -= needed;
                location.CurInternalStructure = static_cast<float>(location.MaxInternalStructure);
            }
        }

        for (int32_t i = 0; i < mech->NumArmorLocations(); i++)
        {
            MCArmorLocation& location = mech->Armor[i];
            float needed = static_cast<float>(location.MaxArmor) - location.CurArmor;

            if (ArmorLocationState(mech, i) != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.CurArmor += points;
                    break;
                }

                points -= needed;
                location.CurArmor = static_cast<float>(location.MaxArmor);
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbGetRepairState(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    // The percentage of internal structure and armor left, over the locations that aren't destroyed.
    double sum = 0.0;
    int32_t maximum = 0;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);

        for (int32_t i = 0; i < mover->NumBodyLocations(); i++)
        {
            if (mover->BodyAt(i).DamageState != 2)
            {
                sum += mover->BodyAt(i).CurInternalStructure;
                maximum += mover->BodyAt(i).MaxInternalStructure;
            }
        }

        for (int32_t i = 0; i < mover->NumArmorLocations(); i++)
        {
            if (ArmorLocationState(mover, i) != 2)
            {
                sum += mover->Armor[i].CurArmor;
                maximum += mover->Armor[i].MaxArmor;
            }
        }
    }

    if (maximum != 0)
    {
        abl.PushInteger(X87Ftol(sum * 100.0 / static_cast<double>(maximum)));
    }
    else
    {
        // Original behaviour (OB-111): with no object, or no location left, MCX.EXE divides by zero and __ftol turns
        // the NaN or infinity into 0x80000000.
        abl.PushInteger(INT32_MIN);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsTeamTargeting(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t teamId = abl.Top().Integer;
    abl.Pop();
    uint32_t targetId = static_cast<uint32_t>(abl.NextInteger());
    uint32_t exceptId = static_cast<uint32_t>(abl.NextInteger());
    MCTeam* team = nullptr;

    if (teamId == 500)
    {
        team = InnerSphereTeam();
    }
    else if (teamId == 0x1f6)
    {
        team = AlliedTeam();
    }
    else if (teamId == 0x1f5)
    {
        team = ClanTeam();
    }

    int targeting = 0;

    if (team)
    {
        targeting = team->IsTargeting(targetId, exceptId);
    }

    abl.PushInteger(targeting != 0 ? 1 : 0);
    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbGetFixed(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t moverId = abl.Top().Integer;
    abl.Pop();
    int32_t bayId = abl.NextInteger();
    uint32_t params = static_cast<uint32_t>(abl.NextInteger());

    // -1 ordered, 0 order refused, 1 bay out of points, 2 wrong kind of bay, 3 already this bay's, 4 bay busy,
    // 5 already being fixed, 6 needs nothing, 7 not a mech or vehicle, 8 not a repair bay, 9 other side.
    int32_t result = -1;
    MCBaseObject* bayObject = ObjectList()->FindObjectFromPart(bayId);

    if (!bayObject || bayObject->ObjectClass != MCObjectClass::TreeBuilding ||
        static_cast<MCTreeBuilding*>(bayObject)->CanRefit == 0)
    {
        result = 8;
    }
    else
    {
        MCTreeBuilding* bay = static_cast<MCTreeBuilding*>(bayObject);

        if (bay->GetRefitPoints() > 0.0f)
        {
            MCBaseObject* moverObject = ObjectList()->FindObjectFromPart(moverId);

            if (!moverObject || (moverObject->ObjectClass != MCObjectClass::BattleMech &&
                                 moverObject->ObjectClass != MCObjectClass::GroundVehicle))
            {
                result = 7;
            }
            else
            {
                MCMover* mover = static_cast<MCMover*>(moverObject);

                if (bay->GetAlignment() != mover->GetAlignment())
                {
                    result = 9;
                }
                else if (bay->RefitBuddy)
                {
                    result = (bay->RefitBuddy != mover) ? 4 : 3;
                }
                else
                {
                    // A mech bay fixes mechs only, a vehicle bay vehicles only.
                    bool rightBay = (mover->ObjectClass == MCObjectClass::BattleMech) == (bay->MechBay != 0);

                    if (!rightBay)
                    {
                        result = 2;
                    }
                    else if (mover->RefitBuddy)
                    {
                        result = 5;
                    }
                    else if (mover->NeedsRefit(0) == 0)
                    {
                        result = 6;
                    }
                    else
                    {
                        result = mover->GetPilot()->OrderGetFixed(MCOrderOrigin::Commander, bay, params) != 0 ? -1 : 0;
                    }
                }
            }
        }
        else
        {
            result = 1;
        }
    }

    abl.PushInteger(result);
    abl.GetCodeToken();
    return IntegerTypePtr;
}
