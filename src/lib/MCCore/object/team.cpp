#include "stdafx.h"
#include "object/team.h"
#include "ai/move.h"
#include "engine/bitflag.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/mover.h"
#include "object/sortlist.h"
#include "object/warrior.h"
#include "terrain/terrain.h"

Team* clanTeam = nullptr;
Team* alliedTeam = nullptr;
Team* innerSphereTeam = nullptr;
Team* homeTeam = nullptr;
SortList* ContactSortList = nullptr;
int inContact = 0;

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const BaseObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>
    /// Inserts <paramref name="tracker"/> into <paramref name="list"/> before the first entry no stronger than it.
    /// </summary>
    void InsertTracker(_SystemTracker*& list, _SystemTracker* tracker)
    {
        _SystemTracker* previous = nullptr;

        for (_SystemTracker* current = list; current != nullptr; current = current->next)
        {
            if (current->effect <= tracker->effect)
            {
                if (previous == nullptr)
                {
                    list = tracker;
                }
                else
                {
                    previous->next = tracker;
                }

                tracker->prev = current->prev;
                tracker->next = current;
                current->prev = tracker;
                return;
            }

            previous = current;
        }

        if (previous != nullptr)
        {
            previous->next = tracker;
            tracker->prev = previous;
            return;
        }

        list = tracker;
    }

    /// <summary>Unlinks <paramref name="tracker"/> from <paramref name="list"/> and frees it.</summary>
    void RemoveTracker(_SystemTracker*& list, _SystemTracker* tracker)
    {
        if (tracker == nullptr)
        {
            return;
        }

        if (tracker->next != nullptr)
        {
            tracker->next->prev = tracker->prev;
        }

        if (tracker->prev == nullptr)
        {
            list = tracker->next;
        }
        else
        {
            tracker->prev->next = tracker->next;
        }

        tracker->owner = nullptr;
        delete tracker;
    }

    /// <summary>A new tracker for component <paramref name="masterId"/> of <paramref name="owner"/>.</summary>
    _SystemTracker* NewTracker(GameObject* owner, int32_t masterId, float effect)
    {
        auto* tracker = new _SystemTracker{};
        tracker->owner = owner;
        tracker->masterId = masterId;
        tracker->prev = nullptr;
        tracker->next = nullptr;
        tracker->effect = effect;
        return tracker;
    }

    /// <summary>Frees every tracker of <paramref name="list"/>.</summary>
    void FreeTrackers(_SystemTracker*& list)
    {
        if (list == nullptr)
        {
            return;
        }

        _SystemTracker* tracker = list;

        do
        {
            _SystemTracker* next = tracker->next;
            delete tracker;
            tracker = next;
        } while (tracker != nullptr);

        list = nullptr;
    }
}

auto Team::init() -> void
{
    id = 0;
    alignment = 0;
    rosterSize = 0;
    roster = nullptr;
    sensorsPerUpdate = 3;
    nextSensorUpdate = 0;
    numEnemyContacts = 0;
    numLOSContacts = 0;
    numSensorContacts = 0;
    firstObjective = 0;
    numObjectives = 0;
    sensors = nullptr;
    maxSensors = 0;
    numSensors = 0;
    jammerList = nullptr;
    ecmList = nullptr;
}

auto Team::init(int32_t newId, int32_t newMaxSensors) -> int32_t
{
    id = newId;
    maxSensors = newMaxSensors;
    // Port fix: sized by the port's pointer (4 bytes each in the original).
    sensors = std::make_unique<SensorSystem*[]>(static_cast<size_t>(newMaxSensors));
    return 0;
}

auto Team::buildRoster(Scenario* scenario) -> void
{
    const uint32_t numParts = scenario->numParts;
    auto isTeamMover = [&](uint32_t i)
    {
        const Part& part = scenario->parts[i];
        return IsMover(part.object) && part.teamId == id;
    };

    int32_t count = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) < static_cast<int32_t>(numParts + 1); i++)
    {
        if (isTeamMover(i))
        {
            count++;
        }
    }

    rosterSize = count;
    sensorsPerUpdate = count < 3 ? count : 3;

    roster.reset();

    if (count != 0)
    {
        roster = std::make_unique<int32_t[]>(count);
    }

    int32_t next = 0;

    for (uint32_t i = 1; static_cast<int32_t>(i) < static_cast<int32_t>(numParts + 1); i++)
    {
        if (isTeamMover(i))
        {
            roster[next++] = scenario->parts[i].object->partId;
        }
    }
}

auto Team::addSensor(SensorSystem* sensor) -> void
{
    if (numSensors == maxSensors)
    {
        Fatal(0, " Too Many Sensors, sir! ");
    }

    sensor->teamSensorSlot = numSensors;
    sensors[numSensors] = sensor;
    numSensors++;
}

auto Team::removeSensor(SensorSystem* sensor) -> void
{
    const int32_t slot = sensor->teamSensorSlot;
    sensor->teamSensorSlot = -1;
    sensors[slot] = nullptr;

    if (slot < numSensors - 1)
    {
        // The last sensor fills the gap.
        SensorSystem* last = sensors[numSensors - 1];
        sensors[slot] = last;
        last->teamSensorSlot = slot;
        sensors[numSensors - 1] = nullptr;
    }

    numSensors--;
}

auto Team::updateSensors() -> void
{
    if (numSensors <= 0)
    {
        return;
    }

    for (int32_t i = 0; i < numSensors; i++)
    {
        sensors[i]->updateScan(0);
    }

    // A few sensors a frame re-check the contacts they hold, in turn.
    for (int32_t i = 0; i < sensorsPerUpdate; i++)
    {
        if (nextSensorUpdate >= numSensors)
        {
            nextSensorUpdate = 0;
        }

        sensors[nextSensorUpdate]->updateContacts();
        nextSensorUpdate++;
    }
}

auto Team::getLOSContacts(GameObject** objects) -> int32_t
{
    for (int32_t i = 0; i < numLOSContacts; i++)
    {
        objects[i] = potentialContactManager->contacts[losContacts[i]].object;
    }

    return numLOSContacts;
}

auto Team::getSensorContacts(GameObject** objects) -> int32_t
{
    for (int32_t i = 0; i < numSensorContacts; i++)
    {
        objects[i] = potentialContactManager->contacts[sensorContacts[i]].object;
    }

    return numSensorContacts;
}

auto Team::getContacts(GameObject* looker, int32_t* contacts, int32_t contactCriteria, int32_t sortType) -> int32_t
{
    // Port fix: the original's table held 200 values (MCX.EXE @ 0x007e4614), fewer than a list's 500 contacts.
    static float sortValues[MAX_TEAM_CONTACTS];

    const int32_t enemyAlignment = alignment == -1 ? 1 : -1;
    int32_t count = numLOSContacts;
    const int16_t* list = losContacts;

    if ((contactCriteria & 0x10) != 0)
    {
        count = numSensorContacts;
        list = sensorContacts;
    }

    int32_t numFound = 0;

    for (int32_t i = 0; i < count; i++)
    {
        BigGameObject* object = potentialContactManager->contacts[list[i]].object;

        if ((contactCriteria & 8) != 0 && IsMover(object) && static_cast<Mover*>(object)->getChallenger() != nullptr)
        {
            continue;
        }

        if ((contactCriteria & 1) != 0 && object->getAlignment() != enemyAlignment)
        {
            continue;
        }

        contacts[numFound] = object->partId;

        if (sortType == 0)
        {
            sortValues[numFound] = 0.0f;
        }
        else if (sortType == 1)
        {
            sortValues[numFound] = IsMover(object) ? static_cast<float>(object->getCurCV()) : 0.0f;
        }
        else if (sortType == 2)
        {
            vector_3d position = object->getPosition();
            sortValues[numFound] = static_cast<float>(looker->distanceFrom(position));
        }

        numFound++;
    }

    if (numFound <= 0 || sortType == 0)
    {
        return numFound;
    }

    if (ContactSortList == nullptr)
    {
        ContactSortList = new SortList;

        if (ContactSortList == nullptr)
        {
            Fatal(0, " Unable to create Team Contact sortList ");
        }

        ContactSortList->init(200);
    }

    // By value, highest first; by distance, nearest first.
    SortList* sortList = ContactSortList;
    sortList->clear(sortType != 2);

    for (int32_t i = 0; i < numFound; i++)
    {
        if (i < sortList->numItems)
        {
            sortList->list[i].id = contacts[i];
            sortList->list[i].value = sortValues[i];
        }
    }

    sortList->sort(sortType != 2);
    // Port fix: the original copied numFound entries back even past the sort list's 200.
    const int32_t numSorted = numFound < sortList->numItems ? numFound : sortList->numItems;

    for (int32_t i = 0; i < numSorted; i++)
    {
        contacts[i] = sortList->list[i].id;
    }

    return numFound;
}

auto Team::getContactType(GameObject* object) -> int32_t
{
    return object->getContactType(id);
}

auto Team::isContact(GameObject* object, int32_t contactCriteria) -> int
{
    const int32_t contactType = object->getContactType(id);

    if (contactType == CONTACT_NONE)
    {
        return 0;
    }

    if ((contactCriteria & 1) != 0 && object->getAlignment() == alignment)
    {
        return 0;
    }

    if ((contactCriteria & 2) != 0 && contactType != CONTACT_VISUAL)
    {
        return 0;
    }

    if ((contactCriteria & 8) != 0 && IsMover(object) && static_cast<Mover*>(object)->getChallenger() != nullptr)
    {
        return 0;
    }

    return 1;
}

auto Team::scanBattlefield() -> void
{
    for (int32_t i = 0; i < numSensors; i++)
    {
        sensors[i]->updateScan(1);
    }
}

auto Team::incNumEnemyContacts() -> void
{
    numEnemyContacts++;

    if (this == homeTeam && numEnemyContacts != 0)
    {
        inContact = 1;
    }
}

auto Team::decNumEnemyContacts() -> void
{
    numEnemyContacts--;

    if (numEnemyContacts == 0)
    {
        if (this == homeTeam)
        {
            inContact = 0;
        }
    }
    else if (numEnemyContacts < 0)
    {
        Fatal(0, " Negative Team Contact Count ");
    }
}

auto Team::addLOSContact(_PotentialContact* contact) -> void
{
    // The original tests the sensor list's count, not the LOS list's.
    if (numSensorContacts >= MAX_TEAM_CONTACTS)
    {
        return;
    }

    losContacts[numLOSContacts] = static_cast<int16_t>(contact->id);
    contact->teamSlot[id] = static_cast<int16_t>(numLOSContacts);
    numLOSContacts++;
}

auto Team::removeLOSContact(int32_t index) -> void
{
    _PotentialContact* pool = potentialContactManager->contacts.get();
    pool[losContacts[index]].teamSlot[id] = -1;
    numLOSContacts--;

    if (numLOSContacts > 0 && index != numLOSContacts)
    {
        losContacts[index] = losContacts[numLOSContacts];
        pool[losContacts[numLOSContacts]].teamSlot[id] = static_cast<int16_t>(index);
    }
}

auto Team::removeLOSContact(_PotentialContact* contact) -> void
{
    const uint16_t slot = static_cast<uint16_t>(contact->teamSlot[id]);

    if (slot < 0xffff)
    {
        removeLOSContact(static_cast<int32_t>(slot));
    }
}

auto Team::addSensorContact(_PotentialContact* contact) -> void
{
    if (numSensorContacts >= MAX_TEAM_CONTACTS)
    {
        return;
    }

    sensorContacts[numSensorContacts] = static_cast<int16_t>(contact->id);
    contact->teamSlot[id] = static_cast<int16_t>(numSensorContacts);
    numSensorContacts++;
}

auto Team::removeSensorContact(int32_t index) -> void
{
    _PotentialContact* pool = potentialContactManager->contacts.get();
    pool[sensorContacts[index]].teamSlot[id] = -1;
    numSensorContacts--;

    if (numSensorContacts > 0 && index != numSensorContacts)
    {
        sensorContacts[index] = sensorContacts[numSensorContacts];
        pool[sensorContacts[numSensorContacts]].teamSlot[id] = static_cast<int16_t>(index);
    }
}

auto Team::removeSensorContact(_PotentialContact* contact) -> void
{
    const uint16_t slot = static_cast<uint16_t>(contact->teamSlot[id]);

    if (slot < 0xffff)
    {
        removeSensorContact(static_cast<int32_t>(slot));
    }
}

auto Team::getRoster(GameObject** objects) -> int32_t
{
    int32_t count = 0;

    for (int32_t i = 0; i < rosterSize; i++)
    {
        Mover* mover = getMoverFromPartId(roster[i]);

        if (mover != nullptr)
        {
            objects[count++] = mover;
        }
    }

    return count;
}

auto Team::disableTargets() -> void
{
    for (int32_t i = 0; i < rosterSize; i++)
    {
        Mover* mover = getMoverFromPartId(roster[i]);

        if (mover == nullptr || !IsMover(mover))
        {
            continue;
        }

        GameObject* target = mover->getPilot()->getLastTarget();

        if (target != nullptr && IsMover(target))
        {
            static_cast<Mover*>(target)->disable(0x42);
        }
    }
}

auto Team::destroyTargets() -> void
{
    for (int32_t i = 0; i < rosterSize; i++)
    {
        Mover* mover = getMoverFromPartId(roster[i]);

        if (mover == nullptr || !IsMover(mover))
        {
            continue;
        }

        GameObject* target = mover->getPilot()->getLastTarget();

        if (target == nullptr || !IsMover(target))
        {
            continue;
        }

        _WeaponShotInfo shotInfo;
        shotInfo.init(nullptr, -3, 5.0f, 0, 0.0f);

        for (int32_t shot = 0; shot < 100; shot++)
        {
            if (RollDice(30) == 0)
            {
                shotInfo.hitLocation = target->calcHitLocation(nullptr, -1, 4, 0);
            }
            else
            {
                shotInfo.hitLocation = target->calcHitLocation(nullptr, -1, 2, 0);
            }

            target->handleWeaponHit(&shotInfo, MPlayer != nullptr ? 1 : 0);
        }
    }
}

auto Team::isTargeting(uint32_t targetPartId, uint32_t exceptPartId) -> int
{
    for (int32_t i = 0; i < rosterSize; i++)
    {
        if (exceptPartId != 0 && static_cast<uint32_t>(roster[i]) == exceptPartId)
        {
            continue;
        }

        Mover* mover = getMoverFromPartId(roster[i]);

        if (mover == nullptr)
        {
            continue;
        }

        MechWarrior* pilot = mover->getPilot();

        if (pilot == nullptr)
        {
            continue;
        }

        GameObject* target = pilot->getLastTarget();

        if (target != nullptr && static_cast<uint32_t>(target->partId) == targetPartId)
        {
            return 1;
        }
    }

    return 0;
}

auto Team::addJammer(GameObject* owner, int32_t masterId) -> _SystemTracker*
{
    _SystemTracker* tracker = NewTracker(owner, masterId, MasterComponentList[masterId].rangeOrHeat);
    InsertTracker(jammerList, tracker);
    return tracker;
}

auto Team::removeJammer(_SystemTracker* tracker) -> void
{
    RemoveTracker(jammerList, tracker);
}

auto Team::getJammerEffect() -> float
{
    for (const _SystemTracker* tracker = jammerList; tracker != nullptr; tracker = tracker->next)
    {
        if (tracker->owner != nullptr)
        {
            return tracker->effect;
        }
    }

    return 1.0f;
}

auto Team::addECM(GameObject* owner, int32_t masterId) -> _SystemTracker*
{
    _SystemTracker* tracker = NewTracker(owner, masterId, MasterComponentList[masterId].damage);
    InsertTracker(ecmList, tracker);
    return tracker;
}

auto Team::removeECM(_SystemTracker* tracker) -> void
{
    RemoveTracker(ecmList, tracker);
}

auto Team::getECMEffect(vector_3d position) -> float
{
    for (const _SystemTracker* tracker = ecmList; tracker != nullptr; tracker = tracker->next)
    {
        const int32_t masterId = tracker->masterId;

        if (masterId != 0x26 && masterId != 0x2a)
        {
            continue;
        }

        GameObject* owner = tracker->owner;

        if (owner == nullptr)
        {
            continue;
        }

        const float ecmRange = MasterComponentList[masterId].rangeOrHeat;

        if (owner->distanceFrom(position) <= ecmRange && owner->getExistsAndAwake() != 0 && owner->status == 0)
        {
            return MasterComponentList[tracker->masterId].damage;
        }
    }

    return 1.0f;
}

auto Team::calcEscapeVector(Mover* mover, float range) -> vector_3d
{
    // Function statics in the original (MCX.EXE @ 0x007e3fd0 and 0x007e4480): 100 entries, the roster's limit.
    static vector_3d awayVectors[100];
    static float distances[100];

    double sumX = 0.0;
    double sumY = 0.0;
    double sumZ = 0.0;
    int32_t nearest = 0;
    int32_t farthest = 0;

    for (int32_t i = 0; i < rosterSize; i++)
    {
        Mover* other = getMoverFromPartId(roster[i]);

        if (other == nullptr)
        {
            distances[i] = -999.0f;
            continue;
        }

        vector_3d otherPosition = other->getPosition();
        const double unroundedDistance = mover->distanceFrom(otherPosition);
        const float distance = static_cast<float>(unroundedDistance);

        if (range < unroundedDistance)
        {
            distances[i] = -999.0f;
            continue;
        }

        const vector_3d from = other->getPosition();
        const vector_3d to = mover->getPosition();
        distances[i] = distance;
        awayVectors[i].x = to.x - from.x;
        awayVectors[i].y = to.y - from.y;
        awayVectors[i].z = to.z - from.z;

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
    for (int32_t i = 0; i < rosterSize; i++)
    {
        if (0.0f <= distances[i])
        {
            const double scale = static_cast<double>(distances[farthest]) / distances[i];
            awayVectors[i].x = static_cast<float>(scale * awayVectors[i].x);
            awayVectors[i].y = static_cast<float>(scale * awayVectors[i].y);
            awayVectors[i].z = static_cast<float>(scale * awayVectors[i].z);
            sumX = sumX + awayVectors[i].x;
            sumY = sumY + awayVectors[i].y;
            sumZ = sumZ + awayVectors[i].z;
        }
    }

    const double length = std::sqrt((sumY * sumY + sumZ * sumZ) + sumX * sumX);

    if (length != 0.0)
    {
        sumX = sumX / length;
        sumY = sumY / length;
        sumZ = sumZ / length;
    }

    vector_3d result;
    result.x = static_cast<float>(sumX);
    result.z = static_cast<float>(sumZ);
    result.y = static_cast<float>(sumY);
    return result;
}

auto Team::statusCount(int32_t* counts) -> void
{
    for (int32_t i = 0; i < rosterSize; i++)
    {
        Mover* mover = getMoverFromPartId(roster[i]);
        Assert(mover != nullptr, static_cast<uint32_t>(i), " Team.statusCount: NULL roster object ");
        const MechWarrior* pilot = mover->getPilot();

        if (mover->getExists() == 0)
        {
            counts[8]++;
        }
        else if (mover->getAwake() == 0)
        {
            counts[7]++;
        }
        else if (pilot == nullptr || pilot->status != 2)
        {
            const uint8_t status = static_cast<uint8_t>(mover->status);

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

auto Team::destroy() -> void
{
    sensors.reset();
    FreeTrackers(ecmList);
    FreeTrackers(jammerList);
    roster.reset();
}

auto Team::lineOfSight(vector_3d position) -> int
{
    int32_t tileR;
    int32_t tileC;
    int32_t cellR;
    int32_t cellC;
    GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);

    // The clan side sees by terrainVisibleBits, the Inner Sphere by ClanVisibleBits (the names are the original's).
    ByteFlag* visibleBits;

    if (alignment == 1)
    {
        visibleBits = Terrain::terrainVisibleBits;
    }
    else if (alignment == -1)
    {
        visibleBits = Terrain::ClanVisibleBits;
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

    if (visibleBits->getFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        return 1;
    }

    return 0;
}

auto disableHomeTeamTargets() -> void
{
    homeTeam->disableTargets();
}

auto killHomeTeamTargets() -> void
{
    homeTeam->destroyTargets();
}
