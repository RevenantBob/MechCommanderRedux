#include "stdafx.h"
#include "object/MCTeam.h"
#include "ai/MCMoveSystem.h"
#include "engine/MCByteFlag.h"
#include "lib/MCFatal.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "object/MCContactSystem.h"
#include "object/MCForces.h"
#include "object/MCMasterComponent.h"
#include "object/MCSortList.h"
#include "object/MCWeaponShotInfo.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"

int32_t InContact = 0;

namespace
{
    /// <summary>Entries the original's contact sort list held: fewer contacts are sorted among filler entries.</summary>
    constexpr int32_t ContactSortListItems = 200;

    /// <summary>
    /// Inserts <paramref name="tracker"/> into <paramref name="list"/> before the first entry no stronger than it;
    /// returns the entry.
    /// </summary>
    MCSystemTracker* InsertTracker(std::list<MCSystemTracker>& list, const MCSystemTracker& tracker)
    {
        const auto position = std::ranges::find_if(list, [&tracker](const MCSystemTracker& entry)
                                                   { return entry.Effect <= tracker.Effect; });
        return &*list.insert(position, tracker);
    }

    /// <summary>Takes <paramref name="tracker"/> out of <paramref name="list"/> (nothing for null).</summary>
    void RemoveTracker(std::list<MCSystemTracker>& list, const MCSystemTracker* tracker)
    {
        list.remove_if([tracker](const MCSystemTracker& entry) { return &entry == tracker; });
    }

    /// <summary>Drops slot <paramref name="index"/> of a team contact list; the last entry fills it.</summary>
    void RemoveTeamContact(std::vector<uint16_t>& list, int32_t index, int32_t teamId)
    {
        MCPotentialContactManager* manager = PotentialContactManager();
        manager->Contact(list[index]).TeamSlot[teamId] = -1;
        const auto last = static_cast<int32_t>(list.size()) - 1;

        if (last > 0 && index != last)
        {
            list[index] = list[last];
            manager->Contact(list[index]).TeamSlot[teamId] = index;
        }

        list.pop_back();
    }
}

MCTeam::MCTeam(int32_t id, int32_t alignment) : Id(id), Alignment(alignment)
{
}

auto MCTeam::BuildRoster(MCScenario* scenario) -> void
{
    Roster.clear();

    for (uint32_t i = 1; static_cast<int32_t>(i) < static_cast<int32_t>(scenario->NumParts() + 1); i++)
    {
        const MCPart& part = scenario->Parts[i];

        if (IsMoverClass(part.Object->ObjectClass) && part.TeamId == Id)
        {
            Roster.push_back(part.Object->PartId);
        }
    }

    SensorsPerUpdate = RosterSize() < 3 ? RosterSize() : 3;
}

auto MCTeam::AddSensor(MCSensorSystem* sensor) -> void
{
    sensor->TeamSensorSlot = static_cast<int32_t>(_Sensors.size());
    _Sensors.push_back(sensor);
}

auto MCTeam::RemoveSensor(MCSensorSystem* sensor) -> void
{
    const int32_t slot = sensor->TeamSensorSlot;
    sensor->TeamSensorSlot = -1;
    const auto last = static_cast<int32_t>(_Sensors.size()) - 1;

    if (slot < last)
    {
        // The last sensor fills the gap.
        _Sensors[slot] = _Sensors[last];
        _Sensors[slot]->TeamSensorSlot = slot;
    }

    _Sensors.pop_back();
}

auto MCTeam::UpdateSensors() -> void
{
    if (_Sensors.empty())
    {
        return;
    }

    for (MCSensorSystem* sensor : _Sensors)
    {
        sensor->UpdateScan(0);
    }

    // A few sensors a frame re-check the contacts they hold, in turn.
    for (int32_t i = 0; i < SensorsPerUpdate; i++)
    {
        if (NextSensorUpdate >= static_cast<int32_t>(_Sensors.size()))
        {
            NextSensorUpdate = 0;
        }

        _Sensors[NextSensorUpdate]->UpdateContacts();
        NextSensorUpdate++;
    }
}

auto MCTeam::GetLosContacts(MCGameObject** objects) -> int32_t
{
    for (size_t i = 0; i < _LosContacts.size(); i++)
    {
        objects[i] = PotentialContactManager()->Contact(_LosContacts[i]).Object;
    }

    return static_cast<int32_t>(_LosContacts.size());
}

auto MCTeam::GetSensorContacts(MCGameObject** objects) -> int32_t
{
    for (size_t i = 0; i < _SensorContacts.size(); i++)
    {
        objects[i] = PotentialContactManager()->Contact(_SensorContacts[i]).Object;
    }

    return static_cast<int32_t>(_SensorContacts.size());
}

auto MCTeam::GetContacts(MCGameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType) -> int32_t
{
    const int32_t enemyAlignment = Alignment == -1 ? 1 : -1;
    const std::vector<uint16_t>& list = (contactCriteria & 0x10) != 0 ? _SensorContacts : _LosContacts;
    std::vector<float> sortValues;
    int32_t numFound = 0;

    for (const uint16_t contactId : list)
    {
        MCBigGameObject* object = PotentialContactManager()->Contact(contactId).Object;

        if ((contactCriteria & 8) != 0 && IsMoverClass(object->ObjectClass) &&
            static_cast<MCMover*>(object)->GetChallenger() != nullptr)
        {
            continue;
        }

        if ((contactCriteria & 1) != 0 && object->GetAlignment() != enemyAlignment)
        {
            continue;
        }

        contacts[numFound] = object->PartId;
        float value = 0.0f;

        if (sortType == 1)
        {
            value = IsMoverClass(object->ObjectClass) ? static_cast<float>(object->GetCurCV()) : 0.0f;
        }
        else if (sortType == 2)
        {
            MCVector3D position = object->GetPosition();
            value = static_cast<float>(looker->DistanceFrom(position));
        }

        sortValues.push_back(value);
        numFound++;
    }

    if (numFound <= 0 || sortType == 0)
    {
        return numFound;
    }

    // By value, highest first; by distance, nearest first, among at least 200 entries as the original's list: the
    // filler sorts last, and decides the order of equal values. Port fix (OB-143): the original's list held only 200,
    // so contacts past the 200th were never sorted; the list now holds every contact found.
    MCSortList sortList(std::max(numFound, ContactSortListItems));
    sortList.Clear(sortType != 2);

    for (int32_t i = 0; i < numFound; i++)
    {
        sortList.List[i].Id = contacts[i];
        sortList.List[i].Value = sortValues[i];
    }

    sortList.Sort(sortType != 2);

    for (int32_t i = 0; i < numFound; i++)
    {
        contacts[i] = sortList.List[i].Id;
    }

    return numFound;
}

auto MCTeam::GetContactType(MCGameObject* object) const -> int32_t
{
    return object->GetContactType(Id);
}

auto MCTeam::IsContact(MCGameObject* object, int32_t contactCriteria) const -> int
{
    const auto contactType = static_cast<MCContactStatus>(object->GetContactType(Id));

    if (contactType == MCContactStatus::None)
    {
        return 0;
    }

    if ((contactCriteria & 1) != 0 && object->GetAlignment() == Alignment)
    {
        return 0;
    }

    if ((contactCriteria & 2) != 0 && contactType != MCContactStatus::Visual)
    {
        return 0;
    }

    if ((contactCriteria & 8) != 0 && IsMoverClass(object->ObjectClass) &&
        static_cast<MCMover*>(object)->GetChallenger() != nullptr)
    {
        return 0;
    }

    return 1;
}

auto MCTeam::ScanBattlefield() -> void
{
    for (MCSensorSystem* sensor : _Sensors)
    {
        sensor->UpdateScan(1);
    }
}

auto MCTeam::AddLosContact(MCPotentialContact* contact) -> void
{
    // Port fix (OB-146): the original guarded its 500 entries with the sensor list's count.
    contact->TeamSlot[Id] = static_cast<int32_t>(_LosContacts.size());
    _LosContacts.push_back(contact->Id);
}

auto MCTeam::RemoveLosContact(int32_t index) -> void
{
    RemoveTeamContact(_LosContacts, index, Id);
}

auto MCTeam::RemoveLosContact(MCPotentialContact* contact) -> void
{
    if (contact->TeamSlot[Id] != -1)
    {
        RemoveLosContact(contact->TeamSlot[Id]);
    }
}

auto MCTeam::AddSensorContact(MCPotentialContact* contact) -> void
{
    contact->TeamSlot[Id] = static_cast<int32_t>(_SensorContacts.size());
    _SensorContacts.push_back(contact->Id);
}

auto MCTeam::RemoveSensorContact(int32_t index) -> void
{
    RemoveTeamContact(_SensorContacts, index, Id);
}

auto MCTeam::RemoveSensorContact(MCPotentialContact* contact) -> void
{
    if (contact->TeamSlot[Id] != -1)
    {
        RemoveSensorContact(contact->TeamSlot[Id]);
    }
}

auto MCTeam::GetRoster(MCGameObject** objects) -> int32_t
{
    int32_t count = 0;

    for (const int32_t partId : Roster)
    {
        MCMover* mover = GetMoverFromPartId(partId);

        if (mover != nullptr)
        {
            objects[count++] = mover;
        }
    }

    return count;
}

auto MCTeam::DisableTargets() -> void
{
    for (const int32_t partId : Roster)
    {
        MCMover* mover = GetMoverFromPartId(partId);

        if (mover == nullptr || !IsMoverClass(mover->ObjectClass))
        {
            continue;
        }

        MCGameObject* target = mover->GetPilot()->GetLastTarget();

        if (target != nullptr && IsMoverClass(target->ObjectClass))
        {
            static_cast<MCMover*>(target)->Disable(0x42);
        }
    }
}

auto MCTeam::DestroyTargets() -> void
{
    for (const int32_t partId : Roster)
    {
        MCMover* mover = GetMoverFromPartId(partId);

        if (mover == nullptr || !IsMoverClass(mover->ObjectClass))
        {
            continue;
        }

        MCGameObject* target = mover->GetPilot()->GetLastTarget();

        if (target == nullptr || !IsMoverClass(target->ObjectClass))
        {
            continue;
        }

        MCWeaponShotInfo shotInfo;
        shotInfo.Init(nullptr, -3, 5.0f, 0, 0.0f);

        for (int32_t shot = 0; shot < 100; shot++)
        {
            if (!RollDice(30))
            {
                shotInfo.HitLocation = target->CalcHitLocation(nullptr, -1, 4, 0);
            }
            else
            {
                shotInfo.HitLocation = target->CalcHitLocation(nullptr, -1, 2, 0);
            }

            target->HandleWeaponHit(&shotInfo, MultiPlayer() != nullptr ? 1 : 0);
        }
    }
}

auto MCTeam::IsTargeting(uint32_t targetPartId, uint32_t exceptPartId) -> int
{
    for (const int32_t partId : Roster)
    {
        if (exceptPartId != 0 && static_cast<uint32_t>(partId) == exceptPartId)
        {
            continue;
        }

        MCMover* mover = GetMoverFromPartId(partId);

        if (mover == nullptr)
        {
            continue;
        }

        MCMechWarrior* pilot = mover->GetPilot();

        if (pilot == nullptr)
        {
            continue;
        }

        MCGameObject* target = pilot->GetLastTarget();

        if (target != nullptr && static_cast<uint32_t>(target->PartId) == targetPartId)
        {
            return 1;
        }
    }

    return 0;
}

auto MCTeam::AddJammer(MCGameObject* owner, int32_t masterId) -> MCSystemTracker*
{
    return InsertTracker(_Jammers, {owner, masterId, MasterComponentList[masterId].RangeOrHeat});
}

auto MCTeam::RemoveJammer(MCSystemTracker* tracker) -> void
{
    RemoveTracker(_Jammers, tracker);
}

auto MCTeam::GetJammerEffect() -> float
{
    for (const MCSystemTracker& tracker : _Jammers)
    {
        if (tracker.Owner != nullptr)
        {
            return tracker.Effect;
        }
    }

    return 1.0f;
}

auto MCTeam::AddEcm(MCGameObject* owner, int32_t masterId) -> MCSystemTracker*
{
    return InsertTracker(_Ecms, {owner, masterId, MasterComponentList[masterId].Damage});
}

auto MCTeam::RemoveEcm(MCSystemTracker* tracker) -> void
{
    RemoveTracker(_Ecms, tracker);
}

auto MCTeam::GetEcmEffect(MCVector3D position) -> float
{
    for (const MCSystemTracker& tracker : _Ecms)
    {
        const int32_t masterId = tracker.MasterId;

        if (masterId != 0x26 && masterId != 0x2a)
        {
            continue;
        }

        MCGameObject* owner = tracker.Owner;

        if (owner == nullptr)
        {
            continue;
        }

        const float ecmRange = MasterComponentList[masterId].RangeOrHeat;

        if (owner->DistanceFrom(position) <= ecmRange && owner->GetExistsAndAwake() != 0 && owner->Status == 0)
        {
            return MasterComponentList[masterId].Damage;
        }
    }

    return 1.0f;
}

auto MCTeam::CalcEscapeVector(MCMover* mover, float range) -> MCVector3D
{
    std::vector<MCVector3D> awayVectors(Roster.size());
    std::vector<float> distances(Roster.size());
    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    int32_t nearest = 0;
    int32_t farthest = 0;

    for (int32_t i = 0; i < RosterSize(); i++)
    {
        MCMover* other = GetMoverFromPartId(Roster[i]);

        if (other == nullptr)
        {
            distances[i] = -999.0f;
            continue;
        }

        MCVector3D otherPosition = other->GetPosition();
        const double unroundedDistance = mover->DistanceFrom(otherPosition);
        const float distance = static_cast<float>(unroundedDistance);

        if (range < unroundedDistance)
        {
            distances[i] = -999.0f;
            continue;
        }

        const MCVector3D from = other->GetPosition();
        const MCVector3D to = mover->GetPosition();
        distances[i] = distance;
        awayVectors[i].X = to.X - from.X;
        awayVectors[i].Y = to.Y - from.Y;
        awayVectors[i].Z = to.Z - from.Z;

        // Original behaviour: the distance is compared with the index itself, not the distance at that index.
        if (static_cast<float>(farthest) < distance)
        {
            farthest = i;
        }

        if (distance < static_cast<float>(nearest))
        {
            nearest = i;
        }
    }

    // Each vector is scaled up by how much nearer than the farthest its mover is.
    for (int32_t i = 0; i < RosterSize(); i++)
    {
        if (0.0f <= distances[i])
        {
            const double scale = static_cast<double>(distances[farthest]) / distances[i];
            awayVectors[i].X = static_cast<float>(scale * awayVectors[i].X);
            awayVectors[i].Y = static_cast<float>(scale * awayVectors[i].Y);
            awayVectors[i].Z = static_cast<float>(scale * awayVectors[i].Z);
            sumX = sumX + awayVectors[i].X;
            sumY = sumY + awayVectors[i].Y;
            sumZ = sumZ + awayVectors[i].Z;
        }
    }

    const double length = std::sqrt((sumY * sumY + sumZ * sumZ) + sumX * sumX);

    if (length != 0.0)
    {
        sumX = sumX / length;
        sumY = sumY / length;
        sumZ = sumZ / length;
    }

    MCVector3D result;
    result.X = static_cast<float>(sumX);
    result.Z = static_cast<float>(sumZ);
    result.Y = static_cast<float>(sumY);
    return result;
}

auto MCTeam::StatusCount(int32_t* counts) -> void
{
    for (int32_t i = 0; i < RosterSize(); i++)
    {
        MCMover* mover = GetMoverFromPartId(Roster[i]);
        Assert(mover != nullptr, static_cast<uint32_t>(i), " Team.statusCount: NULL roster object ");
        const MCMechWarrior* pilot = mover->GetPilot();

        if (mover->GetExists() == 0)
        {
            counts[8]++;
        }
        else if (mover->GetAwake() == 0)
        {
            counts[7]++;
        }
        else if (pilot == nullptr || pilot->Status != 2)
        {
            const uint8_t status = static_cast<uint8_t>(mover->Status);

            if (status > 5)
            {
                Fatal(status, " Status out of bounds ");
            }

            counts[status]++;
        }
        else
        {
            counts[6]++;
        }
    }
}

auto MCTeam::LineOfSight(MCVector3D position) const -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    MCScenarioMap::WorldToMapPos(position, tileR, tileC, cellR, cellC);

    // The clan side sees by terrainVisibleBits, the Inner Sphere by ClanVisibleBits (the names are the original's).
    MCByteFlag* visibleBits;

    if (Alignment == 1)
    {
        visibleBits = Terrain()->ISVisibleBits.get();
    }
    else if (Alignment == -1)
    {
        visibleBits = Terrain()->ClanVisibleBits.get();
    }
    else
    {
        return 0;
    }

    if (visibleBits == nullptr)
    {
        return 0;
    }

    const uint32_t row = static_cast<uint32_t>(tileR);
    const uint32_t col = static_cast<uint32_t>(tileC);

    if (visibleBits->GetFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        return 1;
    }

    return 0;
}

auto DisableHomeTeamTargets() -> void
{
    HomeTeam()->DisableTargets();
}

auto KillHomeTeamTargets() -> void
{
    HomeTeam()->DestroyTargets();
}
