#include "stdafx.h"
#include "platform/MCDirectPlay.h"
#include "platform/MCSocket.h"

#include <random>

namespace
{
    using namespace MCDirectPlayGuids;

    /// <summary>DirectPlay's "object not initialized" (no connection selected yet).</summary>
    constexpr uint32_t DPERR_UNINITIALIZED = 0x88770140;
    /// <summary>DirectPlay's "access denied" (a joiner changing the host's session description).</summary>
    constexpr uint32_t DPERR_ACCESSDENIED = 0x8877000a;

    /// <summary>The first word of every UDP datagram and of the join request: "MCG1".</summary>
    constexpr uint32_t ProtocolMagic = 0x3147434d;
    /// <summary>A UDP session query ("are you hosting this game?").</summary>
    constexpr uint8_t DatagramQuery = 1;
    /// <summary>A UDP answer to a session query: the session, its TCP port and its players.</summary>
    constexpr uint8_t DatagramReply = 2;

    /// <summary>A frame on a TCP connection: a 32-bit length (of the type and payload), the type, the payload.</summary>
    enum class FrameType : uint8_t
    {
        /// <summary>Joiner to host: magic, application GUID, session GUID, password.</summary>
        JoinRequest = 1,
        /// <summary>Host to joiner: result; when DP_OK the session and every player and group.</summary>
        JoinReply,
        /// <summary>Joiner to host: token, the new player (names, flags, data).</summary>
        CreatePlayer,
        /// <summary>Joiner to host: token, the new group.</summary>
        CreateGroup,
        /// <summary>Host to joiner: token, result, the new id.</summary>
        CreateReply,
        /// <summary>Either way: group, player.</summary>
        AddToGroup,
        /// <summary>Either way: group, player.</summary>
        DeleteFromGroup,
        /// <summary>Either way: group, data.</summary>
        SetGroupData,
        /// <summary>Host to joiner: the session.</summary>
        SessionDesc,
        /// <summary>Either way: sender, receiver (player, group or everyone), bytes.</summary>
        Data,
        /// <summary>Host to joiner: a player or group was created (the entity).</summary>
        Created,
        /// <summary>Host to joiner: a player or group was destroyed (its id).</summary>
        Destroyed
    };

    /// <summary>Largest frame a peer may announce (anything bigger means the stream is garbage).</summary>
    constexpr uint32_t MaxFrameSize = 1u << 20;
    /// <summary>How long a join or a create waits for the host's answer.</summary>
    constexpr uint64_t ReplyTimeoutMs = 5000;
    /// <summary>How long connecting to a host may take.</summary>
    constexpr int ConnectTimeoutMs = 3000;
    /// <summary>How long EnumSessions listens for answers after asking.</summary>
    constexpr uint64_t EnumListenMs = 60;
    /// <summary>How long an answering session stays listed without answering again.</summary>
    constexpr uint64_t SessionExpiryMs = 4000;

    /// <summary>The connection data EnumConnections hands out and InitializeConnection takes (the port's own layout).</summary>
    struct ConnectionData
    {
        _GUID serviceProvider;
        /// <summary>The host to look for ("name" or "name:port"); empty = search the LAN.</summary>
        char address[256];
    };

    static_assert(sizeof(ConnectionData) == MCDirectPlay::ConnectionDataSize);

    /// <summary>Appends little-endian values to a byte buffer.</summary>
    class Writer
    {
    public:
        void U8(uint8_t value) { Bytes.push_back(value); }

        void U16(uint16_t value)
        {
            U8(static_cast<uint8_t>(value));
            U8(static_cast<uint8_t>(value >> 8));
        }

        void U32(uint32_t value)
        {
            U16(static_cast<uint16_t>(value));
            U16(static_cast<uint16_t>(value >> 16));
        }

        void Guid(const _GUID& value)
        {
            U32(value.Data1);
            U16(value.Data2);
            U16(value.Data3);

            for (uint8_t b : value.Data4)
            {
                U8(b);
            }
        }

        void Blob(std::span<const uint8_t> data)
        {
            U32(static_cast<uint32_t>(data.size()));
            Bytes.insert(Bytes.end(), data.begin(), data.end());
        }

        void String(std::string_view text)
        {
            Blob(std::span(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
        }

        std::vector<uint8_t> Bytes;
    };

    /// <summary>Reads what <see cref="Writer"/> wrote; any overrun clears <see cref="Ok"/> and yields zeros.</summary>
    class Reader
    {
    public:
        explicit Reader(std::span<const uint8_t> data) : _Data(data) {}

        uint8_t U8()
        {
            if (_Position >= _Data.size())
            {
                Ok = false;
                return 0;
            }

            return _Data[_Position++];
        }

        uint16_t U16()
        {
            const uint16_t low = U8();
            return static_cast<uint16_t>(low | (U8() << 8));
        }

        uint32_t U32()
        {
            const uint32_t low = U16();
            return low | (static_cast<uint32_t>(U16()) << 16);
        }

        _GUID Guid()
        {
            _GUID value{};
            value.Data1 = U32();
            value.Data2 = U16();
            value.Data3 = U16();

            for (uint8_t& b : value.Data4)
            {
                b = U8();
            }

            return value;
        }

        std::vector<uint8_t> Blob()
        {
            const uint32_t size = U32();

            if (!Ok || size > _Data.size() - _Position)
            {
                Ok = false;
                return {};
            }

            std::vector<uint8_t> bytes(_Data.begin() + _Position, _Data.begin() + _Position + size);
            _Position += size;
            return bytes;
        }

        std::string String()
        {
            const std::vector<uint8_t> bytes = Blob();
            return std::string(bytes.begin(), bytes.end());
        }

        bool Ok = true;

    private:
        std::span<const uint8_t> _Data;
        size_t _Position = 0;
    };

    /// <summary>A session's description with its own copies of the strings.</summary>
    struct SessionInfo
    {
        DPSESSIONDESC2 Desc{};
        std::string Name;
        std::string Password;

        void Write(Writer& out, bool withPassword) const
        {
            out.U32(Desc.dwFlags);
            out.Guid(Desc.guidInstance);
            out.Guid(Desc.guidApplication);
            out.U32(Desc.dwMaxPlayers);
            out.U32(Desc.dwCurrentPlayers);
            out.U32(Desc.dwReserved1);
            out.U32(Desc.dwReserved2);
            out.U32(Desc.dwUser1);
            out.U32(Desc.dwUser2);
            out.U32(Desc.dwUser3);
            out.U32(Desc.dwUser4);
            out.String(Name);
            out.String(withPassword ? Password : std::string());
        }

        void Read(Reader& in)
        {
            Desc = {};
            Desc.dwSize = sizeof(DPSESSIONDESC2);
            Desc.dwFlags = in.U32();
            Desc.guidInstance = in.Guid();
            Desc.guidApplication = in.Guid();
            Desc.dwMaxPlayers = in.U32();
            Desc.dwCurrentPlayers = in.U32();
            Desc.dwReserved1 = in.U32();
            Desc.dwReserved2 = in.U32();
            Desc.dwUser1 = in.U32();
            Desc.dwUser2 = in.U32();
            Desc.dwUser3 = in.U32();
            Desc.dwUser4 = in.U32();
            Name = in.String();
            Password = in.String();
        }

        /// <summary>Copies a caller's description (and its strings).</summary>
        void Set(const DPSESSIONDESC2& desc)
        {
            Desc = desc;
            Desc.dwSize = sizeof(DPSESSIONDESC2);
            Name = desc.lpszSessionNameA != nullptr ? desc.lpszSessionNameA : "";
            Password = desc.lpszPasswordA != nullptr ? desc.lpszPasswordA : "";
        }

        /// <summary>
        /// The description with its name pointer aimed at this object's copy. The password is never handed out (an
        /// enumerated session's is null, as DirectPlay's were).
        /// </summary>
        DPSESSIONDESC2 View()
        {
            DPSESSIONDESC2 view = Desc;
            view.lpszSessionNameA = Name.data();
            view.lpszPasswordA = nullptr;
            return view;
        }
    };

    /// <summary>A player or group of the session.</summary>
    struct Entity
    {
        uint32_t Id = 0;
        /// <summary>DPPLAYERTYPE_PLAYER or DPPLAYERTYPE_GROUP.</summary>
        uint32_t Type = DPPLAYERTYPE_PLAYER;
        uint32_t Flags = 0;
        uint32_t Parent = 0;
        std::string ShortName;
        std::string LongName;
        std::vector<uint8_t> Data;
        /// <summary>A group's players.</summary>
        std::vector<uint32_t> Members;
        /// <summary>Host only: the index of the joiner connection that owns this player, -1 for the host's own.</summary>
        int Owner = -1;

        void Write(Writer& out) const
        {
            out.U32(Id);
            out.U32(Type);
            out.U32(Flags);
            out.U32(Parent);
            out.String(ShortName);
            out.String(LongName);
            out.Blob(Data);
            out.U32(static_cast<uint32_t>(Members.size()));

            for (uint32_t member : Members)
            {
                out.U32(member);
            }
        }

        void Read(Reader& in)
        {
            Id = in.U32();
            Type = in.U32();
            Flags = in.U32();
            Parent = in.U32();
            ShortName = in.String();
            LongName = in.String();
            Data = in.Blob();
            const uint32_t count = in.U32();
            Members.clear();

            for (uint32_t i = 0; i < count && in.Ok; i++)
            {
                Members.push_back(in.U32());
            }
        }

        void SetName(const DPNAME* name)
        {
            ShortName = (name != nullptr && name->lpszShortNameA != nullptr) ? name->lpszShortNameA : "";
            LongName = (name != nullptr && name->lpszLongNameA != nullptr) ? name->lpszLongNameA : "";
        }
    };

    /// <summary>A message waiting in <see cref="MCDirectPlay::Receive"/>'s queue.</summary>
    struct Incoming
    {
        uint32_t From = DPID_SYSMSG;
        uint32_t To = 0;
        /// <summary>A player's message: its bytes. A system message: unused.</summary>
        std::vector<uint8_t> Payload;
        /// <summary>A system message's DPSYS_* type (0 for a player's message).</summary>
        uint32_t SystemType = 0;
        /// <summary>The player or group a create/destroy/set-data message is about.</summary>
        Entity Subject;
        /// <summary>Add/delete-player-to-group: the group and the player.</summary>
        uint32_t Group = 0;
        uint32_t Player = 0;
        /// <summary>DPSYS_SETSESSIONDESC: the new description.</summary>
        SessionInfo Session;
    };

    /// <summary>One TCP connection with its unparsed input and unsent output.</summary>
    struct Link
    {
        MCSocket::Handle Socket = MCSocket::InvalidHandle;
        std::vector<uint8_t> Input;
        std::vector<uint8_t> Output;
        /// <summary>Host side: the join was accepted.</summary>
        bool Joined = false;
        /// <summary>Close once <see cref="Output"/> is written (a refused join).</summary>
        bool CloseWhenFlushed = false;
        bool Dead = false;
    };

    /// <summary>A session that answered a query.</summary>
    struct FoundSession
    {
        SessionInfo Info;
        MCSocket::Address Address;
        uint64_t SeenAt = 0;
        std::vector<Entity> Players;
    };

    uint64_t NowMs()
    {
        return SDL_GetTicks();
    }

    _GUID NewGuid()
    {
        std::random_device device;
        std::mt19937 random(device());
        _GUID guid{};
        guid.Data1 = random();
        guid.Data2 = static_cast<uint16_t>(random());
        guid.Data3 = static_cast<uint16_t>((random() & 0x0fff) | 0x4000);

        for (uint8_t& b : guid.Data4)
        {
            b = static_cast<uint8_t>(random());
        }

        return guid;
    }

    /// <summary>Splits "host:port" (the port is optional).</summary>
    std::pair<std::string, uint16_t> SplitAddress(std::string_view text)
    {
        const size_t colon = text.rfind(':');

        if (colon != std::string_view::npos)
        {
            int port = 0;
            const std::string_view digits = text.substr(colon + 1);
            const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), port);

            if (error == std::errc() && end == digits.data() + digits.size() && port > 0 && port < 65536)
            {
                return {std::string(text.substr(0, colon)), static_cast<uint16_t>(port)};
            }
        }

        return {std::string(text), MCDirectPlay::DefaultPort};
    }
}

/// <summary>Everything an MCDirectPlay holds (kept out of the header so it needs no socket types).</summary>
struct MCDirectPlay::Impl
{
    enum class Mode
    {
        None,
        Host,
        Joiner
    };

    /// <summary>Whether InitializeConnection was called.</summary>
    bool Initialized = false;
    /// <summary>The host searched for: empty = the LAN.</summary>
    std::string HostName;
    uint16_t Port = DefaultPort;

    Mode State = Mode::None;
    /// <summary>A joiner whose host went away: everything fails until Close.</summary>
    bool Lost = false;
    SessionInfo Session;
    std::map<uint32_t, Entity> Entities;
    std::set<uint32_t> LocalPlayers;
    std::deque<Incoming> Inbox;
    /// <summary>Host: the next id to give a player or group.</summary>
    uint32_t NextId = 0;

    /// <summary>Host: the listening TCP socket. Host and searcher: the UDP socket.</summary>
    MCSocket::Handle Listener = MCSocket::InvalidHandle;
    MCSocket::Handle Udp = MCSocket::InvalidHandle;
    /// <summary>Host: one per joiner (slots are never reused while hosting, so owners stay valid).</summary>
    std::vector<Link> Joiners;
    /// <summary>Joiner: the connection to the host.</summary>
    Link Server;

    /// <summary>Sessions that answered queries, by instance GUID.</summary>
    std::vector<FoundSession> Found;

    /// <summary>Joiner: the answers the host sent to blocking requests (JoinReply, CreateReply) not yet taken.</summary>
    std::deque<std::pair<FrameType, std::vector<uint8_t>>> Replies;
    uint32_t NextToken = 1;

    ~Impl()
    {
        Shutdown();
        MCSocket::Close(Udp);
    }

    // ---- framing -------------------------------------------------------------------------------------------------

    static void Queue(Link& link, FrameType type, const Writer& payload)
    {
        const uint32_t size = static_cast<uint32_t>(payload.Bytes.size() + 1);
        Writer header;
        header.U32(size);
        header.U8(static_cast<uint8_t>(type));
        link.Output.insert(link.Output.end(), header.Bytes.begin(), header.Bytes.end());
        link.Output.insert(link.Output.end(), payload.Bytes.begin(), payload.Bytes.end());
        Flush(link);
    }

    static void Flush(Link& link)
    {
        while (!link.Dead && !link.Output.empty())
        {
            const int sent = MCSocket::Send(link.Socket, link.Output.data(), link.Output.size());

            if (sent < 0)
            {
                link.Dead = true;
                return;
            }

            if (sent == 0)
            {
                return;
            }

            link.Output.erase(link.Output.begin(), link.Output.begin() + sent);
        }

        if (link.Output.empty() && link.CloseWhenFlushed)
        {
            link.Dead = true;
        }
    }

    /// <summary>Reads what arrived on <paramref name="link"/> and hands each whole frame to <paramref name="handle"/>.</summary>
    template <class Handler> static void ReadFrames(Link& link, Handler&& handle)
    {
        uint8_t chunk[8192];

        while (!link.Dead)
        {
            const int received = MCSocket::Receive(link.Socket, chunk, sizeof(chunk));

            if (received < 0)
            {
                link.Dead = true;
                break;
            }

            if (received == 0)
            {
                break;
            }

            link.Input.insert(link.Input.end(), chunk, chunk + received);
        }

        size_t position = 0;

        while (link.Input.size() - position >= 5)
        {
            const uint8_t* p = link.Input.data() + position;
            const uint32_t size = p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);

            if (size == 0 || size > MaxFrameSize)
            {
                link.Dead = true;
                break;
            }

            if (link.Input.size() - position - 4 < size)
            {
                break;
            }

            const FrameType type = static_cast<FrameType>(p[4]);
            handle(type, std::span<const uint8_t>(p + 5, size - 1));
            position += 4 + size;
        }

        link.Input.erase(link.Input.begin(), link.Input.begin() + position);
    }

    // ---- shared state --------------------------------------------------------------------------------------------

    uint32_t PlayerCount() const
    {
        uint32_t count = 0;

        for (const auto& [id, entity] : Entities)
        {
            if (entity.Type == DPPLAYERTYPE_PLAYER)
            {
                count++;
            }
        }

        return count;
    }

    /// <summary>System messages only reach machines that have a player (DirectPlay delivers them to players).</summary>
    void QueueSystem(Incoming message)
    {
        if (!LocalPlayers.empty())
        {
            message.From = DPID_SYSMSG;
            message.To = *LocalPlayers.begin();
            Inbox.push_back(std::move(message));
        }
    }

    void QueueCreated(const Entity& entity)
    {
        Incoming message;
        message.SystemType = DPSYS_CREATEPLAYERORGROUP;
        message.Subject = entity;
        QueueSystem(std::move(message));
    }

    void QueueDestroyed(const Entity& entity)
    {
        Incoming message;
        message.SystemType = DPSYS_DESTROYPLAYERORGROUP;
        message.Subject = entity;
        QueueSystem(std::move(message));
    }

    void QueueMembership(uint32_t type, uint32_t group, uint32_t player)
    {
        Incoming message;
        message.SystemType = type;
        message.Group = group;
        message.Player = player;
        QueueSystem(std::move(message));
    }

    /// <summary>Adds or removes a group member in the local state.</summary>
    /// <returns>Whether the membership changed.</returns>
    bool ApplyMembership(bool add, uint32_t group, uint32_t player)
    {
        const auto found = Entities.find(group);

        if (found == Entities.end() || found->second.Type != DPPLAYERTYPE_GROUP || !Entities.contains(player))
        {
            return false;
        }

        std::vector<uint32_t>& members = found->second.Members;
        const auto member = std::ranges::find(members, player);

        if (add && member == members.end())
        {
            members.push_back(player);
            return true;
        }

        if (!add && member != members.end())
        {
            members.erase(member);
            return true;
        }

        return false;
    }

    /// <summary>Removes <paramref name="id"/> and, for a player, its group memberships (in the local state).</summary>
    std::optional<Entity> RemoveEntity(uint32_t id)
    {
        const auto found = Entities.find(id);

        if (found == Entities.end())
        {
            return std::nullopt;
        }

        Entity removed = std::move(found->second);
        Entities.erase(found);
        LocalPlayers.erase(id);
        return removed;
    }

    // ---- host ----------------------------------------------------------------------------------------------------

    /// <summary>Host: sends a frame to every joined connection except <paramref name="except"/>.</summary>
    void Broadcast(FrameType type, const Writer& payload, int except)
    {
        for (size_t i = 0; i < Joiners.size(); i++)
        {
            if (static_cast<int>(i) != except && Joiners[i].Joined && !Joiners[i].Dead)
            {
                Queue(Joiners[i], type, payload);
            }
        }
    }

    /// <summary>Host: numbers and records a new player or group and tells everyone else.</summary>
    uint32_t HostCreate(Entity entity, int origin)
    {
        entity.Id = NextId++;
        entity.Owner = origin;
        Writer note;
        entity.Write(note);
        Broadcast(FrameType::Created, note, origin);

        if (origin >= 0)
        {
            QueueCreated(entity);
        }

        const uint32_t id = entity.Id;
        Entities[id] = std::move(entity);
        return id;
    }

    /// <summary>Host: changes a membership and tells everyone else.</summary>
    void HostMembership(bool add, uint32_t group, uint32_t player, int origin)
    {
        if (!ApplyMembership(add, group, player))
        {
            return;
        }

        Writer note;
        note.U32(group);
        note.U32(player);
        Broadcast(add ? FrameType::AddToGroup : FrameType::DeleteFromGroup, note, origin);

        if (origin >= 0)
        {
            QueueMembership(add ? DPSYS_ADDPLAYERTOGROUP : DPSYS_DELETEPLAYERFROMGROUP, group, player);
        }
    }

    void HostSetGroupData(uint32_t group, std::vector<uint8_t> data, int origin)
    {
        const auto found = Entities.find(group);

        if (found == Entities.end())
        {
            return;
        }

        found->second.Data = std::move(data);
        Writer note;
        note.U32(group);
        note.Blob(found->second.Data);
        Broadcast(FrameType::SetGroupData, note, origin);

        if (origin >= 0)
        {
            Incoming message;
            message.SystemType = DPSYS_SETPLAYERORGROUPDATA;
            message.Subject = found->second;
            QueueSystem(std::move(message));
        }
    }

    /// <summary>Host: removes a player (its memberships first) and tells everyone.</summary>
    void HostDestroyPlayer(uint32_t id)
    {
        for (auto& [groupID, group] : Entities)
        {
            if (group.Type == DPPLAYERTYPE_GROUP && std::ranges::find(group.Members, id) != group.Members.end())
            {
                HostMembership(false, groupID, id, -2);
                QueueMembership(DPSYS_DELETEPLAYERFROMGROUP, groupID, id);
            }
        }

        std::optional<Entity> removed = RemoveEntity(id);

        if (removed)
        {
            Writer note;
            note.U32(id);
            Broadcast(FrameType::Destroyed, note, -1);
            QueueDestroyed(*removed);
        }
    }

    /// <summary>Host: delivers a player's message to its receivers (never back to the sender).</summary>
    void Route(uint32_t from, uint32_t to, std::span<const uint8_t> payload)
    {
        std::vector<uint32_t> receivers;

        if (to == DPID_ALLPLAYERS)
        {
            for (const auto& [id, entity] : Entities)
            {
                if (entity.Type == DPPLAYERTYPE_PLAYER)
                {
                    receivers.push_back(id);
                }
            }
        }
        else if (const auto found = Entities.find(to); found != Entities.end())
        {
            if (found->second.Type == DPPLAYERTYPE_GROUP)
            {
                receivers = found->second.Members;
            }
            else
            {
                receivers.push_back(to);
            }
        }

        for (uint32_t receiver : receivers)
        {
            if (receiver == from)
            {
                continue;
            }

            if (LocalPlayers.contains(receiver))
            {
                Incoming message;
                message.From = from;
                message.To = receiver;
                message.Payload.assign(payload.begin(), payload.end());
                Inbox.push_back(std::move(message));
                continue;
            }

            const auto found = Entities.find(receiver);

            if (found != Entities.end() && found->second.Owner >= 0)
            {
                Writer frame;
                frame.U32(from);
                frame.U32(receiver);
                frame.Blob(payload);
                Queue(Joiners[found->second.Owner], FrameType::Data, frame);
            }
        }
    }

    void HostHandle(int index, FrameType type, std::span<const uint8_t> payload)
    {
        Link& link = Joiners[index];
        Reader in(payload);

        if (!link.Joined)
        {
            if (type != FrameType::JoinRequest)
            {
                link.Dead = true;
                return;
            }

            const uint32_t magic = in.U32();
            const _GUID application = in.Guid();
            const _GUID instance = in.Guid();
            const std::string password = in.String();
            uint32_t result = DP_OK;

            if (!in.Ok || magic != ProtocolMagic || !MCSameGuid(application, Session.Desc.guidApplication) ||
                !MCSameGuid(instance, Session.Desc.guidInstance))
            {
                result = DPERR_NOSESSIONS;
            }
            else if ((Session.Desc.dwFlags & (DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED)) != 0 ||
                     PlayerCount() >= Session.Desc.dwMaxPlayers)
            {
                result = DPERR_NONEWPLAYERS;
            }
            else if ((Session.Desc.dwFlags & DPSESSION_PASSWORDREQUIRED) != 0 && password != Session.Password)
            {
                result = DPERR_INVALIDPASSWORD;
            }

            Writer reply;
            reply.U32(result);

            if (result == DP_OK)
            {
                Session.Desc.dwCurrentPlayers = PlayerCount();
                Session.Write(reply, false);
                reply.U32(static_cast<uint32_t>(Entities.size()));

                for (const auto& [id, entity] : Entities)
                {
                    entity.Write(reply);
                }

                link.Joined = true;
            }
            else
            {
                link.CloseWhenFlushed = true;
            }

            Queue(link, FrameType::JoinReply, reply);
            return;
        }

        switch (type)
        {
            case FrameType::CreatePlayer:
            case FrameType::CreateGroup:
            {
                const uint32_t token = in.U32();
                Entity entity;
                entity.Read(in);
                entity.Type = type == FrameType::CreatePlayer ? DPPLAYERTYPE_PLAYER : DPPLAYERTYPE_GROUP;
                entity.Members.clear();
                uint32_t result = in.Ok ? DP_OK : DPERR_INVALIDPARAMS;
                uint32_t id = 0;

                if (result == DP_OK && entity.Type == DPPLAYERTYPE_PLAYER &&
                    ((Session.Desc.dwFlags & DPSESSION_NEWPLAYERSDISABLED) != 0 ||
                     PlayerCount() >= Session.Desc.dwMaxPlayers))
                {
                    result = DPERR_CANTCREATEPLAYER;
                }

                Writer reply;

                if (result == DP_OK)
                {
                    id = HostCreate(std::move(entity), index);
                }

                reply.U32(token);
                reply.U32(result);
                reply.U32(id);
                Queue(link, FrameType::CreateReply, reply);
                break;
            }

            case FrameType::AddToGroup:
            case FrameType::DeleteFromGroup:
            {
                const uint32_t group = in.U32();
                const uint32_t player = in.U32();

                if (in.Ok)
                {
                    HostMembership(type == FrameType::AddToGroup, group, player, index);
                }

                break;
            }

            case FrameType::SetGroupData:
            {
                const uint32_t group = in.U32();
                std::vector<uint8_t> data = in.Blob();

                if (in.Ok)
                {
                    HostSetGroupData(group, std::move(data), index);
                }

                break;
            }

            case FrameType::Data:
            {
                const uint32_t from = in.U32();
                const uint32_t to = in.U32();
                const std::vector<uint8_t> data = in.Blob();

                // A joiner may only speak for its own players.
                const auto sender = Entities.find(from);

                if (in.Ok && sender != Entities.end() && sender->second.Owner == index)
                {
                    Route(from, to, data);
                }

                break;
            }

            default:
            {
                break;
            }
        }
    }

    void HostPump()
    {
        MCSocket::Address from;

        for (;;)
        {
            const MCSocket::Handle socket = MCSocket::AcceptTcp(Listener, &from);

            if (socket == MCSocket::InvalidHandle)
            {
                break;
            }

            Link link;
            link.Socket = socket;
            Joiners.push_back(std::move(link));
        }

        for (size_t i = 0; i < Joiners.size(); i++)
        {
            if (Joiners[i].Socket == MCSocket::InvalidHandle)
            {
                continue;
            }

            Flush(Joiners[i]);
            ReadFrames(Joiners[i], [&](FrameType type, std::span<const uint8_t> payload)
                       { HostHandle(static_cast<int>(i), type, payload); });
            Flush(Joiners[i]);

            if (Joiners[i].Dead)
            {
                MCSocket::Close(Joiners[i].Socket);
                Joiners[i].Socket = MCSocket::InvalidHandle;
                Joiners[i].Joined = false;
                std::vector<uint32_t> owned;

                for (const auto& [id, entity] : Entities)
                {
                    if (entity.Type == DPPLAYERTYPE_PLAYER && entity.Owner == static_cast<int>(i))
                    {
                        owned.push_back(id);
                    }
                }

                for (uint32_t id : owned)
                {
                    HostDestroyPlayer(id);
                }
            }
        }

        AnswerQueries();
    }

    /// <summary>Host: answers the UDP session queries for this game.</summary>
    void AnswerQueries()
    {
        uint8_t datagram[1500];
        MCSocket::Address from;

        for (;;)
        {
            const int size = MCSocket::ReceiveFrom(Udp, datagram, sizeof(datagram), &from);

            if (size <= 0)
            {
                break;
            }

            Reader in(std::span<const uint8_t>(datagram, size));
            const uint32_t magic = in.U32();
            const uint8_t kind = in.U8();
            const _GUID application = in.Guid();

            if (!in.Ok || magic != ProtocolMagic || kind != DatagramQuery ||
                !MCSameGuid(application, Session.Desc.guidApplication))
            {
                continue;
            }

            Writer reply;
            reply.U32(ProtocolMagic);
            reply.U8(DatagramReply);
            reply.U16(Port);
            Session.Desc.dwCurrentPlayers = PlayerCount();
            Session.Write(reply, false);
            std::vector<const Entity*> players;

            for (const auto& [id, entity] : Entities)
            {
                if (entity.Type == DPPLAYERTYPE_PLAYER)
                {
                    players.push_back(&entity);
                }
            }

            reply.U32(static_cast<uint32_t>(players.size()));

            for (const Entity* player : players)
            {
                reply.U32(player->Id);
                reply.String(player->ShortName);
                reply.String(player->LongName);
            }

            if (reply.Bytes.size() <= sizeof(datagram))
            {
                MCSocket::SendTo(Udp, from, reply.Bytes.data(), reply.Bytes.size());
            }
        }
    }

    // ---- joiner --------------------------------------------------------------------------------------------------

    void JoinerHandle(FrameType type, std::span<const uint8_t> payload)
    {
        Reader in(payload);

        switch (type)
        {
            case FrameType::JoinReply:
            case FrameType::CreateReply:
            {
                Replies.emplace_back(type, std::vector<uint8_t>(payload.begin(), payload.end()));
                break;
            }

            case FrameType::Created:
            {
                Entity entity;
                entity.Read(in);

                if (in.Ok && !Entities.contains(entity.Id))
                {
                    entity.Owner = -1;
                    QueueCreated(entity);
                    Entities[entity.Id] = std::move(entity);
                }

                break;
            }

            case FrameType::Destroyed:
            {
                const uint32_t id = in.U32();
                std::optional<Entity> removed = in.Ok ? RemoveEntity(id) : std::nullopt;

                if (removed)
                {
                    for (auto& [groupID, group] : Entities)
                    {
                        std::erase(group.Members, id);
                    }

                    QueueDestroyed(*removed);
                }

                break;
            }

            case FrameType::AddToGroup:
            case FrameType::DeleteFromGroup:
            {
                const uint32_t group = in.U32();
                const uint32_t player = in.U32();
                const bool add = type == FrameType::AddToGroup;

                if (in.Ok && ApplyMembership(add, group, player))
                {
                    QueueMembership(add ? DPSYS_ADDPLAYERTOGROUP : DPSYS_DELETEPLAYERFROMGROUP, group, player);
                }

                break;
            }

            case FrameType::SetGroupData:
            {
                const uint32_t group = in.U32();
                std::vector<uint8_t> data = in.Blob();
                const auto found = Entities.find(group);

                if (in.Ok && found != Entities.end())
                {
                    found->second.Data = std::move(data);
                    Incoming message;
                    message.SystemType = DPSYS_SETPLAYERORGROUPDATA;
                    message.Subject = found->second;
                    QueueSystem(std::move(message));
                }

                break;
            }

            case FrameType::SessionDesc:
            {
                SessionInfo info;
                info.Read(in);

                if (in.Ok)
                {
                    Session = info;
                    Incoming message;
                    message.SystemType = DPSYS_SETSESSIONDESC;
                    message.Session = std::move(info);
                    QueueSystem(std::move(message));
                }

                break;
            }

            case FrameType::Data:
            {
                Incoming message;
                message.From = in.U32();
                message.To = in.U32();
                message.Payload = in.Blob();

                if (in.Ok)
                {
                    Inbox.push_back(std::move(message));
                }

                break;
            }

            default:
            {
                break;
            }
        }
    }

    void JoinerPump()
    {
        if (Server.Socket == MCSocket::InvalidHandle || Lost)
        {
            return;
        }

        Flush(Server);
        ReadFrames(Server, [&](FrameType type, std::span<const uint8_t> payload) { JoinerHandle(type, payload); });
        Flush(Server);

        if (Server.Dead)
        {
            Lost = true;
            Incoming message;
            message.SystemType = DPSYS_SESSIONLOST;
            QueueSystem(std::move(message));
        }
    }

    /// <summary>Joiner: pumps until the host answers with <paramref name="type"/> (and the token, for creates).</summary>
    std::optional<std::vector<uint8_t>> WaitForReply(FrameType type, std::optional<uint32_t> token)
    {
        const uint64_t deadline = NowMs() + ReplyTimeoutMs;

        for (;;)
        {
            for (auto it = Replies.begin(); it != Replies.end(); ++it)
            {
                if (it->first != type)
                {
                    continue;
                }

                if (token)
                {
                    Reader peek(it->second);

                    if (peek.U32() != *token)
                    {
                        continue;
                    }
                }

                std::vector<uint8_t> reply = std::move(it->second);
                Replies.erase(it);
                return reply;
            }

            if (Lost || Server.Dead || NowMs() > deadline)
            {
                return std::nullopt;
            }

            JoinerPump();
            SDL_Delay(1);
        }
    }

    // ---- session search ------------------------------------------------------------------------------------------

    void ReadAnswers()
    {
        if (Udp == MCSocket::InvalidHandle)
        {
            return;
        }

        uint8_t datagram[1500];
        MCSocket::Address from;

        for (;;)
        {
            const int size = MCSocket::ReceiveFrom(Udp, datagram, sizeof(datagram), &from);

            if (size <= 0)
            {
                break;
            }

            Reader in(std::span<const uint8_t>(datagram, size));
            const uint32_t magic = in.U32();
            const uint8_t kind = in.U8();

            if (magic != ProtocolMagic || kind != DatagramReply)
            {
                continue;
            }

            FoundSession found;
            found.Address = MCSocket::Address{from.Ip, in.U16()};
            found.Info.Read(in);
            const uint32_t count = in.U32();

            for (uint32_t i = 0; i < count && in.Ok; i++)
            {
                Entity player;
                player.Id = in.U32();
                player.ShortName = in.String();
                player.LongName = in.String();
                found.Players.push_back(std::move(player));
            }

            if (!in.Ok)
            {
                continue;
            }

            found.SeenAt = NowMs();
            const auto same = std::ranges::find_if(
                Found, [&](const FoundSession& session)
                { return MCSameGuid(session.Info.Desc.guidInstance, found.Info.Desc.guidInstance); });

            if (same != Found.end())
            {
                *same = std::move(found);
            }
            else
            {
                Found.push_back(std::move(found));
            }
        }
    }

    // ---- teardown ------------------------------------------------------------------------------------------------

    void Shutdown()
    {
        for (Link& link : Joiners)
        {
            MCSocket::Close(link.Socket);
        }

        Joiners.clear();
        MCSocket::Close(Server.Socket);
        Server = Link{};
        MCSocket::Close(Listener);
        Listener = MCSocket::InvalidHandle;

        if (State == Mode::Host)
        {
            // The host's UDP socket answered queries on the session's port; a searcher opens its own.
            MCSocket::Close(Udp);
            Udp = MCSocket::InvalidHandle;
        }

        State = Mode::None;
        Lost = false;
        Session = SessionInfo{};
        Entities.clear();
        LocalPlayers.clear();
        Inbox.clear();
        Replies.clear();
    }
};

MCDirectPlay::MCDirectPlay() : _Impl(std::make_unique<Impl>())
{
    MCSocket::Startup();
}

MCDirectPlay::~MCDirectPlay() = default;

uint32_t MCDirectPlay::CreateCompoundAddress(const DPCOMPOUNDADDRESSELEMENT* elements, uint32_t count, void* address,
                                             uint32_t* size)
{
    if (size == nullptr || (elements == nullptr && count != 0))
    {
        return DPERR_INVALIDPARAMS;
    }

    ConnectionData data{};

    for (uint32_t i = 0; i < count; i++)
    {
        const DPCOMPOUNDADDRESSELEMENT& element = elements[i];

        if (MCSameGuid(element.guidDataType, DPAID_ServiceProvider) && element.lpData != nullptr &&
            element.dwDataSize >= sizeof(_GUID))
        {
            std::memcpy(&data.serviceProvider, element.lpData, sizeof(_GUID));
        }
        else if (MCSameGuid(element.guidDataType, DPAID_INet) && element.lpData != nullptr)
        {
            const size_t length = std::min<size_t>(
                strnlen(static_cast<const char*>(element.lpData), element.dwDataSize), sizeof(data.address) - 1);
            std::memcpy(data.address, element.lpData, length);
        }
    }

    if (address == nullptr || *size < sizeof(ConnectionData))
    {
        *size = sizeof(ConnectionData);
        return DPERR_BUFFERTOOSMALL;
    }

    std::memcpy(address, &data, sizeof(ConnectionData));
    *size = sizeof(ConnectionData);
    return DP_OK;
}

uint32_t MCDirectPlay::EnumConnections(const _GUID*, MCEnumConnectionsCallback callback, void* context, uint32_t)
{
    if (callback == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    struct Offer
    {
        const _GUID* provider;
        const char* name;
    };

    // The port offers its one transport twice: as TCP/IP (a typed-in host, or the LAN) and as IPX (the LAN).
    static const Offer offers[] = {{&DPSPGUID_TCPIP, "Internet TCP/IP Connection For DirectPlay"},
                                   {&DPSPGUID_IPX, "IPX Connection For DirectPlay"}};

    for (const Offer& offer : offers)
    {
        ConnectionData data{};
        data.serviceProvider = *offer.provider;
        DPNAME name{sizeof(DPNAME), 0, const_cast<char*>(offer.name), const_cast<char*>(offer.name)};

        if (callback(offer.provider, &data, sizeof(data), &name, 0, context) == 0)
        {
            break;
        }
    }

    return DP_OK;
}

uint32_t MCDirectPlay::InitializeConnection(const void* connection, uint32_t)
{
    if (connection == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if (_Impl->Initialized)
    {
        return DPERR_ALREADYINITIALIZED;
    }

    ConnectionData data;
    std::memcpy(&data, connection, sizeof(data));
    data.address[sizeof(data.address) - 1] = '\0';

    if (!MCSameGuid(data.serviceProvider, DPSPGUID_TCPIP) && !MCSameGuid(data.serviceProvider, DPSPGUID_IPX))
    {
        return DPERR_UNSUPPORTED;
    }

    const auto [host, port] = SplitAddress(data.address);
    _Impl->HostName = host;
    _Impl->Port = port;
    _Impl->Initialized = true;
    return DP_OK;
}

uint32_t MCDirectPlay::EnumSessions(const DPSESSIONDESC2* desc, uint32_t, MCEnumSessionsCallback callback,
                                    void* context, uint32_t flags)
{
    Impl& impl = *_Impl;

    if (!impl.Initialized)
    {
        return DPERR_UNINITIALIZED;
    }

    if (desc == nullptr || callback == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if (impl.Udp == MCSocket::InvalidHandle)
    {
        impl.Udp = MCSocket::OpenUdp(0);
    }

    if (impl.Udp != MCSocket::InvalidHandle && impl.State != Impl::Mode::Host)
    {
        Writer query;
        query.U32(ProtocolMagic);
        query.U8(DatagramQuery);
        query.Guid(desc->guidApplication);

        if (!impl.HostName.empty())
        {
            if (const std::optional<uint32_t> ip = MCSocket::Resolve(impl.HostName))
            {
                MCSocket::SendTo(impl.Udp, {*ip, impl.Port}, query.Bytes.data(), query.Bytes.size());
            }
        }
        else
        {
            MCSocket::SendTo(impl.Udp, {MCSocket::BroadcastIp, impl.Port}, query.Bytes.data(), query.Bytes.size());
            MCSocket::SendTo(impl.Udp, {MCSocket::LoopbackIp, impl.Port}, query.Bytes.data(), query.Bytes.size());
        }

        const uint64_t until = NowMs() + EnumListenMs;

        while (NowMs() < until)
        {
            impl.ReadAnswers();
            SDL_Delay(5);
        }

        impl.ReadAnswers();
    }

    const uint64_t now = NowMs();
    std::erase_if(impl.Found, [&](const FoundSession& found) { return now - found.SeenAt > SessionExpiryMs; });

    for (FoundSession& found : impl.Found)
    {
        const DPSESSIONDESC2& info = found.Info.Desc;

        if (!MCSameGuid(info.guidApplication, desc->guidApplication))
        {
            continue;
        }

        if ((flags & DPENUMSESSIONS_AVAILABLE) != 0 &&
            ((info.dwFlags & (DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED)) != 0 ||
             info.dwCurrentPlayers >= info.dwMaxPlayers))
        {
            continue;
        }

        if ((info.dwFlags & DPSESSION_PASSWORDREQUIRED) != 0 && (flags & DPENUMSESSIONS_PASSWORDREQUIRED) == 0)
        {
            continue;
        }

        DPSESSIONDESC2 view = found.Info.View();
        uint32_t timeout = 0;

        if (callback(&view, &timeout, 0, context) == 0)
        {
            return DP_OK;
        }
    }

    uint32_t timeout = 0;
    callback(nullptr, &timeout, DPESC_TIMEDOUT, context);
    return DP_OK;
}

uint32_t MCDirectPlay::Open(const DPSESSIONDESC2* desc, uint32_t flags)
{
    Impl& impl = *_Impl;

    if (!impl.Initialized)
    {
        return DPERR_UNINITIALIZED;
    }

    if (desc == nullptr || impl.State != Impl::Mode::None)
    {
        return DPERR_INVALIDPARAMS;
    }

    if ((flags & DPOPEN_CREATE) != 0)
    {
        impl.Listener = MCSocket::ListenTcp(impl.Port);
        MCSocket::Close(impl.Udp);
        impl.Udp = MCSocket::OpenUdp(impl.Port);

        if (impl.Listener == MCSocket::InvalidHandle || impl.Udp == MCSocket::InvalidHandle)
        {
            SDL_Log("MCDirectPlay: cannot host on port %u", impl.Port);
            impl.State = Impl::Mode::Host;
            impl.Shutdown();
            return DPERR_CANTCREATESESSION;
        }

        impl.Session.Set(*desc);
        impl.Session.Desc.guidInstance = NewGuid();

        if (impl.Session.Desc.dwMaxPlayers == 0)
        {
            impl.Session.Desc.dwMaxPlayers = 0xffffffffu;
        }

        // Ids start somewhere unremarkable, as DirectPlay's did; 0 (everyone) and 1 (the server player) are never
        // given out.
        impl.NextId = 0x00010000u + (NewGuid().Data1 & 0x0fff0000u);
        impl.State = Impl::Mode::Host;
        SDL_Log("MCDirectPlay: hosting \"%s\" on port %u", impl.Session.Name.c_str(), impl.Port);
        return DP_OK;
    }

    if ((flags & DPOPEN_JOIN) == 0)
    {
        return DPERR_INVALIDPARAMS;
    }

    const auto found = std::ranges::find_if(impl.Found, [&](const FoundSession& session)
                                            { return MCSameGuid(session.Info.Desc.guidInstance, desc->guidInstance); });

    if (found == impl.Found.end())
    {
        return DPERR_NOSESSIONS;
    }

    impl.Server = Link{};
    impl.Server.Socket = MCSocket::ConnectTcp(found->Address, ConnectTimeoutMs);

    if (impl.Server.Socket == MCSocket::InvalidHandle)
    {
        SDL_Log("MCDirectPlay: cannot reach %s", MCSocket::ToString(found->Address).c_str());
        return DPERR_NOCONNECTION;
    }

    impl.State = Impl::Mode::Joiner;
    Writer request;
    request.U32(ProtocolMagic);
    request.Guid(desc->guidApplication);
    request.Guid(desc->guidInstance);
    request.String(desc->lpszPasswordA != nullptr ? desc->lpszPasswordA : "");
    Impl::Queue(impl.Server, FrameType::JoinRequest, request);
    std::optional<std::vector<uint8_t>> reply = impl.WaitForReply(FrameType::JoinReply, std::nullopt);
    uint32_t result = DPERR_TIMEOUT;

    if (reply)
    {
        Reader in(*reply);
        result = in.U32();

        if (result == DP_OK)
        {
            impl.Session.Read(in);
            const uint32_t count = in.U32();

            for (uint32_t i = 0; i < count && in.Ok; i++)
            {
                Entity entity;
                entity.Read(in);
                entity.Owner = -1;
                impl.Entities[entity.Id] = std::move(entity);
            }

            if (!in.Ok)
            {
                result = DPERR_GENERIC;
            }
        }
    }

    if (result != DP_OK)
    {
        impl.Shutdown();
        return result;
    }

    SDL_Log("MCDirectPlay: joined \"%s\" at %s", impl.Session.Name.c_str(), MCSocket::ToString(found->Address).c_str());
    return DP_OK;
}

uint32_t MCDirectPlay::Close()
{
    if (_Impl->State == Impl::Mode::None)
    {
        return DPERR_NOSESSIONS;
    }

    _Impl->Shutdown();
    return DP_OK;
}

uint32_t MCDirectPlay::CreatePlayer(uint32_t* playerID, const DPNAME* name, void*, const void* data, uint32_t dataSize,
                                    uint32_t flags)
{
    Impl& impl = *_Impl;

    if (playerID == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    Entity entity;
    entity.Type = DPPLAYERTYPE_PLAYER;
    entity.Flags = flags;
    entity.SetName(name);

    if (data != nullptr && dataSize != 0)
    {
        entity.Data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + dataSize);
    }

    if (impl.State == Impl::Mode::Host)
    {
        if (impl.PlayerCount() >= impl.Session.Desc.dwMaxPlayers)
        {
            return DPERR_CANTCREATEPLAYER;
        }

        *playerID = impl.HostCreate(std::move(entity), -1);
        impl.LocalPlayers.insert(*playerID);
        return DP_OK;
    }

    if (impl.State != Impl::Mode::Joiner || impl.Lost)
    {
        return DPERR_INVALIDOBJECT;
    }

    const uint32_t token = impl.NextToken++;
    Writer request;
    request.U32(token);
    entity.Write(request);
    Impl::Queue(impl.Server, FrameType::CreatePlayer, request);
    std::optional<std::vector<uint8_t>> reply = impl.WaitForReply(FrameType::CreateReply, token);

    if (!reply)
    {
        return DPERR_TIMEOUT;
    }

    Reader in(*reply);
    in.U32();
    const uint32_t result = in.U32();
    const uint32_t id = in.U32();

    if (result != DP_OK)
    {
        return result;
    }

    entity.Id = id;
    impl.Entities[id] = std::move(entity);
    impl.LocalPlayers.insert(id);
    *playerID = id;
    return DP_OK;
}

uint32_t MCDirectPlay::CreateGroup(uint32_t* groupID, const DPNAME* name, const void* data, uint32_t dataSize,
                                   uint32_t flags)
{
    Impl& impl = *_Impl;

    if (groupID == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    Entity entity;
    entity.Type = DPPLAYERTYPE_GROUP;
    entity.Flags = flags;
    entity.SetName(name);

    if (data != nullptr && dataSize != 0)
    {
        entity.Data.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + dataSize);
    }

    if (impl.State == Impl::Mode::Host)
    {
        *groupID = impl.HostCreate(std::move(entity), -1);
        return DP_OK;
    }

    if (impl.State != Impl::Mode::Joiner || impl.Lost)
    {
        return DPERR_INVALIDOBJECT;
    }

    const uint32_t token = impl.NextToken++;
    Writer request;
    request.U32(token);
    entity.Write(request);
    Impl::Queue(impl.Server, FrameType::CreateGroup, request);
    std::optional<std::vector<uint8_t>> reply = impl.WaitForReply(FrameType::CreateReply, token);

    if (!reply)
    {
        return DPERR_TIMEOUT;
    }

    Reader in(*reply);
    in.U32();
    const uint32_t result = in.U32();
    const uint32_t id = in.U32();

    if (result != DP_OK)
    {
        return DPERR_CANTCREATEGROUP;
    }

    entity.Id = id;
    impl.Entities[id] = std::move(entity);
    *groupID = id;
    return DP_OK;
}

uint32_t MCDirectPlay::AddPlayerToGroup(uint32_t groupID, uint32_t playerID)
{
    Impl& impl = *_Impl;

    if (!impl.Entities.contains(groupID))
    {
        return DPERR_INVALIDGROUP;
    }

    if (!impl.Entities.contains(playerID))
    {
        return DPERR_INVALIDPLAYER;
    }

    if (impl.State == Impl::Mode::Host)
    {
        impl.HostMembership(true, groupID, playerID, -1);
        return DP_OK;
    }

    if (impl.State != Impl::Mode::Joiner || impl.Lost)
    {
        return DPERR_INVALIDOBJECT;
    }

    impl.ApplyMembership(true, groupID, playerID);
    Writer request;
    request.U32(groupID);
    request.U32(playerID);
    Impl::Queue(impl.Server, FrameType::AddToGroup, request);
    return DP_OK;
}

uint32_t MCDirectPlay::DeletePlayerFromGroup(uint32_t groupID, uint32_t playerID)
{
    Impl& impl = *_Impl;

    if (!impl.Entities.contains(groupID))
    {
        return DPERR_INVALIDGROUP;
    }

    if (!impl.Entities.contains(playerID))
    {
        return DPERR_INVALIDPLAYER;
    }

    if (impl.State == Impl::Mode::Host)
    {
        impl.HostMembership(false, groupID, playerID, -1);
        return DP_OK;
    }

    if (impl.State != Impl::Mode::Joiner || impl.Lost)
    {
        return DPERR_INVALIDOBJECT;
    }

    impl.ApplyMembership(false, groupID, playerID);
    Writer request;
    request.U32(groupID);
    request.U32(playerID);
    Impl::Queue(impl.Server, FrameType::DeleteFromGroup, request);
    return DP_OK;
}

uint32_t MCDirectPlay::SetGroupData(uint32_t groupID, const void* data, uint32_t dataSize, uint32_t)
{
    Impl& impl = *_Impl;
    const auto found = impl.Entities.find(groupID);

    if (found == impl.Entities.end())
    {
        return DPERR_INVALIDGROUP;
    }

    std::vector<uint8_t> bytes;

    if (data != nullptr && dataSize != 0)
    {
        bytes.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + dataSize);
    }

    if (impl.State == Impl::Mode::Host)
    {
        impl.HostSetGroupData(groupID, std::move(bytes), -1);
        return DP_OK;
    }

    if (impl.State != Impl::Mode::Joiner || impl.Lost)
    {
        return DPERR_INVALIDOBJECT;
    }

    found->second.Data = bytes;
    Writer request;
    request.U32(groupID);
    request.Blob(bytes);
    Impl::Queue(impl.Server, FrameType::SetGroupData, request);
    return DP_OK;
}

uint32_t MCDirectPlay::SetSessionDesc(const DPSESSIONDESC2* desc, uint32_t)
{
    Impl& impl = *_Impl;

    if (desc == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if (impl.State != Impl::Mode::Host)
    {
        return impl.State == Impl::Mode::None ? DPERR_NOSESSIONS : DPERR_ACCESSDENIED;
    }

    const _GUID instance = impl.Session.Desc.guidInstance;
    const _GUID application = impl.Session.Desc.guidApplication;
    const std::string password = impl.Session.Password;
    impl.Session.Set(*desc);
    impl.Session.Desc.guidInstance = instance;
    impl.Session.Desc.guidApplication = application;

    if (desc->lpszPasswordA == nullptr)
    {
        impl.Session.Password = password;
    }

    impl.Session.Desc.dwCurrentPlayers = impl.PlayerCount();
    Writer note;
    impl.Session.Write(note, false);
    impl.Broadcast(FrameType::SessionDesc, note, -1);
    return DP_OK;
}

uint32_t MCDirectPlay::EnumPlayers(const _GUID* session, MCEnumPlayersCallback callback, void* context, uint32_t flags)
{
    Impl& impl = *_Impl;

    if (callback == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if ((flags & DPENUMPLAYERS_SESSION) != 0)
    {
        if (session == nullptr)
        {
            return DPERR_INVALIDPARAMS;
        }

        const auto found = std::ranges::find_if(impl.Found, [&](const FoundSession& candidate)
                                                { return MCSameGuid(candidate.Info.Desc.guidInstance, *session); });

        if (found == impl.Found.end())
        {
            return DPERR_NOSESSIONS;
        }

        for (Entity& player : found->Players)
        {
            DPNAME name{sizeof(DPNAME), 0, player.ShortName.data(), player.LongName.data()};

            if (callback(player.Id, DPPLAYERTYPE_PLAYER, &name, 0, context) == 0)
            {
                break;
            }
        }

        return DP_OK;
    }

    if (impl.State == Impl::Mode::None)
    {
        return DPERR_NOSESSIONS;
    }

    // Copied first: a callback may change the session.
    std::vector<Entity> players;

    for (const auto& [id, entity] : impl.Entities)
    {
        if (entity.Type == DPPLAYERTYPE_PLAYER)
        {
            players.push_back(entity);
        }
    }

    for (Entity& player : players)
    {
        DPNAME name{sizeof(DPNAME), 0, player.ShortName.data(), player.LongName.data()};

        if (callback(player.Id, DPPLAYERTYPE_PLAYER, &name, player.Flags, context) == 0)
        {
            break;
        }
    }

    return DP_OK;
}

uint32_t MCDirectPlay::EnumGroups(const _GUID*, MCEnumPlayersCallback callback, void* context, uint32_t)
{
    Impl& impl = *_Impl;

    if (callback == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if (impl.State == Impl::Mode::None)
    {
        return DPERR_NOSESSIONS;
    }

    std::vector<Entity> groups;

    for (const auto& [id, entity] : impl.Entities)
    {
        if (entity.Type == DPPLAYERTYPE_GROUP)
        {
            groups.push_back(entity);
        }
    }

    for (Entity& group : groups)
    {
        DPNAME name{sizeof(DPNAME), 0, group.ShortName.data(), group.LongName.data()};

        if (callback(group.Id, DPPLAYERTYPE_GROUP, &name, group.Flags, context) == 0)
        {
            break;
        }
    }

    return DP_OK;
}

uint32_t MCDirectPlay::Send(uint32_t fromID, uint32_t toID, uint32_t, const void* data, uint32_t size)
{
    Impl& impl = *_Impl;

    if (impl.State == Impl::Mode::None)
    {
        return DPERR_NOSESSIONS;
    }

    if (impl.Lost)
    {
        return DPERR_SESSIONLOST;
    }

    if (!impl.LocalPlayers.contains(fromID))
    {
        return DPERR_INVALIDPLAYER;
    }

    if (toID != DPID_ALLPLAYERS && !impl.Entities.contains(toID))
    {
        return DPERR_INVALIDPLAYER;
    }

    const std::span<const uint8_t> payload(static_cast<const uint8_t*>(data), data != nullptr ? size : 0);

    if (impl.State == Impl::Mode::Host)
    {
        impl.Route(fromID, toID, payload);
        return DP_OK;
    }

    Writer frame;
    frame.U32(fromID);
    frame.U32(toID);
    frame.Blob(payload);
    Impl::Queue(impl.Server, FrameType::Data, frame);
    return DP_OK;
}

uint32_t MCDirectPlay::Receive(uint32_t* fromID, uint32_t* toID, uint32_t, void* data, uint32_t* size)
{
    Impl& impl = *_Impl;

    if (size == nullptr || fromID == nullptr || toID == nullptr)
    {
        return DPERR_INVALIDPARAMS;
    }

    if (impl.Inbox.empty())
    {
        Pump();
    }

    if (impl.Inbox.empty())
    {
        return DPERR_NOMESSAGES;
    }

    Incoming& message = impl.Inbox.front();

    if (message.SystemType == 0)
    {
        const uint32_t needed = static_cast<uint32_t>(message.Payload.size());

        if (data == nullptr || *size < needed)
        {
            *size = needed;
            return DPERR_BUFFERTOOSMALL;
        }

        std::memcpy(data, message.Payload.data(), needed);
        *size = needed;
        *fromID = message.From;
        *toID = message.To;
        impl.Inbox.pop_front();
        return DP_OK;
    }

    // A system message: the DPMSG_* structure first, the strings and data it points at after it, all inside the
    // caller's buffer (as DirectPlay laid them out).
    std::vector<uint8_t> image;
    std::vector<std::pair<size_t, size_t>> fixups; // (offset of a pointer field, offset of what it points at)
    const Entity& subject = message.Subject;

    auto appendTail = [&](size_t pointerOffset, const void* bytes, size_t count, bool terminate)
    {
        const size_t at = image.size();
        image.insert(image.end(), static_cast<const uint8_t*>(bytes), static_cast<const uint8_t*>(bytes) + count);

        if (terminate)
        {
            image.push_back(0);
        }

        fixups.emplace_back(pointerOffset, at);
    };

    switch (message.SystemType)
    {
        case DPSYS_CREATEPLAYERORGROUP:
        {
            DPMSG_CREATEPLAYERORGROUP msg{};
            msg.dwType = message.SystemType;
            msg.dwPlayerType = subject.Type;
            msg.dpId = subject.Id;
            msg.dwCurrentPlayers = impl.PlayerCount();
            msg.dwDataSize = static_cast<uint32_t>(subject.Data.size());
            msg.dpnName.dwSize = sizeof(DPNAME);
            msg.dpIdParent = subject.Parent;
            msg.dwFlags = subject.Flags;
            image.assign(reinterpret_cast<uint8_t*>(&msg), reinterpret_cast<uint8_t*>(&msg) + sizeof(msg));
            appendTail(offsetof(DPMSG_CREATEPLAYERORGROUP, dpnName.lpszShortNameA), subject.ShortName.data(),
                       subject.ShortName.size(), true);
            appendTail(offsetof(DPMSG_CREATEPLAYERORGROUP, dpnName.lpszLongNameA), subject.LongName.data(),
                       subject.LongName.size(), true);

            if (!subject.Data.empty())
            {
                appendTail(offsetof(DPMSG_CREATEPLAYERORGROUP, lpData), subject.Data.data(), subject.Data.size(),
                           false);
            }

            break;
        }

        case DPSYS_DESTROYPLAYERORGROUP:
        {
            DPMSG_DESTROYPLAYERORGROUP msg{};
            msg.dwType = message.SystemType;
            msg.dwPlayerType = subject.Type;
            msg.dpId = subject.Id;
            msg.dpnName.dwSize = sizeof(DPNAME);
            msg.dpIdParent = subject.Parent;
            msg.dwFlags = subject.Flags;
            image.assign(reinterpret_cast<uint8_t*>(&msg), reinterpret_cast<uint8_t*>(&msg) + sizeof(msg));
            appendTail(offsetof(DPMSG_DESTROYPLAYERORGROUP, dpnName.lpszShortNameA), subject.ShortName.data(),
                       subject.ShortName.size(), true);
            appendTail(offsetof(DPMSG_DESTROYPLAYERORGROUP, dpnName.lpszLongNameA), subject.LongName.data(),
                       subject.LongName.size(), true);
            break;
        }

        case DPSYS_ADDPLAYERTOGROUP:
        case DPSYS_DELETEPLAYERFROMGROUP:
        {
            const DPMSG_ADDPLAYERTOGROUP msg{message.SystemType, message.Group, message.Player};
            image.assign(reinterpret_cast<const uint8_t*>(&msg), reinterpret_cast<const uint8_t*>(&msg) + sizeof(msg));
            break;
        }

        case DPSYS_SETPLAYERORGROUPDATA:
        {
            DPMSG_SETPLAYERORGROUPDATA msg{};
            msg.dwType = message.SystemType;
            msg.dwPlayerType = subject.Type;
            msg.dpId = subject.Id;
            msg.dwDataSize = static_cast<uint32_t>(subject.Data.size());
            image.assign(reinterpret_cast<uint8_t*>(&msg), reinterpret_cast<uint8_t*>(&msg) + sizeof(msg));

            if (!subject.Data.empty())
            {
                appendTail(offsetof(DPMSG_SETPLAYERORGROUPDATA, lpData), subject.Data.data(), subject.Data.size(),
                           false);
            }

            break;
        }

        case DPSYS_SETSESSIONDESC:
        {
            DPMSG_SETSESSIONDESC msg{};
            msg.dwType = message.SystemType;
            msg.dpDesc = message.Session.Desc;
            msg.dpDesc.lpszSessionNameA = nullptr;
            msg.dpDesc.lpszPasswordA = nullptr;
            image.assign(reinterpret_cast<uint8_t*>(&msg), reinterpret_cast<uint8_t*>(&msg) + sizeof(msg));
            appendTail(offsetof(DPMSG_SETSESSIONDESC, dpDesc.lpszSessionNameA), message.Session.Name.data(),
                       message.Session.Name.size(), true);
            break;
        }

        default:
        {
            const DPMSG_GENERIC msg{message.SystemType};
            image.assign(reinterpret_cast<const uint8_t*>(&msg), reinterpret_cast<const uint8_t*>(&msg) + sizeof(msg));
            break;
        }
    }

    const uint32_t needed = static_cast<uint32_t>(image.size());

    if (data == nullptr || *size < needed)
    {
        *size = needed;
        return DPERR_BUFFERTOOSMALL;
    }

    uint8_t* out = static_cast<uint8_t*>(data);
    std::memcpy(out, image.data(), needed);

    for (const auto& [pointerOffset, targetOffset] : fixups)
    {
        uint8_t* target = out + targetOffset;
        std::memcpy(out + pointerOffset, &target, sizeof(target));
    }

    *size = needed;
    *fromID = DPID_SYSMSG;
    *toID = message.To;
    impl.Inbox.pop_front();
    return DP_OK;
}

void MCDirectPlay::Pump()
{
    switch (_Impl->State)
    {
        case Impl::Mode::Host:
        {
            _Impl->HostPump();
            break;
        }

        case Impl::Mode::Joiner:
        {
            _Impl->JoinerPump();
            break;
        }

        default:
        {
            _Impl->ReadAnswers();
            break;
        }
    }
}
