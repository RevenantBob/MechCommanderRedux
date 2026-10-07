#pragma once

// Original source: mcx\linkup\sessionmanager.cpp (and the inline getters of sessionmanager.h): the game's DirectPlay
// session layer. It enumerates connections (TCP/IP, IPX, modem, serial, lobby), hosts, lists and joins sessions,
// tracks the players and groups, runs a receive thread that sorts incoming messages into queues, and on top of
// DirectPlay implements guaranteed, ordered delivery (per-player send counters, verify lists, resends), latency
// measurement, server switching and file transfer.
//
// Port: DirectPlay is the port's stand-in, MCDirectPlay (platform/MCDirectPlay.h), over TCP/UDP. It offers the TCP/IP
// and IPX connections only, so the modem, serial and lobby paths find nothing (as on a machine without them). The
// "receive thread" was never a thread: ProcessMessages calls ReceiveThread each frame. Win32 handles are opaque
// pointers (their original types in the comments) and DPIDs are uint32_t.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"
#include "platform/MCBlockStore.h"

class FIDPGroup;
class FIDPMessage;
class FIDPPlayer;
class FIDPSession;
class FileTransferInfo;
/// <summary>
/// The linkup blocks that have no single owner yet: message buffers, player and group ids, file names (the linkup
/// heap's in the original, 1,900,000 bytes). Freeing a block that isn't one is ignored, as the heap did.
/// </summary>
extern std::unique_ptr<MCBlockStore> linkUpBlocks;
/// <summary>The game's DirectPlay application id, set by the SessionManager constructor.</summary>
extern _GUID thisAppGUID;
/// <summary>Performance-counter ticks per millisecond.</summary>
extern uint32_t TicksPerMS;
/// <summary><c>timeGetTime</c> when the SessionManager was made (<see cref="TimeStamp"/> counts from it).</summary>
extern uint32_t StartTime;
/// <summary>Guards the free-message queue (a CRITICAL_SECTION in the original).</summary>
extern std::recursive_mutex AddingMessageList;
/// <summary>
/// DPIDs of players the server numbered before this machine knew them: slot n holds the id given player number n.
/// </summary>
extern uint32_t newPlayerNumbers[6];
/// <summary>Nonzero once the host locked the session (no new players).</summary>
extern int sessionLocked;
/// <summary>Nonzero while the receive thread processes a message.</summary>
extern int inReceiveThread;
/// <summary>Nonzero when the other side runs an older MultiPlayer (found while connecting).</summary>
extern int oldVersionOfMPlayer;
/// <summary>Nonzero while the modems' caller-id reporting is switched off by <see cref="DisableCallerID"/>.</summary>
extern int DisabledCallerID;
/// <summary>Per modem (0-5): nonzero when <see cref="DisableCallerID"/> changed its registry setting.</summary>
extern int CallerIDChanged[6];
/// <summary>
/// Nonzero when the game was started by a DirectPlay lobby (userInit sets it from
/// SessionManager::WasLaunchedFromLobby). Lobby games take their player count from the session and skip the
/// pre-numbering message queues.
/// </summary>
/// <remarks>MCX.EXE @ 0x0080a850. It has no symbol; the name is the port's.</remarks>
extern int32_t launchedFromLobby;

/// <summary>Protocol flags: <see cref="FIDPNetworkProtocol::protocolType"/> and SessionManager's available set.</summary>
/// <remarks>The names are the port's; the values are the original's (SessionManager::SetConnectionType).</remarks>
enum FIDPProtocolType
{
    PROTOCOL_TCPIP = 0x01,
    PROTOCOL_IPX = 0x02,
    PROTOCOL_SERIAL = 0x04,
    PROTOCOL_MODEM = 0x08,
    /// <summary>The lobby's own service provider (a GUID of the game's, 0x00784a38).</summary>
    PROTOCOL_LOBBY = 0x10
};

/// <summary>
/// A DirectPlay connection (service provider) the machine offers: its names, the connection data DirectPlay
/// initializes it with, and its <see cref="FIDPProtocolType"/>.
/// </summary>
/// <remarks>Original source: <c>linkup\sessionmanager.cpp</c>, 0x14c bytes (allocated with the global new).</remarks>
class FIDPNetworkProtocol
{
public:
    /// <remarks>MCX.EXE @ 0x0074d930</remarks>
    FIDPNetworkProtocol();
    /// <summary>Frees the connection buffer (<see cref="destroy"/>).</summary>
    /// <remarks>MCX.EXE @ 0x0074d9c0 (vector deleting destructor 0x0074d990)</remarks>
    virtual ~FIDPNetworkProtocol();

    FIDPNetworkProtocol(const FIDPNetworkProtocol&) = delete;
    FIDPNetworkProtocol& operator=(const FIDPNetworkProtocol&) = delete;

    /// <summary>Frees the connection buffer.</summary>
    /// <remarks>MCX.EXE @ 0x0074d9e0</remarks>
    void destroy();

    /// <summary>Keeps a copy of <paramref name="size"/> bytes of DirectPlay connection data (a linkUpBlocks block).</summary>
    /// <returns>0, or -1 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x0074da10</remarks>
    int SetConnectionBuffer(void* connection, int size);

    /// <summary>Sets the short name (up to 63 characters).</summary>
    /// <remarks>MCX.EXE @ 0x0074da90</remarks>
    void SetShortName(char* shortName);

    /// <summary>Sets the long name (up to 255 characters).</summary>
    /// <remarks>MCX.EXE @ 0x0074dad0</remarks>
    void SetLongName(char* longName);

    /// <summary>The short name.</summary>
    /// <remarks>MCX.EXE @ 0x0074a3a0</remarks>
    char* GetShortName() { return shortName; }

    /// <summary>The <see cref="FIDPProtocolType"/>, -1 when not set.</summary>
    /// <remarks>MCX.EXE @ 0x0074a3c0</remarks>
    int GetProtocolType() { return protocolType; }

    /// <summary>Destroys and deletes every protocol of <paramref name="list"/> and empties it.</summary>
    /// <remarks>MCX.EXE @ 0x0074db20</remarks>
    static void ClearList(FLinkedList<FIDPNetworkProtocol>& list);

    char shortName[64]{}; // +0x4
    char longName[256]{}; // +0x44
    /// <summary>DirectPlay's connection data (a linkUpBlocks block).</summary>
    void* connectionBuffer = nullptr; // +0x144
    /// <summary>The <see cref="FIDPProtocolType"/>, -1 when not set.</summary>
    int32_t protocolType = 0; // +0x148
};

/// <summary>
/// The game's DirectPlay session: there is one at a time, found through <see cref="GetGlobalPointer"/>.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\sessionmanager.cpp</c>, 0xba0 bytes. The file-static
/// 0x0080a680 (an instance exists), 0x0080a684 (the instance) and 0x0080a688 (who holds the global pointer)
/// back <see cref="GetGlobalPointer"/>.
/// </remarks>
class SessionManager
{
public:
    /// <summary>
    /// Makes the session manager for application <paramref name="appGUID"/>: creates the DirectPlay object and
    /// enumerates the connections, allocates the message queues and per-player verify buffers, and registers
    /// itself as the global instance.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074ddb0</remarks>
    explicit SessionManager(_GUID appGUID);
    /// <summary>Calls <see cref="destroy"/> and empties every list.</summary>
    /// <remarks>MCX.EXE @ 0x0074e3f0 (vector deleting destructor 0x0074e3c0, vtable slot 0)</remarks>
    virtual ~SessionManager();

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    // The DirectPlay enumeration callbacks forward to the private handlers (the original's __stdcall thunks).
    friend int EnumPlayersCallback(uint32_t, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumGroupsCallback(uint32_t, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumConnectionsCallback(const _GUID*, void*, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumSessionsCallback(const DPSESSIONDESC2*, uint32_t*, uint32_t, void*);

    /// <summary>
    /// Deletes the message queues, releases the DirectPlay object and the global-instance registration.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074e670 (vtable slot 1)</remarks>
    virtual void destroy();

protected:
    /// <summary>Creates the DirectPlay object (IDirectPlay3A) if needed and enumerates the connections.</summary>
    /// <returns>0, or 1 when DirectPlay could not be created.</returns>
    /// <remarks>MCX.EXE @ 0x0074e900</remarks>
    int32_t CreateDirectPlayInterface();

public:
    /// <summary>Leaves the session and releases the DirectPlay object.</summary>
    /// <remarks>MCX.EXE @ 0x0074e9a0</remarks>
    int32_t DestroyDirectPlayInterface();

protected:
    /// <summary>Clears <paramref name="msg"/> and returns it to the free queue.</summary>
    /// <remarks>MCX.EXE @ 0x0074ea00</remarks>
    void AddMessageToEmptyQueue(FIDPMessage* msg);
    /// <summary>Takes a message from the free queue (null when empty).</summary>
    /// <remarks>MCX.EXE @ 0x0074ea40</remarks>
    FIDPMessage* GetMessageFromEmptyQueue();
    /// <summary>Rebuilds <see cref="connections"/> from DirectPlay's EnumConnections.</summary>
    /// <remarks>MCX.EXE @ 0x0074ea80</remarks>
    int32_t EnumerateConnections();

private:
    /// <summary>Sets <paramref name="protocol"/>'s type from its service-provider GUID and adds it to the available set.</summary>
    /// <remarks>MCX.EXE @ 0x0074eb30</remarks>
    void SetConnectionType(FIDPNetworkProtocol* protocol, const _GUID* guid);
    /// <summary>
    /// EnumConnections callback: adds a connection (checks <paramref name="context"/> is this manager).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074ec80 (a __stdcall member in the original, called through a static thunk)</remarks>
    int AddConnection(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                      uint32_t flags, void* context);
    /// <summary>EnumSessions callback: adds a session not already current.</summary>
    /// <remarks>MCX.EXE @ 0x0074edc0 (__stdcall member)</remarks>
    int AddSession(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context);
    /// <summary>EnumPlayers callback: adds player <paramref name="playerID"/> and numbers it when needed.</summary>
    /// <remarks>MCX.EXE @ 0x0074ef30 (__stdcall member)</remarks>
    int NewPlayerEnumeration(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags);
    /// <summary>EnumGroups callback: adds group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074f130 (__stdcall member)</remarks>
    int NewGroupEnumeration(uint32_t groupID, const DPNAME* name, uint32_t flags);

public:
    /// <summary>The group with id <paramref name="groupID"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0074f220</remarks>
    FIDPGroup* GetGroup(uint32_t groupID);
    /// <summary>The player with id <paramref name="playerID"/>, or null (also null for a deleted player's id).</summary>
    /// <remarks>MCX.EXE @ 0x0074f2c0</remarks>
    FIDPPlayer* GetPlayer(uint32_t playerID);

protected:
    /// <summary>
    /// Creates this machine's player named <paramref name="playerName"/> in the current session, fills the free
    /// queue with 900 messages and enumerates the players and groups.
    /// </summary>
    /// <returns>0 or the DirectPlay error.</returns>
    /// <remarks>MCX.EXE @ 0x0074f390</remarks>
    int32_t CreatePlayer(char* playerName);

public:
    /// <summary>Sets <see cref="HomeDirectory"/> (up to 511 characters).</summary>
    /// <remarks>MCX.EXE @ 0x0074f630</remarks>
    void SetHomeDirectory(char* directory);
    /// <summary>Hosts <paramref name="session"/> and creates this machine's player <paramref name="playerName"/>.</summary>
    /// <returns>0; -1 when DirectPlay refused the session; -2 when the player could not be created.</returns>
    /// <remarks>MCX.EXE @ 0x0074f660</remarks>
    int32_t HostSession(FIDPSession& session, char* playerName);
    /// <summary>The enumerated session whose instance GUID is <paramref name="sessionGUID"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x0074f800</remarks>
    FIDPSession* FindMatchingSession(_GUID* sessionGUID);
    /// <summary>Joins the enumerated session <paramref name="sessionGUID"/> as <paramref name="playerName"/>.</summary>
    /// <returns>0, -1 when no such session, or the DirectPlay error.</returns>
    /// <remarks>MCX.EXE @ 0x0074f8a0</remarks>
    int32_t JoinSession(_GUID* sessionGUID, char* playerName);

protected:
    /// <summary>Sends this machine's physical memory to the server (the server records its own).</summary>
    /// <remarks>MCX.EXE @ 0x0074f950</remarks>
    void SendSystemInformation();

public:
    /// <summary>Whether enough latency reports arrived to pick a new server.</summary>
    /// <remarks>MCX.EXE @ 0x0074fa50</remarks>
    int ReadyToChooseServer();

protected:
    /// <summary>Records the physical memory reported by player <paramref name="fromID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074fa80</remarks>
    void ProcessSystemInfoMessage(FISystemInfoMessage* msg, uint32_t fromID);

public:
    /// <summary>The host closes the session to new players.</summary>
    /// <returns>1 if locked, 0 when not hosting.</returns>
    /// <remarks>MCX.EXE @ 0x0074fac0</remarks>
    int LockSession();
    /// <summary>Closes this machine's player and the session, and recreates the DirectPlay object.</summary>
    /// <returns>1 if a session was left, else 0.</returns>
    /// <remarks>MCX.EXE @ 0x0074fb50</remarks>
    int LeaveSession();
    /// <summary>
    /// Creates group <paramref name="groupName"/> with <paramref name="data"/> attached and returns its id in
    /// <paramref name="groupID"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074fcc0</remarks>
    void CreateGroup(uint32_t* groupID, char* groupName, void* data, uint32_t dataSize, uint32_t flags);
    /// <summary>Adds player <paramref name="playerID"/> (0 = this machine's player) to group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074fe40</remarks>
    int AddPlayerToGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Removes player <paramref name="playerID"/> from the game.</summary>
    /// <remarks>MCX.EXE @ 0x0074ff70</remarks>
    int RemovePlayerWithID(uint32_t playerID);
    /// <summary>Destroys <paramref name="player"/>'s DirectPlay player (the host drops a player).</summary>
    /// <remarks>MCX.EXE @ 0x0074ffb0</remarks>
    int RemovePlayerFromGame(FIDPPlayer* player);
    /// <summary>Removes player <paramref name="playerID"/> (0 = this machine's player) from group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007500e0</remarks>
    int RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Sets group <paramref name="groupID"/>'s data.</summary>
    /// <remarks>MCX.EXE @ 0x00750200</remarks>
    void SetGroupData(uint32_t groupID, void* data, uint32_t dataSize, uint32_t flags);
    /// <summary>Initializes DirectPlay with the connection of <see cref="FIDPProtocolType"/> <paramref name="type"/>.</summary>
    /// <returns>0 or an error.</returns>
    /// <remarks>MCX.EXE @ 0x00750260</remarks>
    int32_t SetCurrentConnection(int type);
    /// <summary>Initializes an IPX connection.</summary>
    /// <remarks>MCX.EXE @ 0x00750370</remarks>
    void ConnectIPX();

protected:
    /// <summary>Restores the dial-up networking autodial setting saved by <see cref="DisableDialupNetworking"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00750390</remarks>
    static int32_t EnableDialupNetworking(uint32_t previousState);
    /// <summary>Switches off dial-up networking's autodial (in the registry) so a LAN game doesn't dial out.</summary>
    /// <returns>The previous setting.</returns>
    /// <remarks>MCX.EXE @ 0x007503f0</remarks>
    static uint32_t DisableDialupNetworking();

public:
    /// <summary>Initializes a TCP/IP connection to <paramref name="ipAddress"/> ("" = search the LAN).</summary>
    /// <remarks>MCX.EXE @ 0x00750490</remarks>
    void ConnectTCP(char* ipAddress);
    /// <summary>Initializes a modem connection dialing <paramref name="phoneNumber"/> on <paramref name="modemName"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00750560</remarks>
    int32_t ConnectModem(char* phoneNumber, char* modemName);
    /// <summary>Initializes a serial connection with the given port settings.</summary>
    /// <remarks>MCX.EXE @ 0x007506d0</remarks>
    int32_t ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity, uint32_t flowControl);
    /// <summary>Initializes DirectPlay with a compound address of <paramref name="numElements"/> elements.</summary>
    /// <remarks>MCX.EXE @ 0x00750790</remarks>
    int32_t InitializeConnection(DPCOMPOUNDADDRESSELEMENT* elements, int numElements);
    /// <summary>Adds a modem name from DirectPlay's modem enumeration (at most 10).</summary>
    /// <remarks>MCX.EXE @ 0x007508f0</remarks>
    void AddModemName(const void* data, uint32_t dataSize);
    /// <summary>Enumerates the modems.</summary>
    /// <returns>Nonzero when fewer than 10 were found (the list isn't full).</returns>
    /// <remarks>MCX.EXE @ 0x007509d0</remarks>
    int FindModems();
    /// <summary>Modem <paramref name="index"/>'s name, or null.</summary>
    /// <remarks>MCX.EXE @ 0x00750ad0</remarks>
    char* GetModemName(int32_t index);

protected:
    /// <summary>Creates the DirectPlay lobby object (IDirectPlayLobby2A) into <paramref name="lobby"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00750b00</remarks>
    static int32_t CreateLobby(void** lobby);

public:
    /// <summary>Whether the game was started by a DirectPlay lobby.</summary>
    /// <remarks>MCX.EXE @ 0x00750b60</remarks>
    int WasLaunchedFromLobby();
    /// <summary>
    /// Connects the session the lobby launched the game for, calling <paramref name="showStatus"/> while waiting and
    /// <paramref name="hideStatus"/> after.
    /// </summary>
    /// <returns>0 when connected; 0x8877042e (DPERR_NOTLOBBIED) when the game was not lobby-launched.</returns>
    /// <remarks>MCX.EXE @ 0x00750c00</remarks>
    uint32_t SetupLobbyConnection(void (*showStatus)(), void (*hideStatus)());
    /// <summary>The available connections.</summary>
    /// <remarks>MCX.EXE @ 0x007514e0</remarks>
    FLinkedList<FIDPNetworkProtocol>* GetConnections();
    /// <summary>Re-enumerates the sessions of this game on the current connection.</summary>
    /// <returns>The session list (its cursor rewound), or null without DirectPlay.</returns>
    /// <remarks>MCX.EXE @ 0x00751750. Unnamed in the symbols (the name is the port's).</remarks>
    FLinkedList<FIDPSession>* GetSessions();

private:
    /// <summary>Deletes every enumerated session except the current one.</summary>
    /// <remarks>MCX.EXE @ 0x00751510</remarks>
    void ClearSessionList();

public:
    /// <summary>The players of the current session (enumerated first when this machine has no player yet).</summary>
    /// <remarks>MCX.EXE @ 0x00751850</remarks>
    FLinkedList<FIDPPlayer>* GetPlayers(FIDPSession* session);

protected:
    /// <summary>Rebuilds the player list from DirectPlay.</summary>
    /// <remarks>MCX.EXE @ 0x007518b0</remarks>
    void EnumeratePlayers(FIDPSession* session);

public:
    /// <summary>The groups of the current session (enumerated first).</summary>
    /// <remarks>MCX.EXE @ 0x007519b0</remarks>
    FLinkedList<FIDPGroup>* GetGroups(FIDPSession* session);
    /// <summary>Dials the modem connection.</summary>
    /// <remarks>MCX.EXE @ 0x00751a90</remarks>
    int32_t Dial();
    /// <summary>Hangs up a dial in progress.</summary>
    /// <remarks>MCX.EXE @ 0x00751b50</remarks>
    void CancelDialing();
    /// <summary>Sends each player the numbers of the guaranteed messages received from it (its verify message).</summary>
    /// <remarks>MCX.EXE @ 0x00751b70</remarks>
    int SendVerifies();
    /// <summary>
    /// The per-frame pump: pings when due, resends unverified messages, sends file pieces and processes the system and
    /// application queues.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00751c70</remarks>
    void ProcessMessages();
    /// <summary>Handles the queued DirectPlay system messages.</summary>
    /// <remarks>MCX.EXE @ 0x00751d90</remarks>
    int ProcessSystemMessages();

protected:
    /// <summary>Hands each player's in-order guaranteed messages to the application queue.</summary>
    /// <remarks>MCX.EXE @ 0x00751e50</remarks>
    void ProcessGuaranteedMessages();

public:
    /// <summary>Handles the queued application messages.</summary>
    /// <remarks>MCX.EXE @ 0x00751fa0</remarks>
    int ProcessApplicationMessages();
    /// <summary>The receive thread: waits on the player event and sorts every received message into its queue.</summary>
    /// <remarks>MCX.EXE @ 0x00752030</remarks>
    int ReceiveThread();

private:
    /// <summary>Receive thread: routes one application message (verify, guaranteed, plain).</summary>
    /// <remarks>MCX.EXE @ 0x007521b0</remarks>
    void RTProcessApplicationMessage(FIDPMessage* msg);
    /// <summary>Receive thread: stores a guaranteed message from <paramref name="player"/> in order.</summary>
    /// <remarks>MCX.EXE @ 0x00752740</remarks>
    void RTHandleNewGuaranteedMessage(FIDPMessage* msg, FIDPPlayer* player);
    /// <summary>Receive thread: the DPID of player number <paramref name="playerNumber"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00752860</remarks>
    uint32_t RTGetIDFromPlayerNumber(int playerNumber);
    /// <summary>Receive thread: the player with id <paramref name="playerID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00752910</remarks>
    FIDPPlayer* RTGetPlayer(uint32_t playerID);

protected:
    /// <summary>Resends <paramref name="player"/>'s unverified messages whose delay ran out at <paramref name="now"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007529b0</remarks>
    void UpdatePlayerGuaranteedMessages(FIDPPlayer* player, uint32_t now);

public:
    /// <summary>Resends every player's overdue unverified messages.</summary>
    /// <remarks>MCX.EXE @ 0x00752b10</remarks>
    void UpdateGuaranteedMessages();
    /// <summary>
    /// Sends the next piece of the current outgoing file and drops the finished transfers. Original behaviour: the
    /// list cursor is never advanced, so with several transfers the first gets every piece until it is done.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00752bc0. Unnamed in the symbols (the name is the port's). Nothing in MCX.EXE calls it, so
    /// BroadcastFile only ever sends the announcement.
    /// </remarks>
    void UpdateFileTransfers();

private:
    /// <summary>Stamps each numbered player's next send count into <paramref name="header"/> for a group send.</summary>
    /// <remarks>MCX.EXE @ 0x00752df0</remarks>
    void SetupMessageSendCounts(FIGuaranteedMessageHeader* header, FLinkedList<FIDPPlayer>* players);

public:
    /// <summary>The host starts the game: locks the session.</summary>
    /// <remarks>MCX.EXE @ 0x00752f00</remarks>
    void StartGame();
    /// <summary>Sends the ping message.</summary>
    /// <remarks>MCX.EXE @ 0x00752f80</remarks>
    int32_t SendPing();

protected:
    /// <summary>Tells the players who is in group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007531c0</remarks>
    void SendPlayersInGroupMessages(uint32_t groupID);

private:
    /// <summary>Sends the guaranteed messages queued before this machine had a player number.</summary>
    /// <remarks>MCX.EXE @ 0x00753360</remarks>
    void SendPreIDGuaranteedMessages();

public:
    /// <summary>
    /// Sends a guaranteed message to every member of group <paramref name="groupID"/> (0 = everyone), or queues it
    /// until this machine has a player number.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00753470. Unnamed in the symbols (the name is the port's).</remarks>
    void SendMessageToGroup(uint32_t groupID, FIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>Fills <paramref name="list"/> with the players of group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007539d0</remarks>
    void GetPlayerListForGroup(uint32_t groupID, FLinkedList<FIDPPlayer>* list);
    /// <summary>Sorts the players by latency (<see cref="playersByLatency"/>) and returns the best one's id.</summary>
    /// <remarks>MCX.EXE @ 0x00753b30</remarks>
    uint32_t TallyLatencies();

protected:
    /// <summary>Records the latency player <paramref name="fromID"/> reported.</summary>
    /// <remarks>MCX.EXE @ 0x00753c10</remarks>
    void ProcessLatencyMessage(FIMessageHeader* msg, uint32_t fromID);

public:
    /// <summary>Reports this machine's average latency to the others.</summary>
    /// <remarks>MCX.EXE @ 0x00753d10</remarks>
    void SendLatencyInfo();
    /// <summary>Makes the lowest-latency player the server.</summary>
    /// <remarks>MCX.EXE @ 0x00753e50</remarks>
    void SwitchServers();
    /// <summary>
    /// Sends a guaranteed message to player <paramref name="playerID"/>: numbers it, sends it and queues it for
    /// verification (<paramref name="firstSend"/> = not a resend).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00753f80</remarks>
    void SendMessageToPlayerGuaranteed(uint32_t playerID, FIGuaranteedMessageHeader* header, uint32_t size,
                                       int firstSend);
    /// <summary>Sends a guaranteed message to the server.</summary>
    /// <remarks>MCX.EXE @ 0x007541b0</remarks>
    void SendMessageToServerGuaranteed(FIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to everyone.</summary>
    /// <remarks>MCX.EXE @ 0x00754260</remarks>
    void BroadcastMessage(FIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to the server (queued until this machine has a player number).</summary>
    /// <remarks>MCX.EXE @ 0x00754280</remarks>
    void SendMessageToServer(FIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to <paramref name="toID"/> through DirectPlay.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x00754350. The symbol reads <c>SendMessageA</c>: windows.h's SendMessage macro renamed the
    /// original's <c>SendMessage</c>. The port keeps the name the binary has.
    /// </remarks>
    int32_t SendMessageA(uint32_t toID, FIMessageHeader* header, uint32_t size);
    /// <summary>
    /// Starts sending <paramref name="fileName"/> in <paramref name="directory"/> to everyone;
    /// <paramref name="callback"/> is called when it is done.
    /// </summary>
    /// <returns>The transfer's id.</returns>
    /// <remarks>MCX.EXE @ 0x007543f0</remarks>
    int BroadcastFile(char* fileName, char* directory, void (*callback)(char* fileName, void* data));
    /// <summary>Sends <paramref name="msg"/> the way its header says (group, guaranteed or plain).</summary>
    /// <remarks>MCX.EXE @ 0x00754570</remarks>
    void SendMessageFromInfo(FIDPMessage* msg);
    /// <summary>The average bandwidth used, in <paramref name="bytesPerSecond"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00754680</remarks>
    int32_t GetAverageBandwidth(int* bytesPerSecond);
    /// <summary>
    /// The session manager, if one exists; <paramref name="owner"/> is remembered as the pointer's holder.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007546b0</remarks>
    static SessionManager* GetGlobalPointer(void* owner);
    /// <summary>Releases the global pointer if <paramref name="owner"/> holds it.</summary>
    /// <remarks>MCX.EXE @ 0x007546d0</remarks>
    static int ReleaseGlobalPointer(void* owner);

private:
    /// <summary>The server gives <paramref name="player"/> the lowest free player number and tells everyone.</summary>
    /// <remarks>MCX.EXE @ 0x00754700</remarks>
    void GivePlayerAnID(FIDPPlayer* player);

protected:
    /// <summary>A DirectPlay create-player/group system message: adds it (numbered by the server).</summary>
    /// <remarks>MCX.EXE @ 0x00754880</remarks>
    void AddPlayerOrGroup(uint32_t playerType, uint32_t id, uint32_t parentID, DPNAME* name, uint32_t flags);

public:
    /// <summary>The player with number <paramref name="playerNumber"/>, or null.</summary>
    /// <remarks>MCX.EXE @ 0x00754f20</remarks>
    FIDPPlayer* GetPlayerNumber(int32_t playerNumber);

protected:
    /// <summary>A player or group is leaving (system message before the deletion).</summary>
    /// <remarks>MCX.EXE @ 0x00754fc0</remarks>
    void PlayerOrGroupLeaving(uint32_t playerType, uint32_t id);
    /// <summary>A DirectPlay destroy-player/group system message: removes and deletes it.</summary>
    /// <remarks>MCX.EXE @ 0x007550d0</remarks>
    void DeletePlayerOrGroup(uint32_t playerType, uint32_t id);

public:
    /// <summary>Whether TCP/IP is installed (checked once, from the registry).</summary>
    /// <remarks>MCX.EXE @ 0x00755460</remarks>
    int isTCPAvailable();
    /// <summary>Whether IPX is installed (checked once, from the registry).</summary>
    /// <remarks>MCX.EXE @ 0x00755560</remarks>
    int isIPXAvailable();
    /// <summary>Whether a modem is installed (checked once).</summary>
    /// <remarks>MCX.EXE @ 0x00755660</remarks>
    int isModemAvailable();

private:
    /// <summary>A system message the receive thread handles before queueing it.</summary>
    /// <remarks>MCX.EXE @ 0x007556d0</remarks>
    void HandlePreSystemMessage(FIDPMessage* msg);
    /// <summary>A queued system message, handled on the game thread (calls <see cref="systemCallback"/>).</summary>
    /// <remarks>MCX.EXE @ 0x00755820</remarks>
    void HandlePostSystemMessage(FIDPMessage* msg);

protected:
    /// <summary>
    /// An application message on the game thread: the linkup layer's own types (player numbers, file transfer,
    /// system info, latency, ping) or the game's (<see cref="applicationCallback"/>).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007558d0</remarks>
    void HandleApplicationMessage(FIDPMessage* msg);
    /// <summary>A ping reply: measures the latency.</summary>
    /// <remarks>MCX.EXE @ 0x00756030</remarks>
    void HandlePingUpdate(FIDPMessage* msg);

private:
    /// <summary>Shows a DirectPlay error (in debug builds).</summary>
    /// <returns>Nonzero when <paramref name="error"/> is an error.</returns>
    /// <remarks>MCX.EXE @ 0x007560a0</remarks>
    int ReportError(uint32_t error);

public:
    /// <summary>Writes the session statistics to <paramref name="fileName"/>.</summary>
    /// <remarks>MCX.EXE @ 0x007569f0</remarks>
    void GetProfileData(char* fileName);
    /// <summary>Formats the latency and queue statistics into <paramref name="buffer"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00756cf0</remarks>
    int32_t GetStats(char* buffer);

    /// <summary>The connections DirectPlay offers.</summary>
    FLinkedList<FIDPNetworkProtocol> connections; // +0x4
    /// <summary>The enumerated (and hosted) sessions.</summary>
    FLinkedList<FIDPSession> sessions; // +0x14
    /// <summary>The players of the current session.</summary>
    FLinkedList<FIDPPlayer> players; // +0x24
    /// <summary>The groups of the current session.</summary>
    FLinkedList<FIDPGroup> groups; // +0x34
    /// <summary>Free message buffers (guarded by AddingMessageList).</summary>
    FIDPMsgList* emptyMessages = nullptr; // +0x44
    /// <summary>DirectPlay system messages waiting for the game thread.</summary>
    FIDPMsgList* systemMessages = nullptr; // +0x48
    /// <summary>Application messages waiting for the game thread.</summary>
    FIDPMsgList* applicationMessages = nullptr; // +0x4c
    /// <summary>
    /// Guaranteed messages received before this machine had a player number (it can't read their send counts yet);
    /// handled once the server's player numbers arrive.
    /// </summary>
    FIDPMsgList* preIDReceivedMessages = nullptr; // +0x50
    /// <summary>Group messages sent before this machine had a player number.</summary>
    FIDPMsgList* preIDGroupMessages = nullptr; // +0x54
    /// <summary>Server messages sent before this machine had a player number.</summary>
    FIDPMsgList* preIDServerMessages = nullptr; // +0x58
    /// <summary>Files being sent.</summary>
    FLinkedList<FileTransferInfo> outgoingFiles; // +0x5c
    /// <summary>Files being received.</summary>
    FLinkedList<FileTransferInfo> incomingFiles; // +0x6c
    /// <summary>Players that joined before this machine had a player number (numbered later).</summary>
    FLinkedList<FIDPPlayer> pendingPlayers; // +0x7c
    /// <summary>A second cursor over <see cref="players"/> (8 bytes, allocated with the global new).</summary>
    FLinkedListIterator<FIDPPlayer>* playerIterator = nullptr; // +0x8c
    /// <summary>
    /// Ids GetPlayer refuses (-1 = unused): players deleted from the session. DeletePlayerOrGroup overwrites the used
    /// slots; the exact bookkeeping is not pinned down.
    /// </summary>
    uint32_t deletedPlayerIDs[6]{}; // +0x90
    /// <summary>
    /// The "new server" message (type 6, 0xc bytes: the guaranteed header and the server's DPID at +0x8), kept
    /// ready to announce this machine as the server.
    /// </summary>
    FIGuaranteedMessageHeader* serverMessage = nullptr; // +0xa8
    /// <summary>The session hosted or joined, or null.</summary>
    FIDPSession* currentSession = nullptr; // +0xac
    /// <summary>The <see cref="FIDPProtocolType"/> of the current connection, -1 for none.</summary>
    int32_t currentConnection = 0; // +0xb0
    /// <summary>Signalled by DirectPlay when a message arrives for this machine's player (a HANDLE).</summary>
    void* playerEvent = nullptr; // +0xb4
    /// <summary>Signalled to stop the receive thread (a HANDLE).</summary>
    void* killReceiveEvent = nullptr; // +0xb8
    /// <summary>The receive thread (a HANDLE).</summary>
    void* receiveThread = nullptr; // +0xbc
    /// <summary>This machine's player's DPID (0 when none).</summary>
    uint32_t myPlayerID = 0; // +0xc0
    /// <summary>The server's DPID.</summary>
    uint32_t serverID = 0; // +0xc4
    /// <summary>This machine's player.</summary>
    FIDPPlayer* myPlayer = nullptr; // +0xc8
    /// <summary>Nonzero once <see cref="isModemAvailable"/> checked.</summary>
    int32_t modemChecked = 0;   // +0xd0
    int32_t modemAvailable = 0; // +0xd4
    /// <summary>Nonzero once <see cref="isIPXAvailable"/> checked.</summary>
    int32_t ipxChecked = 0;   // +0xd8
    int32_t ipxAvailable = 0; // +0xdc
    /// <summary>Nonzero once <see cref="isTCPAvailable"/> checked.</summary>
    int32_t tcpChecked = 0;   // +0xe0
    int32_t tcpAvailable = 0; // +0xe4
    /// <summary>Set when every latency report is in (<see cref="ReadyToChooseServer"/>).</summary>
    int32_t readyToChooseServer = 0; // +0xe8
    /// <summary>The autodial setting DisableDialupNetworking saved (0 = nothing to restore).</summary>
    uint32_t dialupState = 0; // +0xec
    /// <summary>Guards the player list and the outgoing queues (a CRITICAL_SECTION, 0x18 bytes, in the original).</summary>
    std::recursive_mutex criticalSection; // +0xf8
    /// <summary>The game's handler of application messages (MultiPlayerApplicationCallback).</summary>
    void (*applicationCallback)(FIDPMessage* msg, void* data) = nullptr; // +0x110
    void* applicationCallbackData = nullptr;                             // +0x114
    /// <summary>The game's handler of system messages (player created/destroyed, added to group, session lost).</summary>
    void (*systemCallback)(FIDPMessage* msg, void* data) = nullptr; // +0x118
    void* systemCallbackData = nullptr;                             // +0x11c
    /// <summary>Only cleared by the constructor: presumably the file-sent callback, which MultiPlayer never sets.</summary>
    void (*fileSentCallback)(char* fileName, void* data) = nullptr; // +0x120
    void* fileSentCallbackData = nullptr;                           // +0x124
    /// <summary>Called when a received file is complete (MultiPlayerFileReceivedCallback).</summary>
    void (*fileReceivedCallback)(char* fileName, void* data) = nullptr; // +0x128
    void* fileReceivedCallbackData = nullptr;                           // +0x12c
    /// <summary>Nonzero when this machine is the server (host).</summary>
    int32_t isHost = 0; // +0x130
    /// <summary>Set once the session is locked (the game started).</summary>
    int32_t gameStarted = 0; // +0x134
    /// <summary>Nonzero once this machine's player has a player number (set at once on the host).</summary>
    int32_t hasPlayerNumber = 0; // +0x138
    /// <summary>The DirectPlay object (IDirectPlay3A* in the original; the port's stand-in).</summary>
    MCDirectPlay* directPlay = nullptr; // +0x13c
    /// <summary>The <see cref="FIDPProtocolType"/> flags of the connections found.</summary>
    uint32_t availableProtocols = 0; // +0x140
    /// <summary>The modems found (each up to 63 characters used).</summary>
    char modemNames[10][256]{}; // +0x144
    /// <summary>The id of the next file transfer (0-255).</summary>
    int32_t nextFileID = 0; // +0xb44
    /// <summary>One 0xd800-byte linkUpBlocks block cut into <see cref="verifyMessages"/>.</summary>
    uint8_t* verifyMessageMemory = nullptr; // +0xb48
    /// <summary>
    /// Per player number: the verify message being built (0x2400 bytes: the header word, a count byte, then 6 bytes
    /// per verified message; sent as count * 6 + 3 bytes).
    /// </summary>
    uint8_t* verifyMessages[6]{}; // +0xb4c
    /// <summary>Player numbers 0-5 sorted by latency (TallyLatencies; reset to 0-5 by LeaveSession).</summary>
    int32_t playersByLatency[6]{}; // +0xb7c
    /// <summary>Performance-counter time of the next ping.</summary>
    uint32_t nextPingTime = 0; // +0xb94
    /// <summary>Milliseconds between pings (2000).</summary>
    uint32_t pingInterval = 0; // +0xb98
    /// <summary>The number of <see cref="modemNames"/>.</summary>
    int32_t numModems = 0; // +0xb9c
};

/// <summary>Empties <paramref name="list"/> (its links; the messages are not deleted).</summary>
/// <remarks>MCX.EXE @ 0x0074d4f0</remarks>
void ClearList(FIDPMsgList* list);

/// <summary>Sets VGA palette entry 0 (a debugging aid of the original: port writes to 0x3c8/0x3c9).</summary>
/// <remarks>MCX.EXE @ 0x0074d510</remarks>
void DebugColor(uint8_t red, uint8_t green, uint8_t blue);

/// <summary>Milliseconds since the SessionManager was made (<see cref="StartTime"/>).</summary>
/// <remarks>MCX.EXE @ 0x0074d540</remarks>
uint32_t TimeStamp();

/// <summary>Switches off the modems' caller-id reporting (AT#CID=0) in the registry for a modem game.</summary>
/// <remarks>MCX.EXE @ 0x0074d560</remarks>
void DisableCallerID();

/// <summary>Restores the caller-id settings <see cref="DisableCallerID"/> changed.</summary>
/// <remarks>MCX.EXE @ 0x0074d6d0</remarks>
void ReEnableCallerID();

/// <summary>Creates <see cref="linkUpBlocks"/> if it doesn't exist.</summary>
/// <remarks>MCX.EXE @ 0x0074d810 (InitLinkUpHeap)</remarks>
void InitLinkUpBlocks();

/// <summary>Frees every block of <see cref="linkUpBlocks"/> and the store.</summary>
/// <remarks>MCX.EXE @ 0x0074d8c0 (DestroyLinkUpHeap)</remarks>
void DestroyLinkUpBlocks();

/// <summary>DirectPlay EnumPlayers callback: forwards to SessionManager::NewPlayerEnumeration.</summary>
/// <remarks>MCX.EXE @ 0x0074dd00 (__stdcall)</remarks>
int EnumPlayersCallback(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags, void* context);

/// <summary>DirectPlay EnumGroups callback: forwards to SessionManager::NewGroupEnumeration.</summary>
/// <remarks>MCX.EXE @ 0x0074dd30 (__stdcall)</remarks>
int EnumGroupsCallback(uint32_t groupID, uint32_t groupType, const DPNAME* name, uint32_t flags, void* context);

/// <summary>The receive thread's entry: runs SessionManager::ReceiveThread on <paramref name="sessionManager"/>.</summary>
/// <remarks>MCX.EXE @ 0x0074dd60 (__stdcall)</remarks>
uint32_t SessionManagerReceiveThread(void* sessionManager);

/// <summary>DirectPlay EnumConnections callback: forwards to SessionManager::AddConnection.</summary>
/// <remarks>MCX.EXE @ 0x0074eaf0 (__stdcall). Unnamed in the symbols (the name is the port's).</remarks>
int EnumConnectionsCallback(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                            uint32_t flags, void* context);

/// <summary>DirectPlay address-enumeration callback: collects modem names into the SessionManager.</summary>
/// <remarks>MCX.EXE @ 0x00750890 (__stdcall)</remarks>
int ModemCallback(const _GUID& dataType, uint32_t dataSize, const void* data, void* context);

/// <summary>DirectPlay EnumSessions callback: forwards to SessionManager::AddSession (stops on timeout).</summary>
/// <remarks>MCX.EXE @ 0x00751800 (__stdcall). Unnamed in the symbols (the name is the port's).</remarks>
int EnumSessionsCallback(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context);

/// <summary>Removes entry <paramref name="index"/> of an array of <paramref name="count"/> by shifting the rest down.</summary>
/// <remarks>MCX.EXE @ 0x00751d50</remarks>
void ShiftPointerArray(int32_t* array, int index, int count);

/// <summary>qsort comparator of two player numbers by their players' average latency.</summary>
/// <remarks>MCX.EXE @ 0x00753140. Unnamed in the symbols (the name is the port's).</remarks>
int CompareLatencies(const int32_t* playerNumber1, const int32_t* playerNumber2);

/// <summary>qsort comparator of two longs.</summary>
/// <remarks>MCX.EXE @ 0x00754850. Unnamed in the symbols (the name is the port's).</remarks>
int CompareLongs(const int32_t* value1, const int32_t* value2);

/// <summary>Writes the session manager's statistics to the log.</summary>
/// <remarks>MCX.EXE @ 0x00756b80</remarks>
void OutputSessionManagerStats();
