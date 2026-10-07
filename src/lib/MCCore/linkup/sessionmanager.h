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

class MCFidpGroup;
class MCFidpMessage;
class MCFidpPlayer;
class MCFidpSession;
class MCFileTransferInfo;
/// <summary>
/// The linkup blocks that have no single owner yet: message buffers, player and group ids, file names (the linkup
/// heap's in the original, 1,900,000 bytes). Freeing a block that isn't one is ignored, as the heap did.
/// </summary>
extern std::unique_ptr<MCBlockStore> LinkUpBlocks;
/// <summary>The game's DirectPlay application id, set by the SessionManager constructor.</summary>
extern _GUID ThisAppGuid;
/// <summary>Performance-counter ticks per millisecond.</summary>
extern uint32_t TicksPerMS;
/// <summary><c>timeGetTime</c> when the SessionManager was made (<see cref="TimeStamp"/> counts from it).</summary>
extern uint32_t StartTime;
/// <summary>Guards the free-message queue (a CRITICAL_SECTION in the original).</summary>
extern std::recursive_mutex AddingMessageList;
/// <summary>
/// DPIDs of players the server numbered before this machine knew them: slot n holds the id given player number n.
/// </summary>
extern uint32_t NewPlayerNumbers[6];
/// <summary>Nonzero once the host locked the session (no new players).</summary>
extern int SessionLocked;
/// <summary>Nonzero while the receive thread processes a message.</summary>
extern int InReceiveThread;
/// <summary>Nonzero when the other side runs an older MultiPlayer (found while connecting).</summary>
extern int OldVersionOfMPlayer;
/// <summary>Nonzero while the modems' caller-id reporting is switched off by <see cref="DisableCallerID"/>.</summary>
extern int DisabledCallerID;
/// <summary>Per modem (0-5): nonzero when <see cref="DisableCallerID"/> changed its registry setting.</summary>
extern int CallerIDChanged[6];
/// <summary>
/// Nonzero when the game was started by a DirectPlay lobby (userInit sets it from
/// SessionManager::WasLaunchedFromLobby). Lobby games take their player count from the session and skip the
/// pre-numbering message queues.
/// </summary>
extern int32_t LaunchedFromLobby;

/// <summary>Protocol flags: <see cref="MCFidpNetworkProtocol::ProtocolType"/> and SessionManager's available set.</summary>
/// <remarks>The names are the port's; the values are the original's (SessionManager::SetConnectionType).</remarks>
enum MCFidpProtocolType
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
/// initializes it with, and its <see cref="MCFidpProtocolType"/>.
/// </summary>
/// <remarks>Original source: <c>linkup\sessionmanager.cpp</c>, 0x14c bytes (allocated with the global new).</remarks>
class MCFidpNetworkProtocol
{
public:
    MCFidpNetworkProtocol();
    /// <summary>Frees the connection buffer (<see cref="Destroy"/>).</summary>
    virtual ~MCFidpNetworkProtocol();

    MCFidpNetworkProtocol(const MCFidpNetworkProtocol&) = delete;
    MCFidpNetworkProtocol& operator=(const MCFidpNetworkProtocol&) = delete;

    /// <summary>Frees the connection buffer.</summary>
    void Destroy();

    /// <summary>Keeps a copy of <paramref name="size"/> bytes of DirectPlay connection data (a linkUpBlocks block).</summary>
    /// <returns>0, or -1 when out of memory.</returns>
    int SetConnectionBuffer(void* connection, int size);

    /// <summary>Sets the short name (up to 63 characters).</summary>
    void SetShortName(char* shortName);

    /// <summary>Sets the long name (up to 255 characters).</summary>
    void SetLongName(char* longName);

    /// <summary>The short name.</summary>
    char* GetShortName() { return ShortName; }

    /// <summary>The <see cref="MCFidpProtocolType"/>, -1 when not set.</summary>
    int GetProtocolType() { return ProtocolType; }

    /// <summary>Destroys and deletes every protocol of <paramref name="list"/> and empties it.</summary>
    static void ClearList(MCFLinkedList<MCFidpNetworkProtocol>& list);

    char ShortName[64]{};
    char LongName[256]{};
    /// <summary>DirectPlay's connection data (a linkUpBlocks block).</summary>
    void* ConnectionBuffer = nullptr;
    /// <summary>The <see cref="MCFidpProtocolType"/>, -1 when not set.</summary>
    int32_t ProtocolType = 0;
};

/// <summary>
/// The game's DirectPlay session: there is one at a time, found through <see cref="GetGlobalPointer"/>.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\sessionmanager.cpp</c>, 0xba0 bytes. The file-static
/// 0x0080a680 (an instance exists), 0x0080a684 (the instance) and 0x0080a688 (who holds the global pointer)
/// back <see cref="GetGlobalPointer"/>.
/// </remarks>
class MCSessionManager
{
public:
    /// <summary>
    /// Makes the session manager for application <paramref name="appGUID"/>: creates the DirectPlay object and
    /// enumerates the connections, allocates the message queues and per-player verify buffers, and registers
    /// itself as the global instance.
    /// </summary>
    explicit MCSessionManager(_GUID appGUID);
    /// <summary>Calls <see cref="Destroy"/> and empties every list.</summary>
    virtual ~MCSessionManager();

    MCSessionManager(const MCSessionManager&) = delete;
    MCSessionManager& operator=(const MCSessionManager&) = delete;

    // The DirectPlay enumeration callbacks forward to the private handlers (the original's __stdcall thunks).
    friend int EnumPlayersCallback(uint32_t, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumGroupsCallback(uint32_t, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumConnectionsCallback(const _GUID*, void*, uint32_t, const DPNAME*, uint32_t, void*);
    friend int EnumSessionsCallback(const DPSESSIONDESC2*, uint32_t*, uint32_t, void*);

    /// <summary>
    /// Deletes the message queues, releases the DirectPlay object and the global-instance registration.
    /// </summary>
    virtual void Destroy();

protected:
    /// <summary>Creates the DirectPlay object (IDirectPlay3A) if needed and enumerates the connections.</summary>
    /// <returns>0, or 1 when DirectPlay could not be created.</returns>
    int32_t CreateDirectPlayInterface();

public:
    /// <summary>Leaves the session and releases the DirectPlay object.</summary>
    int32_t DestroyDirectPlayInterface();

protected:
    /// <summary>Clears <paramref name="msg"/> and returns it to the free queue.</summary>
    void AddMessageToEmptyQueue(MCFidpMessage* msg);
    /// <summary>Takes a message from the free queue (null when empty).</summary>
    MCFidpMessage* GetMessageFromEmptyQueue();
    /// <summary>Rebuilds <see cref="Connections"/> from DirectPlay's EnumConnections.</summary>
    int32_t EnumerateConnections();

private:
    /// <summary>Sets <paramref name="protocol"/>'s type from its service-provider GUID and adds it to the available set.</summary>
    void SetConnectionType(MCFidpNetworkProtocol* protocol, const _GUID* guid);
    /// <summary>
    /// EnumConnections callback: adds a connection (checks <paramref name="context"/> is this manager).
    /// </summary>
    int AddConnection(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                      uint32_t flags, void* context);
    /// <summary>EnumSessions callback: adds a session not already current.</summary>
    int AddSession(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context);
    /// <summary>EnumPlayers callback: adds player <paramref name="playerID"/> and numbers it when needed.</summary>
    int NewPlayerEnumeration(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags);
    /// <summary>EnumGroups callback: adds group <paramref name="groupID"/>.</summary>
    int NewGroupEnumeration(uint32_t groupID, const DPNAME* name, uint32_t flags);

public:
    /// <summary>The group with id <paramref name="groupID"/>, or null.</summary>
    MCFidpGroup* GetGroup(uint32_t groupID);
    /// <summary>The player with id <paramref name="playerID"/>, or null (also null for a deleted player's id).</summary>
    MCFidpPlayer* GetPlayer(uint32_t playerID);

protected:
    /// <summary>
    /// Creates this machine's player named <paramref name="playerName"/> in the current session, fills the free
    /// queue with 900 messages and enumerates the players and groups.
    /// </summary>
    /// <returns>0 or the DirectPlay error.</returns>
    int32_t CreatePlayer(char* playerName);

public:
    /// <summary>Sets <see cref="HomeDirectory"/> (up to 511 characters).</summary>
    void SetHomeDirectory(char* directory);
    /// <summary>Hosts <paramref name="session"/> and creates this machine's player <paramref name="playerName"/>.</summary>
    /// <returns>0; -1 when DirectPlay refused the session; -2 when the player could not be created.</returns>
    int32_t HostSession(MCFidpSession& session, char* playerName);
    /// <summary>The enumerated session whose instance GUID is <paramref name="sessionGUID"/>, or null.</summary>
    MCFidpSession* FindMatchingSession(_GUID* sessionGUID);
    /// <summary>Joins the enumerated session <paramref name="sessionGUID"/> as <paramref name="playerName"/>.</summary>
    /// <returns>0, -1 when no such session, or the DirectPlay error.</returns>
    int32_t JoinSession(_GUID* sessionGUID, char* playerName);

protected:
    /// <summary>Sends this machine's physical memory to the server (the server records its own).</summary>
    void SendSystemInformation();

public:
    /// <summary>Whether enough latency reports arrived to pick a new server.</summary>
    int ReadyToChooseServer();

protected:
    /// <summary>Records the physical memory reported by player <paramref name="fromID"/>.</summary>
    void ProcessSystemInfoMessage(MCFISystemInfoMessage* msg, uint32_t fromID);

public:
    /// <summary>The host closes the session to new players.</summary>
    /// <returns>1 if locked, 0 when not hosting.</returns>
    int LockSession();
    /// <summary>Closes this machine's player and the session, and recreates the DirectPlay object.</summary>
    /// <returns>1 if a session was left, else 0.</returns>
    int LeaveSession();
    /// <summary>
    /// Creates group <paramref name="groupName"/> with <paramref name="data"/> attached and returns its id in
    /// <paramref name="groupID"/>.
    /// </summary>
    void CreateGroup(uint32_t* groupID, char* groupName, void* data, uint32_t dataSize, uint32_t flags);
    /// <summary>Adds player <paramref name="playerID"/> (0 = this machine's player) to group <paramref name="groupID"/>.</summary>
    int AddPlayerToGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Removes player <paramref name="playerID"/> from the game.</summary>
    int RemovePlayerWithID(uint32_t playerID);
    /// <summary>Destroys <paramref name="player"/>'s DirectPlay player (the host drops a player).</summary>
    int RemovePlayerFromGame(MCFidpPlayer* player);
    /// <summary>Removes player <paramref name="playerID"/> (0 = this machine's player) from group <paramref name="groupID"/>.</summary>
    int RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Sets group <paramref name="groupID"/>'s data.</summary>
    void SetGroupData(uint32_t groupID, void* data, uint32_t dataSize, uint32_t flags);
    /// <summary>Initializes DirectPlay with the connection of <see cref="MCFidpProtocolType"/> <paramref name="type"/>.</summary>
    /// <returns>0 or an error.</returns>
    int32_t SetCurrentConnection(int type);
    /// <summary>Initializes an IPX connection.</summary>
    void ConnectIpx();

protected:
    /// <summary>Restores the dial-up networking autodial setting saved by <see cref="DisableDialupNetworking"/>.</summary>
    static int32_t EnableDialupNetworking(uint32_t previousState);
    /// <summary>Switches off dial-up networking's autodial (in the registry) so a LAN game doesn't dial out.</summary>
    /// <returns>The previous setting.</returns>
    static uint32_t DisableDialupNetworking();

public:
    /// <summary>Initializes a TCP/IP connection to <paramref name="ipAddress"/> ("" = search the LAN).</summary>
    void ConnectTcp(char* ipAddress);
    /// <summary>Initializes a modem connection dialing <paramref name="phoneNumber"/> on <paramref name="modemName"/>.</summary>
    int32_t ConnectModem(char* phoneNumber, char* modemName);
    /// <summary>Initializes a serial connection with the given port settings.</summary>
    int32_t ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity, uint32_t flowControl);
    /// <summary>Initializes DirectPlay with a compound address of <paramref name="numElements"/> elements.</summary>
    int32_t InitializeConnection(DPCOMPOUNDADDRESSELEMENT* elements, int numElements);
    /// <summary>Adds a modem name from DirectPlay's modem enumeration (at most 10).</summary>
    void AddModemName(const void* data, uint32_t dataSize);
    /// <summary>Enumerates the modems.</summary>
    /// <returns>Nonzero when fewer than 10 were found (the list isn't full).</returns>
    int FindModems();
    /// <summary>Modem <paramref name="index"/>'s name, or null.</summary>
    char* GetModemName(int32_t index);

protected:
    /// <summary>Creates the DirectPlay lobby object (IDirectPlayLobby2A) into <paramref name="lobby"/>.</summary>
    static int32_t CreateLobby(void** lobby);

public:
    /// <summary>Whether the game was started by a DirectPlay lobby.</summary>
    int WasLaunchedFromLobby();
    /// <summary>
    /// Connects the session the lobby launched the game for, calling <paramref name="showStatus"/> while waiting and
    /// <paramref name="hideStatus"/> after.
    /// </summary>
    /// <returns>0 when connected; 0x8877042e (DPERR_NOTLOBBIED) when the game was not lobby-launched.</returns>
    uint32_t SetupLobbyConnection(void (*showStatus)(), void (*hideStatus)());
    /// <summary>The available connections.</summary>
    MCFLinkedList<MCFidpNetworkProtocol>* GetConnections();
    /// <summary>Re-enumerates the sessions of this game on the current connection.</summary>
    /// <returns>The session list (its cursor rewound), or null without DirectPlay.</returns>
    MCFLinkedList<MCFidpSession>* GetSessions();

private:
    /// <summary>Deletes every enumerated session except the current one.</summary>
    void ClearSessionList();

public:
    /// <summary>The players of the current session (enumerated first when this machine has no player yet).</summary>
    MCFLinkedList<MCFidpPlayer>* GetPlayers(MCFidpSession* session);

protected:
    /// <summary>Rebuilds the player list from DirectPlay.</summary>
    void EnumeratePlayers(MCFidpSession* session);

public:
    /// <summary>The groups of the current session (enumerated first).</summary>
    MCFLinkedList<MCFidpGroup>* GetGroups(MCFidpSession* session);
    /// <summary>Dials the modem connection.</summary>
    int32_t Dial();
    /// <summary>Hangs up a dial in progress.</summary>
    void CancelDialing();
    /// <summary>Sends each player the numbers of the guaranteed messages received from it (its verify message).</summary>
    int SendVerifies();
    /// <summary>
    /// The per-frame pump: pings when due, resends unverified messages, sends file pieces and processes the system and
    /// application queues.
    /// </summary>
    void ProcessMessages();
    /// <summary>Handles the queued DirectPlay system messages.</summary>
    int ProcessSystemMessages();

protected:
    /// <summary>Hands each player's in-order guaranteed messages to the application queue.</summary>
    void ProcessGuaranteedMessages();

public:
    /// <summary>Handles the queued application messages.</summary>
    int ProcessApplicationMessages();
    /// <summary>The receive thread: waits on the player event and sorts every received message into its queue.</summary>
    int ReceiveThread();

private:
    /// <summary>Receive thread: routes one application message (verify, guaranteed, plain).</summary>
    void RTProcessApplicationMessage(MCFidpMessage* msg);
    /// <summary>Receive thread: stores a guaranteed message from <paramref name="player"/> in order.</summary>
    void RTHandleNewGuaranteedMessage(MCFidpMessage* msg, MCFidpPlayer* player);
    /// <summary>Receive thread: the DPID of player number <paramref name="playerNumber"/>.</summary>
    uint32_t RTGetIDFromPlayerNumber(int playerNumber);
    /// <summary>Receive thread: the player with id <paramref name="playerID"/>.</summary>
    MCFidpPlayer* RTGetPlayer(uint32_t playerID);

protected:
    /// <summary>Resends <paramref name="player"/>'s unverified messages whose delay ran out at <paramref name="now"/>.</summary>
    void UpdatePlayerGuaranteedMessages(MCFidpPlayer* player, uint32_t now);

public:
    /// <summary>Resends every player's overdue unverified messages.</summary>
    void UpdateGuaranteedMessages();
    /// <summary>
    /// Sends the next piece of the current outgoing file and drops the finished transfers. Original behaviour: the
    /// list cursor is never advanced, so with several transfers the first gets every piece until it is done.
    /// </summary>
    /// <remarks>
    /// Nothing in MCX.EXE calls it, so BroadcastFile only ever sends the announcement.
    /// </remarks>
    void UpdateFileTransfers();

private:
    /// <summary>Stamps each numbered player's next send count into <paramref name="header"/> for a group send.</summary>
    void SetupMessageSendCounts(MCFIGuaranteedMessageHeader* header, MCFLinkedList<MCFidpPlayer>* players);

public:
    /// <summary>The host starts the game: locks the session.</summary>
    void StartGame();
    /// <summary>Sends the ping message.</summary>
    int32_t SendPing();

protected:
    /// <summary>Tells the players who is in group <paramref name="groupID"/>.</summary>
    void SendPlayersInGroupMessages(uint32_t groupID);

private:
    /// <summary>Sends the guaranteed messages queued before this machine had a player number.</summary>
    void SendPreIDGuaranteedMessages();

public:
    /// <summary>
    /// Sends a guaranteed message to every member of group <paramref name="groupID"/> (0 = everyone), or queues it
    /// until this machine has a player number.
    /// </summary>
    void SendMessageToGroup(uint32_t groupID, MCFIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>Fills <paramref name="list"/> with the players of group <paramref name="groupID"/>.</summary>
    void GetPlayerListForGroup(uint32_t groupID, MCFLinkedList<MCFidpPlayer>* list);
    /// <summary>Sorts the players by latency (<see cref="PlayersByLatency"/>) and returns the best one's id.</summary>
    uint32_t TallyLatencies();

protected:
    /// <summary>Records the latency player <paramref name="fromID"/> reported.</summary>
    void ProcessLatencyMessage(MCFIMessageHeader* msg, uint32_t fromID);

public:
    /// <summary>Reports this machine's average latency to the others.</summary>
    void SendLatencyInfo();
    /// <summary>Makes the lowest-latency player the server.</summary>
    void SwitchServers();
    /// <summary>
    /// Sends a guaranteed message to player <paramref name="playerID"/>: numbers it, sends it and queues it for
    /// verification (<paramref name="firstSend"/> = not a resend).
    /// </summary>
    void SendMessageToPlayerGuaranteed(uint32_t playerID, MCFIGuaranteedMessageHeader* header, uint32_t size,
                                       int firstSend);
    /// <summary>Sends a guaranteed message to the server.</summary>
    void SendMessageToServerGuaranteed(MCFIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to everyone.</summary>
    void BroadcastMessage(MCFIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to the server (queued until this machine has a player number).</summary>
    void SendMessageToServer(MCFIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to <paramref name="toID"/> through DirectPlay.</summary>
    /// <remarks>
    /// The symbol reads <c>SendMessageA</c>: windows.h's SendMessage macro renamed the
    /// original's <c>SendMessage</c>. The port keeps the name the binary has.
    /// </remarks>
    int32_t SendMessageA(uint32_t toID, MCFIMessageHeader* header, uint32_t size);
    /// <summary>
    /// Starts sending <paramref name="fileName"/> in <paramref name="directory"/> to everyone;
    /// <paramref name="callback"/> is called when it is done.
    /// </summary>
    /// <returns>The transfer's id.</returns>
    int BroadcastFile(char* fileName, char* directory, void (*callback)(char* fileName, void* data));
    /// <summary>Sends <paramref name="msg"/> the way its header says (group, guaranteed or plain).</summary>
    void SendMessageFromInfo(MCFidpMessage* msg);
    /// <summary>The average bandwidth used, in <paramref name="bytesPerSecond"/>.</summary>
    int32_t GetAverageBandwidth(int* bytesPerSecond);
    /// <summary>
    /// The session manager, if one exists; <paramref name="owner"/> is remembered as the pointer's holder.
    /// </summary>
    static MCSessionManager* GetGlobalPointer(void* owner);
    /// <summary>Releases the global pointer if <paramref name="owner"/> holds it.</summary>
    static int ReleaseGlobalPointer(void* owner);

private:
    /// <summary>The server gives <paramref name="player"/> the lowest free player number and tells everyone.</summary>
    void GivePlayerAnID(MCFidpPlayer* player);

protected:
    /// <summary>A DirectPlay create-player/group system message: adds it (numbered by the server).</summary>
    void AddPlayerOrGroup(uint32_t playerType, uint32_t id, uint32_t parentID, DPNAME* name, uint32_t flags);

public:
    /// <summary>The player with number <paramref name="playerNumber"/>, or null.</summary>
    MCFidpPlayer* GetPlayerNumber(int32_t playerNumber);

protected:
    /// <summary>A player or group is leaving (system message before the deletion).</summary>
    void PlayerOrGroupLeaving(uint32_t playerType, uint32_t id);
    /// <summary>A DirectPlay destroy-player/group system message: removes and deletes it.</summary>
    void DeletePlayerOrGroup(uint32_t playerType, uint32_t id);

public:
    /// <summary>Whether TCP/IP is installed (checked once, from the registry).</summary>
    int IsTcpAvailable();
    /// <summary>Whether IPX is installed (checked once, from the registry).</summary>
    int IsIpxAvailable();
    /// <summary>Whether a modem is installed (checked once).</summary>
    int IsModemAvailable();

private:
    /// <summary>A system message the receive thread handles before queueing it.</summary>
    void HandlePreSystemMessage(MCFidpMessage* msg);
    /// <summary>A queued system message, handled on the game thread (calls <see cref="SystemCallback"/>).</summary>
    void HandlePostSystemMessage(MCFidpMessage* msg);

protected:
    /// <summary>
    /// An application message on the game thread: the linkup layer's own types (player numbers, file transfer,
    /// system info, latency, ping) or the game's (<see cref="ApplicationCallback"/>).
    /// </summary>
    void HandleApplicationMessage(MCFidpMessage* msg);
    /// <summary>A ping reply: measures the latency.</summary>
    void HandlePingUpdate(MCFidpMessage* msg);

private:
    /// <summary>Shows a DirectPlay error (in debug builds).</summary>
    /// <returns>Nonzero when <paramref name="error"/> is an error.</returns>
    int ReportError(uint32_t error);

public:
    /// <summary>Writes the session statistics to <paramref name="fileName"/>.</summary>
    void GetProfileData(char* fileName);
    /// <summary>Formats the latency and queue statistics into <paramref name="buffer"/>.</summary>
    int32_t GetStats(char* buffer);

    /// <summary>The connections DirectPlay offers.</summary>
    MCFLinkedList<MCFidpNetworkProtocol> Connections;
    /// <summary>The enumerated (and hosted) sessions.</summary>
    MCFLinkedList<MCFidpSession> Sessions;
    /// <summary>The players of the current session.</summary>
    MCFLinkedList<MCFidpPlayer> Players;
    /// <summary>The groups of the current session.</summary>
    MCFLinkedList<MCFidpGroup> Groups;
    /// <summary>Free message buffers (guarded by AddingMessageList).</summary>
    MCFidpMsgList* EmptyMessages = nullptr;
    /// <summary>DirectPlay system messages waiting for the game thread.</summary>
    MCFidpMsgList* SystemMessages = nullptr;
    /// <summary>Application messages waiting for the game thread.</summary>
    MCFidpMsgList* ApplicationMessages = nullptr;
    /// <summary>
    /// Guaranteed messages received before this machine had a player number (it can't read their send counts yet);
    /// handled once the server's player numbers arrive.
    /// </summary>
    MCFidpMsgList* PreIDReceivedMessages = nullptr;
    /// <summary>Group messages sent before this machine had a player number.</summary>
    MCFidpMsgList* PreIDGroupMessages = nullptr;
    /// <summary>Server messages sent before this machine had a player number.</summary>
    MCFidpMsgList* PreIDServerMessages = nullptr;
    /// <summary>Files being sent.</summary>
    MCFLinkedList<MCFileTransferInfo> OutgoingFiles;
    /// <summary>Files being received.</summary>
    MCFLinkedList<MCFileTransferInfo> IncomingFiles;
    /// <summary>Players that joined before this machine had a player number (numbered later).</summary>
    MCFLinkedList<MCFidpPlayer> PendingPlayers;
    /// <summary>A second cursor over <see cref="Players"/> (8 bytes, allocated with the global new).</summary>
    MCFLinkedListIterator<MCFidpPlayer>* PlayerIterator = nullptr;
    /// <summary>
    /// Ids GetPlayer refuses (-1 = unused): players deleted from the session. DeletePlayerOrGroup overwrites the used
    /// slots; the exact bookkeeping is not pinned down.
    /// </summary>
    uint32_t DeletedPlayerIDs[6]{};
    /// <summary>
    /// The "new server" message (type 6, 0xc bytes: the guaranteed header and the server's DPID at +0x8), kept
    /// ready to announce this machine as the server.
    /// </summary>
    MCFIGuaranteedMessageHeader* ServerMessage = nullptr;
    /// <summary>The session hosted or joined, or null.</summary>
    MCFidpSession* CurrentSession = nullptr;
    /// <summary>The <see cref="MCFidpProtocolType"/> of the current connection, -1 for none.</summary>
    int32_t CurrentConnection = 0;
    /// <summary>Signalled by DirectPlay when a message arrives for this machine's player (a HANDLE).</summary>
    void* PlayerEvent = nullptr;
    /// <summary>Signalled to stop the receive thread (a HANDLE).</summary>
    void* KillReceiveEvent = nullptr;
    /// <summary>The receive thread (a HANDLE).</summary>
    void* ReceiveThreadHandle = nullptr;
    /// <summary>This machine's player's DPID (0 when none).</summary>
    uint32_t MyPlayerID = 0;
    /// <summary>The server's DPID.</summary>
    uint32_t ServerID = 0;
    /// <summary>This machine's player.</summary>
    MCFidpPlayer* MyPlayer = nullptr;
    /// <summary>Nonzero once <see cref="IsModemAvailable"/> checked.</summary>
    int32_t ModemChecked = 0;
    int32_t ModemAvailable = 0;
    /// <summary>Nonzero once <see cref="IsIpxAvailable"/> checked.</summary>
    int32_t IpxChecked = 0;
    int32_t IpxAvailable = 0;
    /// <summary>Nonzero once <see cref="IsTcpAvailable"/> checked.</summary>
    int32_t TcpChecked = 0;
    int32_t TcpAvailable = 0;
    /// <summary>Set when every latency report is in (<see cref="ReadyToChooseServer"/>).</summary>
    int32_t LatencyReportsIn = 0;
    /// <summary>The autodial setting DisableDialupNetworking saved (0 = nothing to restore).</summary>
    uint32_t DialupState = 0;
    /// <summary>Guards the player list and the outgoing queues (a CRITICAL_SECTION, 0x18 bytes, in the original).</summary>
    std::recursive_mutex CriticalSection;
    /// <summary>The game's handler of application messages (MultiPlayerApplicationCallback).</summary>
    void (*ApplicationCallback)(MCFidpMessage* msg, void* data) = nullptr;
    void* ApplicationCallbackData = nullptr;
    /// <summary>The game's handler of system messages (player created/destroyed, added to group, session lost).</summary>
    void (*SystemCallback)(MCFidpMessage* msg, void* data) = nullptr;
    void* SystemCallbackData = nullptr;
    /// <summary>Only cleared by the constructor: presumably the file-sent callback, which MultiPlayer never sets.</summary>
    void (*FileSentCallback)(char* fileName, void* data) = nullptr;
    void* FileSentCallbackData = nullptr;
    /// <summary>Called when a received file is complete (MultiPlayerFileReceivedCallback).</summary>
    void (*FileReceivedCallback)(char* fileName, void* data) = nullptr;
    void* FileReceivedCallbackData = nullptr;
    /// <summary>Nonzero when this machine is the server (host).</summary>
    int32_t IsHost = 0;
    /// <summary>Set once the session is locked (the game started).</summary>
    int32_t GameStarted = 0;
    /// <summary>Nonzero once this machine's player has a player number (set at once on the host).</summary>
    int32_t HasPlayerNumber = 0;
    /// <summary>The DirectPlay object (IDirectPlay3A* in the original; the port's stand-in).</summary>
    MCDirectPlay* DirectPlay = nullptr;
    /// <summary>The <see cref="MCFidpProtocolType"/> flags of the connections found.</summary>
    uint32_t AvailableProtocols = 0;
    /// <summary>The modems found (each up to 63 characters used).</summary>
    char ModemNames[10][256]{};
    /// <summary>The id of the next file transfer (0-255).</summary>
    int32_t NextFileID = 0;
    /// <summary>One 0xd800-byte linkUpBlocks block cut into <see cref="VerifyMessages"/>.</summary>
    uint8_t* VerifyMessageMemory = nullptr;
    /// <summary>
    /// Per player number: the verify message being built (0x2400 bytes: the header word, a count byte, then 6 bytes
    /// per verified message; sent as count * 6 + 3 bytes).
    /// </summary>
    uint8_t* VerifyMessages[6]{};
    /// <summary>Player numbers 0-5 sorted by latency (TallyLatencies; reset to 0-5 by LeaveSession).</summary>
    int32_t PlayersByLatency[6]{};
    /// <summary>Performance-counter time of the next ping.</summary>
    uint32_t NextPingTime = 0;
    /// <summary>Milliseconds between pings (2000).</summary>
    uint32_t PingInterval = 0;
    /// <summary>The number of <see cref="ModemNames"/>.</summary>
    int32_t NumModems = 0;
};

/// <summary>Empties <paramref name="list"/> (its links; the messages are not deleted).</summary>
void ClearList(MCFidpMsgList* list);

/// <summary>Sets VGA palette entry 0 (a debugging aid of the original: port writes to 0x3c8/0x3c9).</summary>
void DebugColor(uint8_t red, uint8_t green, uint8_t blue);

/// <summary>Milliseconds since the SessionManager was made (<see cref="StartTime"/>).</summary>
uint32_t TimeStamp();

/// <summary>Switches off the modems' caller-id reporting (AT#CID=0) in the registry for a modem game.</summary>
void DisableCallerID();

/// <summary>Restores the caller-id settings <see cref="DisableCallerID"/> changed.</summary>
void ReEnableCallerID();

/// <summary>Creates <see cref="LinkUpBlocks"/> if it doesn't exist.</summary>
void InitLinkUpBlocks();

/// <summary>Frees every block of <see cref="LinkUpBlocks"/> and the store.</summary>
void DestroyLinkUpBlocks();

/// <summary>DirectPlay EnumPlayers callback: forwards to SessionManager::NewPlayerEnumeration.</summary>
int EnumPlayersCallback(uint32_t playerID, uint32_t playerType, const DPNAME* name, uint32_t flags, void* context);

/// <summary>DirectPlay EnumGroups callback: forwards to SessionManager::NewGroupEnumeration.</summary>
int EnumGroupsCallback(uint32_t groupID, uint32_t groupType, const DPNAME* name, uint32_t flags, void* context);

/// <summary>The receive thread's entry: runs SessionManager::ReceiveThread on <paramref name="sessionManager"/>.</summary>
uint32_t SessionManagerReceiveThread(void* sessionManager);

/// <summary>DirectPlay EnumConnections callback: forwards to SessionManager::AddConnection.</summary>
int EnumConnectionsCallback(const _GUID* serviceProvider, void* connection, uint32_t connectionSize, const DPNAME* name,
                            uint32_t flags, void* context);

/// <summary>DirectPlay address-enumeration callback: collects modem names into the SessionManager.</summary>
int ModemCallback(const _GUID& dataType, uint32_t dataSize, const void* data, void* context);

/// <summary>DirectPlay EnumSessions callback: forwards to SessionManager::AddSession (stops on timeout).</summary>
int EnumSessionsCallback(const DPSESSIONDESC2* desc, uint32_t* timeout, uint32_t flags, void* context);

/// <summary>Removes entry <paramref name="index"/> of an array of <paramref name="count"/> by shifting the rest down.</summary>
void ShiftPointerArray(int32_t* array, int index, int count);

/// <summary>qsort comparator of two player numbers by their players' average latency.</summary>
int CompareLatencies(const int32_t* playerNumber1, const int32_t* playerNumber2);

/// <summary>qsort comparator of two longs.</summary>
int CompareLongs(const int32_t* value1, const int32_t* value2);

/// <summary>Writes the session manager's statistics to the log.</summary>
void OutputSessionManagerStats();
