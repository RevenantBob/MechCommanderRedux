#include "stdafx.h"
#include "object/MCPotentialContact.h"
#include "object/MCBigGameObject.h"
#include "object/MCContactSystem.h"
#include "object/MCForces.h"
#include "object/MCSensorSystem.h"
#include "object/MCTeam.h"

//---------------------------------------------------------------------------
// _PotentialContact
//---------------------------------------------------------------------------

auto MCPotentialContact::Reset() -> void
{
    Object = nullptr;
    ContactType = MCPotentialContactType::Allied;
    Visibility = 0;
    ContactStatus = {};
    NumSensors = {};
    LostVisual = {};
    TeamSlot = {-1, -1, -1};
    SensorSlots.clear();
}

auto MCPotentialContact::SensorSlot(int32_t sensorId) const -> int32_t
{
    return sensorId < static_cast<int32_t>(SensorSlots.size()) ? SensorSlots[sensorId] : -1;
}

auto MCPotentialContact::SetSensorSlot(int32_t sensorId, int32_t slot) -> void
{
    if (sensorId >= static_cast<int32_t>(SensorSlots.size()))
    {
        SensorSlots.resize(static_cast<size_t>(sensorId) + 1, -1);
    }

    SensorSlots[sensorId] = slot;
}

auto MCPotentialContact::UpdateStatus(MCTeam* team) -> void
{
    if (team == nullptr)
    {
        return;
    }

    const int32_t teamId = team->Id;
    const MCContactStatus oldStatus = ContactStatus[teamId];
    MCContactStatus newStatus = MCContactStatus::None;

    if (team->LineOfSight(Object->GetPosition()) == 0)
    {
        if (NumSensors[teamId] != 0)
        {
            newStatus = MCContactStatus::Sensor;
        }
    }
    else if (Visibility != 3)
    {
        newStatus = MCContactStatus::Visual;
    }

    if (oldStatus == newStatus)
    {
        return;
    }

    ContactStatus[teamId] = newStatus;

    if (oldStatus == MCContactStatus::Visual)
    {
        team->RemoveLosContact(this);
    }
    else if (oldStatus == MCContactStatus::Sensor)
    {
        LostVisual[teamId] = 0;
        team->RemoveSensorContact(this);
    }

    if (newStatus == MCContactStatus::Visual)
    {
        team->AddLosContact(this);
        return;
    }

    if (newStatus == MCContactStatus::Sensor)
    {
        if (oldStatus == MCContactStatus::Visual)
        {
            LostVisual[teamId] = 1;
        }

        team->AddSensorContact(this);
    }
}

//---------------------------------------------------------------------------
// PotentialContactManager
//---------------------------------------------------------------------------

MCPotentialContactManager::MCPotentialContactManager(int32_t initialContacts)
{
    // The free list starts with contact 0 and goes up.
    for (int32_t i = 0; i < initialContacts; i++)
    {
        _Pool.emplace_back().Id = static_cast<uint16_t>(i);
    }

    for (int32_t i = initialContacts - 1; i >= 0; i--)
    {
        _Free.push_back(static_cast<uint16_t>(i));
    }
}

auto MCPotentialContactManager::Add(MCPotentialContactType type, MCBigGameObject* object, int8_t visibility)
    -> MCPotentialContact*
{
    // The pool used to be FIT-sized and fatal when full; it grows instead.
    if (_Free.empty())
    {
        _Free.push_back(static_cast<uint16_t>(_Pool.size()));
        _Pool.emplace_back().Id = _Free.back();
    }

    MCPotentialContact& contact = _Pool[_Free.back()];
    _Free.pop_back();
    contact.Reset();
    contact.Object = object;
    contact.ContactType = type;
    contact.Visibility = visibility;
    std::list<MCPotentialContact*>& list = _Lists[static_cast<size_t>(type)];
    list.push_front(&contact);
    contact.ListEntry = list.begin();
    return &contact;
}

auto MCPotentialContactManager::GetContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly) -> int32_t
{
    // Per team, the lists to count: the enemy's first.
    static constexpr std::array<std::array<MCPotentialContactType, 3>, 3> ListOrder = {{
        {MCPotentialContactType::Clan, MCPotentialContactType::InnerSphere, MCPotentialContactType::Allied},
        {MCPotentialContactType::InnerSphere, MCPotentialContactType::Clan, MCPotentialContactType::Allied},
        {MCPotentialContactType::Clan, MCPotentialContactType::InnerSphere, MCPotentialContactType::Allied},
    }};

    counts[0] = 0;
    counts[1] = 0;
    counts[2] = 0;
    const int32_t numLists = enemiesOnly ? 1 : 3;

    for (int32_t list = 0; list < numLists; list++)
    {
        for (MCPotentialContact* contact : List(ListOrder[teamId][list]))
        {
            int tagged = 0;
            const auto contactType = static_cast<MCContactStatus>(contact->Object->GetContactType(teamId, tagged));

            if (tagged != 0)
            {
                counts[0]++;
            }
            else if (contactType == MCContactStatus::Sensor)
            {
                counts[2]++;
            }
            else if (contactType == MCContactStatus::Visual)
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

    for (int32_t sensorId = 0; sensorId < static_cast<int32_t>(contact->SensorSlots.size()); sensorId++)
    {
        if (contact->SensorSlot(sensorId) != -1)
        {
            SensorSystemManager()->Sensor(sensorId)->RemoveSensorContact(contact);
        }
    }

    for (int32_t i = 0; i < MCForces::NumTeams; i++)
    {
        if (contact->TeamSlot[i] == -1)
        {
            continue;
        }

        if (contact->ContactStatus[i] == MCContactStatus::Visual)
        {
            TeamById(i)->RemoveLosContact(contact);
        }
        else if (contact->ContactStatus[i] == MCContactStatus::Sensor)
        {
            TeamById(i)->RemoveSensorContact(contact);
        }
    }

    _Lists[static_cast<size_t>(contact->ContactType)].erase(contact->ListEntry);
    contact->Object = nullptr;
    _Free.push_back(contact->Id);
}

auto MCPotentialContactManager::Move(MCPotentialContact* contact, MCPotentialContactType type, int8_t visibility)
    -> void
{
    if (contact == nullptr)
    {
        return;
    }

    _Lists[static_cast<size_t>(contact->ContactType)].erase(contact->ListEntry);
    contact->Visibility = visibility;
    contact->ContactType = type;
    std::list<MCPotentialContact*>& list = _Lists[static_cast<size_t>(type)];
    list.push_front(contact);
    contact->ListEntry = list.begin();
}

auto MCPotentialContactManager::UpdateStatus() -> void
{
    for (const std::list<MCPotentialContact*>& list : _Lists)
    {
        for (MCPotentialContact* contact : list)
        {
            const MCTeam* team = contact->Object->GetTeam();

            if (team == ClanTeam())
            {
                contact->UpdateStatus(InnerSphereTeam());
                contact->UpdateStatus(AlliedTeam());
            }
            else if (team == InnerSphereTeam())
            {
                contact->UpdateStatus(ClanTeam());
                contact->UpdateStatus(AlliedTeam());
            }
            else
            {
                contact->UpdateStatus(InnerSphereTeam());
                contact->UpdateStatus(ClanTeam());
            }
        }
    }
}
