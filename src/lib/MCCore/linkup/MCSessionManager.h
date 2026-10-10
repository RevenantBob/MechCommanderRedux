#pragma once

// Original source: mcx\linkup\sessionmanager.cpp (and the inline getters of sessionmanager.h): the game's DirectPlay
// session layer. It enumerates connections (TCP/IP, IPX, modem, serial, lobby), hosts, lists and joins sessions,
// tracks the players and groups, runs a receive pass that sorts incoming messages into queues, and on top of
// DirectPlay implements guaranteed, ordered delivery (per-player send counters, verify lists, resends), latency
// measurement, server switching and file transfer.
//
// Port: DirectPlay is the port's stand-in, MCDirectPlay (platform/MCDirectPlay.h), over TCP/UDP. It offers the TCP/IP
// and IPX connections only, so the modem, serial and lobby paths find nothing (as on a machine without them). The
// "receive thread" was never a thread: ProcessMessages calls ReceiveThread each frame. DPIDs are uint32_t.

#include "linkup/MCFidpNetworkProtocol.h"
#include "linkup/MCLinkupMessages.h"

class MCDirectPlay;
class MCFidpGroup;
class MCFidpMessage;
class MCFidpPlayer;
class MCFidpSession;
class MCFileTransferInfo;

/// <summary>
/// Whether a DirectPlay lobby started the game (the session's start-up sets it from
/// <see cref="MCSessionManager::WasLaunchedFromLobby"/>). Lobby games take their player count from the session and
/// skip the pre-numbering message queues.
/// </summary>
extern bool LaunchedFromLobby;

/// <summary>
/// The game's DirectPlay session. The MultiPlayer object owns it; the ABL debugger's console can make one of its own.
/// </summary>
class MCSessionManager
{
public:
    /// <summary>
    /// Makes the session manager for application <paramref name="appGUID"/>: creates the DirectPlay object and
    /// enumerates the connections.
    /// </summary>
    explicit MCSessionManager(const _GUID& appGUID);
    /// <summary>Leaves the session and releases the DirectPlay object.</summary>
    ~MCSessionManager();

    MCSessionManager(const MCSessionManager&) = delete;
    MCSessionManager& operator=(const MCSessionManager&) = delete;

    /// <summary>Performance-counter ticks per millisecond.</summary>
    static uint32_t TicksPerMs();

    /// <summary>Leaves the session and releases the DirectPlay object.</summary>
    void DestroyDirectPlayInterface();

    // ---- connections, sessions, players, groups ----

    /// <summary>The group with id <paramref name="groupID"/>, or null.</summary>
    MCFidpGroup* GetGroup(uint32_t groupID);
    /// <summary>The player with id <paramref name="playerID"/>, or null.</summary>
    MCFidpPlayer* GetPlayer(uint32_t playerID);
    /// <summary>The player with number <paramref name="playerNumber"/>, or null.</summary>
    MCFidpPlayer* GetPlayerNumber(int32_t playerNumber);
    /// <summary>Sets the directory transferred files go in (cut to 511 characters).</summary>
    void SetHomeDirectory(std::string_view directory);
    /// <summary>Hosts <paramref name="session"/> and creates this machine's player <paramref name="playerName"/>.</summary>
    /// <returns>0; -1 when DirectPlay refused the session; -2 when the player could not be created.</returns>
    int32_t HostSession(const MCFidpSession& session, std::string_view playerName);
    /// <summary>The enumerated session whose instance GUID is <paramref name="sessionGUID"/>, or null.</summary>
    MCFidpSession* FindMatchingSession(const _GUID& sessionGUID);
    /// <summary>Joins the enumerated session <paramref name="sessionGUID"/> as <paramref name="playerName"/>.</summary>
    /// <returns>0, -1 when no such session, or the DirectPlay error.</returns>
    int32_t JoinSession(const _GUID& sessionGUID, std::string_view playerName);
    /// <summary>Whether every player reported its latency, so a new server can be picked.</summary>
    bool ReadyToChooseServer() const;
    /// <summary>The host closes the session to new players.</summary>
    /// <returns>Whether it was locked (false when not hosting).</returns>
    bool LockSession();
    /// <summary>Closes this machine's player and the session, and recreates the DirectPlay object.</summary>
    /// <returns>Whether a session was left.</returns>
    bool LeaveSession();
    /// <summary>
    /// Creates group <paramref name="groupName"/> with <paramref name="data"/> attached; DirectPlay puts its id in
    /// <paramref name="groupID"/>.
    /// </summary>
    void CreateGroup(uint32_t& groupID, std::string_view groupName, std::span<const uint8_t> data, uint32_t flags);
    /// <summary>Adds player <paramref name="playerID"/> (0 = this machine's player) to group <paramref name="groupID"/>.</summary>
    /// <returns>Whether the group exists (and the player was found or already in it).</returns>
    bool AddPlayerToGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Tells <paramref name="player"/> it is out of the game (the host drops a player).</summary>
    /// <returns>0, or -1 when the player has no number (or a removal is under way).</returns>
    int RemovePlayerFromGame(MCFidpPlayer* player);
    /// <summary>Removes player <paramref name="playerID"/> (0 = this machine's player) from group <paramref name="groupID"/>.</summary>
    /// <returns>Whether it was removed.</returns>
    bool RemovePlayerFromGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>Initializes DirectPlay with the connection of kind <paramref name="type"/>.</summary>
    /// <returns>0 or an error.</returns>
    int32_t SetCurrentConnection(MCNetProtocol type);
    /// <summary>Initializes an IPX connection.</summary>
    void ConnectIpx();
    /// <summary>Initializes a TCP/IP connection to <paramref name="ipAddress"/> ("" = search the LAN).</summary>
    void ConnectTcp(std::string_view ipAddress);
    /// <summary>Initializes a modem connection dialing <paramref name="phoneNumber"/> on <paramref name="modemName"/>.</summary>
    int32_t ConnectModem(std::string_view phoneNumber, std::string_view modemName);
    /// <summary>Initializes a serial connection with the given port settings.</summary>
    int32_t ConnectComPort(uint32_t port, uint32_t baudRate, uint32_t stopBits, uint32_t parity, uint32_t flowControl);
    /// <summary>Enumerates the modems.</summary>
    /// <returns>Whether the modem connection could be looked at.</returns>
    bool FindModems();
    /// <summary>Modem <paramref name="index"/>'s name, or null.</summary>
    const char* GetModemName(int32_t index) const;
    /// <summary>Whether the game was started by a DirectPlay lobby.</summary>
    bool WasLaunchedFromLobby();
    /// <summary>
    /// Connects the session the lobby launched the game for, calling <paramref name="showStatus"/> while waiting and
    /// <paramref name="hideStatus"/> after.
    /// </summary>
    /// <returns>0 when connected; 0x8877042e (DPERR_NOTLOBBIED) when the game was not lobby-launched.</returns>
    uint32_t SetupLobbyConnection(const std::function<void()>& showStatus, const std::function<void()>& hideStatus);
    /// <summary>The available connections.</summary>
    const std::vector<std::unique_ptr<MCFidpNetworkProtocol>>& GetConnections() const { return Connections; }
    /// <summary>Re-enumerates the sessions of this game on the current connection.</summary>
    /// <returns>The session list, or null without DirectPlay.</returns>
    const std::vector<std::unique_ptr<MCFidpSession>>* GetSessions();
    /// <summary>The players of the current session (enumerated first when this machine has no player yet).</summary>
    const std::vector<std::unique_ptr<MCFidpPlayer>>& GetPlayers(MCFidpSession* session);
    /// <summary>The groups of the current session (enumerated first).</summary>
    const std::vector<std::unique_ptr<MCFidpGroup>>& GetGroups(MCFidpSession* session);
    /// <summary>Dials the modem connection.</summary>
    int32_t Dial();
    /// <summary>Hangs up a dial in progress.</summary>
    void CancelDialing();
    /// <summary>Whether TCP/IP is installed (checked once).</summary>
    bool IsTcpAvailable();
    /// <summary>Whether IPX is installed (checked once).</summary>
    bool IsIpxAvailable();
    /// <summary>Whether a modem is installed (looked for on every call, as the original did).</summary>
    bool IsModemAvailable();

    // ---- the per-frame pump ----

    /// <summary>Sends each player the numbers of the guaranteed messages received from it (its verify message).</summary>
    void SendVerifies();
    /// <summary>
    /// The per-frame pump: pings when due, receives, resends unverified messages and processes the system and
    /// application queues.
    /// </summary>
    void ProcessMessages();
    /// <summary>Handles the queued DirectPlay system messages.</summary>
    /// <returns>How many were handled, -1 when the session was lost.</returns>
    int ProcessSystemMessages();
    /// <summary>Handles the queued application messages, then each player's in-order guaranteed ones.</summary>
    /// <returns>How many were queued, -1 when this machine was removed from the game.</returns>
    int ProcessApplicationMessages();
    /// <summary>The receive pass: sorts every received message into its queue, then sends the verifies.</summary>
    int ReceiveThread();
    /// <summary>Resends every player's overdue unverified messages.</summary>
    void UpdateGuaranteedMessages();
    /// <summary>
    /// Sends the next piece of the current outgoing file and drops the finished transfers. Original behaviour: the list
    /// cursor is never advanced, so with several transfers the first gets every piece until it is done.
    /// </summary>
    /// <remarks>Nothing in MCX.EXE calls it (OB-107), so BroadcastFile only ever sends the announcement.</remarks>
    void UpdateFileTransfers();

    // ---- sending ----

    /// <summary>The host starts the game: tells everyone.</summary>
    void StartGame();
    /// <summary>Sends the ping message (the host: with the other players by latency).</summary>
    void SendPing();
    /// <summary>
    /// Sends a guaranteed message to every member of group <paramref name="groupID"/> (0 = everyone), or queues it
    /// until this machine has a player number.
    /// </summary>
    void SendMessageToGroup(uint32_t groupID, MCFIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>The players of group <paramref name="groupID"/> (null for an id the session doesn't know).</summary>
    std::vector<MCFidpPlayer*> GetPlayerListForGroup(uint32_t groupID);
    /// <summary>The mean of the other players' average latencies (0 alone).</summary>
    uint32_t TallyLatencies();
    /// <summary>Reports this machine's average latency to the server.</summary>
    void SendLatencyInfo();
    /// <summary>The host hands the server role to the player with the most memory.</summary>
    void SwitchServers();
    /// <summary>
    /// Sends a guaranteed message to player <paramref name="playerID"/>: numbers it (when <paramref name="firstSend"/>),
    /// sends it and queues it for verification.
    /// </summary>
    void SendMessageToPlayerGuaranteed(uint32_t playerID, MCFIGuaranteedMessageHeader* header, uint32_t size,
                                       bool firstSend);
    /// <summary>Sends a guaranteed message to the server.</summary>
    void SendMessageToServerGuaranteed(MCFIGuaranteedMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to everyone.</summary>
    void BroadcastMessage(MCFIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to the server (queued until this machine has a player number).</summary>
    void SendMessageToServer(MCFIMessageHeader* header, uint32_t size);
    /// <summary>Sends a plain message to <paramref name="toID"/> through DirectPlay (at most 0x200 bytes).</summary>
    /// <remarks>The original's <c>SendMessage</c> (renamed <c>SendMessageA</c> by windows.h).</remarks>
    int32_t SendPlainMessage(uint32_t toID, const MCFIMessageHeader* header, uint32_t size);
    /// <summary>
    /// Starts sending <paramref name="fileName"/> in <paramref name="directory"/> to everyone;
    /// <paramref name="callback"/> is called when it is done.
    /// </summary>
    /// <returns>The transfer's id.</returns>
    int BroadcastFile(std::string_view fileName, std::optional<std::string_view> directory,
                      std::function<void(const std::string& fileName)> callback);
    /// <summary>Sends <paramref name="msg"/> the way its header says (group, guaranteed or plain).</summary>
    void SendMessageFromInfo(MCFidpMessage& msg);
    /// <summary>The latency statistics line (null outside a session).</summary>
    std::optional<std::string> GetStats();

    /// <summary>The connections DirectPlay offers.</summary>
    std::vector<std::unique_ptr<MCFidpNetworkProtocol>> Connections;
    /// <summary>The enumerated (and hosted) sessions.</summary>
    std::vector<std::unique_ptr<MCFidpSession>> Sessions;
    /// <summary>The players of the current session.</summary>
    std::vector<std::unique_ptr<MCFidpPlayer>> Players;
    /// <summary>The groups of the current session.</summary>
    std::vector<std::unique_ptr<MCFidpGroup>> Groups;
    /// <summary>DirectPlay system messages waiting for the game thread.</summary>
    std::deque<MCFidpMessage*> SystemMessages;
    /// <summary>Application messages waiting for the game thread.</summary>
    std::deque<MCFidpMessage*> ApplicationMessages;
    /// <summary>
    /// Guaranteed messages received before this machine had a player number (it can't read their send counts yet);
    /// handled once the server's player numbers arrive.
    /// </summary>
    std::deque<MCFidpMessage*> PreIDReceivedMessages;
    /// <summary>Group messages sent before this machine had a player number.</summary>
    std::deque<MCFidpMessage*> PreIDGroupMessages;
    /// <summary>Server messages sent before this machine had a player number.</summary>
    std::deque<MCFidpMessage*> PreIDServerMessages;
    /// <summary>Files being sent.</summary>
    std::vector<std::unique_ptr<MCFileTransferInfo>> OutgoingFiles;
    /// <summary>Files being received.</summary>
    std::vector<std::unique_ptr<MCFileTransferInfo>> IncomingFiles;
    /// <summary>The session hosted or joined (one of <see cref="Sessions"/>), or null.</summary>
    MCFidpSession* CurrentSession = nullptr;
    /// <summary>The kind of the current connection.</summary>
    MCNetProtocol CurrentConnection = MCNetProtocol::None;
    /// <summary>This machine's player's DPID (0 when none).</summary>
    uint32_t MyPlayerID = 0;
    /// <summary>The server's DPID.</summary>
    uint32_t ServerID = 0;
    /// <summary>This machine's player (one of <see cref="Players"/>).</summary>
    MCFidpPlayer* MyPlayer = nullptr;
    /// <summary>Whether this machine is the server (host).</summary>
    bool IsHost = false;
    /// <summary>Set once the game started (the session is locked, or the host said so).</summary>
    bool GameStarted = false;
    /// <summary>Whether this machine's player has a player number (set at once on the host).</summary>
    bool HasPlayerNumber = false;
    /// <summary>Guards the player list and the outgoing queues.</summary>
    std::recursive_mutex CriticalSection;
    /// <summary>The game's handler of application messages (MultiPlayerApplicationCallback).</summary>
    std::function<void(MCFidpMessage& msg)> ApplicationCallback;
    /// <summary>The game's handler of system messages (player created/destroyed, added to group, session lost).</summary>
    std::function<void(MCFidpMessage& msg)> SystemCallback;
    /// <summary>Called when a received file is complete (MultiPlayerFileReceivedCallback).</summary>
    std::function<void(const std::string& fileName)> FileReceivedCallback;
    /// <summary>The DirectPlay object (the port's stand-in).</summary>
    std::unique_ptr<MCDirectPlay> DirectPlay;
    /// <summary>The <see cref="MCNetProtocol"/> bits of the connections found.</summary>
    uint32_t AvailableProtocols = 0;
    /// <summary>Player numbers 0-5 sorted by latency (the server's ping; reset to 0-5 by LeaveSession).</summary>
    std::array<int32_t, MaxLinkupPlayers> PlayersByLatency{0, 1, 2, 3, 4, 5};

private:
    /// <summary>Creates the DirectPlay object if needed and enumerates the connections.</summary>
    void CreateDirectPlayInterface();
    /// <summary>Clears <paramref name="msg"/> and returns it to the free queue.</summary>
    void AddMessageToEmptyQueue(MCFidpMessage* msg);
    /// <summary>Takes a message from the free queue (a new one when the pool is used up).</summary>
    MCFidpMessage* GetMessageFromEmptyQueue();
    /// <summary>Takes a message from the free queue and fills it with <paramref name="size"/> bytes of <paramref name="header"/>.</summary>
    MCFidpMessage* CopyToFreeMessage(const MCFIMessageHeader* header, uint32_t size, uint32_t toID);
    /// <summary>Rebuilds <see cref="Connections"/> from DirectPlay's EnumConnections.</summary>
    void EnumerateConnections();
    /// <summary>The kind of a service provider, adding it to the available set.</summary>
    MCNetProtocol ConnectionType(const _GUID& guid);
    /// <summary>EnumConnections callback: adds a connection.</summary>
    void AddConnection(const _GUID& serviceProvider, std::span<const uint8_t> connection, const DPNAME& name);
    /// <summary>EnumSessions callback: adds a session not already current.</summary>
    void AddSession(const DPSESSIONDESC2& desc);
    /// <summary>EnumPlayers callback: adds player <paramref name="playerID"/> and numbers it when needed.</summary>
    void NewPlayerEnumeration(uint32_t playerID, const DPNAME& name, uint32_t flags);
    /// <summary>EnumGroups callback: adds group <paramref name="groupID"/>.</summary>
    void NewGroupEnumeration(uint32_t groupID, const DPNAME& name, uint32_t flags);
    /// <summary>
    /// Creates this machine's player named <paramref name="playerName"/> in the current session, adds 900 messages to
    /// the pool and enumerates the players and groups.
    /// </summary>
    /// <returns>0 or the DirectPlay error.</returns>
    int32_t CreatePlayer(std::string_view playerName);
    /// <summary>Sends this machine's physical memory to the server (the server records its own).</summary>
    void SendSystemInformation();
    /// <summary>Records the physical memory reported by player <paramref name="fromID"/>.</summary>
    void ProcessSystemInfoMessage(const MCFISystemInfoMessage& msg, uint32_t fromID);
    /// <summary>Sets group <paramref name="groupID"/>'s data in DirectPlay.</summary>
    void SetGroupData(uint32_t groupID, std::span<const uint8_t> data, uint32_t flags);
    /// <summary>Initializes DirectPlay with a compound address.</summary>
    int32_t InitializeConnection(std::span<const DPCOMPOUNDADDRESSELEMENT> elements);
    /// <summary>Deletes every enumerated session except the current one.</summary>
    void ClearSessionList();
    /// <summary>Rebuilds the player list from DirectPlay.</summary>
    void EnumeratePlayers(MCFidpSession* session);
    /// <summary>Hands each player's in-order guaranteed messages to the application handler.</summary>
    void ProcessGuaranteedMessages();
    /// <summary>Receive pass: routes one application message (verify, guaranteed, plain).</summary>
    void RTProcessApplicationMessage(MCFidpMessage* msg);
    /// <summary>Receive pass: the server's player numbers arrived (this machine gets its number).</summary>
    void RTProcessPlayerNumbers(MCFidpMessage* msg, MCFidpPlayer* sender);
    /// <summary>Receive pass: stores a guaranteed message from <paramref name="player"/> in order.</summary>
    void RTHandleNewGuaranteedMessage(MCFidpMessage* msg, MCFidpPlayer* player);
    /// <summary>Receive pass: the DPID of player number <paramref name="playerNumber"/> (0 when none).</summary>
    uint32_t RTGetIDFromPlayerNumber(int playerNumber);
    /// <summary>Adds an entry for <paramref name="sendCount"/> to player number <paramref name="playerNumber"/>'s verify.</summary>
    void AddVerifyEntry(int32_t playerNumber, uint8_t sendCount);
    /// <summary>Resends <paramref name="player"/>'s unverified messages whose delay ran out at <paramref name="now"/>.</summary>
    void UpdatePlayerGuaranteedMessages(MCFidpPlayer& player, uint32_t now);
    /// <summary>Stamps each numbered player's next send count into <paramref name="header"/> for a group send.</summary>
    void SetupMessageSendCounts(MCFIGuaranteedMessageHeader* header, std::span<MCFidpPlayer* const> players);
    /// <summary>Tells the players who is in each group (<paramref name="playerID"/>: only that player; 0 everyone).</summary>
    void SendPlayersInGroupMessages(uint32_t playerID);
    /// <summary>Sends the guaranteed messages queued before this machine had a player number.</summary>
    void SendPreIDGuaranteedMessages();
    /// <summary>Records the latency player <paramref name="fromID"/> reported.</summary>
    void ProcessLatencyMessage(const MCFIValueMessage& msg, uint32_t fromID);
    /// <summary>The server gives <paramref name="player"/> the lowest free player number.</summary>
    void GivePlayerAnID(MCFidpPlayer& player);
    /// <summary>A DirectPlay create-player/group system message: adds it (numbered by the server).</summary>
    void AddPlayerOrGroup(uint32_t playerType, uint32_t id, const DPNAME& name, uint32_t flags);
    /// <summary>A player or group is leaving (system message before the deletion).</summary>
    void PlayerOrGroupLeaving(uint32_t playerType, uint32_t id);
    /// <summary>A DirectPlay destroy-player/group system message: removes and deletes it.</summary>
    void DeletePlayerOrGroup(uint32_t playerType, uint32_t id);
    /// <summary>Adds player <paramref name="playerID"/> to group <paramref name="groupID"/> as a message said.</summary>
    void PlayerJoinedGroup(uint32_t groupID, uint32_t playerID);
    /// <summary>A system message the receive pass handles before queueing it.</summary>
    void HandlePreSystemMessage(MCFidpMessage& msg);
    /// <summary>A queued system message, after the game's handler saw it.</summary>
    void HandlePostSystemMessage(MCFidpMessage& msg);
    /// <summary>
    /// An application message on the game thread: the linkup layer's own types (player numbers, file transfer, system
    /// info, latency, ping) or the game's (<see cref="ApplicationCallback"/>).
    /// </summary>
    void HandleApplicationMessage(MCFidpMessage* msg);
    /// <summary>The server's numbering in a lobby game.</summary>
    void ApplyLobbyPlayerNumbers(const MCFIPlayerNumbersMessage& numbers);
    /// <summary>A file piece arrived.</summary>
    void HandleFileData(const MCFidpMessage& msg);
    /// <summary>A ping from the server: its latency order.</summary>
    void HandlePingUpdate(const MCFidpMessage& msg);

    /// <summary>The game's DirectPlay application id.</summary>
    _GUID _AppGuid{};
    /// <summary>Every message the session made: the queues and lists hold them.</summary>
    std::vector<std::unique_ptr<MCFidpMessage>> _MessagePool;
    /// <summary>Free message buffers (guarded by <see cref="_EmptyQueueLock"/>).</summary>
    std::deque<MCFidpMessage*> _EmptyMessages;
    /// <summary>Guards the free-message queue.</summary>
    std::recursive_mutex _EmptyQueueLock;
    /// <summary>The "new server" message, kept ready to announce the next server.</summary>
    MCFIValueMessage _ServerMessage{};
    /// <summary>Per player number: the verify message being built.</summary>
    std::array<MCFIVerifyMessage, MaxLinkupPlayers> _VerifyMessages{};
    /// <summary>
    /// DPIDs of players the server numbered before this machine knew them: slot n holds the id given player number n.
    /// </summary>
    std::array<uint32_t, MaxLinkupPlayers> _NewPlayerNumbers{};
    /// <summary>Set once the host locked the session (no new players).</summary>
    bool _SessionLocked = false;
    /// <summary>RemovePlayerFromGame's re-entry guard.</summary>
    bool _RemovingPlayer = false;
    /// <summary>Where transferred files go.</summary>
    std::string _HomeDirectory;
    /// <summary>The modems found.</summary>
    std::vector<std::string> _ModemNames;
    /// <summary>Whether <see cref="IsIpxAvailable"/> / <see cref="IsTcpAvailable"/> looked already, and what they found.</summary>
    bool _IpxChecked = false;
    bool _IpxAvailable = false;
    bool _TcpChecked = false;
    bool _TcpAvailable = false;
    /// <summary>Set when every latency report is in (<see cref="ReadyToChooseServer"/>).</summary>
    bool _LatencyReportsIn = false;
    /// <summary>The id of the next file transfer (0-255).</summary>
    int32_t _NextFileID = 0;
    /// <summary>Performance-counter time of the next ping.</summary>
    uint32_t _NextPingTime = 0;
    /// <summary>Milliseconds between pings.</summary>
    uint32_t _PingInterval = 2000;
};
