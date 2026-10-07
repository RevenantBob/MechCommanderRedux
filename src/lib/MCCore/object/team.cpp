#include "stdafx.h"
#include "object/team.h"
#include "ai/move.h"
#include "engine/bitflag.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/mover.h"
#include "object/sortlist.h"
#include "object/warrior.h"
#include "terrain/terrain.h"

MCTeam* ClanTeam = nullptr;
MCTeam* AlliedTeam = nullptr;
MCTeam* InnerSphereTeam = nullptr;
MCTeam* HomeTeam = nullptr;
MCSortList* ContactSortList = nullptr;
int InContact = 0;

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const MCBaseObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>
    /// Inserts <paramref name="tracker"/> into <paramref name="list"/> before the first entry no stronger than it.
    /// </summary>
    void InsertTracker(MCSystemTracker*& list, MCSystemTracker* tracker)
    {
        MCSystemTracker* previous = nullptr;

        for (MCSystemTracker* current = list; current != nullptr; current = current->Next)
        {
            if (current->Effect <= tracker->Effect)
            {
                if (previous == nullptr)
                {
                    list = tracker;
                }
                else
                {
                    previous->Next = tracker;
                }

                tracker->Prev = current->Prev;
                tracker->Next = current;
                current->Prev = tracker;
                return;
            }

            previous = current;
        }

        if (previous != nullptr)
        {
            previous->Next = tracker;
            tracker->Prev = previous;
            return;
        }

        list = tracker;
    }

    /// <summary>Unlinks <paramref name="tracker"/> from <paramref name="list"/> and frees it.</summary>
    void RemoveTracker(MCSystemTracker*& list, MCSystemTracker* tracker)
    {
        if (tracker == nullptr)
        {
            return;
        }

        if (tracker->Next != nullptr)
        {
            tracker->Next->Prev = tracker->Prev;
        }

        if (tracker->Prev == nullptr)
        {
            list = tracker->Next;
        }
        else
        {
            tracker->Prev->Next = tracker->Next;
        }

        tracker->Owner = nullptr;
        delete tracker;
    }

    /// <summary>A new tracker for component <paramref name="masterId"/> of <paramref name="owner"/>.</summary>
    MCSystemTracker* NewTracker(MCGameObject* owner, int32_t masterId, float effect)
    {
        auto* tracker = new MCSystemTracker{};
        tracker->Owner = owner;
        tracker->MasterId = masterId;
        tracker->Prev = nullptr;
        tracker->Next = nullptr;
        tracker->Effect = effect;
        return tracker;
    }

    /// <summary>Frees every tracker of <paramref name="list"/>.</summary>
    void FreeTrackers(MCSystemTracker*& list)
    {
        if (list == nullptr)
        {
            return;
        }

        MCSystemTracker* tracker = list;

        do
        {
            MCSystemTracker* next = tracker->Next;
            delete tracker;
            tracker = next;
        } while (tracker != nullptr);

        list = nullptr;
    }
}

auto MCTeam::Init() -> void
{
    Id = 0;
    Alignment = 0;
    RosterSize = 0;
    Roster = nullptr;
    SensorsPerUpdate = 3;
    NextSensorUpdate = 0;
    NumEnemyContacts = 0;
    NumLosContacts = 0;
    NumSensorContacts = 0;
    FirstObjective = 0;
    NumObjectives = 0;
    Sensors = nullptr;
    MaxSensors = 0;
    NumSensors = 0;
    JammerList = nullptr;
    EcmList = nullptr;
}

auto MCTeam::Init(int32_t newId, int32_t newMaxSensors) -> int32_t
{
    Id = newId;
    MaxSensors = newMaxSensors;
    // Port fix: sized by the port's pointer (4 bytes each in the original).
    Sensors = std::make_unique<MCSensorSystem*[]>(static_cast<size_t>(newMaxSensors));
    return 0;
}

auto MCTeam::BuildRoster(MCScenario* scenario) -> void
{
    const uint32_t numParts = scenario->NumParts;
    auto isTeamMover = [&](uint32_t i)
    {
        const MCPart& part = scenario->Parts[i];
        return IsMover(part.Object) && part.TeamId == Id;
    };

    int32_t count = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) < static_cast<int32_t>(numParts + 1); i++)
    {
        if (isTeamMover(i))
        {
            count++;
        }
    }

    RosterSize = count;
    SensorsPerUpdate = count < 3 ? count : 3;

    Roster.reset();

    if (count != 0)
    {
        Roster = std::make_unique<int32_t[]>(count);
    }

    int32_t next = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) < static_cast<int32_t>(numParts + 1); i++)
    {
        if (isTeamMover(i))
        {
            Roster[next++] = scenario->Parts[i].Object->PartId;
        }
    }
}

auto MCTeam::AddSensor(MCSensorSystem* sensor) -> void
{
    if (NumSensors == MaxSensors)
    {
        Fatal(0, " Too Many Sensors, sir! ");
    }

    sensor->TeamSensorSlot = NumSensors;
    Sensors[NumSensors] = sensor;
    NumSensors++;
}

auto MCTeam::RemoveSensor(MCSensorSystem* sensor) -> void
{
    const int32_t slot = sensor->TeamSensorSlot;
    sensor->TeamSensorSlot = -1;
    Sensors[slot] = nullptr;

    if (slot < NumSensors - 1)
    {
        // The last sensor fills the gap.
        MCSensorSystem* last = Sensors[NumSensors - 1];
        Sensors[slot] = last;
        last->TeamSensorSlot = slot;
        Sensors[NumSensors - 1] = nullptr;
    }

    NumSensors--;
}

auto MCTeam::UpdateSensors() -> void
{
    if (NumSensors <= 0)
    {
        return;
    }

    for (int32_t i = 0; i < NumSensors; i++)
    {
        Sensors[i]->UpdateScan(0);
    }

    // A few sensors a frame re-check the contacts they hold, in turn.
    for (int32_t i = 0; i < SensorsPerUpdate; i++)
    {
        if (NextSensorUpdate >= NumSensors)
        {
            NextSensorUpdate = 0;
        }

        Sensors[NextSensorUpdate]->UpdateContacts();
        NextSensorUpdate++;
    }
}

auto MCTeam::GetLosContacts(MCGameObject** objects) -> int32_t
{
    for (int32_t i = 0; i < NumLosContacts; i++)
    {
        objects[i] = PotentialContactManager->Contacts[LosContacts[i]].Object;
    }

    return NumLosContacts;
}

auto MCTeam::GetSensorContacts(MCGameObject** objects) -> int32_t
{
    for (int32_t i = 0; i < NumSensorContacts; i++)
    {
        objects[i] = PotentialContactManager->Contacts[SensorContacts[i]].Object;
    }

    return NumSensorContacts;
}

auto MCTeam::GetContacts(MCGameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType) -> int32_t
{
    // Port fix: the original's table held 200 values, fewer than a list's 500 contacts.
    static float sortValues[MAX_TEAM_CONTACTS];

    const int32_t enemyAlignment = Alignment == -1 ? 1 : -1;
    int32_t count = NumLosContacts;
    const int16_t* list = LosContacts;

    if ((contactCriteria & 0x10) != 0)
    {
        count = NumSensorContacts;
        list = SensorContacts;
    }

    int32_t numFound = 0;

    for (int32_t i = 0; i < count; i++)
    {
        MCBigGameObject* object = PotentialContactManager->Contacts[list[i]].Object;

        if ((contactCriteria & 8) != 0 && IsMover(object) && static_cast<MCMover*>(object)->GetChallenger() != nullptr)
        {
            continue;
        }

        if ((contactCriteria & 1) != 0 && object->GetAlignment() != enemyAlignment)
        {
            continue;
        }

        contacts[numFound] = object->PartId;

        if (sortType == 0)
        {
            sortValues[numFound] = 0.0f;
        }
        else if (sortType == 1)
        {
            sortValues[numFound] = IsMover(object) ? static_cast<float>(object->GetCurCV()) : 0.0f;
        }
        else if (sortType == 2)
        {
            MCVector3D position = object->GetPosition();
            sortValues[numFound] = static_cast<float>(looker->DistanceFrom(position));
        }

        numFound++;
    }

    if (numFound <= 0 || sortType == 0)
    {
        return numFound;
    }

    if (ContactSortList == nullptr)
    {
        ContactSortList = new MCSortList;

        if (ContactSortList == nullptr)
        {
            Fatal(0, " Unable to create Team Contact sortList ");
        }

        ContactSortList->Init(200);
    }

    // By value, highest first; by distance, nearest first.
    MCSortList* sortList = ContactSortList;
    sortList->Clear(sortType != 2);

    for (int32_t i = 0; i < numFound; i++)
    {
        if (i < sortList->NumItems)
        {
            sortList->List[i].Id = contacts[i];
            sortList->List[i].Value = sortValues[i];
        }
    }

    sortList->Sort(sortType != 2);
    // Port fix: the original copied numFound entries back even past the sort list's 200.
    const int32_t numSorted = numFound < sortList->NumItems ? numFound : sortList->NumItems;

    for (int32_t i = 0; i < numSorted; i++)
    {
        contacts[i] = sortList->List[i].Id;
    }

    return numFound;
}

auto MCTeam::GetContactType(MCGameObject* object) -> int32_t
{
    return object->GetContactType(Id);
}

auto MCTeam::IsContact(MCGameObject* object, int32_t contactCriteria) -> int
{
    const int32_t contactType = object->GetContactType(Id);

    if (contactType == CONTACT_NONE)
    {
        return 0;
    }

    if ((contactCriteria & 1) != 0 && object->GetAlignment() == Alignment)
    {
        return 0;
    }

    if ((contactCriteria & 2) != 0 && contactType != CONTACT_VISUAL)
    {
        return 0;
    }

    if ((contactCriteria & 8) != 0 && IsMover(object) && static_cast<MCMover*>(object)->GetChallenger() != nullptr)
    {
        return 0;
    }

    return 1;
}

auto MCTeam::ScanBattlefield() -> void
{
    for (int32_t i = 0; i < NumSensors; i++)
    {
        Sensors[i]->UpdateScan(1);
    }
}

auto MCTeam::IncNumEnemyContacts() -> void
{
    NumEnemyContacts++;

    if (this == HomeTeam && NumEnemyContacts != 0)
    {
        InContact = 1;
    }
}

auto MCTeam::DecNumEnemyContacts() -> void
{
    NumEnemyContacts--;

    if (NumEnemyContacts == 0)
    {
        if (this == HomeTeam)
        {
            InContact = 0;
        }
    }
    else if (NumEnemyContacts < 0)
    {
        Fatal(0, " Negative Team Contact Count ");
    }
}

auto MCTeam::AddLosContact(MCPotentialContact* contact) -> void
{
    // The original tests the sensor list's count, not the LOS list's.
    if (NumSensorContacts >= MAX_TEAM_CONTACTS)
    {
        return;
    }

    LosContacts[NumLosContacts] = static_cast<int16_t>(contact->Id);
    contact->TeamSlot[Id] = static_cast<int16_t>(NumLosContacts);
    NumLosContacts++;
}

auto MCTeam::RemoveLosContact(int32_t index) -> void
{
    MCPotentialContact* pool = PotentialContactManager->Contacts.get();
    pool[LosContacts[index]].TeamSlot[Id] = -1;
    NumLosContacts--;

    if (NumLosContacts > 0 && index != NumLosContacts)
    {
        LosContacts[index] = LosContacts[NumLosContacts];
        pool[LosContacts[NumLosContacts]].TeamSlot[Id] = static_cast<int16_t>(index);
    }
}

auto MCTeam::RemoveLosContact(MCPotentialContact* contact) -> void
{
    const uint16_t slot = static_cast<uint16_t>(contact->TeamSlot[Id]);

    if (slot < 0xffff)
    {
        RemoveLosContact(static_cast<int32_t>(slot));
    }
}

auto MCTeam::AddSensorContact(MCPotentialContact* contact) -> void
{
    if (NumSensorContacts >= MAX_TEAM_CONTACTS)
    {
        return;
    }

    SensorContacts[NumSensorContacts] = static_cast<int16_t>(contact->Id);
    contact->TeamSlot[Id] = static_cast<int16_t>(NumSensorContacts);
    NumSensorContacts++;
}

auto MCTeam::RemoveSensorContact(int32_t index) -> void
{
    MCPotentialContact* pool = PotentialContactManager->Contacts.get();
    pool[SensorContacts[index]].TeamSlot[Id] = -1;
    NumSensorContacts--;

    if (NumSensorContacts > 0 && index != NumSensorContacts)
    {
        SensorContacts[index] = SensorContacts[NumSensorContacts];
        pool[SensorContacts[NumSensorContacts]].TeamSlot[Id] = static_cast<int16_t>(index);
    }
}

auto MCTeam::RemoveSensorContact(MCPotentialContact* contact) -> void
{
    const uint16_t slot = static_cast<uint16_t>(contact->TeamSlot[Id]);

    if (slot < 0xffff)
    {
        RemoveSensorContact(static_cast<int32_t>(slot));
    }
}

auto MCTeam::GetRoster(MCGameObject** objects) -> int32_t
{
    int32_t count = 0;

    for (int32_t i = 0; i < RosterSize; i++)
    {
        MCMover* mover = GetMoverFromPartId(Roster[i]);

        if (mover != nullptr)
        {
            objects[count++] = mover;
        }
    }

    return count;
}

auto MCTeam::DisableTargets() -> void
{
    for (int32_t i = 0; i < RosterSize; i++)
    {
        MCMover* mover = GetMoverFromPartId(Roster[i]);

        if (mover == nullptr || !IsMover(mover))
        {
            continue;
        }

        MCGameObject* target = mover->GetPilot()->GetLastTarget();

        if (target != nullptr && IsMover(target))
        {
            static_cast<MCMover*>(target)->Disable(0x42);
        }
    }
}

auto MCTeam::DestroyTargets() -> void
{
    for (int32_t i = 0; i < RosterSize; i++)
    {
        MCMover* mover = GetMoverFromPartId(Roster[i]);

        if (mover == nullptr || !IsMover(mover))
        {
            continue;
        }

        MCGameObject* target = mover->GetPilot()->GetLastTarget();

        if (target == nullptr || !IsMover(target))
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

            target->HandleWeaponHit(&shotInfo, MPlayer != nullptr ? 1 : 0);
        }
    }
}

auto MCTeam::IsTargeting(uint32_t targetPartId, uint32_t exceptPartId) -> int
{
    for (int32_t i = 0; i < RosterSize; i++)
    {
        if (exceptPartId != 0 && static_cast<uint32_t>(Roster[i]) == exceptPartId)
        {
            continue;
        }

        MCMover* mover = GetMoverFromPartId(Roster[i]);

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
    MCSystemTracker* tracker = NewTracker(owner, masterId, MasterComponentList[masterId].RangeOrHeat);
    InsertTracker(JammerList, tracker);
    return tracker;
}

auto MCTeam::RemoveJammer(MCSystemTracker* tracker) -> void
{
    RemoveTracker(JammerList, tracker);
}

auto MCTeam::GetJammerEffect() -> float
{
    for (const MCSystemTracker* tracker = JammerList; tracker != nullptr; tracker = tracker->Next)
    {
        if (tracker->Owner != nullptr)
        {
            return tracker->Effect;
        }
    }

    return 1.0f;
}

auto MCTeam::AddEcm(MCGameObject* owner, int32_t masterId) -> MCSystemTracker*
{
    MCSystemTracker* tracker = NewTracker(owner, masterId, MasterComponentList[masterId].Damage);
    InsertTracker(EcmList, tracker);
    return tracker;
}

auto MCTeam::RemoveEcm(MCSystemTracker* tracker) -> void
{
    RemoveTracker(EcmList, tracker);
}

auto MCTeam::GetEcmEffect(MCVector3D position) -> float
{
    for (const MCSystemTracker* tracker = EcmList; tracker != nullptr; tracker = tracker->Next)
    {
        const int32_t masterId = tracker->MasterId;

        if (masterId != 0x26 && masterId != 0x2a)
        {
            continue;
        }

        MCGameObject* owner = tracker->Owner;

        if (owner == nullptr)
        {
            continue;
        }

        const float ecmRange = MasterComponentList[masterId].RangeOrHeat;

        if (owner->DistanceFrom(position) <= ecmRange && owner->GetExistsAndAwake() != 0 && owner->Status == 0)
        {
            return MasterComponentList[tracker->MasterId].Damage;
        }
    }

    return 1.0f;
}

auto MCTeam::CalcEscapeVector(MCMover* mover, float range) -> MCVector3D
{
    // Function statics in the original: 100 entries, the roster's limit.
    static MCVector3D awayVectors[100];
    static float distances[100];

    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    int32_t nearest = 0;
    int32_t farthest = 0;

    for (int32_t i = 0; i < RosterSize; i++)
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

        // The original compares the distance with the index itself, not the distance at that index.
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
    for (int32_t i = 0; i < RosterSize; i++)
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
    for (int32_t i = 0; i < RosterSize; i++)
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

auto MCTeam::Destroy() -> void
{
    Sensors.reset();
    FreeTrackers(EcmList);
    FreeTrackers(JammerList);
    Roster.reset();
}

auto MCTeam::LineOfSight(MCVector3D position) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->WorldToMapPos(position, tileR, tileC, cellR, cellC);

    // The clan side sees by terrainVisibleBits, the Inner Sphere by ClanVisibleBits (the names are the original's).
    MCByteFlag* visibleBits;

    if (Alignment == 1)
    {
        visibleBits = MCTerrain::TerrainVisibleBits;
    }
    else if (Alignment == -1)
    {
        visibleBits = MCTerrain::ClanVisibleBits;
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
    HomeTeam->DisableTargets();
}

auto KillHomeTeamTargets() -> void
{
    HomeTeam->DestroyTargets();
}
