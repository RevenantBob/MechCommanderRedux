#include "stdafx.h"
#include "object/contact.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/cmponent.h"
#include "object/mover.h"
#include "object/objtype.h"
#include "object/sortlist.h"
#include "object/team.h"
#include "object/warrior.h"
#include "mission/scenario.h"
#include "sound/radio.h"

PotentialContactManager* potentialContactManager = nullptr;
SensorSystemManager* sensorSystemManager = nullptr;
int32_t SensorSystem::numSensors = 0;
SortList* SensorSystem::sortList = nullptr;
int SensorAutomaticSuccess = 0;
char SensorSkillMoveRange[4] = {45, 59, 69, 80};
float SensorSkillMoveFactor[4][2] = {{0.8f, 0.5f}, {0.85f, 0.55f}, {0.9f, 0.6f}, {0.95f, 0.65f}};
float SensorModifier[8] = {40.0f, 0.5f, 1.0f, 100.0f, -50.0f, 0.0f, -30.0f, -40.0f};

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const GameObject* object)
    {
        const ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }
}

//---------------------------------------------------------------------------
// _PotentialContact
//---------------------------------------------------------------------------

auto _PotentialContact::init() -> void
{
    object = nullptr;
    contactType = POTENTIAL_CONTACT_ALLIED;
    visibility = 0;

    for (int32_t i = 0; i < 3; i++)
    {
        contactStatus[i] = CONTACT_NONE;
        numSensors[i] = 0;
        lostVisual[i] = 0;
        teamSlot[i] = -1;
    }

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        sensorSlot[i] = 0xff;
    }

    next = nullptr;
    prev = nullptr;
}

auto _PotentialContact::updateStatus(Team* team) -> void
{
    if (team == nullptr)
    {
        return;
    }

    const int32_t teamId = team->id;
    const uint8_t oldStatus = contactStatus[teamId];
    uint8_t newStatus = CONTACT_NONE;

    if (team->lineOfSight(object->getPosition()) == 0)
    {
        if (numSensors[teamId] != 0)
        {
            newStatus = CONTACT_SENSOR;
        }
    }
    else if (visibility != 3)
    {
        newStatus = CONTACT_VISUAL;
    }

    if (oldStatus == newStatus)
    {
        return;
    }

    contactStatus[teamId] = newStatus;

    if (oldStatus == CONTACT_VISUAL)
    {
        team->removeLOSContact(this);
    }
    else if (oldStatus == CONTACT_SENSOR)
    {
        lostVisual[teamId] = 0;
        team->removeSensorContact(this);
    }

    if (newStatus == CONTACT_VISUAL)
    {
        team->addLOSContact(this);
        return;
    }

    if (newStatus == CONTACT_SENSOR)
    {
        if (oldStatus == CONTACT_VISUAL)
        {
            lostVisual[teamId] = 1;
        }

        team->addSensorContact(this);
    }
}

//---------------------------------------------------------------------------
// PotentialContactManager
//---------------------------------------------------------------------------

auto PotentialContactManager::operator new(size_t size) noexcept -> void*
{
    return ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));
}

auto PotentialContactManager::operator delete(void* ptr) -> void
{
    ObjectTypeManager::objectCache->free(ptr);
}

auto PotentialContactManager::init(FitIniFile* file) -> int32_t
{
    maxContacts = 0;
    numFree = 0;
    contactList[0] = nullptr;
    contactList[1] = nullptr;
    contactList[2] = nullptr;
    contacts = nullptr;
    freeList = nullptr;

    int32_t result = file->seekBlock("PotentialContactManager");

    if (result != 0)
    {
        return result;
    }

    result = file->readIdLong("MaxPotentialContacts", maxContacts);

    if (result != 0)
    {
        return result;
    }

    if (maxContacts < 2)
    {
        Fatal(0, " Way too few contacts in Potential Contact Manager! ");
    }

    // Port fix: sized by the port's struct (0x60 bytes in the original).
    contacts = static_cast<_PotentialContact*>(
        ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(maxContacts * sizeof(_PotentialContact))));

    if (contacts == nullptr)
    {
        Fatal(static_cast<int32_t>(0xdddd0002), " No RAM For Potential Contact Manager ");
    }

    // Chain the pool into the free list; the rest of each contact is set when add() takes it.
    contacts[0].id = 0;
    contacts[0].object = nullptr;
    contacts[0].prev = nullptr;
    contacts[0].next = &contacts[1];

    for (int32_t i = 1; i < maxContacts - 1; i++)
    {
        contacts[i].id = static_cast<uint16_t>(i);
        contacts[i].object = nullptr;
        contacts[i].prev = &contacts[i - 1];
        contacts[i].next = &contacts[i + 1];
    }

    _PotentialContact& last = contacts[maxContacts - 1];
    last.id = static_cast<uint16_t>(maxContacts - 1);
    last.object = nullptr;
    last.prev = &contacts[maxContacts - 2];
    last.next = nullptr;

    freeList = contacts;
    numFree = maxContacts;
    return 0;
}

auto PotentialContactManager::add(int32_t type, BigGameObject* object, char visibility) -> _PotentialContact*
{
    if (numFree == 0)
    {
        Fatal(0, " No More Free Potential Contacts ");
    }

    numFree--;

    _PotentialContact* contact = freeList;
    freeList = contact->next;

    if (freeList != nullptr)
    {
        freeList->prev = nullptr;
    }

    contact->init();
    contact->object = object;
    contact->contactType = static_cast<int8_t>(type);
    contact->visibility = visibility;
    contact->prev = nullptr;
    contact->next = contactList[type];

    if (contactList[type] != nullptr)
    {
        contactList[type]->prev = contact;
    }

    contactList[type] = contact;
    return contact;
}

auto PotentialContactManager::getContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly) -> int32_t
{
    // Per team, the lists to count: the enemy's first.
    static constexpr int8_t listOrder[3][3] = {{1, 0, 2}, {0, 1, 2}, {1, 0, 2}};

    counts[0] = 0;
    counts[1] = 0;
    counts[2] = 0;
    const int8_t* order = listOrder[teamId];
    const int32_t numLists = enemiesOnly ? 1 : 3;

    for (int32_t list = 0; list < numLists; list++)
    {
        for (_PotentialContact* contact = contactList[order[list]]; contact != nullptr; contact = contact->next)
        {
            int tagged = 0;
            const int32_t contactType = contact->object->getContactType(teamId, tagged);

            if (tagged != 0)
            {
                counts[0]++;
            }
            else if (contactType == CONTACT_SENSOR)
            {
                counts[2]++;
            }
            else if (contactType == CONTACT_VISUAL)
            {
                counts[1]++;
            }
        }
    }

    return 0;
}

auto PotentialContactManager::remove(_PotentialContact* contact) -> void
{
    if (contact == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        if (contact->sensorSlot[i] != 0xff)
        {
            sensorSystemManager->sensors[i]->removeSensorContact(contact);
        }
    }

    for (int32_t i = 0; i < 3; i++)
    {
        if (contact->teamSlot[i] == -1)
        {
            continue;
        }

        if (contact->contactStatus[i] == CONTACT_VISUAL)
        {
            TeamTable[i]->removeLOSContact(contact);
        }
        else if (contact->contactStatus[i] == CONTACT_SENSOR)
        {
            TeamTable[i]->removeSensorContact(contact);
        }
    }

    if (contact->prev == nullptr)
    {
        contactList[contact->contactType] = contact->next;
    }
    else
    {
        contact->prev->next = contact->next;
    }

    if (contact->next != nullptr)
    {
        contact->next->prev = contact->prev;
    }

    contact->object = nullptr;
    contact->prev = nullptr;
    contact->next = freeList;
    freeList = contact;
    numFree++;
}

auto PotentialContactManager::move(_PotentialContact* contact, int32_t type, char visibility) -> void
{
    if (contact == nullptr)
    {
        return;
    }

    if (contact->prev == nullptr)
    {
        contactList[contact->contactType] = contact->next;
    }
    else
    {
        contact->prev->next = contact->next;
    }

    if (contact->next != nullptr)
    {
        contact->next->prev = contact->prev;
    }

    contact->visibility = visibility;
    contact->contactType = static_cast<int8_t>(type);
    contact->prev = nullptr;
    contact->next = contactList[type];

    if (contactList[type] != nullptr)
    {
        contactList[type]->prev = contact;
    }

    contactList[type] = contact;
}

auto PotentialContactManager::updateStatus() -> void
{
    for (int32_t list = 0; list < 3; list++)
    {
        for (_PotentialContact* contact = contactList[list]; contact != nullptr; contact = contact->next)
        {
            const Team* team = contact->object->getTeam();

            if (team == clanTeam)
            {
                contact->updateStatus(innerSphereTeam);
                contact->updateStatus(alliedTeam);
            }
            else if (team == innerSphereTeam)
            {
                contact->updateStatus(clanTeam);
                contact->updateStatus(alliedTeam);
            }
            else
            {
                contact->updateStatus(innerSphereTeam);
                contact->updateStatus(clanTeam);
            }
        }
    }
}

auto PotentialContactManager::destroy() -> void
{
    ObjectTypeManager::objectCache->free(contacts);
    contacts = nullptr;
    maxContacts = 0;
    numFree = 0;
    freeList = nullptr;
}

//---------------------------------------------------------------------------
// SensorSystem
//---------------------------------------------------------------------------

auto SensorSystem::operator new(size_t size) noexcept -> void*
{
    return ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));
}

auto SensorSystem::operator delete(void* ptr) -> void
{
    ObjectTypeManager::objectCache->free(ptr);
}

auto SensorSystem::init() -> void
{
    teamIndex = -1;
    teamSensorSlot = -1;
    owner = nullptr;
    team = nullptr;
    range = -1.0f;
    teamMultiplier[0] = 1.0f;
    teamMultiplier[1] = 1.0f;
    teamMultiplier[2] = 1.0f;
    multiplier = 1.0f;
    lastScanTime = 0.0f;
    id = numSensors;
    nextScanTime = static_cast<float>(numSensors * 0.1 + 0.25);
    numSensors++;
    scanFrequency = ContactUpdateFrequency;
    lastMultiplierTime = 0.0f;
    numContacts = 0;
    totalContacts = 0;

    if (sortList == nullptr)
    {
        sortList = new SortList;

        if (sortList == nullptr)
        {
            Fatal(0, " Unable to create Contact::sortList ");
        }

        sortList->init(MAX_SENSOR_CONTACTS);
    }
}

auto SensorSystem::destroy() -> void
{
    numSensors--;

    if (numSensors == 0)
    {
        if (sortList != nullptr)
        {
            sortList->destroy();
            delete sortList;
        }

        sortList = nullptr;
    }
}

auto SensorSystem::setRange(float newRange) -> void
{
    range = newRange;
    speedRange[2] = newRange;
    speedRange[1] = newRange;
    speedRange[0] = newRange;
    currentRange = newRange;
    rangeChangeTurn = -1;

    if (owner == nullptr || !IsMover(owner) || owner->getPilot() == nullptr)
    {
        return;
    }

    // The pilot's sensor skill picks the row: the lower the skill, the more moving and running cut the range.
    float moveFactor = 1.0f;
    float runFactor = 1.0f;
    const MechWarrior* pilot = owner->getPilot();

    for (int32_t row = 0; row < 4; row++)
    {
        if (static_cast<float>(pilot->skills[2]) <= static_cast<float>(SensorSkillMoveRange[row]))
        {
            moveFactor = SensorSkillMoveFactor[row][0];
            runFactor = SensorSkillMoveFactor[row][1];
            break;
        }
    }

    speedRange[0] = newRange;
    speedRange[1] = moveFactor * newRange;
    speedRange[2] = runFactor * newRange;
}

auto SensorSystem::getSkilledRange() -> float
{
    if (!IsMover(owner))
    {
        return multiplier * range;
    }

    const int32_t speedState = static_cast<Mover*>(owner)->getSpeedState();
    const int32_t now = turn;
    const float newRange = speedRange[speedState] * multiplier;

    if (newRange != currentRange && rangeChangeTurn == -1)
    {
        rangeChangeTurn = turn + 6;
    }

    if (rangeChangeTurn == -1)
    {
        return newRange;
    }

    if (rangeChangeTurn <= now)
    {
        currentRange = newRange;
        rangeChangeTurn = -1;
        return newRange;
    }

    // Ease toward the new range over the turns left.
    return currentRange - (currentRange - newRange) * (1.0f / static_cast<float>(rangeChangeTurn - now));
}

auto SensorSystem::setTeam(Team* newTeam) -> void
{
    clearSensorContacts();

    if (team != nullptr)
    {
        team->removeSensor(this);
        teamSensorSlot = -1;
        teamIndex = -1;
    }

    team = newTeam;

    if (newTeam == nullptr)
    {
        return;
    }

    newTeam->addSensor(this);

    if (team == innerSphereTeam)
    {
        teamIndex = 0;
    }
    else if (team == clanTeam)
    {
        teamIndex = 1;
    }
    else if (team == alliedTeam)
    {
        teamIndex = 2;
    }
    else
    {
        Fatal(0, " SensorSystem: Bad Team ");
    }
}

auto SensorSystem::enabled() -> int
{
    if (teamSensorSlot < 0 || owner->getExistsAndAwake() == 0)
    {
        return 0;
    }

    if (!IsMover(owner))
    {
        return 1;
    }

    const Mover* mover = static_cast<const Mover*>(owner);

    if (mover->status != 5 && mover->sensor != 0xff && mover->inventory[mover->sensor].disabled == 0)
    {
        return 1;
    }

    return 0;
}

auto SensorSystem::disable() -> void
{
    clearSensorContacts();
    setTeam(nullptr);
}

auto SensorSystem::calcTeamEffect(Team* team) -> float
{
    const float jammerEffect = team->getJammerEffect();
    const float ecmEffect = team->getECMEffect(owner->getPosition());
    return jammerEffect < ecmEffect ? jammerEffect : ecmEffect;
}

auto SensorSystem::calcTeamMultipliers() -> void
{
    if (scenarioTime == lastMultiplierTime)
    {
        return;
    }

    lastMultiplierTime = scenarioTime;
    teamMultiplier[0] = 1.0f;
    teamMultiplier[1] = 1.0f;
    teamMultiplier[2] = 1.0f;

    const int32_t alignment = owner->getAlignment();

    if (alignment == -1)
    {
        // Unaligned: the Inner Sphere and allied effects both count, the stronger one for each slot it fills.
        float innerSphereEffect = 1.0f;

        if (innerSphereTeam != nullptr)
        {
            innerSphereEffect = calcTeamEffect(innerSphereTeam);
        }

        float effect = 1.0f;

        if (alliedTeam != nullptr)
        {
            effect = calcTeamEffect(alliedTeam);
        }

        if (innerSphereEffect < effect)
        {
            effect = innerSphereEffect;
        }

        teamMultiplier[0] = effect;
        teamMultiplier[2] = effect;
    }
    else if (alignment == 0)
    {
        if (innerSphereTeam != nullptr)
        {
            teamMultiplier[0] = calcTeamEffect(innerSphereTeam);
        }

        if (clanTeam != nullptr)
        {
            teamMultiplier[1] = calcTeamEffect(clanTeam);
        }

        if (alliedTeam != nullptr)
        {
            teamMultiplier[2] = calcTeamEffect(alliedTeam);
        }
    }
    else if (alignment == 1 && clanTeam != nullptr)
    {
        teamMultiplier[1] = calcTeamEffect(clanTeam);
    }

    multiplier = team != clanTeam ? teamMultiplier[1] : teamMultiplier[0];
}

auto SensorSystem::addSensorContact(_PotentialContact* contact) -> void
{
    if (numContacts >= MAX_SENSOR_CONTACTS || contact->sensorSlot[id] != 0xff)
    {
        return;
    }

    contacts[numContacts] = contact->id;
    contact->sensorSlot[id] = static_cast<uint8_t>(numContacts);
    contact->numSensors[teamIndex]++;
    numContacts++;
}

auto SensorSystem::removeSensorContact(int32_t index) -> void
{
    _PotentialContact* pool = potentialContactManager->contacts;
    _PotentialContact& contact = pool[contacts[index]];
    contact.sensorSlot[id] = 0xff;
    contact.numSensors[teamIndex]--;
    numContacts--;

    if (numContacts > 0 && index != numContacts)
    {
        contacts[index] = contacts[numContacts];
        pool[contacts[numContacts]].sensorSlot[id] = static_cast<uint8_t>(index);
    }
}

auto SensorSystem::removeSensorContact(_PotentialContact* contact) -> void
{
    if (contact->sensorSlot[id] < 0xff)
    {
        removeSensorContact(static_cast<int32_t>(contact->sensorSlot[id]));
    }
}

auto SensorSystem::clearSensorContacts() -> void
{
    while (numContacts != 0)
    {
        removeSensorContact(0);
    }
}

auto SensorSystem::updateContacts() -> void
{
    if (teamSensorSlot == -1 || range == -1.0f)
    {
        return;
    }

    if (owner->getAwake() == 0 || owner->getExists() == 0 || enabled() == 0)
    {
        clearSensorContacts();
        return;
    }

    if (scenarioTime == lastScanTime)
    {
        return;
    }

    calcTeamMultipliers();

    if (numContacts < 1)
    {
        return;
    }

    int32_t index = 0;

    do
    {
        GameObject* target = potentialContactManager->contacts[contacts[index]].object;

        if (target->isDisabled() == 0 && onSensors(target) != 0)
        {
            index++;
        }
        else
        {
            removeSensorContact(index);
        }
    } while (index < numContacts);
}

auto SensorSystem::updateScan(int forceScan) -> void
{
    if (forceScan == 0 && (teamSensorSlot == -1 || range == -1.0f || turn <= 1))
    {
        return;
    }

    if (owner->getAwake() == 0 || owner->getExists() == 0 || enabled() == 0)
    {
        clearSensorContacts();
        return;
    }

    if (scenarioTime <= nextScanTime && forceScan == 0)
    {
        return;
    }

    owner->getAlignment();
    calcTeamMultipliers();

    if (scanBattlefield() > 0 && IsMover(owner))
    {
        owner->getPilot()->radioMessage(RADIO_SENSOR_CONTACT, 0);
    }

    lastScanTime = scenarioTime;

    if (forceScan != 0)
    {
        return;
    }

    nextScanTime = scanFrequency + nextScanTime;
}

auto SensorSystem::scanBattlefield(PotentialContactType type) -> int32_t
{
    if (team == nullptr)
    {
        Fatal(0, " Sensor Owner has no team ");
    }

    if (teamSensorSlot == -1 || range == -1.0f)
    {
        return 0;
    }

    int32_t newContacts = 0;
    _PotentialContact* contact = potentialContactManager->contactList[type];
    const Team* ownerTeam = owner->getTeam();

    for (; contact != nullptr; contact = contact->next)
    {
        GameObject* target = contact->object;

        if (target->getTeam() == ownerTeam)
        {
            continue;
        }

        if (onSensors(target) == 0)
        {
            removeSensorContact(contact);
        }
        else
        {
            if (contact->contactStatus[teamIndex] == CONTACT_NONE)
            {
                newContacts++;
            }

            addSensorContact(contact);
        }
    }

    totalContacts += newContacts;
    return newContacts;
}

auto SensorSystem::scanBattlefield() -> int32_t
{
    switch (owner->getAlignment())
    {
        case -1:
        {
            const int32_t innerSphere = scanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t allied = scanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return innerSphere + allied;
        }

        case 0:
        {
            const int32_t innerSphere = scanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t clan = scanBattlefield(POTENTIAL_CONTACT_CLAN);
            const int32_t allied = scanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return innerSphere + clan + allied;
        }

        case 1:
        {
            // The Inner Sphere list is scanned, but its new contacts aren't counted.
            scanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t clan = scanBattlefield(POTENTIAL_CONTACT_CLAN);
            const int32_t allied = scanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return clan + allied;
        }

        default:
            return 0;
    }
}

auto SensorSystem::onSensors(GameObject* target) -> int
{
    if (teamSensorSlot == -1)
    {
        return 0;
    }

    if (range <= 0.0f)
    {
        return 0;
    }

    if (SensorAutomaticSuccess != 0)
    {
        return 1;
    }

    const _PotentialContact* contact = target->getPotentialContact();

    if (contact != nullptr)
    {
        if (contact->visibility == 2)
        {
            return 0;
        }

        if (contact->visibility == 3 && team->lineOfSight(target->getPosition()) != 0)
        {
            return 0;
        }
    }

    float distance;
    float skilledRange;

    if (IsMover(owner))
    {
        const Mover* mover = static_cast<const Mover*>(owner);

        if (mover->sensor == 0xff)
        {
            return 0;
        }

        if (mover->inventory[mover->sensor].disabled != 0)
        {
            return 0;
        }

        vector_3d targetPosition = target->getPosition();
        distance = owner->distanceFrom(targetPosition);
        skilledRange = getSkilledRange();
        // A working probe reaches hidden targets within its share of the range.
        float probeRange = -1.0f;

        if (mover->probe != 0xff && mover->inventory[mover->probe].disabled == 0)
        {
            probeRange = MasterComponentList[mover->inventory[mover->probe].masterID].rangeOrHeat * skilledRange;
        }

        if (distance <= probeRange)
        {
            return skilledRange < distance ? 0 : 1;
        }
    }
    else
    {
        vector_3d targetPosition = target->getPosition();
        distance = owner->distanceFrom(targetPosition);
        skilledRange = getSkilledRange();
    }

    if (target->status == 5)
    {
        return 0;
    }

    return skilledRange < distance ? 0 : 1;
}

//---------------------------------------------------------------------------
// SensorSystemManager
//---------------------------------------------------------------------------

auto SensorSystemManager::operator new(size_t size) noexcept -> void*
{
    return ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));
}

auto SensorSystemManager::operator delete(void* ptr) -> void
{
    ObjectTypeManager::objectCache->free(ptr);
}

auto SensorSystemManager::init(FitIniFile*) -> int32_t
{
    // Port fix: sized by the port's pointer (0x104 bytes in the original).
    sensors = static_cast<SensorSystem**>(
        ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(MAX_SENSORS * sizeof(SensorSystem*))));

    if (sensors == nullptr)
    {
        Fatal(0, " No RAM For Sensor System Manager ");
    }

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        SensorSystem* sensor = new SensorSystem;

        if (sensor != nullptr)
        {
            sensor->init();
        }

        sensors[i] = sensor;
    }

    sensors[0]->id = 0;
    sensors[0]->prev = nullptr;
    sensors[0]->next = sensors[1];

    for (int32_t i = 1; i < MAX_SENSORS - 1; i++)
    {
        sensors[i]->id = i;
        sensors[i]->prev = sensors[i - 1];
        sensors[i]->next = sensors[i + 1];
    }

    sensors[MAX_SENSORS - 1]->id = MAX_SENSORS - 1;
    sensors[MAX_SENSORS - 1]->prev = sensors[MAX_SENSORS - 2];
    sensors[MAX_SENSORS - 1]->next = nullptr;

    freeList = sensors[0];
    numFree = MAX_SENSORS;
    return 0;
}

auto SensorSystemManager::newSensor() -> SensorSystem*
{
    if (numFree == 0)
    {
        Fatal(0, " No More Free Sensors ");
    }

    SensorSystem* sensor = freeList;
    numFree--;
    freeList = sensor->next;

    if (freeList != nullptr)
    {
        freeList->prev = nullptr;
    }

    sensor->next = nullptr;
    return sensor;
}

auto SensorSystemManager::freeSensor(SensorSystem* sensor) -> void
{
    SensorSystem* oldFirst = freeList;
    numFree++;
    freeList = sensor;
    sensor->prev = nullptr;
    sensor->next = oldFirst;

    // Port fix: the original wrote through a null free list when every sensor was taken.
    if (oldFirst != nullptr)
    {
        oldFirst->prev = sensor;
    }
}

auto SensorSystemManager::destroy() -> void
{
    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        // Clears duplicates from i on, i itself included, so the delete below never runs (see the header).
        for (int32_t j = i; j < MAX_SENSORS; j++)
        {
            if (sensors[i] == sensors[j])
            {
                sensors[j] = nullptr;
            }
        }

        if (sensors[i] != nullptr)
        {
            sensors[i]->destroy();
            delete sensors[i];
        }

        sensors[i] = nullptr;
    }

    ObjectTypeManager::objectCache->free(sensors);
    sensors = nullptr;
    numFree = 0;
    freeList = nullptr;
}
