#include "stdafx.h"
#include "linkup/sessionmanager.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/fidpgroup.h"
#include "linkup/filetransferinfo.h"
#include "linkup/session.h"
#include "lib/aerror.h"
#include "platform/MCSocket.h"

std::unique_ptr<MCBlockStore> linkUpBlocks;
_GUID thisAppGUID{};
uint32_t TicksPerMS = 0;
uint32_t StartTime = 0;
std::recursive_mutex AddingMessageList;
uint32_t newPlayerNumbers[6];
int sessionLocked = 0;
int inReceiveThread = 0;
int oldVersionOfMPlayer = 1;
int DisabledCallerID = 0;
int CallerIDChanged[6];
int32_t launchedFromLobby = 0;

namespace
{
    using namespace MCDirectPlayGuids;

    /// <summary>Whether a SessionManager exists (0x0080a680).</summary>
    int instanceExists = 0;
    /// <summary>The SessionManager (0x0080a684).</summary>
    SessionManager* instance = nullptr;
    /// <summary>Who last took the global pointer (0x0080a688).</summary>
    void* globalPointerHolder = nullptr;
    /// <summary>RemovePlayerFromGame's re-entry guard (0x0080a69c).</summary>
    int removingPlayer = 0;

    /// <summary>
    /// Port: stands in for the Win32 events CreatePlayer made (DirectPlay signalled one on every arrival; nothing
    /// ever waited on either), so the fields read as "created".
    /// </summary>
    int eventStandIn = 0;

#pragma pack(push, 1)

    /// <summary>Message type 2 (guaranteed): the server's numbering of the players.</summary>
    /// <remarks>0x21 bytes. The names are the port's.</remarks>
    struct FIPlayerNumbersMessage : FIGuaranteedMessageHeader
    {
        /// <summary>The DPID of player number n (0 = no such player).</summary>
        uint32_t playerIDs[6]{}; // +0x8
        /// <summary>The sending server's own number.</summary>
        uint8_t serverNumber = 0; // +0x20
    };

    static_assert(sizeof(FIPlayerNumbersMessage) == 0x21);

    /// <summary>Message type 3: the players of a group, as the server knows them.</summary>
    /// <remarks>Sent as 0x24 bytes (six members) from a 300-byte buffer. The names are the port's.</remarks>
    struct FIPlayersInGroupMessage : FIGuaranteedMessageHeader
    {
        uint32_t groupID = 0;     // +0x8
        uint32_t playerIDs[72]{}; // +0xc
    };

    /// <summary>Message types 6 (new server), 9 (player removed) and 12 (latency): the header and one 32-bit value.</summary>
    /// <remarks>0xc bytes. The names are the port's.</remarks>
    struct FIValueMessage : FIGuaranteedMessageHeader
    {
        uint32_t value = 0; // +0x8
    };

    static_assert(sizeof(FIValueMessage) == 0xc);

    /// <summary>
    /// Message type 10: the server's ping, with the other players' numbers sorted by latency (the order the next
    /// server is picked in).
    /// </summary>
    /// <remarks>9 + count bytes, built in a 256-byte buffer. The names are the port's.</remarks>
    struct FIPingMessage : FIGuaranteedMessageHeader
    {
        uint8_t count = 0;          // +0x8
        uint8_t playerNumbers[6]{}; // +0x9
    };

    /// <summary>
    /// Message type 1: the numbers of the guaranteed messages received from one player since the last verify. Each
    /// entry is a MessageTagger with only the receiver's slot set.
    /// </summary>
    /// <remarks>3 + count * 6 bytes, built in a 0x2400-byte buffer per player number. The names are the port's.</remarks>
    struct FIVerifyMessage : FIMessageHeader
    {
        uint8_t count = 0;       // +0x2
        uint8_t entries[1][6]{}; // +0x3
    };

#pragma pack(pop)

    /// <summary>The message buffer's header word.</summary>
    uint16_t& HeaderOf(void* buffer)
    {
        return static_cast<FIMessageHeader*>(buffer)->header;
    }

    /// <summary>The message type (bits 0-9) of a buffer.</summary>
    uint16_t TypeOf(const void* buffer)
    {
        return static_cast<const FIMessageHeader*>(buffer)->header & FIMSG_TYPE_MASK;
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
        reinterpret_cast<FIVerifyMessage*>(verify)->count = 0;
    }
}

// ---- free functions ----------------------------------------------------------------------------------------------

void ClearList(FIDPMsgList* list)
{
    list->Size();

    while (list->head != nullptr)
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
    if (linkUpBlocks == nullptr)
    {
        linkUpBlocks = std::make_unique<MCBlockStore>();
    }
}

void DestroyLinkUpBlocks()
{
    if (linkUpBlocks != nullptr)
    {
        linkUpBlocks->Clear();
        linkUpBlocks.reset();
    }
}

int EnumPlayersCallback(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags, void* context)
{
    return static_cast<SessionManager*>(context)->NewPlayerEnumeration(playerID, playerType, name, flags);
}

int EnumGroupsCallback(uint32_t groupID, uint32_t, const DPNAME* name, uint32_t flags, void* context)
{
    static_cast<SessionManager*>(context)->NewGroupEnumeration(groupID, name, flags);
    return 1;
}

uint32_t SessionManagerReceiveThread(void* sessionManager)
{
    return static_cast<uint32_t>(static_cast<SessionManager*>(sessionManager)->ReceiveThread());
}

int EnumConnectionsCallback(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                            uint32_t flags, void* context)
{
    if (serviceProvider == nullptr)
    {
        return 0;
    }

    return static_cast<SessionManager*>(context)->AddConnection(serviceProvider, connection, connectionSize, name,
                                                                flags, context);
}

int ModemCallback(const _GUID& dataType, uint32_t dataSize, const void* data, void*)
{
    if (MCSameGuid(dataType, DPAID_Modem))
    {
        SessionManager::GetGlobalPointer(nullptr)->AddModemName(data, dataSize);
    }

    return SessionManager::GetGlobalPointer(nullptr)->numModems < 10;
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

    return static_cast<SessionManager*>(context)->AddSession(desc, timeout, flags, context);
}

void ShiftPointerArray(int32_t* array, int index, int count)
{
    std::memmove(array + index, array + index + 1, (count - index) * sizeof(int32_t));
    array[count] = 0;
}

int CompareLatencies(const int32_t* playerNumber1, const int32_t* playerNumber2)
{
    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);
    FIDPPlayer* player1 = sessionManager->GetPlayerNumber(*playerNumber1);
    FIDPPlayer* player2 = sessionManager->GetPlayerNumber(*playerNumber2);

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
    SessionManager* sessionManager = SessionManager::GetGlobalPointer(nullptr);
    FLinkedList<FIDPPlayer>* players = sessionManager->GetPlayers(nullptr);
    FLinkedListIterator<FIDPPlayer> iterator(players);
    iterator.current = players->head;
    const int numPlayers = players->count;

    for (int i = 0; i < numPlayers; i++)
    {
        FIDPPlayer* player = iterator.current != nullptr ? iterator.current->data : nullptr;
        char line[512];
        // Original behaviour: the line is formatted and dropped (its output call was compiled out).
        std::snprintf(line, sizeof(line), "Messages to player %s - vlist size = %d\\n", player->name,
                      player->verifyList.count);
        Assert(iterator.current != nullptr, 0, nullptr);
        iterator.current = iterator.current->next;
    }
}

// ---- FIDPNetworkProtocol -----------------------------------------------------------------------------------------

FIDPNetworkProtocol::FIDPNetworkProtocol()
{
    shortName[0] = '\0';
    longName[0] = '\0';
    connectionBuffer = nullptr;
    protocolType = -1;
}

FIDPNetworkProtocol::~FIDPNetworkProtocol()
{
    destroy();
}

void FIDPNetworkProtocol::destroy()
{
    if (connectionBuffer != nullptr)
    {
        linkUpBlocks->Free(connectionBuffer);
        // Port fix: cleared, so the destructor after ClearList's destroy() doesn't free it twice.
        connectionBuffer = nullptr;
    }
}

int FIDPNetworkProtocol::SetConnectionBuffer(void* connection, int size)
{
    if (connectionBuffer != nullptr)
    {
        linkUpBlocks->Free(connectionBuffer);
    }

    connectionBuffer = linkUpBlocks->Allocate(size);

    if (connectionBuffer == nullptr)
    {
        return -1;
    }

    std::memcpy(connectionBuffer, connection, size);
    return 0;
}

void FIDPNetworkProtocol::SetShortName(char* name)
{
    if (name == nullptr)
    {
        shortName[0] = '\0';
    }
    else
    {
        // Port fix: always terminated.
        std::memset(shortName, 0, sizeof(shortName));
        std::strncpy(shortName, name, 0x3f);
    }
}

void FIDPNetworkProtocol::SetLongName(char* name)
{
    if (name == nullptr)
    {
        longName[0] = '\0';
    }
    else
    {
        std::memset(longName, 0, sizeof(longName));
        std::strncpy(longName, name, 0xff);
    }
}

void FIDPNetworkProtocol::ClearList(FLinkedList<FIDPNetworkProtocol>& list)
{
    const int numProtocols = list.count;
    list.current = list.head;

    for (int i = 0; i < numProtocols; i++)
    {
        FIDPNetworkProtocol* protocol = list.current->data;
        list.Del(protocol);
        protocol->destroy();
        delete protocol;
    }
}

// ---- SessionManager: lifetime ------------------------------------------------------------------------------------

SessionManager::SessionManager(_GUID appGUID)
{
    Assert(instanceExists == 0, 0, nullptr);
    TicksPerMS = static_cast<uint32_t>(static_cast<uint32_t>(MCPort::PerformanceFrequency()) / 1000);
    globalPointerHolder = nullptr;
    instanceExists = 1;
    instance = this;
    ipxChecked = 0;
    tcpChecked = 0;
    modemChecked = 0;
    directPlay = nullptr;
    gameStarted = 0;
    StartTime = MCPort::Milliseconds();
    const int32_t result = CreateDirectPlayInterface();
    Assert(result == 0, 0, "This application requires DirectX 5 or later.");
    thisAppGUID = appGUID;
    myPlayerID = 0;
    serverID = 0;
    playerEvent = nullptr;
    killReceiveEvent = nullptr;
    receiveThread = nullptr;
    currentSession = nullptr;
    currentConnection = -1;
    isHost = 0;
    hasPlayerNumber = 0;
    nextFileID = 0;

    auto* newServer = new FIValueMessage{};
    newServer->header = 0;
    newServer->header |= FIMSG_GUARANTEED;
    newServer->tagger.Clear();
    newServer->value = 0;
    newServer->header = static_cast<uint16_t>((newServer->header & ~FIMSG_TYPE_MASK) | 6);
    serverMessage = newServer;

    for (int i = 0; i < 6; i++)
    {
        deletedPlayerIDs[i] = 0xffffffffu;
        playersByLatency[i] = i;
    }

    applicationCallback = nullptr;
    applicationCallbackData = nullptr;
    systemCallback = nullptr;
    systemCallbackData = nullptr;
    fileSentCallback = nullptr;
    fileSentCallbackData = nullptr;
    fileReceivedCallback = nullptr;
    fileReceivedCallbackData = nullptr;
    playerIterator = new FLinkedListIterator<FIDPPlayer>(&players);
    verifyMessageMemory = static_cast<uint8_t*>(linkUpBlocks->Allocate(0xd800));

    for (int i = 0; i < 6; i++)
    {
        verifyMessages[i] = verifyMessageMemory + i * 0x2400;
    }

    emptyMessages = new FIDPMsgList();
    systemMessages = new FIDPMsgList();
    applicationMessages = new FIDPMsgList();
    preIDReceivedMessages = new FIDPMsgList();
    preIDGroupMessages = new FIDPMsgList();
    preIDServerMessages = new FIDPMsgList();
    nextPingTime = PerformanceTicks();
    pingInterval = 2000;
    dialupState = 0;
}

SessionManager::~SessionManager()
{
    destroy();

    while (pendingPlayers.head != nullptr)
    {
        pendingPlayers.Del(pendingPlayers.head->data);
    }

    while (incomingFiles.head != nullptr)
    {
        incomingFiles.Del(incomingFiles.head->data);
    }

    while (outgoingFiles.head != nullptr)
    {
        outgoingFiles.Del(outgoingFiles.head->data);
    }

    while (groups.head != nullptr)
    {
        groups.Del(groups.head->data);
    }

    while (players.head != nullptr)
    {
        players.Del(players.head->data);
    }

    while (sessions.head != nullptr)
    {
        sessions.Del(sessions.head->data);
    }

    while (connections.head != nullptr)
    {
        connections.Del(connections.head->data);
    }
}

void SessionManager::destroy()
{
    std::lock_guard lock(criticalSection);
    delete emptyMessages;
    emptyMessages = nullptr;
    delete systemMessages;
    systemMessages = nullptr;
    delete applicationMessages;
    applicationMessages = nullptr;
    delete preIDReceivedMessages;
    preIDReceivedMessages = nullptr;
    delete preIDGroupMessages;
    preIDGroupMessages = nullptr;
    delete preIDServerMessages;
    preIDServerMessages = nullptr;
    instance = nullptr;
    globalPointerHolder = nullptr;
    instanceExists = 0;
    DestroyDirectPlayInterface();
    delete static_cast<FIValueMessage*>(serverMessage);
    serverMessage = nullptr;
    delete playerIterator;
    playerIterator = nullptr;
}

int32_t SessionManager::CreateDirectPlayInterface()
{
    // Port: CoCreateInstance(CLSID_DirectPlay, IID_IDirectPlay3A) becomes the port's stand-in, which can't fail.
    if (directPlay == nullptr)
    {
        directPlay = new MCDirectPlay();
    }

    EnumerateConnections();
    return 0;
}

int32_t SessionManager::DestroyDirectPlayInterface()
{
    LeaveSession();

    if (directPlay != nullptr)
    {
        delete directPlay;
        directPlay = nullptr;
    }

    currentConnection = -1;
    return 0;
}

// ---- message queues ----------------------------------------------------------------------------------------------

void SessionManager::AddMessageToEmptyQueue(FIDPMessage* msg)
{
    std::lock_guard lock(AddingMessageList);
    msg->Clear();
    emptyMessages->Add(msg);
}

FIDPMessage* SessionManager::GetMessageFromEmptyQueue()
{
    std::lock_guard lock(AddingMessageList);
    FIDPMessage* msg = emptyMessages->Head();
    emptyMessages->TossHead();
    return msg;
}

// ---- connections, sessions, players, groups ----------------------------------------------------------------------

int32_t SessionManager::EnumerateConnections()
{
    Assert(directPlay != nullptr, 0, nullptr);
    FIDPNetworkProtocol::ClearList(connections);
    return static_cast<int32_t>(directPlay->EnumConnections(&thisAppGUID, EnumConnectionsCallback, this, 0));
}

void SessionManager::SetConnectionType(FIDPNetworkProtocol* protocol, const _GUID* guid)
{
    if (MCSameGuid(*guid, DPSPGUID_TCPIP))
    {
        protocol->protocolType = PROTOCOL_TCPIP;
        availableProtocols |= PROTOCOL_TCPIP;
    }
    else if (MCSameGuid(*guid, DPSPGUID_IPX))
    {
        protocol->protocolType = PROTOCOL_IPX;
        availableProtocols |= PROTOCOL_IPX;
    }
    else if (MCSameGuid(*guid, DPSPGUID_MODEM))
    {
        protocol->protocolType = PROTOCOL_MODEM;
        availableProtocols |= PROTOCOL_MODEM;
    }
    else if (MCSameGuid(*guid, DPSPGUID_SERIAL))
    {
        protocol->protocolType = PROTOCOL_SERIAL;
        availableProtocols |= PROTOCOL_SERIAL;
    }
    else if (MCSameGuid(*guid, DPSPGUID_LOBBY))
    {
        protocol->protocolType = PROTOCOL_LOBBY;
        availableProtocols |= PROTOCOL_LOBBY;
    }
}

int SessionManager::AddConnection(const _GUID* serviceProvider, void* connection, uint32_t connectionSize,
                                  const DPNAME* name, uint32_t, void* context)
{
    if (context != this)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Wrong object receiving callback", nullptr);
        return 0;
    }

    FIDPNetworkProtocol* protocol = new FIDPNetworkProtocol();
    protocol->SetShortName(name->lpszShortNameA);
    protocol->SetLongName(name->lpszLongNameA);
    protocol->SetConnectionBuffer(connection, static_cast<int>(connectionSize));
    SetConnectionType(protocol, serviceProvider);
    connections.Add(protocol);
    return 1;
}

int SessionManager::AddSession(const DPSESSIONDESC2* desc, uint32_t*, uint32_t, void* context)
{
    if (context != this)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Wrong object receiving EnumSessions callback",
                                 nullptr);
        return 0;
    }

    Assert(desc != nullptr, 0, nullptr);

    if (currentSession != nullptr && MCSameGuid(currentSession->sessionDesc.guidInstance, desc->guidInstance))
    {
        return 1;
    }

    sessions.Add(new FIDPSession(*desc));
    return 1;
}

int SessionManager::NewPlayerEnumeration(uint32_t playerID, uint32_t, const DPNAME* name, uint32_t flags)
{
    FIDPPlayer* player = new FIDPPlayer(playerID, name, flags);
    Assert(player != nullptr, 0, "Player is null");

    {
        // Port fix: a scoped lock; the original left the critical section entered when the player was already
        // listed (and leaked the new one, as the port still does).
        std::lock_guard lock(criticalSection);

        if (GetPlayer(playerID) != nullptr)
        {
            return 1;
        }

        players.Add(player);
    }

    if (player->id == myPlayerID)
    {
        myPlayer = player;

        if (isHost != 0)
        {
            myPlayer->playerNumber = 0;
        }
    }
    else if (launchedFromLobby == 0)
    {
        if (isHost == 0 && hasPlayerNumber != 0)
        {
            GivePlayerAnID(player);
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (newPlayerNumbers[i] == playerID)
            {
                player->playerNumber = i;
                player->hasPlayerNumber = 1;
            }
        }
    }

    return 1;
}

int SessionManager::NewGroupEnumeration(uint32_t groupID, const DPNAME* name, uint32_t flags)
{
    groups.Add(new FIDPGroup(groupID, 0, name, flags));
    return 1;
}

FIDPGroup* SessionManager::GetGroup(uint32_t groupID)
{
    groups.current = groups.head;

    for (int i = 0; i < groups.count; i++)
    {
        FIDPGroup* group = groups.ReadAndNext();

        if (group->id == groupID)
        {
            return group;
        }
    }

    return nullptr;
}

FIDPPlayer* SessionManager::GetPlayer(uint32_t playerID)
{
    for (int i = 0; i < 6; i++)
    {
        if (deletedPlayerIDs[i] == playerID)
        {
            return nullptr;
        }
    }

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data->id == playerID)
        {
            return link->data;
        }
    }

    return nullptr;
}

int32_t SessionManager::CreatePlayer(char* playerName)
{
    sessionLocked = 0;

    for (int i = 0; i < 6; i++)
    {
        newPlayerNumbers[i] = 0;
    }

    DPNAME name{0x10, 0, playerName, nullptr};
    Assert(playerEvent == nullptr, 0, "Player event already initialized");
    Assert(killReceiveEvent == nullptr, 0, " kill event already initialized");
    playerEvent = &eventStandIn;
    killReceiveEvent = &eventStandIn;
    Assert(playerEvent != nullptr, 0, "Could not create hKillReceiveEvent");
    Assert(killReceiveEvent != nullptr, 0, "Could not create hKillReceiveEvent");
    const uint32_t result = directPlay->CreatePlayer(&myPlayerID, &name, playerEvent, nullptr, 0, 0);

    if (result == DP_OK)
    {
        {
            std::lock_guard lock(AddingMessageList);
            emptyMessages->Head();

            for (int i = 0; i < 900; i++)
            {
                emptyMessages->Add(new FIDPMessage(myPlayerID, 0x200));
            }
        }

        if (isHost != 0)
        {
            serverID = myPlayerID;
            static_cast<FIValueMessage*>(serverMessage)->value = myPlayerID;
            hasPlayerNumber = 1;
        }

        EnumeratePlayers(nullptr);
        GetGroups(nullptr);

        if (dialupState != 0)
        {
            EnableDialupNetworking(dialupState);
            dialupState = 0;
        }
    }
    else
    {
        playerEvent = nullptr;
        killReceiveEvent = nullptr;
        myPlayerID = 0;
    }

    return static_cast<int32_t>(result);
}

void SessionManager::SetHomeDirectory(char* directory)
{
    std::strncpy(HomeDirectory, directory, 0x1ff);
}

int32_t SessionManager::HostSession(FIDPSession& session, char* playerName)
{
    Assert(directPlay != nullptr, 0, "Can't host session.  No DirectPlayObject");
    isHost = 1;

    if (currentConnection == PROTOCOL_MODEM)
    {
        DisableCallerID();
    }

    if (directPlay->Open(&session.sessionDesc, DPOPEN_CREATE | DPOPEN_RETURNSTATUS) != DP_OK)
    {
        return -1;
    }

    FIDPSession* hosted = new FIDPSession(session);
    sessions.Add(hosted);
    currentSession = hosted;

    if (CreatePlayer(playerName) != 0)
    {
        return -2;
    }

    SendSystemInformation();
    return 0;
}

FIDPSession* SessionManager::FindMatchingSession(_GUID* sessionGUID)
{
    sessions.current = sessions.head;

    for (;;)
    {
        FIDPSession* session = sessions.ReadAndNext();

        if (session == nullptr)
        {
            return nullptr;
        }

        if (MCSameGuid(session->sessionDesc.guidInstance, *sessionGUID))
        {
            return session;
        }
    }
}

int32_t SessionManager::JoinSession(_GUID* sessionGUID, char* playerName)
{
    Assert(directPlay != nullptr, 0, nullptr);
    FIDPSession* session = FindMatchingSession(sessionGUID);

    if (session == nullptr)
    {
        return -1;
    }

    currentSession = session;
    int32_t result = static_cast<int32_t>(directPlay->Open(&session->sessionDesc, DPOPEN_JOIN | DPOPEN_RETURNSTATUS));

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

void SessionManager::SendSystemInformation()
{
    FISystemInfoMessage msg;
    msg.tagger.Clear();
    msg.header = 0x100b;
    msg.totalPhysicalMemory = MCPort::TotalPhysicalMemory();

    if (isHost == 0)
    {
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
    }
    else
    {
        myPlayer->totalPhysicalMemory = msg.totalPhysicalMemory;
    }
}

int SessionManager::ReadyToChooseServer()
{
    if (myPlayer == nullptr)
    {
        return 0;
    }

    return readyToChooseServer;
}

void SessionManager::ProcessSystemInfoMessage(FISystemInfoMessage* msg, uint32_t fromID)
{
    FIDPPlayer* player = GetPlayer(fromID);

    if (player != nullptr)
    {
        player->totalPhysicalMemory = msg->totalPhysicalMemory;
    }
}

int SessionManager::LockSession()
{
    if (currentSession == nullptr || isHost == 0)
    {
        return 0;
    }

    currentSession->sessionDesc.dwFlags |= DPSESSION_NEWPLAYERSDISABLED;
    ReportError(directPlay->SetSessionDesc(&currentSession->sessionDesc, 0));
    sessionLocked = 1;
    gameStarted = 1;
    return 1;
}

int SessionManager::LeaveSession()
{
    if (currentConnection == PROTOCOL_TCPIP && dialupState != 0)
    {
        EnableDialupNetworking(dialupState);
        dialupState = 0;
    }

    if (DisabledCallerID != 0)
    {
        ReEnableCallerID();
        DisabledCallerID = 0;
    }

    if (currentSession == nullptr || myPlayer == nullptr)
    {
        return 0;
    }

    receiveThread = nullptr;
    killReceiveEvent = nullptr;
    playerEvent = nullptr;
    myPlayerID = 0;
    isHost = 0;
    serverID = 0;
    hasPlayerNumber = 0;
    currentSession = nullptr;
    myPlayer = nullptr;
    directPlay->Close();
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();

    for (int i = 0; i < 6; i++)
    {
        playersByLatency[i] = i;
    }

    return 1;
}

void SessionManager::CreateGroup(uint32_t* groupID, char* groupName, void* data, uint32_t dataSize, uint32_t flags)
{
    DPNAME name{0x10, 0, groupName, nullptr};
    const uint32_t result = directPlay->CreateGroup(groupID, &name, nullptr, 0, flags);
    ReportError(result);
    FIDPGroup* group = new FIDPGroup(*groupID, 0, &name, flags);

    if (data != nullptr)
    {
        group->SetGroupData(data, dataSize);
    }

    groups.Add(group);
    SetGroupData(*groupID, data, dataSize, 0);
    ReportError(result);
}

int SessionManager::AddPlayerToGroup(uint32_t groupID, uint32_t playerID)
{
    int added = 0;
    groups.current = groups.head;

    if (playerID == 0)
    {
        playerID = myPlayerID;
    }

    for (int i = 0; i < groups.count; i++)
    {
        FIDPGroup* group = groups.ReadAndNext();

        if (group->id != groupID)
        {
            continue;
        }

        if (group->AddPlayer(playerID) != 0)
        {
            FIDPPlayer* player = GetPlayer(playerID);

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
        ReportError(directPlay->AddPlayerToGroup(groupID, playerID));
    }

    return added;
}

int SessionManager::RemovePlayerWithID(uint32_t playerID)
{
    FIDPPlayer* player = GetPlayer(playerID);

    if (player == nullptr)
    {
        return -1;
    }

    return RemovePlayerFromGame(player);
}

int SessionManager::RemovePlayerFromGame(FIDPPlayer* player)
{
    if (removingPlayer != 0)
    {
        return -1;
    }

    removingPlayer = 1;

    if (player->hasPlayerNumber == 0)
    {
        removingPlayer = 0;
        return -1;
    }

    FIValueMessage* msg = static_cast<FIValueMessage*>(linkUpBlocks->Allocate(sizeof(FIValueMessage)));
    msg->tagger.Clear();
    SetHeader(msg, FIMSG_GUARANTEED, 9);
    msg->value = player->id;
    SendMessageToPlayerGuaranteed(player->id, msg, sizeof(FIValueMessage), 1);
    player->hasPlayerNumber = 0;
    linkUpBlocks->Free(msg);
    removingPlayer = 0;
    return 0;
}

int SessionManager::RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID)
{
    int removed = 0;

    if (playerID == 0)
    {
        playerID = myPlayerID;
    }

    for (FLink<FIDPGroup>* link = groups.head; link != nullptr; link = link->next)
    {
        FIDPGroup* group = link->data;

        if (group->id != groupID)
        {
            continue;
        }

        if (group->RemovePlayer(playerID) != 0)
        {
            FIDPPlayer* player = GetPlayer(playerID);

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
        ReportError(directPlay->DeletePlayerFromGroup(groupID, playerID));
    }

    return removed;
}

void SessionManager::SetGroupData(uint32_t groupID, void* data, uint32_t dataSize, uint32_t flags)
{
    if (data != nullptr && dataSize != 0)
    {
        ReportError(directPlay->SetGroupData(groupID, data, dataSize, flags));
    }
}

int32_t SessionManager::SetCurrentConnection(int type)
{
    void* connection = nullptr;
    connections.current = connections.head;

    for (;;)
    {
        FIDPNetworkProtocol* protocol = connections.ReadAndNext();

        if (protocol == nullptr)
        {
            break;
        }

        if (protocol->protocolType == type)
        {
            connection = protocol->connectionBuffer;
            break;
        }
    }

    Assert(connection != nullptr, 0, nullptr);
    // Port fix: recreating the DirectPlay object below re-enumerates the connections, which frees the buffer just
    // found (the original went on using it); the port keeps a copy.
    std::vector<uint8_t> copy(static_cast<uint8_t*>(connection),
                              static_cast<uint8_t*>(connection) + MCDirectPlay::ConnectionDataSize);

    if (currentConnection >= 0)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    const int32_t result = static_cast<int32_t>(directPlay->InitializeConnection(copy.data(), 0));

    if (type != PROTOCOL_MODEM && result == 0)
    {
        GetSessions();
    }

    currentConnection = type;
    return result;
}

void SessionManager::ConnectIPX()
{
    SetCurrentConnection(PROTOCOL_IPX);
}

int32_t SessionManager::EnableDialupNetworking(uint32_t)
{
    // Port: the original restored HKCU "Software\Microsoft\Windows\CurrentVersion\Internet Settings\EnableAutodial".
    // The port never changes it (see DisableDialupNetworking).
    return 0;
}

uint32_t SessionManager::DisableDialupNetworking()
{
    // Port: the original switched Windows' dial-up autodial off in the registry for the length of a TCP/IP game, so
    // looking for sessions wouldn't dial the internet provider. Not the port's business: it reports "was off".
    return 0;
}

void SessionManager::ConnectTCP(char* ipAddress)
{
    if (currentConnection == PROTOCOL_TCPIP)
    {
        DestroyDirectPlayInterface();
        CreateDirectPlayInterface();
    }

    dialupState = DisableDialupNetworking();
    DPCOMPOUNDADDRESSELEMENT address[2];
    address[0].guidDataType = DPAID_ServiceProvider;
    address[0].dwDataSize = sizeof(_GUID);
    address[0].lpData = const_cast<_GUID*>(&DPSPGUID_TCPIP);
    address[1].guidDataType = DPAID_INet;
    address[1].dwDataSize = static_cast<uint32_t>(std::strlen(ipAddress) + 1);
    address[1].lpData = ipAddress;

    if (InitializeConnection(address, 2) == 0)
    {
        currentConnection = PROTOCOL_TCPIP;
    }
}

int32_t SessionManager::ConnectModem(char* phoneNumber, char* modemName)
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
        currentConnection = PROTOCOL_MODEM;
    }

    return result;
}

int32_t SessionManager::ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity,
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
        currentConnection = PROTOCOL_SERIAL;
        GetSessions();
    }

    return result;
}

int32_t SessionManager::InitializeConnection(DPCOMPOUNDADDRESSELEMENT* elements, int numElements)
{
    if (currentConnection >= 0)
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

    void* address = linkUpBlocks->Allocate(size);
    result = MCDirectPlay::CreateCompoundAddress(elements, numElements, address, &size);

    if (result == DP_OK)
    {
        result = directPlay->InitializeConnection(address, 0);
    }

    if (address != nullptr)
    {
        linkUpBlocks->Free(address);
    }

    return static_cast<int32_t>(result);
}

void SessionManager::AddModemName(const void* data, uint32_t dataSize)
{
    uint32_t used = 0;
    const char* name = static_cast<const char*>(data);

    while (used < dataSize)
    {
        char* slot = modemNames[numModems];
        std::strncpy(slot, name, 0x3f);
        used += static_cast<uint32_t>(std::strlen(slot) + 1);
        name += std::strlen(slot) + 1;

        if (std::strlen(slot) < 2)
        {
            break;
        }

        numModems++;
    }
}

int SessionManager::FindModems()
{
    if (currentSession != nullptr)
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
    numModems = 0;
    return -1;
}

char* SessionManager::GetModemName(int32_t index)
{
    if (index < numModems)
    {
        return modemNames[index];
    }

    return nullptr;
}

int32_t SessionManager::CreateLobby(void** lobby)
{
    // Port: the DirectPlay lobby object (DirectPlayLobbyCreateA, IDirectPlayLobby2A) only served to build compound
    // addresses, which the port's stand-in does itself (MCDirectPlay::CreateCompoundAddress).
    *lobby = nullptr;
    return 0;
}

int SessionManager::WasLaunchedFromLobby()
{
    // Port: no DirectPlay lobby launches the game; GetConnectionSettings answered DPERR_NOTLOBBIED.
    ReportError(DPERR_NOTLOBBIED);
    return 0;
}

uint32_t SessionManager::SetupLobbyConnection(void (*)(), void (*)())
{
    // Port: the original dropped its DirectPlay object, asked the lobby for the connection it was launched with
    // (Connect), and joined or hosted that session. Without a lobby GetConnectionSettings answers DPERR_NOTLOBBIED,
    // and the original then made a new DirectPlay object without enumerating its connections; so does the port.
    if (directPlay != nullptr)
    {
        directPlay->Close();
        delete directPlay;
        directPlay = nullptr;
    }

    if (directPlay == nullptr)
    {
        directPlay = new MCDirectPlay();
    }

    return DPERR_NOTLOBBIED;
}

FLinkedList<FIDPNetworkProtocol>* SessionManager::GetConnections()
{
    connections.current = connections.head;
    return &connections;
}

void SessionManager::ClearSessionList()
{
    sessions.current = sessions.head;

    if (currentSession == nullptr)
    {
        FIDPSession::ClearList(sessions);
        return;
    }

    const int numSessions = sessions.count;

    for (int i = 0; i < numSessions; i++)
    {
        FIDPSession* session = sessions.ReadAndNext();

        if (session != currentSession)
        {
            sessions.Del(session);
            delete session;
        }
    }
}

FLinkedList<FIDPSession>* SessionManager::GetSessions()
{
    if (directPlay == nullptr)
    {
        return nullptr;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = thisAppGUID;
    directPlay->EnumSessions(&desc, 0, EnumSessionsCallback, this,
                             DPENUMSESSIONS_AVAILABLE | DPENUMSESSIONS_ASYNC | DPENUMSESSIONS_PASSWORDREQUIRED |
                                 DPENUMSESSIONS_RETURNSTATUS);
    sessions.current = sessions.head;
    return &sessions;
}

FLinkedList<FIDPPlayer>* SessionManager::GetPlayers(FIDPSession* session)
{
    if (myPlayerID == 0)
    {
        EnumeratePlayers(session);
    }
    else
    {
        players.current = players.head;
    }

    players.current = players.head;
    return &players;
}

void SessionManager::EnumeratePlayers(FIDPSession* session)
{
    FIDPPlayer::ClearList(players);
    Assert(directPlay != nullptr, 0, nullptr);
    Assert(currentConnection >= 0, 0, nullptr);

    if (session == nullptr)
    {
        directPlay->EnumPlayers(nullptr, EnumPlayersCallback, this, 0);
    }
    else
    {
        directPlay->EnumPlayers(&session->sessionDesc.guidInstance, EnumPlayersCallback, this, DPENUMPLAYERS_SESSION);
    }

    players.current = players.head;

    if (currentSession != nullptr)
    {
        currentSession->sessionDesc.dwCurrentPlayers = players.count;
    }
}

FLinkedList<FIDPGroup>* SessionManager::GetGroups(FIDPSession* session)
{
    FIDPGroup::ClearList(groups);
    Assert(directPlay != nullptr, 0, nullptr);
    Assert(currentConnection >= 0, 0, nullptr);

    if (session == nullptr)
    {
        directPlay->EnumGroups(nullptr, EnumGroupsCallback, this, 0);
    }
    else
    {
        // Original behaviour: a listed session's groups are asked for with EnumPlayers (its players come back).
        directPlay->EnumPlayers(&session->sessionDesc.guidInstance, EnumGroupsCallback, this, DPENUMPLAYERS_SESSION);
    }

    groups.current = groups.head;
    return &groups;
}

int32_t SessionManager::Dial()
{
    if (currentConnection != PROTOCOL_MODEM)
    {
        return 1;
    }

    ClearSessionList();
    DPSESSIONDESC2 desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(DPSESSIONDESC2);
    desc.guidApplication = thisAppGUID;
    const int32_t result = static_cast<int32_t>(
        directPlay->EnumSessions(&desc, 0, EnumSessionsCallback, this,
                                 DPENUMSESSIONS_AVAILABLE | DPENUMSESSIONS_ASYNC | DPENUMSESSIONS_PASSWORDREQUIRED |
                                     DPENUMSESSIONS_RETURNSTATUS));

    if (result == 0 && DisabledCallerID != 0)
    {
        ReEnableCallerID();
        DisabledCallerID = 0;
    }

    return result;
}

void SessionManager::CancelDialing()
{
    DestroyDirectPlayInterface();
    CreateDirectPlayInterface();
}

// ---- the per-frame pump ------------------------------------------------------------------------------------------

int SessionManager::SendVerifies()
{
    for (int i = 0; i < 6; i++)
    {
        const FIVerifyMessage* verify = reinterpret_cast<FIVerifyMessage*>(verifyMessages[i]);

        if (verify->count != 0)
        {
            FIDPPlayer* player = GetPlayerNumber(i);

            if (player != nullptr)
            {
                SendMessageA(player->id, reinterpret_cast<FIMessageHeader*>(verifyMessages[i]), verify->count * 6 + 3);
            }
        }
    }

    for (int i = 0; i < 6; i++)
    {
        ResetVerify(verifyMessages[i]);
    }

    return 0;
}

void SessionManager::ProcessMessages()
{
    if (myPlayerID == 0)
    {
        return;
    }

    if (hasPlayerNumber != 0 && players.count > 1 && isHost != 0)
    {
        const uint32_t now = PerformanceTicks();

        // Port fix: compared as a signed difference; the low 32 bits of a modern performance counter wrap every few
        // minutes, and a plain "next < now" then stopped (or flooded) the pings until it wrapped again.
        if (static_cast<int32_t>(now - nextPingTime) > 0)
        {
            SendPing();
            nextPingTime = now + pingInterval * TicksPerMS;
        }
    }

    std::lock_guard lock(criticalSection);
    ReceiveThread();
    UpdateGuaranteedMessages();

    if (ProcessSystemMessages() != -1)
    {
        ProcessApplicationMessages();
    }
}

int SessionManager::ProcessSystemMessages()
{
    const int numMessages = systemMessages->Size();
    int i = 0;

    for (; i < numMessages; i++)
    {
        FIDPMessage* msg = systemMessages->Head();
        HandlePreSystemMessage(msg);
        systemMessages->TossHead();

        if (systemCallback != nullptr)
        {
            systemCallback(msg, systemCallbackData);
        }

        if (reinterpret_cast<DPMSG_GENERIC*>(msg->messageBuffer)->dwType == DPSYS_SESSIONLOST)
        {
            return -1;
        }

        HandlePostSystemMessage(msg);
        AddMessageToEmptyQueue(msg);
    }

    return i;
}

void SessionManager::ProcessGuaranteedMessages()
{
    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        FIDPPlayer* player = link->data;

        if (player == myPlayer)
        {
            continue;
        }

        while (FIDPMessage* msg = player->NextMessageToProcess())
        {
            if (msg->messageBuffer == nullptr)
            {
                delete msg;
                continue;
            }

            HandleApplicationMessage(msg);

            if (TypeOf(msg->messageBuffer) == 9)
            {
                return;
            }
        }
    }
}

int SessionManager::ProcessApplicationMessages()
{
    const int numMessages = applicationMessages->Size();
    FIDPMessage* msg = applicationMessages->Head();

    while (msg != nullptr)
    {
        HandleApplicationMessage(msg);

        if (TypeOf(msg->messageBuffer) == 9)
        {
            return -1;
        }

        applicationMessages->TossHead();
        msg = applicationMessages->Head();
    }

    ProcessGuaranteedMessages();
    return numMessages;
}

int SessionManager::ReceiveThread()
{
    if (myPlayer == nullptr)
    {
        return -1;
    }

    inReceiveThread = 1;

    for (int i = 0; i < 6; i++)
    {
        ResetVerify(verifyMessages[i]);
    }

    int32_t result;

    do
    {
        FIDPMessage* msg = GetMessageFromEmptyQueue();

        if (msg == nullptr)
        {
            return 0;
        }

        result = msg->ReceiveMessage(directPlay);

        if (result == 0)
        {
            if (msg->fromID == DPID_SYSMSG)
            {
                systemMessages->Add(msg);
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
        const FIVerifyMessage* verify = reinterpret_cast<FIVerifyMessage*>(verifyMessages[i]);

        if (verify->count != 0)
        {
            SendMessageA(RTGetIDFromPlayerNumber(i), reinterpret_cast<FIMessageHeader*>(verifyMessages[i]),
                         verify->count * 6 + 3);
        }
    }

    inReceiveThread = 0;
    return 0;
}

void SessionManager::RTProcessApplicationMessage(FIDPMessage* msg)
{
    FIDPPlayer* sender = RTGetPlayer(msg->fromID);

    if (sender == nullptr)
    {
        AddMessageToEmptyQueue(msg);
        return;
    }

    uint8_t* buffer = msg->messageBuffer;
    const uint16_t header = HeaderOf(buffer);
    const uint16_t type = header & FIMSG_TYPE_MASK;

    if (hasPlayerNumber != 0)
    {
        if ((header & FIMSG_GUARANTEED) == 0 || type == 9)
        {
            if (type == 1)
            {
                const FIVerifyMessage* verify = reinterpret_cast<FIVerifyMessage*>(buffer);

                for (int i = 0; i < verify->count; i++)
                {
                    FIDPMessage* verified = sender->RemoveFromVerifyList(verify->entries[i][myPlayer->playerNumber]);

                    if (verified != nullptr)
                    {
                        AddMessageToEmptyQueue(verified);
                    }
                }

                AddMessageToEmptyQueue(msg);
            }
            else
            {
                applicationMessages->Add(msg);
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
            applicationMessages->Add(msg);
        }
        else if (launchedFromLobby == 0)
        {
            preIDReceivedMessages->Add(msg);
        }
        else
        {
            AddMessageToEmptyQueue(msg);
        }

        return;
    }

    // The server's player numbers: this machine (and everyone it knows) gets its number.
    const FIPlayerNumbersMessage* numbers = reinterpret_cast<FIPlayerNumbersMessage*>(buffer);
    const int numPlayers = players.count;

    if (launchedFromLobby == 0)
    {
        for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
        {
            FIDPPlayer* player = link->data;
            int number = -1;

            for (int i = 0; i < 6; i++)
            {
                if (numbers->playerIDs[i] == player->id)
                {
                    number = i;
                    break;
                }
            }

            player->playerNumber = number;
            player->hasPlayerNumber = 1;

            if (player->playerNumber > 5)
            {
                return;
            }
        }
    }
    else
    {
        for (int i = 0; i < 6; i++)
        {
            if (numbers->playerIDs[i] == 0)
            {
                continue;
            }

            FIDPPlayer* player = GetPlayer(numbers->playerIDs[i]);

            if (player == nullptr)
            {
                newPlayerNumbers[i] = numbers->playerIDs[i];
            }
            else
            {
                player->playerNumber = i;
                player->hasPlayerNumber = 1;
            }
        }
    }

    serverID = numbers->playerIDs[numbers->serverNumber];

    if (myPlayer->playerNumber == -1)
    {
        AddMessageToEmptyQueue(msg);
        return;
    }

    hasPlayerNumber = 1;

    if (launchedFromLobby == 0)
    {
        int numbered = 0;

        for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
        {
            FIDPPlayer* player = link->data;

            if (player->playerNumber == -1)
            {
                GivePlayerAnID(player);

                if (player->playerNumber < 0 || player->playerNumber > 5)
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

    const uint8_t sendCount = numbers->tagger.sendCount[myPlayer->playerNumber];
    sender->HandleIncomingMessage(msg, sendCount);
    sender->NextMessageToProcess();
    uint8_t* verify = verifyMessages[sender->playerNumber];
    FIVerifyMessage* verifyMessage = reinterpret_cast<FIVerifyMessage*>(verify);
    std::memset(verifyMessage->entries[verifyMessage->count], 0, 6);
    verifyMessage->entries[verifyMessage->count][sender->playerNumber] = sendCount;
    verifyMessage->count++;
    AddMessageToEmptyQueue(msg);

    // The guaranteed messages that came before the numbers can be put in order now.
    const int numEarly = preIDReceivedMessages->Size();

    for (int i = 0; i < numEarly; i++)
    {
        FIDPMessage* early = preIDReceivedMessages->Head();
        FIDPPlayer* earlySender = RTGetPlayer(early->fromID);

        if (earlySender == nullptr)
        {
            AddMessageToEmptyQueue(early);
        }
        else
        {
            RTHandleNewGuaranteedMessage(early, earlySender);
        }

        preIDReceivedMessages->TossHead();
    }

    SendPreIDGuaranteedMessages();
}

void SessionManager::RTHandleNewGuaranteedMessage(FIDPMessage* msg, FIDPPlayer* player)
{
    int stored = 0;
    const uint8_t sendCount = msg->messageBuffer[2 + myPlayer->playerNumber];

    if (player == nullptr || player->playerNumber >= 6 || player->playerNumber < 0)
    {
        return;
    }

    FIVerifyMessage* verify = reinterpret_cast<FIVerifyMessage*>(verifyMessages[player->playerNumber]);

    if (verify->count < 0x28)
    {
        std::memset(verify->entries[verify->count], 0, 6);
        verify->entries[verify->count][player->playerNumber] = sendCount;
        verify->count++;
        stored = player->HandleIncomingMessage(msg, sendCount);
    }

    if (stored == 0)
    {
        AddMessageToEmptyQueue(msg);
    }
}

uint32_t SessionManager::RTGetIDFromPlayerNumber(int playerNumber)
{
    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data->playerNumber == playerNumber)
        {
            return link->data->id;
        }
    }

    return 0;
}

FIDPPlayer* SessionManager::RTGetPlayer(uint32_t playerID)
{
    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data->id == playerID)
        {
            return link->data;
        }
    }

    return nullptr;
}

void SessionManager::UpdatePlayerGuaranteedMessages(FIDPPlayer* player, uint32_t now)
{
    player->verifyList.current = player->verifyList.head;
    const int numMessages = player->verifyList.count;

    for (int i = 0; i < numMessages; i++)
    {
        FIDPMessage* msg = player->verifyList.ReadAndNext();

        if ((player->hasPlayerNumber != 0 || TypeOf(msg->messageBuffer) == 9) &&
            player->resendDelay * msg->timesSent < (now - msg->sendTime) / TicksPerMS)
        {
            SendMessageA(player->id, reinterpret_cast<FIMessageHeader*>(msg->messageBuffer), msg->messageSize);
            msg->wasResent = 1;
            msg->timesSent++;
            msg->sendTime = now;
        }
    }
}

void SessionManager::UpdateGuaranteedMessages()
{
    const uint32_t now = PerformanceTicks();

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data != myPlayer)
        {
            UpdatePlayerGuaranteedMessages(link->data, now);
        }
    }
}

// Nothing in MCX.EXE calls this (OB-107).
void SessionManager::UpdateFileTransfers()
{
    const int numTransfers = outgoingFiles.count;
    outgoingFiles.current = outgoingFiles.head;

    for (int i = 0; i < numTransfers; i++)
    {
        FileTransferInfo* transfer = outgoingFiles.current->data;
        const int finished = transfer->PrepareNextMessage();
        SendMessageFromInfo(transfer->message);

        if (finished != 0)
        {
            outgoingFiles.Del(transfer);

            if (transfer->callback != nullptr)
            {
                transfer->callback(transfer->fileName, nullptr);
            }

            delete transfer;
        }
    }
}

// ---- sending -----------------------------------------------------------------------------------------------------

void SessionManager::SetupMessageSendCounts(FIGuaranteedMessageHeader* header, FLinkedList<FIDPPlayer>* list)
{
    for (FLink<FIDPPlayer>* link = list->head; link != nullptr; link = link->next)
    {
        FIDPPlayer* player = link->data;

        if (player != myPlayer && player->hasPlayerNumber != 0 && player->playerNumber != -1 &&
            player->playerNumber < 6)
        {
            player->outgoingSendCount++;
            header->tagger.sendCount[player->playerNumber] = player->outgoingSendCount;
        }
    }

    header->header |= FIMSG_GROUP_MESSAGE;
    header->header |= FIMSG_GUARANTEED;
}

void SessionManager::StartGame()
{
    FIGuaranteedMessageHeader msg;
    msg.tagger.Clear();
    msg.header = 0x1005;
    SendMessageToGroup(0, &msg, sizeof(msg));
    gameStarted = 1;
}

int32_t SessionManager::SendPing()
{
    // Port fix: cleared (the original sent the uninitialised rest of a stack buffer, its header included).
    uint8_t buffer[256] = {};
    FIPingMessage* ping = reinterpret_cast<FIPingMessage*>(buffer);
    ping->header |= 10;

    if (isHost == 0)
    {
        SendMessageToGroup(0, ping, 9);
        return 0;
    }

    int32_t numbers[6];
    size_t count = 0;

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        // Port fix: bounded to the six numbers the message holds.
        if (link->data != myPlayer && count < 6)
        {
            numbers[count] = link->data->playerNumber;
            count++;
        }
    }

    Assert(count == static_cast<size_t>(players.count - 1), 0, "nPlayers is incorrect");
    std::qsort(numbers, count, sizeof(int32_t), [](const void* a, const void* b)
               { return CompareLatencies(static_cast<const int32_t*>(a), static_cast<const int32_t*>(b)); });
    ping->count = static_cast<uint8_t>(count);

    for (size_t i = 0; i < count; i++)
    {
        ping->playerNumbers[i] = static_cast<uint8_t>(numbers[i]);
    }

    SendMessageToGroup(0, ping, ping->count + 9);
    return 0;
}

void SessionManager::SendPlayersInGroupMessages(uint32_t groupID)
{
    FIPlayersInGroupMessage* msg = static_cast<FIPlayersInGroupMessage*>(linkUpBlocks->Allocate(300));
    groups.current = groups.head;

    for (int i = 0; i < groups.count; i++)
    {
        std::memset(msg, 0, 300);
        msg->header = static_cast<uint16_t>((msg->header & ~FIMSG_TYPE_MASK) | 3);
        FIDPGroup* group = groups.ReadAndNext();
        msg->groupID = group->id;
        group->players.current = group->players.head;

        for (int j = 0; j < group->players.count; j++)
        {
            msg->playerIDs[j] = *group->players.ReadAndNext();
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

    linkUpBlocks->Free(msg);
}

void SessionManager::SendPreIDGuaranteedMessages()
{
    preIDGroupMessages->Head();
    const int numGroupMessages = preIDGroupMessages->Size();

    for (int i = 0; i < numGroupMessages; i++)
    {
        FIDPMessage* msg = preIDGroupMessages->Head();
        SendMessageFromInfo(msg);
        AddMessageToEmptyQueue(msg);
        preIDGroupMessages->TossHead();
    }

    const int numServerMessages = preIDServerMessages->Size();
    preIDServerMessages->Head();

    for (int i = 0; i < numServerMessages; i++)
    {
        FIDPMessage* msg = preIDServerMessages->Head();
        FIGuaranteedMessageHeader* header = reinterpret_cast<FIGuaranteedMessageHeader*>(msg->messageBuffer);

        // Original behaviour: these messages are not returned to the free queue.
        if ((header->header & FIMSG_GUARANTEED) == 0)
        {
            SendMessageToServer(header, msg->messageSize);
        }
        else
        {
            SendMessageToServerGuaranteed(header, msg->messageSize);
        }

        preIDServerMessages->TossHead();
    }
}

void SessionManager::SendMessageToGroup(uint32_t groupID, FIGuaranteedMessageHeader* header, uint32_t size)
{
    if (hasPlayerNumber == 0)
    {
        if (launchedFromLobby == 0)
        {
            std::lock_guard lock(criticalSection);
            FIDPMessage* msg = GetMessageFromEmptyQueue();
            header->header |= FIMSG_GROUP_MESSAGE;
            msg->SetMessageBuffer(header, size);
            msg->toID = groupID;
            preIDGroupMessages->Add(msg);
        }

        return;
    }

    FLinkedList<FIDPPlayer>* list;

    if (groupID == 0)
    {
        list = &players;
    }
    else
    {
        list = new FLinkedList<FIDPPlayer>();
        GetPlayerListForGroup(groupID, list);
    }

    if (sessionLocked == 0 || launchedFromLobby == 0)
    {
        for (FLink<FIDPPlayer>* link = list->head; link != nullptr; link = link->next)
        {
            // Port fix: a member the session no longer knows is listed as null; skipped.
            if (link->data != myPlayer && link->data != nullptr)
            {
                SendMessageToPlayerGuaranteed(link->data->id, header, size, 1);
            }
        }
    }
    else
    {
        for (FLink<FIDPPlayer>* link = list->head; link != nullptr; link = link->next)
        {
            if (link->data != myPlayer && link->data != nullptr && link->data->IsVerifyListFull() != 0)
            {
                RemovePlayerFromGame(link->data);
            }
        }

        std::lock_guard lock(criticalSection);
        SetupMessageSendCounts(header, list);

        if (SendMessageA(groupID, header, size) == 0)
        {
            for (FLink<FIDPPlayer>* link = list->head; link != nullptr; link = link->next)
            {
                FIDPPlayer* player = link->data;

                if (player != nullptr && player->hasPlayerNumber != 0 && player != myPlayer)
                {
                    const uint32_t playerID = player->id;
                    FIDPMessage* msg = GetMessageFromEmptyQueue();
                    header->header &= ~FIMSG_GROUP_MESSAGE;
                    msg->SetMessageBuffer(header, size);
                    msg->toID = playerID;
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

void SessionManager::GetPlayerListForGroup(uint32_t groupID, FLinkedList<FIDPPlayer>* list)
{
    FIDPGroup* group = GetGroup(groupID);
    Assert(group != nullptr, 0, "Group does not exist");
    group->players.current = group->players.head;
    const int numPlayers = group->players.count;

    for (int i = 0; i < numPlayers; i++)
    {
        const uint32_t* playerID = group->players.ReadAndNext();
        list->Add(GetPlayer(*playerID));
    }
}

uint32_t SessionManager::TallyLatencies()
{
    uint32_t total = 0;

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data != myPlayer)
        {
            total += link->data->AverageLatency();
        }
    }

    if (players.count == 1)
    {
        return 0;
    }

    return total / static_cast<uint32_t>(players.count - 1);
}

void SessionManager::ProcessLatencyMessage(FIMessageHeader* msg, uint32_t fromID)
{
    bool allReported = true;
    FIDPPlayer* player = GetPlayer(fromID);
    Assert(player != nullptr, 0, "ProcessLatencyMessage - null player");
    player->reportedLatency = reinterpret_cast<FIValueMessage*>(msg)->value;

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data->reportedLatency == 0)
        {
            allReported = false;
            break;
        }
    }

    readyToChooseServer = allReported ? 1 : 0;
}

void SessionManager::SendLatencyInfo()
{
    if (isHost == 0)
    {
        FIValueMessage msg;
        msg.value = TallyLatencies();
        msg.tagger.Clear();
        msg.header = 0x100c;
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
    }
    else
    {
        myPlayer->reportedLatency = TallyLatencies();

        if (myPlayer->reportedLatency == 0)
        {
            myPlayer->reportedLatency = 1000;
        }
    }
}

void SessionManager::SwitchServers()
{
    if (isHost == 0 || launchedFromLobby != 0)
    {
        return;
    }

    uint32_t mostMemory = 100000;
    FIDPPlayer* best = players.head != nullptr ? players.head->data : nullptr;

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (mostMemory < link->data->totalPhysicalMemory)
        {
            best = link->data;
            mostMemory = link->data->totalPhysicalMemory;
        }
    }

    if (best != myPlayer)
    {
        serverID = best->id;
        isHost = 0;
        static_cast<FIValueMessage*>(serverMessage)->value = serverID;
        SendMessageToGroup(0, serverMessage, sizeof(FIValueMessage));
    }
}

void SessionManager::SendMessageToPlayerGuaranteed(uint32_t playerID, FIGuaranteedMessageHeader* header, uint32_t size,
                                                   int firstSend)
{
    if (hasPlayerNumber == 0)
    {
        if (launchedFromLobby == 0)
        {
            std::lock_guard lock(criticalSection);
            FIDPMessage* msg = GetMessageFromEmptyQueue();
            header->header &= ~FIMSG_GROUP_MESSAGE;
            msg->SetMessageBuffer(header, size);
            msg->toID = playerID;
            preIDGroupMessages->Add(msg);
        }

        return;
    }

    FIDPPlayer* player = GetPlayer(playerID);

    if (player == myPlayer || player == nullptr)
    {
        return;
    }

    if ((header->header & FIMSG_TYPE_MASK) != 9 &&
        (player->hasPlayerNumber == 0 || (player->IsVerifyListFull() != 0 && RemovePlayerFromGame(player) == 0)))
    {
        return;
    }

    std::lock_guard lock(criticalSection);

    if (firstSend != 0 && player->playerNumber >= 0 && player->playerNumber < 6)
    {
        player->outgoingSendCount++;
        header->tagger.sendCount[player->playerNumber] = player->outgoingSendCount;
    }

    header->header |= FIMSG_GUARANTEED;
    header->header &= ~FIMSG_GROUP_MESSAGE;
    const uint32_t sendTime = MCPort::Milliseconds();
    const int32_t result = SendMessageA(playerID, header, size);
    FIDPMessage* msg = GetMessageFromEmptyQueue();
    header->header &= ~FIMSG_GROUP_MESSAGE;
    msg->SetMessageBuffer(header, size);
    msg->toID = playerID;
    player->AddToVerifyList(msg);

    if (result != 0)
    {
        // Original behaviour: a failed send is backdated so it is resent at once; the time is timeGetTime's (ms)
        // where the verify list keeps performance-counter ticks.
        msg->sendTime = sendTime - player->resendDelay * TicksPerMS;

        if (msg->wasResent == 0)
        {
            msg->firstSendTime = msg->sendTime;
        }
    }
}

void SessionManager::SendMessageToServerGuaranteed(FIGuaranteedMessageHeader* header, uint32_t size)
{
    if (hasPlayerNumber == 0)
    {
        std::lock_guard lock(criticalSection);
        FIDPMessage* msg = GetMessageFromEmptyQueue();
        header->header &= ~FIMSG_GROUP_MESSAGE;
        msg->SetMessageBuffer(header, size);
        msg->toID = serverID;
        preIDServerMessages->Add(msg);
        return;
    }

    SendMessageToPlayerGuaranteed(serverID, header, size, 1);
}

void SessionManager::BroadcastMessage(FIMessageHeader* header, uint32_t size)
{
    SendMessageA(0, header, size);
}

void SessionManager::SendMessageToServer(FIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(criticalSection);

    if (hasPlayerNumber == 0)
    {
        FIDPMessage* msg = GetMessageFromEmptyQueue();
        header->header &= ~FIMSG_GROUP_MESSAGE;
        msg->SetMessageBuffer(header, size);
        msg->toID = serverID;
        preIDServerMessages->Add(msg);
    }
    else if (serverID != myPlayerID)
    {
        SendMessageA(serverID, header, size);
    }
}

int32_t SessionManager::SendMessageA(uint32_t toID, FIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(criticalSection);

    if (size > 0x200)
    {
        size = 0x200;
    }

    const uint32_t result = directPlay->Send(myPlayerID, toID, 0, header, size);
    ReportError(result);
    return static_cast<int32_t>(result);
}

int SessionManager::BroadcastFile(char* fileName, char* directory, void (*callback)(char* fileName, void* data))
{
    FileTransferInfo* transfer =
        new FileTransferInfo(myPlayerID, 0, fileName, directory, 0, FileTransferInfo::TRANSFER_SEND);
    transfer->callback = callback;
    transfer->fileID = nextFileID;
    nextFileID++;

    if (nextFileID > 0xff)
    {
        nextFileID = 0;
    }

    outgoingFiles.Add(transfer);
    int size;
    FIBeginFileTransferMessage* begin = transfer->CreateBeginTransferMessage(size);
    BroadcastMessage(begin, static_cast<uint32_t>(size));
    linkUpBlocks->Free(begin);
    return nextFileID - 1;
}

void SessionManager::SendMessageFromInfo(FIDPMessage* msg)
{
    FIGuaranteedMessageHeader* header = reinterpret_cast<FIGuaranteedMessageHeader*>(msg->messageBuffer);

    if (msg->toID == 0)
    {
        SendMessageToGroup(0, header, msg->messageSize);
    }
    else if ((header->header & FIMSG_GROUP_MESSAGE) != 0)
    {
        SendMessageToGroup(msg->toID, header, msg->messageSize);
    }
    else if ((header->header & FIMSG_GUARANTEED) == 0)
    {
        SendMessageA(msg->toID, header, msg->messageSize);
    }
    else
    {
        SendMessageToPlayerGuaranteed(msg->toID, header, msg->messageSize, 1);
    }
}

int32_t SessionManager::GetAverageBandwidth(int*)
{
    if (currentSession == nullptr)
    {
        return -1;
    }

    return -3;
}

SessionManager* SessionManager::GetGlobalPointer(void* owner)
{
    if (instanceExists == 0)
    {
        return nullptr;
    }

    globalPointerHolder = owner;
    return instance;
}

int SessionManager::ReleaseGlobalPointer(void* owner)
{
    const bool held = owner == globalPointerHolder;

    if (held)
    {
        globalPointerHolder = nullptr;
    }

    return held;
}

// ---- players coming and going ------------------------------------------------------------------------------------

void SessionManager::GivePlayerAnID(FIDPPlayer* player)
{
    // numbers[0] is a sentinel; numbers[1..count] are the listed players' numbers, sorted.
    int32_t numbers[7];

    for (int i = 0; i < 6; i++)
    {
        numbers[i + 1] = 30000;
    }

    numbers[0] = 0;
    // Port fix: at most six numbers fit (the original would write past the array with more players listed).
    const int count = std::min(players.count, 6);
    FLink<FIDPPlayer>* link = players.head;

    for (int i = 0; i < count; i++)
    {
        numbers[i + 1] = link != nullptr ? link->data->playerNumber : 0;
        Assert(link != nullptr, 0, nullptr);
        link = link->next;
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

    player->playerNumber = number;
    player->hasPlayerNumber = 1;
}

void SessionManager::AddPlayerOrGroup(uint32_t playerType, uint32_t id, uint32_t parentID, DPNAME* name, uint32_t flags)
{
    FLink<FIDPPlayer>* firstLink = players.head;

    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        groups.current = groups.head;

        for (int i = 0; i < groups.count; i++)
        {
            if (groups.ReadAndNext()->id == id)
            {
                return;
            }
        }

        groups.Add(new FIDPGroup(id, parentID, name, flags));
        return;
    }

    if (GetPlayer(id) != nullptr)
    {
        return;
    }

    FIDPPlayer* player = new FIDPPlayer(id, name, flags);
    Assert(player != nullptr, 0, "Player is null");

    if (hasPlayerNumber == 0)
    {
        pendingPlayers.Add(player);
    }
    else if (isHost != 0 || launchedFromLobby == 0)
    {
        GivePlayerAnID(player);

        if (player->playerNumber < 0 || player->playerNumber > 5)
        {
            Fatal(player->playerNumber, "Could not connect to game.");
        }
    }

    players.Add(player);

    if (isHost == 0 && launchedFromLobby != 0)
    {
        player->hasPlayerNumber = 0;
    }

    if (launchedFromLobby != 0)
    {
        for (int i = 0; i < 6; i++)
        {
            if (newPlayerNumbers[i] == id)
            {
                player->playerNumber = i;
                player->hasPlayerNumber = 1;
            }
        }
    }

    currentSession->sessionDesc.dwCurrentPlayers = players.count;

    if (isHost == 0)
    {
        return;
    }

    // The server tells the new player (or, in a lobby game, everyone) who has which number.
    FIPlayerNumbersMessage numbersMessage = {};
    FIPlayerNumbersMessage* numbers = &numbersMessage;
    numbers->header = 0;
    numbers->header |= FIMSG_GUARANTEED;
    numbers->tagger.Clear();
    numbers->header = static_cast<uint16_t>((numbers->header & ~FIMSG_TYPE_MASK) | 2);
    numbers->serverNumber = 0;

    for (int i = 0; i < 6; i++)
    {
        numbers->playerIDs[i] = 0;
    }

    for (FLink<FIDPPlayer>* link = firstLink; link != nullptr; link = link->next)
    {
        FIDPPlayer* listed = link->data;

        // Port fix: an unnumbered player is skipped (the original wrote it at index -1, into the send counters).
        if (listed->playerNumber >= 0 && listed->playerNumber < 6)
        {
            numbers->playerIDs[listed->playerNumber] = listed->id;
        }

        if (listed->id == myPlayerID)
        {
            numbers->serverNumber = static_cast<uint8_t>(listed->playerNumber);
        }
    }

    if (launchedFromLobby == 0)
    {
        SendMessageToPlayerGuaranteed(player->id, numbers, sizeof(FIPlayerNumbersMessage), 1);
    }
    else
    {
        SendMessageToGroup(0, numbers, sizeof(FIPlayerNumbersMessage));
    }

    SendPlayersInGroupMessages(player->id);
}

FIDPPlayer* SessionManager::GetPlayerNumber(int32_t playerNumber)
{
    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data->playerNumber == playerNumber)
        {
            return link->data;
        }
    }

    return nullptr;
}

void SessionManager::PlayerOrGroupLeaving(uint32_t playerType, uint32_t id)
{
    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        return;
    }

    FIDPPlayer* player = GetPlayer(id);

    if (player == nullptr)
    {
        return;
    }

    player->hasPlayerNumber = 0;

    if (player->id != serverID)
    {
        return;
    }

    // The server left: the next player by the server's latency order takes over.
    FIDPPlayer* next = nullptr;

    // Port fix: bounded to the six numbers (the original ran on past the array when nobody was left).
    for (int i = 0; next == nullptr && i < 6; i++)
    {
        next = GetPlayerNumber(playersByLatency[i]);

        if (next != nullptr && next->hasPlayerNumber == 0)
        {
            next = nullptr;
        }
    }

    Assert(next != nullptr, 0, "No more players");

    if (next == nullptr)
    {
        return;
    }

    if (next == myPlayer)
    {
        isHost = 1;
    }

    serverID = next->id;
}

void SessionManager::DeletePlayerOrGroup(uint32_t playerType, uint32_t id)
{
    if (playerType == DPPLAYERTYPE_PLAYER)
    {
        FIDPPlayer* player = GetPlayer(id);

        if (player == nullptr)
        {
            return;
        }

        players.Del(player);
        delete player;
        currentSession->sessionDesc.dwCurrentPlayers = players.count;

        // Original behaviour: only slots already in use are overwritten, and the constructor leaves them all unused,
        // so no id is ever recorded.
        for (int i = 0; i < 6; i++)
        {
            if (deletedPlayerIDs[i] != 0xffffffffu)
            {
                deletedPlayerIDs[i] = id;
            }
        }

        return;
    }

    FIDPGroup* group = GetGroup(id);

    if (group != nullptr)
    {
        // Original behaviour: the group leaves the list but is not deleted.
        groups.Del(group);
    }
}

int SessionManager::isTCPAvailable()
{
    // Port: the original looked for the TCP/IP stack in the registry (Enum\Network\MSTCP, or the Tcpip service).
    // The port's transport is TCP/IP: available when the sockets library starts.
    if (tcpChecked == 0)
    {
        tcpAvailable = MCSocket::Startup() ? 1 : 0;
        tcpChecked = 1;
    }

    return tcpAvailable;
}

int SessionManager::isIPXAvailable()
{
    // Port: the original looked for the IPX stack in the registry (Enum\Network\NWLINK, or the NwlnkIpx service).
    // The port's "IPX" connection is its LAN search over TCP/IP (see MCDirectPlay), available with TCP/IP.
    if (ipxChecked == 0)
    {
        ipxAvailable = MCSocket::Startup() ? 1 : 0;
        ipxChecked = 1;
    }

    return ipxAvailable;
}

int SessionManager::isModemAvailable()
{
    // Original behaviour: modemChecked is never set, so every call looks for modems again.
    if (modemChecked == 0)
    {
        if (FindModems() == 0)
        {
            modemAvailable = GetModemName(0) != nullptr ? 1 : 0;
        }
        else
        {
            modemAvailable = 0;
        }
    }

    return modemAvailable;
}

void SessionManager::HandlePreSystemMessage(FIDPMessage* msg)
{
    const uint32_t type = reinterpret_cast<DPMSG_GENERIC*>(msg->messageBuffer)->dwType;

    if (type == DPSYS_ADDPLAYERTOGROUP)
    {
        DPMSG_ADDPLAYERTOGROUP* added = reinterpret_cast<DPMSG_ADDPLAYERTOGROUP*>(msg->messageBuffer);
        FIDPGroup* group = GetGroup(added->dpIdGroup);
        Assert(group != nullptr, 0, "group is null");

        if (group != nullptr && group->AddPlayer(added->dpIdPlayer) != 0)
        {
            FIDPPlayer* player = GetPlayer(added->dpIdPlayer);

            if (player != nullptr)
            {
                player->JoinGroup(added->dpIdGroup);
            }
        }
    }
    else if (type == DPSYS_CREATEPLAYERORGROUP)
    {
        DPMSG_CREATEPLAYERORGROUP* created = reinterpret_cast<DPMSG_CREATEPLAYERORGROUP*>(msg->messageBuffer);
        AddPlayerOrGroup(created->dwPlayerType, created->dpId, created->dpIdParent, &created->dpnName,
                         created->dwFlags);
    }
    else if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        DPMSG_DESTROYPLAYERORGROUP* destroyed = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(msg->messageBuffer);
        PlayerOrGroupLeaving(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_SETSESSIONDESC && launchedFromLobby != 0)
    {
        DPMSG_SETSESSIONDESC* changed = reinterpret_cast<DPMSG_SETSESSIONDESC*>(msg->messageBuffer);

        if ((changed->dpDesc.dwFlags & DPSESSION_NEWPLAYERSDISABLED) != 0)
        {
            sessionLocked = 1;
        }
    }
}

void SessionManager::HandlePostSystemMessage(FIDPMessage* msg)
{
    const uint32_t type = reinterpret_cast<DPMSG_GENERIC*>(msg->messageBuffer)->dwType;

    if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        DPMSG_DESTROYPLAYERORGROUP* destroyed = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(msg->messageBuffer);
        DeletePlayerOrGroup(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_DELETEPLAYERFROMGROUP)
    {
        DPMSG_ADDPLAYERTOGROUP* removed = reinterpret_cast<DPMSG_ADDPLAYERTOGROUP*>(msg->messageBuffer);
        FIDPGroup* group = GetGroup(removed->dpIdGroup);

        // Port fix: a group that no longer exists is skipped (the original called through a null group).
        if (group != nullptr && group->RemovePlayer(removed->dpIdPlayer) != 0)
        {
            FIDPPlayer* player = GetPlayer(removed->dpIdPlayer);

            if (player != nullptr)
            {
                player->LeaveGroup(removed->dpIdGroup);
            }
        }
    }
}

void SessionManager::HandleApplicationMessage(FIDPMessage* msg)
{
    bool passOn = true;
    uint8_t* buffer = msg->messageBuffer;

    switch (TypeOf(buffer))
    {
        case 2:
        {
            if (launchedFromLobby != 0)
            {
                const FIPlayerNumbersMessage* numbers = reinterpret_cast<FIPlayerNumbersMessage*>(buffer);

                for (int i = 0; i < 6; i++)
                {
                    if (numbers->playerIDs[i] == 0)
                    {
                        continue;
                    }

                    FIDPPlayer* player = GetPlayer(numbers->playerIDs[i]);

                    if (player == nullptr)
                    {
                        newPlayerNumbers[i] = numbers->playerIDs[i];
                    }
                    else
                    {
                        player->playerNumber = i;
                        player->hasPlayerNumber = 1;
                    }
                }
            }

            break;
        }

        case 3:
        {
            const FIPlayersInGroupMessage* members = reinterpret_cast<FIPlayersInGroupMessage*>(buffer);
            FIDPGroup* group = GetGroup(members->groupID);

            if (group != nullptr)
            {
                for (int i = 0; i < 6 && members->playerIDs[i] != 0; i++)
                {
                    uint32_t playerID = members->playerIDs[i];

                    if (group->AddPlayer(playerID) != 0)
                    {
                        FIDPPlayer* player = GetPlayer(playerID);

                        if (player != nullptr)
                        {
                            player->JoinGroup(members->groupID);
                        }
                    }
                }
            }

            passOn = false;
            break;
        }

        case 5:
        {
            gameStarted = 1;
            break;
        }

        case 6:
        {
            serverID = reinterpret_cast<FIValueMessage*>(buffer)->value;
            isHost = serverID == myPlayerID ? 1 : 0;
            break;
        }

        case 7:
        {
            FIBeginFileTransferMessage* begin = reinterpret_cast<FIBeginFileTransferMessage*>(buffer);
            char* fileName = std::strtok(begin->fileName, "\\");
            char* directory = std::strtok(nullptr, "");
            FileTransferInfo* transfer = new FileTransferInfo(msg->fromID, myPlayerID, fileName, directory,
                                                              begin->fileSize, FileTransferInfo::TRANSFER_RECEIVE);
            transfer->fileID = begin->fileID;
            incomingFiles.Add(transfer);
            passOn = false;
            break;
        }

        case 8:
        {
            const FIFileDataMessage* piece = reinterpret_cast<FIFileDataMessage*>(buffer);
            const int size = static_cast<int>(msg->messageSize);
            incomingFiles.current = incomingFiles.head;

            for (int i = 0; i < incomingFiles.count; i++)
            {
                FileTransferInfo* transfer = incomingFiles.ReadAndNext();

                if (static_cast<uint32_t>(transfer->fileID) != piece->fileID)
                {
                    continue;
                }

                if (transfer->AddBytes(const_cast<uint8_t*>(piece->data), size - 9) != 0)
                {
                    incomingFiles.Del(transfer);

                    if (fileReceivedCallback != nullptr)
                    {
                        fileReceivedCallback(transfer->fileName, fileReceivedCallbackData);
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
            ProcessSystemInfoMessage(reinterpret_cast<FISystemInfoMessage*>(buffer), msg->fromID);
            passOn = false;
            break;
        }

        case 12:
        {
            ProcessLatencyMessage(reinterpret_cast<FIMessageHeader*>(buffer), msg->fromID);
            passOn = true;
            break;
        }

        default:
        {
            break;
        }
    }

    if (passOn && applicationCallback != nullptr)
    {
        // Port: the original left the critical section around the callback (for its receive thread, which never
        // existed); the port has one thread, and its callers don't always hold the lock, so it is left alone.
        applicationCallback(msg, applicationCallbackData);
    }

    AddMessageToEmptyQueue(msg);
}

void SessionManager::HandlePingUpdate(FIDPMessage* msg)
{
    if (msg->fromID != serverID)
    {
        return;
    }

    const FIPingMessage* ping = reinterpret_cast<FIPingMessage*>(msg->messageBuffer);

    // Port fix: bounded to the six numbers the message holds.
    for (int i = 0; i < ping->count && i < 6; i++)
    {
        playersByLatency[i] = ping->playerNumbers[i];
    }
}

int SessionManager::ReportError(uint32_t error)
{
    // The original copied the error's description into a local buffer (a long switch over the DirectPlay and COM
    // codes, else FormatMessage) for a debugger to look at, and dropped it. Only the answer is used.
    return error != 0 ? 1 : 0;
}

void SessionManager::GetProfileData(char* fileName)
{
    uint8_t lastVerified = 0xff;
    players.current = players.head;

    for (int i = 0; i < players.count; i++)
    {
        FIDPPlayer* player = players.ReadAndNext();

        if (player == myPlayer)
        {
            continue;
        }

        char line[64];
        std::snprintf(line, sizeof(line), "[%02d]:  SC:%02d ", player->playerNumber, player->outgoingSendCount);
        std::strcat(fileName, line);

        if (player->verifyList.count > 0)
        {
            player->verifyList.current = player->verifyList.head;
            FIDPMessage* oldest = player->verifyList.head != nullptr ? player->verifyList.head->data : nullptr;
            lastVerified = oldest->messageBuffer[2 + player->playerNumber];
        }

        std::snprintf(line, sizeof(line), "(%02d) ", lastVerified);
        std::strcat(fileName, line);
        std::snprintf(line, sizeof(line), "Rcv %02d |||  ", player->nextIncomingToProcess);
        std::strcat(fileName, line);
    }
}

int32_t SessionManager::GetStats(char* buffer)
{
    if (currentSession == nullptr)
    {
        return -1;
    }

    if (isHost == 0)
    {
        FIDPPlayer* server = GetPlayer(serverID);
        std::sprintf(buffer, "Latency to server (%s) = %d", server->name, server->lastLatency);
        return 0;
    }

    std::sprintf(buffer, "Latencies -- ");

    for (FLink<FIDPPlayer>* link = players.head; link != nullptr; link = link->next)
    {
        if (link->data != myPlayer)
        {
            char entry[512];
            std::snprintf(entry, sizeof(entry), "<%s: %4d> ", link->data->name, link->data->lastLatency);
            std::strcat(buffer, entry);
        }
    }

    return 0;
}
