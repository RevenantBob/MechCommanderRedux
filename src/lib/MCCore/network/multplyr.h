#pragma once

// Original source: mcx\network\multplyr.cpp: the game's side of multiplayer. MultiPlayer owns the SessionManager
// (linkup), keeps the rosters of movers and turrets that are synchronised, queues the "chunks" (packed 32-bit
// records of moves, weapon fire, hits, status and world events) and sends them at fixed frequencies; the handleApp*
// functions apply the messages the other machines send.
//
// Port: SessionManager runs over platform/MCDirectPlay (TCP/UDP) instead of DirectPlay. MPlayer stays null in a
// single-player game, as in the original. The chunk classes that
// single-player code also uses live with their objects: MoveChunk (ai/move.h), StatusChunk (object/mover.h),
// WeaponFireChunk and WeaponHitChunk (object/gameobj.h), ArtilleryChunk (object/artlry.h). WorldStateChunk is here.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

class FIDPMessage;
class FitIniFile;
class GameObject;
class Mover;
class MoverGroup;
class SessionManager;
class TacticalOrder;
class Turret;
class WeaponHitChunk;
class vector_3d;
struct _WeaponShotInfo;

class MultiPlayer;

/// <summary>The game's DirectPlay application GUID.</summary>
/// <remarks>MCX.EXE @ 0x0077a3b8 (multplyr.cpp's copy; honorb.cpp has its own at 0x00784a70)</remarks>
inline constexpr _GUID MultiPlayerAppGUID{0x09608800, 0x4815, 0x11d2, {0x92, 0xd2, 0x00, 0x60, 0x97, 0x3c, 0xfb, 0x2c}};

/// <summary>The multiplayer game, or null in a single-player game.</summary>
extern MultiPlayer* MPlayer;
/// <summary>Nonzero while a multiplayer game is set up (setupLobbyGame).</summary>
extern int isMPlayerGame;
/// <summary>Consecutive "session lost" system messages during a mission; over 10 ends the game.</summary>
extern int32_t BadSessionCounter;
/// <summary>The number of players in a LAN game (1 outside multiplayer).</summary>
extern int32_t NumLANPlayers;
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

/// <summary>The kinds of <see cref="WorldStateChunk"/> (the low 4 bits of its packed word).</summary>
/// <remarks>The names are the port's; the values are the original's.</remarks>
enum WorldStateChunkType
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
/// <see cref="data"/> goes over the network. Packing, by type (bit 0 is the lowest; all start with the 4-bit type):
/// mine: tileCol 4-13, tileRow 14-23, param2 (mine state) 24-26, param1 (team) 27;
/// terrain fire: item 4-6, vertexNum 7-15, blockNum 16-23, param1 (seconds) 24-31;
/// artillery: tileCol 4-13, tileRow 14-23, param2 + 1 (seconds) 24-28, param1 (strike type) 29-31;
/// script message: param1 (message) 4-11, param2 + 32000 (value) 12-27;
/// pilot kill: param2 (kill kind) 4-6, param1 (mover roster index) 7-11.
/// </remarks>
class WorldStateChunk
{
public:
    /// <summary>
    /// A mine at map cell (<paramref name="tileRow"/>, <paramref name="tileCol"/>) of team
    /// <paramref name="teamId"/> (0-2; 1 is the Clan layout): <paramref name="mineState"/> 0-3, where 3 (exploded)
    /// adds <paramref name="explosionType"/> 0-2.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00601300</remarks>
    void buildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState, int32_t explosionType);

    /// <summary>
    /// Terrain object <paramref name="object"/> (its part id split into block, vertex and item) set on fire for
    /// <paramref name="seconds"/> (0-255).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00601400</remarks>
    void buildTerrainFire(GameObject* object, int32_t seconds);

    /// <summary>
    /// An artillery strike of <paramref name="strikeType"/> (0-7) by commander <paramref name="commanderId"/> (0-5)
    /// at <paramref name="location"/>, landing in <paramref name="seconds"/> (-1-30).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006014d0</remarks>
    void buildArtillery(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds);

    /// <summary>Mission-script message <paramref name="message"/> (0-255) with <paramref name="value"/> (±32000).</summary>
    /// <remarks>MCX.EXE @ 0x00601620</remarks>
    void buildMissionScriptMessage(int32_t message, int32_t value);

    /// <summary>A kill of kind <paramref name="killType"/> (0-7) by the mover at <paramref name="moverIndex"/> of the roster.</summary>
    /// <remarks>MCX.EXE @ 0x006016c0</remarks>
    void buildPilotKillStat(int32_t moverIndex, int32_t killType);

    /// <summary>Packs the fields into <see cref="data"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00601760</remarks>
    void pack();

    /// <summary>Unpacks <see cref="data"/> into the fields (fatal on a type it doesn't list).</summary>
    /// <remarks>MCX.EXE @ 0x00601a10</remarks>
    void unpack();

    /// <summary>Whether every unpacked field matches <paramref name="chunk"/>'s.</summary>
    /// <remarks>MCX.EXE @ 0x00601c20</remarks>
    int equalTo(WorldStateChunk* chunk);

    /// <summary>The <see cref="WorldStateChunkType"/>.</summary>
    int8_t type = 0;     // +0x0
    int16_t tileRow = 0; // +0x1
    int16_t tileCol = 0; // +0x3
    /// <summary>Terrain fire: the object's part id (0x1000 + blockNum * 0xc80 + vertexNum * 8 + item).</summary>
    int32_t objectWID = 0; // +0x5
    int32_t blockNum = 0;  // +0x9
    int32_t vertexNum = 0; // +0xd
    int8_t item = 0;       // +0x11
    /// <summary>The first value (see the type).</summary>
    int32_t param1 = 0; // +0x12
    /// <summary>The second value (see the type).</summary>
    int32_t param2 = 0; // +0x16
    /// <summary>The packed word sent over the network.</summary>
    uint32_t data = 0; // +0x1a
};

static_assert(sizeof(WorldStateChunk) == 0x1e);

/// <summary>
/// The game's message types (bits 0-9 of the linkup header), handled by MultiPlayerApplicationCallback. 6, 9 and 12
/// are the linkup layer's, passed on.
/// </summary>
/// <remarks>The names are the port's (after the handlers); the values are the original's.</remarks>
enum MultiPlayerMessageType
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
struct MPChatMessage : public FIGuaranteedMessageHeader
{
    /// <summary>1 when sent to everyone (receiver 0).</summary>
    uint8_t toAll = 0; // +0x8
    char text[1]{};    // +0x9
};

static_assert(sizeof(MPChatMessage) == 10);

/// <summary>Player check-in and ready-for-battle (guaranteed): the sender's check-in id and home team.</summary>
struct MPPlayerCheckInMessage : public FIGuaranteedMessageHeader
{
    int8_t checkInId = 0; // +0x8
    int8_t homeTeam = 0;  // +0x9
};

static_assert(sizeof(MPPlayerCheckInMessage) == 10);

/// <summary>Player setup (guaranteed): the server's group ids.</summary>
struct MPPlayerSetupMessage : public FIGuaranteedMessageHeader
{
    uint32_t allPlayerGroupID = 0;   // +0x8
    uint32_t clanGroupID = 0;        // +0xc
    uint32_t innerSphereGroupID = 0; // +0x10
};

static_assert(sizeof(MPPlayerSetupMessage) == 0x14);

/// <summary>Check-in receipt (guaranteed, to the server): the check-in id; also end scenario: the result.</summary>
struct MPLongMessage : public FIGuaranteedMessageHeader
{
    int32_t value = 0; // +0x8
};

static_assert(sizeof(MPLongMessage) == 0xc);

/// <summary>Player order (guaranteed, to the server): a tactical order packed into two words.</summary>
struct MPPlayerOrderMessage : public FIGuaranteedMessageHeader
{
    int8_t checkInId = 0; // +0x8
    /// <summary>Bit 0: queued; bit 5: the order came from a group; bits 1-4: 1 &lt;&lt; (group id + 1) per group.</summary>
    uint8_t flags = 0; // +0x9
    /// <summary>Two words of the order for move/attack orders (order +0x24, +0x28).</summary>
    uint32_t orderParam1 = 0; // +0xa
    uint32_t orderParam2 = 0; // +0xe
    /// <summary>TacticalOrder::pack's two words (order +0x130, +0x134).</summary>
    uint32_t packedOrder[2]{}; // +0x12
};

static_assert(sizeof(MPPlayerOrderMessage) == 0x1a);

/// <summary>Player mover group (guaranteed): which movers form a group and its point man.</summary>
struct MPPlayerMoverGroupMessage : public FIGuaranteedMessageHeader
{
    int8_t checkInId = 0; // +0x8
    int8_t groupId = 0;   // +0x9
    /// <summary>(1 &lt;&lt; local index per mover) &lt;&lt; 4 | the point man's local index.</summary>
    uint16_t members = 0; // +0xa
};

static_assert(sizeof(MPPlayerMoverGroupMessage) == 0xc);

/// <summary>Player artillery (guaranteed): the target's x and y and the packed ArtilleryChunk.</summary>
struct MPPlayerArtilleryMessage : public FIGuaranteedMessageHeader
{
    float targetX = 0;          // +0x8
    float targetY = 0;          // +0xc
    uint32_t artilleryData = 0; // +0x10
};

static_assert(sizeof(MPPlayerArtilleryMessage) == 0x14);

/// <summary>
/// Start scenario (guaranteed, to everyone): per player a value (the caller's array), per mover a flag byte (bit 0:
/// the pilot's +0x38 set) and the mission name; sent as strlen(name) + 0x39 bytes.
/// </summary>
struct MPStartScenarioMessage : public FIGuaranteedMessageHeader
{
    int32_t playerValues[6]{}; // +0x8
    uint8_t moverFlags[24]{};  // +0x20
    char missionName[1]{};     // +0x38
};

static_assert(sizeof(MPStartScenarioMessage) == 0x39);

/// <summary>
/// Load mission and start (guaranteed, from the session screen): a file name; sent as strlen(name) + 0xd bytes. The
/// word at +0x8 is never written.
/// </summary>
struct MPFileNameMessage : public FIGuaranteedMessageHeader
{
    int32_t unused = 0; // +0x8
    char fileName[1]{}; // +0xc
};

static_assert(sizeof(MPFileNameMessage) == 0xd);

/// <summary>Join team (guaranteed, from the host): the player, the team (0 = none) and the slot on it.</summary>
struct MPJoinTeamMessage : public FIGuaranteedMessageHeader
{
    uint32_t playerID = 0; // +0x8
    int8_t team = 0;       // +0xc
    int8_t slot = 0;       // +0xd
};

static_assert(sizeof(MPJoinTeamMessage) == 0xe);

/// <summary>
/// Two longs (guaranteed, from the session screen): switch screen (the screen; the second unused), RP update (the
/// points, then the team) and tech base change (the team, then the tech base).
/// </summary>
struct MPTwoLongMessage : public FIGuaranteedMessageHeader
{
    int32_t value1 = 0; // +0x8
    int32_t value2 = 0; // +0xc
};

static_assert(sizeof(MPTwoLongMessage) == 0x10);

#pragma pack(pop)

/// <summary>A player's DPID and the team it joined (MultiPlayer::playerTeams).</summary>
/// <remarks>8 bytes; the name is the port's.</remarks>
struct MPPlayerTeam
{
    uint32_t playerID = 0; // +0x0
    int32_t team = 0;      // +0x4
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
class MultiPlayer
{
public:
    /// <summary>Inline in the original: calls <see cref="init()"/>.</summary>
    MultiPlayer() { init(); }

    /// <summary>Inline in the original: calls <see cref="destroy"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0075f2e0 (scalar deleting destructor, emitted in main.cpp)</remarks>
    ~MultiPlayer() { destroy(); }

    MultiPlayer(const MultiPlayer&) = delete;
    MultiPlayer& operator=(const MultiPlayer&) = delete;

    /// <summary>Clears the session pointer and message buffer, sets the chat handler and the startup parameters.</summary>
    /// <remarks>MCX.EXE @ 0x00604ec0</remarks>
    void init();

    /// <summary>
    /// Gets or creates the SessionManager, points its home directory at the working directory, installs the
    /// message callbacks and allocates the 0x1400-byte message buffer.
    /// </summary>
    /// <remarks>
    /// MCX.EXE @ 0x00604f20. The three arguments are not used (userInit passes 0x7d000, 0x100, 100); the names are
    /// the port's guesses.
    /// </remarks>
    int32_t init(int32_t heapSize, int32_t maxMessageSize, int32_t maxMessages);

    /// <summary>
    /// Reads the mover, turret and world-state update periods from prefs.cfg's Multiplayer block into
    /// <see cref="MultiplayBroadcastFrequencies"/>; a missing one or one outside 0-5 seconds takes the default for
    /// the connection (0.2/0.5/0.33 on a LAN outside a lobby, else 0.33/1/0.75).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00605110</remarks>
    void initUpdateFrequencies();

    /// <summary>
    /// Reads a scripted game's Multiplayer block (Server, NumPlayers, CheckInId, HomeTeam, frequencies, Protocol,
    /// SessionName, PlayerName) and connects.
    /// </summary>
    /// <returns>0, or SetupLobbyConnection's result.</returns>
    /// <remarks>MCX.EXE @ 0x006053f0 (vtable slot 0)</remarks>
    virtual int32_t init(FitIniFile* file);

    /// <summary>The number of players (the session's in a lobby game, else NumLANPlayers).</summary>
    /// <remarks>MCX.EXE @ 0x006057d0</remarks>
    int32_t numPlayers();

    /// <summary>Connects the lobby's session; the server creates the groups and sends its setup.</summary>
    /// <remarks>MCX.EXE @ 0x00605810</remarks>
    int32_t setupLobbyGame();

    /// <summary>Adds a mover this machine controls (at most 12); stores its index in the mover (+0x1d8).</summary>
    /// <remarks>MCX.EXE @ 0x00605960</remarks>
    void addToLocalMovers(Mover* mover);

    /// <summary>Adds a mover to the synchronised roster (at most 24); stores its index in the mover (+0x1dc).</summary>
    /// <remarks>MCX.EXE @ 0x006059c0</remarks>
    void addToMoverRoster(Mover* mover);

    /// <summary>Adds a mover to player <paramref name="playerNumber"/>'s roster (first free of 12 slots).</summary>
    /// <remarks>MCX.EXE @ 0x00605a20. Unnamed in the symbols; the name is the one its assert message uses.</remarks>
    void addToPlayerMoverRoster(int32_t playerNumber, Mover* mover);

    /// <summary>Adds a turret to the synchronised roster (at most 64); stores its index in the turret (+0x11c).</summary>
    /// <remarks>MCX.EXE @ 0x00605aa0</remarks>
    void addToTurretRoster(Turret* turret);

    /// <summary>Packs <paramref name="chunk"/> (checking it unpacks the same) and queues it (at most 1024).</summary>
    /// <returns>The number queued.</returns>
    /// <remarks>MCX.EXE @ 0x00605b00</remarks>
    int32_t addWorldStateChunk(WorldStateChunk* chunk);

    /// <remarks>MCX.EXE @ 0x00605c10</remarks>
    int32_t addMissionScriptMessageChunk(int32_t message, int32_t value);

    /// <remarks>MCX.EXE @ 0x00605c80</remarks>
    int32_t addArtilleryChunk(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds);

    /// <remarks>MCX.EXE @ 0x00605d10</remarks>
    int32_t addMineChunk(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState, int32_t explosionType);

    /// <summary>Queues a terrain fire (only for terrain object types 0x10, 0x15, 0x18, 0x1b).</summary>
    /// <remarks>MCX.EXE @ 0x00605d90</remarks>
    int32_t addLightOnFireChunk(GameObject* object, int32_t seconds);

    /// <remarks>MCX.EXE @ 0x00605e70</remarks>
    int32_t addPilotKillStat(Mover* mover, int32_t killType);

    /// <summary>Copies the queued world-state words into <paramref name="chunks"/>.</summary>
    /// <returns>How many.</returns>
    /// <remarks>MCX.EXE @ 0x00605ef0</remarks>
    int32_t grabWorldStateChunks(uint32_t* chunks);

    /// <summary>Packs <paramref name="chunk"/> (checking it unpacks the same) and queues it (at most 1024).</summary>
    /// <remarks>MCX.EXE @ 0x00605f50</remarks>
    int32_t addWeaponHitChunk(WeaponHitChunk* chunk);

    /// <summary>
    /// Builds a weapon-hit chunk for a shot at <paramref name="target"/> and queues it. <paramref name="hitFlag"/>
    /// goes through WeaponHitChunk::build to buildMoverTarget.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00606040</remarks>
    int32_t addWeaponHitChunk(GameObject* target, _WeaponShotInfo* shotInfo, int hitFlag);

    /// <summary>Moves up to <paramref name="maxChunks"/> queued weapon-hit words into <paramref name="chunks"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00606100</remarks>
    void grabWeaponHitChunks(uint32_t* chunks, int32_t maxChunks);

    /// <remarks>MCX.EXE @ 0x006061d0</remarks>
    int32_t connectIPX();

    /// <remarks>MCX.EXE @ 0x00606200</remarks>
    int32_t connectInternet(char* ipAddress);

    /// <summary>Hosts a session named <paramref name="sessionName"/> for <paramref name="maxPlayers"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00606230</remarks>
    int32_t createSession(char* sessionName, char* playerName, int32_t maxPlayers);

    /// <summary>Joins session <paramref name="sessionName"/> (the first one when null).</summary>
    /// <remarks>MCX.EXE @ 0x006064a0</remarks>
    int32_t joinSession(char* sessionName, char* playerName);

    /// <summary>Runs the SessionManager's message pump.</summary>
    /// <remarks>MCX.EXE @ 0x006065e0</remarks>
    int32_t processReceiveList();

    /// <summary>Sends a message to the host (guaranteed or not).</summary>
    /// <returns>0; -1 without a session; -2 on the host itself.</returns>
    /// <remarks>MCX.EXE @ 0x00606620</remarks>
    int32_t sendToHost(FIMessageHeader* msg, int32_t size, int guaranteed);

    /// <summary>Sends chat <paramref name="text"/> to <paramref name="toID"/> (0 = everyone).</summary>
    /// <remarks>MCX.EXE @ 0x00606690</remarks>
    int32_t sendChat(uint32_t toID, char* text);

    /// <remarks>MCX.EXE @ 0x006067d0</remarks>
    int32_t sendPlayerCheckIn();

    /// <summary>The server's setup message to <paramref name="toID"/> (0 = everyone): the group ids.</summary>
    /// <remarks>MCX.EXE @ 0x006068b0. The last two arguments are not used.</remarks>
    int32_t sendPlayerSetup(uint32_t toID, uint32_t serverID, uint32_t innerSphereGroupID, uint32_t clanGroupID,
                            uint32_t unused1, uint32_t unused2);

    /// <remarks>MCX.EXE @ 0x006069c0</remarks>
    int32_t sendPlayerCheckInReceipt(int32_t checkInId);

    /// <remarks>MCX.EXE @ 0x00606a70</remarks>
    int32_t sendStartPlanning();

    /// <remarks>MCX.EXE @ 0x00606b60</remarks>
    int32_t sendReadyForBattle();

    /// <remarks>MCX.EXE @ 0x00606c50</remarks>
    int32_t sendPrepareScenario();

    /// <summary>
    /// Records the new server <paramref name="serverID"/>; a machine that just became server announces it and
    /// takes over the movers' orders.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00606d30</remarks>
    void setServer(uint32_t serverID);

    /// <summary>The server starts the scenario on every machine.</summary>
    /// <remarks>MCX.EXE @ 0x00606f70. Unnamed in the symbols; the name is the one its assert message uses.</remarks>
    int32_t sendStartScenario(int32_t* playerValues, char* missionName);

    /// <remarks>MCX.EXE @ 0x00607150</remarks>
    int32_t sendEndScenario(uint32_t toID, int32_t result);

    /// <summary>Sends a player's tactical order for its movers or groups to the server.</summary>
    /// <remarks>MCX.EXE @ 0x00607220</remarks>
    int32_t sendPlayerOrder(uint32_t toID, TacticalOrder* order, int queued, int32_t numMovers, int32_t* moverParts,
                            int32_t numGroups, MoverGroup** groups, int fromGroup);

    /// <remarks>MCX.EXE @ 0x00607550</remarks>
    int32_t sendPlayerMoverGroup(uint32_t toID, int32_t groupId, int32_t numMovers, Mover** movers, int32_t pointIndex);

    /// <remarks>MCX.EXE @ 0x006076b0</remarks>
    int32_t sendPlayerArtillery(uint32_t toID, int32_t strikeType, vector_3d location, int32_t seconds);

    /// <summary>Broadcasts every rostered mover's move chunk, status chunk and a byte of its pilot.</summary>
    /// <remarks>MCX.EXE @ 0x00607840. Unnamed in the symbols; the name is the one its assert message uses.</remarks>
    int32_t sendMoverUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00607a30</remarks>
    int32_t sendTurretUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00607c00</remarks>
    int32_t sendMoverWeaponFireUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00607d90</remarks>
    int32_t sendTurretWeaponFireUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00607f70</remarks>
    int32_t sendMoverCriticalHitUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00608160</remarks>
    int32_t sendWeaponHitUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00608270</remarks>
    int32_t sendWorldStateUpdate(uint32_t toID);

    /// <remarks>MCX.EXE @ 0x00608420</remarks>
    int32_t sendFile(char* fileName, char* directory);

    /// <remarks>MCX.EXE @ 0x00608450</remarks>
    int32_t sendFileInquiry(char* fileName);

    /// <summary>The server's per-frame sends: world state and weapons, movers and turrets, each on its timer.</summary>
    /// <remarks>MCX.EXE @ 0x00608510</remarks>
    int32_t updateClients();

    /// <summary>A client's per-frame keep-alive to the server (every second).</summary>
    /// <remarks>MCX.EXE @ 0x00608610</remarks>
    int32_t updateServer();

    /// <remarks>MCX.EXE @ 0x00608690</remarks>
    int playersInSession();

    /// <summary>The ids of the players in this machine's team group, or null.</summary>
    /// <remarks>MCX.EXE @ 0x006086c0</remarks>
    FLinkedList<uint32_t>* playersOnHomeTeam();

    /// <summary>The ids of the players in the other team's group, or null.</summary>
    /// <remarks>MCX.EXE @ 0x00608700</remarks>
    FLinkedList<uint32_t>* playersOnEnemyTeam();

    /// <remarks>MCX.EXE @ 0x00608740</remarks>
    int isMyTeammate(uint32_t playerID);

    /// <summary>Whether every numbered player has checked in.</summary>
    /// <remarks>MCX.EXE @ 0x006087f0</remarks>
    int allPlayersCheckedIn();

    /// <remarks>MCX.EXE @ 0x006088f0</remarks>
    void switchServers();

    /// <summary>Handles player <paramref name="playerID"/> leaving: host and server hand-over, messages, ending a lone game.</summary>
    /// <remarks>MCX.EXE @ 0x00608960</remarks>
    void playerLeftGame(uint32_t playerID);

    /// <remarks>MCX.EXE @ 0x00608fc0</remarks>
    void leaveSession();

    /// <summary>Clears the rosters, check-ins, teams, timers and queues for the next game.</summary>
    /// <remarks>MCX.EXE @ 0x00608fe0</remarks>
    void resetForNewGame();

    /// <summary>Clears everything <see cref="resetForNewGame"/> does and the groups, ids, names and roles.</summary>
    /// <remarks>MCX.EXE @ 0x006091c0</remarks>
    void initStartupParameters();

    /// <summary>Deletes the SessionManager, the linkup blocks and the message buffer.</summary>
    /// <remarks>MCX.EXE @ 0x00609400</remarks>
    void destroy();

    SessionManager* sessionManager = nullptr; // +0x4
    /// <summary>Where messages are built (0x1400 bytes).</summary>
    uint8_t* msgBuffer = nullptr;    // +0x8
    uint32_t allPlayerGroupID = 0;   // +0xc
    uint32_t innerSphereGroupID = 0; // +0x10
    uint32_t clanGroupID = 0;        // +0x14
    /// <summary>The group of this machine's team (one of the two above).</summary>
    uint32_t homeTeamGroupID = 0;  // +0x18
    uint32_t enemyTeamGroupID = 0; // +0x1c
    /// <summary>The server's DPID.</summary>
    uint32_t serverID = 0; // +0x20
    /// <summary>The host's DPID (sendToHost; the server takes over when the host leaves).</summary>
    uint32_t hostID = 0;        // +0x24
    int32_t numLocalMovers = 0; // +0x28
    int32_t numMovers = 0;      // +0x2c
    int32_t numTurrets = 0;     // +0x30
    /// <summary>The movers this machine controls.</summary>
    Mover* localMovers[12]{}; // +0x34
    /// <summary>Every synchronised mover, by roster index.</summary>
    Mover* moverRoster[24]{}; // +0x64
    /// <summary>Per player number: its movers.</summary>
    Mover* playerMoverRoster[6][12]{}; // +0xc4
    /// <summary>Every synchronised turret, by roster index.</summary>
    Turret* turretRoster[64]{}; // +0x1e4
    /// <summary>
    /// Set around a send to have this machine also handle the message itself (the host's own chat, setup, start
    /// planning, ...).
    /// </summary>
    int32_t handleOwnMessages = 0; // +0x2e4
    /// <summary>Nonzero when this machine is the server.</summary>
    int32_t isServer = 0; // +0x2e8
    /// <summary>Nonzero when this machine hosted the session.</summary>
    int32_t isHost = 0; // +0x2ec
    /// <summary>Set when the host left and the server took over.</summary>
    int32_t hostLeft = 0; // +0x2f0
    /// <summary>This machine's check-in id (player slot; -1 until known, 0 on a scripted server).</summary>
    int32_t checkInId = 0; // +0x2f4
    /// <summary>This machine's team (-1 until chosen).</summary>
    int32_t homeTeam = 0;   // +0x2f8
    char sessionName[80]{}; // +0x2fc
    char playerName[80]{};  // +0x34c
    /// <summary>Per player number: checked in (the DPID on a scripted server).</summary>
    int32_t playerCheckedIn[6]{}; // +0x39c
    /// <summary>Per player number: set by message 45 (SessionScreen::someoneCheckedIn); role not pinned down.</summary>
    int32_t playerSessionCheckIn[6]{}; // +0x3b4
    /// <summary>Set by the start-planning message: the players are in logistics.</summary>
    int32_t inLogistics = 0; // +0x3cc
    /// <summary>Nonzero while a mission runs (updateClients only sends then).</summary>
    int32_t inMission = 0; // +0x3d0
    /// <summary>The scenario result the end-scenario message carried.</summary>
    int32_t scenarioResult = 0; // +0x3d4
    /// <summary>Each player's DPID and team.</summary>
    MPPlayerTeam playerTeams[6]{}; // +0x3d8
    /// <summary>The chat handler (handleAppChat; Logistics installs its own).</summary>
    void (*chatCallback)(FIDPMessage* msg, void* data) = nullptr; // +0x408
    /// <summary>scenarioTime of the next mover update.</summary>
    float nextMoverUpdateTime = 0; // +0x414
    /// <summary>Seconds between mover updates.</summary>
    float moverUpdateFrequency = 0; // +0x418
    /// <summary>The number of the next mover update (receivers drop older ones).</summary>
    uint16_t moverUpdateSequence = 0; // +0x41c
    float nextTurretUpdateTime = 0;   // +0x420
    float turretUpdateFrequency = 0;  // +0x424
    /// <summary>The number of the next turret update (receivers drop older ones).</summary>
    uint16_t turretUpdateSequence = 0;   // +0x428
    float nextWorldStateUpdateTime = 0;  // +0x430
    float worldStateUpdateFrequency = 0; // +0x434
    int32_t numWeaponHitChunks = 0;      // +0x438
    /// <summary>Queued packed weapon-hit chunks.</summary>
    uint32_t weaponHitChunks[1024]{}; // +0x43c
    int32_t numWorldStateChunks = 0;  // +0x143c
    /// <summary>Queued packed world-state chunks.</summary>
    uint32_t worldStateChunks[1024]{}; // +0x1440
};

/// <summary>Shows the "connecting" dialog of the logistics screen.</summary>
/// <remarks>MCX.EXE @ 0x00601070</remarks>
void ShowConnectStatus();

/// <summary>Hides the "connecting" dialog.</summary>
/// <remarks>MCX.EXE @ 0x00601190</remarks>
void DestroyConnectStatusWindow();

/// <summary>Reads the Multiplayer block of the game system file (WarpFactor).</summary>
/// <remarks>MCX.EXE @ 0x00601230</remarks>
int32_t loadMultiplayerGameSystem(FitIniFile* file);

/// <summary>System message: a player was created.</summary>
/// <remarks>MCX.EXE @ 0x00601cf0</remarks>
void handleSysCreatePlayer(void* msg);

/// <summary>System message: a player was added to a group.</summary>
/// <remarks>MCX.EXE @ 0x00601d70</remarks>
void handleSysAddPlayerToGroup(void* msg);

/// <summary>The default chat handler.</summary>
/// <remarks>MCX.EXE @ 0x00601df0</remarks>
void handleAppChat(FIDPMessage* msg, void* data);

// The application-message handlers: each gets the sender's DPID and the message bytes.

/// <remarks>MCX.EXE @ 0x00601e80</remarks>
void handleAppNewServer(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00601ea0</remarks>
void handleAppPlayerCheckIn(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00601f90</remarks>
void handleAppPlayerSetup(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006020f0</remarks>
void handleAppPlayerCheckInReceipt(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00602190</remarks>
void handleAppStartPlanning(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006021b0</remarks>
void handleAppReadyForBattle(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00602260</remarks>
void handleAppJoinTeam(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006022b0</remarks>
void handleAppRPUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00602330</remarks>
void handleAppTechbaseChange(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00602380</remarks>
void handleAppSwitchScreen(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006023b0</remarks>
void handleAppStartScenario(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00602590</remarks>
void handleAppEndScenario(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006025b0</remarks>
void handleAppPlayerOrder(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603040</remarks>
void handleAppPlayerMoverGroup(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006031a0</remarks>
void handleAppPlayerArtillery(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603270</remarks>
void handleAppMoverUpdate(uint32_t fromID, const void* msg);
/// <summary>Sets each rostered turret's target from the server's turret update (drops out-of-order updates).</summary>
/// <remarks>MCX.EXE @ 0x00603440. Unnamed in the symbols; the name is the one its assert message uses.</remarks>
void handleAppTurretUpdate(uint32_t fromID, const void* msg);
/// <summary>Hands each mover its weapon-fire chunks from the server's update.</summary>
/// <remarks>MCX.EXE @ 0x00603590. Unnamed in the symbols (the name is the port's, after its message type).</remarks>
void handleAppMoverWeaponFireUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603650</remarks>
void handleAppTurretWeaponFireUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603730</remarks>
void handleAppMoverCriticalHitUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006038a0</remarks>
void handleAppWeaponHitUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603b20</remarks>
void handleAppWorldStateUpdate(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00603f80</remarks>
void handleAppPlayerUpdate(uint32_t fromID, const void* msg);

/// <summary>A checksum of file <paramref name="fileName"/> (to check the players have the same mission files).</summary>
/// <remarks>MCX.EXE @ 0x00603fb0</remarks>
uint32_t getCheckSum(char* fileName);

/// <remarks>MCX.EXE @ 0x00604070</remarks>
void handleAppFileInquiry(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00604160</remarks>
void handleAppFileReport(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x006041c0</remarks>
void handleAppLoadMission(uint32_t fromID, const void* msg);
/// <remarks>MCX.EXE @ 0x00604200</remarks>
void handleAppStart(uint32_t fromID, const void* msg);

/// <summary>The lost-connection dialog's exit: leaves the session and returns to the main menu.</summary>
/// <remarks>MCX.EXE @ 0x00604280</remarks>
void LostConnectionDialogExit();

/// <summary>This machine's player was removed from the session (or the session was lost).</summary>
/// <remarks>MCX.EXE @ 0x00604300</remarks>
void handleLocalPlayerRemoved(uint32_t fromID, const void* msg);

/// <summary>
/// The SessionManager's system-message callback: player created or destroyed, player added to a group, session
/// lost.
/// </summary>
/// <remarks>MCX.EXE @ 0x00604560. Unnamed in the symbols (the name is the port's).</remarks>
void MultiPlayerSystemCallback(FIDPMessage* msg, void* data);

/// <summary>The SessionManager's application-message callback: dispatches on the message type.</summary>
/// <remarks>MCX.EXE @ 0x006046a0</remarks>
void MultiPlayerApplicationCallback(FIDPMessage* msg, void* data);

/// <summary>File-sent callback (empty).</summary>
/// <remarks>MCX.EXE @ 0x00604e20</remarks>
void MultiPlayerFileSentCallback(char* fileName, void* data);

/// <summary>File-received callback (empty).</summary>
/// <remarks>MCX.EXE @ 0x00604e30</remarks>
void MultiPlayerFileReceivedCallback(char* fileName, void* data);
