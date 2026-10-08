#pragma once

class MCBigGameObject;
class MCTeam;

/// <summary>Which of the potential contact manager's lists an object is on (by alignment).</summary>
enum class MCPotentialContactType : int8_t
{
    /// <summary>Inner Sphere objects.</summary>
    InnerSphere = 0,
    /// <summary>Clan objects.</summary>
    Clan = 1,
    /// <summary>Allied objects.</summary>
    Allied = 2,
};

/// <summary>How a team knows a contact (<see cref="MCPotentialContact::ContactStatus"/>).</summary>
enum class MCContactStatus : uint8_t
{
    None = 0,
    /// <summary>In the team's line of sight.</summary>
    Visual = 1,
    /// <summary>Only on the team's sensors.</summary>
    Sensor = 2,
};

/// <summary>
/// An object that can be seen, and how each team sees it. Pooled by the <see cref="MCPotentialContactManager"/>.
/// </summary>
/// <remarks>Original source: <c>object\contact.cpp</c> (<c>_PotentialContact</c>).</remarks>
struct MCPotentialContact
{
    /// <summary>Clears the per-team state and sensor slots; allied list, visibility 0.</summary>
    void Reset();
    /// <summary>
    /// Works out whether <paramref name="team"/> sees the object by line of sight, by sensors only, or not, and
    /// moves it between the team's LOS and sensor contact lists when that changes.
    /// </summary>
    void UpdateStatus(MCTeam* team);
    /// <summary>The contact's slot in sensor <paramref name="sensorId"/>'s list; -1 for none.</summary>
    int32_t SensorSlot(int32_t sensorId) const;
    /// <summary>Sets the contact's slot in sensor <paramref name="sensorId"/>'s list (-1: none).</summary>
    void SetSensorSlot(int32_t sensorId, int32_t slot);

    /// <summary>Index in the manager's pool (and in the sensors' and teams' contact lists).</summary>
    uint16_t Id = 0;
    /// <summary>The object.</summary>
    MCBigGameObject* Object = nullptr;
    /// <summary>The manager list it is on.</summary>
    MCPotentialContactType ContactType = MCPotentialContactType::Allied;
    /// <summary>How visible it is: 2 can't be sensed at all, 3 can't be seen by line of sight.</summary>
    int8_t Visibility = 0;
    /// <summary>Per team: how the team knows it.</summary>
    std::array<MCContactStatus, 3> ContactStatus{};
    /// <summary>Per team: how many of its sensors hold the contact.</summary>
    std::array<int32_t, 3> NumSensors{};
    /// <summary>Per team: it went from visual to sensor contact (BigGameObject::getContactType's tagged).</summary>
    std::array<uint8_t, 3> LostVisual{};
    /// <summary>Per team: the contact's slot in that team's LOS or sensor list, -1 for none.</summary>
    std::array<int32_t, 3> TeamSlot{-1, -1, -1};
    /// <summary>Per sensor id: the contact's slot in that sensor's list, -1 for none (missing entries too).</summary>
    std::vector<int32_t> SensorSlots;
    /// <summary>Its place in the manager's list.</summary>
    std::list<MCPotentialContact*>::iterator ListEntry;
};

/// <summary>The pool of potential contacts and the three lists they are on.</summary>
/// <remarks>Original source: <c>object\contact.cpp</c>. Part of the <see cref="MCContactSystem"/>.</remarks>
class MCPotentialContactManager
{
public:
    /// <summary>A pool of <paramref name="initialContacts"/> free contacts (it grows past them).</summary>
    explicit MCPotentialContactManager(int32_t initialContacts);
    MCPotentialContactManager(const MCPotentialContactManager&) = delete;
    MCPotentialContactManager& operator=(const MCPotentialContactManager&) = delete;

    /// <summary>
    /// Takes a free contact for <paramref name="object"/> onto list <paramref name="type"/> (in front); the contact
    /// freed last is taken first, as in the original.
    /// </summary>
    MCPotentialContact* Add(MCPotentialContactType type, MCBigGameObject* object, int8_t visibility);
    /// <summary>
    /// Counts what team <paramref name="teamId"/> knows of the enemy list (and, unless
    /// <paramref name="enemiesOnly"/>, of every list): counts[0] tagged, [1] visual, [2] sensor contacts.
    /// </summary>
    int32_t GetContactCounts(int32_t* counts, int32_t teamId, int enemiesOnly);
    /// <summary>Takes <paramref name="contact"/> off every sensor and team list and back to the free ones.</summary>
    void Remove(MCPotentialContact* contact);
    /// <summary>Moves <paramref name="contact"/> to the front of list <paramref name="type"/> with a new
    /// visibility.</summary>
    void Move(MCPotentialContact* contact, MCPotentialContactType type, int8_t visibility);
    /// <summary>Updates every contact's status for the two teams it isn't on.</summary>
    void UpdateStatus();
    /// <summary>Contact <paramref name="id"/>.</summary>
    MCPotentialContact& Contact(uint16_t id) { return _Pool[id]; }
    /// <summary>The contacts on list <paramref name="type"/>, newest first.</summary>
    const std::list<MCPotentialContact*>& List(MCPotentialContactType type) const
    {
        return _Lists[static_cast<size_t>(type)];
    }

    /// <summary>Contacts free to take without growing the pool.</summary>
    size_t NumFree() const { return _Free.size(); }

private:
    /// <summary>The contacts by id (a deque: contacts never move as it grows).</summary>
    std::deque<MCPotentialContact> _Pool;
    /// <summary>The free contacts' ids; the next taken is at the back.</summary>
    std::vector<uint16_t> _Free;
    /// <summary>The contacts by <see cref="MCPotentialContactType"/>.</summary>
    std::array<std::list<MCPotentialContact*>, 3> _Lists;
};
