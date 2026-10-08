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
#include "object/gvehicl.h"
#include "object/mover.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/tbldng.h"
#include "object/terrobj.h"
#include "object/train.h"
#include "object/turret.h"
#include "object/warrior.h"
#include "sound/radio.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "object/MCWeaponShotInfo.h"

// Object queries: distances, existence, status, sides, areas, activity, type ids, armor, pilots and damage.

namespace
{
    /// <summary>The distance in world units from <paramref name="object"/> to (x, y), ignoring height.</summary>
    auto FlatDistance(MCGameObject* object, float x, float y) -> double
    {
        MCVector3D objectPosition = object->GetPosition();
        const float dx = x - objectPosition.X;
        const float dy = y - objectPosition.Y;
        return std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy + 0.0);
    }

    /// <summary>
    /// distancetoobject / distancetoposition: meters from (x, y) to an object, or to the nearest existing, awake
    /// mover of a group; <paramref name="result"/> keeps its value when there is none.
    /// </summary>
    auto DistanceFromId(MCAblRuntime& abl, int32_t partId, float x, float y, float& result) -> void
    {
        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(abl, partId);

            if (object)
            {
                result = static_cast<float>(FlatDistance(object, x, y) * MetersPerWorldUnit);
            }

            return;
        }

        const std::vector<MCMover*> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());
        float closest = 3.4e38f;

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (movers[i]->GetExistsAndAwake())
            {
                const auto distance = static_cast<float>(FlatDistance(movers[i], x, y));

                if (distance < closest)
                {
                    closest = distance;
                }
            }
        }

        if (static_cast<double>(closest) < 3.4e38)
        {
            result = MetersPerWorldUnit * closest;
        }
    }

    /// <summary>Takes <paramref name="object"/> out of the object list (it gets destroyed with it).</summary>
    auto RemoveFromObjectList(MCBaseObject* object) -> void
    {
        ObjectList()->Remove(object);
    }

}

auto ExecHbDistanceToObject(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t targetId = abl.Top().Integer;
    abl.Top().Real = -1.0f;
    MCGameObject* target = FindObject(abl, targetId);

    if (target)
    {
        MCVector3D targetPosition = target->GetPosition();
        DistanceFromId(abl, partId, targetPosition.X, targetPosition.Y, abl.Top().Real);
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbDistanceToPosition(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    float* position = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    abl.PushReal(-1.0f);
    DistanceFromId(abl, partId, position[0], position[1], abl.Top().Real);
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbObjectSuicide(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();

    if (IsGroupId(partId))
    {
        const std::vector<MCMover*> movers = GetGroupMovers(partId);
        const auto numMovers = static_cast<int32_t>(movers.size());

        for (int32_t i = 0; i < numMovers; i++)
        {
            RemoveFromObjectList(movers[i]);
        }
    }
    else
    {
        MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

        if (object)
        {
            RemoveFromObjectList(object);
        }
    }

    abl.GetCodeToken();
}

auto ExecHbObjectCreate(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    for (int32_t i = 0; i < CurrentCreatorPart; i++)
    {
        if (CreatedPartRoster[i].PartId == partId)
        {
            if (CreatedPartRoster[i].Created == 0)
            {
                Scenario->CreateScenarioObject(partId);
                InnerSphereTeam()->ScanBattlefield();

                if (AlliedTeam())
                {
                    AlliedTeam()->ScanBattlefield();
                }

                abl.Top().Integer = partId;
            }
            break;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectExists(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (IsGroupId(partId))
    {
        if (!GetGroupMovers(partId).empty())
        {
            abl.Top().Integer = 1;
        }
    }
    else if (FindObject(abl, partId))
    {
        abl.Top().Integer = 1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectStatus(MCAblRuntime& abl) -> MCAblType*
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
            abl.Top().Integer = static_cast<uint8_t>(object->Status);
        }

        abl.GetCodeToken();
        return IntegerTypePtr;
    }

    // A group is 1 (gone) unless one of its movers is neither disabled nor destroyed and has a pilot who hasn't
    // withdrawn.
    const std::vector<MCMover*> movers = GetGroupMovers(partId);
    const auto numMovers = static_cast<int32_t>(movers.size());

    for (int32_t i = 0; i < numMovers; i++)
    {
        uint8_t status = static_cast<uint8_t>(movers[i]->Status);

        if (status != 2 && status != 1)
        {
            MCMechWarrior* pilot = movers[i]->GetPilot();

            if (pilot && pilot->Status != 2)
            {
                abl.Top().Integer = 0;
                abl.GetCodeToken();
                return IntegerTypePtr;
            }
        }
    }

    abl.Top().Integer = 1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectStatusCount(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    int32_t* counts = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object)
        {
            counts[static_cast<uint8_t>(object->Status)]++;
        }
    }
    else if (partId < 0x21)
    {
        CommanderById(0)->GetGroup(partId - 1)->StatusCount(counts);
    }
    else if (partId >= 0x149 && partId < 0x169)
    {
        CommanderById(2)->GetGroup(partId - 0x149)->StatusCount(counts);
    }
    else if (partId >= 0xa5 && partId < 0xc5)
    {
        CommanderById(1)->GetGroup(partId - 0xa5)->StatusCount(counts);
    }
    else if (partId == 500)
    {
        InnerSphereTeam()->StatusCount(counts);
    }
    else if (partId == 0x1f6)
    {
        if (AlliedTeam())
        {
            AlliedTeam()->StatusCount(counts);
        }
    }
    else if (partId == 0x1f5)
    {
        ClanTeam()->StatusCount(counts);
    }

    abl.GetCodeToken();
    return nullptr;
}

auto ExecHbObjectVisible(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t lookerId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCGameObject* target = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(abl.Top().Integer));
    abl.Top().Integer = 0;

    if (target)
    {
        if (!IsGroupId(lookerId))
        {
            MCGameObject* looker = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(lookerId));

            if (looker)
            {
                abl.Top().Integer = looker->LineOfSight(target);
            }
        }
        else
        {
            for (MCBaseObject* looker = ObjectList()->FindObjectInGroup(nullptr, lookerId); looker;
                 looker = ObjectList()->FindObjectInGroup(looker, lookerId))
            {
                if (static_cast<MCGameObject*>(looker)->LineOfSight(target))
                {
                    abl.Top().Integer = 1;
                    break;
                }
            }
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectSide(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && object->GetObjectType())
    {
        abl.Top().Integer = static_cast<MCGameObject*>(object)->GetAlignment();
    }
    else
    {
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectCommander(MCAblRuntime& abl) -> MCAblType*
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
            abl.Top().Integer = object->GetCommanderId();
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectClass(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);
    abl.Top().Integer = object ? static_cast<int32_t>(object->ObjectClass) : -1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbInArea(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    float* position = reinterpret_cast<float*>(abl.NextReference());
    abl.Pop();
    float radius = abl.NextReal();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t numRequired = abl.Top().Integer;
    MCVector3D center;
    center.X = position[0];
    center.Y = position[1];
    center.Z = position[2];

    if (!IsGroupId(partId))
    {
        // Original behaviour: with a count of 0 a single object is always in the area.
        abl.Top().Integer = 1;

        if (numRequired != 0)
        {
            abl.Top().Integer = 0;
            MCGameObject* object = FindObject(abl, partId);

            if (object && object->GetExists() && object->GetAwake() && !object->IsDisabled() &&
                !object->IsDestroyed() && object->DistanceFrom(center) <= radius)
            {
                abl.Top().Integer = 1;
            }
        }

        abl.GetCodeToken();
        return BooleanTypePtr;
    }

    const std::vector<MCMover*> movers = GetGroupMovers(partId);
    const auto numMovers = static_cast<int32_t>(movers.size());

    if (numRequired == -1)
    {
        // All of the group's working movers: false if one that exists and is awake is outside, or there are none.
        abl.Top().Integer = 1;
        int32_t numWorking = 0;

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMover* mover = movers[i];

            if (!mover->IsDisabled())
            {
                numWorking++;

                if (mover->GetExists() && mover->GetAwake() && mover->DistanceFrom(center) > radius)
                {
                    abl.Top().Integer = 0;
                    break;
                }
            }
        }

        if (numWorking == 0)
        {
            abl.Top().Integer = 0;
        }

        abl.GetCodeToken();
        return BooleanTypePtr;
    }

    // At least numRequired movers that exist, are awake and work.
    abl.Top().Integer = 0;
    int32_t numInside = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = movers[i];

        if (mover->GetExists() && mover->GetAwake() && !mover->IsDisabled() && !mover->IsDestroyed() &&
            mover->DistanceFrom(center) <= radius)
        {
            if (++numInside == numRequired)
            {
                abl.Top().Integer = 1;
                break;
            }
        }
    }

    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetObjActive(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int active = abl.Top().Integer == 1 ? 1 : 0;
    int32_t numChanged = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object && object->GetAwake() != active)
        {
            object->SetAwake(active);
            TheInterface->ActivateMech(object->PartId);
            numChanged = 1;
        }
    }
    else
    {
        // Original behaviour: the walk stops at the first member already in the wanted state.
        MCBaseObject* object = ObjectList()->FindObjectInGroup(nullptr, partId);

        while (object && static_cast<MCGameObject*>(object)->GetAwake() != active)
        {
            object->SetAwake(active);
            TheInterface->ActivateMech(object->PartId);
            numChanged++;
            object = ObjectList()->FindObjectInGroup(object, partId);
        }
    }

    abl.Top().Integer = numChanged;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjWithdraw(MCAblRuntime& abl) -> MCAblType*
{
    // Original behaviour (OB-043): two items are pushed for the one result, so every call leaves one behind.
    abl.PushInteger(0);
    abl.PushInteger(0);
    MCVector3D nowhere;
    nowhere.X = 0.0f;
    nowhere.Y = 0.0f;
    nowhere.Z = 0.0f;

    if (abl.Brain.IsUnitOrder == 0)
    {
        if (abl.Brain.Warrior)
        {
            abl.Brain.Warrior->OrderWithdraw(0, MCOrderOrigin::Commander, nowhere);
        }
        else
        {
            abl.Top().Integer = -2;
        }
    }
    else if (abl.Brain.Group)
    {
        abl.Brain.Group->OrderWithdraw(MCOrderOrigin::Commander, nowhere);
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjInWithdraw(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 1;

    if (!IsGroupId(partId))
    {
        MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

        if (object && object->GetObjectType() && !static_cast<MCGameObject*>(object)->IsWithdrawing())
        {
            abl.Top().Integer = 0;
        }
    }
    else
    {
        for (MCBaseObject* object = ObjectList()->FindObjectInGroup(nullptr, partId); object && abl.Top().Integer == 1;
             object = ObjectList()->FindObjectInGroup(object, partId))
        {
            if (!static_cast<MCGameObject*>(object)->IsWithdrawing())
            {
                abl.Top().Integer = 0;
            }
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjTypeId(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = -1;
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && object->GetObjectType())
    {
        abl.Top().Integer = object->GetObjectType()->ObjTypeNum;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbTerrainObjectId(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t blockNumber = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = (blockNumber * 400 + abl.Top().Integer) * 8 + 0x1000;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbVehicleId(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = -1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponAmmo(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    // The weapon index goes through a float on its way to the call.
    float weaponIndex = static_cast<float>(abl.Top().Integer);
    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        abl.Top().Integer = static_cast<MCMover*>(object)->GetWeaponShots(static_cast<int32_t>(weaponIndex));
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetSensors(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && IsMover(object) && static_cast<MCMover*>(object)->SensorSystem)
    {
        abl.Top().Integer = static_cast<MCMover*>(object)->SensorSystem->Enabled();
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetBRValue(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCGameObject* object = FindObject(abl, abl.Top().Integer);
    abl.Top().Integer = object ? object->GetCurCV() : -1;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetBRValue(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t newCV = abl.NextInteger();
    MCGameObject* object = FindObject(abl, partId);

    if (object)
    {
        object->SetCurCV(newCV);
    }

    // Original behaviour: nothing is left on the stack for the integer result.
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetArmorPts(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->NumArmorLocations; i++)
        {
            total = static_cast<int32_t>(static_cast<float>(total) + mover->Armor[i].CurArmor);
        }

        abl.Top().Integer = total;
    }
    else
    {
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMaxArmor(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->NumArmorLocations; i++)
        {
            total += mover->Armor[i].MaxArmor;
        }

        abl.Top().Integer = total;
    }
    else
    {
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetPilotId(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && IsMover(object))
    {
        abl.Top().Integer = static_cast<MCGameObject*>(object)->GetPilot()->Index;
    }
    else
    {
        abl.Top().Integer = -1;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetPilotWounds(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (object && IsMover(object))
    {
        abl.Top().Real = static_cast<MCGameObject*>(object)->GetPilot()->Wounds;
    }
    else
    {
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetPilotWounds(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t wounds = abl.NextInteger();

    if (wounds > 6)
    {
        wounds = 6;
    }

    MCBaseObject* object = ObjectList()->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        static_cast<MCGameObject*>(object)->GetPilot()->Wounds = static_cast<float>(wounds);
    }

    // Original behaviour: nothing is left on the stack for the real result.
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetObjActive(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (IsGroupId(partId))
    {
        int32_t numAwake = 0;

        for (MCBaseObject* object = ObjectList()->FindObjectInGroup(nullptr, partId); object && abl.Top().Integer == 0;
             object = ObjectList()->FindObjectInGroup(object, partId))
        {
            if (static_cast<MCGameObject*>(object)->GetAwake())
            {
                numAwake++;
            }
        }

        abl.Top().Integer = numAwake;
    }
    else
    {
        MCGameObject* object = FindObject(abl, partId);

        if (object && object->GetAwake())
        {
            abl.Top().Integer = 1;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>Whether getobjectdamage and its kin handle <paramref name="object"/>: a typed building, terrain
    /// object or misc terrain object.</summary>
    auto IsDamageableScenery(MCBaseObject* object) -> bool
    {
        if (!object || !object->GetObjectType())
        {
            return false;
        }

        return static_cast<MCGameObject*>(object)->IsBuilding() ||
               object->ObjectClass == MCObjectClass::TerrainObject ||
               object->ObjectClass == MCObjectClass::MiscTerrainObject;
    }

    /// <summary>Applies <paramref name="shotInfo"/> to <paramref name="target"/> as the game's weapon hits do: in
    /// multiplayer only on the server, which sends it on.</summary>
    /// <returns>False on a multiplayer client (nothing applied).</returns>
    auto ApplyShot(MCGameObject* target, MCWeaponShotInfo* shotInfo) -> bool
    {
        if (MPlayer == nullptr)
        {
            target->HandleWeaponHit(shotInfo, 0);
            return true;
        }

        if (MPlayer->IsServer == 0)
        {
            return false;
        }

        target->HandleWeaponHit(shotInfo, 1);
        return true;
    }
}

auto ExecHbGetObjDamage(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* baseObject = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (!IsDamageableScenery(baseObject))
    {
        abl.Top().Integer = 0;
        abl.GetCodeToken();
        return IntegerTypePtr;
    }

    MCGameObject* object = static_cast<MCGameObject*>(baseObject);
    double damage = object->GetDamage();
    uint32_t damageLevel;

    // Original behaviour: a misc terrain object getDamageLevel doesn't list gives its raw damage times 100.
    if (GetDamageLevel(object, damageLevel))
    {
        damage = damage / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    abl.Top().Integer = static_cast<int32_t>(std::floor(damage * 100.0));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetObjDmgPts(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);

    if (IsDamageableScenery(object))
    {
        abl.Top().Integer = static_cast<int32_t>(static_cast<MCGameObject*>(object)->GetDamage());
    }
    else
    {
        abl.Top().Integer = 0;
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMaxDmg(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCBaseObject* object = ObjectList()->FindObjectFromPart(abl.Top().Integer);
    uint32_t damageLevel = 0;

    if (IsDamageableScenery(object))
    {
        GetDamageLevel(static_cast<MCGameObject*>(object), damageLevel);
    }

    abl.Top().Integer = static_cast<int32_t>(damageLevel);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetObjDamage(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t percent = abl.NextInteger();

    if (percent > 100)
    {
        percent = 100;
    }

    MCBaseObject* baseObject = ObjectList()->FindObjectFromPart(partId);

    if (baseObject && baseObject->GetObjectType() && percent > 0)
    {
        // Raises the damage to percent of the damage level (it never lowers it).
        MCGameObject* object = static_cast<MCGameObject*>(baseObject);
        uint32_t damageLevel;

        if (GetDamageLevel(object, damageLevel))
        {
            float currentDamage = object->GetDamage();
            float extraDamage = static_cast<float>(static_cast<double>(percent) * 0.01 *
                                                       static_cast<float>(static_cast<int32_t>(damageLevel)) -
                                                   currentDamage);

            if (extraDamage > 0.0f)
            {
                MCWeaponShotInfo shotInfo;
                shotInfo.Init(nullptr, -1, extraDamage, 0, 0.0f);
                ApplyShot(object, &shotInfo);
            }
        }
    }

    abl.GetCodeToken();
}

auto ExecHbDamageObject(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t partId = abl.Top().Integer;
    abl.Pop();
    int32_t attackerId = abl.NextInteger();
    int32_t weaponMasterId = abl.NextInteger();
    float damage = abl.NextReal();
    int32_t hitLocation = abl.NextInteger();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    float entryAngle = abl.Top().Real;

    MCGameObject* attacker = FindObject(abl, attackerId);

    if (!attacker)
    {
        abl.Top().Integer = -1;
        abl.GetCodeToken();
        return IntegerTypePtr;
    }

    MCWeaponShotInfo shotInfo;

    if (!IsGroupId(partId))
    {
        MCGameObject* target = FindObject(abl, partId);

        if (!target)
        {
            abl.Top().Integer = -2;
            abl.GetCodeToken();
            return IntegerTypePtr;
        }

        shotInfo.Init(attacker, weaponMasterId, damage, hitLocation, entryAngle);
        ApplyShot(target, &shotInfo);
        abl.Top().Integer = 1;
        abl.GetCodeToken();
        return IntegerTypePtr;
    }

    const std::vector<MCMover*> movers = GetGroupMovers(partId);
    const auto numMovers = static_cast<int32_t>(movers.size());
    shotInfo.Init(attacker, weaponMasterId, damage, hitLocation, entryAngle);

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (!ApplyShot(movers[i], &shotInfo))
        {
            break;
        }
    }

    abl.Top().Integer = numMovers;
    abl.GetCodeToken();
    return IntegerTypePtr;
}
