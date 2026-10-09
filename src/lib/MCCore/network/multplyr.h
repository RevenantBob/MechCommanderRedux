#pragma once

// Original source: mcx\network\multplyr.cpp: the game's side of multiplayer. MultiPlayer owns the SessionManager
// (linkup), keeps the rosters of movers and turrets that are synchronised, queues the "chunks" (packed 32-bit
// records of moves, weapon fire, hits, status and world events) and sends them at fixed frequencies; the handleApp*
// functions apply the messages the other machines send.
//
// Port: SessionManager runs over platform/MCDirectPlay (TCP/UDP) instead of DirectPlay. MPlayer stays null in a
// single-player game, as in the original. The chunk classes that
// single-player code also uses live with their objects: MoveChunk (ai/MCMoveChunk.h), StatusChunk
// (object/MCStatusChunk.h), WeaponFireChunk and WeaponHitChunk (object/MCWeaponFireChunk.h, object/MCWeaponHitChunk.h),
// ArtilleryChunk (object/MCArtilleryChunk.h). WorldStateChunk is here.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"
#include "object/MCWeaponHitChunk.h"
#include "object/MCWeaponShotInfo.h"

class MCFidpMessage;
class MCFitIniFile;
class MCGameObject;
class MCMover;
class MCMoverGroup;
class MCSessionManager;
class MCTacticalOrder;
class MCTurret;
class MCWeaponHitChunk;
class MCVector3D;
struct MCWeaponShotInfo;

class MCMultiPlayer;

/// <summary>The game's DirectPlay application GUID.</summary>
inline constexpr _GUID MultiPlayerAppGuid{0x09608800, 0x4815, 0x11d2, {0x92, 0xd2, 0x00, 0x60, 0x97, 0x3c, 0xfb, 0x2c}};

/// <summary>The multiplayer game, or null in a single-player game.</summary>
extern MCMultiPlayer* MPlayer;
/// <summary>Nonzero while a multiplayer game is set up (setupLobbyGame).</summary>
extern int IsMPlayerGame;
/// <summary>Consecutive "session lost" system messages during a mission; over 10 ends the game.</summary>
extern int32_t BadSessionCounter;
/// <summary>The number of players in a LAN game (1 outside multiplayer).</summary>
extern int32_t NumLanPlayers;
/// <summary>
/// The update periods in seconds, as initUpdateFrequencies settled them: mover updates, turret updates, world-state
/// (and weapon) updates.
/// </summary>
extern float MultiplayBroadcastFrequencies[3];
/// <summary>The connection type (FIDPProtocolType) of the last game, kept for the next session screen.</summary>
extern uint32_t LastConnectionType;
/// <summary>The multiplayer game speed factor ("WarpFactor" of the Multiplayer block), used by mech and vehicle moves.</summary>
extern float WarpFactor;
/// <summary>Per WorldStateChunk type: how many were queued (statistics).</summary>
extern int32_t WorldStateChunkTally[10];
/// <summary>Nonzero once the mission is over and the game is heading to the results screen.</summary>
extern int EventsToMissionResultsScreen;

/// <summary>The kinds of <see cref="MCWorldStateChunk"/> (the low 4 bits of its packed word).</summary>
/// <remarks>The names are the port's; the values are the original's.</remarks>
enum MCWorldStateChunkType
{
    WSCHUNK_MINE = 0,
    WSCHUNK_TERRAIN_FIRE = 1,
    /// <summary>2-7: an artillery strike called by commander (type - 2).</summary>
    WSCHUNK_ARTILLERY = 2,
    WSCHUNK_MISSION_SCRIPT_MESSAGE = 8,
    WSCHUNK_PILOT_KILL_STAT = 9
};

#pragma pack(push, 1)

/// <summary>
/// A world event the server tells the clients about, packed into one 32-bit word: a mine laid or set off, a terrain
/// object set on fire, an artillery strike, a mission-script message, a pilot's kill.
/// </summary>
/// <remarks>
/// Original source: <c>network\multplyr.cpp</c>, 0x1e bytes, byte-packed (the fields sit at odd offsets). Only
/// <see cref="Data"/> goes over the network. Packing, by type (bit 0 is the lowest; all start with the 4-bit type):
/// mine: tileCol 4-13, tileRow 14-23, param2 (mine state) 24-26, param1 (team) 27;
/// terrain fire: item 4-6, vertexNum 7-15, blockNum 16-23, param1 (seconds) 24-31;
/// artillery: tileCol 4-13, tileRow 14-23, param2 + 1 (seconds) 24-28, param1 (strike type) 29-31;
/// script message: param1 (message) 4-11, param2 + 32000 (value) 12-27;
/// pilot kill: param2 (kill kind) 4-6, param1 (mover roster index) 7-11.
/// </remarks>
class MCWorldStateChunk
{
public:
    /// <summary>
    /// A mine at map cell (<paramref name="tileRow"/>, <paramref name="tileCol"/>) of team
    /// <paramref name="teamId"/> (0-2; 1 is the Clan layout): <paramref name="mineState"/> 0-3, where 3 (exploded)
    /// adds <paramref name="explosionType"/> 0-2.
    /// </summary>
    void BuildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState, int32_t explosionType);

    /// <summary>
    /// Terrain object <paramref name="object"/> (its part id split into block, vertex and item) set on fire for
    /// <paramref name="seconds"/> (0-255).
    /// </summary>
    void BuildTerrainFire(MCGameObject* object, int32_t seconds);

    /// <summary>
    /// An artillery strike of <paramref name="strikeType"/> (0-7) by commander <paramref name="commanderId"/> (0-5)
    /// at <paramref name="location"/>, landing in <paramref name="seconds"/> (-1-30).
    /// </summary>
    void BuildArtillery(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds);

    /// <summary>Mission-script message <paramref name="message"/> (0-255) with <paramref name="value"/> (±32000).</summary>
    void BuildMissionScriptMessage(int32_t message, int32_t value);

    /// <summary>A kill of kind <paramref name="killType"/> (0-7) by the mover at <paramref name="moverIndex"/> of the roster.</summary>
    void BuildPilotKillStat(int32_t moverIndex, int32_t killType);

    /// <summary>Packs the fields into <see cref="Data"/>.</summary>
    void Pack();

    /// <summary>Unpacks <see cref="Data"/> into the fields (fatal on a type it doesn't list).</summary>
    void Unpack();

    /// <summary>Whether every unpacked field matches <paramref name="chunk"/>'s.</summary>
    int EqualTo(MCWorldStateChunk* chunk);

    /// <summary>The <see cref="MCWorldStateChunkType"/>.</summary>
    int8_t Type = 0;
    int16_t TileRow = 0;
    int16_t TileCol = 0;
    /// <summary>Terrain fire: the object's part id (0x1000 + blockNum * 0xc80 + vertexNum * 8 + item).</summary>
    int32_t ObjectWid = 0;
    int32_t BlockNum = 0;
    int32_t VertexNum = 0;
    int8_t Item = 0;
    /// <summary>The first value (see the type).</summary>
    int32_t Param1 = 0;
    /// <summary>The second value (see the type).</summary>
    int32_t Param2 = 0;
    /// <summary>The packed word sent over the network.</summary>
    uint32_t Data = 0;
};

static_assert(sizeof(MCWorldStateChunk) == 0x1e);

/// <summary>
/// The game's message types (bits 0-9 of the linkup header), handled by MultiPlayerApplicationCallback. 6, 9 and 12
/// are the linkup layer's, passed on.
/// </summary>
/// <remarks>The names are the port's (after the handlers); the values are the original's.</remarks>
enum MCMultiPlayerMessageType
{
    MPMSG_NEW_SERVER = 6,
    MPMSG_PLAYER_REMOVED = 9,
    MPMSG_LATENCY = 12,
    MPMSG_CHAT = 13,
    MPMSG_PLAYER_CHECK_IN = 15,
    MPMSG_PLAYER_SETUP = 16,
    MPMSG_PLAYER_CHECK_IN_RECEIPT = 17,
    MPMSG_START_PLANNING = 19,
    MPMSG_START_SCENARIO = 20,
    MPMSG_END_SCENARIO = 21,
    MPMSG_PLAYER_ORDER = 22,
    MPMSG_PLAYER_MOVER_GROUP = 23,
    MPMSG_PLAYER_ARTILLERY = 24,
    MPMSG_MOVER_UPDATE = 25,
    MPMSG_TURRET_UPDATE = 26,
    MPMSG_MOVER_WEAPON_FIRE_UPDATE = 27,
    MPMSG_TURRET_WEAPON_FIRE_UPDATE = 28,
    MPMSG_MOVER_CRITICAL_HIT_UPDATE = 29,
    MPMSG_WEAPON_HIT_UPDATE = 30,
    MPMSG_WORLD_STATE_UPDATE = 31,
    MPMSG_DEPLOY_FORCE = 32,
    MPMSG_REMOVE_FORCE = 33,
    MPMSG_PLAYER_UPDATE = 34,
    MPMSG_PREPARE_SCENARIO = 35,
    MPMSG_READY_FOR_BATTLE = 36,
    MPMSG_FILE_INQUIRY = 37,
    MPMSG_FILE_REPORT = 38,
    MPMSG_LOAD_MISSION = 39,
    MPMSG_START = 40,
    MPMSG_JOIN_TEAM = 41,
    MPMSG_SWITCH_SCREEN = 42,
    MPMSG_RP_UPDATE = 43,
    MPMSG_TECHBASE_CHANGE = 44,
    MPMSG_SESSION_CHECK_IN = 45
};

// The fixed layouts of the game's messages, as the send functions build them in MultiPlayer::msgBuffer. The struct
// names are the port's (the original wrote through raw pointers). The update messages (mover, turret, weapon fire,
// hits, world state) are variable arrays of packed chunks and are built in place.

/// <summary>Chat (guaranteed): a flag and the zero-terminated text; sent as strlen(text) + 10 bytes.</summary>
struct MCMPChatMessage : public MCFIGuaranteedMessageHeader
{
    /// <summary>1 when sent to everyone (receiver 0).</summary>
    uint8_t ToAll = 0;
    char Text[1]{};
};

static_assert(sizeof(MCMPChatMessage) == 10);

/// <summary>Player check-in and ready-for-battle (guaranteed): the sender's check-in id and home team.</summary>
struct MCMPPlayerCheckInMessage : public MCFIGuaranteedMessageHeader
{
    int8_t CheckInId = 0;
    int8_t HomeTeam = 0;
};

static_assert(sizeof(MCMPPlayerCheckInMessage) == 10);

/// <summary>Player setup (guaranteed): the server's group ids.</summary>
struct MCMPPlayerSetupMessage : public MCFIGuaranteedMessageHeader
{
    uint32_t AllPlayerGroupID = 0;
    uint32_t ClanGroupID = 0;
    uint32_t InnerSphereGroupID = 0;
};

static_assert(sizeof(MCMPPlayerSetupMessage) == 0x14);

/// <summary>Check-in receipt (guaranteed, to the server): the check-in id; also end scenario: the result.</summary>
struct MCMPLongMessage : public MCFIGuaranteedMessageHeader
{
    int32_t Value = 0;
};

static_assert(sizeof(MCMPLongMessage) == 0xc);

/// <summary>Player order (guaranteed, to the server): a tactical order packed into two words.</summary>
struct MCMPPlayerOrderMessage : public MCFIGuaranteedMessageHeader
{
    int8_t CheckInId = 0;
    /// <summary>Bit 0: queued; bit 5: the order came from a group; bits 1-4: 1 &lt;&lt; (group id + 1) per group.</summary>
    uint8_t Flags = 0;
    /// <summary>Two words of the order for move/attack orders (order +0x24, +0x28).</summary>
    uint32_t OrderParam1 = 0;
    uint32_t OrderParam2 = 0;
    /// <summary>TacticalOrder::pack's two words (order +0x130, +0x134).</summary>
    uint32_t PackedOrder[2]{};
};

static_assert(sizeof(MCMPPlayerOrderMessage) == 0x1a);

/// <summary>Player mover group (guaranteed): which movers form a group and its point man.</summary>
struct MCMPPlayerMoverGroupMessage : public MCFIGuaranteedMessageHeader
{
    int8_t CheckInId = 0;
    int8_t GroupId = 0;
    /// <summary>(1 &lt;&lt; local index per mover) &lt;&lt; 4 | the point man's local index.</summary>
    uint16_t Members = 0;
};

static_assert(sizeof(MCMPPlayerMoverGroupMessage) == 0xc);

/// <summary>Player artillery (guaranteed): the target's x and y and the packed ArtilleryChunk.</summary>
struct MCMPPlayerArtilleryMessage : public MCFIGuaranteedMessageHeader
{
    float TargetX = 0;
    float TargetY = 0;
    uint32_t ArtilleryData = 0;
};

static_assert(sizeof(MCMPPlayerArtilleryMessage) == 0x14);

/// <summary>
/// Start scenario (guaranteed, to everyone): per player a value (the caller's array), per mover a flag byte (bit 0:
/// the pilot's +0x38 set) and the mission name; sent as strlen(name) + 0x39 bytes.
/// </summary>
struct MCMPStartScenarioMessage : public MCFIGuaranteedMessageHeader
{
    int32_t PlayerValues[6]{};
    uint8_t MoverFlags[24]{};
    char MissionName[1]{};
};

static_assert(sizeof(MCMPStartScenarioMessage) == 0x39);

/// <summary>
/// Load mission and start (guaranteed, from the session screen): a file name; sent as strlen(name) + 0xd bytes. The
/// word at +0x8 is never written.
/// </summary>
struct MCMPFileNameMessage : public MCFIGuaranteedMessageHeader
{
    int32_t Unused = 0;
    char FileName[1]{};
};

static_assert(sizeof(MCMPFileNameMessage) == 0xd);

/// <summary>Join team (guaranteed, from the host): the player, the team (0 = none) and the slot on it.</summary>
struct MCMPJoinTeamMessage : public MCFIGuaranteedMessageHeader
{
    uint32_t PlayerID = 0;
    int8_t Team = 0;
    int8_t Slot = 0;
};

static_assert(sizeof(MCMPJoinTeamMessage) == 0xe);

/// <summary>
/// Two longs (guaranteed, from the session screen): switch screen (the screen; the second unused), RP update (the
/// points, then the team) and tech base change (the team, then the tech base).
/// </summary>
struct MCMPTwoLongMessage : public MCFIGuaranteedMessageHeader
{
    int32_t Value1 = 0;
    int32_t Value2 = 0;
};

static_assert(sizeof(MCMPTwoLongMessage) == 0x10);

#pragma pack(pop)

/// <summary>A player's DPID and the team it joined (MultiPlayer::playerTeams).</summary>
/// <remarks>8 bytes; the name is the port's.</remarks>
struct MCMPPlayerTeam
{
    uint32_t PlayerID = 0;
    int32_t Team = 0;
};

/// <summary>
/// The multiplayer game: the session, the players' check-in and teams, the synchronised rosters, the queues of
/// outgoing chunks and the update timers.
/// </summary>
/// <remarks>
/// Original source: <c>network\multplyr.cpp</c>, 0x2458 bytes. Its vtable holds only
/// <see cref="init(FitIniFile*)"/>; the destructor is not virtual (the scalar deleting destructor at 0x0075f2e0 is
/// the inline one).
/// </remarks>
class MCMultiPlayer
{
public:
    /// <summary>Inline in the original: calls <see cref="init()"/>.</summary>
    MCMultiPlayer() { Init(); }

    /// <summary>Inline in the original: calls <see cref="Destroy"/>.</summary>
    ~MCMultiPlayer() { Destroy(); }

    MCMultiPlayer(const MCMultiPlayer&) = delete;
    MCMultiPlayer& operator=(const MCMultiPlayer&) = delete;

    /// <summary>Clears the session pointer and message buffer, sets the chat handler and the startup parameters.</summary>
    void Init();

    /// <summary>
    /// Gets or creates the SessionManager, points its home directory at the working directory, installs the
    /// message callbacks and allocates the 0x1400-byte message buffer.
    /// </summary>
    /// <remarks>
    /// The three arguments are not used (UserInit passes 0x7d000, 0x100, 100); the names are the port's guesses.
    /// </remarks>
    int32_t Init(int32_t heapSize, int32_t maxMessageSize, int32_t maxMessages);

    /// <summary>
    /// Reads the mover, turret and world-state update periods from prefs.cfg's Multiplayer block into
    /// <see cref="MultiplayBroadcastFrequencies"/>; a missing one or one outside 0-5 seconds takes the default for
    /// the connection (0.2/0.5/0.33 on a LAN outside a lobby, else 0.33/1/0.75).
    /// </summary>
    void InitUpdateFrequencies();

    /// <summary>
    /// Reads a scripted game's Multiplayer block (Server, NumPlayers, CheckInId, HomeTeam, frequencies, Protocol,
    /// SessionName, PlayerName) and connects.
    /// </summary>
    /// <returns>0, or SetupLobbyConnection's result.</returns>
    virtual int32_t Init(MCFitIniFile* file);

    /// <summary>The number of players (the session's in a lobby game, else NumLANPlayers).</summary>
    int32_t NumPlayers();

    /// <summary>Connects the lobby's session; the server creates the groups and sends its setup.</summary>
    int32_t SetupLobbyGame();

    /// <summary>Adds a mover this machine controls (at most 12); stores its index in the mover (+0x1d8).</summary>
    void AddToLocalMovers(MCMover* mover);

    /// <summary>Adds a mover to the synchronised roster (at most 24); stores its index in the mover (+0x1dc).</summary>
    void AddToMoverRoster(MCMover* mover);

    /// <summary>Adds a mover to player <paramref name="playerNumber"/>'s roster (first free of 12 slots).</summary>
    void AddToPlayerMoverRoster(int32_t playerNumber, MCMover* mover);

    /// <summary>Adds a turret to the synchronised roster (at most 64); stores its index in the turret (+0x11c).</summary>
    void AddToTurretRoster(MCTurret* turret);

    /// <summary>Packs <paramref name="chunk"/> (checking it unpacks the same) and queues it (at most 1024).</summary>
    /// <returns>The number queued.</returns>
    int32_t AddWorldStateChunk(MCWorldStateChunk* chunk);

    int32_t AddMissionScriptMessageChunk(int32_t message, int32_t value);

    int32_t AddArtilleryChunk(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds);

    int32_t AddMineChunk(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState, int32_t explosionType);

    /// <summary>Queues a terrain fire (only for terrain object types 0x10, 0x15, 0x18, 0x1b).</summary>
    int32_t AddLightOnFireChunk(MCGameObject* object, int32_t seconds);

    int32_t AddPilotKillStat(MCMover* mover, int32_t killType);

    /// <summary>Copies the queued world-state words into <paramref name="chunks"/>.</summary>
    /// <returns>How many.</returns>
    int32_t GrabWorldStateChunks(uint32_t* chunks);

    /// <summary>Packs <paramref name="chunk"/> (checking it unpacks the same) and queues it (at most 1024).</summary>
    int32_t AddWeaponHitChunk(MCWeaponHitChunk* chunk);

    /// <summary>
    /// Builds a weapon-hit chunk for a shot at <paramref name="target"/> and queues it. <paramref name="hitFlag"/>
    /// goes through WeaponHitChunk::build to buildMoverTarget.
    /// </summary>
    int32_t AddWeaponHitChunk(MCGameObject* target, MCWeaponShotInfo* shotInfo, int hitFlag);

    /// <summary>Moves up to <paramref name="maxChunks"/> queued weapon-hit words into <paramref name="chunks"/>.</summary>
    void GrabWeaponHitChunks(uint32_t* chunks, int32_t maxChunks);

    int32_t ConnectIpx();

    int32_t ConnectInternet(char* ipAddress);

    /// <summary>Hosts a session named <paramref name="sessionName"/> for <paramref name="maxPlayers"/>.</summary>
    int32_t CreateSession(char* sessionName, char* playerName, int32_t maxPlayers);

    /// <summary>Joins session <paramref name="sessionName"/> (the first one when null).</summary>
    int32_t JoinSession(char* sessionName, char* playerName);

    /// <summary>Runs the SessionManager's message pump.</summary>
    int32_t ProcessReceiveList();

    /// <summary>Sends a message to the host (guaranteed or not).</summary>
    /// <returns>0; -1 without a session; -2 on the host itself.</returns>
    int32_t SendToHost(MCFIMessageHeader* msg, int32_t size, int guaranteed);

    /// <summary>Sends chat <paramref name="text"/> to <paramref name="toID"/> (0 = everyone).</summary>
    int32_t SendChat(uint32_t toID, char* text);

    int32_t SendPlayerCheckIn();

    /// <summary>The server's setup message to <paramref name="toID"/> (0 = everyone): the group ids.</summary>
    /// <remarks>The last two arguments are not used.</remarks>
    int32_t SendPlayerSetup(uint32_t toID, uint32_t serverID, uint32_t innerSphereGroupID, uint32_t clanGroupID,
                            uint32_t unused1, uint32_t unused2);

    int32_t SendPlayerCheckInReceipt(int32_t checkInId);

    int32_t SendStartPlanning();

    int32_t SendReadyForBattle();

    int32_t SendPrepareScenario();

    /// <summary>
    /// Records the new server <paramref name="serverID"/>; a machine that just became server announces it and
    /// takes over the movers' orders.
    /// </summary>
    void SetServer(uint32_t serverID);

    /// <summary>The server starts the scenario on every machine.</summary>
    int32_t SendStartScenario(int32_t* playerValues, char* missionName);

    int32_t SendEndScenario(uint32_t toID, int32_t result);

    /// <summary>Sends a player's tactical order for its movers or groups to the server.</summary>
    int32_t SendPlayerOrder(uint32_t toID, MCTacticalOrder* order, int queued, int32_t numMovers, int32_t* moverParts,
                            int32_t numGroups, MCMoverGroup** groups, int fromGroup);

    int32_t SendPlayerMoverGroup(uint32_t toID, int32_t groupId, int32_t numMovers, MCMover** movers,
                                 int32_t pointIndex);

    int32_t SendPlayerArtillery(uint32_t toID, int32_t strikeType, MCVector3D location, int32_t seconds);

    /// <summary>Broadcasts every rostered mover's move chunk, status chunk and a byte of its pilot.</summary>
    int32_t SendMoverUpdate(uint32_t toID);

    int32_t SendTurretUpdate(uint32_t toID);

    int32_t SendMoverWeaponFireUpdate(uint32_t toID);

    int32_t SendTurretWeaponFireUpdate(uint32_t toID);

    int32_t SendMoverCriticalHitUpdate(uint32_t toID);

    int32_t SendWeaponHitUpdate(uint32_t toID);

    int32_t SendWorldStateUpdate(uint32_t toID);

    int32_t SendFile(char* fileName, char* directory);

    int32_t SendFileInquiry(char* fileName);

    /// <summary>The server's per-frame sends: world state and weapons, movers and turrets, each on its timer.</summary>
    int32_t UpdateClients();

    /// <summary>A client's per-frame keep-alive to the server (every second).</summary>
    int32_t UpdateServer();

    int PlayersInSession();

    /// <summary>The ids of the players in this machine's team group, or null.</summary>
    MCFLinkedList<uint32_t>* PlayersOnHomeTeam();

    /// <summary>The ids of the players in the other team's group, or null.</summary>
    MCFLinkedList<uint32_t>* PlayersOnEnemyTeam();

    int IsMyTeammate(uint32_t playerID);

    /// <summary>Whether every numbered player has checked in.</summary>
    int AllPlayersCheckedIn();

    void SwitchServers();

    /// <summary>Handles player <paramref name="playerID"/> leaving: host and server hand-over, messages, ending a lone game.</summary>
    void PlayerLeftGame(uint32_t playerID);

    void LeaveSession();

    /// <summary>Clears the rosters, check-ins, teams, timers and queues for the next game.</summary>
    void ResetForNewGame();

    /// <summary>Clears everything <see cref="ResetForNewGame"/> does and the groups, ids, names and roles.</summary>
    void InitStartupParameters();

    /// <summary>Deletes the SessionManager, the linkup blocks and the message buffer.</summary>
    void Destroy();

    MCSessionManager* SessionManager = nullptr;
    /// <summary>Where messages are built (0x1400 bytes).</summary>
    uint8_t* MsgBuffer = nullptr;
    uint32_t AllPlayerGroupID = 0;
    uint32_t InnerSphereGroupID = 0;
    uint32_t ClanGroupID = 0;
    /// <summary>The group of this machine's team (one of the two above).</summary>
    uint32_t HomeTeamGroupID = 0;
    uint32_t EnemyTeamGroupID = 0;
    /// <summary>The server's DPID.</summary>
    uint32_t ServerID = 0;
    /// <summary>The host's DPID (sendToHost; the server takes over when the host leaves).</summary>
    uint32_t HostID = 0;
    int32_t NumLocalMovers = 0;
    int32_t NumMovers = 0;
    int32_t NumTurrets = 0;
    /// <summary>The movers this machine controls.</summary>
    MCMover* LocalMovers[12]{};
    /// <summary>Every synchronised mover, by roster index.</summary>
    MCMover* MoverRoster[24]{};
    /// <summary>Per player number: its movers.</summary>
    MCMover* PlayerMoverRoster[6][12]{};
    /// <summary>Every synchronised turret, by roster index.</summary>
    MCTurret* TurretRoster[64]{};
    /// <summary>
    /// Set around a send to have this machine also handle the message itself (the host's own chat, setup, start
    /// planning, ...).
    /// </summary>
    int32_t HandleOwnMessages = 0;
    /// <summary>Nonzero when this machine is the server.</summary>
    int32_t IsServer = 0;
    /// <summary>Nonzero when this machine hosted the session.</summary>
    int32_t IsHost = 0;
    /// <summary>Set when the host left and the server took over.</summary>
    int32_t HostLeft = 0;
    /// <summary>This machine's check-in id (player slot; -1 until known, 0 on a scripted server).</summary>
    int32_t CheckInId = 0;
    /// <summary>This machine's team (-1 until chosen).</summary>
    int32_t HomeTeam = 0;
    char SessionName[80]{};
    char PlayerName[80]{};
    /// <summary>Per player number: checked in (the DPID on a scripted server).</summary>
    int32_t PlayerCheckedIn[6]{};
    /// <summary>Per player number: set by message 45 (SessionScreen::someoneCheckedIn); role not pinned down.</summary>
    int32_t PlayerSessionCheckIn[6]{};
    /// <summary>Set by the start-planning message: the players are in logistics.</summary>
    int32_t InLogistics = 0;
    /// <summary>Nonzero while a mission runs (updateClients only sends then).</summary>
    int32_t InMission = 0;
    /// <summary>The scenario result the end-scenario message carried.</summary>
    int32_t ScenarioResult = 0;
    /// <summary>Each player's DPID and team.</summary>
    MCMPPlayerTeam PlayerTeams[6]{};
    /// <summary>The chat handler (handleAppChat; Logistics installs its own).</summary>
    void (*ChatCallback)(MCFidpMessage* msg, void* data) = nullptr;
    /// <summary>scenarioTime of the next mover update.</summary>
    float NextMoverUpdateTime = 0;
    /// <summary>Seconds between mover updates.</summary>
    float MoverUpdateFrequency = 0;
    /// <summary>The number of the next mover update (receivers drop older ones).</summary>
    uint16_t MoverUpdateSequence = 0;
    float NextTurretUpdateTime = 0;
    float TurretUpdateFrequency = 0;
    /// <summary>The number of the next turret update (receivers drop older ones).</summary>
    uint16_t TurretUpdateSequence = 0;
    float NextWorldStateUpdateTime = 0;
    float WorldStateUpdateFrequency = 0;
    int32_t NumWeaponHitChunks = 0;
    /// <summary>Queued packed weapon-hit chunks.</summary>
    uint32_t WeaponHitChunks[1024]{};
    int32_t NumWorldStateChunks = 0;
    /// <summary>Queued packed world-state chunks.</summary>
    uint32_t WorldStateChunks[1024]{};
};

/// <summary>Shows the "connecting" dialog of the logistics screen.</summary>
void ShowConnectStatus();

/// <summary>Hides the "connecting" dialog.</summary>
void DestroyConnectStatusWindow();

/// <summary>Reads the Multiplayer block of the game system file (WarpFactor).</summary>
int32_t LoadMultiplayerGameSystem(MCFitIniFile* file);

/// <summary>System message: a player was created.</summary>
void HandleSysCreatePlayer(void* msg);

/// <summary>System message: a player was added to a group.</summary>
void HandleSysAddPlayerToGroup(void* msg);

/// <summary>The default chat handler.</summary>
void HandleAppChat(MCFidpMessage* msg, void* data);

// The application-message handlers: each gets the sender's DPID and the message bytes.

void HandleAppNewServer(uint32_t fromID, const void* msg);
void HandleAppPlayerCheckIn(uint32_t fromID, const void* msg);
void HandleAppPlayerSetup(uint32_t fromID, const void* msg);
void HandleAppPlayerCheckInReceipt(uint32_t fromID, const void* msg);
void HandleAppStartPlanning(uint32_t fromID, const void* msg);
void HandleAppReadyForBattle(uint32_t fromID, const void* msg);
void HandleAppJoinTeam(uint32_t fromID, const void* msg);
void HandleAppRPUpdate(uint32_t fromID, const void* msg);
void HandleAppTechbaseChange(uint32_t fromID, const void* msg);
void HandleAppSwitchScreen(uint32_t fromID, const void* msg);
void HandleAppStartScenario(uint32_t fromID, const void* msg);
void HandleAppEndScenario(uint32_t fromID, const void* msg);
void HandleAppPlayerOrder(uint32_t fromID, const void* msg);
void HandleAppPlayerMoverGroup(uint32_t fromID, const void* msg);
void HandleAppPlayerArtillery(uint32_t fromID, const void* msg);
void HandleAppMoverUpdate(uint32_t fromID, const void* msg);
/// <summary>Sets each rostered turret's target from the server's turret update (drops out-of-order updates).</summary>
void HandleAppTurretUpdate(uint32_t fromID, const void* msg);
/// <summary>Hands each mover its weapon-fire chunks from the server's update.</summary>
void HandleAppMoverWeaponFireUpdate(uint32_t fromID, const void* msg);
void HandleAppTurretWeaponFireUpdate(uint32_t fromID, const void* msg);
void HandleAppMoverCriticalHitUpdate(uint32_t fromID, const void* msg);
void HandleAppWeaponHitUpdate(uint32_t fromID, const void* msg);
void HandleAppWorldStateUpdate(uint32_t fromID, const void* msg);
void HandleAppPlayerUpdate(uint32_t fromID, const void* msg);

/// <summary>A checksum of file <paramref name="fileName"/> (to check the players have the same mission files).</summary>
uint32_t GetCheckSum(char* fileName);

void HandleAppFileInquiry(uint32_t fromID, const void* msg);
void HandleAppFileReport(uint32_t fromID, const void* msg);
void HandleAppLoadMission(uint32_t fromID, const void* msg);
void HandleAppStart(uint32_t fromID, const void* msg);

/// <summary>The lost-connection dialog's exit: leaves the session and returns to the main menu.</summary>
void LostConnectionDialogExit();

/// <summary>This machine's player was removed from the session (or the session was lost).</summary>
void HandleLocalPlayerRemoved(uint32_t fromID, const void* msg);

/// <summary>
/// The SessionManager's system-message callback: player created or destroyed, player added to a group, session
/// lost.
/// </summary>
void MultiPlayerSystemCallback(MCFidpMessage* msg, void* data);

/// <summary>The SessionManager's application-message callback: dispatches on the message type.</summary>
void MultiPlayerApplicationCallback(MCFidpMessage* msg, void* data);

/// <summary>File-sent callback (empty).</summary>
void MultiPlayerFileSentCallback(char* fileName, void* data);

/// <summary>File-received callback (empty).</summary>
void MultiPlayerFileReceivedCallback(char* fileName, void* data);
