#include "stdafx.h"
#include "network/multplyr.h"
#include "abl/abldbug.h"
#include "abl/ablxstd.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "iface/parser.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/fidpgroup.h"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/loggen.h"
#include "logistics/logdlg.h"
#include "logistics/logmain.h"
#include "logistics/logsession.h"
#include "main/honorb.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/comndr.h"
#include "object/explode.h"
#include "object/gameobj.h"
#include "object/group.h"
#include "object/mech.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/tbldng.h"
#include "object/terrobj.h"
#include "object/tree.h"
#include "object/turret.h"
#include "object/warrior.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"

MultiPlayer* MPlayer = nullptr;
int isMPlayerGame = 0;
int32_t BadSessionCounter = 0;
int32_t NumLANPlayers = 1;
float MultiplayBroadcastFrequencies[3] = {};
uint32_t LastConnectionType = 0;
float WarpFactor = 1.0f;
int32_t WorldStateChunkTally[10] = {};

namespace
{
    /// <summary>
    /// Set by the prepare-scenario message (sendPrepareScenario) and cleared once the scenario starts: a dropped
    /// connection then ends the scenario instead of going back to the session screen.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x007a9e28. It has no symbol; the name is the port's.</remarks>
    int32_t prepareScenarioReceived = 0;

    /// <summary>The entry angle of a weapon hit (WeaponHitChunk::entryAngle) in degrees.</summary>
    /// <remarks>MCX.EXE @ 0x007890b0</remarks>
    const float HitEntryAngles[4] = {0.0f, 180.0f, -90.0f, 90.0f};

    /// <summary>The radio message a pilot sends for a kill, by kill kind.</summary>
    /// <remarks>
    /// MCX.EXE @ 0x007890c0. The original's table has six entries; kinds 6 and 7 (allowed by buildPilotKillStat)
    /// read on into the next data, 0 and the bytes of a string.
    /// </remarks>
    const int32_t KillRadioMessages[8] = {19, 19, 19, 19, 18, 17, 0, 0};

    /// <summary>Whether a mission is running a multiplayer game with company (every in-mission handler's check).</summary>
    bool InMultiplayerMission()
    {
        return scenario != nullptr && EventsToMissionResultsScreen == 0 && MPlayer->numPlayers() > 1;
    }
}

auto WorldStateChunk::operator new(size_t size) noexcept -> void*
{
    if (systemHeap == nullptr)
    {
        return std::malloc(size);
    }

    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto WorldStateChunk::operator delete(void* ptr) -> void
{
    if (systemHeap == nullptr)
    {
        std::free(ptr);
    }
    else
    {
        systemHeap->free(ptr);
    }
}

auto WorldStateChunk::buildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState,
                                int32_t explosionType) -> void
{
    type = WSCHUNK_MINE;
    this->tileRow = static_cast<int16_t>(tileRow);
    this->tileCol = static_cast<int16_t>(tileCol);
    param1 = teamId;
    Assert(teamId >= 0 && teamId <= 2, teamId, " WorldStateChunk.buildMine: bad team id ");
    param2 = mineState;

    if (param2 == 3)
    {
        param2 += explosionType;
    }

    Assert(mineState >= 0 && mineState <= 3, mineState, " WorldStateChunk.buildMine: bad mine state ");
    Assert(explosionType >= 0 && explosionType <= 2, explosionType,
           " WorldStateChunk.buildMine: bad mine explosionType ");
    data = 0;
}

auto WorldStateChunk::buildTerrainFire(GameObject* object, int32_t seconds) -> void
{
    type = WSCHUNK_TERRAIN_FIRE;
    objectWID = object->partId;
    blockNum = (objectWID - 0x1000) / 0xc80;
    int32_t rest = objectWID - 0x1000 - blockNum * 0xc80;
    vertexNum = rest / 8;
    item = static_cast<int8_t>(rest - vertexNum * 8);
    param1 = seconds;
    Assert(seconds >= 0 && seconds <= 255, seconds, " WorldStateChunk.buildTerrainFire: bad seconds ");
    data = 0;
}

auto WorldStateChunk::buildArtillery(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds)
    -> void
{
    type = static_cast<int8_t>(commanderId + WSCHUNK_ARTILLERY);
    Assert(commanderId >= 0 && commanderId <= 5, commanderId, " WorldStateChunk.BuildArtillery: bad commander id ");
    param1 = strikeType;
    Assert(strikeType >= 0 && strikeType <= 7, strikeType, " WorldStateChunk.BuildArtillery: bad artillery type ");
    param2 = seconds;
    Assert(seconds >= -1 && seconds <= 30, seconds, " WorldStateChunk.BuildArtillery: bad seconds ");
    int32_t cellRow = 0;
    int32_t cellCol = 0;
    worldCoordToMapCell(location, cellRow, cellCol);
    tileRow = static_cast<int16_t>(cellRow);
    tileCol = static_cast<int16_t>(cellCol);
    data = 0;
}

auto WorldStateChunk::buildMissionScriptMessage(int32_t message, int32_t value) -> void
{
    type = WSCHUNK_MISSION_SCRIPT_MESSAGE;
    param1 = message;
    Assert(message >= 0 && message <= 255, message, " WorldState.BuildMissionScriptMessage: bad message Code ");
    param2 = value;
    Assert(value >= -32000 && value <= 32000, value, " WorldState.BuildMissionScriptMessage: bad message Param ");
    data = 0;
}

auto WorldStateChunk::buildPilotKillStat(int32_t moverIndex, int32_t killType) -> void
{
    type = WSCHUNK_PILOT_KILL_STAT;
    param1 = moverIndex;
    Assert(moverIndex >= 0 && moverIndex < MPlayer->numMovers, moverIndex,
           " WorldState.BuildPilotKillStat: bad mover index ");
    param2 = killType;
    Assert(killType >= 0 && killType <= 7, killType, " WorldState.BuildPilotKillStat: bad vehicle class ");
    data = 0;
}

auto WorldStateChunk::pack() -> void
{
    data = 0;

    switch (type)
    {
        case WSCHUNK_MINE:
        {
            data |= param1;
            data <<= 3;
            data |= param2;
            data <<= 10;
            data |= tileRow;
            data <<= 10;
            data |= tileCol;
            data <<= 4;
            break;
        }

        case WSCHUNK_TERRAIN_FIRE:
        {
            data |= param1;
            data <<= 8;
            data |= blockNum;
            data <<= 9;
            data |= vertexNum;
            data <<= 3;
            data |= item;
            data <<= 4;
            break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        {
            data |= param1;
            data <<= 5;
            data |= param2 + 1;
            data <<= 10;
            data |= tileRow;
            data <<= 10;
            data |= tileCol;
            data <<= 4;
            break;
        }

        case WSCHUNK_MISSION_SCRIPT_MESSAGE:
        {
            data |= param2 + 32000;
            data <<= 8;
            data |= param1;
            data <<= 4;
            break;
        }

        case WSCHUNK_PILOT_KILL_STAT:
        {
            data |= param1;
            data <<= 3;
            data |= param2;
            data <<= 4;
            break;
        }
    }

    data |= type;
}

auto WorldStateChunk::unpack() -> void
{
    uint32_t packed = data;
    type = static_cast<int8_t>(packed & 0xf);
    uint32_t rest = packed >> 4;

    switch (type)
    {
        case WSCHUNK_MINE:
        {
            tileCol = static_cast<int16_t>(rest & 0x3ff);
            tileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
            param2 = (packed >> 24) & 7;
            param1 = (packed >> 27) & 1;
            break;
        }

        case WSCHUNK_TERRAIN_FIRE:
        {
            item = static_cast<int8_t>(rest & 7);
            vertexNum = (packed >> 7) & 0x1ff;
            blockNum = (packed >> 16) & 0xff;
            objectWID = vertexNum * 8 + 0x1000 + blockNum * 0xc80 + item;
            // Only 6 of the 8 bits pack wrote come back (OB-104): a fire of 64 seconds or more arrives shorter.
            param1 = (packed >> 24) & 0x3f;
            break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        {
            tileCol = static_cast<int16_t>(rest & 0x3ff);
            tileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
            param2 = static_cast<int32_t>((packed >> 24) & 0x1f) - 1;
            param1 = packed >> 29;
            break;
        }

        case WSCHUNK_MISSION_SCRIPT_MESSAGE:
        {
            param1 = rest & 0xff;
            param2 = static_cast<int32_t>((packed >> 12) & 0xffff) - 32000;
            break;
        }

        case WSCHUNK_PILOT_KILL_STAT:
        {
            param2 = rest & 7;
            param1 = (packed >> 7) & 0x1f;
            break;
        }

        default:
        {
            Fatal(0, " WorldStateChunk.unpack: bad type ");
        }
    }
}

auto WorldStateChunk::equalTo(WorldStateChunk* chunk) -> int
{
    return type == chunk->type && tileRow == chunk->tileRow && tileCol == chunk->tileCol &&
           objectWID == chunk->objectWID && blockNum == chunk->blockNum && vertexNum == chunk->vertexNum &&
           item == chunk->item && param1 == chunk->param1 && param2 == chunk->param2;
}

auto MultiPlayer::operator new(size_t size) noexcept -> void*
{
    if (systemHeap == nullptr)
    {
        return std::malloc(size);
    }

    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto MultiPlayer::operator delete(void* ptr) -> void
{
    if (systemHeap == nullptr)
    {
        std::free(ptr);
    }
    else
    {
        systemHeap->free(ptr);
    }
}

auto MultiPlayer::init() -> void
{
    if (globalLogPtr != nullptr)
    {
        globalLogPtr->currentMission = -1;
    }

    sessionManager = nullptr;
    msgBuffer = nullptr;
    BadSessionCounter = 0;
    chatCallback = handleAppChat;
    initStartupParameters();
    isMPlayerGame = 0;
}

auto MultiPlayer::init(int32_t heapSize, int32_t maxMessageSize, int32_t maxMessages) -> int32_t
{
    sessionManager = SessionManager::GetGlobalPointer(nullptr);

    if (sessionManager == nullptr)
    {
        InitLinkUpHeap();
        sessionManager = new SessionManager(MultiPlayerAppGUID);
        Assert(sessionManager != nullptr, 0, "Error creating sessionManager");
    }

    // Port: the original passed _getcwd. The port's files are relative to the data directory, so "." stands for it.
    char homeDirectory[] = ".";
    sessionManager->SetHomeDirectory(homeDirectory);
    sessionManager->applicationCallback = MultiPlayerApplicationCallback;
    sessionManager->applicationCallbackData = nullptr;
    sessionManager->systemCallback = MultiPlayerSystemCallback;
    sessionManager->systemCallbackData = nullptr;
    sessionManager->fileReceivedCallback = MultiPlayerFileReceivedCallback;
    sessionManager->fileReceivedCallbackData = nullptr;

    if (msgBuffer != nullptr)
    {
        systemHeap->free(msgBuffer);
    }

    msgBuffer = static_cast<uint8_t*>(systemHeap->malloc(0x1400));
    Assert(msgBuffer != nullptr, 0, " MultiPlayer: no RAM for msgBuffer ");
    return 0;
}

auto MultiPlayer::initUpdateFrequencies() -> void
{
    FitIniFile prefsFile;
    moverUpdateFrequency = -1.0f;
    turretUpdateFrequency = -1.0f;
    worldStateUpdateFrequency = -1.0f;
    int32_t result = prefsFile.open("prefs.cfg", READ, 0x32);
    Assert(result == 0, 0, "Could not open prefs.cfg");

    if (prefsFile.seekBlock("Multiplayer") == 0)
    {
        if (prefsFile.readIdFloat("MoverUpdateFrequency", moverUpdateFrequency) != 0)
        {
            moverUpdateFrequency = -1.0f;
        }

        if (prefsFile.readIdFloat("TurretUpdateFrequency", turretUpdateFrequency) != 0)
        {
            turretUpdateFrequency = -1.0f;
        }

        if (prefsFile.readIdFloat("WorldStateUpdateFrequency", worldStateUpdateFrequency) != 0)
        {
            worldStateUpdateFrequency = -1.0f;
        }
    }

    prefsFile.close();

    // A period outside 0-5 seconds (or missing) takes the default: shorter ones on a LAN (IPX or TCP/IP) outside a
    // lobby.
    auto outOfRange = [](float frequency) { return frequency < 0.0f || frequency > 5.0f; };
    const int32_t connection = sessionManager->currentConnection;

    if ((connection == 2 || connection == 1) && launchedFromLobby == 0)
    {
        if (outOfRange(moverUpdateFrequency))
        {
            moverUpdateFrequency = 0.2f;
        }

        if (outOfRange(worldStateUpdateFrequency))
        {
            worldStateUpdateFrequency = 0.33f;
        }

        if (outOfRange(turretUpdateFrequency))
        {
            turretUpdateFrequency = 0.5f;
        }
    }
    else
    {
        if (outOfRange(moverUpdateFrequency))
        {
            moverUpdateFrequency = 0.33f;
        }

        if (outOfRange(worldStateUpdateFrequency))
        {
            worldStateUpdateFrequency = 0.75f;
        }

        if (outOfRange(turretUpdateFrequency))
        {
            turretUpdateFrequency = 1.0f;
        }
    }

    MultiplayBroadcastFrequencies[0] = moverUpdateFrequency;
    MultiplayBroadcastFrequencies[1] = turretUpdateFrequency;
    MultiplayBroadcastFrequencies[2] = worldStateUpdateFrequency;
}

auto MultiPlayer::init(FitIniFile* file) -> int32_t
{
    int32_t result = file->readIdBoolean("Server", isServer);
    Assert(result == 0, 0, " could not find Multiplayer:Server ");

    if (launchedFromLobby == 0)
    {
        result = file->readIdLong("NumPlayers", NumLANPlayers);
        Assert(result == 0, 0, " could not find Multiplayer:NumPlayers ");
    }

    if (startupPakFile == nullptr)
    {
        result = file->readIdLong("CheckInId", checkInId);

        if (result != 0)
        {
            checkInId = isServer == 0 ? -1 : 0;
        }
    }
    else
    {
        checkInId = isServer == 0 ? -1 : 0;
    }

    result = file->readIdLong("HomeTeam", homeTeam);
    Assert(result == 0, 0, " could not find Multiplayer:HomeTeam ");
    uint32_t connectResult = sessionManager->SetupLobbyConnection(nullptr, nullptr);

    if (connectResult == 0)
    {
        isServer = sessionManager->isHost;

        if (isServer != 0)
        {
            playerCheckedIn[checkInId] = static_cast<int32_t>(sessionManager->myPlayer->id);
            char allPlayerGroup[] = "AllPlayerGroup";
            char innerSphereGroup[] = "InnerSphereGroup";
            char clanGroup[] = "ClanGroup";
            sessionManager->CreateGroup(&allPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
            sessionManager->CreateGroup(&innerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
            sessionManager->CreateGroup(&clanGroupID, clanGroup, nullptr, 0, 0);
            result = file->readIdFloat("MoverUpdateFrequency", moverUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:MoverUpdateFrequency ");
            result = file->readIdFloat("WorldStateUpdateFrequency", worldStateUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:WorldStateUpdateFrequency ");
        }
    }
    else if (connectResult == DPERR_NOTLOBBIED)
    {
        if (isServer != 0)
        {
            result = file->readIdFloat("MoverUpdateFrequency", moverUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:MoverUpdateFrequency ");
            result = file->readIdFloat("WorldStateUpdateFrequency", worldStateUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:WorldStateUpdateFrequency ");
        }

        uint32_t protocol = 0;
        result = file->readIdULong("Protocol", protocol);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find protocol in Multiplayer info file");
        result = file->readIdString("SessionName", sessionName, 0x4f);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find Multiplayer:SessionName ");
        result = file->readIdString("PlayerName", playerName, 0x4f);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find Multiplayer:PlayerName ");
        Assert((sessionManager->availableProtocols & protocol) != 0, 0,
               "Connection protocol specified is not available");
        result = sessionManager->SetCurrentConnection(static_cast<int>(protocol));
        Assert(result == 0, 0, "Could not connect.");
    }

    return static_cast<int32_t>(connectResult);
}

auto MultiPlayer::numPlayers() -> int32_t
{
    if (launchedFromLobby != 0)
    {
        return sessionManager->GetPlayers(nullptr)->Size();
    }

    return NumLANPlayers;
}

auto MultiPlayer::setupLobbyGame() -> int32_t
{
    uint32_t result = sessionManager->SetupLobbyConnection(ShowConnectStatus, DestroyConnectStatusWindow);

    if (result == 0)
    {
        isMPlayerGame = 1;
        isServer = sessionManager->isHost;
        isHost = isServer;

        if (sessionManager->currentConnection != 0x10)
        {
            NumLANPlayers = sessionManager->GetPlayers(nullptr)->Size();
        }

        if (isServer != 0)
        {
            char allPlayerGroup[] = "AllPlayerGroup";
            char innerSphereGroup[] = "InnerSphereGroup";
            char clanGroup[] = "ClanGroup";
            sessionManager->CreateGroup(&allPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
            sessionManager->CreateGroup(&innerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
            sessionManager->CreateGroup(&clanGroupID, clanGroup, nullptr, 0, 0);
            handleOwnMessages = 1;
            sendPlayerSetup(0, MPlayer->serverID, MPlayer->innerSphereGroupID, MPlayer->clanGroupID, 0, 0);
            handleOwnMessages = 0;
        }
    }

    return static_cast<int32_t>(result);
}

auto MultiPlayer::addToLocalMovers(Mover* mover) -> void
{
    if (numLocalMovers == 12)
    {
        Fatal(0, " Too many local movers for network ");
    }

    localMovers[numLocalMovers] = mover;
    mover->netPlayerId = numLocalMovers;
    numLocalMovers++;
}

auto MultiPlayer::addToMoverRoster(Mover* mover) -> void
{
    if (numMovers == 24)
    {
        Fatal(0, " Too many movers for multiplay ");
    }

    moverRoster[numMovers] = mover;
    mover->netRosterIndex = numMovers;
    numMovers++;
}

auto MultiPlayer::addToPlayerMoverRoster(int32_t playerNumber, Mover* mover) -> void
{
    int32_t i = 0;

    for (; i < 12; i++)
    {
        if (playerMoverRoster[playerNumber][i] == nullptr)
        {
            playerMoverRoster[playerNumber][i] = mover;
            break;
        }
    }

    Assert(i < 12, 0, " MultiPlayer.addToPlayerMoverRoster: Too many local movers ");
}

auto MultiPlayer::addToTurretRoster(Turret* turret) -> void
{
    if (numTurrets == 64)
    {
        Fatal(0, " Too many turrets for multiplay ");
    }

    turretRoster[numTurrets] = turret;
    turret->netRosterIndex = numTurrets;
    numTurrets++;
}

namespace
{
    /// <summary>An empty chunk as the original's locals start (type 0, cells -1, the rest 0).</summary>
    WorldStateChunk EmptyWorldStateChunk()
    {
        WorldStateChunk chunk;
        chunk.type = 0;
        chunk.tileRow = -1;
        chunk.tileCol = -1;
        chunk.objectWID = 0;
        chunk.blockNum = 0;
        chunk.vertexNum = 0;
        chunk.item = 0;
        chunk.param1 = 0;
        chunk.param2 = 0;
        chunk.data = 0;
        return chunk;
    }

    /// <summary>An empty weapon-hit chunk as the original's locals start (hit location -1, the rest 0).</summary>
    WeaponHitChunk EmptyWeaponHitChunk()
    {
        WeaponHitChunk chunk;
        chunk.targetType = 0;
        chunk.targetId = 0;
        chunk.targetBlockOrTrainNumber = 0;
        chunk.targetVertexOrCarNumber = 0;
        chunk.targetItemNumber = 0;
        chunk.cause = 0;
        chunk.damage = 0.0f;
        chunk.hitLocation = -1;
        chunk.entryAngle = 0;
        chunk.refit = 0;
        chunk.data = 0;
        return chunk;
    }

    /// <summary>Starts a guaranteed message of <paramref name="type"/> in <paramref name="buffer"/>.</summary>
    /// <remarks>Inline in the original: every send clears the tagger and sets the header word the same way.</remarks>
    FIGuaranteedMessageHeader* StartGuaranteedMessage(uint8_t* buffer, uint16_t type)
    {
        auto* header = reinterpret_cast<FIGuaranteedMessageHeader*>(buffer);

        for (int32_t i = 0; i < 6; i++)
        {
            header->tagger.sendCount[i] = 0;
        }

        header->header = 0;
        header->header |= FIMSG_GUARANTEED;
        header->header &= 0xfc00;
        header->header |= type;
        return header;
    }

    /// <summary>scenarioTime of a client's next player update to the server.</summary>
    /// <remarks>MCX.EXE @ 0x007a9e5c. It has no symbol; the name is the port's.</remarks>
    float nextPlayerUpdateTime = 0.0f;
}

auto MultiPlayer::addWorldStateChunk(WorldStateChunk* chunk) -> int32_t
{
    if (numWorldStateChunks == 1024)
    {
        Fatal(0, " Multiplayer::addWorldStateChunk--Too many worldstate chunks ");
    }

    chunk->pack();
    WorldStateChunk check = EmptyWorldStateChunk();
    check.data = chunk->data;
    check.unpack();

    if (chunk->equalTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.addWorldStateChunk: WorldState Chunks don't match ");
    }

    WorldStateChunkTally[chunk->type]++;
    worldStateChunks[numWorldStateChunks] = chunk->data;
    numWorldStateChunks++;
    return numWorldStateChunks;
}

auto MultiPlayer::addMissionScriptMessageChunk(int32_t message, int32_t value) -> int32_t
{
    WorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.buildMissionScriptMessage(message, value);
    return addWorldStateChunk(&chunk);
}

auto MultiPlayer::addArtilleryChunk(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds)
    -> int32_t
{
    WorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.buildArtillery(commanderId, strikeType, location, seconds);
    return addWorldStateChunk(&chunk);
}

auto MultiPlayer::addMineChunk(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState,
                               int32_t explosionType) -> int32_t
{
    WorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.buildMine(tileRow, tileCol, teamId, mineState, explosionType);
    return addWorldStateChunk(&chunk);
}

auto MultiPlayer::addLightOnFireChunk(GameObject* object, int32_t seconds) -> int32_t
{
    WorldStateChunk chunk = EmptyWorldStateChunk();

    if (object != nullptr && object->getObjectType() != nullptr)
    {
        switch (object->objectClass)
        {
            case 0x10:
            case 0x15:
            case 0x18:
            case 0x1b:
            {
                chunk.buildTerrainFire(object, seconds);
                break;
            }

            default:
            {
                Fatal(0, " MultiPlayer.addLightOnFireChunk: bad fire victim ");
            }
        }
    }

    // Without a victim the empty chunk (a mine at cell -1, -1) is queued.
    return addWorldStateChunk(&chunk);
}

auto MultiPlayer::addPilotKillStat(Mover* mover, int32_t killType) -> int32_t
{
    WorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.buildPilotKillStat(mover->netRosterIndex, killType);
    return addWorldStateChunk(&chunk);
}

auto MultiPlayer::grabWorldStateChunks(uint32_t* chunks) -> int32_t
{
    for (int32_t i = 0; i < numWorldStateChunks; i++)
    {
        chunks[i] = worldStateChunks[i];
    }

    return numWorldStateChunks;
}

auto MultiPlayer::addWeaponHitChunk(WeaponHitChunk* chunk) -> int32_t
{
    if (numWeaponHitChunks == 1024)
    {
        Fatal(0, " MultiPlayer::addWeaponHitChunk--Too many weaponhit chunks ");
    }

    chunk->pack();
    WeaponHitChunk check = EmptyWeaponHitChunk();
    check.data = chunk->data;
    check.unpack();

    if (chunk->equalTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.addWeaponHitChunk: WeaponHit chunks don't match (save whchunk.dbg file) ");
    }

    weaponHitChunks[numWeaponHitChunks] = chunk->data;
    numWeaponHitChunks++;
    return numWeaponHitChunks;
}

auto MultiPlayer::addWeaponHitChunk(GameObject* target, _WeaponShotInfo* shotInfo, int hitFlag) -> int32_t
{
    WeaponHitChunk chunk = EmptyWeaponHitChunk();
    chunk.build(target, shotInfo, hitFlag);
    return addWeaponHitChunk(&chunk);
}

auto MultiPlayer::grabWeaponHitChunks(uint32_t* chunks, int32_t maxChunks) -> void
{
    if (numWeaponHitChunks <= 0)
    {
        return;
    }

    if (numWeaponHitChunks < maxChunks)
    {
        maxChunks = numWeaponHitChunks;
    }

    for (int32_t i = 0; i < maxChunks; i++)
    {
        chunks[i] = weaponHitChunks[i];
    }

    int32_t numLeft = numWeaponHitChunks - maxChunks;

    for (int32_t i = 0; i < numLeft; i++)
    {
        weaponHitChunks[i] = weaponHitChunks[maxChunks + i];
    }

    numWeaponHitChunks = numLeft;
}

auto MultiPlayer::connectIPX() -> int32_t
{
    if (sessionManager == nullptr)
    {
        return -1;
    }

    sessionManager->SetCurrentConnection(2);
    return 0;
}

auto MultiPlayer::connectInternet(char* ipAddress) -> int32_t
{
    if (sessionManager == nullptr)
    {
        return -1;
    }

    sessionManager->ConnectTCP(ipAddress);
    return 0;
}

auto MultiPlayer::createSession(char* newSessionName, char* newPlayerName, int32_t maxPlayers) -> int32_t
{
    if (newSessionName == nullptr)
    {
        newSessionName = sessionName;
    }

    if (newPlayerName == nullptr)
    {
        newPlayerName = playerName;
    }

    if (sessionManager == nullptr)
    {
        return -1;
    }

    FIDPSession session;
    session.SetName(newSessionName);
    session.sessionDesc.dwMaxPlayers = static_cast<uint32_t>(maxPlayers);

    if (sessionManager->HostSession(session, newPlayerName) != 0)
    {
        return -1;
    }

    isServer = 1;
    isHost = 1;
    serverID = sessionManager->myPlayer->id;
    hostID = serverID;
    char allPlayerGroup[] = "AllPlayerGroup";
    char innerSphereGroup[] = "InnerSphereGroup";
    char clanGroup[] = "ClanGroup";
    sessionManager->CreateGroup(&allPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
    sessionManager->CreateGroup(&innerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
    sessionManager->CreateGroup(&clanGroupID, clanGroup, nullptr, 0, 0);

    if (homeTeam == 0)
    {
        sessionManager->AddPlayerToGroup(innerSphereGroupID, 0);
        homeTeamGroupID = innerSphereGroupID;
        enemyTeamGroupID = clanGroupID;
    }
    else if (homeTeam == 1)
    {
        sessionManager->AddPlayerToGroup(clanGroupID, 0);
        homeTeamGroupID = clanGroupID;
        enemyTeamGroupID = innerSphereGroupID;
    }

    if (checkInId == -1)
    {
        checkInId = 0;
    }

    initUpdateFrequencies();
    return 0;
}

auto MultiPlayer::joinSession(char* newSessionName, char* newPlayerName) -> int32_t
{
    if (sessionManager == nullptr)
    {
        return -1;
    }

    if (newSessionName == nullptr)
    {
        newSessionName = sessionName;
    }

    if (newPlayerName == nullptr)
    {
        newPlayerName = playerName;
    }

    FLinkedList<FIDPSession>* sessions = sessionManager->GetSessions();
    int32_t numSessions = sessions->Size();

    if (numSessions == 0)
    {
        return -1;
    }

    sessions->current = sessions->head;

    for (int32_t i = 0; i < numSessions; i++)
    {
        FIDPSession* session = sessions->ReadAndNext();

        if (std::strcmp(session->sessionDesc.lpszSessionNameA, newSessionName) == 0)
        {
            if (sessionManager->JoinSession(&session->sessionDesc.guidInstance, newPlayerName) != 0)
            {
                return -2;
            }

            serverID = sessionManager->serverID;
            return 0;
        }
    }

    return -1;
}

auto MultiPlayer::processReceiveList() -> int32_t
{
    Assert(sessionManager != nullptr, 0, nullptr);
    sessionManager->ProcessMessages();
    return 0;
}

auto MultiPlayer::sendToHost(FIMessageHeader* msg, int32_t size, int guaranteed) -> int32_t
{
    if (sessionManager == nullptr)
    {
        return -1;
    }

    if (isHost != 0)
    {
        return -2;
    }

    if (guaranteed == 0)
    {
        sessionManager->SendMessageA(hostID, msg, static_cast<uint32_t>(size));
    }
    else
    {
        sessionManager->SendMessageToPlayerGuaranteed(hostID, static_cast<FIGuaranteedMessageHeader*>(msg),
                                                      static_cast<uint32_t>(size), 1);
    }

    return 0;
}

auto MultiPlayer::sendChat(uint32_t toID, char* text) -> int32_t
{
    if (sessionManager == nullptr)
    {
        return -1;
    }

    auto* chat = static_cast<MPChatMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_CHAT));
    std::strcpy(chat->text, text);
    chat->toAll = toID == 0 ? 1 : 0;
    const auto size = static_cast<uint32_t>(std::strlen(text) + 10);
    sessionManager->SendMessageToGroup(toID, chat, size);

    if (handleOwnMessages != 0)
    {
        uint32_t myID = sessionManager->myPlayer->id;
        FIDPMessage message(myID, 0x200);
        message.fromID = myID;
        message.SetMessageBuffer(chat, size);
        chatCallback(&message, nullptr);
    }

    return 0;
}

auto MultiPlayer::sendPlayerCheckIn() -> int32_t
{
    auto* checkIn = static_cast<MPPlayerCheckInMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_CHECK_IN));
    checkIn->checkInId = static_cast<int8_t>(checkInId);
    checkIn->homeTeam = static_cast<int8_t>(homeTeam);

    if (isServer == 0)
    {
        sessionManager->SendMessageToServerGuaranteed(checkIn, 10);
    }
    else
    {
        handleAppPlayerCheckIn(sessionManager->myPlayer->id, checkIn);
    }

    return 0;
}

auto MultiPlayer::sendPlayerSetup(uint32_t toID, uint32_t setupServerID, uint32_t setupInnerSphereGroupID,
                                  uint32_t setupClanGroupID, uint32_t unused1, uint32_t unused2) -> int32_t
{
    auto* setup = static_cast<MPPlayerSetupMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_SETUP));
    setup->allPlayerGroupID = allPlayerGroupID;
    setup->clanGroupID = setupClanGroupID;
    setup->innerSphereGroupID = setupInnerSphereGroupID;

    if (numPlayers() > 1)
    {
        if (toID == 0)
        {
            sessionManager->SendMessageToGroup(0, setup, 0x14);
        }
        else
        {
            sessionManager->SendMessageToPlayerGuaranteed(toID, setup, 0x14, 1);
        }
    }

    if (handleOwnMessages != 0)
    {
        handleAppPlayerSetup(sessionManager->myPlayer->id, msgBuffer);
    }

    return 0;
}

auto MultiPlayer::sendPlayerCheckInReceipt(int32_t playerCheckInId) -> int32_t
{
    Assert(isServer == 0, 0, nullptr);
    auto* receipt = static_cast<MPLongMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_CHECK_IN_RECEIPT));
    receipt->value = playerCheckInId;
    sessionManager->SendMessageToServerGuaranteed(receipt, 0xc);
    return 0;
}

auto MultiPlayer::sendStartPlanning() -> int32_t
{
    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_START_PLANNING);
    sessionManager->SendMessageToGroup(0, header, 8);

    if (handleOwnMessages != 0)
    {
        handleAppStartPlanning(sessionManager->myPlayer->id, header);

        for (int32_t i = 0; i < 6; i++)
        {
            playerCheckedIn[i] = 0;
        }
    }

    return 0;
}

auto MultiPlayer::sendReadyForBattle() -> int32_t
{
    // The type is first set to check-in (15), then to ready-for-battle (36).
    auto* ready = static_cast<MPPlayerCheckInMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_CHECK_IN));
    ready->header &= 0xfc00;
    ready->header |= MPMSG_READY_FOR_BATTLE;
    ready->checkInId = static_cast<int8_t>(checkInId);
    ready->homeTeam = static_cast<int8_t>(homeTeam);
    sessionManager->SendMessageToGroup(0, ready, 10);
    handleAppReadyForBattle(sessionManager->myPlayer->id, ready);
    return 0;
}

auto MultiPlayer::sendPrepareScenario() -> int32_t
{
    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_PREPARE_SCENARIO);
    sessionManager->SendMessageToGroup(0, header, 8);

    if (isHost != 0)
    {
        for (int32_t i = 0; i < 6; i++)
        {
            playerCheckedIn[i] = 0;
        }

        if (globalLogPtr != nullptr)
        {
            globalLogPtr->handlePrepareScenarioMessage();
        }

        prepareScenarioReceived = 1;
    }

    return 0;
}

namespace
{
    /// <summary>
    /// A machine that just became the server takes over the movers: each gets the server's control (setControl 2)
    /// and its pilot's current order is cleared.
    /// </summary>
    /// <remarks>Inline in the original (setServer and playerLeftGame).</remarks>
    void TakeOverMovers(MultiPlayer* multiPlayer)
    {
        for (int32_t i = 0; i < multiPlayer->numMovers; i++)
        {
            Mover* mover = multiPlayer->moverRoster[i];

            if (mover == nullptr)
            {
                continue;
            }

            int32_t result = mover->setControl(2, 0xffffffff, -1);

            if (result != 0)
            {
                Fatal(result, " MPlayer.setServer: unable to set control ");
            }

            MechWarrior* pilot = mover->getPilot();

            if (pilot != nullptr)
            {
                pilot->clearCurTacOrder(0, 0);
                pilot->orderState = 0;
            }
        }
    }
}

auto MultiPlayer::setServer(uint32_t newServerID) -> void
{
    serverID = newServerID;
    int32_t wasServer = isServer;

    if (sessionManager->isHost == 0)
    {
        isServer = 0;
    }
    else
    {
        isServer = 1;

        if (wasServer == 0)
        {
            FIDPPlayer* me = sessionManager->myPlayer;
            char format[256];
            char text[512];
            cLoadString(thisInstance, 0x378, format, 0xfe);
            std::snprintf(text, sizeof(text), format, me->name);
            handleOwnMessages = 1;
            sendChat(0, text);
            handleOwnMessages = 0;

            for (int32_t i = 0; i < 6; i++)
            {
                playerCheckedIn[i] = 0;
            }
        }
    }

    if (inMission != 0 && isServer != 0 && wasServer == 0)
    {
        TakeOverMovers(this);
    }
}

auto MultiPlayer::sendStartScenario(int32_t* playerValues, char* missionName) -> int32_t
{
    auto* start = static_cast<MPStartScenarioMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_START_SCENARIO));

    for (int32_t i = 0; i < MPlayer->numPlayers(); i++)
    {
        start->playerValues[i] = playerValues[i];
    }

    for (int32_t i = 0; i < MPlayer->numMovers; i++)
    {
        MechWarrior* pilot = MPlayer->moverRoster[i]->getPilot();
        Assert(pilot != nullptr, 0, " sendStartScenario: no pilot ");
        start->moverFlags[i] = 0;

        if (pilot->escapesThruEjection != 0)
        {
            start->moverFlags[i] |= 1;
        }
    }

    std::strcpy(start->missionName, missionName);
    sessionManager->SendMessageToGroup(0, start, static_cast<uint32_t>(std::strlen(missionName) + 0x39));

    if (handleOwnMessages != 0)
    {
        handleAppStartScenario(sessionManager->myPlayer->id, msgBuffer);
    }

    for (int32_t i = 0; i < 6; i++)
    {
        playerCheckedIn[i] = 0;
    }

    return 0;
}

auto MultiPlayer::sendEndScenario(uint32_t toID, int32_t result) -> int32_t
{
    auto* end = static_cast<MPLongMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_END_SCENARIO));
    end->value = result;
    sessionManager->SendMessageToGroup(0, end, 0xc);

    if (handleOwnMessages != 0)
    {
        handleAppEndScenario(sessionManager->myPlayer->id, msgBuffer);
    }

    return 0;
}

auto MultiPlayer::sendPlayerOrder(uint32_t toID, TacticalOrder* order, int queued, int32_t numMovers,
                                  int32_t* moverParts, int32_t numGroups, MoverGroup** groups, int fromGroup) -> int32_t
{
    auto* message = static_cast<MPPlayerOrderMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_ORDER));
    message->checkInId = static_cast<int8_t>(checkInId);

    // The client clears its own movers' order queues for a stop order; the server sends the orders themselves.
    for (int32_t i = 0; i < numMovers; i++)
    {
        auto* mover = static_cast<Mover*>(objectList->findObjectFromPart(moverParts[i]));

        if (mover == nullptr || mover == order->target)
        {
            continue;
        }

        order->setGroupFlag(mover->netPlayerId, 1);

        if (fromGroup == 0)
        {
            TacticalOrder clearOrder;
            clearOrder.init();
            clearOrder.setId(mover->getPilot());

            if (order->code == TACTICAL_ORDER_STOP)
            {
                mover->getPilot()->clearTacOrderQueue();
            }

            clearOrder.destroy();
        }
    }

    if (order->code == TACTICAL_ORDER_MOVETO_POINT || order->code == TACTICAL_ORDER_JUMPTO_POINT)
    {
        message->orderParam1 = std::bit_cast<uint32_t>(order->moveParams.wayPath.points[0]);
        message->orderParam2 = std::bit_cast<uint32_t>(order->moveParams.wayPath.points[1]);
    }

    order->pack(nullptr, nullptr);
    message->packedOrder[0] = order->data[0];
    message->packedOrder[1] = order->data[1];
    uint8_t flags = queued != 0 ? 1 : 0;

    if (fromGroup != 0)
    {
        flags |= 0x20;
    }

    for (int32_t i = 0; i < numGroups; i++)
    {
        flags |= static_cast<uint8_t>(1 << (groups[i]->getId() + 1));
        Mover* groupMovers[12];
        int32_t numGroupMovers = groups[i]->getMovers(groupMovers);

        for (int32_t j = 0; j < numGroupMovers; j++)
        {
            if (fromGroup == 0)
            {
                TacticalOrder clearOrder;
                clearOrder.init();
                clearOrder.setId(groupMovers[j]->getPilot());

                if (order->code == TACTICAL_ORDER_STOP)
                {
                    groupMovers[j]->getPilot()->clearTacOrderQueue();
                }

                clearOrder.destroy();
            }
        }
    }

    message->flags = flags;
    sessionManager->SendMessageToServerGuaranteed(message, 0x1a);
    return 0;
}

auto MultiPlayer::sendPlayerMoverGroup(uint32_t toID, int32_t groupId, int32_t numGroupMovers, Mover** movers,
                                       int32_t pointIndex) -> int32_t
{
    if (pointIndex < numGroupMovers && pointIndex > -1)
    {
        auto* message =
            static_cast<MPPlayerMoverGroupMessage*>(StartGuaranteedMessage(msgBuffer, MPMSG_PLAYER_MOVER_GROUP));
        message->checkInId = static_cast<int8_t>(checkInId);
        message->groupId = static_cast<int8_t>(groupId);
        uint16_t members = 0;

        for (int32_t i = 0; i < numGroupMovers; i++)
        {
            members |= static_cast<uint16_t>(1 << (movers[i]->netPlayerId & 0x1f));
        }

        message->members = static_cast<uint16_t>(members << 4 | movers[pointIndex]->netPlayerId);
        Assert(1, 0xc, " sendPlayerMoverGroup: msgSz too large! ");
        sessionManager->SendMessageToGroup(0, message, 0xc);
    }

    return 0;
}

auto MultiPlayer::sendPlayerArtillery(uint32_t toID, int32_t strikeType, vector_3d location, int32_t seconds) -> int32_t
{
    auto* message = reinterpret_cast<MPPlayerArtilleryMessage*>(msgBuffer);
    message->tagger.Clear();
    message->header = 0;
    message->header |= FIMSG_GUARANTEED;
    message->header &= 0xfc00;
    message->header |= MPMSG_PLAYER_ARTILLERY;
    ArtilleryChunk chunk;
    chunk.commanderId = -1;
    chunk.strikeType = -1;
    chunk.cellRow = -1;
    chunk.cellCol = -1;
    chunk.seconds = -1;
    chunk.data = 0;
    chunk.build(checkInId, strikeType, location, seconds);
    chunk.pack();
    ArtilleryChunk check;
    check.commanderId = -1;
    check.strikeType = -1;
    check.cellRow = -1;
    check.cellCol = -1;
    check.seconds = -1;
    check.data = chunk.data;
    check.unpack();

    if (chunk.equalTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.sendPlayerArtillery: Artillery chunks don't match ");
    }

    message->targetX = location.x;
    message->targetY = location.y;
    message->artilleryData = chunk.data;
    sessionManager->SendMessageToGroup(0, message, 0x14);
    return 0;
}

auto MultiPlayer::sendMoverUpdate(uint32_t toID) -> int32_t
{
    auto* header = reinterpret_cast<FIMessageHeader*>(msgBuffer);
    header->header = 0;
    header->header &= 0xfc00;
    header->header |= MPMSG_MOVER_UPDATE;
    *reinterpret_cast<uint16_t*>(msgBuffer + 2) = moverUpdateSequence;
    moverUpdateSequence++;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = moverRoster[i];
        Assert(mover != nullptr, 0, " SendMoverUpdate: No Mover ");
        mover->buildMoveChunk();
        std::memcpy(msgBuffer + 4 + i * 4, &mover->getMoveChunk()->data, 4);
    }

    uint8_t* statusChunks = msgBuffer + 4 + numMovers * 4;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = moverRoster[i];
        mover->buildStatusChunk();
        std::memcpy(statusChunks, &mover->getStatusChunk()->data, 4);
        statusChunks += 4;
    }

    uint8_t* orderIds = statusChunks;

    for (int32_t i = 0; i < numMovers; i++)
    {
        *orderIds = static_cast<uint8_t>(moverRoster[i]->getPilot()->curTacOrder.id);
        orderIds++;
    }

    auto size = static_cast<uint32_t>(numMovers + 4 + numMovers * 8);
    Assert(size < 0x1400, size, " sendMoverUpdate: msgSz too large! ");
    sessionManager->BroadcastMessage(header, size);
    return 0;
}

auto MultiPlayer::sendTurretUpdate(uint32_t toID) -> int32_t
{
    if (numTurrets == 0)
    {
        return 0;
    }

    auto* header = reinterpret_cast<FIMessageHeader*>(msgBuffer);
    header->header = 0;
    header->header &= 0xfc00;
    header->header |= MPMSG_TURRET_UPDATE;
    *reinterpret_cast<uint16_t*>(msgBuffer + 2) = turretUpdateSequence;
    turretUpdateSequence++;

    for (int32_t i = 0; i < numTurrets; i++)
    {
        Turret* turret = turretRoster[i];
        Assert(turret != nullptr, 0, " SendTurretUpdate: No Turret ");

        if (turret->getAwake() == 0 || turret->target == nullptr)
        {
            msgBuffer[4 + i] = 0xff;
        }
        else
        {
            GameObject* target = turret->target;
            ObjectClass targetClass = target->objectClass;

            // A mover's roster index, or (a building) its part id less 0x48, which handleAppTurretUpdate can't
            // tell from a roster index.
            if (targetClass == 2 || targetClass == 3 || targetClass == 4 || targetClass == 8)
            {
                msgBuffer[4 + i] = static_cast<uint8_t>(static_cast<Mover*>(target)->netRosterIndex);
            }
            else
            {
                msgBuffer[4 + i] = static_cast<uint8_t>(target->partId - 0x48);
            }
        }
    }

    auto size = static_cast<uint32_t>(numTurrets + 4);
    Assert(size < 0x1400, size, " sendTurretUpdate: msgSz too large! ");
    sessionManager->BroadcastMessage(header, size);
    return 0;
}

auto MultiPlayer::sendMoverWeaponFireUpdate(uint32_t toID) -> int32_t
{
    // As many messages as it takes: each holds at most 0x77 chunks.
    while (true)
    {
        FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_MOVER_WEAPON_FIRE_UPDATE);
        msgBuffer[8] = 0;
        msgBuffer[9] = static_cast<uint8_t>(numMovers);

        for (int32_t i = 0; i < numMovers; i++)
        {
            msgBuffer[10 + i] = 0;
        }

        int32_t numChunks = 0;

        for (int32_t i = 0; i < numMovers; i++)
        {
            int32_t grabbed = moverRoster[i]->grabWeaponFireChunks(
                0, reinterpret_cast<uint32_t*>(msgBuffer + 0x22 + numChunks * 4), 0x77 - numChunks);
            msgBuffer[10 + i] = static_cast<uint8_t>(grabbed);
            numChunks += grabbed;

            if (numChunks == 0x77)
            {
                break;
            }
        }

        if (numChunks < 1)
        {
            return 0;
        }

        auto size = static_cast<uint32_t>(numChunks * 4 + 0x22);
        Assert(size < 0x200, size, " sendMoverWeaponFireUpdate: msgSz too large! ");
        sessionManager->SendMessageToGroup(0, header, size);
    }
}

auto MultiPlayer::sendTurretWeaponFireUpdate(uint32_t toID) -> int32_t
{
    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_TURRET_WEAPON_FIRE_UPDATE);
    int32_t numFiring = 0;

    for (int32_t i = 0; i < numTurrets; i++)
    {
        if (turretRoster[i]->numWeaponFireChunks[0] > 0)
        {
            numFiring++;
        }
    }

    if (numFiring == 0)
    {
        return 0;
    }

    msgBuffer[8] = static_cast<uint8_t>(numFiring);
    int32_t entry = 0;
    int32_t numChunks = 0;

    for (int32_t i = 0; i < numTurrets; i++)
    {
        Turret* turret = turretRoster[i];
        int32_t turretChunks = turret->numWeaponFireChunks[0];

        if (turretChunks > 0)
        {
            turret->grabWeaponFireChunks(0, reinterpret_cast<uint32_t*>(msgBuffer + numChunks * 4 + numFiring + 9));
            numChunks += turretChunks;
            msgBuffer[9 + entry] = static_cast<uint8_t>(turretChunks + turret->netRosterIndex * 4);
            entry++;
            turret->clearWeaponFireChunks(0);
        }
    }

    if (numChunks > 0)
    {
        auto size = static_cast<uint32_t>(entry + 9 + numChunks * 4);
        Assert(size < 0x1400, size, " sendTurretWeaponFireUpdate: msgSz too large! ");
        sessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MultiPlayer::sendMoverCriticalHitUpdate(uint32_t toID) -> int32_t
{
    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_MOVER_CRITICAL_HIT_UPDATE);
    msgBuffer[8] = 0;
    msgBuffer[9] = static_cast<uint8_t>(numMovers);
    int32_t numBytes = 0;

    for (int32_t i = 0; i < static_cast<int8_t>(msgBuffer[9]); i++)
    {
        Mover* mover = moverRoster[static_cast<int8_t>(msgBuffer[8]) + i];
        auto numCriticalHits = static_cast<uint32_t>(mover->grabCriticalHitChunks(0, msgBuffer + numBytes + 0x3a));
        msgBuffer[10 + i] = static_cast<uint8_t>(numCriticalHits);
        Assert(numCriticalHits < 0x81, numCriticalHits, " sendMoverCritHits: bad numCH ");
        auto numRadio = static_cast<uint32_t>(mover->grabRadioChunks(0, msgBuffer + numBytes + numCriticalHits + 0x3a));
        msgBuffer[0x22 + i] = static_cast<uint8_t>(numRadio);
        Assert(numRadio < 8, numRadio, " sendMoverCritHits: bad numRDO ");
        numBytes += static_cast<int32_t>(numCriticalHits + numRadio);
        mover->clearCriticalHitChunks(0);
        mover->clearRadioChunks(0);
    }

    if (numBytes > 0)
    {
        auto size = static_cast<uint32_t>(numBytes + 0x3a);
        Assert(size < 0x1400, size, " sendMoverCriticalHitUpdate: msgSz too large! ");
        sessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MultiPlayer::sendWeaponHitUpdate(uint32_t toID) -> int32_t
{
    while (numWeaponHitChunks > 0)
    {
        FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_WEAPON_HIT_UPDATE);
        int32_t numChunks = numWeaponHitChunks;

        if (numChunks > 0x7d)
        {
            numChunks = 0x7d;
        }

        msgBuffer[8] = static_cast<uint8_t>(numChunks);
        grabWeaponHitChunks(reinterpret_cast<uint32_t*>(msgBuffer + 9), numChunks);
        auto size = static_cast<uint32_t>(msgBuffer[8] * 4 + 9);
        Assert(size < 0x200, size, " sendWeaponHitUpdate: msgSz too large! ");
        sessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MultiPlayer::sendWorldStateUpdate(uint32_t toID) -> int32_t
{
    if (numWorldStateChunks <= 0)
    {
        return 0;
    }

    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_WORLD_STATE_UPDATE);
    msgBuffer[8] = static_cast<uint8_t>(numWorldStateChunks);
    grabWorldStateChunks(reinterpret_cast<uint32_t*>(msgBuffer + 10));
    numWorldStateChunks = 0;
    auto size = static_cast<uint32_t>(msgBuffer[8] * 4 + 10);

    if (size > 0x1ff)
    {
        char text[256];
        std::snprintf(text, sizeof(text), " sendWorldStateUpdate: msgSz too large! [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d] ",
                      WorldStateChunkTally[0], WorldStateChunkTally[1], WorldStateChunkTally[2],
                      WorldStateChunkTally[3], WorldStateChunkTally[4], WorldStateChunkTally[5],
                      WorldStateChunkTally[6], WorldStateChunkTally[7], WorldStateChunkTally[8],
                      WorldStateChunkTally[9]);
        DebugMissionScriptMessages();
        Assert(0, size, text);
    }

    sessionManager->SendMessageToGroup(0, header, size);

    for (int32_t i = 0; i < 10; i++)
    {
        WorldStateChunkTally[i] = 0;
    }

    NumMissionScriptMessages = 0;
    return 0;
}

auto MultiPlayer::sendFile(char* fileName, char* directory) -> int32_t
{
    // The original passes the directory as the file name and the name as the directory (OB-107).
    sessionManager->BroadcastFile(directory, fileName, MultiPlayerFileSentCallback);
    return 0;
}

auto MultiPlayer::sendFileInquiry(char* fileName) -> int32_t
{
    FIGuaranteedMessageHeader* header = StartGuaranteedMessage(msgBuffer, MPMSG_FILE_INQUIRY);
    std::memcpy(msgBuffer + 8, fileName, std::strlen(fileName) + 1);
    sessionManager->SendMessageToGroup(allPlayerGroupID, header, static_cast<uint32_t>(std::strlen(fileName) + 9));
    return 0;
}

auto MultiPlayer::updateClients() -> int32_t
{
    if (inMission == 0)
    {
        return 0;
    }

    if (nextWorldStateUpdateTime < scenarioTime)
    {
        sendWorldStateUpdate(0);
        sendMoverWeaponFireUpdate(0);
        sendTurretWeaponFireUpdate(0);
        sendWeaponHitUpdate(0);
        sendMoverCriticalHitUpdate(0);
        nextWorldStateUpdateTime += worldStateUpdateFrequency;
    }

    if (nextMoverUpdateTime < scenarioTime)
    {
        sendMoverUpdate(0);
        nextMoverUpdateTime += moverUpdateFrequency;
    }

    if (nextTurretUpdateTime < scenarioTime)
    {
        sendTurretUpdate(0);
        nextTurretUpdateTime += turretUpdateFrequency;
    }

    return 0;
}

auto MultiPlayer::updateServer() -> int32_t
{
    if (nextPlayerUpdateTime < scenarioTime)
    {
        auto* header = reinterpret_cast<FIMessageHeader*>(msgBuffer);
        header->header = 0;
        header->header &= 0xfc00;
        header->header |= MPMSG_PLAYER_UPDATE;
        sessionManager->SendMessageToServer(header, 6);
        nextPlayerUpdateTime += 1.0f;
    }

    return 0;
}

auto MultiPlayer::playersInSession() -> int
{
    return sessionManager->GetPlayers(nullptr)->Size();
}

auto MultiPlayer::playersOnHomeTeam() -> FLinkedList<uint32_t>*
{
    FIDPGroup* group = sessionManager->GetGroup(homeTeamGroupID);
    return group != nullptr ? &group->players : nullptr;
}

auto MultiPlayer::playersOnEnemyTeam() -> FLinkedList<uint32_t>*
{
    FIDPGroup* group = sessionManager->GetGroup(enemyTeamGroupID);
    return group != nullptr ? &group->players : nullptr;
}

auto MultiPlayer::isMyTeammate(uint32_t playerID) -> int
{
    FLinkedList<uint32_t>* players = playersOnHomeTeam();

    if (players == nullptr)
    {
        return 0;
    }

    for (FLink<uint32_t>* link = players->head; link != nullptr; link = link->next)
    {
        if (*link->data == playerID)
        {
            return 1;
        }
    }

    return 0;
}

auto MultiPlayer::allPlayersCheckedIn() -> int
{
    FLinkedList<FIDPPlayer>* players = sessionManager->GetPlayers(nullptr);

    for (FLink<FIDPPlayer>* link = players->head; link != nullptr; link = link->next)
    {
        FIDPPlayer* player = link->data;

        if (player->hasPlayerNumber != 0)
        {
            Assert(player->playerNumber >= 0 && player->playerNumber <= 5, 0, "Invalid player number");

            if (playerCheckedIn[player->playerNumber] == 0)
            {
                return 0;
            }
        }
    }

    return 1;
}

auto MultiPlayer::switchServers() -> void
{
    if (isServer != 0)
    {
        sessionManager->SwitchServers();
        serverID = sessionManager->serverID;
        isServer = sessionManager->isHost != 0 ? 1 : 0;
    }
}

auto MultiPlayer::playerLeftGame(uint32_t playerID) -> void
{
    int onHomeTeam = 0;

    if (sessionManager->currentConnection != 0x10)
    {
        NumLANPlayers--;
    }

    if (EventsToMissionResultsScreen != 0)
    {
        return;
    }

    int32_t wasServer = isServer;

    if (playerID == hostID)
    {
        hostID = sessionManager->serverID;
        hostLeft = 1;
        isHost = sessionManager->isHost;
    }

    if (sessionManager->isHost != 0)
    {
        isServer = 1;
        FIDPPlayer* player = sessionManager->GetPlayer(playerID);

        if (player != nullptr)
        {
            char format[256];
            char serverText[256];
            char text[768];
            cLoadString(thisInstance, 0x382, format, 0xfe);
            cLoadString(thisInstance, 899, serverText, 0xfe);
            std::snprintf(text, sizeof(text), format, player->name);

            if (wasServer == 0)
            {
                std::strncat(text, serverText, sizeof(text) - std::strlen(text) - 1);
            }

            handleOwnMessages = 1;
            sendChat(0, text);
            handleOwnMessages = 0;
        }
    }

    if (playerID == serverID && inLogistics != 0 && prepareScenarioReceived != 0)
    {
        sendPlayerCheckIn();
    }

    serverID = sessionManager->serverID;

    if (launchedFromLobby == 0)
    {
        if (numPlayers() < 2)
        {
            mission->endScenarioRequested = 1;
        }
    }
    else if (numPlayers() == 2)
    {
        mission->endScenarioRequested = 1;
    }

    if (inMission != 0)
    {
        if (isServer != 0 && wasServer == 0)
        {
            TakeOverMovers(this);
        }

        return;
    }

    if (inLogistics == 0)
    {
        if (globalLogPtr->currentScreen == globalLogPtr->sessionScreen ||
            globalLogPtr->currentScreen == globalLogPtr->loadScreen)
        {
            char reason[256];
            char text[512];
            cLoadString(thisInstance, launchedFromLobby == 0 ? 0x35f : 0x365, reason, 0xfe);
            FIDPPlayer* player = MPlayer->sessionManager->GetPlayer(playerID);
            // Port fix: the player is already gone when DirectPlay reports it; the original printed its freed name.
            std::snprintf(text, sizeof(text), "%s %s", player != nullptr ? player->name : "", reason);
            ReusableDialog* dialog = globalLogPtr->messageDialog;
            dialog->setText(text);
            dialog->setTwoButton(0);
            dialog->callback = CancelBool;
            dialog->okButton->callback()->setExec(nullptr);
            char upArt[] = "bh_okay.tga";
            char downArt[] = "bg_okay.tga";
            dialog->okButton->setUpPicture(upArt);
            dialog->okButton->setDownPicture(downArt);
            dialog->okButton->disabled = 0;
            dialog->okButton->draw();
            dialog->timeout = 15000;
            dialog->activate();
        }
    }
    else
    {
        if (globalLogPtr != nullptr)
        {
            for (int32_t i = 0; i < 6; i++)
            {
                if (playerTeams[i].playerID == playerID)
                {
                    if (static_cast<uint32_t>(playerTeams[i].team) == homeTeamGroupID)
                    {
                        onHomeTeam = 1;
                    }

                    break;
                }
            }

            globalLogPtr->handleLostPlayer(playerID, onHomeTeam);
        }

        if (isServer != 0 && allPlayersCheckedIn() != 0 && prepareScenarioReceived != 0)
        {
            handleOwnMessages = 1;
            char missionName[] = "";
            sendStartScenario(playerCheckedIn, missionName);
            handleOwnMessages = 0;
        }
    }
}

auto MultiPlayer::leaveSession() -> void
{
    sessionManager->LeaveSession();
    initStartupParameters();
}

auto MultiPlayer::resetForNewGame() -> void
{
    if (homeTeamGroupID != 0)
    {
        sessionManager->RemovePlayerFromGroup(homeTeamGroupID, 0);
    }

    homeTeamGroupID = 0;
    enemyTeamGroupID = 0;
    numLocalMovers = 0;
    numMovers = 0;
    numTurrets = 0;
    handleOwnMessages = 0;
    hostLeft = 0;
    homeTeam = -1;

    for (int32_t i = 0; i < 6; i++)
    {
        playerCheckedIn[i] = 0;

        for (int32_t j = 0; j < 12; j++)
        {
            playerMoverRoster[i][j] = nullptr;
        }
    }

    inLogistics = 0;
    inMission = 0;
    prepareScenarioReceived = 0;
    scenarioResult = 0;

    for (int32_t i = 0; i < 24; i++)
    {
        moverRoster[i] = nullptr;
    }

    for (int32_t i = 0; i < 6; i++)
    {
        playerTeams[i].playerID = 0;
        playerTeams[i].team = 0;
    }

    nextMoverUpdateTime = 0.0f;
    moverUpdateSequence = 0;
    nextTurretUpdateTime = 0.0f;
    turretUpdateSequence = 0;
    nextWorldStateUpdateTime = 0.0f;
    unknown42C = 0;
    numWeaponHitChunks = 0;
    numWorldStateChunks = 0;
}

auto MultiPlayer::initStartupParameters() -> void
{
    allPlayerGroupID = 0;
    innerSphereGroupID = 0;
    clanGroupID = 0;
    serverID = 0;
    homeTeamGroupID = 0;
    enemyTeamGroupID = 0;
    hostID = 0;
    numLocalMovers = 0;
    numMovers = 0;
    numTurrets = 0;
    handleOwnMessages = 0;
    hostLeft = 0;
    isHost = 0;
    isServer = 0;
    checkInId = -1;
    NumLANPlayers = 1;
    homeTeam = -1;
    sessionName[0] = '\0';
    playerName[0] = '\0';

    for (int32_t i = 0; i < 6; i++)
    {
        playerCheckedIn[i] = 0;

        for (int32_t j = 0; j < 12; j++)
        {
            playerMoverRoster[i][j] = nullptr;
        }
    }

    inLogistics = 0;
    inMission = 0;
    prepareScenarioReceived = 0;
    scenarioResult = 0;

    for (int32_t i = 0; i < 24; i++)
    {
        moverRoster[i] = nullptr;
    }

    for (int32_t i = 0; i < 6; i++)
    {
        playerTeams[i].playerID = 0;
        playerTeams[i].team = 0;
    }

    nextMoverUpdateTime = 0.0f;
    moverUpdateSequence = 0;
    nextTurretUpdateTime = 0.0f;
    turretUpdateSequence = 0;
    nextWorldStateUpdateTime = 0.0f;
    unknown42C = 0;
    numWeaponHitChunks = 0;
    numWorldStateChunks = 0;
}

auto MultiPlayer::destroy() -> void
{
    if (sessionManager != nullptr)
    {
        // Port fix: the original only called destroy (vtable slot 1) and let DestroyLinkUpHeap drop the memory; the
        // port runs the destructor so the members (the mutex, the sockets) are released too.
        delete sessionManager;
        sessionManager = nullptr;
    }

    DestroyLinkUpHeap();

    if (msgBuffer != nullptr)
    {
        systemHeap->free(msgBuffer);
        msgBuffer = nullptr;
    }
}

auto ShowConnectStatus() -> void
{
    char text[256];
    cLoadString(thisInstance, 0x354, text, 0xfe);

    if (globalLogPtr != nullptr)
    {
        globalLogPtr->messageDialog->setText(text);
        ReusableDialog* dialog = globalLogPtr->messageDialog;

        if (dialog->okButton != nullptr)
        {
            dialog->okButton->ShowGUIWindow(0);
        }

        if (dialog->cancelButton != nullptr)
        {
            dialog->cancelButton->ShowGUIWindow(0);
        }

        globalLogPtr->messageDialog->callback = nullptr;
        globalLogPtr->messageDialog->activate();
        UpdateDisplay(0, 0, 0, 0, 0);
    }
}

auto DestroyConnectStatusWindow() -> void
{
    if (globalLogPtr != nullptr)
    {
        globalLogPtr->messageDialog->deactivate(0);
        ReusableDialog* dialog = globalLogPtr->messageDialog;

        if (dialog->okButton != nullptr)
        {
            dialog->okButton->ShowGUIWindow(1);
        }

        if (dialog->cancelButton != nullptr)
        {
            dialog->cancelButton->ShowGUIWindow(1);
        }

        dialog->setTwoButton(1);
    }
}

auto loadMultiplayerGameSystem(FitIniFile* file) -> int32_t
{
    int32_t result = file->seekBlock("Multiplayer");

    if (result == 0)
    {
        result = file->readIdFloat("WarpFactor", WarpFactor);

        if (result == 0)
        {
            result = 0;
        }
    }

    return result;
}

auto handleSysCreatePlayer(void* msg) -> void
{
    auto* create = static_cast<DPMSG_CREATEPLAYERORGROUP*>(msg);

    if (MPlayer->sessionManager->currentConnection != 0x10)
    {
        NumLANPlayers++;
    }

    if (MPlayer->isServer != 0)
    {
        MPlayer->sendPlayerSetup(create->dpId, MPlayer->serverID, MPlayer->innerSphereGroupID, MPlayer->clanGroupID, 0,
                                 0);
    }
}

auto handleSysAddPlayerToGroup(void* msg) -> void
{
    auto* add = static_cast<DPMSG_ADDPLAYERTOGROUP*>(msg);
    FIDPPlayer* player = MPlayer->sessionManager->GetPlayer(add->dpIdPlayer);

    if (player != nullptr && player->playerNumber >= 0 && player->playerNumber < 6)
    {
        MPlayer->playerTeams[player->playerNumber].playerID = add->dpIdPlayer;
        MPlayer->playerTeams[player->playerNumber].team = static_cast<int32_t>(add->dpIdGroup);
    }
}

auto handleAppChat(FIDPMessage* msg, void* data) -> void
{
    auto* chat = reinterpret_cast<MPChatMessage*>(msg->messageBuffer);
    FIDPPlayer* player = MPlayer->sessionManager->GetPlayer(msg->fromID);
    char line[256];
    // Port fix: an unknown sender (already gone) has no name; the original read through the null player.
    std::snprintf(line, sizeof(line), "%s: %s", player != nullptr ? player->name : "", chat->text);

    if (ABLi_getDebugger() != nullptr)
    {
        ABLi_getDebugger()->print(line);
    }
}

auto handleAppNewServer(uint32_t fromID, const void* msg) -> void
{
    MPlayer->setServer(static_cast<const MPLongMessage*>(msg)->value);
}

auto handleAppPlayerCheckIn(uint32_t fromID, const void* msg) -> void
{
    auto* checkIn = static_cast<const MPPlayerCheckInMessage*>(msg);

    if (globalLogPtr != nullptr && globalLogPtr->playerLights != nullptr)
    {
        globalLogPtr->playerLights->setPlayerStatus(fromID, 2);
    }

    if (MPlayer->isServer != 0)
    {
        Assert(checkIn->checkInId >= 0 && checkIn->checkInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        MPlayer->playerCheckedIn[checkIn->checkInId] = static_cast<int32_t>(fromID);

        if (MPlayer->allPlayersCheckedIn() != 0)
        {
            MPlayer->handleOwnMessages = 1;
            char missionName[] = "";
            MPlayer->sendStartScenario(MPlayer->playerCheckedIn, missionName);
            MPlayer->handleOwnMessages = 0;
        }
    }
}

auto handleAppPlayerSetup(uint32_t fromID, const void* msg) -> void
{
    auto* setup = static_cast<const MPPlayerSetupMessage*>(msg);
    SessionManager* sessionManager = MPlayer->sessionManager;

    if (MPlayer->checkInId == -1)
    {
        MPlayer->checkInId = sessionManager->myPlayer->playerNumber;
    }

    MPlayer->initUpdateFrequencies();
    MPlayer->hostID = sessionManager->serverID;
    MPlayer->serverID = MPlayer->hostID;
    MPlayer->allPlayerGroupID = setup->allPlayerGroupID;
    MPlayer->clanGroupID = setup->clanGroupID;
    MPlayer->innerSphereGroupID = setup->innerSphereGroupID;

    if (MPlayer->homeTeam == 0)
    {
        sessionManager->AddPlayerToGroup(MPlayer->innerSphereGroupID, 0);
        MPlayer->homeTeamGroupID = MPlayer->innerSphereGroupID;
        MPlayer->enemyTeamGroupID = MPlayer->clanGroupID;
    }
    else if (MPlayer->homeTeam == 1)
    {
        sessionManager->AddPlayerToGroup(MPlayer->clanGroupID, 0);
        MPlayer->homeTeamGroupID = MPlayer->clanGroupID;
        MPlayer->enemyTeamGroupID = MPlayer->innerSphereGroupID;
    }

    sessionManager->AddPlayerToGroup(MPlayer->allPlayerGroupID, 0);
}

auto handleAppPlayerCheckInReceipt(uint32_t fromID, const void* msg) -> void
{
    int32_t checkInId = static_cast<const MPLongMessage*>(msg)->value;
    Assert(checkInId >= 0 && checkInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
    MPlayer->playerCheckedIn[checkInId] = static_cast<int32_t>(fromID);

    if (MPlayer->allPlayersCheckedIn() != 0)
    {
        MPlayer->handleOwnMessages = 1;
        MPlayer->sendStartPlanning();
        MPlayer->handleOwnMessages = 0;
    }
}

auto handleAppStartPlanning(uint32_t fromID, const void* msg) -> void
{
    MPlayer->inLogistics = 1;
}

auto handleAppReadyForBattle(uint32_t fromID, const void* msg) -> void
{
    auto* ready = static_cast<const MPPlayerCheckInMessage*>(msg);

    if (globalLogPtr == nullptr)
    {
        return;
    }

    globalLogPtr->playerLights->setPlayerStatus(fromID, 2);

    if (MPlayer->isHost != 0)
    {
        Assert(ready->checkInId >= 0 && ready->checkInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        MPlayer->playerCheckedIn[ready->checkInId] = static_cast<int32_t>(fromID);

        if (MPlayer->allPlayersCheckedIn() != 0)
        {
            MPlayer->sendPrepareScenario();
        }
    }
}

auto handleAppJoinTeam(uint32_t fromID, const void* msg) -> void
{
    auto* join = static_cast<const MPJoinTeamMessage*>(msg);

    if (globalLogPtr->sessionScreen != nullptr)
    {
        globalLogPtr->sessionScreen->assignPlayer(join->playerID, join->team, join->slot, 0);
    }
}

auto handleAppRPUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* update = static_cast<const MPTwoLongMessage*>(msg);
    SessionScreen* sessionScreen = globalLogPtr->sessionScreen;

    if (sessionScreen != nullptr)
    {
        if (update->value2 == 1)
        {
            sessionScreen->setTeam1RP(update->value1);
        }

        if (update->value2 == 2)
        {
            sessionScreen->setTeam2RP(update->value1);
        }

        sessionScreen->draw();
    }
}

auto handleAppTechbaseChange(uint32_t fromID, const void* msg) -> void
{
    auto* change = static_cast<const MPTwoLongMessage*>(msg);

    if (globalLogPtr != nullptr && globalLogPtr->sessionScreen != nullptr)
    {
        globalLogPtr->sessionScreen->setTeamTechBase(static_cast<char>(change->value1),
                                                     static_cast<char>(change->value2));
    }
}

auto handleAppSwitchScreen(uint32_t fromID, const void* msg) -> void
{
    if (static_cast<const MPTwoLongMessage*>(msg)->value1 == 1)
    {
        globalLogPtr->setUpSessionScreen();
    }
}

auto handleAppStartScenario(uint32_t fromID, const void* msg) -> void
{
    auto* start = static_cast<const MPStartScenarioMessage*>(msg);

    for (uint32_t i = 1; i <= scenario->numParts; i++)
    {
        FIDPPlayer* player = MPlayer->sessionManager->GetPlayerNumber(scenario->parts[i].commanderId);

        if (player != nullptr)
        {
            auto* mover = static_cast<Mover*>(scenario->parts[i].object);
            mover->netOwnerID = player->id;

            if (mover->netName != nullptr)
            {
                std::strncpy(mover->netName, player->name, 0xff);
            }
        }
    }

    for (int32_t i = 0; i < MPlayer->numMovers; i++)
    {
        MPlayer->moverRoster[i]->getPilot()->escapesThruEjection = (start->moverFlags[i] & 1) != 0 ? 1 : 0;
    }

    MPlayer->inMission = 1;
    prepareScenarioReceived = 0;

    if (ABLi_getDebugger() != nullptr)
    {
        ABLi_getDebugger()->print(const_cast<char*>(start->missionName));
    }
}

auto handleAppEndScenario(uint32_t fromID, const void* msg) -> void
{
    MPlayer->scenarioResult = static_cast<const MPLongMessage*>(msg)->value;
}

auto handleAppPlayerOrder(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MPPlayerOrderMessage*>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    Commander* commander = CommanderTable[message->checkInId];

    if (MPlayer->isServer == 0)
    {
        return;
    }

    TacticalOrder order;
    order.init();
    order.data[0] = message->packedOrder[0];
    order.data[1] = message->packedOrder[1];
    order.unpack();
    vector_3d wayPoint;
    wayPoint.x = std::bit_cast<float>(message->orderParam1);
    wayPoint.y = std::bit_cast<float>(message->orderParam2);
    wayPoint.z = land->getTerrainElevation(wayPoint);
    order.setWayPoint(0, wayPoint);

    // A jump-attack (method 1) becomes a jump to the target's position, as Parser::SendTacOrder does locally.
    if (order.code == TACTICAL_ORDER_ATTACK_OBJECT && order.attackParams.method == 1)
    {
        order.code = TACTICAL_ORDER_JUMPTO_OBJECT;
        order.moveParams.wait = 0;
        order.moveParams.wayPath.mode[0] = 0;

        if (order.target != nullptr)
        {
            order.setWayPoint(0, order.target->getPosition());
        }
    }

    if (order.code == TACTICAL_ORDER_JUMPTO_OBJECT)
    {
        order.code = TACTICAL_ORDER_JUMPTO_POINT;
        Assert(order.target != nullptr, 0, " JumpToObject is NULL ");
        order.setWayPoint(0, order.target->getPosition());
    }

    Mover* movers[12];
    Mover* point = nullptr;
    int32_t numMovers = order.getGroup(message->checkInId, movers, &point, 0);
    vector_3d jumpGoals[72];
    int32_t numGoals = 0;

    if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
    {
        numGoals = numMovers;

        for (int32_t groupId = 0; groupId < 4; groupId++)
        {
            if ((message->flags & (2 << groupId)) != 0)
            {
                numGoals += commander->getGroup(groupId)->numMovers;
            }
        }

        CalcJumpGoals(order.getWayPoint(0), numGoals, jumpGoals, order.getJumpTarget());
    }

    int fromGroup = (message->flags & 0x20) != 0 ? 1 : 0;

    if (numMovers > 0)
    {
        // The client sends its sort flag in bit 0, but the server reads bit 4, group 3's bit (OB-105).
        int sortMovers = (message->flags & 0x10) != 0 ? 1 : 0;

        if (sortMovers != 0)
        {
            SortMoverList(numMovers, movers, order.getWayPoint(0));
        }

        for (int32_t i = 0; i < numMovers; i++)
        {
            Mover* mover = movers[i];

            if (mover == nullptr || mover == order.target)
            {
                continue;
            }

            if (sortMovers != 0)
            {
                order.selectionIndex = mover->selectionIndex;
            }

            if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
            {
                order.setWayPoint(0, jumpGoals[i]);
            }

            mover->handleTacticalOrder(order, 1, fromGroup);
        }
    }

    int32_t goalIndex = numMovers;

    for (int32_t groupId = 0; groupId < 4; groupId++)
    {
        if ((message->flags & (2 << groupId)) == 0)
        {
            continue;
        }

        vector_3d* destinations = nullptr;

        if (order.code == TACTICAL_ORDER_JUMPTO_POINT)
        {
            destinations = &jumpGoals[goalIndex];
            goalIndex += commander->getGroup(groupId)->numMovers;
        }

        commander->getGroup(groupId)->handleTacticalOrder(order, 1, destinations, fromGroup);
    }

    order.destroy();
}

auto handleAppPlayerMoverGroup(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MPPlayerMoverGroupMessage*>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    Mover** playerMovers = MPlayer->playerMoverRoster[message->checkInId];
    uint32_t memberBits = message->members >> 4;
    Mover* movers[12];
    int32_t numMovers = 0;

    for (int32_t i = 0; i < 12; i++)
    {
        if ((memberBits & 1) != 0)
        {
            movers[numMovers++] = playerMovers[i];
        }

        memberBits >>= 1;
    }

    int32_t pointIndex = message->members & 0xf;
    int32_t i = 0;

    for (; i < numMovers; i++)
    {
        if (playerMovers[pointIndex] == movers[i])
        {
            pointIndex = i;
            break;
        }
    }

    Assert(i < numMovers, 0, " handleAppPlayerMoverGroup: bad pointMover ");
    CommanderTable[message->checkInId]->setGroup(message->groupId, numMovers, movers, pointIndex);
}

auto handleAppPlayerArtillery(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MPPlayerArtilleryMessage*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer == 0)
    {
        return;
    }

    ArtilleryChunk chunk;
    chunk.commanderId = -1;
    chunk.strikeType = -1;
    chunk.cellRow = -1;
    chunk.cellCol = -1;
    chunk.seconds = -1;
    chunk.data = message->artilleryData;
    chunk.unpack();
    vector_3d location;
    location.x = message->targetX;
    location.y = message->targetY;
    location.z = land->getTerrainElevation(location);
    CallArtillery(chunk.commanderId, chunk.strikeType, location, chunk.seconds, 0);
}

auto handleAppMoverUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);
    uint16_t sequence = *reinterpret_cast<const uint16_t*>(bytes + 2);

    if (!InMultiplayerMission() || sequence < MPlayer->moverUpdateSequence)
    {
        return;
    }

    if (MPlayer->isServer == 0)
    {
        const uint8_t* chunks = bytes + 4;

        for (int32_t i = 0; i < MPlayer->numMovers; i++)
        {
            Mover* mover = MPlayer->moverRoster[i];
            Assert(mover != nullptr, 0, " handleAppMoveUpdate: No Mover ");
            mover->handleMoveChunk(*reinterpret_cast<const uint32_t*>(chunks + i * 4));
        }

        const uint8_t* statusChunks = chunks + MPlayer->numMovers * 4;

        for (int32_t i = 0; i < MPlayer->numMovers; i++)
        {
            MPlayer->moverRoster[i]->handleStatusChunk(sequence - MPlayer->moverUpdateSequence,
                                                       *reinterpret_cast<const uint32_t*>(statusChunks));
            statusChunks += 4;
        }

        const uint8_t* orderIds = statusChunks;

        for (int32_t i = 0; i < MPlayer->numMovers; i++)
        {
            MPlayer->moverRoster[i]->getPilot()->updateClientOrderQueue(*orderIds);
            orderIds++;
        }
    }

    MPlayer->moverUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

auto handleAppTurretUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);
    uint16_t sequence = *reinterpret_cast<const uint16_t*>(bytes + 2);

    if (!InMultiplayerMission() || sequence < MPlayer->turretUpdateSequence)
    {
        return;
    }

    if (MPlayer->isServer == 0)
    {
        for (int32_t i = 0; i < MPlayer->numTurrets; i++)
        {
            Turret* turret = MPlayer->turretRoster[i];
            Assert(turret != nullptr, 0, " handleAppTurretUpdate: No Turret ");
            auto targetIndex = static_cast<int8_t>(bytes[4 + i]);

            // -1 is no target; 0-127 a mover of the roster. The original's third case (a part id from
            // 0x80248 down) can't be reached with a signed byte, so building targets are misread (OB-106).
            if (targetIndex < 0)
            {
                turret->target = nullptr;
            }
            else
            {
                turret->target = MPlayer->moverRoster[targetIndex];
            }
        }
    }

    MPlayer->turretUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

auto handleAppMoverWeaponFireUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer != 0)
    {
        return;
    }

    auto firstMover = static_cast<int8_t>(bytes[8]);
    auto numMovers = static_cast<int8_t>(bytes[9]);
    int32_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        uint8_t numChunks = bytes[10 + i];
        MPlayer->moverRoster[firstMover + i]->addWeaponFireChunks(
            1, reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(bytes + 0x22 + chunkIndex * 4)), numChunks);
        chunkIndex += numChunks;
    }
}

auto handleAppTurretWeaponFireUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer != 0)
    {
        return;
    }

    auto numTurrets = static_cast<int8_t>(bytes[8]);
    int32_t chunkIndex = 0;

    for (int32_t i = 0; i < numTurrets; i++)
    {
        // Each turret's byte: its roster index (bits 2-7) and its number of chunks (bits 0-1).
        uint8_t entry = bytes[9 + i];
        int32_t numChunks = entry & 3;
        MPlayer->turretRoster[entry >> 2]->addWeaponFireChunks(
            1, reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(bytes + 9 + numTurrets + chunkIndex * 4)), numChunks);
        chunkIndex += numChunks;
    }
}

auto handleAppMoverCriticalHitUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer != 0)
    {
        return;
    }

    auto firstMover = static_cast<int8_t>(bytes[8]);
    auto numMovers = static_cast<int8_t>(bytes[9]);
    int32_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = MPlayer->moverRoster[firstMover + i];
        uint32_t numCriticalHits = bytes[10 + i];
        Assert(numCriticalHits < 0x81, numCriticalHits, " handleAppMoverCritHits: bad numCH ");

        if (numCriticalHits != 0)
        {
            mover->addCriticalHitChunks(1, const_cast<uint8_t*>(bytes + 0x3a + chunkIndex),
                                        static_cast<int32_t>(numCriticalHits));
            chunkIndex += static_cast<int32_t>(numCriticalHits);
        }

        uint32_t numRadio = bytes[0x22 + i];
        Assert(numRadio < 8, numRadio, " handleAppMoverCritHits: bad numRDO ");

        if (numRadio != 0)
        {
            mover->addRadioChunks(1, const_cast<uint8_t*>(bytes + 0x3a + chunkIndex), static_cast<int32_t>(numRadio));
            chunkIndex += static_cast<int32_t>(numRadio);
        }
    }
}

auto handleAppWeaponHitUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer != 0)
    {
        return;
    }

    for (int32_t i = 0; i < bytes[8]; i++)
    {
        WeaponHitChunk chunk;
        chunk.targetType = 0;
        chunk.targetId = 0;
        chunk.targetBlockOrTrainNumber = 0;
        chunk.targetVertexOrCarNumber = 0;
        chunk.targetItemNumber = 0;
        chunk.cause = 0;
        chunk.damage = 0.0f;
        chunk.hitLocation = -1;
        chunk.entryAngle = 0;
        chunk.refit = 0;
        std::memcpy(&chunk.data, bytes + 9 + i * 4, sizeof(chunk.data));
        chunk.unpack();

        if (chunk.refit == 0)
        {
            _WeaponShotInfo shotInfo;
            shotInfo.attacker = nullptr;
            shotInfo.masterId = chunk.cause;
            shotInfo.damage = chunk.damage;
            shotInfo.hitLocation = chunk.hitLocation;
            shotInfo.entryAngle = HitEntryAngles[chunk.entryAngle];

            if (chunk.targetType == 0)
            {
                MPlayer->moverRoster[chunk.targetId]->handleWeaponHit(&shotInfo, 0);
            }
            else if (chunk.targetType == 1 || chunk.targetType == 2)
            {
                static_cast<GameObject*>(objectList->findObjectFromPart(chunk.targetId))->handleWeaponHit(&shotInfo, 0);
            }
            else
            {
                Fatal(0, " Multiplayer.handleAppWeaponHitUpdate: bad targetType for weaponHit ");
            }
        }
        else
        {
            Mover* target = nullptr;

            if (chunk.targetType == 0)
            {
                target = MPlayer->moverRoster[chunk.targetId];
            }
            else if (chunk.targetType == 1 || chunk.targetType == 2)
            {
                target = static_cast<Mover*>(objectList->findObjectFromPart(chunk.targetId));
            }
            else
            {
                Fatal(0, " Multiplayer.handleAppWeaponHitUpdate: bad targetType for refit ");
            }

            float pointsUsed = 0.0f;
            DoRefit(target, chunk.damage, pointsUsed, chunk.damage == -6.0f ? 1 : 0);
        }
    }
}

auto handleAppWorldStateUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->isServer != 0)
    {
        return;
    }

    for (int32_t i = 0; i < bytes[8]; i++)
    {
        WorldStateChunk chunk;
        chunk.type = 0;
        chunk.tileRow = -1;
        chunk.tileCol = -1;
        chunk.objectWID = 0;
        chunk.blockNum = 0;
        chunk.vertexNum = 0;
        chunk.item = 0;
        chunk.param1 = 0;
        chunk.param2 = 0;
        std::memcpy(&chunk.data, bytes + 10 + i * 4, sizeof(chunk.data));
        chunk.unpack();

        switch (chunk.type)
        {
            case WSCHUNK_MINE:
            {
                int32_t tileR = chunk.tileRow / 3;
                int32_t tileC = chunk.tileCol / 3;
                int32_t layout = chunk.param2;

                if (layout > 3)
                {
                    layout = 3;
                }

                MapTile& tile = GameMap->map[tileR * GameMap->width + tileC];

                if (chunk.param1 == 1)
                {
                    tile.overlay = (tile.overlay & 0xffff9fff) | (layout << 13);
                }
                else
                {
                    tile.overlay = (tile.overlay & 0xffffe7ff) | (layout << 11);
                }

                if (chunk.param2 > 3)
                {
                    vector_3d position;
                    mapCellToWorldPos(chunk.tileRow, chunk.tileCol, position);
                    position.z = land->getTerrainElevation(position);

                    if (chunk.param2 == 4)
                    {
                        CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
                    }
                    else if (chunk.param2 == 5)
                    {
                        CreateExplosion(MineExplosion, position, MineSplashDamage,
                                        MineSplashRange * worldUnitsPerMeter);
                    }
                }

                break;
            }

            case WSCHUNK_TERRAIN_FIRE:
            {
                auto* object = static_cast<GameObject*>(objectList->findObjectFromPart(chunk.objectWID));

                if (object != nullptr && object->getObjectType() != nullptr)
                {
                    switch (object->objectClass)
                    {
                        case 0x10:
                        {
                            static_cast<Building*>(object)->lightOnFire(static_cast<float>(chunk.param1));
                            break;
                        }

                        case 0x15:
                        {
                            static_cast<Tree*>(object)->lightOnFire(static_cast<float>(chunk.param1));
                            break;
                        }

                        case 0x18:
                        {
                            static_cast<TerrainObject*>(object)->lightOnFire(static_cast<float>(chunk.param1));
                            break;
                        }

                        case 0x1b:
                        {
                            static_cast<TreeBuilding*>(object)->lightOnFire(static_cast<float>(chunk.param1));
                            break;
                        }

                        default:
                        {
                            Fatal(0, " handleAppWorldStateUpdate: bad fire victim ");
                        }
                    }
                }

                break;
            }

            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            {
                vector_3d location;
                mapCellToWorldPos(chunk.tileRow, chunk.tileCol, location);
                location.z = land->getTerrainElevation(location);
                CallArtillery(chunk.type - WSCHUNK_ARTILLERY, chunk.param1, location, chunk.param2, 0);
                break;
            }

            case WSCHUNK_MISSION_SCRIPT_MESSAGE:
            {
                scenario->handleMultiplayMessage(chunk.param1, chunk.param2);
                break;
            }

            case WSCHUNK_PILOT_KILL_STAT:
            {
                Mover* mover = MPlayer->moverRoster[chunk.param1];

                if (mover != nullptr)
                {
                    mover->getPilot()->numKilled[chunk.param2][1]++;

                    if (mover->getPilot()->onHomeTeam() != 0)
                    {
                        mover->getPilot()->radioMessage(KillRadioMessages[chunk.param2], 0);
                    }
                }

                break;
            }

            default:
            {
                Fatal(0, " Multiplayer.handleAppWorldStateUpdate: bad worldStateType ");
            }
        }
    }
}

auto handleAppPlayerUpdate(uint32_t fromID, const void* msg) -> void
{
    Assert(MPlayer->isServer != 0, 0, " Sending player update to non-server ");
}

auto getCheckSum(char* fileName) -> uint32_t
{
    File file;

    if (file.open(fileName, READ, 0x32) != 0)
    {
        return 0;
    }

    uint32_t length = file.getLength();
    auto* contents = static_cast<uint8_t*>(std::malloc(length));

    if (contents == nullptr)
    {
        return 0;
    }

    file.read(contents, length);
    // Only the file's first four bytes: the "checksum" tells little more than the file's existence.
    uint32_t checkSum = 0;
    std::memcpy(&checkSum, contents, length < 4 ? length : 4);
    std::free(contents);
    file.close();
    return checkSum;
}

auto handleAppFileInquiry(uint32_t fromID, const void* msg) -> void
{
    auto* inquiry = static_cast<const MPFileNameMessage*>(msg);
    // The inquiry's name starts at +0x8 (sendFileInquiry writes it there), not at MPFileNameMessage's +0xc.
    const char* fileName = reinterpret_cast<const char*>(inquiry) + 8;
    uint32_t checkSum = getCheckSum(const_cast<char*>(fileName));
    auto* report = reinterpret_cast<MPFileNameMessage*>(MPlayer->msgBuffer);

    for (int32_t i = 0; i < 6; i++)
    {
        report->tagger.sendCount[i] = 0;
    }

    report->header = 0;
    report->header |= FIMSG_GUARANTEED;
    report->header &= 0xfc00;
    report->header |= MPMSG_FILE_REPORT;
    report->unused = static_cast<int32_t>(checkSum);
    std::memcpy(report->fileName, fileName, std::strlen(fileName) + 1);
    MPlayer->sessionManager->SendMessageToPlayerGuaranteed(
        fromID, report, static_cast<uint32_t>(std::strlen(report->fileName) + 0xd), 1);
}

auto handleAppFileReport(uint32_t fromID, const void* msg) -> void
{
    int haveFile = static_cast<const MPLongMessage*>(msg)->value != 0 ? 1 : 0;

    if (globalLogPtr->sessionScreen != nullptr)
    {
        globalLogPtr->sessionScreen->fileReport(fromID, haveFile);
    }
}

auto handleAppLoadMission(uint32_t fromID, const void* msg) -> void
{
    if (globalLogPtr->sessionScreen != nullptr)
    {
        globalLogPtr->sessionScreen->loadMission(
            const_cast<char*>(static_cast<const MPFileNameMessage*>(msg)->fileName));
    }
}

auto handleAppStart(uint32_t fromID, const void* msg) -> void
{
    application->RemoveTimer(globalLogPtr->sessionScreen, 0);
    MPlayer->sessionManager->SendLatencyInfo();
    soundSystem->playBettySample(0x19);
    globalLogPtr->initializeMultiplayer();
    char extension[] = ".MPK";
    globalLogPtr->loadCampaign(const_cast<char*>(static_cast<const MPFileNameMessage*>(msg)->fileName), extension, 0,
                               0);
    globalLogPtr->setUpBriefingScreen(0);
}

auto LostConnectionDialogExit() -> void
{
    if (launchedFromLobby == 0)
    {
        if (MPlayer != nullptr)
        {
            delete MPlayer;
            MPlayer = nullptr;
        }
    }
    else
    {
        killTheGame();
    }
}

auto handleLocalPlayerRemoved(uint32_t fromID, const void* msg) -> void
{
    LastConnectionType = static_cast<uint32_t>(MPlayer->sessionManager->currentConnection);

    if (prepareScenarioReceived == 0 && MPlayer->inMission == 0)
    {
        MPlayer->inLogistics = 0;
        globalLogPtr->destroyMultiplayer();

        if (globalLogPtr->currentScreen != globalLogPtr->mainScreen)
        {
            whackTimer = 1;
        }

        globalLogPtr->currentScreen->ShowGUIWindow(0);
        globalLogPtr->currentScreen = globalLogPtr->mainScreen;
        globalLogPtr->logisticsState = 1;
        globalLogPtr->showLogScreen(1, 1);
        MPlayer->leaveSession();
    }
    else
    {
        MPlayer->inMission = 0;
        MPlayer->leaveSession();
        mission->EndScenario();
    }

    char text[512];
    cLoadString(thisInstance, 0x369, text, 0xfe);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(text);
    dialog->setTwoButton(0);
    dialog->callback = nullptr;
    dialog->okButton->callback()->setExec(LostConnectionDialogExit);
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    dialog->okButton->setUpPicture(upArt);
    dialog->okButton->setDownPicture(downArt);
    dialog->okButton->disabled = 0;
    dialog->okButton->draw();
    dialog->activate();
}

auto MultiPlayerSystemCallback(FIDPMessage* msg, void* data) -> void
{
    auto* system = reinterpret_cast<DPMSG_GENERIC*>(msg->messageBuffer);

    switch (system->dwType)
    {
        case DPSYS_CREATEPLAYERORGROUP:
        {
            if (reinterpret_cast<DPMSG_CREATEPLAYERORGROUP*>(system)->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                handleSysCreatePlayer(system);
            }

            break;
        }

        case DPSYS_DESTROYPLAYERORGROUP:
        {
            auto* destroy = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(system);

            if (destroy->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                MPlayer->playerLeftGame(destroy->dpId);
            }

            break;
        }

        case DPSYS_ADDPLAYERTOGROUP:
        {
            handleSysAddPlayerToGroup(system);
            break;
        }

        case DPSYS_SESSIONLOST:
        {
            if (scenario == nullptr || scenario->startingUp == 0)
            {
                handleLocalPlayerRemoved(msg->fromID, nullptr);
            }
            else
            {
                BadSessionCounter++;

                if (BadSessionCounter > 10)
                {
                    killTheGame();
                }
            }

            break;
        }
    }
}

auto MultiPlayerApplicationCallback(FIDPMessage* msg, void* data) -> void
{
    const uint32_t fromID = msg->fromID;
    const void* message = msg->messageBuffer;

    switch (*reinterpret_cast<const uint16_t*>(message) & FIMSG_TYPE_MASK)
    {
        case MPMSG_NEW_SERVER:
        {
            handleAppNewServer(fromID, message);
            break;
        }

        case MPMSG_PLAYER_REMOVED:
        {
            handleLocalPlayerRemoved(fromID, message);
            break;
        }

        case MPMSG_LATENCY:
        {
            if (MPlayer->sessionManager->ReadyToChooseServer() != 0)
            {
                MPlayer->switchServers();
            }

            break;
        }

        case MPMSG_CHAT:
        {
            MPlayer->chatCallback(msg, nullptr);
            break;
        }

        case MPMSG_PLAYER_CHECK_IN:
        {
            handleAppPlayerCheckIn(fromID, message);
            break;
        }

        case MPMSG_PLAYER_SETUP:
        {
            handleAppPlayerSetup(fromID, message);
            break;
        }

        case MPMSG_PLAYER_CHECK_IN_RECEIPT:
        {
            handleAppPlayerCheckInReceipt(fromID, message);
            break;
        }

        case MPMSG_START_PLANNING:
        {
            handleAppStartPlanning(fromID, message);
            break;
        }

        case MPMSG_START_SCENARIO:
        {
            handleAppStartScenario(fromID, message);
            break;
        }

        case MPMSG_END_SCENARIO:
        {
            handleAppEndScenario(fromID, message);
            break;
        }

        case MPMSG_PLAYER_ORDER:
        {
            handleAppPlayerOrder(fromID, message);
            break;
        }

        case MPMSG_PLAYER_MOVER_GROUP:
        {
            handleAppPlayerMoverGroup(fromID, message);
            break;
        }

        case MPMSG_PLAYER_ARTILLERY:
        {
            handleAppPlayerArtillery(fromID, message);
            break;
        }

        case MPMSG_MOVER_UPDATE:
        {
            handleAppMoverUpdate(fromID, message);
            break;
        }

        case MPMSG_TURRET_UPDATE:
        {
            handleAppTurretUpdate(fromID, message);
            break;
        }

        case MPMSG_MOVER_WEAPON_FIRE_UPDATE:
        {
            handleAppMoverWeaponFireUpdate(fromID, message);
            break;
        }

        case MPMSG_TURRET_WEAPON_FIRE_UPDATE:
        {
            handleAppTurretWeaponFireUpdate(fromID, message);
            break;
        }

        case MPMSG_MOVER_CRITICAL_HIT_UPDATE:
        {
            handleAppMoverCriticalHitUpdate(fromID, message);
            break;
        }

        case MPMSG_WEAPON_HIT_UPDATE:
        {
            handleAppWeaponHitUpdate(fromID, message);
            break;
        }

        case MPMSG_WORLD_STATE_UPDATE:
        {
            handleAppWorldStateUpdate(fromID, message);
            break;
        }

        case MPMSG_DEPLOY_FORCE:
        {
            if (globalLogPtr != nullptr)
            {
                globalLogPtr->HandleDeployForceMessage(fromID, message);
            }

            break;
        }

        case MPMSG_REMOVE_FORCE:
        {
            if (globalLogPtr != nullptr)
            {
                globalLogPtr->HandleRemoveForceMessage(fromID, message);
            }

            break;
        }

        case MPMSG_PLAYER_UPDATE:
        {
            handleAppPlayerUpdate(fromID, message);
            break;
        }

        case MPMSG_PREPARE_SCENARIO:
        {
            for (int32_t i = 0; i < 6; i++)
            {
                MPlayer->playerCheckedIn[i] = 0;
            }

            if (globalLogPtr != nullptr)
            {
                globalLogPtr->handlePrepareScenarioMessage();
            }

            prepareScenarioReceived = 1;
            break;
        }

        case MPMSG_READY_FOR_BATTLE:
        {
            handleAppReadyForBattle(fromID, message);
            break;
        }

        case MPMSG_FILE_INQUIRY:
        {
            handleAppFileInquiry(fromID, message);
            break;
        }

        case MPMSG_FILE_REPORT:
        {
            handleAppFileReport(fromID, message);
            break;
        }

        case MPMSG_LOAD_MISSION:
        {
            handleAppLoadMission(fromID, message);
            break;
        }

        case MPMSG_START:
        {
            handleAppStart(fromID, message);
            break;
        }

        case MPMSG_JOIN_TEAM:
        {
            handleAppJoinTeam(fromID, message);
            break;
        }

        case MPMSG_SWITCH_SCREEN:
        {
            handleAppSwitchScreen(fromID, message);
            break;
        }

        case MPMSG_RP_UPDATE:
        {
            handleAppRPUpdate(fromID, message);
            break;
        }

        case MPMSG_TECHBASE_CHANGE:
        {
            handleAppTechbaseChange(fromID, message);
            break;
        }

        case MPMSG_SESSION_CHECK_IN:
        {
            int32_t playerNumber = MPlayer->sessionManager->GetPlayer(fromID)->playerNumber;
            Assert(playerNumber >= 0 && playerNumber <= 5, 0, "PNUM BAD");
            MPlayer->playerSessionCheckIn[playerNumber] = 1;

            if (globalLogPtr != nullptr && globalLogPtr->sessionScreen != nullptr)
            {
                globalLogPtr->sessionScreen->someoneCheckedIn();
            }

            break;
        }
    }
}

auto MultiPlayerFileSentCallback(char* fileName, void* data) -> void
{
}

auto MultiPlayerFileReceivedCallback(char* fileName, void* data) -> void
{
}
