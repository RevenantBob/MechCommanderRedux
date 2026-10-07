#include "stdafx.h"
#include "linkup/sessionmanager.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/fidpgroup.h"
#include "linkup/filetransferinfo.h"
#include "linkup/session.h"
#include "lib/aerror.h"
#include "platform/MCSocket.h"

std::unique_ptr<MCBlockStore> LinkUpBlocks;
_GUID ThisAppGuid{};
uint32_t TicksPerMS = 0;
uint32_t StartTime = 0;
std::recursive_mutex AddingMessageList;
uint32_t NewPlayerNumbers[6];
int SessionLocked = 0;
int InReceiveThread = 0;
int OldVersionOfMPlayer = 1;
int DisabledCallerID = 0;
int CallerIDChanged[6];
int32_t LaunchedFromLobby = 0;

namespace
{
    using namespace MCDirectPlayGuids;

    /// <summary>Whether a SessionManager exists (0x0080a680).</summary>
    int InstanceExists = 0;
    /// <summary>The SessionManager (0x0080a684).</summary>
    MCSessionManager* Instance = nullptr;
    /// <summary>Who last took the global pointer (0x0080a688).</summary>
    void* GlobalPointerHolder = nullptr;
    /// <summary>RemovePlayerFromGame's re-entry guard (0x0080a69c).</summary>
    int RemovingPlayer = 0;

    /// <summary>
    /// Port: stands in for the Win32 events CreatePlayer made (DirectPlay signalled one on every arrival; nothing
    /// ever waited on either), so the fields read as "created".
    /// </summary>
    int EventStandIn = 0;

#pragma pack(push, 1)

    /// <summary>Message type 2 (guaranteed): the server's numbering of the players.</summary>
    /// <remarks>0x21 bytes. The names are the port's.</remarks>
    struct MCFIPlayerNumbersMessage : MCFIGuaranteedMessageHeader
    {
        /// <summary>The DPID of player number n (0 = no such player).</summary>
        uint32_t PlayerIDs[6]{};
        /// <summary>The sending server's own number.</summary>
        uint8_t ServerNumber = 0;
    };

    static_assert(sizeof(MCFIPlayerNumbersMessage) == 0x21);

    /// <summary>Message type 3: the players of a group, as the server knows them.</summary>
    /// <remarks>Sent as 0x24 bytes (six members) from a 300-byte buffer. The names are the port's.</remarks>
    struct MCFIPlayersInGroupMessage : MCFIGuaranteedMessageHeader
    {
        uint32_t GroupID = 0;
        uint32_t PlayerIDs[72]{};
    };

    /// <summary>Message types 6 (new server), 9 (player removed) and 12 (latency): the header and one 32-bit value.</summary>
    /// <remarks>0xc bytes. The names are the port's.</remarks>
    struct MCFIValueMessage : MCFIGuaranteedMessageHeader
    {
        uint32_t Value = 0;
    };

    static_assert(sizeof(MCFIValueMessage) == 0xc);

    /// <summary>
    /// Message type 10: the server's ping, with the other players' numbers sorted by latency (the order the next
    /// server is picked in).
    /// </summary>
    /// <remarks>9 + count bytes, built in a 256-byte buffer. The names are the port's.</remarks>
    struct MCFIPingMessage : MCFIGuaranteedMessageHeader
    {
        uint8_t Count = 0;
        uint8_t PlayerNumbers[6]{};
    };

    /// <summary>
    /// Message type 1: the numbers of the guaranteed messages received from one player since the last verify. Each
    /// entry is a MessageTagger with only the receiver's slot set.
    /// </summary>
    /// <remarks>3 + count * 6 bytes, built in a 0x2400-byte buffer per player number. The names are the port's.</remarks>
    struct MCFIVerifyMessage : MCFIMessageHeader
    {
        uint8_t Count = 0;
        uint8_t Entries[1][6]{};
    };

#pragma pack(pop)

    /// <summary>The message buffer's header word.</summary>
    uint16_t& HeaderOf(void* buffer)
    {
        return static_cast<MCFIMessageHeader*>(buffer)->Header;
    }

    /// <summary>The message type (bits 0-9) of a buffer.</summary>
    uint16_t TypeOf(const void* buffer)
    {
        return static_cast<const MCFIMessageHeader*>(buffer)->Header & FIMSG_TYPE_MASK;
    }

    /// <summary>Sets a header word to <paramref name="type"/> with <paramref name="flags"/>.</summary>
    void SetHeader(void* buffer, uint16_t flags, uint16_t type)
    {
        uint16_t& header = HeaderOf(buffer);
        header = 0;
        header |= flags;
        header = static_cast<uint16_t>((header & ~FIMSG_TYPE_MASK) | type);
    }

    /// <summary>The low 32 bits of the performance counter, as the original kept its times.</summary>
    uint32_t PerformanceTicks()
    {
        return static_cast<uint32_t>(MCPort::PerformanceCounter());
    }

    /// <summary>Resets a verify buffer to an empty type-1 message.</summary>
    void ResetVerify(uint8_t* verify)
    {
        SetHeader(verify, 0, 1);
        reinterpret_cast<MCFIVerifyMessage*>(verify)->Count = 0;
    }
}

// ---- free functions ----------------------------------------------------------------------------------------------

void ClearList(MCFidpMsgList* list)
{
    list->Size();

    while (list->HeadLink != nullptr)
    {
        list->TossHead();
    }
}

void DebugColor(uint8_t, uint8_t, uint8_t)
{
    // Port: the original wrote VGA palette entry 0 through ports 0x3c8/0x3c9 (a debugging aid); nothing to do.
}

uint32_t TimeStamp()
{
    return MCPort::Milliseconds() - StartTime;
}

void DisableCallerID()
{
    // Port: the original switched the installed modems' caller-id reporting off in the registry (AT#CID=0) for a
    // modem game. The port has no modem connection, so no modem game is ever hosted and there is nothing to change.
    if (DisabledCallerID == 0)
    {
        for (int i = 0; i < 6; i++)
        {
            CallerIDChanged[i] = 0;
        }
    }
}

void ReEnableCallerID()
{
    // Port: the registry settings DisableCallerID would have changed are never changed (see there).
    for (int i = 0; i < 6; i++)
    {
        CallerIDChanged[i] = 0;
    }

    DisabledCallerID = 0;
}

void InitLinkUpBlocks()
{
    if (LinkUpBlocks == nullptr)
    {
        LinkUpBlocks = std::make_unique<MCBlockStore>();
    }
}

void DestroyLinkUpBlocks()
{
    if (LinkUpBlocks != nullptr)
    {
        LinkUpBlocks->Clear();
        LinkUpBlocks.reset();
    }
}

int EnumPlayersCallback(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags, void* context)
{
    return static_cast<MCSessionManager*>(context)->NewPlayerEnumeration(playerID, playerType, name, flags);
}

int EnumGroupsCallback(uint32_t groupID, uint32_t, const DPNAME* name, uint32_t flags, void* context)
{
    static_cast<MCSessionManager*>(context)->NewGroupEnumeration(groupID, name, flags);
    return 1;
}

uint32_t SessionManagerReceiveThread(void* sessionManager)
{
    return static_cast<uint32_t>(static_cast<MCSessionManager*>(sessionManager)->ReceiveThread());
}

int EnumConnectionsCallback(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                            uint32_t flags, void* context)
{
    if (serviceProvider == nullptr)
    {
        return 0;
    }

    return static_cast<MCSessionManager*>(context)->AddConnection(serviceProvider, connection, connectionSize, name,
                                                                  flags, context);
}

int ModemCallback(const _GUID& dataType, uint32_t dataSize, const void* data, void*)
{
    if (MCSameGuid(dataType, DPAID_Modem))
    {
        MCSessionManager::GetGlobalPointer(nullptr)->AddModemName(data, dataSize);
    }

    return MCSessionManager::GetGlobalPointer(nullptr)->NumModems < 10;
}

int EnumSessionsCallback(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context)
{
    if (context == nullptr)
    {
        return 0;
    }

    if ((flags & DPESC_TIMEDOUT) != 0)
    {
        return 0;
    }

    return static_cast<MCSessionManager*>(context)->AddSession(desc, timeout, flags, context);
}

void ShiftPointerArray(int32_t* array, int index, int count)
{
    std::memmove(array + index, array + index + 1, (count - index) * sizeof(int32_t));
    array[count] = 0;
}

int CompareLatencies(const int32_t* playerNumber1, const int32_t* playerNumber2)
{
    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);
    MCFidpPlayer* player1 = sessionManager->GetPlayerNumber(*playerNumber1);
    MCFidpPlayer* player2 = sessionManager->GetPlayerNumber(*playerNumber2);

    if (player2->AverageLatency() < player1->AverageLatency())
    {
        return 1;
    }

    if (player1->AverageLatency() < player2->AverageLatency())
    {
        return -1;
    }

    return 0;
}

int CompareLongs(const int32_t* value1, const int32_t* value2)
{
    if (*value2 < *value1)
    {
        return 1;
    }

    if (*value1 < *value2)
    {
        return -1;
    }

    return 0;
}

void OutputSessionManagerStats()
{
    MCSessionManager* sessionManager = MCSessionManager::GetGlobalPointer(nullptr);
    MCFLinkedList<MCFidpPlayer>* players = sessionManager->GetPlayers(nullptr);
    MCFLinkedListIterator<MCFidpPlayer> iterator(players);
    iterator.Current = players->HeadLink;
    const int numPlayers = players->Count;

    for (int i = 0; i < numPlayers; i++)
    {
        MCFidpPlayer* player = iterator.Current != nullptr ? iterator.Current->Data : nullptr;
        char line[512];
        // Original behaviour: the line is formatted and dropped (its output call was compiled out).
        std::snprintf(line, sizeof(line), "Messages to player %s - vlist size = %d\\n", player->Name,
                      player->VerifyList.Count);
        Assert(iterator.Current != nullptr, 0, nullptr);
        iterator.Current = iterator.Current->Next;
    }
}

// ---- FIDPNetworkProtocol -----------------------------------------------------------------------------------------

MCFidpNetworkProtocol::MCFidpNetworkProtocol()
{
    ShortName[0] = '\0';
    LongName[0] = '\0';
    ConnectionBuffer = nullptr;
    ProtocolType = -1;
}

MCFidpNetworkProtocol::~MCFidpNetworkProtocol()
{
    Destroy();
}

void MCFidpNetworkProtocol::Destroy()
{
    if (ConnectionBuffer != nullptr)
    {
        LinkUpBlocks->Free(ConnectionBuffer);
        // Port fix: cleared, so the destructor after ClearList's destroy() doesn't free it twice.
        ConnectionBuffer = nullptr;
    }
}

int MCFidpNetworkProtocol::SetConnectionBuffer(void* connection, int size)
{
    if (ConnectionBuffer != nullptr)
    {
        LinkUpBlocks->Free(ConnectionBuffer);
    }

    ConnectionBuffer = LinkUpBlocks->Allocate(size);

    if (ConnectionBuffer == nullptr)
    {
        return -1;
    }

    std::memcpy(ConnectionBuffer, connection, size);
    return 0;
}

void MCFidpNetworkProtocol::SetShortName(char* name)
{
    if (name == nullptr)
    {
        ShortName[0] = '\0';
    }
    else
    {
        // Port fix: always terminated.
        std::memset(ShortName, 0, sizeof(ShortName));
        std::strncpy(ShortName, name, 0x3f);
    }
}

void MCFidpNetworkProtocol::SetLongName(char* name)
{
    if (name == nullptr)
    {
        LongName[0] = '\0';
    }
    else
    {
        std::memset(LongName, 0, sizeof(LongName));
        std::strncpy(LongName, name, 0xff);
    }
}

void MCFidpNetworkProtocol::ClearList(MCFLinkedList<MCFidpNetworkProtocol>& list)
{
    const int numProtocols = list.Count;
    list.Current = list.HeadLink;

    for (int i = 0; i < numProtocols; i++)
    {
        MCFidpNetworkProtocol* protocol = list.Current->Data;
        list.Del(protocol);
        protocol->Destroy();
        delete protocol;
    }
}

// ---- SessionManager: lifetime ------------------------------------------------------------------------------------

MCSessionManager::MCSessionManager(_GUID appGUID)
{
    Assert(InstanceExists == 0, 0, nullptr);
    TicksPerMS = static_cast<uint32_t>(static_cast<uint32_t>(MCPort::PerformanceFrequency()) / 1000);
    GlobalPointerHolder = nullptr;
    InstanceExists = 1;
    Instance = this;
    IpxChecked = 0;
    TcpChecked = 0;
    ModemChecked = 0;
    DirectPlay = nullptr;
    GameStarted = 0;
    StartTime = MCPort::Milliseconds();
    const int32_t result = CreateDirectPlayInterface();
    Assert(result == 0, 0, "This application requires DirectX 5 or later.");
    ThisAppGuid = appGUID;
    MyPlayerID = 0;
    ServerID = 0;
    PlayerEvent = nullptr;
    KillReceiveEvent = nullptr;
    ReceiveThreadHandle = nullptr;
    CurrentSession = nullptr;
    CurrentConnection = -1;
    IsHost = 0;
    HasPlayerNumber = 0;
    NextFileID = 0;

    auto* newServer = new MCFIValueMessage{};
    newServer->Header = 0;
    newServer->Header |= FIMSG_GUARANTEED;
    newServer->Tagger.Clear();
    newServer->Value = 0;
    newServer->Header = static_cast<uint16_t>((newServer->Header & ~FIMSG_TYPE_MASK) | 6);
    ServerMessage = newServer;

    for (int i = 0; i < 6; i++)
    {
        DeletedPlayerIDs[i] = 0xffffffffu;
        PlayersByLatency[i] = i;
    }

    ApplicationCallback = nullptr;
    ApplicationCallbackData = nullptr;
    SystemCallback = nullptr;
    SystemCallbackData = nullptr;
    FileSentCallback = nullptr;
    FileSentCallbackData = nullptr;
    FileReceivedCallback = nullptr;
    FileReceivedCallbackData = nullptr;
    PlayerIterator = new MCFLinkedListIterator<MCFidpPlayer>(&Players);
    VerifyMessageMemory = static_cast<uint8_t*>(LinkUpBlocks->Allocate(0xd800));

    for (int i = 0; i < 6; i++)
    {
        VerifyMessages[i] = VerifyMessageMemory + i * 0x2400;
    }

    EmptyMessages = new MCFidpMsgList();
    SystemMessages = new MCFidpMsgList();
    ApplicationMessages = new MCFidpMsgList();
    PreIDReceivedMessages = new MCFidpMsgList();
    PreIDGroupMessages = new MCFidpMsgList();
    PreIDServerMessages = new MCFidpMsgList();
    NextPingTime = PerformanceTicks();
    PingInterval = 2000;
    DialupState = 0;
}

MCSessionManager::~MCSessionManager()
{
    Destroy();

    while (PendingPlayers.HeadLink != nullptr)
    {
        PendingPlayers.Del(PendingPlayers.HeadLink->Data);
    }

    while (IncomingFiles.HeadLink != nullptr)
    {
        IncomingFiles.Del(IncomingFiles.HeadLink->Data);
    }

    while (OutgoingFiles.HeadLink != nullptr)
    {
        OutgoingFiles.Del(OutgoingFiles.HeadLink->Data);
    }

    while (Groups.HeadLink != nullptr)
    {
        Groups.Del(Groups.HeadLink->Data);
    }

    while (Players.HeadLink != nullptr)
    {
        Players.Del(Players.HeadLink->Data);
    }

    while (Sessions.HeadLink != nullptr)
    {
        Sessions.Del(Sessions.HeadLink->Data);
    }

    while (Connections.HeadLink != nullptr)
    {
        Connections.Del(Connections.HeadLink->Data);
    }
}

void MCSessionManager::Destroy()
{
    std::lock_guard lock(CriticalSection);
    delete EmptyMessages;
    EmptyMessages = nullptr;
    delete SystemMessages;
    SystemMessages = nullptr;
    delete ApplicationMessages;
    ApplicationMessages = nullptr;
    delete PreIDReceivedMessages;
    PreIDReceivedMessages = nullptr;
    delete PreIDGroupMessages;
    PreIDGroupMessages = nullptr;
    delete PreIDServerMessages;
    PreIDServerMessages = nullptr;
    Instance = nullptr;
    GlobalPointerHolder = nullptr;
    InstanceExists = 0;
    DestroyDirectPlayInterface();
    delete static_cast<MCFIValueMessage*>(ServerMessage);
    ServerMessage = nullptr;
    delete PlayerIterator;
    PlayerIterator = nullptr;
}

int32_t MCSessionManager::CreateDirectPlayInterface()
{
    // Port: CoCreateInstance(CLSID_DirectPlay, IID_IDirectPlay3A) becomes the port's stand-in, which can't fail.
    if (DirectPlay == nullptr)
    {
        DirectPlay = new MCDirectPlay();
    }

    EnumerateConnections();
    return 0;
}

int32_t MCSessionManager::DestroyDirectPlayInterface()
{
    LeaveSession();

    if (DirectPlay != nullptr)
    {
        delete DirectPlay;
        DirectPlay = nullptr;
    }

    CurrentConnection = -1;
    return 0;
}

// ---- message queues ----------------------------------------------------------------------------------------------

void MCSessionManager::AddMessageToEmptyQueue(MCFidpMessage* msg)
{
    std::lock_guard lock(AddingMessageList);
    msg->Clear();
    EmptyMessages->Add(msg);
}

MCFidpMessage* MCSessionManager::GetMessageFromEmptyQueue()
{
    std::lock_guard lock(AddingMessageList);
    MCFidpMessage* msg = EmptyMessages->Head();
    EmptyMessages->TossHead();
    return msg;
}

// ---- connections, sessions, players, groups ----------------------------------------------------------------------

int32_t MCSessionManager::EnumerateConnections()
{
    Assert(DirectPlay != nullptr, 0, nullptr);
    MCFidpNetworkProtocol::ClearList(Connections);
    return static_cast<int32_t>(DirectPlay->EnumConnections(&ThisAppGuid, EnumConnectionsCallback, this, 0));
}

void MCSessionManager::SetConnectionType(MCFidpNetworkProtocol* protocol, const _GUID* guid)
{
    if (MCSameGuid(*guid, DPSPGUID_TCPIP))
    {
        protocol->ProtocolType = PROTOCOL_TCPIP;
        AvailableProtocols |= PROTOCOL_TCPIP;
    }
    else if (MCSameGuid(*guid, DPSPGUID_IPX))
    {
        protocol->ProtocolType = PROTOCOL_IPX;
        AvailableProtocols |= PROTOCOL_IPX;
    }
    else if (MCSameGuid(*guid, DPSPGUID_MODEM))
    {
        protocol->ProtocolType = PROTOCOL_MODEM;
        AvailableProtocols |= PROTOCOL_MODEM;
    }
    else if (MCSameGuid(*guid, DPSPGUID_SERIAL))
    {
        protocol->ProtocolType = PROTOCOL_SERIAL;
        AvailableProtocols |= PROTOCOL_SERIAL;
    }
    else if (MCSameGuid(*guid, DPSPGUID_LOBBY))
    {
        protocol->ProtocolType = PROTOCOL_LOBBY;
        AvailableProtocols |= PROTOCOL_LOBBY;
    }
}

int MCSessionManager::AddConnection(const _GUID* serviceProvider, void* connection, uint32_t connectionSize,
                                    const DPNAME* name, uint32_t, void* context)
{
    if (context != this)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Wrong object receiving callback", nullptr);
        return 0;
    }

    MCFidpNetworkProtocol* protocol = new MCFidpNetworkProtocol();
    protocol->SetShortName(name->lpszShortNameA);
    protocol->SetLongName(name->lpszLongNameA);
    protocol->SetConnectionBuffer(connection, static_cast<int>(connectionSize));
    SetConnectionType(protocol, serviceProvider);
    Connections.Add(protocol);
    return 1;
}

int MCSessionManager::AddSession(const DPSESSIONDESC2* desc, uint32_t*, uint32_t, void* context)
{
    if (context != this)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Wrong object receiving EnumSessions callback",
                                 nullptr);
        return 0;
    }

    Assert(desc != nullptr, 0, nullptr);

    if (CurrentSession != nullptr && MCSameGuid(CurrentSession->SessionDesc.guidInstance, desc->guidInstance))
    {
        return 1;
    }

    Sessions.Add(new MCFidpSession(*desc));
    return 1;
}

int MCSessionManager::NewPlayerEnumeration(uint32_t playerID, uint32_t, const DPNAME* name, uint32_t flags)
{
    MCFidpPlayer* player = new MCFidpPlayer(playerID, name, flags);
    Assert(player != nullptr, 0, "Player is null");

    {
        // Port fix: a scoped lock; the original left the critical section entered when the player was already
        // listed (and leaked the new one, as the port still does).
        std::lock_guard lock(CriticalSection);

        if (GetPlayer(playerID) != nullptr)
        {
            return 1;
        }

        Players.Add(player);
    }

    if (player->Id == MyPlayerID)
    {
        MyPlayer = player;

        if (IsHost != 0)
        {
            MyPlayer->PlayerNumber = 0;
        }
    }
    else if (LaunchedFromLobby == 0)
    {
        if (IsHost == 0 && HasPlayerNumber != 0)
        {
            GivePlayerAnID(player);
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (NewPlayerNumbers[i] == playerID)
            {
                player->PlayerNumber = i;
                player->HasPlayerNumber = 1;
            }
        }
    }

    return 1;
}

int MCSessionManager::NewGroupEnumeration(uint32_t groupID, const DPNAME* name, uint32_t flags)
{
    Groups.Add(new MCFidpGroup(groupID, 0, name, flags));
    return 1;
}

MCFidpGroup* MCSessionManager::GetGroup(uint32_t groupID)
{
    Groups.Current = Groups.HeadLink;

    for (int i = 0; i < Groups.Count; i++)
    {
        MCFidpGroup* group = Groups.ReadAndNext();

        if (group->Id == groupID)
        {
            return group;
        }
    }

    return nullptr;
}

MCFidpPlayer* MCSessionManager::GetPlayer(uint32_t playerID)
{
    for (int i = 0; i < 6; i++)
    {
        if (DeletedPlayerIDs[i] == playerID)
        {
            return nullptr;
        }
    }

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data->Id == playerID)
        {
            return link->Data;
        }
    }

    return nullptr;
}

int32_t MCSessionManager::CreatePlayer(char* playerName)
{
    SessionLocked = 0;

    for (int i = 0; i < 6; i++)
    {
        NewPlayerNumbers[i] = 0;
    }

    DPNAME name{0x10, 0, playerName, nullptr};
    Assert(PlayerEvent == nullptr, 0, "Player event already initialized");
    Assert(KillReceiveEvent == nullptr, 0, " kill event already initialized");
    PlayerEvent = &EventStandIn;
    KillReceiveEvent = &EventStandIn;
    Assert(PlayerEvent != nullptr, 0, "Could not create hKillReceiveEvent");
    Assert(KillReceiveEvent != nullptr, 0, "Could not create hKillReceiveEvent");
    const uint32_t result = DirectPlay->CreatePlayer(&MyPlayerID, &name, PlayerEvent, nullptr, 0, 0);

    if (result == DP_OK)
    {
        {
            std::lock_guard lock(AddingMessageList);
            EmptyMessages->Head();

            for (int i = 0; i < 900; i++)
            {
                EmptyMessages->Add(new MCFidpMessage(MyPlayerID, 0x200));
            }
        }

        if (IsHost != 0)
        {
            ServerID = MyPlayerID;
            static_cast<MCFIValueMessage*>(ServerMessage)->Value = MyPlayerID;
            HasPlayerNumber = 1;
        }

        EnumeratePlayers(nullptr);
        GetGroups(nullptr);

        if (DialupState != 0)
        {
            EnableDialupNetworking(DialupState);
            DialupState = 0;
        }
    }
    else
    {
        PlayerEvent = nullptr;
        KillReceiveEvent = nullptr;
        MyPlayerID = 0;
    }

    return static_cast<int32_t>(result);
}

void MCSessionManager::SetHomeDirectory(char* directory)
{
    std::strncpy(HomeDirectory, directory, 0x1ff);
}

int32_t MCSessionManager::HostSession(MCFidpSession& session, char* playerName)
{
    Assert(DirectPlay != nullptr, 0, "Can't host session.  No DirectPlayObject");
    IsHost = 1;

    if (CurrentConnection == PROTOCOL_MODEM)
    {
        DisableCallerID();
    }

    if (DirectPlay->Open(&session.SessionDesc, DPOPEN_CREATE | DPOPEN_RETURNSTATUS) != DP_OK)
    {
        return -1;
    }

    MCFidpSession* hosted = new MCFidpSession(session);
    Sessions.Add(hosted);
    CurrentSession = hosted;

    if (CreatePlayer(playerName) != 0)
    {
        return -2;
    }

    SendSystemInformation();
    return 0;
}

MCFidpSession* MCSessionManager::FindMatchingSession(_GUID* sessionGUID)
{
    Sessions.Current = Sessions.HeadLink;

    for (;;)
    {
        MCFidpSession* session = Sessions.ReadAndNext();

        if (session == nullptr)
        {
            return nullptr;
        }

        if (MCSameGuid(session->SessionDesc.guidInstance, *sessionGUID))
        {
            return session;
        }
    }
}

int32_t MCSessionManager::JoinSession(_GUID* sessionGUID, char* playerName)
{
    Assert(DirectPlay != nullptr, 0, nullptr);
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

void MCSessionManager::SendSystemInformation()
{
    MCFISystemInfoMessage msg;
    msg.Tagger.Clear();
    msg.Header = 0x100b;
    msg.TotalPhysicalMemory = MCPort::TotalPhysicalMemory();

    if (IsHost == 0)
    {
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
    }
    else
    {
        MyPlayer->TotalPhysicalMemory = msg.TotalPhysicalMemory;
    }
}

int MCSessionManager::ReadyToChooseServer()
{
    if (MyPlayer == nullptr)
    {
        return 0;
    }

    return LatencyReportsIn;
}

void MCSessionManager::ProcessSystemInfoMessage(MCFISystemInfoMessage* msg, uint32_t fromID)
{
    MCFidpPlayer* player = GetPlayer(fromID);

    if (player != nullptr)
    {
        player->TotalPhysicalMemory = msg->TotalPhysicalMemory;
    }
}

int MCSessionManager::LockSession()
{
    if (CurrentSession == nullptr || IsHost == 0)
    {
        return 0;
    }

    CurrentSession->SessionDesc.dwFlags |= DPSESSION_NEWPLAYERSDISABLED;
    ReportError(DirectPlay->SetSessionDesc(&CurrentSession->SessionDesc, 0));
    SessionLocked = 1;
    GameStarted = 1;
    return 1;
}

int MCSessionManager::LeaveSession()
{
    if (CurrentConnection == PROTOCOL_TCPIP && DialupState != 0)
    {
        EnableDialupNetworking(DialupState);
        DialupState = 0;
    }

    if (DisabledCallerID != 0)
    {
        ReEnableCallerID();
        DisabledCallerID = 0;
    }

    if (CurrentSession == nullptr || MyPlayer == nullptr)
    {
        return 0;
    }

    ReceiveThreadHandle = nullptr;
    KillReceiveEvent = nullptr;
    PlayerEvent = nullptr;
    MyPlayerID = 0;
    IsHost = 0;
    ServerID = 0;
    HasPlayerNumber = 0;
    CurrentSession = nullptr;
    MyPlayer = nullptr;
    DirectPlay->Close();
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();

    for (int i = 0; i < 6; i++)
    {
        PlayersByLatency[i] = i;
    }

    return 1;
}

void MCSessionManager::CreateGroup(uint32_t* groupID, char* groupName, void* data, uint32_t dataSize, uint32_t flags)
{
    DPNAME name{0x10, 0, groupName, nullptr};
    const uint32_t result = DirectPlay->CreateGroup(groupID, &name, nullptr, 0, flags);
    ReportError(result);
    MCFidpGroup* group = new MCFidpGroup(*groupID, 0, &name, flags);

    if (data != nullptr)
    {
        group->SetGroupData(data, dataSize);
    }

    Groups.Add(group);
    SetGroupData(*groupID, data, dataSize, 0);
    ReportError(result);
}

int MCSessionManager::AddPlayerToGroup(uint32_t groupID, uint32_t playerID)
{
    int added = 0;
    Groups.Current = Groups.HeadLink;

    if (playerID == 0)
    {
        playerID = MyPlayerID;
    }

    for (int i = 0; i < Groups.Count; i++)
    {
        MCFidpGroup* group = Groups.ReadAndNext();

        if (group->Id != groupID)
        {
            continue;
        }

        if (group->AddPlayer(playerID) != 0)
        {
            MCFidpPlayer* player = GetPlayer(playerID);

            if (player == nullptr)
            {
                break;
            }

            player->JoinGroup(groupID);
        }

        added = 1;
        break;
    }

    if (added != 0)
    {
        ReportError(DirectPlay->AddPlayerToGroup(groupID, playerID));
    }

    return added;
}

int MCSessionManager::RemovePlayerWithID(uint32_t playerID)
{
    MCFidpPlayer* player = GetPlayer(playerID);

    if (player == nullptr)
    {
        return -1;
    }

    return RemovePlayerFromGame(player);
}

int MCSessionManager::RemovePlayerFromGame(MCFidpPlayer* player)
{
    if (RemovingPlayer != 0)
    {
        return -1;
    }

    RemovingPlayer = 1;

    if (player->HasPlayerNumber == 0)
    {
        RemovingPlayer = 0;
        return -1;
    }

    MCFIValueMessage* msg = static_cast<MCFIValueMessage*>(LinkUpBlocks->Allocate(sizeof(MCFIValueMessage)));
    msg->Tagger.Clear();
    SetHeader(msg, FIMSG_GUARANTEED, 9);
    msg->Value = player->Id;
    SendMessageToPlayerGuaranteed(player->Id, msg, sizeof(MCFIValueMessage), 1);
    player->HasPlayerNumber = 0;
    LinkUpBlocks->Free(msg);
    RemovingPlayer = 0;
    return 0;
}

int MCSessionManager::RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID)
{
    int removed = 0;

    if (playerID == 0)
    {
        playerID = MyPlayerID;
    }

    for (MCFLink<MCFidpGroup>* link = Groups.HeadLink; link != nullptr; link = link->Next)
    {
        MCFidpGroup* group = link->Data;

        if (group->Id != groupID)
        {
            continue;
        }

        if (group->RemovePlayer(playerID) != 0)
        {
            MCFidpPlayer* player = GetPlayer(playerID);

            if (player != nullptr)
            {
                player->LeaveGroup(groupID);
                removed = 1;
            }
        }

        break;
    }

    if (removed != 0)
    {
        ReportError(DirectPlay->DeletePlayerFromGroup(groupID, playerID));
    }

    return removed;
}

void MCSessionManager::SetGroupData(uint32_t groupID, void* data, uint32_t dataSize, uint32_t flags)
{
    if (data != nullptr && dataSize != 0)
    {
        ReportError(DirectPlay->SetGroupData(groupID, data, dataSize, flags));
    }
}

int32_t MCSessionManager::SetCurrentConnection(int type)
{
    void* connection = nullptr;
    Connections.Current = Connections.HeadLink;

    for (;;)
    {
        MCFidpNetworkProtocol* protocol = Connections.ReadAndNext();

        if (protocol == nullptr)
        {
            break;
        }

        if (protocol->ProtocolType == type)
        {
            connection = protocol->ConnectionBuffer;
            break;
        }
    }

    Assert(connection != nullptr, 0, nullptr);
    // Port fix: recreating the DirectPlay object below re-enumerates the connections, which frees the buffer just
    // found (the original went on using it); the port keeps a copy.
    std::vector<uint8_t> copy(static_cast<uint8_t*>(connection),
                              static_cast<uint8_t*>(connection) + MCDirectPlay::ConnectionDataSize);

    if (CurrentConnection >= 0)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    const int32_t result = static_cast<int32_t>(DirectPlay->InitializeConnection(copy.data(), 0));

    if (type != PROTOCOL_MODEM && result == 0)
    {
        GetSessions();
    }

    CurrentConnection = type;
    return result;
}

void MCSessionManager::ConnectIpx()
{
    SetCurrentConnection(PROTOCOL_IPX);
}

int32_t MCSessionManager::EnableDialupNetworking(uint32_t)
{
    // Port: the original restored HKCU "Software\Microsoft\Windows\CurrentVersion\Internet Settings\EnableAutodial".
    // The port never changes it (see DisableDialupNetworking).
    return 0;
}

uint32_t MCSessionManager::DisableDialupNetworking()
{
    // Port: the original switched Windows' dial-up autodial off in the registry for the length of a TCP/IP game, so
    // looking for sessions wouldn't dial the internet provider. Not the port's business: it reports "was off".
    return 0;
}

void MCSessionManager::ConnectTcp(char* ipAddress)
{
    if (CurrentConnection == PROTOCOL_TCPIP)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    DialupState = DisableDialupNetworking();
    DPCOMPOUNDADDRESSELEMENT address[2];
    address[0].guidDataType = DPAID_ServiceProvider;
    address[0].dwDataSize = sizeof(_GUID);
    address[0].lpData = const_cast<_GUID*>(&DPSPGUID_TCPIP);
    address[1].guidDataType = DPAID_INet;
    address[1].dwDataSize = static_cast<uint32_t>(std::strlen(ipAddress) + 1);
    address[1].lpData = ipAddress;

    if (InitializeConnection(address, 2) == 0)
    {
        CurrentConnection = PROTOCOL_TCPIP;
    }
}

int32_t MCSessionManager::ConnectModem(char* phoneNumber, char* modemName)
{
    if (modemName == nullptr)
    {
        return -1;
    }

    DPCOMPOUNDADDRESSELEMENT address[3];
    address[0].guidDataType = DPAID_ServiceProvider;
    address[0].dwDataSize = sizeof(_GUID);
    address[0].lpData = const_cast<_GUID*>(&DPSPGUID_MODEM);
    int numElements = 1;

    if (std::strlen(modemName) > 1)
    {
        address[1].guidDataType = DPAID_Modem;
        address[1].dwDataSize = static_cast<uint32_t>(std::strlen(modemName) + 1);
        address[1].lpData = modemName;
        numElements = 2;
    }

    address[numElements].guidDataType = DPAID_Phone;
    address[numElements].dwDataSize = static_cast<uint32_t>(std::strlen(phoneNumber) + 1);
    address[numElements].lpData = phoneNumber;
    const int32_t result = InitializeConnection(address, numElements + 1);

    if (result == 0)
    {
        CurrentConnection = PROTOCOL_MODEM;
    }

    return result;
}

int32_t MCSessionManager::ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity,
                                         uint32_t flowControl)
{
    uint32_t settings[5] = {port, baudRate, stopBits, parity, flowControl};
    DPCOMPOUNDADDRESSELEMENT address[2];
    address[0].guidDataType = DPAID_ServiceProvider;
    address[0].dwDataSize = sizeof(_GUID);
    address[0].lpData = const_cast<_GUID*>(&DPSPGUID_SERIAL);
    address[1].guidDataType = DPAID_ComPort;
    address[1].dwDataSize = sizeof(settings);
    address[1].lpData = settings;
    const int32_t result = InitializeConnection(address, 2);

    if (result == 0)
    {
        CurrentConnection = PROTOCOL_SERIAL;
        GetSessions();
    }

    return result;
}

int32_t MCSessionManager::InitializeConnection(DPCOMPOUNDADDRESSELEMENT* elements, int numElements)
{
    if (CurrentConnection >= 0)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    void* lobby = nullptr;
    uint32_t result = static_cast<uint32_t>(CreateLobby(&lobby));
    ReportError(result);
    uint32_t size = 0;
    result = MCDirectPlay::CreateCompoundAddress(elements, numElements, nullptr, &size);

    if (result != DPERR_BUFFERTOOSMALL)
    {
        ReportError(result);
    }

    void* address = LinkUpBlocks->Allocate(size);
    result = MCDirectPlay::CreateCompoundAddress(elements, numElements, address, &size);

    if (result == DP_OK)
    {
        result = DirectPlay->InitializeConnection(address, 0);
    }

    if (address != nullptr)
    {
        LinkUpBlocks->Free(address);
    }

    return static_cast<int32_t>(result);
}

void MCSessionManager::AddModemName(const void* data, uint32_t dataSize)
{
    uint32_t used = 0;
    const char* name = static_cast<const char*>(data);

    while (used < dataSize)
    {
        char* slot = ModemNames[NumModems];
        std::strncpy(slot, name, 0x3f);
        used += static_cast<uint32_t>(std::strlen(slot) + 1);
        name += std::strlen(slot) + 1;

        if (std::strlen(slot) < 2)
        {
            break;
        }

        NumModems++;
    }
}

int MCSessionManager::FindModems()
{
    if (CurrentSession != nullptr)
    {
        return -1;
    }

    if (ConnectModem(const_cast<char*>(""), const_cast<char*>("")) != 0)
    {
        return -1;
    }

    // Port: not reached. MCDirectPlay has no modem service provider, so ConnectModem always fails above. The
    // original went on to read the modem connection's address (GetPlayerAddress) and walk it with the lobby's
    // EnumAddress, collecting the names through ModemCallback / AddModemName.
    NumModems = 0;
    return -1;
}

char* MCSessionManager::GetModemName(int32_t index)
{
    if (index < NumModems)
    {
        return ModemNames[index];
    }

    return nullptr;
}

int32_t MCSessionManager::CreateLobby(void** lobby)
{
    // Port: the DirectPlay lobby object (DirectPlayLobbyCreateA, IDirectPlayLobby2A) only served to build compound
    // addresses, which the port's stand-in does itself (MCDirectPlay::CreateCompoundAddress).
    *lobby = nullptr;
    return 0;
}

int MCSessionManager::WasLaunchedFromLobby()
{
    // Port: no DirectPlay lobby launches the game; GetConnectionSettings answered DPERR_NOTLOBBIED.
    ReportError(DPERR_NOTLOBBIED);
    return 0;
}

uint32_t MCSessionManager::SetupLobbyConnection(void (*)(), void (*)())
{
    // Port: the original dropped its DirectPlay object, asked the lobby for the connection it was launched with
    // (Connect), and joined or hosted that session. Without a lobby GetConnectionSettings answers DPERR_NOTLOBBIED,
    // and the original then made a new DirectPlay object without enumerating its connections; so does the port.
    if (DirectPlay != nullptr)
    {
        DirectPlay->Close();
        delete DirectPlay;
        DirectPlay = nullptr;
    }

    if (DirectPlay == nullptr)
    {
        DirectPlay = new MCDirectPlay();
    }

    return DPERR_NOTLOBBIED;
}

MCFLinkedList<MCFidpNetworkProtocol>* MCSessionManager::GetConnections()
{
    Connections.Current = Connections.HeadLink;
    return &Connections;
}

void MCSessionManager::ClearSessionList()
{
    Sessions.Current = Sessions.HeadLink;

    if (CurrentSession == nullptr)
    {
        MCFidpSession::ClearList(Sessions);
        return;
    }

    const int numSessions = Sessions.Count;

    for (int i = 0; i < numSessions; i++)
    {
        MCFidpSession* session = Sessions.ReadAndNext();

        if (session != CurrentSession)
        {
            Sessions.Del(session);
            delete session;
        }
    }
}

MCFLinkedList<MCFidpSession>* MCSessionManager::GetSessions()
{
    if (DirectPlay == nullptr)
    {
        return nullptr;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = ThisAppGuid;
    DirectPlay->EnumSessions(&desc, 0, EnumSessionsCallback, this,
                             DPENUMSESSIONS_AVAILABLE | DPENUMSESSIONS_ASYNC | DPENUMSESSIONS_PASSWORDREQUIRED |
                                 DPENUMSESSIONS_RETURNSTATUS);
    Sessions.Current = Sessions.HeadLink;
    return &Sessions;
}

MCFLinkedList<MCFidpPlayer>* MCSessionManager::GetPlayers(MCFidpSession* session)
{
    if (MyPlayerID == 0)
    {
        EnumeratePlayers(session);
    }
    else
    {
        Players.Current = Players.HeadLink;
    }

    Players.Current = Players.HeadLink;
    return &Players;
}

void MCSessionManager::EnumeratePlayers(MCFidpSession* session)
{
    MCFidpPlayer::ClearList(Players);
    Assert(DirectPlay != nullptr, 0, nullptr);
    Assert(CurrentConnection >= 0, 0, nullptr);

    if (session == nullptr)
    {
        DirectPlay->EnumPlayers(nullptr, EnumPlayersCallback, this, 0);
    }
    else
    {
        DirectPlay->EnumPlayers(&session->SessionDesc.guidInstance, EnumPlayersCallback, this, DPENUMPLAYERS_SESSION);
    }

    Players.Current = Players.HeadLink;

    if (CurrentSession != nullptr)
    {
        CurrentSession->SessionDesc.dwCurrentPlayers = Players.Count;
    }
}

MCFLinkedList<MCFidpGroup>* MCSessionManager::GetGroups(MCFidpSession* session)
{
    MCFidpGroup::ClearList(Groups);
    Assert(DirectPlay != nullptr, 0, nullptr);
    Assert(CurrentConnection >= 0, 0, nullptr);

    if (session == nullptr)
    {
        DirectPlay->EnumGroups(nullptr, EnumGroupsCallback, this, 0);
    }
    else
    {
        // Original behaviour: a listed session's groups are asked for with EnumPlayers (its players come back).
        DirectPlay->EnumPlayers(&session->SessionDesc.guidInstance, EnumGroupsCallback, this, DPENUMPLAYERS_SESSION);
    }

    Groups.Current = Groups.HeadLink;
    return &Groups;
}

int32_t MCSessionManager::Dial()
{
    if (CurrentConnection != PROTOCOL_MODEM)
    {
        return 1;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = ThisAppGuid;
    const int32_t result = static_cast<int32_t>(
        DirectPlay->EnumSessions(&desc, 0, EnumSessionsCallback, this,
                                 DPENUMSESSIONS_AVAILABLE | DPENUMSESSIONS_ASYNC | DPENUMSESSIONS_PASSWORDREQUIRED |
                                     DPENUMSESSIONS_RETURNSTATUS));

    if (result == 0 && DisabledCallerID != 0)
    {
        ReEnableCallerID();
        DisabledCallerID = 0;
    }

    return result;
}

void MCSessionManager::CancelDialing()
{
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();
}

// ---- the per-frame pump ------------------------------------------------------------------------------------------

int MCSessionManager::SendVerifies()
{
    for (int i = 0; i < 6; i++)
    {
        const MCFIVerifyMessage* verify = reinterpret_cast<MCFIVerifyMessage*>(VerifyMessages[i]);

        if (verify->Count != 0)
        {
            MCFidpPlayer* player = GetPlayerNumber(i);

            if (player != nullptr)
            {
                SendMessageA(player->Id, reinterpret_cast<MCFIMessageHeader*>(VerifyMessages[i]),
                             verify->Count * 6 + 3);
            }
        }
    }

    for (int i = 0; i < 6; i++)
    {
        ResetVerify(VerifyMessages[i]);
    }

    return 0;
}

void MCSessionManager::ProcessMessages()
{
    if (MyPlayerID == 0)
    {
        return;
    }

    if (HasPlayerNumber != 0 && Players.Count > 1 && IsHost != 0)
    {
        const uint32_t now = PerformanceTicks();

        // Port fix: compared as a signed difference; the low 32 bits of a modern performance counter wrap every few
        // minutes, and a plain "next < now" then stopped (or flooded) the pings until it wrapped again.
        if (static_cast<int32_t>(now - NextPingTime) > 0)
        {
            SendPing();
            NextPingTime = now + PingInterval * TicksPerMS;
        }
    }

    std::lock_guard lock(CriticalSection);
    ReceiveThread();
    UpdateGuaranteedMessages();

    if (ProcessSystemMessages() != -1)
    {
        ProcessApplicationMessages();
    }
}

int MCSessionManager::ProcessSystemMessages()
{
    const int numMessages = SystemMessages->Size();
    int i = 0;

    for (; i < numMessages; i++)
    {
        MCFidpMessage* msg = SystemMessages->Head();
        HandlePreSystemMessage(msg);
        SystemMessages->TossHead();

        if (SystemCallback != nullptr)
        {
            SystemCallback(msg, SystemCallbackData);
        }

        if (reinterpret_cast<DPMSG_GENERIC*>(msg->MessageBuffer)->dwType == DPSYS_SESSIONLOST)
        {
            return -1;
        }

        HandlePostSystemMessage(msg);
        AddMessageToEmptyQueue(msg);
    }

    return i;
}

void MCSessionManager::ProcessGuaranteedMessages()
{
    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        MCFidpPlayer* player = link->Data;

        if (player == MyPlayer)
        {
            continue;
        }

        while (MCFidpMessage* msg = player->NextMessageToProcess())
        {
            if (msg->MessageBuffer == nullptr)
            {
                delete msg;
                continue;
            }

            HandleApplicationMessage(msg);

            if (TypeOf(msg->MessageBuffer) == 9)
            {
                return;
            }
        }
    }
}

int MCSessionManager::ProcessApplicationMessages()
{
    const int numMessages = ApplicationMessages->Size();
    MCFidpMessage* msg = ApplicationMessages->Head();

    while (msg != nullptr)
    {
        HandleApplicationMessage(msg);

        if (TypeOf(msg->MessageBuffer) == 9)
        {
            return -1;
        }

        ApplicationMessages->TossHead();
        msg = ApplicationMessages->Head();
    }

    ProcessGuaranteedMessages();
    return numMessages;
}

int MCSessionManager::ReceiveThread()
{
    if (MyPlayer == nullptr)
    {
        return -1;
    }

    InReceiveThread = 1;

    for (int i = 0; i < 6; i++)
    {
        ResetVerify(VerifyMessages[i]);
    }

    int32_t result;

    do
    {
        MCFidpMessage* msg = GetMessageFromEmptyQueue();

        if (msg == nullptr)
        {
            return 0;
        }

        result = msg->ReceiveMessage(DirectPlay);

        if (result == 0)
        {
            if (msg->FromID == DPID_SYSMSG)
            {
                SystemMessages->Add(msg);
            }
            else
            {
                RTProcessApplicationMessage(msg);
            }
        }
        else
        {
            AddMessageToEmptyQueue(msg);
        }
    } while (result == 0);

    for (int i = 0; i < 6; i++)
    {
        const MCFIVerifyMessage* verify = reinterpret_cast<MCFIVerifyMessage*>(VerifyMessages[i]);

        if (verify->Count != 0)
        {
            SendMessageA(RTGetIDFromPlayerNumber(i), reinterpret_cast<MCFIMessageHeader*>(VerifyMessages[i]),
                         verify->Count * 6 + 3);
        }
    }

    InReceiveThread = 0;
    return 0;
}

void MCSessionManager::RTProcessApplicationMessage(MCFidpMessage* msg)
{
    MCFidpPlayer* sender = RTGetPlayer(msg->FromID);

    if (sender == nullptr)
    {
        AddMessageToEmptyQueue(msg);
        return;
    }

    uint8_t* buffer = msg->MessageBuffer;
    const uint16_t header = HeaderOf(buffer);
    const uint16_t type = header & FIMSG_TYPE_MASK;

    if (HasPlayerNumber != 0)
    {
        if ((header & FIMSG_GUARANTEED) == 0 || type == 9)
        {
            if (type == 1)
            {
                const MCFIVerifyMessage* verify = reinterpret_cast<MCFIVerifyMessage*>(buffer);

                for (int i = 0; i < verify->Count; i++)
                {
                    MCFidpMessage* verified = sender->RemoveFromVerifyList(verify->Entries[i][MyPlayer->PlayerNumber]);

                    if (verified != nullptr)
                    {
                        AddMessageToEmptyQueue(verified);
                    }
                }

                AddMessageToEmptyQueue(msg);
            }
            else
            {
                ApplicationMessages->Add(msg);
            }
        }
        else
        {
            RTHandleNewGuaranteedMessage(msg, sender);
        }

        return;
    }

    if (type != 2)
    {
        if ((header & FIMSG_GUARANTEED) == 0 || type == 9)
        {
            ApplicationMessages->Add(msg);
        }
        else if (LaunchedFromLobby == 0)
        {
            PreIDReceivedMessages->Add(msg);
        }
        else
        {
            AddMessageToEmptyQueue(msg);
        }

        return;
    }

    // The server's player numbers: this machine (and everyone it knows) gets its number.
    const MCFIPlayerNumbersMessage* numbers = reinterpret_cast<MCFIPlayerNumbersMessage*>(buffer);
    const int numPlayers = Players.Count;

    if (LaunchedFromLobby == 0)
    {
        for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
        {
            MCFidpPlayer* player = link->Data;
            int number = -1;

            for (int i = 0; i < 6; i++)
            {
                if (numbers->PlayerIDs[i] == player->Id)
                {
                    number = i;
                    break;
                }
            }

            player->PlayerNumber = number;
            player->HasPlayerNumber = 1;

            if (player->PlayerNumber > 5)
            {
                return;
            }
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (numbers->PlayerIDs[i] == 0)
            {
                continue;
            }

            MCFidpPlayer* player = GetPlayer(numbers->PlayerIDs[i]);

            if (player == nullptr)
            {
                NewPlayerNumbers[i] = numbers->PlayerIDs[i];
            }
            else
            {
                player->PlayerNumber = i;
                player->HasPlayerNumber = 1;
            }
        }
    }

    ServerID = numbers->PlayerIDs[numbers->ServerNumber];

    if (MyPlayer->PlayerNumber == -1)
    {
        AddMessageToEmptyQueue(msg);
        return;
    }

    HasPlayerNumber = 1;

    if (LaunchedFromLobby == 0)
    {
        int numbered = 0;

        for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
        {
            MCFidpPlayer* player = link->Data;

            if (player->PlayerNumber == -1)
            {
                GivePlayerAnID(player);

                if (player->PlayerNumber < 0 || player->PlayerNumber > 5)
                {
                    return;
                }
            }

            numbered++;
        }

        if (numbered < numPlayers)
        {
            return;
        }
    }

    const uint8_t sendCount = numbers->Tagger.SendCount[MyPlayer->PlayerNumber];
    sender->HandleIncomingMessage(msg, sendCount);
    sender->NextMessageToProcess();
    uint8_t* verify = VerifyMessages[sender->PlayerNumber];
    MCFIVerifyMessage* verifyMessage = reinterpret_cast<MCFIVerifyMessage*>(verify);
    std::memset(verifyMessage->Entries[verifyMessage->Count], 0, 6);
    verifyMessage->Entries[verifyMessage->Count][sender->PlayerNumber] = sendCount;
    verifyMessage->Count++;
    AddMessageToEmptyQueue(msg);

    // The guaranteed messages that came before the numbers can be put in order now.
    const int numEarly = PreIDReceivedMessages->Size();

    for (int i = 0; i < numEarly; i++)
    {
        MCFidpMessage* early = PreIDReceivedMessages->Head();
        MCFidpPlayer* earlySender = RTGetPlayer(early->FromID);

        if (earlySender == nullptr)
        {
            AddMessageToEmptyQueue(early);
        }
        else
        {
            RTHandleNewGuaranteedMessage(early, earlySender);
        }

        PreIDReceivedMessages->TossHead();
    }

    SendPreIDGuaranteedMessages();
}

void MCSessionManager::RTHandleNewGuaranteedMessage(MCFidpMessage* msg, MCFidpPlayer* player)
{
    int stored = 0;
    const uint8_t sendCount = msg->MessageBuffer[2 + MyPlayer->PlayerNumber];

    if (player == nullptr || player->PlayerNumber >= 6 || player->PlayerNumber < 0)
    {
        return;
    }

    MCFIVerifyMessage* verify = reinterpret_cast<MCFIVerifyMessage*>(VerifyMessages[player->PlayerNumber]);

    if (verify->Count < 0x28)
    {
        std::memset(verify->Entries[verify->Count], 0, 6);
        verify->Entries[verify->Count][player->PlayerNumber] = sendCount;
        verify->Count++;
        stored = player->HandleIncomingMessage(msg, sendCount);
    }

    if (stored == 0)
    {
        AddMessageToEmptyQueue(msg);
    }
}

uint32_t MCSessionManager::RTGetIDFromPlayerNumber(int playerNumber)
{
    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data->PlayerNumber == playerNumber)
        {
            return link->Data->Id;
        }
    }

    return 0;
}

MCFidpPlayer* MCSessionManager::RTGetPlayer(uint32_t playerID)
{
    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data->Id == playerID)
        {
            return link->Data;
        }
    }

    return nullptr;
}

void MCSessionManager::UpdatePlayerGuaranteedMessages(MCFidpPlayer* player, uint32_t now)
{
    player->VerifyList.Current = player->VerifyList.HeadLink;
    const int numMessages = player->VerifyList.Count;

    for (int i = 0; i < numMessages; i++)
    {
        MCFidpMessage* msg = player->VerifyList.ReadAndNext();

        if ((player->HasPlayerNumber != 0 || TypeOf(msg->MessageBuffer) == 9) &&
            player->ResendDelay * msg->TimesSent < (now - msg->SendTime) / TicksPerMS)
        {
            SendMessageA(player->Id, reinterpret_cast<MCFIMessageHeader*>(msg->MessageBuffer), msg->MessageSize);
            msg->WasResent = 1;
            msg->TimesSent++;
            msg->SendTime = now;
        }
    }
}

void MCSessionManager::UpdateGuaranteedMessages()
{
    const uint32_t now = PerformanceTicks();

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data != MyPlayer)
        {
            UpdatePlayerGuaranteedMessages(link->Data, now);
        }
    }
}

// Nothing in MCX.EXE calls this (OB-107).
void MCSessionManager::UpdateFileTransfers()
{
    const int numTransfers = OutgoingFiles.Count;
    OutgoingFiles.Current = OutgoingFiles.HeadLink;

    for (int i = 0; i < numTransfers; i++)
    {
        MCFileTransferInfo* transfer = OutgoingFiles.Current->Data;
        const int finished = transfer->PrepareNextMessage();
        SendMessageFromInfo(transfer->Message);

        if (finished != 0)
        {
            OutgoingFiles.Del(transfer);

            if (transfer->Callback != nullptr)
            {
                transfer->Callback(transfer->FileName, nullptr);
            }

            delete transfer;
        }
    }
}

// ---- sending -----------------------------------------------------------------------------------------------------

void MCSessionManager::SetupMessageSendCounts(MCFIGuaranteedMessageHeader* header, MCFLinkedList<MCFidpPlayer>* list)
{
    for (MCFLink<MCFidpPlayer>* link = list->HeadLink; link != nullptr; link = link->Next)
    {
        MCFidpPlayer* player = link->Data;

        if (player != MyPlayer && player->HasPlayerNumber != 0 && player->PlayerNumber != -1 &&
            player->PlayerNumber < 6)
        {
            player->OutgoingSendCount++;
            header->Tagger.SendCount[player->PlayerNumber] = player->OutgoingSendCount;
        }
    }

    header->Header |= FIMSG_GROUP_MESSAGE;
    header->Header |= FIMSG_GUARANTEED;
}

void MCSessionManager::StartGame()
{
    MCFIGuaranteedMessageHeader msg;
    msg.Tagger.Clear();
    msg.Header = 0x1005;
    SendMessageToGroup(0, &msg, sizeof(msg));
    GameStarted = 1;
}

int32_t MCSessionManager::SendPing()
{
    // Port fix: cleared (the original sent the uninitialised rest of a stack buffer, its header included).
    uint8_t buffer[256] = {};
    MCFIPingMessage* ping = reinterpret_cast<MCFIPingMessage*>(buffer);
    ping->Header |= 10;

    if (IsHost == 0)
    {
        SendMessageToGroup(0, ping, 9);
        return 0;
    }

    int32_t numbers[6];
    size_t count = 0;

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        // Port fix: bounded to the six numbers the message holds.
        if (link->Data != MyPlayer && count < 6)
        {
            numbers[count] = link->Data->PlayerNumber;
            count++;
        }
    }

    Assert(count == static_cast<size_t>(Players.Count - 1), 0, "nPlayers is incorrect");
    std::qsort(numbers, count, sizeof(int32_t), [](const void* a, const void* b)
               { return CompareLatencies(static_cast<const int32_t*>(a), static_cast<const int32_t*>(b)); });
    ping->Count = static_cast<uint8_t>(count);

    for (size_t i = 0; i < count; i++)
    {
        ping->PlayerNumbers[i] = static_cast<uint8_t>(numbers[i]);
    }

    SendMessageToGroup(0, ping, ping->Count + 9);
    return 0;
}

void MCSessionManager::SendPlayersInGroupMessages(uint32_t groupID)
{
    MCFIPlayersInGroupMessage* msg = static_cast<MCFIPlayersInGroupMessage*>(LinkUpBlocks->Allocate(300));
    Groups.Current = Groups.HeadLink;

    for (int i = 0; i < Groups.Count; i++)
    {
        std::memset(msg, 0, 300);
        msg->Header = static_cast<uint16_t>((msg->Header & ~FIMSG_TYPE_MASK) | 3);
        MCFidpGroup* group = Groups.ReadAndNext();
        msg->GroupID = group->Id;
        group->Players.Current = group->Players.HeadLink;

        for (int j = 0; j < group->Players.Count; j++)
        {
            msg->PlayerIDs[j] = *group->Players.ReadAndNext();
        }

        if (groupID == 0)
        {
            SendMessageToGroup(0, msg, 0x24);
        }
        else
        {
            SendMessageToPlayerGuaranteed(groupID, msg, 0x24, 1);
        }
    }

    LinkUpBlocks->Free(msg);
}

void MCSessionManager::SendPreIDGuaranteedMessages()
{
    PreIDGroupMessages->Head();
    const int numGroupMessages = PreIDGroupMessages->Size();

    for (int i = 0; i < numGroupMessages; i++)
    {
        MCFidpMessage* msg = PreIDGroupMessages->Head();
        SendMessageFromInfo(msg);
        AddMessageToEmptyQueue(msg);
        PreIDGroupMessages->TossHead();
    }

    const int numServerMessages = PreIDServerMessages->Size();
    PreIDServerMessages->Head();

    for (int i = 0; i < numServerMessages; i++)
    {
        MCFidpMessage* msg = PreIDServerMessages->Head();
        MCFIGuaranteedMessageHeader* header = reinterpret_cast<MCFIGuaranteedMessageHeader*>(msg->MessageBuffer);

        // Original behaviour: these messages are not returned to the free queue.
        if ((header->Header & FIMSG_GUARANTEED) == 0)
        {
            SendMessageToServer(header, msg->MessageSize);
        }
        else
        {
            SendMessageToServerGuaranteed(header, msg->MessageSize);
        }

        PreIDServerMessages->TossHead();
    }
}

void MCSessionManager::SendMessageToGroup(uint32_t groupID, MCFIGuaranteedMessageHeader* header, uint32_t size)
{
    if (HasPlayerNumber == 0)
    {
        if (LaunchedFromLobby == 0)
        {
            std::lock_guard lock(CriticalSection);
            MCFidpMessage* msg = GetMessageFromEmptyQueue();
            header->Header |= FIMSG_GROUP_MESSAGE;
            msg->SetMessageBuffer(header, size);
            msg->ToID = groupID;
            PreIDGroupMessages->Add(msg);
        }

        return;
    }

    MCFLinkedList<MCFidpPlayer>* list;

    if (groupID == 0)
    {
        list = &Players;
    }
    else
    {
        list = new MCFLinkedList<MCFidpPlayer>();
        GetPlayerListForGroup(groupID, list);
    }

    if (SessionLocked == 0 || LaunchedFromLobby == 0)
    {
        for (MCFLink<MCFidpPlayer>* link = list->HeadLink; link != nullptr; link = link->Next)
        {
            // Port fix: a member the session no longer knows is listed as null; skipped.
            if (link->Data != MyPlayer && link->Data != nullptr)
            {
                SendMessageToPlayerGuaranteed(link->Data->Id, header, size, 1);
            }
        }
    }
    else
    {
        for (MCFLink<MCFidpPlayer>* link = list->HeadLink; link != nullptr; link = link->Next)
        {
            if (link->Data != MyPlayer && link->Data != nullptr && link->Data->IsVerifyListFull() != 0)
            {
                RemovePlayerFromGame(link->Data);
            }
        }

        std::lock_guard lock(CriticalSection);
        SetupMessageSendCounts(header, list);

        if (SendMessageA(groupID, header, size) == 0)
        {
            for (MCFLink<MCFidpPlayer>* link = list->HeadLink; link != nullptr; link = link->Next)
            {
                MCFidpPlayer* player = link->Data;

                if (player != nullptr && player->HasPlayerNumber != 0 && player != MyPlayer)
                {
                    const uint32_t playerID = player->Id;
                    MCFidpMessage* msg = GetMessageFromEmptyQueue();
                    header->Header &= ~FIMSG_GROUP_MESSAGE;
                    msg->SetMessageBuffer(header, size);
                    msg->ToID = playerID;
                    player->AddToVerifyList(msg);
                }
            }
        }
    }

    if (groupID != 0 && list != nullptr)
    {
        delete list;
    }
}

void MCSessionManager::GetPlayerListForGroup(uint32_t groupID, MCFLinkedList<MCFidpPlayer>* list)
{
    MCFidpGroup* group = GetGroup(groupID);
    Assert(group != nullptr, 0, "Group does not exist");
    group->Players.Current = group->Players.HeadLink;
    const int numPlayers = group->Players.Count;

    for (int i = 0; i < numPlayers; i++)
    {
        const uint32_t* playerID = group->Players.ReadAndNext();
        list->Add(GetPlayer(*playerID));
    }
}

uint32_t MCSessionManager::TallyLatencies()
{
    uint32_t total = 0;

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data != MyPlayer)
        {
            total += link->Data->AverageLatency();
        }
    }

    if (Players.Count == 1)
    {
        return 0;
    }

    return total / static_cast<uint32_t>(Players.Count - 1);
}

void MCSessionManager::ProcessLatencyMessage(MCFIMessageHeader* msg, uint32_t fromID)
{
    bool allReported = true;
    MCFidpPlayer* player = GetPlayer(fromID);
    Assert(player != nullptr, 0, "ProcessLatencyMessage - null player");
    player->ReportedLatency = reinterpret_cast<MCFIValueMessage*>(msg)->Value;

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data->ReportedLatency == 0)
        {
            allReported = false;
            break;
        }
    }

    LatencyReportsIn = allReported ? 1 : 0;
}

void MCSessionManager::SendLatencyInfo()
{
    if (IsHost == 0)
    {
        MCFIValueMessage msg;
        msg.Value = TallyLatencies();
        msg.Tagger.Clear();
        msg.Header = 0x100c;
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
    }
    else
    {
        MyPlayer->ReportedLatency = TallyLatencies();

        if (MyPlayer->ReportedLatency == 0)
        {
            MyPlayer->ReportedLatency = 1000;
        }
    }
}

void MCSessionManager::SwitchServers()
{
    if (IsHost == 0 || LaunchedFromLobby != 0)
    {
        return;
    }

    uint32_t mostMemory = 100000;
    MCFidpPlayer* best = Players.HeadLink != nullptr ? Players.HeadLink->Data : nullptr;

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (mostMemory < link->Data->TotalPhysicalMemory)
        {
            best = link->Data;
            mostMemory = link->Data->TotalPhysicalMemory;
        }
    }

    if (best != MyPlayer)
    {
        ServerID = best->Id;
        IsHost = 0;
        static_cast<MCFIValueMessage*>(ServerMessage)->Value = ServerID;
        SendMessageToGroup(0, ServerMessage, sizeof(MCFIValueMessage));
    }
}

void MCSessionManager::SendMessageToPlayerGuaranteed(uint32_t playerID, MCFIGuaranteedMessageHeader* header,
                                                     uint32_t size, int firstSend)
{
    if (HasPlayerNumber == 0)
    {
        if (LaunchedFromLobby == 0)
        {
            std::lock_guard lock(CriticalSection);
            MCFidpMessage* msg = GetMessageFromEmptyQueue();
            header->Header &= ~FIMSG_GROUP_MESSAGE;
            msg->SetMessageBuffer(header, size);
            msg->ToID = playerID;
            PreIDGroupMessages->Add(msg);
        }

        return;
    }

    MCFidpPlayer* player = GetPlayer(playerID);

    if (player == MyPlayer || player == nullptr)
    {
        return;
    }

    if ((header->Header & FIMSG_TYPE_MASK) != 9 &&
        (player->HasPlayerNumber == 0 || (player->IsVerifyListFull() != 0 && RemovePlayerFromGame(player) == 0)))
    {
        return;
    }

    std::lock_guard lock(CriticalSection);

    if (firstSend != 0 && player->PlayerNumber >= 0 && player->PlayerNumber < 6)
    {
        player->OutgoingSendCount++;
        header->Tagger.SendCount[player->PlayerNumber] = player->OutgoingSendCount;
    }

    header->Header |= FIMSG_GUARANTEED;
    header->Header &= ~FIMSG_GROUP_MESSAGE;
    const uint32_t sendTime = MCPort::Milliseconds();
    const int32_t result = SendMessageA(playerID, header, size);
    MCFidpMessage* msg = GetMessageFromEmptyQueue();
    header->Header &= ~FIMSG_GROUP_MESSAGE;
    msg->SetMessageBuffer(header, size);
    msg->ToID = playerID;
    player->AddToVerifyList(msg);

    if (result != 0)
    {
        // Original behaviour: a failed send is backdated so it is resent at once; the time is timeGetTime's (ms)
        // where the verify list keeps performance-counter ticks.
        msg->SendTime = sendTime - player->ResendDelay * TicksPerMS;

        if (msg->WasResent == 0)
        {
            msg->FirstSendTime = msg->SendTime;
        }
    }
}

void MCSessionManager::SendMessageToServerGuaranteed(MCFIGuaranteedMessageHeader* header, uint32_t size)
{
    if (HasPlayerNumber == 0)
    {
        std::lock_guard lock(CriticalSection);
        MCFidpMessage* msg = GetMessageFromEmptyQueue();
        header->Header &= ~FIMSG_GROUP_MESSAGE;
        msg->SetMessageBuffer(header, size);
        msg->ToID = ServerID;
        PreIDServerMessages->Add(msg);
        return;
    }

    SendMessageToPlayerGuaranteed(ServerID, header, size, 1);
}

void MCSessionManager::BroadcastMessage(MCFIMessageHeader* header, uint32_t size)
{
    SendMessageA(0, header, size);
}

void MCSessionManager::SendMessageToServer(MCFIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(CriticalSection);

    if (HasPlayerNumber == 0)
    {
        MCFidpMessage* msg = GetMessageFromEmptyQueue();
        header->Header &= ~FIMSG_GROUP_MESSAGE;
        msg->SetMessageBuffer(header, size);
        msg->ToID = ServerID;
        PreIDServerMessages->Add(msg);
    }
    else if (ServerID != MyPlayerID)
    {
        SendMessageA(ServerID, header, size);
    }
}

int32_t MCSessionManager::SendMessageA(uint32_t toID, MCFIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(CriticalSection);

    if (size > 0x200)
    {
        size = 0x200;
    }

    const uint32_t result = DirectPlay->Send(MyPlayerID, toID, 0, header, size);
    ReportError(result);
    return static_cast<int32_t>(result);
}

int MCSessionManager::BroadcastFile(char* fileName, char* directory, void (*callback)(char* fileName, void* data))
{
    MCFileTransferInfo* transfer =
        new MCFileTransferInfo(MyPlayerID, 0, fileName, directory, 0, MCFileTransferInfo::TRANSFER_SEND);
    transfer->Callback = callback;
    transfer->FileID = NextFileID;
    NextFileID++;

    if (NextFileID > 0xff)
    {
        NextFileID = 0;
    }

    OutgoingFiles.Add(transfer);
    int size;
    MCFIBeginFileTransferMessage* begin = transfer->CreateBeginTransferMessage(size);
    BroadcastMessage(begin, static_cast<uint32_t>(size));
    LinkUpBlocks->Free(begin);
    return NextFileID - 1;
}

void MCSessionManager::SendMessageFromInfo(MCFidpMessage* msg)
{
    MCFIGuaranteedMessageHeader* header = reinterpret_cast<MCFIGuaranteedMessageHeader*>(msg->MessageBuffer);

    if (msg->ToID == 0)
    {
        SendMessageToGroup(0, header, msg->MessageSize);
    }
    else if ((header->Header & FIMSG_GROUP_MESSAGE) != 0)
    {
        SendMessageToGroup(msg->ToID, header, msg->MessageSize);
    }
    else if ((header->Header & FIMSG_GUARANTEED) == 0)
    {
        SendMessageA(msg->ToID, header, msg->MessageSize);
    }
    else
    {
        SendMessageToPlayerGuaranteed(msg->ToID, header, msg->MessageSize, 1);
    }
}

int32_t MCSessionManager::GetAverageBandwidth(int*)
{
    if (CurrentSession == nullptr)
    {
        return -1;
    }

    return -3;
}

MCSessionManager* MCSessionManager::GetGlobalPointer(void* owner)
{
    if (InstanceExists == 0)
    {
        return nullptr;
    }

    GlobalPointerHolder = owner;
    return Instance;
}

int MCSessionManager::ReleaseGlobalPointer(void* owner)
{
    const bool held = owner == GlobalPointerHolder;

    if (held)
    {
        GlobalPointerHolder = nullptr;
    }

    return held;
}

// ---- players coming and going ------------------------------------------------------------------------------------

void MCSessionManager::GivePlayerAnID(MCFidpPlayer* player)
{
    // numbers[0] is a sentinel; numbers[1..count] are the listed players' numbers, sorted.
    int32_t numbers[7];

    for (int i = 0; i < 6; i++)
    {
        numbers[i + 1] = 30000;
    }

    numbers[0] = 0;
    // Port fix: at most six numbers fit (the original would write past the array with more players listed).
    const int count = std::min(Players.Count, 6);
    MCFLink<MCFidpPlayer>* link = Players.HeadLink;

    for (int i = 0; i < count; i++)
    {
        numbers[i + 1] = link != nullptr ? link->Data->PlayerNumber : 0;
        Assert(link != nullptr, 0, nullptr);
        link = link->Next;
    }

    std::qsort(numbers + 1, count, sizeof(int32_t), [](const void* a, const void* b)
               { return CompareLongs(static_cast<const int32_t*>(a), static_cast<const int32_t*>(b)); });
    int32_t number = numbers[count] + 1;

    for (int i = 1; i < count; i++)
    {
        if (numbers[i] + 1 < numbers[i + 1])
        {
            number = numbers[i] + 1;
            break;
        }
    }

    player->PlayerNumber = number;
    player->HasPlayerNumber = 1;
}

void MCSessionManager::AddPlayerOrGroup(uint32_t playerType, uint32_t id, uint32_t parentID, DPNAME* name,
                                        uint32_t flags)
{
    MCFLink<MCFidpPlayer>* firstLink = Players.HeadLink;

    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        Groups.Current = Groups.HeadLink;

        for (int i = 0; i < Groups.Count; i++)
        {
            if (Groups.ReadAndNext()->Id == id)
            {
                return;
            }
        }

        Groups.Add(new MCFidpGroup(id, parentID, name, flags));
        return;
    }

    if (GetPlayer(id) != nullptr)
    {
        return;
    }

    MCFidpPlayer* player = new MCFidpPlayer(id, name, flags);
    Assert(player != nullptr, 0, "Player is null");

    if (HasPlayerNumber == 0)
    {
        PendingPlayers.Add(player);
    }
    else if (IsHost != 0 || LaunchedFromLobby == 0)
    {
        GivePlayerAnID(player);

        if (player->PlayerNumber < 0 || player->PlayerNumber > 5)
        {
            Fatal(player->PlayerNumber, "Could not connect to game.");
        }
    }

    Players.Add(player);

    if (IsHost == 0 && LaunchedFromLobby != 0)
    {
        player->HasPlayerNumber = 0;
    }

    if (LaunchedFromLobby != 0)
    {
        for (int i = 0; i < 6; i++)
        {
            if (NewPlayerNumbers[i] == id)
            {
                player->PlayerNumber = i;
                player->HasPlayerNumber = 1;
            }
        }
    }

    CurrentSession->SessionDesc.dwCurrentPlayers = Players.Count;

    if (IsHost == 0)
    {
        return;
    }

    // The server tells the new player (or, in a lobby game, everyone) who has which number.
    MCFIPlayerNumbersMessage numbersMessage = {};
    MCFIPlayerNumbersMessage* numbers = &numbersMessage;
    numbers->Header = 0;
    numbers->Header |= FIMSG_GUARANTEED;
    numbers->Tagger.Clear();
    numbers->Header = static_cast<uint16_t>((numbers->Header & ~FIMSG_TYPE_MASK) | 2);
    numbers->ServerNumber = 0;

    for (int i = 0; i < 6; i++)
    {
        numbers->PlayerIDs[i] = 0;
    }

    for (MCFLink<MCFidpPlayer>* link = firstLink; link != nullptr; link = link->Next)
    {
        MCFidpPlayer* listed = link->Data;

        // Port fix: an unnumbered player is skipped (the original wrote it at index -1, into the send counters).
        if (listed->PlayerNumber >= 0 && listed->PlayerNumber < 6)
        {
            numbers->PlayerIDs[listed->PlayerNumber] = listed->Id;
        }

        if (listed->Id == MyPlayerID)
        {
            numbers->ServerNumber = static_cast<uint8_t>(listed->PlayerNumber);
        }
    }

    if (LaunchedFromLobby == 0)
    {
        SendMessageToPlayerGuaranteed(player->Id, numbers, sizeof(MCFIPlayerNumbersMessage), 1);
    }
    else
    {
        SendMessageToGroup(0, numbers, sizeof(MCFIPlayerNumbersMessage));
    }

    SendPlayersInGroupMessages(player->Id);
}

MCFidpPlayer* MCSessionManager::GetPlayerNumber(int32_t playerNumber)
{
    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data->PlayerNumber == playerNumber)
        {
            return link->Data;
        }
    }

    return nullptr;
}

void MCSessionManager::PlayerOrGroupLeaving(uint32_t playerType, uint32_t id)
{
    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        return;
    }

    MCFidpPlayer* player = GetPlayer(id);

    if (player == nullptr)
    {
        return;
    }

    player->HasPlayerNumber = 0;

    if (player->Id != ServerID)
    {
        return;
    }

    // The server left: the next player by the server's latency order takes over.
    MCFidpPlayer* next = nullptr;

    // Port fix: bounded to the six numbers (the original ran on past the array when nobody was left).
    for (int i = 0; next == nullptr && i < 6; i++)
    {
        next = GetPlayerNumber(PlayersByLatency[i]);

        if (next != nullptr && next->HasPlayerNumber == 0)
        {
            next = nullptr;
        }
    }

    Assert(next != nullptr, 0, "No more players");

    if (next == nullptr)
    {
        return;
    }

    if (next == MyPlayer)
    {
        IsHost = 1;
    }

    ServerID = next->Id;
}

void MCSessionManager::DeletePlayerOrGroup(uint32_t playerType, uint32_t id)
{
    if (playerType == DPPLAYERTYPE_PLAYER)
    {
        MCFidpPlayer* player = GetPlayer(id);

        if (player == nullptr)
        {
            return;
        }

        Players.Del(player);
        delete player;
        CurrentSession->SessionDesc.dwCurrentPlayers = Players.Count;

        // Original behaviour: only slots already in use are overwritten, and the constructor leaves them all unused,
        // so no id is ever recorded.
        for (int i = 0; i < 6; i++)
        {
            if (DeletedPlayerIDs[i] != 0xffffffffu)
            {
                DeletedPlayerIDs[i] = id;
            }
        }

        return;
    }

    MCFidpGroup* group = GetGroup(id);

    if (group != nullptr)
    {
        // Original behaviour: the group leaves the list but is not deleted.
        Groups.Del(group);
    }
}

int MCSessionManager::IsTcpAvailable()
{
    // Port: the original looked for the TCP/IP stack in the registry (Enum\Network\MSTCP, or the Tcpip service).
    // The port's transport is TCP/IP: available when the sockets library starts.
    if (TcpChecked == 0)
    {
        TcpAvailable = MCSocket::Startup() ? 1 : 0;
        TcpChecked = 1;
    }

    return TcpAvailable;
}

int MCSessionManager::IsIpxAvailable()
{
    // Port: the original looked for the IPX stack in the registry (Enum\Network\NWLINK, or the NwlnkIpx service).
    // The port's "IPX" connection is its LAN search over TCP/IP (see MCDirectPlay), available with TCP/IP.
    if (IpxChecked == 0)
    {
        IpxAvailable = MCSocket::Startup() ? 1 : 0;
        IpxChecked = 1;
    }

    return IpxAvailable;
}

int MCSessionManager::IsModemAvailable()
{
    // Original behaviour: modemChecked is never set, so every call looks for modems again.
    if (ModemChecked == 0)
    {
        if (FindModems() == 0)
        {
            ModemAvailable = GetModemName(0) != nullptr ? 1 : 0;
        }
        else
        {
            ModemAvailable = 0;
        }
    }

    return ModemAvailable;
}

void MCSessionManager::HandlePreSystemMessage(MCFidpMessage* msg)
{
    const uint32_t type = reinterpret_cast<DPMSG_GENERIC*>(msg->MessageBuffer)->dwType;

    if (type == DPSYS_ADDPLAYERTOGROUP)
    {
        DPMSG_ADDPLAYERTOGROUP* added = reinterpret_cast<DPMSG_ADDPLAYERTOGROUP*>(msg->MessageBuffer);
        MCFidpGroup* group = GetGroup(added->dpIdGroup);
        Assert(group != nullptr, 0, "group is null");

        if (group != nullptr && group->AddPlayer(added->dpIdPlayer) != 0)
        {
            MCFidpPlayer* player = GetPlayer(added->dpIdPlayer);

            if (player != nullptr)
            {
                player->JoinGroup(added->dpIdGroup);
            }
        }
    }
    else if (type == DPSYS_CREATEPLAYERORGROUP)
    {
        DPMSG_CREATEPLAYERORGROUP* created = reinterpret_cast<DPMSG_CREATEPLAYERORGROUP*>(msg->MessageBuffer);
        AddPlayerOrGroup(created->dwPlayerType, created->dpId, created->dpIdParent, &created->dpnName,
                         created->dwFlags);
    }
    else if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        DPMSG_DESTROYPLAYERORGROUP* destroyed = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(msg->MessageBuffer);
        PlayerOrGroupLeaving(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_SETSESSIONDESC && LaunchedFromLobby != 0)
    {
        DPMSG_SETSESSIONDESC* changed = reinterpret_cast<DPMSG_SETSESSIONDESC*>(msg->MessageBuffer);

        if ((changed->dpDesc.dwFlags & DPSESSION_NEWPLAYERSDISABLED) != 0)
        {
            SessionLocked = 1;
        }
    }
}

void MCSessionManager::HandlePostSystemMessage(MCFidpMessage* msg)
{
    const uint32_t type = reinterpret_cast<DPMSG_GENERIC*>(msg->MessageBuffer)->dwType;

    if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        DPMSG_DESTROYPLAYERORGROUP* destroyed = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(msg->MessageBuffer);
        DeletePlayerOrGroup(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_DELETEPLAYERFROMGROUP)
    {
        DPMSG_ADDPLAYERTOGROUP* removed = reinterpret_cast<DPMSG_ADDPLAYERTOGROUP*>(msg->MessageBuffer);
        MCFidpGroup* group = GetGroup(removed->dpIdGroup);

        // Port fix: a group that no longer exists is skipped (the original called through a null group).
        if (group != nullptr && group->RemovePlayer(removed->dpIdPlayer) != 0)
        {
            MCFidpPlayer* player = GetPlayer(removed->dpIdPlayer);

            if (player != nullptr)
            {
                player->LeaveGroup(removed->dpIdGroup);
            }
        }
    }
}

void MCSessionManager::HandleApplicationMessage(MCFidpMessage* msg)
{
    bool passOn = true;
    uint8_t* buffer = msg->MessageBuffer;

    switch (TypeOf(buffer))
    {
        case 2:
        {
            if (LaunchedFromLobby != 0)
            {
                const MCFIPlayerNumbersMessage* numbers = reinterpret_cast<MCFIPlayerNumbersMessage*>(buffer);

                for (int i = 0; i < 6; i++)
                {
                    if (numbers->PlayerIDs[i] == 0)
                    {
                        continue;
                    }

                    MCFidpPlayer* player = GetPlayer(numbers->PlayerIDs[i]);

                    if (player == nullptr)
                    {
                        NewPlayerNumbers[i] = numbers->PlayerIDs[i];
                    }
                    else
                    {
                        player->PlayerNumber = i;
                        player->HasPlayerNumber = 1;
                    }
                }
            }

            break;
        }

        case 3:
        {
            const MCFIPlayersInGroupMessage* members = reinterpret_cast<MCFIPlayersInGroupMessage*>(buffer);
            MCFidpGroup* group = GetGroup(members->GroupID);

            if (group != nullptr)
            {
                for (int i = 0; i < 6 && members->PlayerIDs[i] != 0; i++)
                {
                    uint32_t playerID = members->PlayerIDs[i];

                    if (group->AddPlayer(playerID) != 0)
                    {
                        MCFidpPlayer* player = GetPlayer(playerID);

                        if (player != nullptr)
                        {
                            player->JoinGroup(members->GroupID);
                        }
                    }
                }
            }

            passOn = false;
            break;
        }

        case 5:
        {
            GameStarted = 1;
            break;
        }

        case 6:
        {
            ServerID = reinterpret_cast<MCFIValueMessage*>(buffer)->Value;
            IsHost = ServerID == MyPlayerID ? 1 : 0;
            break;
        }

        case 7:
        {
            MCFIBeginFileTransferMessage* begin = reinterpret_cast<MCFIBeginFileTransferMessage*>(buffer);
            char* fileName = std::strtok(begin->FileName, "\\");
            char* directory = std::strtok(nullptr, "");
            MCFileTransferInfo* transfer = new MCFileTransferInfo(
                msg->FromID, MyPlayerID, fileName, directory, begin->FileSize, MCFileTransferInfo::TRANSFER_RECEIVE);
            transfer->FileID = begin->FileID;
            IncomingFiles.Add(transfer);
            passOn = false;
            break;
        }

        case 8:
        {
            const MCFIFileDataMessage* piece = reinterpret_cast<MCFIFileDataMessage*>(buffer);
            const int size = static_cast<int>(msg->MessageSize);
            IncomingFiles.Current = IncomingFiles.HeadLink;

            for (int i = 0; i < IncomingFiles.Count; i++)
            {
                MCFileTransferInfo* transfer = IncomingFiles.ReadAndNext();

                if (static_cast<uint32_t>(transfer->FileID) != piece->FileID)
                {
                    continue;
                }

                if (transfer->AddBytes(const_cast<uint8_t*>(piece->Data), size - 9) != 0)
                {
                    IncomingFiles.Del(transfer);

                    if (FileReceivedCallback != nullptr)
                    {
                        FileReceivedCallback(transfer->FileName, FileReceivedCallbackData);
                    }

                    delete transfer;
                }

                break;
            }

            passOn = false;
            break;
        }

        case 10:
        {
            HandlePingUpdate(msg);
            passOn = false;
            break;
        }

        case 11:
        {
            ProcessSystemInfoMessage(reinterpret_cast<MCFISystemInfoMessage*>(buffer), msg->FromID);
            passOn = false;
            break;
        }

        case 12:
        {
            ProcessLatencyMessage(reinterpret_cast<MCFIMessageHeader*>(buffer), msg->FromID);
            passOn = true;
            break;
        }

        default:
        {
            break;
        }
    }

    if (passOn && ApplicationCallback != nullptr)
    {
        // Port: the original left the critical section around the callback (for its receive thread, which never
        // existed); the port has one thread, and its callers don't always hold the lock, so it is left alone.
        ApplicationCallback(msg, ApplicationCallbackData);
    }

    AddMessageToEmptyQueue(msg);
}

void MCSessionManager::HandlePingUpdate(MCFidpMessage* msg)
{
    if (msg->FromID != ServerID)
    {
        return;
    }

    const MCFIPingMessage* ping = reinterpret_cast<MCFIPingMessage*>(msg->MessageBuffer);

    // Port fix: bounded to the six numbers the message holds.
    for (int i = 0; i < ping->Count && i < 6; i++)
    {
        PlayersByLatency[i] = ping->PlayerNumbers[i];
    }
}

int MCSessionManager::ReportError(uint32_t error)
{
    // The original copied the error's description into a local buffer (a long switch over the DirectPlay and COM
    // codes, else FormatMessage) for a debugger to look at, and dropped it. Only the answer is used.
    return error != 0 ? 1 : 0;
}

void MCSessionManager::GetProfileData(char* fileName)
{
    uint8_t lastVerified = 0xff;
    Players.Current = Players.HeadLink;

    for (int i = 0; i < Players.Count; i++)
    {
        MCFidpPlayer* player = Players.ReadAndNext();

        if (player == MyPlayer)
        {
            continue;
        }

        char line[64];
        std::snprintf(line, sizeof(line), "[%02d]:  SC:%02d ", player->PlayerNumber, player->OutgoingSendCount);
        std::strcat(fileName, line);

        if (player->VerifyList.Count > 0)
        {
            player->VerifyList.Current = player->VerifyList.HeadLink;
            MCFidpMessage* oldest =
                player->VerifyList.HeadLink != nullptr ? player->VerifyList.HeadLink->Data : nullptr;
            lastVerified = oldest->MessageBuffer[2 + player->PlayerNumber];
        }

        std::snprintf(line, sizeof(line), "(%02d) ", lastVerified);
        std::strcat(fileName, line);
        std::snprintf(line, sizeof(line), "Rcv %02d |||  ", player->NextIncomingToProcess);
        std::strcat(fileName, line);
    }
}

int32_t MCSessionManager::GetStats(char* buffer)
{
    if (CurrentSession == nullptr)
    {
        return -1;
    }

    if (IsHost == 0)
    {
        MCFidpPlayer* server = GetPlayer(ServerID);
        std::sprintf(buffer, "Latency to server (%s) = %d", server->Name, server->LastLatency);
        return 0;
    }

    std::sprintf(buffer, "Latencies -- ");

    for (MCFLink<MCFidpPlayer>* link = Players.HeadLink; link != nullptr; link = link->Next)
    {
        if (link->Data != MyPlayer)
        {
            char entry[512];
            std::snprintf(entry, sizeof(entry), "<%s: %4d> ", link->Data->Name, link->Data->LastLatency);
            std::strcat(buffer, entry);
        }
    }

    return 0;
}
