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
    Object.reset();
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

auto MCPotentialContact::UpdateStatus(MCTeam& team) -> void
{
    const int32_t teamId = team.Id;
    const MCContactStatus oldStatus = ContactStatus[teamId];
    MCContactStatus newStatus = MCContactStatus::None;

    if (team.LineOfSight(LockObject(Object)->GetPosition()) == 0)
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
        team.RemoveLosContact(*this);
    }
    else if (oldStatus == MCContactStatus::Sensor)
    {
        LostVisual[teamId] = 0;
        team.RemoveSensorContact(*this);
    }

    if (newStatus == MCContactStatus::Visual)
    {
        team.AddLosContact(*this);
        return;
    }

    if (newStatus == MCContactStatus::Sensor)
    {
        if (oldStatus == MCContactStatus::Visual)
        {
            LostVisual[teamId] = 1;
        }

        team.AddSensorContact(*this);
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
        _Pool.push_back(std::make_shared<MCPotentialContact>());
        _Pool.back()->Id = static_cast<uint16_t>(i);
    }

    for (int32_t i = initialContacts - 1; i >= 0; i--)
    {
        _Free.push_back(static_cast<uint16_t>(i));
    }
}

auto MCPotentialContactManager::Add(MCPotentialContactType type, MCBigGameObject& object, int8_t visibility)
    -> std::shared_ptr<MCPotentialContact>
{
    // The pool used to be FIT-sized and fatal when full; it grows instead.
    if (_Free.empty())
    {
        _Free.push_back(static_cast<uint16_t>(_Pool.size()));
        _Pool.push_back(std::make_shared<MCPotentialContact>());
        _Pool.back()->Id = _Free.back();
    }

    std::shared_ptr<MCPotentialContact> contact = _Pool[_Free.back()];
    _Free.pop_back();
    contact->Reset();
    contact->Object = object.WeakAs<MCBigGameObject>();
    contact->ContactType = type;
    contact->Visibility = visibility;
    std::list<uint16_t>& list = _Lists[static_cast<size_t>(type)];
    list.push_front(contact->Id);
    contact->ListEntry = list.begin();
    return contact;
}

auto MCPotentialContactManager::Remove(MCPotentialContact& contact) -> void
{
    for (int32_t sensorId = 0; sensorId < static_cast<int32_t>(contact.SensorSlots.size()); sensorId++)
    {
        if (contact.SensorSlot(sensorId) != -1)
        {
            SensorSystemManager().Sensor(sensorId).RemoveSensorContact(contact);
        }
    }

    for (int32_t i = 0; i < MCForces::NumTeams; i++)
    {
        if (contact.TeamSlot[i] == -1)
        {
            continue;
        }

        if (contact.ContactStatus[i] == MCContactStatus::Visual)
        {
            TeamById(i)->RemoveLosContact(contact);
        }
        else if (contact.ContactStatus[i] == MCContactStatus::Sensor)
        {
            TeamById(i)->RemoveSensorContact(contact);
        }
    }

    _Lists[static_cast<size_t>(contact.ContactType)].erase(contact.ListEntry);
    contact.Object.reset();
    _Free.push_back(contact.Id);
}

auto MCPotentialContactManager::Move(MCPotentialContact& contact, MCPotentialContactType type, int8_t visibility)
    -> void
{
    _Lists[static_cast<size_t>(contact.ContactType)].erase(contact.ListEntry);
    contact.Visibility = visibility;
    contact.ContactType = type;
    std::list<uint16_t>& list = _Lists[static_cast<size_t>(type)];
    list.push_front(contact.Id);
    contact.ListEntry = list.begin();
}

auto MCPotentialContactManager::UpdateStatus() -> void
{
    for (const std::list<uint16_t>& list : _Lists)
    {
        for (const uint16_t id : list)
        {
            MCPotentialContact& contact = Contact(id);
            // A team that isn't in the mission (the allied one) has nothing to update.
            const auto update = [&contact](std::shared_ptr<MCTeam> team)
            {
                if (team != nullptr)
                {
                    contact.UpdateStatus(*team);
                }
            };

            const std::shared_ptr<MCTeam> team = LockObject(contact.Object)->GetTeam();

            if (team == ClanTeam())
            {
                update(InnerSphereTeam());
                update(AlliedTeam());
            }
            else if (team == InnerSphereTeam())
            {
                update(ClanTeam());
                update(AlliedTeam());
            }
            else
            {
                update(InnerSphereTeam());
                update(ClanTeam());
            }
        }
    }
}
