#include "stdafx.h"
#include "object/contact.h"
#include "lib/aerror.h"
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

MCPotentialContactManager* PotentialContactManager = nullptr;
MCSensorSystemManager* SensorSystemManager = nullptr;
int32_t MCSensorSystem::NumSensors = 0;
MCSortList* MCSensorSystem::SortList = nullptr;
int SensorAutomaticSuccess = 0;
char SensorSkillMoveRange[4] = {45, 59, 69, 80};
float SensorSkillMoveFactor[4][2] = {{0.8f, 0.5f}, {0.85f, 0.55f}, {0.9f, 0.6f}, {0.95f, 0.65f}};
float SensorModifier[8] = {40.0f, 0.5f, 1.0f, 100.0f, -50.0f, 0.0f, -30.0f, -40.0f};

namespace
{
    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or plain mover).</summary>
    bool IsMover(const MCGameObject* object)
    {
        const MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }
}

//---------------------------------------------------------------------------
// _PotentialContact
//---------------------------------------------------------------------------

auto MCPotentialContact::Init() -> void
{
    Object = nullptr;
    ContactType = POTENTIAL_CONTACT_ALLIED;
    Visibility = 0;

    for (int32_t i = 0; i < 3; i++)
    {
        ContactStatus[i] = CONTACT_NONE;
        NumSensors[i] = 0;
        LostVisual[i] = 0;
        TeamSlot[i] = -1;
    }

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        SensorSlot[i] = 0xff;
    }

    Next = nullptr;
    Prev = nullptr;
}

auto MCPotentialContact::UpdateStatus(MCTeam* team) -> void
{
    if (team == nullptr)
    {
        return;
    }

    const int32_t teamId = team->Id;
    const uint8_t oldStatus = ContactStatus[teamId];
    uint8_t newStatus = CONTACT_NONE;

    if (team->LineOfSight(Object->GetPosition()) == 0)
    {
        if (NumSensors[teamId] != 0)
        {
            newStatus = CONTACT_SENSOR;
        }
    }
    else if (Visibility != 3)
    {
        newStatus = CONTACT_VISUAL;
    }

    if (oldStatus == newStatus)
    {
        return;
    }

    ContactStatus[teamId] = newStatus;

    if (oldStatus == CONTACT_VISUAL)
    {
        team->RemoveLosContact(this);
    }
    else if (oldStatus == CONTACT_SENSOR)
    {
        LostVisual[teamId] = 0;
        team->RemoveSensorContact(this);
    }

    if (newStatus == CONTACT_VISUAL)
    {
        team->AddLosContact(this);
        return;
    }

    if (newStatus == CONTACT_SENSOR)
    {
        if (oldStatus == CONTACT_VISUAL)
        {
            LostVisual[teamId] = 1;
        }

        team->AddSensorContact(this);
    }
}

//---------------------------------------------------------------------------
// PotentialContactManager
//---------------------------------------------------------------------------

auto MCPotentialContactManager::Init(MCFitIniFile* file) -> int32_t
{
    MaxContacts = 0;
    NumFree = 0;
    ContactList[0] = nullptr;
    ContactList[1] = nullptr;
    ContactList[2] = nullptr;
    Contacts = nullptr;
    FreeList = nullptr;

    int32_t result = file->SeekBlock("PotentialContactManager");

    if (result != 0)
    {
        return result;
    }

    result = file->ReadIdLong("MaxPotentialContacts", MaxContacts);

    if (result != 0)
    {
        return result;
    }

    if (MaxContacts < 2)
    {
        Fatal(0, " Way too few contacts in Potential Contact Manager! ");
    }

    // Port fix: sized by the port's struct (0x60 bytes in the original).
    Contacts = std::make_unique<MCPotentialContact[]>(static_cast<size_t>(MaxContacts));

    // Chain the pool into the free list; the rest of each contact is set when add() takes it.
    Contacts[0].Id = 0;
    Contacts[0].Object = nullptr;
    Contacts[0].Prev = nullptr;
    Contacts[0].Next = &Contacts[1];

    for (int32_t i = 1; i < MaxContacts - 1; i++)
    {
        Contacts[i].Id = static_cast<uint16_t>(i);
        Contacts[i].Object = nullptr;
        Contacts[i].Prev = &Contacts[i - 1];
        Contacts[i].Next = &Contacts[i + 1];
    }

    MCPotentialContact& last = Contacts[MaxContacts - 1];
    last.Id = static_cast<uint16_t>(MaxContacts - 1);
    last.Object = nullptr;
    last.Prev = &Contacts[MaxContacts - 2];
    last.Next = nullptr;

    FreeList = Contacts.get();
    NumFree = MaxContacts;
    return 0;
}

auto MCPotentialContactManager::Add(int32_t type, MCBigGameObject* object, char visibility) -> MCPotentialContact*
{
    if (NumFree == 0)
    {
        Fatal(0, " No More Free Potential Contacts ");
    }

    NumFree--;

    MCPotentialContact* contact = FreeList;
    FreeList = contact->Next;

    if (FreeList != nullptr)
    {
        FreeList->Prev = nullptr;
    }

    contact->Init();
    contact->Object = object;
    contact->ContactType = static_cast<int8_t>(type);
    contact->Visibility = visibility;
    contact->Prev = nullptr;
    contact->Next = ContactList[type];

    if (ContactList[type] != nullptr)
    {
        ContactList[type]->Prev = contact;
    }

    ContactList[type] = contact;
    return contact;
}

auto MCPotentialContactManager::GetContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly) -> int32_t
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
        for (MCPotentialContact* contact = ContactList[order[list]]; contact != nullptr; contact = contact->Next)
        {
            int tagged = 0;
            const int32_t contactType = contact->Object->GetContactType(teamId, tagged);

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

auto MCPotentialContactManager::Remove(MCPotentialContact* contact) -> void
{
    if (contact == nullptr)
    {
        return;
    }

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        if (contact->SensorSlot[i] != 0xff)
        {
            SensorSystemManager->Sensors[i]->RemoveSensorContact(contact);
        }
    }

    for (int32_t i = 0; i < 3; i++)
    {
        if (contact->TeamSlot[i] == -1)
        {
            continue;
        }

        if (contact->ContactStatus[i] == CONTACT_VISUAL)
        {
            TeamTable[i]->RemoveLosContact(contact);
        }
        else if (contact->ContactStatus[i] == CONTACT_SENSOR)
        {
            TeamTable[i]->RemoveSensorContact(contact);
        }
    }

    if (contact->Prev == nullptr)
    {
        ContactList[contact->ContactType] = contact->Next;
    }
    else
    {
        contact->Prev->Next = contact->Next;
    }

    if (contact->Next != nullptr)
    {
        contact->Next->Prev = contact->Prev;
    }

    contact->Object = nullptr;
    contact->Prev = nullptr;
    contact->Next = FreeList;
    FreeList = contact;
    NumFree++;
}

auto MCPotentialContactManager::Move(MCPotentialContact* contact, int32_t type, char visibility) -> void
{
    if (contact == nullptr)
    {
        return;
    }

    if (contact->Prev == nullptr)
    {
        ContactList[contact->ContactType] = contact->Next;
    }
    else
    {
        contact->Prev->Next = contact->Next;
    }

    if (contact->Next != nullptr)
    {
        contact->Next->Prev = contact->Prev;
    }

    contact->Visibility = visibility;
    contact->ContactType = static_cast<int8_t>(type);
    contact->Prev = nullptr;
    contact->Next = ContactList[type];

    if (ContactList[type] != nullptr)
    {
        ContactList[type]->Prev = contact;
    }

    ContactList[type] = contact;
}

auto MCPotentialContactManager::UpdateStatus() -> void
{
    for (int32_t list = 0; list < 3; list++)
    {
        for (MCPotentialContact* contact = ContactList[list]; contact != nullptr; contact = contact->Next)
        {
            const MCTeam* team = contact->Object->GetTeam();

            if (team == ClanTeam)
            {
                contact->UpdateStatus(InnerSphereTeam);
                contact->UpdateStatus(AlliedTeam);
            }
            else if (team == InnerSphereTeam)
            {
                contact->UpdateStatus(ClanTeam);
                contact->UpdateStatus(AlliedTeam);
            }
            else
            {
                contact->UpdateStatus(InnerSphereTeam);
                contact->UpdateStatus(ClanTeam);
            }
        }
    }
}

auto MCPotentialContactManager::Destroy() -> void
{
    Contacts.reset();
    MaxContacts = 0;
    NumFree = 0;
    FreeList = nullptr;
}

//---------------------------------------------------------------------------
// SensorSystem
//---------------------------------------------------------------------------

auto MCSensorSystem::Init() -> void
{
    TeamIndex = -1;
    TeamSensorSlot = -1;
    Owner = nullptr;
    Team = nullptr;
    Range = -1.0f;
    TeamMultiplier[0] = 1.0f;
    TeamMultiplier[1] = 1.0f;
    TeamMultiplier[2] = 1.0f;
    Multiplier = 1.0f;
    LastScanTime = 0.0f;
    Id = NumSensors;
    NextScanTime = static_cast<float>(NumSensors * 0.1 + 0.25);
    NumSensors++;
    ScanFrequency = ContactUpdateFrequency;
    LastMultiplierTime = 0.0f;
    NumContacts = 0;
    TotalContacts = 0;

    if (SortList == nullptr)
    {
        SortList = new MCSortList;

        if (SortList == nullptr)
        {
            Fatal(0, " Unable to create Contact::sortList ");
        }

        SortList->Init(MAX_SENSOR_CONTACTS);
    }
}

auto MCSensorSystem::Destroy() -> void
{
    NumSensors--;

    if (NumSensors == 0)
    {
        if (SortList != nullptr)
        {
            SortList->Destroy();
            delete SortList;
        }

        SortList = nullptr;
    }
}

auto MCSensorSystem::SetRange(float newRange) -> void
{
    Range = newRange;
    SpeedRange[2] = newRange;
    SpeedRange[1] = newRange;
    SpeedRange[0] = newRange;
    CurrentRange = newRange;
    RangeChangeTurn = -1;

    if (Owner == nullptr || !IsMover(Owner) || Owner->GetPilot() == nullptr)
    {
        return;
    }

    // The pilot's sensor skill picks the row: the lower the skill, the more moving and running cut the range.
    float moveFactor = 1.0f;
    float runFactor = 1.0f;
    const MCMechWarrior* pilot = Owner->GetPilot();

    for (int32_t row = 0; row < 4; row++)
    {
        if (static_cast<float>(pilot->Skills[2]) <= static_cast<float>(SensorSkillMoveRange[row]))
        {
            moveFactor = SensorSkillMoveFactor[row][0];
            runFactor = SensorSkillMoveFactor[row][1];
            break;
        }
    }

    SpeedRange[0] = newRange;
    SpeedRange[1] = moveFactor * newRange;
    SpeedRange[2] = runFactor * newRange;
}

namespace
{
    double SkilledRangeUnrounded(MCSensorSystem& sensor)
    {
        if (!IsMover(sensor.Owner))
        {
            return static_cast<double>(sensor.Multiplier) * sensor.Range;
        }

        const int32_t speedState = static_cast<MCMover*>(sensor.Owner)->GetSpeedState();
        const int32_t now = Turn;
        const double newRange = static_cast<double>(sensor.SpeedRange[speedState]) * sensor.Multiplier;

        if (newRange != sensor.CurrentRange && sensor.RangeChangeTurn == -1)
        {
            sensor.RangeChangeTurn = Turn + 6;
        }

        if (sensor.RangeChangeTurn == -1)
        {
            return newRange;
        }

        if (sensor.RangeChangeTurn <= now)
        {
            sensor.CurrentRange = static_cast<float>(newRange);
            sensor.RangeChangeTurn = -1;
            return newRange;
        }

        return sensor.CurrentRange -
               (sensor.CurrentRange - newRange) * (1.0 / static_cast<double>(sensor.RangeChangeTurn - now));
    }
}

auto MCSensorSystem::GetSkilledRange() -> float
{
    // Ease toward the new range over the turns left.
    return static_cast<float>(SkilledRangeUnrounded(*this));
}

auto MCSensorSystem::SetTeam(MCTeam* newTeam) -> void
{
    ClearSensorContacts();

    if (Team != nullptr)
    {
        Team->RemoveSensor(this);
        TeamSensorSlot = -1;
        TeamIndex = -1;
    }

    Team = newTeam;

    if (newTeam == nullptr)
    {
        return;
    }

    newTeam->AddSensor(this);

    if (Team == InnerSphereTeam)
    {
        TeamIndex = 0;
    }
    else if (Team == ClanTeam)
    {
        TeamIndex = 1;
    }
    else if (Team == AlliedTeam)
    {
        TeamIndex = 2;
    }
    else
    {
        Fatal(0, " SensorSystem: Bad Team ");
    }
}

auto MCSensorSystem::Enabled() -> int
{
    if (TeamSensorSlot < 0 || Owner->GetExistsAndAwake() == 0)
    {
        return 0;
    }

    if (!IsMover(Owner))
    {
        return 1;
    }

    const MCMover* mover = static_cast<const MCMover*>(Owner);

    if (mover->Status != 5 && mover->Sensor != 0xff && mover->Inventory[mover->Sensor].Disabled == 0)
    {
        return 1;
    }

    return 0;
}

auto MCSensorSystem::Disable() -> void
{
    ClearSensorContacts();
    SetTeam(nullptr);
}

auto MCSensorSystem::CalcTeamEffect(MCTeam* team) -> float
{
    const float jammerEffect = team->GetJammerEffect();
    const float ecmEffect = team->GetEcmEffect(Owner->GetPosition());
    return jammerEffect < ecmEffect ? jammerEffect : ecmEffect;
}

auto MCSensorSystem::CalcTeamMultipliers() -> void
{
    if (ScenarioTime == LastMultiplierTime)
    {
        return;
    }

    LastMultiplierTime = ScenarioTime;
    TeamMultiplier[0] = 1.0f;
    TeamMultiplier[1] = 1.0f;
    TeamMultiplier[2] = 1.0f;

    const int32_t alignment = Owner->GetAlignment();

    if (alignment == -1)
    {
        // Unaligned: the Inner Sphere and allied effects both count, the stronger one for each slot it fills.
        float innerSphereEffect = 1.0f;

        if (InnerSphereTeam != nullptr)
        {
            innerSphereEffect = CalcTeamEffect(InnerSphereTeam);
        }

        float effect = 1.0f;

        if (AlliedTeam != nullptr)
        {
            effect = CalcTeamEffect(AlliedTeam);
        }

        if (innerSphereEffect < effect)
        {
            effect = innerSphereEffect;
        }

        TeamMultiplier[0] = effect;
        TeamMultiplier[2] = effect;
    }
    else if (alignment == 0)
    {
        if (InnerSphereTeam != nullptr)
        {
            TeamMultiplier[0] = CalcTeamEffect(InnerSphereTeam);
        }

        if (ClanTeam != nullptr)
        {
            TeamMultiplier[1] = CalcTeamEffect(ClanTeam);
        }

        if (AlliedTeam != nullptr)
        {
            TeamMultiplier[2] = CalcTeamEffect(AlliedTeam);
        }
    }
    else if (alignment == 1 && ClanTeam != nullptr)
    {
        TeamMultiplier[1] = CalcTeamEffect(ClanTeam);
    }

    Multiplier = Team != ClanTeam ? TeamMultiplier[1] : TeamMultiplier[0];
}

auto MCSensorSystem::AddSensorContact(MCPotentialContact* contact) -> void
{
    if (NumContacts >= MAX_SENSOR_CONTACTS || contact->SensorSlot[Id] != 0xff)
    {
        return;
    }

    Contacts[NumContacts] = contact->Id;
    contact->SensorSlot[Id] = static_cast<uint8_t>(NumContacts);
    contact->NumSensors[TeamIndex]++;
    NumContacts++;
}

auto MCSensorSystem::RemoveSensorContact(int32_t index) -> void
{
    MCPotentialContact* pool = PotentialContactManager->Contacts.get();
    MCPotentialContact& contact = pool[Contacts[index]];
    contact.SensorSlot[Id] = 0xff;
    contact.NumSensors[TeamIndex]--;
    NumContacts--;

    if (NumContacts > 0 && index != NumContacts)
    {
        Contacts[index] = Contacts[NumContacts];
        pool[Contacts[NumContacts]].SensorSlot[Id] = static_cast<uint8_t>(index);
    }
}

auto MCSensorSystem::RemoveSensorContact(MCPotentialContact* contact) -> void
{
    if (contact->SensorSlot[Id] < 0xff)
    {
        RemoveSensorContact(static_cast<int32_t>(contact->SensorSlot[Id]));
    }
}

auto MCSensorSystem::ClearSensorContacts() -> void
{
    while (NumContacts != 0)
    {
        RemoveSensorContact(0);
    }
}

auto MCSensorSystem::UpdateContacts() -> void
{
    if (TeamSensorSlot == -1 || Range == -1.0f)
    {
        return;
    }

    if (Owner->GetAwake() == 0 || Owner->GetExists() == 0 || Enabled() == 0)
    {
        ClearSensorContacts();
        return;
    }

    if (ScenarioTime == LastScanTime)
    {
        return;
    }

    CalcTeamMultipliers();

    if (NumContacts < 1)
    {
        return;
    }

    int32_t index = 0;

    do
    {
        MCGameObject* target = PotentialContactManager->Contacts[Contacts[index]].Object;

        if (target->IsDisabled() == 0 && OnSensors(target) != 0)
        {
            index++;
        }
        else
        {
            RemoveSensorContact(index);
        }
    } while (index < NumContacts);
}

auto MCSensorSystem::UpdateScan(int forceScan) -> void
{
    if (forceScan == 0 && (TeamSensorSlot == -1 || Range == -1.0f || Turn <= 1))
    {
        return;
    }

    if (Owner->GetAwake() == 0 || Owner->GetExists() == 0 || Enabled() == 0)
    {
        ClearSensorContacts();
        return;
    }

    if (ScenarioTime <= NextScanTime && forceScan == 0)
    {
        return;
    }

    Owner->GetAlignment();
    CalcTeamMultipliers();

    if (ScanBattlefield() > 0 && IsMover(Owner))
    {
        Owner->GetPilot()->RadioMessage(RADIO_SENSOR_CONTACT, 0);
    }

    LastScanTime = ScenarioTime;

    if (forceScan != 0)
    {
        return;
    }

    NextScanTime = ScanFrequency + NextScanTime;
}

auto MCSensorSystem::ScanBattlefield(MCPotentialContactType type) -> int32_t
{
    if (Team == nullptr)
    {
        Fatal(0, " Sensor Owner has no team ");
    }

    if (TeamSensorSlot == -1 || Range == -1.0f)
    {
        return 0;
    }

    int32_t newContacts = 0;
    MCPotentialContact* contact = PotentialContactManager->ContactList[type];
    const MCTeam* ownerTeam = Owner->GetTeam();

    for (; contact != nullptr; contact = contact->Next)
    {
        MCGameObject* target = contact->Object;

        if (target->GetTeam() == ownerTeam)
        {
            continue;
        }

        if (OnSensors(target) == 0)
        {
            RemoveSensorContact(contact);
        }
        else
        {
            if (contact->ContactStatus[TeamIndex] == CONTACT_NONE)
            {
                newContacts++;
            }

            AddSensorContact(contact);
        }
    }

    TotalContacts += newContacts;
    return newContacts;
}

auto MCSensorSystem::ScanBattlefield() -> int32_t
{
    switch (Owner->GetAlignment())
    {
        case -1:
        {
            const int32_t innerSphere = ScanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t allied = ScanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return innerSphere + allied;
        }

        case 0:
        {
            const int32_t innerSphere = ScanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t clan = ScanBattlefield(POTENTIAL_CONTACT_CLAN);
            const int32_t allied = ScanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return innerSphere + clan + allied;
        }

        case 1:
        {
            // The Inner Sphere list is scanned, but its new contacts aren't counted.
            ScanBattlefield(POTENTIAL_CONTACT_INNER_SPHERE);
            const int32_t clan = ScanBattlefield(POTENTIAL_CONTACT_CLAN);
            const int32_t allied = ScanBattlefield(POTENTIAL_CONTACT_ALLIED);
            return clan + allied;
        }

        default:
            return 0;
    }
}

auto MCSensorSystem::OnSensors(MCGameObject* target) -> int
{
    if (TeamSensorSlot == -1)
    {
        return 0;
    }

    if (Range <= 0.0f)
    {
        return 0;
    }

    if (SensorAutomaticSuccess != 0)
    {
        return 1;
    }

    const MCPotentialContact* contact = target->GetPotentialContact();

    if (contact != nullptr)
    {
        if (contact->Visibility == 2)
        {
            return 0;
        }

        if (contact->Visibility == 3 && Team->LineOfSight(target->GetPosition()) != 0)
        {
            return 0;
        }
    }

    float distance;
    double skilledRange;

    if (IsMover(Owner))
    {
        const MCMover* mover = static_cast<const MCMover*>(Owner);

        if (mover->Sensor == 0xff)
        {
            return 0;
        }

        if (mover->Inventory[mover->Sensor].Disabled != 0)
        {
            return 0;
        }

        MCVector3D targetPosition = target->GetPosition();
        distance = static_cast<float>(Owner->DistanceFrom(targetPosition));
        skilledRange = SkilledRangeUnrounded(*this);
        // A working probe reaches hidden targets within its share of the range.
        double probeRange = -1.0;

        if (mover->Probe != 0xff && mover->Inventory[mover->Probe].Disabled == 0)
        {
            probeRange = static_cast<double>(MasterComponentList[mover->Inventory[mover->Probe].MasterID].RangeOrHeat) *
                         skilledRange;
        }

        if (distance <= probeRange)
        {
            return skilledRange < distance ? 0 : 1;
        }
    }
    else
    {
        MCVector3D targetPosition = target->GetPosition();
        distance = static_cast<float>(Owner->DistanceFrom(targetPosition));
        skilledRange = SkilledRangeUnrounded(*this);
    }

    if (target->Status == 5)
    {
        return 0;
    }

    return skilledRange < distance ? 0 : 1;
}

//---------------------------------------------------------------------------
// SensorSystemManager
//---------------------------------------------------------------------------

auto MCSensorSystemManager::Init(MCFitIniFile*) -> int32_t
{
    // Port fix: sized by the port's pointer (0x104 bytes in the original).
    Sensors = std::make_unique<MCSensorSystem*[]>(MAX_SENSORS);

    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        MCSensorSystem* sensor = new MCSensorSystem;

        if (sensor != nullptr)
        {
            sensor->Init();
        }

        Sensors[i] = sensor;
    }

    Sensors[0]->Id = 0;
    Sensors[0]->Prev = nullptr;
    Sensors[0]->Next = Sensors[1];

    for (int32_t i = 1; i < MAX_SENSORS - 1; i++)
    {
        Sensors[i]->Id = i;
        Sensors[i]->Prev = Sensors[i - 1];
        Sensors[i]->Next = Sensors[i + 1];
    }

    Sensors[MAX_SENSORS - 1]->Id = MAX_SENSORS - 1;
    Sensors[MAX_SENSORS - 1]->Prev = Sensors[MAX_SENSORS - 2];
    Sensors[MAX_SENSORS - 1]->Next = nullptr;

    FreeList = Sensors[0];
    NumFree = MAX_SENSORS;
    return 0;
}

auto MCSensorSystemManager::NewSensor() -> MCSensorSystem*
{
    if (NumFree == 0)
    {
        Fatal(0, " No More Free Sensors ");
    }

    MCSensorSystem* sensor = FreeList;
    NumFree--;
    FreeList = sensor->Next;

    if (FreeList != nullptr)
    {
        FreeList->Prev = nullptr;
    }

    sensor->Next = nullptr;
    return sensor;
}

auto MCSensorSystemManager::FreeSensor(MCSensorSystem* sensor) -> void
{
    MCSensorSystem* oldFirst = FreeList;
    NumFree++;
    FreeList = sensor;
    sensor->Prev = nullptr;
    sensor->Next = oldFirst;

    // Port fix: the original wrote through a null free list when every sensor was taken.
    if (oldFirst != nullptr)
    {
        oldFirst->Prev = sensor;
    }
}

auto MCSensorSystemManager::Destroy() -> void
{
    for (int32_t i = 0; i < MAX_SENSORS; i++)
    {
        // Clears duplicates from i on, i itself included, so the delete below never runs (see the header).
        for (int32_t j = i; j < MAX_SENSORS; j++)
        {
            if (Sensors[i] == Sensors[j])
            {
                Sensors[j] = nullptr;
            }
        }

        if (Sensors[i] != nullptr)
        {
            Sensors[i]->Destroy();
            delete Sensors[i];
        }

        Sensors[i] = nullptr;
    }

    Sensors.reset();
    NumFree = 0;
    FreeList = nullptr;
}
