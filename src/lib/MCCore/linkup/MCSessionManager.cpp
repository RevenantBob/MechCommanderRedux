#include "stdafx.h"
#include "linkup/MCSessionManager.h"
#include "linkup/MCFidpGroup.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCFidpSession.h"
#include "linkup/MCFileTransferInfo.h"
#include "lib/MCFatal.h"
#include "platform/MCDirectPlay.h"
#include "platform/MCSocket.h"

// The session manager's life, its connections, and the sessions, players and groups DirectPlay lists. The receive
// pass and guaranteed delivery are in MCSessionManagerReceive.cpp, the sends in MCSessionManagerSend.cpp, the players
// coming and going and the message handlers in MCSessionManagerPlayers.cpp.

bool LaunchedFromLobby = false;

namespace
{
    using namespace MCDirectPlayGuids;

    /// <summary>The enumeration flags GetSessions and Dial ask DirectPlay for.</summary>
    constexpr uint32_t EnumSessionFlags =
        DPENUMSESSIONS_AVAILABLE | DPENUMSESSIONS_ASYNC | DPENUMSESSIONS_PASSWORDREQUIRED | DPENUMSESSIONS_RETURNSTATUS;
}

uint32_t MCSessionManager::TicksPerMs()
{
    return static_cast<uint32_t>(MCPort::PerformanceFrequency()) / 1000;
}

// ---- lifetime ------------------------------------------------------------------------------------------------------

MCSessionManager::MCSessionManager(const _GUID& appGUID) : _AppGuid(appGUID)
{
    CreateDirectPlayInterface();
    _ServerMessage.Header = LinkupHeader(MCLinkupMessageType::NewServer, FIMSG_GUARANTEED);
    _NextPingTime = static_cast<uint32_t>(MCPort::PerformanceCounter());
}

MCSessionManager::~MCSessionManager()
{
    std::scoped_lock lock(CriticalSection);
    DestroyDirectPlayInterface();
}

void MCSessionManager::CreateDirectPlayInterface()
{
    // Port: CoCreateInstance(CLSID_DirectPlay, IID_IDirectPlay3A) becomes the port's stand-in, which can't fail.
    if (DirectPlay == nullptr)
    {
        DirectPlay = std::make_unique<MCDirectPlay>();
    }

    EnumerateConnections();
}

void MCSessionManager::DestroyDirectPlayInterface()
{
    LeaveSession();
    DirectPlay.reset();
    CurrentConnection = MCNetProtocol::None;
}

// ---- message queues ------------------------------------------------------------------------------------------------

void MCSessionManager::AddMessageToEmptyQueue(MCFidpMessage* msg)
{
    std::scoped_lock lock(_EmptyQueueLock);
    msg->Clear();
    _EmptyMessages.push_back(msg);
}

MCFidpMessage* MCSessionManager::GetMessageFromEmptyQueue()
{
    std::scoped_lock lock(_EmptyQueueLock);

    // Port fix: the pool grows when it is used up (the original's senders went on with the null they got).
    if (_EmptyMessages.empty())
    {
        return _MessagePool.emplace_back(std::make_unique<MCFidpMessage>(MyPlayerID, 0x200)).get();
    }

    MCFidpMessage* msg = _EmptyMessages.front();
    _EmptyMessages.pop_front();
    return msg;
}

MCFidpMessage* MCSessionManager::CopyToFreeMessage(const MCFIMessageHeader* header, uint32_t size, uint32_t toID)
{
    MCFidpMessage* msg = GetMessageFromEmptyQueue();
    msg->SetMessageBuffer(header, size);
    msg->ToID = toID;
    return msg;
}

// ---- connections ---------------------------------------------------------------------------------------------------

void MCSessionManager::EnumerateConnections()
{
    Assert(DirectPlay != nullptr, 0);
    Connections.clear();
    MCDirectPlay::EnumConnections(
        &_AppGuid,
        [](const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name, uint32_t,
           void* context)
        {
            if (serviceProvider == nullptr)
            {
                return 0;
            }

            static_cast<MCSessionManager*>(context)->AddConnection(
                *serviceProvider, std::span(static_cast<const uint8_t*>(connection), connectionSize), *name);
            return 1;
        },
        this, 0);
}

MCNetProtocol MCSessionManager::ConnectionType(const _GUID& guid)
{
    constexpr std::pair<const _GUID*, MCNetProtocol> known[] = {{&DPSPGUID_TCPIP, MCNetProtocol::TcpIp},
                                                                {&DPSPGUID_IPX, MCNetProtocol::Ipx},
                                                                {&DPSPGUID_MODEM, MCNetProtocol::Modem},
                                                                {&DPSPGUID_SERIAL, MCNetProtocol::Serial},
                                                                {&DPSPGUID_LOBBY, MCNetProtocol::Lobby}};

    for (const auto& [provider, protocol] : known)
    {
        if (MCSameGuid(guid, *provider))
        {
            AvailableProtocols |= ProtocolBit(protocol);
            return protocol;
        }
    }

    return MCNetProtocol::None;
}

void MCSessionManager::AddConnection(const _GUID& serviceProvider, std::span<const uint8_t> connection,
                                     const DPNAME& name)
{
    auto protocol = std::make_unique<MCFidpNetworkProtocol>(name.lpszShortNameA, name.lpszLongNameA, connection);
    protocol->ProtocolType = ConnectionType(serviceProvider);
    Connections.push_back(std::move(protocol));
}

int32_t MCSessionManager::SetCurrentConnection(MCNetProtocol type)
{
    const auto found =
        std::ranges::find_if(Connections, [&](const auto& protocol) { return protocol->ProtocolType == type; });
    Assert(found != Connections.end(), 0);

    if (found == Connections.end())
    {
        return -1;
    }

    // Port fix: recreating the DirectPlay object below re-enumerates the connections, which frees the data just found
    // (the original went on using it); a copy is kept.
    std::vector<uint8_t> copy = (*found)->ConnectionBuffer;
    copy.resize(std::max<size_t>(copy.size(), MCDirectPlay::ConnectionDataSize));

    if (CurrentConnection != MCNetProtocol::None)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    const int32_t result = static_cast<int32_t>(DirectPlay->InitializeConnection(copy.data(), 0));

    if (type != MCNetProtocol::Modem && result == 0)
    {
        GetSessions();
    }

    CurrentConnection = type;
    return result;
}

void MCSessionManager::ConnectIpx()
{
    SetCurrentConnection(MCNetProtocol::Ipx);
}

void MCSessionManager::ConnectTcp(std::string_view ipAddress)
{
    if (CurrentConnection == MCNetProtocol::TcpIp)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    std::string address(ipAddress);
    const std::array<DPCOMPOUNDADDRESSELEMENT, 2> elements = {
        {{DPAID_ServiceProvider, sizeof(_GUID), const_cast<_GUID*>(&DPSPGUID_TCPIP)},
         {DPAID_INet, static_cast<uint32_t>(address.size() + 1), address.data()}}};

    if (InitializeConnection(elements) == 0)
    {
        CurrentConnection = MCNetProtocol::TcpIp;
    }
}

int32_t MCSessionManager::ConnectModem(std::string_view phoneNumber, std::string_view modemName)
{
    std::string phone(phoneNumber);
    std::string modem(modemName);
    std::vector<DPCOMPOUNDADDRESSELEMENT> elements{
        {DPAID_ServiceProvider, sizeof(_GUID), const_cast<_GUID*>(&DPSPGUID_MODEM)}};

    if (modem.size() > 1)
    {
        elements.push_back({DPAID_Modem, static_cast<uint32_t>(modem.size() + 1), modem.data()});
    }

    elements.push_back({DPAID_Phone, static_cast<uint32_t>(phone.size() + 1), phone.data()});
    const int32_t result = InitializeConnection(elements);

    if (result == 0)
    {
        CurrentConnection = MCNetProtocol::Modem;
    }

    return result;
}

int32_t MCSessionManager::ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity,
                                         uint32_t flowControl)
{
    std::array<uint32_t, 5> settings = {port, baudRate, stopBits, parity, flowControl};
    const std::array<DPCOMPOUNDADDRESSELEMENT, 2> elements = {
        {{DPAID_ServiceProvider, sizeof(_GUID), const_cast<_GUID*>(&DPSPGUID_SERIAL)},
         {DPAID_ComPort, static_cast<uint32_t>(sizeof(settings)), settings.data()}}};
    const int32_t result = InitializeConnection(elements);

    if (result == 0)
    {
        CurrentConnection = MCNetProtocol::Serial;
        GetSessions();
    }

    return result;
}

int32_t MCSessionManager::InitializeConnection(std::span<const DPCOMPOUNDADDRESSELEMENT> elements)
{
    if (CurrentConnection != MCNetProtocol::None)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    // Port: the original made a DirectPlay lobby object only to build the address, which the stand-in does itself.
    const auto count = static_cast<int>(elements.size());
    auto* first = const_cast<DPCOMPOUNDADDRESSELEMENT*>(elements.data());
    uint32_t size = 0;
    MCDirectPlay::CreateCompoundAddress(first, count, nullptr, &size);
    std::vector<uint8_t> address(size);
    uint32_t result = MCDirectPlay::CreateCompoundAddress(first, count, address.data(), &size);

    if (result == DP_OK)
    {
        result = DirectPlay->InitializeConnection(address.data(), 0);
    }

    return static_cast<int32_t>(result);
}

bool MCSessionManager::FindModems()
{
    if (CurrentSession != nullptr || ConnectModem("", "") != 0)
    {
        return false;
    }

    // Port: not reached. MCDirectPlay has no modem service provider, so ConnectModem always fails above. The original
    // went on to read the modem connection's address and walk it with the lobby's EnumAddress, collecting the names.
    _ModemNames.clear();
    return false;
}

const char* MCSessionManager::GetModemName(int32_t index) const
{
    return index >= 0 && static_cast<size_t>(index) < _ModemNames.size() ? _ModemNames[index].c_str() : nullptr;
}

bool MCSessionManager::WasLaunchedFromLobby()
{
    // Port: no DirectPlay lobby launches the game; GetConnectionSettings answered DPERR_NOTLOBBIED.
    return false;
}

uint32_t MCSessionManager::SetupLobbyConnection(const std::function<void()>&, const std::function<void()>&)
{
    // Port: the original dropped its DirectPlay object, asked the lobby for the connection it was launched with
    // (Connect, showing the status while it waited), and joined or hosted that session. Without a lobby
    // GetConnectionSettings answers DPERR_NOTLOBBIED, and the original then made a new DirectPlay object without
    // enumerating its connections; so does the port.
    if (DirectPlay != nullptr)
    {
        DirectPlay->Close();
    }

    DirectPlay = std::make_unique<MCDirectPlay>();
    return DPERR_NOTLOBBIED;
}

int32_t MCSessionManager::Dial()
{
    if (CurrentConnection != MCNetProtocol::Modem)
    {
        return 1;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc{};
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = _AppGuid;
    return static_cast<int32_t>(DirectPlay->EnumSessions(
        &desc, 0,
        [](const DPSESSIONDESC2* session, uint32_t*, uint32_t flags, void* context)
        {
            if ((flags & DPESC_TIMEDOUT) != 0)
            {
                return 0;
            }

            static_cast<MCSessionManager*>(context)->AddSession(*session);
            return 1;
        },
        this, EnumSessionFlags));
}

void MCSessionManager::CancelDialing()
{
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();
}

bool MCSessionManager::IsTcpAvailable()
{
    // Port: the original looked for the TCP/IP stack in the registry. The port's transport is TCP/IP: available when
    // the sockets library starts.
    if (!_TcpChecked)
    {
        _TcpAvailable = MCSocket::Startup();
        _TcpChecked = true;
    }

    return _TcpAvailable;
}

bool MCSessionManager::IsIpxAvailable()
{
    // Port: the original looked for the IPX stack in the registry. The port's "IPX" connection is its LAN search over
    // TCP/IP (see MCDirectPlay), available with TCP/IP.
    if (!_IpxChecked)
    {
        _IpxAvailable = MCSocket::Startup();
        _IpxChecked = true;
    }

    return _IpxAvailable;
}

bool MCSessionManager::IsModemAvailable()
{
    // Original behaviour: the "checked" flag is never set, so every call looks for modems again.
    return FindModems() && GetModemName(0) != nullptr;
}

// ---- sessions ------------------------------------------------------------------------------------------------------

void MCSessionManager::AddSession(const DPSESSIONDESC2& desc)
{
    if (CurrentSession != nullptr && MCSameGuid(CurrentSession->SessionDesc.guidInstance, desc.guidInstance))
    {
        return;
    }

    Sessions.push_back(std::make_unique<MCFidpSession>(desc));
}

void MCSessionManager::ClearSessionList()
{
    std::erase_if(Sessions, [&](const auto& session) { return session.get() != CurrentSession; });
}

const std::vector<std::unique_ptr<MCFidpSession>>* MCSessionManager::GetSessions()
{
    if (DirectPlay == nullptr)
    {
        return nullptr;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc{};
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = _AppGuid;
    DirectPlay->EnumSessions(
        &desc, 0,
        [](const DPSESSIONDESC2* session, uint32_t*, uint32_t flags, void* context)
        {
            if ((flags & DPESC_TIMEDOUT) != 0)
            {
                return 0;
            }

            static_cast<MCSessionManager*>(context)->AddSession(*session);
            return 1;
        },
        this, EnumSessionFlags);
    return &Sessions;
}

MCFidpSession* MCSessionManager::FindMatchingSession(const _GUID& sessionGUID)
{
    const auto found = std::ranges::find_if(Sessions, [&](const auto& session)
                                            { return MCSameGuid(session->SessionDesc.guidInstance, sessionGUID); });
    return found != Sessions.end() ? found->get() : nullptr;
}

int32_t MCSessionManager::HostSession(const MCFidpSession& session, std::string_view playerName)
{
    Assert(DirectPlay != nullptr, 0, "Can't host session.  No DirectPlayObject");
    IsHost = true;

    if (DirectPlay->Open(&session.SessionDesc, DPOPEN_CREATE | DPOPEN_RETURNSTATUS) != DP_OK)
    {
        return -1;
    }

    CurrentSession = Sessions.emplace_back(std::make_unique<MCFidpSession>(session)).get();

    if (CreatePlayer(playerName) != 0)
    {
        return -2;
    }

    SendSystemInformation();
    return 0;
}

int32_t MCSessionManager::JoinSession(const _GUID& sessionGUID, std::string_view playerName)
{
    Assert(DirectPlay != nullptr, 0);
    MCFidpSession* session = FindMatchingSession(sessionGUID);

    if (session == nullptr)
    {
        return -1;
    }

    CurrentSession = session;
    int32_t result = static_cast<int32_t>(DirectPlay->Open(&session->SessionDesc, DPOPEN_JOIN | DPOPEN_RETURNSTATUS));

    if (result == 0)
    {
        result = CreatePlayer(playerName);

        if (result == 0)
        {
            SendSystemInformation();
        }
    }

    return result;
}

bool MCSessionManager::LockSession()
{
    if (CurrentSession == nullptr || !IsHost)
    {
        return false;
    }

    CurrentSession->SessionDesc.dwFlags |= DPSESSION_NEWPLAYERSDISABLED;
    DirectPlay->SetSessionDesc(&CurrentSession->SessionDesc, 0);
    _SessionLocked = true;
    GameStarted = true;
    return true;
}

bool MCSessionManager::LeaveSession()
{
    if (CurrentSession == nullptr || MyPlayer == nullptr)
    {
        return false;
    }

    MyPlayerID = 0;
    IsHost = false;
    ServerID = 0;
    HasPlayerNumber = false;
    CurrentSession = nullptr;
    MyPlayer = nullptr;
    DirectPlay->Close();
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();
    PlayersByLatency = {0, 1, 2, 3, 4, 5};
    return true;
}

// ---- players and groups --------------------------------------------------------------------------------------------

int32_t MCSessionManager::CreatePlayer(std::string_view playerName)
{
    _SessionLocked = false;
    _NewPlayerNumbers.fill(0);
    std::string name(playerName);
    DPNAME dpName{0x10, 0, name.data(), nullptr};
    // Port: the original made two Win32 events here (DirectPlay signalled one on every arrival; nothing waited on
    // either).
    const uint32_t result = DirectPlay->CreatePlayer(&MyPlayerID, &dpName, nullptr, nullptr, 0, 0);

    if (result != DP_OK)
    {
        MyPlayerID = 0;
        return static_cast<int32_t>(result);
    }

    {
        std::scoped_lock lock(_EmptyQueueLock);

        for (int i = 0; i < 900; i++)
        {
            _EmptyMessages.push_back(
                _MessagePool.emplace_back(std::make_unique<MCFidpMessage>(MyPlayerID, 0x200)).get());
        }
    }

    if (IsHost)
    {
        ServerID = MyPlayerID;
        _ServerMessage.Value = MyPlayerID;
        HasPlayerNumber = true;
    }

    EnumeratePlayers(nullptr);
    GetGroups(nullptr);
    return 0;
}

void MCSessionManager::SetHomeDirectory(std::string_view directory)
{
    _HomeDirectory = directory.substr(0, 0x1ff);
}

void MCSessionManager::NewPlayerEnumeration(uint32_t playerID, const DPNAME& name, uint32_t flags)
{
    MCFidpPlayer* player = nullptr;

    {
        std::scoped_lock lock(CriticalSection);

        if (GetPlayer(playerID) != nullptr)
        {
            return;
        }

        player = Players.emplace_back(std::make_unique<MCFidpPlayer>(playerID, name, flags)).get();
    }

    if (player->Id == MyPlayerID)
    {
        MyPlayer = player;

        if (IsHost)
        {
            MyPlayer->PlayerNumber = 0;
        }
    }
    else if (!LaunchedFromLobby)
    {
        if (!IsHost && HasPlayerNumber)
        {
            GivePlayerAnID(*player);
        }
    }
    else
    {
        for (int32_t i = 0; i < MaxLinkupPlayers; i++)
        {
            if (_NewPlayerNumbers[i] == playerID)
            {
                player->PlayerNumber = i;
                player->HasPlayerNumber = true;
            }
        }
    }
}

void MCSessionManager::NewGroupEnumeration(uint32_t groupID, const DPNAME& name, uint32_t flags)
{
    Groups.push_back(std::make_unique<MCFidpGroup>(groupID, name, flags));
}

MCFidpGroup* MCSessionManager::GetGroup(uint32_t groupID)
{
    const auto found = std::ranges::find_if(Groups, [&](const auto& group) { return group->Id == groupID; });
    return found != Groups.end() ? found->get() : nullptr;
}

MCFidpPlayer* MCSessionManager::GetPlayer(uint32_t playerID)
{
    const auto found = std::ranges::find_if(Players, [&](const auto& player) { return player->Id == playerID; });
    return found != Players.end() ? found->get() : nullptr;
}

MCFidpPlayer* MCSessionManager::GetPlayerNumber(int32_t playerNumber)
{
    const auto found =
        std::ranges::find_if(Players, [&](const auto& player) { return player->PlayerNumber == playerNumber; });
    return found != Players.end() ? found->get() : nullptr;
}

const std::vector<std::unique_ptr<MCFidpPlayer>>& MCSessionManager::GetPlayers(MCFidpSession* session)
{
    if (MyPlayerID == 0)
    {
        EnumeratePlayers(session);
    }

    return Players;
}

void MCSessionManager::EnumeratePlayers(MCFidpSession* session)
{
    // A list made again drops this machine's player too: its pointer is found again by the enumeration.
    MyPlayer = nullptr;
    Players.clear();
    Assert(DirectPlay != nullptr, 0);
    Assert(CurrentConnection != MCNetProtocol::None, 0);
    const MCEnumPlayersCallback callback = [](uint32_t id, uint32_t, const DPNAME* name, uint32_t flags, void* context)
    {
        static_cast<MCSessionManager*>(context)->NewPlayerEnumeration(id, *name, flags);
        return 1;
    };

    if (session == nullptr)
    {
        DirectPlay->EnumPlayers(nullptr, callback, this, 0);
    }
    else
    {
        DirectPlay->EnumPlayers(&session->SessionDesc.guidInstance, callback, this, DPENUMPLAYERS_SESSION);
    }

    if (CurrentSession != nullptr)
    {
        CurrentSession->SessionDesc.dwCurrentPlayers = static_cast<uint32_t>(Players.size());
    }
}

const std::vector<std::unique_ptr<MCFidpGroup>>& MCSessionManager::GetGroups(MCFidpSession* session)
{
    Groups.clear();
    Assert(DirectPlay != nullptr, 0);
    Assert(CurrentConnection != MCNetProtocol::None, 0);
    const MCEnumPlayersCallback callback = [](uint32_t id, uint32_t, const DPNAME* name, uint32_t flags, void* context)
    {
        static_cast<MCSessionManager*>(context)->NewGroupEnumeration(id, *name, flags);
        return 1;
    };

    if (session == nullptr)
    {
        DirectPlay->EnumGroups(nullptr, callback, this, 0);
    }
    else
    {
        // Original behaviour: a listed session's groups are asked for with EnumPlayers (its players come back).
        DirectPlay->EnumPlayers(&session->SessionDesc.guidInstance, callback, this, DPENUMPLAYERS_SESSION);
    }

    return Groups;
}

void MCSessionManager::CreateGroup(uint32_t& groupID, std::string_view groupName, std::span<const uint8_t> data,
                                   uint32_t flags)
{
    std::string name(groupName);
    DPNAME dpName{0x10, 0, name.data(), nullptr};
    DirectPlay->CreateGroup(&groupID, &dpName, nullptr, 0, flags);
    auto group = std::make_unique<MCFidpGroup>(groupID, dpName, flags);

    if (!data.empty())
    {
        group->SetGroupData(data);
    }

    Groups.push_back(std::move(group));
    SetGroupData(groupID, data, 0);
}

bool MCSessionManager::AddPlayerToGroup(uint32_t groupID, uint32_t playerID)
{
    if (playerID == 0)
    {
        playerID = MyPlayerID;
    }

    MCFidpGroup* group = GetGroup(groupID);

    if (group == nullptr)
    {
        return false;
    }

    if (group->AddPlayer(playerID))
    {
        MCFidpPlayer* player = GetPlayer(playerID);

        if (player == nullptr)
        {
            return false;
        }

        player->JoinGroup(groupID);
    }

    DirectPlay->AddPlayerToGroup(groupID, playerID);
    return true;
}

bool MCSessionManager::RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID)
{
    if (playerID == 0)
    {
        playerID = MyPlayerID;
    }

    MCFidpGroup* group = GetGroup(groupID);

    if (group == nullptr || !group->RemovePlayer(playerID))
    {
        return false;
    }

    MCFidpPlayer* player = GetPlayer(playerID);

    if (player == nullptr)
    {
        return false;
    }

    player->LeaveGroup(groupID);
    DirectPlay->DeletePlayerFromGroup(groupID, playerID);
    return true;
}

void MCSessionManager::SetGroupData(uint32_t groupID, std::span<const uint8_t> data, uint32_t flags) const
{
    if (!data.empty())
    {
        DirectPlay->SetGroupData(groupID, data.data(), static_cast<uint32_t>(data.size()), flags);
    }
}
