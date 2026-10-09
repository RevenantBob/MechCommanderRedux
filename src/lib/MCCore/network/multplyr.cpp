#include "stdafx.h"
#include "network/multplyr.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRoutines.h"
#include "abl/MCAblRuntime.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCRefit.h"
#include "ai/MCTacticalOrder.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "iface/parser.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
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
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCForces.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCBigGameObject.h"
#include "object/MCMoverGroup.h"
#include "object/MCBattleMech.h"
#include "object/MCBattleMechType.h"
#include "object/MCMechGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCTerrainObject.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCTree.h"
#include "object/MCTreeType.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"
#include "object/MCMechWarrior.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"
#include "object/MCWeaponHitChunk.h"
#include "object/MCWeaponShotInfo.h"

MCMultiPlayer* MPlayer = nullptr;
int IsMPlayerGame = 0;
int32_t BadSessionCounter = 0;
int32_t NumLanPlayers = 1;
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
    int32_t PrepareScenarioReceived = 0;

    /// <summary>The entry angle of a weapon hit (WeaponHitChunk::entryAngle) in degrees.</summary>
    const float HitEntryAngles[4] = {0.0f, 180.0f, -90.0f, 90.0f};

    /// <summary>The radio message a pilot sends for a kill, by kill kind.</summary>
    /// <remarks>
    /// The original's table has six entries; kinds 6 and 7 (allowed by BuildPilotKillStat)
    /// read on into the next data, 0 and the bytes of a string.
    /// </remarks>
    const int32_t KillRadioMessages[8] = {19, 19, 19, 19, 18, 17, 0, 0};

    /// <summary>Whether a mission is running a multiplayer game with company (every in-mission handler's check).</summary>
    bool InMultiplayerMission()
    {
        return Scenario != nullptr && EventsToMissionResultsScreen == 0 && MPlayer->NumPlayers() > 1;
    }
}

auto MCWorldStateChunk::BuildMine(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState,
                                  int32_t explosionType) -> void
{
    Type = WSCHUNK_MINE;
    this->TileRow = static_cast<int16_t>(tileRow);
    this->TileCol = static_cast<int16_t>(tileCol);
    Param1 = teamId;
    Assert(teamId >= 0 && teamId <= 2, teamId, " WorldStateChunk.buildMine: bad team id ");
    Param2 = mineState;

    if (Param2 == 3)
    {
        Param2 += explosionType;
    }

    Assert(mineState >= 0 && mineState <= 3, mineState, " WorldStateChunk.buildMine: bad mine state ");
    Assert(explosionType >= 0 && explosionType <= 2, explosionType,
           " WorldStateChunk.buildMine: bad mine explosionType ");
    Data = 0;
}

auto MCWorldStateChunk::BuildTerrainFire(MCGameObject* object, int32_t seconds) -> void
{
    Type = WSCHUNK_TERRAIN_FIRE;
    ObjectWid = object->PartId;
    BlockNum = (ObjectWid - 0x1000) / 0xc80;
    int32_t rest = ObjectWid - 0x1000 - BlockNum * 0xc80;
    VertexNum = rest / 8;
    Item = static_cast<int8_t>(rest - VertexNum * 8);
    Param1 = seconds;
    Assert(seconds >= 0 && seconds <= 255, seconds, " WorldStateChunk.buildTerrainFire: bad seconds ");
    Data = 0;
}

auto MCWorldStateChunk::BuildArtillery(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds)
    -> void
{
    Type = static_cast<int8_t>(commanderId + WSCHUNK_ARTILLERY);
    Assert(commanderId >= 0 && commanderId <= 5, commanderId, " WorldStateChunk.BuildArtillery: bad commander id ");
    Param1 = strikeType;
    Assert(strikeType >= 0 && strikeType <= 7, strikeType, " WorldStateChunk.BuildArtillery: bad artillery type ");
    Param2 = seconds;
    Assert(seconds >= -1 && seconds <= 30, seconds, " WorldStateChunk.BuildArtillery: bad seconds ");
    int32_t cellRow = 0;
    int32_t cellCol = 0;
    WorldCoordToMapCell(location, cellRow, cellCol);
    TileRow = static_cast<int16_t>(cellRow);
    TileCol = static_cast<int16_t>(cellCol);
    Data = 0;
}

auto MCWorldStateChunk::BuildMissionScriptMessage(int32_t message, int32_t value) -> void
{
    Type = WSCHUNK_MISSION_SCRIPT_MESSAGE;
    Param1 = message;
    Assert(message >= 0 && message <= 255, message, " WorldState.BuildMissionScriptMessage: bad message Code ");
    Param2 = value;
    Assert(value >= -32000 && value <= 32000, value, " WorldState.BuildMissionScriptMessage: bad message Param ");
    Data = 0;
}

auto MCWorldStateChunk::BuildPilotKillStat(int32_t moverIndex, int32_t killType) -> void
{
    Type = WSCHUNK_PILOT_KILL_STAT;
    Param1 = moverIndex;
    Assert(moverIndex >= 0 && moverIndex < MPlayer->NumMovers, moverIndex,
           " WorldState.BuildPilotKillStat: bad mover index ");
    Param2 = killType;
    Assert(killType >= 0 && killType <= 7, killType, " WorldState.BuildPilotKillStat: bad vehicle class ");
    Data = 0;
}

auto MCWorldStateChunk::Pack() -> void
{
    Data = 0;

    switch (Type)
    {
        case WSCHUNK_MINE:
        {
            Data |= Param1;
            Data <<= 3;
            Data |= Param2;
            Data <<= 10;
            Data |= TileRow;
            Data <<= 10;
            Data |= TileCol;
            Data <<= 4;
            break;
        }

        case WSCHUNK_TERRAIN_FIRE:
        {
            Data |= Param1;
            Data <<= 8;
            Data |= BlockNum;
            Data <<= 9;
            Data |= VertexNum;
            Data <<= 3;
            Data |= Item;
            Data <<= 4;
            break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        {
            Data |= Param1;
            Data <<= 5;
            Data |= Param2 + 1;
            Data <<= 10;
            Data |= TileRow;
            Data <<= 10;
            Data |= TileCol;
            Data <<= 4;
            break;
        }

        case WSCHUNK_MISSION_SCRIPT_MESSAGE:
        {
            Data |= Param2 + 32000;
            Data <<= 8;
            Data |= Param1;
            Data <<= 4;
            break;
        }

        case WSCHUNK_PILOT_KILL_STAT:
        {
            Data |= Param1;
            Data <<= 3;
            Data |= Param2;
            Data <<= 4;
            break;
        }
    }

    Data |= Type;
}

auto MCWorldStateChunk::Unpack() -> void
{
    uint32_t packed = Data;
    Type = static_cast<int8_t>(packed & 0xf);
    uint32_t rest = packed >> 4;

    switch (Type)
    {
        case WSCHUNK_MINE:
        {
            TileCol = static_cast<int16_t>(rest & 0x3ff);
            TileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
            Param2 = (packed >> 24) & 7;
            Param1 = (packed >> 27) & 1;
            break;
        }

        case WSCHUNK_TERRAIN_FIRE:
        {
            Item = static_cast<int8_t>(rest & 7);
            VertexNum = (packed >> 7) & 0x1ff;
            BlockNum = (packed >> 16) & 0xff;
            ObjectWid = VertexNum * 8 + 0x1000 + BlockNum * 0xc80 + Item;
            // Only 6 of the 8 bits pack wrote come back (OB-104): a fire of 64 seconds or more arrives shorter.
            Param1 = (packed >> 24) & 0x3f;
            break;
        }

        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        {
            TileCol = static_cast<int16_t>(rest & 0x3ff);
            TileRow = static_cast<int16_t>((packed >> 14) & 0x3ff);
            Param2 = static_cast<int32_t>((packed >> 24) & 0x1f) - 1;
            Param1 = packed >> 29;
            break;
        }

        case WSCHUNK_MISSION_SCRIPT_MESSAGE:
        {
            Param1 = rest & 0xff;
            Param2 = static_cast<int32_t>((packed >> 12) & 0xffff) - 32000;
            break;
        }

        case WSCHUNK_PILOT_KILL_STAT:
        {
            Param2 = rest & 7;
            Param1 = (packed >> 7) & 0x1f;
            break;
        }

        default:
        {
            Fatal(0, " WorldStateChunk.unpack: bad type ");
        }
    }
}

auto MCWorldStateChunk::EqualTo(MCWorldStateChunk* chunk) -> int
{
    return Type == chunk->Type && TileRow == chunk->TileRow && TileCol == chunk->TileCol &&
           ObjectWid == chunk->ObjectWid && BlockNum == chunk->BlockNum && VertexNum == chunk->VertexNum &&
           Item == chunk->Item && Param1 == chunk->Param1 && Param2 == chunk->Param2;
}

auto MCMultiPlayer::Init() -> void
{
    if (GlobalLogPtr != nullptr)
    {
        GlobalLogPtr->CurrentMission = -1;
    }

    SessionManager = nullptr;
    MsgBuffer = nullptr;
    BadSessionCounter = 0;
    ChatCallback = HandleAppChat;
    InitStartupParameters();
    IsMPlayerGame = 0;
}

auto MCMultiPlayer::Init(int32_t heapSize, int32_t maxMessageSize, int32_t maxMessages) -> int32_t
{
    SessionManager = MCSessionManager::GetGlobalPointer(nullptr);

    if (SessionManager == nullptr)
    {
        InitLinkUpBlocks();
        SessionManager = new MCSessionManager(MultiPlayerAppGuid);
        Assert(SessionManager != nullptr, 0, "Error creating sessionManager");
    }

    // Port: the original passed _getcwd. The port's files are relative to the data directory, so "." stands for it.
    char homeDirectory[] = ".";
    SessionManager->SetHomeDirectory(homeDirectory);
    SessionManager->ApplicationCallback = MultiPlayerApplicationCallback;
    SessionManager->ApplicationCallbackData = nullptr;
    SessionManager->SystemCallback = MultiPlayerSystemCallback;
    SessionManager->SystemCallbackData = nullptr;
    SessionManager->FileReceivedCallback = MultiPlayerFileReceivedCallback;
    SessionManager->FileReceivedCallbackData = nullptr;

    if (MsgBuffer != nullptr)
    {
        delete[] MsgBuffer;
    }

    MsgBuffer = new uint8_t[0x1400]{};
    return 0;
}

auto MCMultiPlayer::InitUpdateFrequencies() -> void
{
    MCFitIniFile prefsFile;
    MoverUpdateFrequency = -1.0f;
    TurretUpdateFrequency = -1.0f;
    WorldStateUpdateFrequency = -1.0f;
    int32_t result = prefsFile.Open("prefs.cfg");
    Assert(result == 0, 0, "Could not open prefs.cfg");

    if (prefsFile.SeekBlock("Multiplayer") == 0)
    {
        if (prefsFile.ReadIdFloat("MoverUpdateFrequency", MoverUpdateFrequency) != 0)
        {
            MoverUpdateFrequency = -1.0f;
        }

        if (prefsFile.ReadIdFloat("TurretUpdateFrequency", TurretUpdateFrequency) != 0)
        {
            TurretUpdateFrequency = -1.0f;
        }

        if (prefsFile.ReadIdFloat("WorldStateUpdateFrequency", WorldStateUpdateFrequency) != 0)
        {
            WorldStateUpdateFrequency = -1.0f;
        }
    }

    prefsFile.Close();

    // A period outside 0-5 seconds (or missing) takes the default: shorter ones on a LAN (IPX or TCP/IP) outside a
    // lobby.
    auto outOfRange = [](float frequency) { return frequency < 0.0f || frequency > 5.0f; };
    const int32_t connection = SessionManager->CurrentConnection;

    if ((connection == 2 || connection == 1) && LaunchedFromLobby == 0)
    {
        if (outOfRange(MoverUpdateFrequency))
        {
            MoverUpdateFrequency = 0.2f;
        }

        if (outOfRange(WorldStateUpdateFrequency))
        {
            WorldStateUpdateFrequency = 0.33f;
        }

        if (outOfRange(TurretUpdateFrequency))
        {
            TurretUpdateFrequency = 0.5f;
        }
    }
    else
    {
        if (outOfRange(MoverUpdateFrequency))
        {
            MoverUpdateFrequency = 0.33f;
        }

        if (outOfRange(WorldStateUpdateFrequency))
        {
            WorldStateUpdateFrequency = 0.75f;
        }

        if (outOfRange(TurretUpdateFrequency))
        {
            TurretUpdateFrequency = 1.0f;
        }
    }

    MultiplayBroadcastFrequencies[0] = MoverUpdateFrequency;
    MultiplayBroadcastFrequencies[1] = TurretUpdateFrequency;
    MultiplayBroadcastFrequencies[2] = WorldStateUpdateFrequency;
}

auto MCMultiPlayer::Init(MCFitIniFile* file) -> int32_t
{
    int32_t result = file->ReadIdBoolean("Server", IsServer);
    Assert(result == 0, 0, " could not find Multiplayer:Server ");

    if (LaunchedFromLobby == 0)
    {
        result = file->ReadIdLong("NumPlayers", NumLanPlayers);
        Assert(result == 0, 0, " could not find Multiplayer:NumPlayers ");
    }

    if (StartupPakFile == nullptr)
    {
        result = file->ReadIdLong("CheckInId", CheckInId);

        if (result != 0)
        {
            CheckInId = IsServer == 0 ? -1 : 0;
        }
    }
    else
    {
        CheckInId = IsServer == 0 ? -1 : 0;
    }

    result = file->ReadIdLong("HomeTeam", HomeTeam);
    Assert(result == 0, 0, " could not find Multiplayer:HomeTeam ");
    uint32_t connectResult = SessionManager->SetupLobbyConnection(nullptr, nullptr);

    if (connectResult == 0)
    {
        IsServer = SessionManager->IsHost;

        if (IsServer != 0)
        {
            PlayerCheckedIn[CheckInId] = static_cast<int32_t>(SessionManager->MyPlayer->Id);
            char allPlayerGroup[] = "AllPlayerGroup";
            char innerSphereGroup[] = "InnerSphereGroup";
            char clanGroup[] = "ClanGroup";
            SessionManager->CreateGroup(&AllPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
            SessionManager->CreateGroup(&InnerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
            SessionManager->CreateGroup(&ClanGroupID, clanGroup, nullptr, 0, 0);
            result = file->ReadIdFloat("MoverUpdateFrequency", MoverUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:MoverUpdateFrequency ");
            result = file->ReadIdFloat("WorldStateUpdateFrequency", WorldStateUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:WorldStateUpdateFrequency ");
        }
    }
    else if (connectResult == DPERR_NOTLOBBIED)
    {
        if (IsServer != 0)
        {
            result = file->ReadIdFloat("MoverUpdateFrequency", MoverUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:MoverUpdateFrequency ");
            result = file->ReadIdFloat("WorldStateUpdateFrequency", WorldStateUpdateFrequency);
            Assert(result == 0, 0, " could not find Multiplayer:WorldStateUpdateFrequency ");
        }

        uint32_t protocol = 0;
        result = file->ReadIdULong("Protocol", protocol);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find protocol in Multiplayer info file");
        result = file->ReadIdString("SessionName", SessionName, 0x4f);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find Multiplayer:SessionName ");
        result = file->ReadIdString("PlayerName", PlayerName, 0x4f);
        Assert(result == 0, static_cast<uint32_t>(result), " could not find Multiplayer:PlayerName ");
        Assert((SessionManager->AvailableProtocols & protocol) != 0, 0,
               "Connection protocol specified is not available");
        result = SessionManager->SetCurrentConnection(static_cast<int>(protocol));
        Assert(result == 0, 0, "Could not connect.");
    }

    return static_cast<int32_t>(connectResult);
}

auto MCMultiPlayer::NumPlayers() -> int32_t
{
    if (LaunchedFromLobby != 0)
    {
        return SessionManager->GetPlayers(nullptr)->Size();
    }

    return NumLanPlayers;
}

auto MCMultiPlayer::SetupLobbyGame() -> int32_t
{
    uint32_t result = SessionManager->SetupLobbyConnection(ShowConnectStatus, DestroyConnectStatusWindow);

    if (result == 0)
    {
        IsMPlayerGame = 1;
        IsServer = SessionManager->IsHost;
        IsHost = IsServer;

        if (SessionManager->CurrentConnection != 0x10)
        {
            NumLanPlayers = SessionManager->GetPlayers(nullptr)->Size();
        }

        if (IsServer != 0)
        {
            char allPlayerGroup[] = "AllPlayerGroup";
            char innerSphereGroup[] = "InnerSphereGroup";
            char clanGroup[] = "ClanGroup";
            SessionManager->CreateGroup(&AllPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
            SessionManager->CreateGroup(&InnerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
            SessionManager->CreateGroup(&ClanGroupID, clanGroup, nullptr, 0, 0);
            HandleOwnMessages = 1;
            SendPlayerSetup(0, MPlayer->ServerID, MPlayer->InnerSphereGroupID, MPlayer->ClanGroupID, 0, 0);
            HandleOwnMessages = 0;
        }
    }

    return static_cast<int32_t>(result);
}

auto MCMultiPlayer::AddToLocalMovers(MCMover* mover) -> void
{
    if (NumLocalMovers == 12)
    {
        Fatal(0, " Too many local movers for network ");
    }

    LocalMovers[NumLocalMovers] = mover;
    mover->NetPlayerId = NumLocalMovers;
    NumLocalMovers++;
}

auto MCMultiPlayer::AddToMoverRoster(MCMover* mover) -> void
{
    if (NumMovers == 24)
    {
        Fatal(0, " Too many movers for multiplay ");
    }

    MoverRoster[NumMovers] = mover;
    mover->NetRosterIndex = NumMovers;
    NumMovers++;
}

auto MCMultiPlayer::AddToPlayerMoverRoster(int32_t playerNumber, MCMover* mover) -> void
{
    int32_t i = 0;

    for (; i < 12; i++)
    {
        if (PlayerMoverRoster[playerNumber][i] == nullptr)
        {
            PlayerMoverRoster[playerNumber][i] = mover;
            break;
        }
    }

    Assert(i < 12, 0, " MultiPlayer.addToPlayerMoverRoster: Too many local movers ");
}

auto MCMultiPlayer::AddToTurretRoster(MCTurret* turret) -> void
{
    if (NumTurrets == 64)
    {
        Fatal(0, " Too many turrets for multiplay ");
    }

    TurretRoster[NumTurrets] = turret;
    turret->NetRosterIndex = NumTurrets;
    NumTurrets++;
}

namespace
{
    /// <summary>An empty chunk as the original's locals start (type 0, cells -1, the rest 0).</summary>
    MCWorldStateChunk EmptyWorldStateChunk()
    {
        MCWorldStateChunk chunk;
        chunk.Type = 0;
        chunk.TileRow = -1;
        chunk.TileCol = -1;
        chunk.ObjectWid = 0;
        chunk.BlockNum = 0;
        chunk.VertexNum = 0;
        chunk.Item = 0;
        chunk.Param1 = 0;
        chunk.Param2 = 0;
        chunk.Data = 0;
        return chunk;
    }

    /// <summary>An empty weapon-hit chunk as the original's locals start (hit location -1, the rest 0).</summary>
    MCWeaponHitChunk EmptyWeaponHitChunk()
    {
        MCWeaponHitChunk chunk;
        chunk.TargetType = 0;
        chunk.TargetId = 0;
        chunk.TargetBlockOrTrainNumber = 0;
        chunk.TargetVertexOrCarNumber = 0;
        chunk.TargetItemNumber = 0;
        chunk.Cause = 0;
        chunk.Damage = 0.0f;
        chunk.HitLocation = -1;
        chunk.EntryAngle = 0;
        chunk.Refit = 0;
        chunk.Data = 0;
        return chunk;
    }

    /// <summary>Starts a guaranteed message of <paramref name="type"/> in <paramref name="buffer"/>.</summary>
    /// <remarks>Inline in the original: every send clears the tagger and sets the header word the same way.</remarks>
    MCFIGuaranteedMessageHeader* StartGuaranteedMessage(uint8_t* buffer, uint16_t type)
    {
        auto* header = reinterpret_cast<MCFIGuaranteedMessageHeader*>(buffer);

        for (int32_t i = 0; i < 6; i++)
        {
            header->Tagger.SendCount[i] = 0;
        }

        header->Header = 0;
        header->Header |= FIMSG_GUARANTEED;
        header->Header &= 0xfc00;
        header->Header |= type;
        return header;
    }

    /// <summary>scenarioTime of a client's next player update to the server.</summary>
    float NextPlayerUpdateTime = 0.0f;
}

auto MCMultiPlayer::AddWorldStateChunk(MCWorldStateChunk* chunk) -> int32_t
{
    if (NumWorldStateChunks == 1024)
    {
        Fatal(0, " Multiplayer::addWorldStateChunk--Too many worldstate chunks ");
    }

    chunk->Pack();
    MCWorldStateChunk check = EmptyWorldStateChunk();
    check.Data = chunk->Data;
    check.Unpack();

    if (chunk->EqualTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.addWorldStateChunk: WorldState Chunks don't match ");
    }

    WorldStateChunkTally[chunk->Type]++;
    WorldStateChunks[NumWorldStateChunks] = chunk->Data;
    NumWorldStateChunks++;
    return NumWorldStateChunks;
}

auto MCMultiPlayer::AddMissionScriptMessageChunk(int32_t message, int32_t value) -> int32_t
{
    MCWorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.BuildMissionScriptMessage(message, value);
    return AddWorldStateChunk(&chunk);
}

auto MCMultiPlayer::AddArtilleryChunk(int32_t commanderId, int32_t strikeType, MCVector3D location, int32_t seconds)
    -> int32_t
{
    MCWorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.BuildArtillery(commanderId, strikeType, location, seconds);
    return AddWorldStateChunk(&chunk);
}

auto MCMultiPlayer::AddMineChunk(int32_t tileRow, int32_t tileCol, int32_t teamId, int32_t mineState,
                                 int32_t explosionType) -> int32_t
{
    MCWorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.BuildMine(tileRow, tileCol, teamId, mineState, explosionType);
    return AddWorldStateChunk(&chunk);
}

auto MCMultiPlayer::AddLightOnFireChunk(MCGameObject* object, int32_t seconds) -> int32_t
{
    MCWorldStateChunk chunk = EmptyWorldStateChunk();

    if (object != nullptr && object->GetObjectType() != nullptr)
    {
        switch (object->ObjectClass)
        {
            case MCObjectClass::Building:
            case MCObjectClass::Tree:
            case MCObjectClass::MiscTerrainObject:
            case MCObjectClass::TreeBuilding:
            {
                chunk.BuildTerrainFire(object, seconds);
                break;
            }

            default:
            {
                Fatal(0, " MultiPlayer.addLightOnFireChunk: bad fire victim ");
            }
        }
    }

    // Without a victim the empty chunk (a mine at cell -1, -1) is queued.
    return AddWorldStateChunk(&chunk);
}

auto MCMultiPlayer::AddPilotKillStat(MCMover* mover, int32_t killType) -> int32_t
{
    MCWorldStateChunk chunk = EmptyWorldStateChunk();
    chunk.BuildPilotKillStat(mover->NetRosterIndex, killType);
    return AddWorldStateChunk(&chunk);
}

auto MCMultiPlayer::GrabWorldStateChunks(uint32_t* chunks) -> int32_t
{
    for (int32_t i = 0; i < NumWorldStateChunks; i++)
    {
        chunks[i] = WorldStateChunks[i];
    }

    return NumWorldStateChunks;
}

auto MCMultiPlayer::AddWeaponHitChunk(MCWeaponHitChunk* chunk) -> int32_t
{
    if (NumWeaponHitChunks == 1024)
    {
        Fatal(0, " MultiPlayer::addWeaponHitChunk--Too many weaponhit chunks ");
    }

    chunk->Pack();
    MCWeaponHitChunk check = EmptyWeaponHitChunk();
    check.Data = chunk->Data;
    check.Unpack();

    if (chunk->EqualTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.addWeaponHitChunk: WeaponHit chunks don't match (save whchunk.dbg file) ");
    }

    WeaponHitChunks[NumWeaponHitChunks] = chunk->Data;
    NumWeaponHitChunks++;
    return NumWeaponHitChunks;
}

auto MCMultiPlayer::AddWeaponHitChunk(MCGameObject* target, MCWeaponShotInfo* shotInfo, int hitFlag) -> int32_t
{
    MCWeaponHitChunk chunk = EmptyWeaponHitChunk();
    chunk.Build(target, shotInfo, hitFlag);
    return AddWeaponHitChunk(&chunk);
}

auto MCMultiPlayer::GrabWeaponHitChunks(uint32_t* chunks, int32_t maxChunks) -> void
{
    if (NumWeaponHitChunks <= 0)
    {
        return;
    }

    if (NumWeaponHitChunks < maxChunks)
    {
        maxChunks = NumWeaponHitChunks;
    }

    for (int32_t i = 0; i < maxChunks; i++)
    {
        chunks[i] = WeaponHitChunks[i];
    }

    int32_t numLeft = NumWeaponHitChunks - maxChunks;

    for (int32_t i = 0; i < numLeft; i++)
    {
        WeaponHitChunks[i] = WeaponHitChunks[maxChunks + i];
    }

    NumWeaponHitChunks = numLeft;
}

auto MCMultiPlayer::ConnectIpx() -> int32_t
{
    if (SessionManager == nullptr)
    {
        return -1;
    }

    SessionManager->SetCurrentConnection(2);
    return 0;
}

auto MCMultiPlayer::ConnectInternet(char* ipAddress) -> int32_t
{
    if (SessionManager == nullptr)
    {
        return -1;
    }

    SessionManager->ConnectTcp(ipAddress);
    return 0;
}

auto MCMultiPlayer::CreateSession(char* newSessionName, char* newPlayerName, int32_t maxPlayers) -> int32_t
{
    if (newSessionName == nullptr)
    {
        newSessionName = SessionName;
    }

    if (newPlayerName == nullptr)
    {
        newPlayerName = PlayerName;
    }

    if (SessionManager == nullptr)
    {
        return -1;
    }

    MCFidpSession session;
    session.SetName(newSessionName);
    session.SessionDesc.dwMaxPlayers = static_cast<uint32_t>(maxPlayers);

    if (SessionManager->HostSession(session, newPlayerName) != 0)
    {
        return -1;
    }

    IsServer = 1;
    IsHost = 1;
    ServerID = SessionManager->MyPlayer->Id;
    HostID = ServerID;
    char allPlayerGroup[] = "AllPlayerGroup";
    char innerSphereGroup[] = "InnerSphereGroup";
    char clanGroup[] = "ClanGroup";
    SessionManager->CreateGroup(&AllPlayerGroupID, allPlayerGroup, nullptr, 0, 0);
    SessionManager->CreateGroup(&InnerSphereGroupID, innerSphereGroup, nullptr, 0, 0);
    SessionManager->CreateGroup(&ClanGroupID, clanGroup, nullptr, 0, 0);

    if (HomeTeam == 0)
    {
        SessionManager->AddPlayerToGroup(InnerSphereGroupID, 0);
        HomeTeamGroupID = InnerSphereGroupID;
        EnemyTeamGroupID = ClanGroupID;
    }
    else if (HomeTeam == 1)
    {
        SessionManager->AddPlayerToGroup(ClanGroupID, 0);
        HomeTeamGroupID = ClanGroupID;
        EnemyTeamGroupID = InnerSphereGroupID;
    }

    if (CheckInId == -1)
    {
        CheckInId = 0;
    }

    InitUpdateFrequencies();
    return 0;
}

auto MCMultiPlayer::JoinSession(char* newSessionName, char* newPlayerName) -> int32_t
{
    if (SessionManager == nullptr)
    {
        return -1;
    }

    if (newSessionName == nullptr)
    {
        newSessionName = SessionName;
    }

    if (newPlayerName == nullptr)
    {
        newPlayerName = PlayerName;
    }

    MCFLinkedList<MCFidpSession>* sessions = SessionManager->GetSessions();
    int32_t numSessions = sessions->Size();

    if (numSessions == 0)
    {
        return -1;
    }

    sessions->Current = sessions->HeadLink;

    for (int32_t i = 0; i < numSessions; i++)
    {
        MCFidpSession* session = sessions->ReadAndNext();

        if (std::strcmp(session->SessionDesc.lpszSessionNameA, newSessionName) == 0)
        {
            if (SessionManager->JoinSession(&session->SessionDesc.guidInstance, newPlayerName) != 0)
            {
                return -2;
            }

            ServerID = SessionManager->ServerID;
            return 0;
        }
    }

    return -1;
}

auto MCMultiPlayer::ProcessReceiveList() -> int32_t
{
    Assert(SessionManager != nullptr, 0);
    SessionManager->ProcessMessages();
    return 0;
}

auto MCMultiPlayer::SendToHost(MCFIMessageHeader* msg, int32_t size, int guaranteed) -> int32_t
{
    if (SessionManager == nullptr)
    {
        return -1;
    }

    if (IsHost != 0)
    {
        return -2;
    }

    if (guaranteed == 0)
    {
        SessionManager->SendMessageA(HostID, msg, static_cast<uint32_t>(size));
    }
    else
    {
        SessionManager->SendMessageToPlayerGuaranteed(HostID, static_cast<MCFIGuaranteedMessageHeader*>(msg),
                                                      static_cast<uint32_t>(size), 1);
    }

    return 0;
}

auto MCMultiPlayer::SendChat(uint32_t toID, char* text) -> int32_t
{
    if (SessionManager == nullptr)
    {
        return -1;
    }

    auto* chat = static_cast<MCMPChatMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_CHAT));
    std::strcpy(chat->Text, text);
    chat->ToAll = toID == 0 ? 1 : 0;
    const auto size = static_cast<uint32_t>(std::strlen(text) + 10);
    SessionManager->SendMessageToGroup(toID, chat, size);

    if (HandleOwnMessages != 0)
    {
        uint32_t myID = SessionManager->MyPlayer->Id;
        MCFidpMessage message(myID, 0x200);
        message.FromID = myID;
        message.SetMessageBuffer(chat, size);
        ChatCallback(&message, nullptr);
    }

    return 0;
}

auto MCMultiPlayer::SendPlayerCheckIn() -> int32_t
{
    auto* checkIn = static_cast<MCMPPlayerCheckInMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_CHECK_IN));
    checkIn->CheckInId = static_cast<int8_t>(CheckInId);
    checkIn->HomeTeam = static_cast<int8_t>(HomeTeam);

    if (IsServer == 0)
    {
        SessionManager->SendMessageToServerGuaranteed(checkIn, 10);
    }
    else
    {
        HandleAppPlayerCheckIn(SessionManager->MyPlayer->Id, checkIn);
    }

    return 0;
}

auto MCMultiPlayer::SendPlayerSetup(uint32_t toID, uint32_t setupServerID, uint32_t setupInnerSphereGroupID,
                                    uint32_t setupClanGroupID, uint32_t unused1, uint32_t unused2) -> int32_t
{
    auto* setup = static_cast<MCMPPlayerSetupMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_SETUP));
    setup->AllPlayerGroupID = AllPlayerGroupID;
    setup->ClanGroupID = setupClanGroupID;
    setup->InnerSphereGroupID = setupInnerSphereGroupID;

    if (NumPlayers() > 1)
    {
        if (toID == 0)
        {
            SessionManager->SendMessageToGroup(0, setup, 0x14);
        }
        else
        {
            SessionManager->SendMessageToPlayerGuaranteed(toID, setup, 0x14, 1);
        }
    }

    if (HandleOwnMessages != 0)
    {
        HandleAppPlayerSetup(SessionManager->MyPlayer->Id, MsgBuffer);
    }

    return 0;
}

auto MCMultiPlayer::SendPlayerCheckInReceipt(int32_t playerCheckInId) -> int32_t
{
    Assert(IsServer == 0, 0);
    auto* receipt = static_cast<MCMPLongMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_CHECK_IN_RECEIPT));
    receipt->Value = playerCheckInId;
    SessionManager->SendMessageToServerGuaranteed(receipt, 0xc);
    return 0;
}

auto MCMultiPlayer::SendStartPlanning() -> int32_t
{
    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_START_PLANNING);
    SessionManager->SendMessageToGroup(0, header, 8);

    if (HandleOwnMessages != 0)
    {
        HandleAppStartPlanning(SessionManager->MyPlayer->Id, header);

        for (int32_t i = 0; i < 6; i++)
        {
            PlayerCheckedIn[i] = 0;
        }
    }

    return 0;
}

auto MCMultiPlayer::SendReadyForBattle() -> int32_t
{
    // The type is first set to check-in (15), then to ready-for-battle (36).
    auto* ready = static_cast<MCMPPlayerCheckInMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_CHECK_IN));
    ready->Header &= 0xfc00;
    ready->Header |= MPMSG_READY_FOR_BATTLE;
    ready->CheckInId = static_cast<int8_t>(CheckInId);
    ready->HomeTeam = static_cast<int8_t>(HomeTeam);
    SessionManager->SendMessageToGroup(0, ready, 10);
    HandleAppReadyForBattle(SessionManager->MyPlayer->Id, ready);
    return 0;
}

auto MCMultiPlayer::SendPrepareScenario() -> int32_t
{
    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_PREPARE_SCENARIO);
    SessionManager->SendMessageToGroup(0, header, 8);

    if (IsHost != 0)
    {
        for (int32_t i = 0; i < 6; i++)
        {
            PlayerCheckedIn[i] = 0;
        }

        if (GlobalLogPtr != nullptr)
        {
            GlobalLogPtr->HandlePrepareScenarioMessage();
        }

        PrepareScenarioReceived = 1;
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
    void TakeOverMovers(MCMultiPlayer* multiPlayer)
    {
        for (int32_t i = 0; i < multiPlayer->NumMovers; i++)
        {
            MCMover* mover = multiPlayer->MoverRoster[i];

            if (mover == nullptr)
            {
                continue;
            }

            int32_t result = mover->SetControl(2, 0xffffffff, -1);

            if (result != 0)
            {
                Fatal(result, " MPlayer.setServer: unable to set control ");
            }

            MCMechWarrior* pilot = mover->GetPilot();

            if (pilot != nullptr)
            {
                pilot->ClearCurTacOrder(0, 0);
                pilot->OrderState = MCOrderState::General;
            }
        }
    }
}

auto MCMultiPlayer::SetServer(uint32_t newServerID) -> void
{
    ServerID = newServerID;
    int32_t wasServer = IsServer;

    if (SessionManager->IsHost == 0)
    {
        IsServer = 0;
    }
    else
    {
        IsServer = 1;

        if (wasServer == 0)
        {
            MCFidpPlayer* me = SessionManager->MyPlayer;
            char format[256];
            char text[512];
            CLoadString(ThisInstance, 0x378, format, 0xfe);
            std::snprintf(text, sizeof(text), format, me->Name);
            HandleOwnMessages = 1;
            SendChat(0, text);
            HandleOwnMessages = 0;

            for (int32_t i = 0; i < 6; i++)
            {
                PlayerCheckedIn[i] = 0;
            }
        }
    }

    if (InMission != 0 && IsServer != 0 && wasServer == 0)
    {
        TakeOverMovers(this);
    }
}

auto MCMultiPlayer::SendStartScenario(int32_t* playerValues, char* missionName) -> int32_t
{
    auto* start = static_cast<MCMPStartScenarioMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_START_SCENARIO));

    for (int32_t i = 0; i < MPlayer->NumPlayers(); i++)
    {
        start->PlayerValues[i] = playerValues[i];
    }

    for (int32_t i = 0; i < MPlayer->NumMovers; i++)
    {
        MCMechWarrior* pilot = MPlayer->MoverRoster[i]->GetPilot();
        Assert(pilot != nullptr, 0, " sendStartScenario: no pilot ");
        start->MoverFlags[i] = 0;

        if (pilot->EscapesThruEjection != 0)
        {
            start->MoverFlags[i] |= 1;
        }
    }

    std::strcpy(start->MissionName, missionName);
    SessionManager->SendMessageToGroup(0, start, static_cast<uint32_t>(std::strlen(missionName) + 0x39));

    if (HandleOwnMessages != 0)
    {
        HandleAppStartScenario(SessionManager->MyPlayer->Id, MsgBuffer);
    }

    for (int32_t i = 0; i < 6; i++)
    {
        PlayerCheckedIn[i] = 0;
    }

    return 0;
}

auto MCMultiPlayer::SendEndScenario(uint32_t toID, int32_t result) -> int32_t
{
    auto* end = static_cast<MCMPLongMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_END_SCENARIO));
    end->Value = result;
    SessionManager->SendMessageToGroup(0, end, 0xc);

    if (HandleOwnMessages != 0)
    {
        HandleAppEndScenario(SessionManager->MyPlayer->Id, MsgBuffer);
    }

    return 0;
}

auto MCMultiPlayer::SendPlayerOrder(uint32_t toID, MCTacticalOrder* order, int queued, int32_t numMovers,
                                    int32_t* moverParts, int32_t numGroups, MCMoverGroup** groups, int fromGroup)
    -> int32_t
{
    auto* message = static_cast<MCMPPlayerOrderMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_ORDER));
    message->CheckInId = static_cast<int8_t>(CheckInId);

    // The client clears its own movers' order queues for a stop order; the server sends the orders themselves.
    for (int32_t i = 0; i < numMovers; i++)
    {
        auto* mover = static_cast<MCMover*>(ObjectList()->FindObjectFromPart(moverParts[i]));

        if (mover == nullptr || mover == order->Target)
        {
            continue;
        }

        order->SetGroupFlag(mover->NetPlayerId, 1);

        if (fromGroup == 0)
        {
            MCTacticalOrder clearOrder;
            clearOrder.Reset();
            clearOrder.SetId(mover->GetPilot());

            if (order->Code == MCTacticalOrderCode::Stop)
            {
                mover->GetPilot()->ClearTacOrderQueue();
            }
        }
    }

    if (order->Code == MCTacticalOrderCode::MoveToPoint || order->Code == MCTacticalOrderCode::JumpToPoint)
    {
        message->OrderParam1 = std::bit_cast<uint32_t>(order->MoveParams.WayPath.Points[0]);
        message->OrderParam2 = std::bit_cast<uint32_t>(order->MoveParams.WayPath.Points[1]);
    }

    order->Pack();
    message->PackedOrder[0] = order->Data[0];
    message->PackedOrder[1] = order->Data[1];
    uint8_t flags = queued != 0 ? 1 : 0;

    if (fromGroup != 0)
    {
        flags |= 0x20;
    }

    for (int32_t i = 0; i < numGroups; i++)
    {
        flags |= static_cast<uint8_t>(1 << (groups[i]->GetId() + 1));
        MCMover* groupMovers[12];
        int32_t numGroupMovers = groups[i]->GetMovers(groupMovers);

        for (int32_t j = 0; j < numGroupMovers; j++)
        {
            if (fromGroup == 0)
            {
                MCTacticalOrder clearOrder;
                clearOrder.Reset();
                clearOrder.SetId(groupMovers[j]->GetPilot());

                if (order->Code == MCTacticalOrderCode::Stop)
                {
                    groupMovers[j]->GetPilot()->ClearTacOrderQueue();
                }
            }
        }
    }

    message->Flags = flags;
    SessionManager->SendMessageToServerGuaranteed(message, 0x1a);
    return 0;
}

auto MCMultiPlayer::SendPlayerMoverGroup(uint32_t toID, int32_t groupId, int32_t numGroupMovers, MCMover** movers,
                                         int32_t pointIndex) -> int32_t
{
    if (pointIndex < numGroupMovers && pointIndex > -1)
    {
        auto* message =
            static_cast<MCMPPlayerMoverGroupMessage*>(StartGuaranteedMessage(MsgBuffer, MPMSG_PLAYER_MOVER_GROUP));
        message->CheckInId = static_cast<int8_t>(CheckInId);
        message->GroupId = static_cast<int8_t>(groupId);
        uint16_t members = 0;

        for (int32_t i = 0; i < numGroupMovers; i++)
        {
            members |= static_cast<uint16_t>(1 << (movers[i]->NetPlayerId & 0x1f));
        }

        message->Members = static_cast<uint16_t>(members << 4 | movers[pointIndex]->NetPlayerId);
        Assert(1, 0xc, " sendPlayerMoverGroup: msgSz too large! ");
        SessionManager->SendMessageToGroup(0, message, 0xc);
    }

    return 0;
}

auto MCMultiPlayer::SendPlayerArtillery(uint32_t toID, int32_t strikeType, MCVector3D location, int32_t seconds)
    -> int32_t
{
    auto* message = reinterpret_cast<MCMPPlayerArtilleryMessage*>(MsgBuffer);
    message->Tagger.Clear();
    message->Header = 0;
    message->Header |= FIMSG_GUARANTEED;
    message->Header &= 0xfc00;
    message->Header |= MPMSG_PLAYER_ARTILLERY;
    MCArtilleryChunk chunk;
    chunk.CommanderId = -1;
    chunk.StrikeType = -1;
    chunk.CellRow = -1;
    chunk.CellCol = -1;
    chunk.Seconds = -1;
    chunk.Data = 0;
    chunk.Build(CheckInId, strikeType, location, seconds);
    chunk.Pack();
    MCArtilleryChunk check;
    check.CommanderId = -1;
    check.StrikeType = -1;
    check.CellRow = -1;
    check.CellCol = -1;
    check.Seconds = -1;
    check.Data = chunk.Data;
    check.Unpack();

    if (chunk.EqualTo(&check) == 0)
    {
        Fatal(0, " MultiPlayer.sendPlayerArtillery: Artillery chunks don't match ");
    }

    message->TargetX = location.X;
    message->TargetY = location.Y;
    message->ArtilleryData = chunk.Data;
    SessionManager->SendMessageToGroup(0, message, 0x14);
    return 0;
}

auto MCMultiPlayer::SendMoverUpdate(uint32_t toID) -> int32_t
{
    auto* header = reinterpret_cast<MCFIMessageHeader*>(MsgBuffer);
    header->Header = 0;
    header->Header &= 0xfc00;
    header->Header |= MPMSG_MOVER_UPDATE;
    *reinterpret_cast<uint16_t*>(MsgBuffer + 2) = MoverUpdateSequence;
    MoverUpdateSequence++;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMover* mover = MoverRoster[i];
        Assert(mover != nullptr, 0, " SendMoverUpdate: No Mover ");
        mover->BuildMoveChunk();
        std::memcpy(MsgBuffer + 4 + i * 4, &mover->GetMoveChunk()->Data, 4);
    }

    uint8_t* statusChunks = MsgBuffer + 4 + NumMovers * 4;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        MCMover* mover = MoverRoster[i];
        mover->BuildStatusChunk();
        std::memcpy(statusChunks, &mover->GetStatusChunk()->Data, 4);
        statusChunks += 4;
    }

    uint8_t* orderIds = statusChunks;

    for (int32_t i = 0; i < NumMovers; i++)
    {
        *orderIds = static_cast<uint8_t>(MoverRoster[i]->GetPilot()->CurTacOrder.Id);
        orderIds++;
    }

    auto size = static_cast<uint32_t>(NumMovers + 4 + NumMovers * 8);
    Assert(size < 0x1400, size, " sendMoverUpdate: msgSz too large! ");
    SessionManager->BroadcastMessage(header, size);
    return 0;
}

auto MCMultiPlayer::SendTurretUpdate(uint32_t toID) -> int32_t
{
    if (NumTurrets == 0)
    {
        return 0;
    }

    auto* header = reinterpret_cast<MCFIMessageHeader*>(MsgBuffer);
    header->Header = 0;
    header->Header &= 0xfc00;
    header->Header |= MPMSG_TURRET_UPDATE;
    *reinterpret_cast<uint16_t*>(MsgBuffer + 2) = TurretUpdateSequence;
    TurretUpdateSequence++;

    for (int32_t i = 0; i < NumTurrets; i++)
    {
        MCTurret* turret = TurretRoster[i];
        Assert(turret != nullptr, 0, " SendTurretUpdate: No Turret ");

        if (turret->GetAwake() == 0 || turret->Target == nullptr)
        {
            MsgBuffer[4 + i] = 0xff;
        }
        else
        {
            MCGameObject* target = turret->Target;
            MCObjectClass targetClass = target->ObjectClass;

            // A mover's roster index, or (a building) its part id less 0x48, which handleAppTurretUpdate can't
            // tell from a roster index.
            if (IsMoverClass(targetClass))
            {
                MsgBuffer[4 + i] = static_cast<uint8_t>(static_cast<MCMover*>(target)->NetRosterIndex);
            }
            else
            {
                MsgBuffer[4 + i] = static_cast<uint8_t>(target->PartId - 0x48);
            }
        }
    }

    auto size = static_cast<uint32_t>(NumTurrets + 4);
    Assert(size < 0x1400, size, " sendTurretUpdate: msgSz too large! ");
    SessionManager->BroadcastMessage(header, size);
    return 0;
}

auto MCMultiPlayer::SendMoverWeaponFireUpdate(uint32_t toID) -> int32_t
{
    // As many messages as it takes: each holds at most 0x77 chunks.
    while (true)
    {
        MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_MOVER_WEAPON_FIRE_UPDATE);
        MsgBuffer[8] = 0;
        MsgBuffer[9] = static_cast<uint8_t>(NumMovers);

        for (int32_t i = 0; i < NumMovers; i++)
        {
            MsgBuffer[10 + i] = 0;
        }

        int32_t numChunks = 0;

        for (int32_t i = 0; i < NumMovers; i++)
        {
            int32_t grabbed = MoverRoster[i]->GrabWeaponFireChunks(
                0, std::span(reinterpret_cast<uint32_t*>(MsgBuffer + 0x22 + numChunks * 4),
                             static_cast<size_t>(0x77 - numChunks)));
            MsgBuffer[10 + i] = static_cast<uint8_t>(grabbed);
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
        SessionManager->SendMessageToGroup(0, header, size);
    }
}

auto MCMultiPlayer::SendTurretWeaponFireUpdate(uint32_t toID) -> int32_t
{
    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_TURRET_WEAPON_FIRE_UPDATE);
    int32_t numFiring = 0;

    for (int32_t i = 0; i < NumTurrets; i++)
    {
        if (!TurretRoster[i]->WeaponFireChunks[0].empty())
        {
            numFiring++;
        }
    }

    if (numFiring == 0)
    {
        return 0;
    }

    MsgBuffer[8] = static_cast<uint8_t>(numFiring);
    int32_t entry = 0;
    int32_t numChunks = 0;

    for (int32_t i = 0; i < NumTurrets; i++)
    {
        MCTurret* turret = TurretRoster[i];

        if (!turret->WeaponFireChunks[0].empty())
        {
            const size_t offset = static_cast<size_t>(numChunks * 4 + numFiring + 9);
            const int32_t turretChunks = turret->GrabWeaponFireChunks(
                0, std::span(reinterpret_cast<uint32_t*>(MsgBuffer + offset), (0x1400 - offset) / 4));
            numChunks += turretChunks;
            MsgBuffer[9 + entry] = static_cast<uint8_t>(turretChunks + turret->NetRosterIndex * 4);
            entry++;
            turret->ClearWeaponFireChunks(0);
        }
    }

    if (numChunks > 0)
    {
        auto size = static_cast<uint32_t>(entry + 9 + numChunks * 4);
        Assert(size < 0x1400, size, " sendTurretWeaponFireUpdate: msgSz too large! ");
        SessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MCMultiPlayer::SendMoverCriticalHitUpdate(uint32_t toID) -> int32_t
{
    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_MOVER_CRITICAL_HIT_UPDATE);
    MsgBuffer[8] = 0;
    MsgBuffer[9] = static_cast<uint8_t>(NumMovers);
    int32_t numBytes = 0;

    for (int32_t i = 0; i < static_cast<int8_t>(MsgBuffer[9]); i++)
    {
        MCMover* mover = MoverRoster[static_cast<int8_t>(MsgBuffer[8]) + i];
        auto numCriticalHits = static_cast<uint32_t>(mover->GrabCriticalHitChunks(0, MsgBuffer + numBytes + 0x3a));
        MsgBuffer[10 + i] = static_cast<uint8_t>(numCriticalHits);
        Assert(numCriticalHits < 0x81, numCriticalHits, " sendMoverCritHits: bad numCH ");
        auto numRadio = static_cast<uint32_t>(mover->GrabRadioChunks(0, MsgBuffer + numBytes + numCriticalHits + 0x3a));
        MsgBuffer[0x22 + i] = static_cast<uint8_t>(numRadio);
        Assert(numRadio < 8, numRadio, " sendMoverCritHits: bad numRDO ");
        numBytes += static_cast<int32_t>(numCriticalHits + numRadio);
        mover->ClearCriticalHitChunks(0);
        mover->ClearRadioChunks(0);
    }

    if (numBytes > 0)
    {
        auto size = static_cast<uint32_t>(numBytes + 0x3a);
        Assert(size < 0x1400, size, " sendMoverCriticalHitUpdate: msgSz too large! ");
        SessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MCMultiPlayer::SendWeaponHitUpdate(uint32_t toID) -> int32_t
{
    while (NumWeaponHitChunks > 0)
    {
        MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_WEAPON_HIT_UPDATE);
        int32_t numChunks = NumWeaponHitChunks;

        if (numChunks > 0x7d)
        {
            numChunks = 0x7d;
        }

        MsgBuffer[8] = static_cast<uint8_t>(numChunks);
        GrabWeaponHitChunks(reinterpret_cast<uint32_t*>(MsgBuffer + 9), numChunks);
        auto size = static_cast<uint32_t>(MsgBuffer[8] * 4 + 9);
        Assert(size < 0x200, size, " sendWeaponHitUpdate: msgSz too large! ");
        SessionManager->SendMessageToGroup(0, header, size);
    }

    return 0;
}

auto MCMultiPlayer::SendWorldStateUpdate(uint32_t toID) -> int32_t
{
    if (NumWorldStateChunks <= 0)
    {
        return 0;
    }

    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_WORLD_STATE_UPDATE);
    MsgBuffer[8] = static_cast<uint8_t>(NumWorldStateChunks);
    GrabWorldStateChunks(reinterpret_cast<uint32_t*>(MsgBuffer + 10));
    NumWorldStateChunks = 0;
    auto size = static_cast<uint32_t>(MsgBuffer[8] * 4 + 10);

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

    SessionManager->SendMessageToGroup(0, header, size);

    for (int32_t i = 0; i < 10; i++)
    {
        WorldStateChunkTally[i] = 0;
    }

    if (MCAblRuntime* abl = AblRuntime())
    {
        abl->MissionScriptMessages.clear();
    }

    return 0;
}

auto MCMultiPlayer::SendFile(char* fileName, char* directory) -> int32_t
{
    // The original passes the directory as the file name and the name as the directory (OB-107).
    SessionManager->BroadcastFile(directory, fileName, MultiPlayerFileSentCallback);
    return 0;
}

auto MCMultiPlayer::SendFileInquiry(char* fileName) -> int32_t
{
    MCFIGuaranteedMessageHeader* header = StartGuaranteedMessage(MsgBuffer, MPMSG_FILE_INQUIRY);
    std::memcpy(MsgBuffer + 8, fileName, std::strlen(fileName) + 1);
    SessionManager->SendMessageToGroup(AllPlayerGroupID, header, static_cast<uint32_t>(std::strlen(fileName) + 9));
    return 0;
}

auto MCMultiPlayer::UpdateClients() -> int32_t
{
    if (InMission == 0)
    {
        return 0;
    }

    if (NextWorldStateUpdateTime < ScenarioTime)
    {
        SendWorldStateUpdate(0);
        SendMoverWeaponFireUpdate(0);
        SendTurretWeaponFireUpdate(0);
        SendWeaponHitUpdate(0);
        SendMoverCriticalHitUpdate(0);
        NextWorldStateUpdateTime += WorldStateUpdateFrequency;
    }

    if (NextMoverUpdateTime < ScenarioTime)
    {
        SendMoverUpdate(0);
        NextMoverUpdateTime += MoverUpdateFrequency;
    }

    if (NextTurretUpdateTime < ScenarioTime)
    {
        SendTurretUpdate(0);
        NextTurretUpdateTime += TurretUpdateFrequency;
    }

    return 0;
}

auto MCMultiPlayer::UpdateServer() -> int32_t
{
    if (NextPlayerUpdateTime < ScenarioTime)
    {
        auto* header = reinterpret_cast<MCFIMessageHeader*>(MsgBuffer);
        header->Header = 0;
        header->Header &= 0xfc00;
        header->Header |= MPMSG_PLAYER_UPDATE;
        SessionManager->SendMessageToServer(header, 6);
        NextPlayerUpdateTime += 1.0f;
    }

    return 0;
}

auto MCMultiPlayer::PlayersInSession() -> int
{
    return SessionManager->GetPlayers(nullptr)->Size();
}

auto MCMultiPlayer::PlayersOnHomeTeam() -> MCFLinkedList<uint32_t>*
{
    MCFidpGroup* group = SessionManager->GetGroup(HomeTeamGroupID);
    return group != nullptr ? &group->Players : nullptr;
}

auto MCMultiPlayer::PlayersOnEnemyTeam() -> MCFLinkedList<uint32_t>*
{
    MCFidpGroup* group = SessionManager->GetGroup(EnemyTeamGroupID);
    return group != nullptr ? &group->Players : nullptr;
}

auto MCMultiPlayer::IsMyTeammate(uint32_t playerID) -> int
{
    MCFLinkedList<uint32_t>* players = PlayersOnHomeTeam();

    if (players == nullptr)
    {
        return 0;
    }

    for (MCFLink<uint32_t>* link = players->HeadLink; link != nullptr; link = link->Next)
    {
        if (*link->Data == playerID)
        {
            return 1;
        }
    }

    return 0;
}

auto MCMultiPlayer::AllPlayersCheckedIn() -> int
{
    MCFLinkedList<MCFidpPlayer>* players = SessionManager->GetPlayers(nullptr);

    for (MCFLink<MCFidpPlayer>* link = players->HeadLink; link != nullptr; link = link->Next)
    {
        MCFidpPlayer* player = link->Data;

        if (player->HasPlayerNumber != 0)
        {
            Assert(player->PlayerNumber >= 0 && player->PlayerNumber <= 5, 0, "Invalid player number");

            if (PlayerCheckedIn[player->PlayerNumber] == 0)
            {
                return 0;
            }
        }
    }

    return 1;
}

auto MCMultiPlayer::SwitchServers() -> void
{
    if (IsServer != 0)
    {
        SessionManager->SwitchServers();
        ServerID = SessionManager->ServerID;
        IsServer = SessionManager->IsHost != 0 ? 1 : 0;
    }
}

auto MCMultiPlayer::PlayerLeftGame(uint32_t playerID) -> void
{
    int onHomeTeam = 0;

    if (SessionManager->CurrentConnection != 0x10)
    {
        NumLanPlayers--;
    }

    if (EventsToMissionResultsScreen != 0)
    {
        return;
    }

    int32_t wasServer = IsServer;

    if (playerID == HostID)
    {
        HostID = SessionManager->ServerID;
        HostLeft = 1;
        IsHost = SessionManager->IsHost;
    }

    if (SessionManager->IsHost != 0)
    {
        IsServer = 1;
        MCFidpPlayer* player = SessionManager->GetPlayer(playerID);

        if (player != nullptr)
        {
            char format[256];
            char serverText[256];
            char text[768];
            CLoadString(ThisInstance, 0x382, format, 0xfe);
            CLoadString(ThisInstance, 899, serverText, 0xfe);
            std::snprintf(text, sizeof(text), format, player->Name);

            if (wasServer == 0)
            {
                std::strncat(text, serverText, sizeof(text) - std::strlen(text) - 1);
            }

            HandleOwnMessages = 1;
            SendChat(0, text);
            HandleOwnMessages = 0;
        }
    }

    if (playerID == ServerID && InLogistics != 0 && PrepareScenarioReceived != 0)
    {
        SendPlayerCheckIn();
    }

    ServerID = SessionManager->ServerID;

    if (LaunchedFromLobby == 0)
    {
        if (NumPlayers() < 2)
        {
            Mission->EndScenarioRequested = 1;
        }
    }
    else if (NumPlayers() == 2)
    {
        Mission->EndScenarioRequested = 1;
    }

    if (InMission != 0)
    {
        if (IsServer != 0 && wasServer == 0)
        {
            TakeOverMovers(this);
        }

        return;
    }

    if (InLogistics == 0)
    {
        if (GlobalLogPtr->CurrentScreen == GlobalLogPtr->SessionScreen ||
            GlobalLogPtr->CurrentScreen == GlobalLogPtr->LoadScreen)
        {
            char reason[256];
            char text[512];
            CLoadString(ThisInstance, LaunchedFromLobby == 0 ? 0x35f : 0x365, reason, 0xfe);
            MCFidpPlayer* player = MPlayer->SessionManager->GetPlayer(playerID);
            // Port fix: the player is already gone when DirectPlay reports it; the original printed its freed name.
            std::snprintf(text, sizeof(text), "%s %s", player != nullptr ? player->Name : "", reason);
            MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
            dialog->SetText(text);
            dialog->SetTwoButton(0);
            dialog->Callback = CancelBool;
            dialog->OkButton->Callback()->SetExec(nullptr);
            char upArt[] = "bh_okay.tga";
            char downArt[] = "bg_okay.tga";
            dialog->OkButton->SetUpPicture(upArt);
            dialog->OkButton->SetDownPicture(downArt);
            dialog->OkButton->Disabled = 0;
            dialog->OkButton->Draw();
            dialog->Timeout = 15000;
            dialog->Activate();
        }
    }
    else
    {
        if (GlobalLogPtr != nullptr)
        {
            for (int32_t i = 0; i < 6; i++)
            {
                if (PlayerTeams[i].PlayerID == playerID)
                {
                    if (static_cast<uint32_t>(PlayerTeams[i].Team) == HomeTeamGroupID)
                    {
                        onHomeTeam = 1;
                    }

                    break;
                }
            }

            GlobalLogPtr->HandleLostPlayer(playerID, onHomeTeam);
        }

        if (IsServer != 0 && AllPlayersCheckedIn() != 0 && PrepareScenarioReceived != 0)
        {
            HandleOwnMessages = 1;
            char missionName[] = "";
            SendStartScenario(PlayerCheckedIn, missionName);
            HandleOwnMessages = 0;
        }
    }
}

auto MCMultiPlayer::LeaveSession() -> void
{
    SessionManager->LeaveSession();
    InitStartupParameters();
}

auto MCMultiPlayer::ResetForNewGame() -> void
{
    if (HomeTeamGroupID != 0)
    {
        SessionManager->RemovePlayerFromGroup(HomeTeamGroupID, 0);
    }

    HomeTeamGroupID = 0;
    EnemyTeamGroupID = 0;
    NumLocalMovers = 0;
    NumMovers = 0;
    NumTurrets = 0;
    HandleOwnMessages = 0;
    HostLeft = 0;
    HomeTeam = -1;

    for (int32_t i = 0; i < 6; i++)
    {
        PlayerCheckedIn[i] = 0;

        for (int32_t j = 0; j < 12; j++)
        {
            PlayerMoverRoster[i][j] = nullptr;
        }
    }

    InLogistics = 0;
    InMission = 0;
    PrepareScenarioReceived = 0;
    ScenarioResult = 0;

    for (int32_t i = 0; i < 24; i++)
    {
        MoverRoster[i] = nullptr;
    }

    for (int32_t i = 0; i < 6; i++)
    {
        PlayerTeams[i].PlayerID = 0;
        PlayerTeams[i].Team = 0;
    }

    NextMoverUpdateTime = 0.0f;
    MoverUpdateSequence = 0;
    NextTurretUpdateTime = 0.0f;
    TurretUpdateSequence = 0;
    NextWorldStateUpdateTime = 0.0f;
    NumWeaponHitChunks = 0;
    NumWorldStateChunks = 0;
}

auto MCMultiPlayer::InitStartupParameters() -> void
{
    AllPlayerGroupID = 0;
    InnerSphereGroupID = 0;
    ClanGroupID = 0;
    ServerID = 0;
    HomeTeamGroupID = 0;
    EnemyTeamGroupID = 0;
    HostID = 0;
    NumLocalMovers = 0;
    NumMovers = 0;
    NumTurrets = 0;
    HandleOwnMessages = 0;
    HostLeft = 0;
    IsHost = 0;
    IsServer = 0;
    CheckInId = -1;
    NumLanPlayers = 1;
    HomeTeam = -1;
    SessionName[0] = '\0';
    PlayerName[0] = '\0';

    for (int32_t i = 0; i < 6; i++)
    {
        PlayerCheckedIn[i] = 0;

        for (int32_t j = 0; j < 12; j++)
        {
            PlayerMoverRoster[i][j] = nullptr;
        }
    }

    InLogistics = 0;
    InMission = 0;
    PrepareScenarioReceived = 0;
    ScenarioResult = 0;

    for (int32_t i = 0; i < 24; i++)
    {
        MoverRoster[i] = nullptr;
    }

    for (int32_t i = 0; i < 6; i++)
    {
        PlayerTeams[i].PlayerID = 0;
        PlayerTeams[i].Team = 0;
    }

    NextMoverUpdateTime = 0.0f;
    MoverUpdateSequence = 0;
    NextTurretUpdateTime = 0.0f;
    TurretUpdateSequence = 0;
    NextWorldStateUpdateTime = 0.0f;
    NumWeaponHitChunks = 0;
    NumWorldStateChunks = 0;
}

auto MCMultiPlayer::Destroy() -> void
{
    if (SessionManager != nullptr)
    {
        // Port fix: the original only called destroy (vtable slot 1) and let DestroyLinkUpHeap drop the memory; the
        // port runs the destructor so the members (the mutex, the sockets) are released too.
        delete SessionManager;
        SessionManager = nullptr;
    }

    DestroyLinkUpBlocks();

    if (MsgBuffer != nullptr)
    {
        delete[] MsgBuffer;
        MsgBuffer = nullptr;
    }
}

auto ShowConnectStatus() -> void
{
    char text[256];
    CLoadString(ThisInstance, 0x354, text, 0xfe);

    if (GlobalLogPtr != nullptr)
    {
        GlobalLogPtr->MessageDialog->SetText(text);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;

        if (dialog->OkButton != nullptr)
        {
            dialog->OkButton->ShowGuiWindow(0);
        }

        if (dialog->CancelButton != nullptr)
        {
            dialog->CancelButton->ShowGuiWindow(0);
        }

        GlobalLogPtr->MessageDialog->Callback = nullptr;
        GlobalLogPtr->MessageDialog->Activate();
        UpdateDisplay(0, 0, 0, 0, 0);
    }
}

auto DestroyConnectStatusWindow() -> void
{
    if (GlobalLogPtr != nullptr)
    {
        GlobalLogPtr->MessageDialog->Deactivate(0);
        MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;

        if (dialog->OkButton != nullptr)
        {
            dialog->OkButton->ShowGuiWindow(1);
        }

        if (dialog->CancelButton != nullptr)
        {
            dialog->CancelButton->ShowGuiWindow(1);
        }

        dialog->SetTwoButton(1);
    }
}

auto LoadMultiplayerGameSystem(MCFitIniFile* file) -> int32_t
{
    int32_t result = file->SeekBlock("Multiplayer");

    if (result == 0)
    {
        result = file->ReadIdFloat("WarpFactor", WarpFactor);

        if (result == 0)
        {
            result = 0;
        }
    }

    return result;
}

auto HandleSysCreatePlayer(void* msg) -> void
{
    auto* create = static_cast<DPMSG_CREATEPLAYERORGROUP*>(msg);

    if (MPlayer->SessionManager->CurrentConnection != 0x10)
    {
        NumLanPlayers++;
    }

    if (MPlayer->IsServer != 0)
    {
        MPlayer->SendPlayerSetup(create->dpId, MPlayer->ServerID, MPlayer->InnerSphereGroupID, MPlayer->ClanGroupID, 0,
                                 0);
    }
}

auto HandleSysAddPlayerToGroup(void* msg) -> void
{
    auto* add = static_cast<DPMSG_ADDPLAYERTOGROUP*>(msg);
    MCFidpPlayer* player = MPlayer->SessionManager->GetPlayer(add->dpIdPlayer);

    if (player != nullptr && player->PlayerNumber >= 0 && player->PlayerNumber < 6)
    {
        MPlayer->PlayerTeams[player->PlayerNumber].PlayerID = add->dpIdPlayer;
        MPlayer->PlayerTeams[player->PlayerNumber].Team = static_cast<int32_t>(add->dpIdGroup);
    }
}

auto HandleAppChat(MCFidpMessage* msg, void* data) -> void
{
    auto* chat = reinterpret_cast<MCMPChatMessage*>(msg->MessageBuffer);
    MCFidpPlayer* player = MPlayer->SessionManager->GetPlayer(msg->FromID);
    char line[256];
    // Port fix: a sender already gone from the session has no name; the original read through the null player.
    std::snprintf(line, sizeof(line), "%s: %s", player != nullptr ? player->Name : "", chat->Text);

    if (AblGetDebugger() != nullptr)
    {
        AblGetDebugger()->Print(line);
    }
}

auto HandleAppNewServer(uint32_t fromID, const void* msg) -> void
{
    MPlayer->SetServer(static_cast<const MCMPLongMessage*>(msg)->Value);
}

auto HandleAppPlayerCheckIn(uint32_t fromID, const void* msg) -> void
{
    auto* checkIn = static_cast<const MCMPPlayerCheckInMessage*>(msg);

    if (GlobalLogPtr != nullptr && GlobalLogPtr->PlayerLights != nullptr)
    {
        GlobalLogPtr->PlayerLights->SetPlayerStatus(fromID, 2);
    }

    if (MPlayer->IsServer != 0)
    {
        Assert(checkIn->CheckInId >= 0 && checkIn->CheckInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        MPlayer->PlayerCheckedIn[checkIn->CheckInId] = static_cast<int32_t>(fromID);

        if (MPlayer->AllPlayersCheckedIn() != 0)
        {
            MPlayer->HandleOwnMessages = 1;
            char missionName[] = "";
            MPlayer->SendStartScenario(MPlayer->PlayerCheckedIn, missionName);
            MPlayer->HandleOwnMessages = 0;
        }
    }
}

auto HandleAppPlayerSetup(uint32_t fromID, const void* msg) -> void
{
    auto* setup = static_cast<const MCMPPlayerSetupMessage*>(msg);
    MCSessionManager* sessionManager = MPlayer->SessionManager;

    if (MPlayer->CheckInId == -1)
    {
        MPlayer->CheckInId = sessionManager->MyPlayer->PlayerNumber;
    }

    MPlayer->InitUpdateFrequencies();
    MPlayer->HostID = sessionManager->ServerID;
    MPlayer->ServerID = MPlayer->HostID;
    MPlayer->AllPlayerGroupID = setup->AllPlayerGroupID;
    MPlayer->ClanGroupID = setup->ClanGroupID;
    MPlayer->InnerSphereGroupID = setup->InnerSphereGroupID;

    if (MPlayer->HomeTeam == 0)
    {
        sessionManager->AddPlayerToGroup(MPlayer->InnerSphereGroupID, 0);
        MPlayer->HomeTeamGroupID = MPlayer->InnerSphereGroupID;
        MPlayer->EnemyTeamGroupID = MPlayer->ClanGroupID;
    }
    else if (MPlayer->HomeTeam == 1)
    {
        sessionManager->AddPlayerToGroup(MPlayer->ClanGroupID, 0);
        MPlayer->HomeTeamGroupID = MPlayer->ClanGroupID;
        MPlayer->EnemyTeamGroupID = MPlayer->InnerSphereGroupID;
    }

    sessionManager->AddPlayerToGroup(MPlayer->AllPlayerGroupID, 0);
}

auto HandleAppPlayerCheckInReceipt(uint32_t fromID, const void* msg) -> void
{
    int32_t checkInId = static_cast<const MCMPLongMessage*>(msg)->Value;
    Assert(checkInId >= 0 && checkInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
    MPlayer->PlayerCheckedIn[checkInId] = static_cast<int32_t>(fromID);

    if (MPlayer->AllPlayersCheckedIn() != 0)
    {
        MPlayer->HandleOwnMessages = 1;
        MPlayer->SendStartPlanning();
        MPlayer->HandleOwnMessages = 0;
    }
}

auto HandleAppStartPlanning(uint32_t fromID, const void* msg) -> void
{
    MPlayer->InLogistics = 1;
}

auto HandleAppReadyForBattle(uint32_t fromID, const void* msg) -> void
{
    auto* ready = static_cast<const MCMPPlayerCheckInMessage*>(msg);

    if (GlobalLogPtr == nullptr)
    {
        return;
    }

    GlobalLogPtr->PlayerLights->SetPlayerStatus(fromID, 2);

    if (MPlayer->IsHost != 0)
    {
        Assert(ready->CheckInId >= 0 && ready->CheckInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        MPlayer->PlayerCheckedIn[ready->CheckInId] = static_cast<int32_t>(fromID);

        if (MPlayer->AllPlayersCheckedIn() != 0)
        {
            MPlayer->SendPrepareScenario();
        }
    }
}

auto HandleAppJoinTeam(uint32_t fromID, const void* msg) -> void
{
    auto* join = static_cast<const MCMPJoinTeamMessage*>(msg);

    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->AssignPlayer(join->PlayerID, join->Team, join->Slot, 0);
    }
}

auto HandleAppRPUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* update = static_cast<const MCMPTwoLongMessage*>(msg);
    MCSessionScreen* sessionScreen = GlobalLogPtr->SessionScreen;

    if (sessionScreen != nullptr)
    {
        if (update->Value2 == 1)
        {
            sessionScreen->SetTeam1RP(update->Value1);
        }

        if (update->Value2 == 2)
        {
            sessionScreen->SetTeam2RP(update->Value1);
        }

        sessionScreen->Draw();
    }
}

auto HandleAppTechbaseChange(uint32_t fromID, const void* msg) -> void
{
    auto* change = static_cast<const MCMPTwoLongMessage*>(msg);

    if (GlobalLogPtr != nullptr && GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->SetTeamTechBase(static_cast<char>(change->Value1),
                                                     static_cast<char>(change->Value2));
    }
}

auto HandleAppSwitchScreen(uint32_t fromID, const void* msg) -> void
{
    if (static_cast<const MCMPTwoLongMessage*>(msg)->Value1 == 1)
    {
        GlobalLogPtr->SetUpSessionScreen();
    }
}

auto HandleAppStartScenario(uint32_t fromID, const void* msg) -> void
{
    auto* start = static_cast<const MCMPStartScenarioMessage*>(msg);

    for (uint32_t i = 1; i <= Scenario->NumParts; i++)
    {
        MCFidpPlayer* player = MPlayer->SessionManager->GetPlayerNumber(Scenario->Parts[i].CommanderId);

        if (player != nullptr)
        {
            auto* mover = static_cast<MCMover*>(Scenario->Parts[i].Object);
            mover->NetOwnerID = player->Id;

            // The original copied at most 255 characters into the name's buffer.
            mover->NetName.assign(player->Name, strnlen(player->Name, 0xff));
        }
    }

    for (int32_t i = 0; i < MPlayer->NumMovers; i++)
    {
        MPlayer->MoverRoster[i]->GetPilot()->EscapesThruEjection = (start->MoverFlags[i] & 1) != 0 ? 1 : 0;
    }

    MPlayer->InMission = 1;
    PrepareScenarioReceived = 0;

    if (AblGetDebugger() != nullptr)
    {
        AblGetDebugger()->Print(const_cast<char*>(start->MissionName));
    }
}

auto HandleAppEndScenario(uint32_t fromID, const void* msg) -> void
{
    MPlayer->ScenarioResult = static_cast<const MCMPLongMessage*>(msg)->Value;
}

auto HandleAppPlayerOrder(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MCMPPlayerOrderMessage*>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    MCCommander* commander = CommanderById(message->CheckInId);

    if (MPlayer->IsServer == 0)
    {
        return;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Data[0] = message->PackedOrder[0];
    order.Data[1] = message->PackedOrder[1];
    order.Unpack();
    MCVector3D wayPoint;
    wayPoint.X = std::bit_cast<float>(message->OrderParam1);
    wayPoint.Y = std::bit_cast<float>(message->OrderParam2);
    wayPoint.Z = Terrain()->GetTerrainElevation(wayPoint);
    order.SetWayPoint(0, wayPoint);

    // A jump-attack (method 1) becomes a jump to the target's position, as Parser::SendTacOrder does locally.
    if (order.Code == MCTacticalOrderCode::AttackObject && order.AttackParams.Method == 1)
    {
        order.Code = MCTacticalOrderCode::JumpToObject;
        order.MoveParams.Wait = 0;
        order.MoveParams.WayPath.Mode[0] = 0;

        if (order.Target != nullptr)
        {
            order.SetWayPoint(0, order.Target->GetPosition());
        }
    }

    if (order.Code == MCTacticalOrderCode::JumpToObject)
    {
        order.Code = MCTacticalOrderCode::JumpToPoint;
        Assert(order.Target != nullptr, 0, " JumpToObject is NULL ");
        order.SetWayPoint(0, order.Target->GetPosition());
    }

    MCMover* movers[12];
    MCMover* point = nullptr;
    int32_t numMovers = order.GetGroup(message->CheckInId, movers, &point);
    MCVector3D jumpGoals[72];
    int32_t numGoals = 0;

    if (order.Code == MCTacticalOrderCode::JumpToPoint)
    {
        numGoals = numMovers;

        for (int32_t groupId = 0; groupId < 4; groupId++)
        {
            if ((message->Flags & (2 << groupId)) != 0)
            {
                numGoals += commander->GetGroup(groupId)->NumMovers();
            }
        }

        CalcJumpGoals(order.GetWayPoint(0), numGoals, jumpGoals, order.GetJumpTarget());
    }

    int fromGroup = (message->Flags & 0x20) != 0 ? 1 : 0;

    if (numMovers > 0)
    {
        // The client sends its sort flag in bit 0, but the server reads bit 4, group 3's bit (OB-105).
        int sortMovers = (message->Flags & 0x10) != 0 ? 1 : 0;

        if (sortMovers != 0)
        {
            SortMoverList(numMovers, movers, order.GetWayPoint(0));
        }

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMover* mover = movers[i];

            if (mover == nullptr || mover == order.Target)
            {
                continue;
            }

            if (sortMovers != 0)
            {
                order.SelectionIndex = mover->SelectionIndex;
            }

            if (order.Code == MCTacticalOrderCode::JumpToPoint)
            {
                order.SetWayPoint(0, jumpGoals[i]);
            }

            mover->HandleTacticalOrder(order, 1, fromGroup);
        }
    }

    int32_t goalIndex = numMovers;

    for (int32_t groupId = 0; groupId < 4; groupId++)
    {
        if ((message->Flags & (2 << groupId)) == 0)
        {
            continue;
        }

        MCVector3D* destinations = nullptr;

        if (order.Code == MCTacticalOrderCode::JumpToPoint)
        {
            destinations = &jumpGoals[goalIndex];
            goalIndex += commander->GetGroup(groupId)->NumMovers();
        }

        commander->GetGroup(groupId)->HandleTacticalOrder(order, 1, destinations, fromGroup);
    }
}

auto HandleAppPlayerMoverGroup(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MCMPPlayerMoverGroupMessage*>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    MCMover** playerMovers = MPlayer->PlayerMoverRoster[message->CheckInId];
    uint32_t memberBits = message->Members >> 4;
    MCMover* movers[12];
    int32_t numMovers = 0;

    for (int32_t i = 0; i < 12; i++)
    {
        if ((memberBits & 1) != 0)
        {
            movers[numMovers++] = playerMovers[i];
        }

        memberBits >>= 1;
    }

    int32_t pointIndex = message->Members & 0xf;
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
    CommanderById(message->CheckInId)->SetGroup(message->GroupId, numMovers, movers, pointIndex);
}

auto HandleAppPlayerArtillery(uint32_t fromID, const void* msg) -> void
{
    auto* message = static_cast<const MCMPPlayerArtilleryMessage*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer == 0)
    {
        return;
    }

    MCArtilleryChunk chunk;
    chunk.CommanderId = -1;
    chunk.StrikeType = -1;
    chunk.CellRow = -1;
    chunk.CellCol = -1;
    chunk.Seconds = -1;
    chunk.Data = message->ArtilleryData;
    chunk.Unpack();
    MCVector3D location;
    location.X = message->TargetX;
    location.Y = message->TargetY;
    location.Z = Terrain()->GetTerrainElevation(location);
    CallArtillery(chunk.CommanderId, chunk.StrikeType, location, chunk.Seconds, 0);
}

auto HandleAppMoverUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);
    uint16_t sequence = *reinterpret_cast<const uint16_t*>(bytes + 2);

    if (!InMultiplayerMission() || sequence < MPlayer->MoverUpdateSequence)
    {
        return;
    }

    if (MPlayer->IsServer == 0)
    {
        const uint8_t* chunks = bytes + 4;

        for (int32_t i = 0; i < MPlayer->NumMovers; i++)
        {
            MCMover* mover = MPlayer->MoverRoster[i];
            Assert(mover != nullptr, 0, " handleAppMoveUpdate: No Mover ");
            mover->HandleMoveChunk(*reinterpret_cast<const uint32_t*>(chunks + i * 4));
        }

        const uint8_t* statusChunks = chunks + MPlayer->NumMovers * 4;

        for (int32_t i = 0; i < MPlayer->NumMovers; i++)
        {
            MPlayer->MoverRoster[i]->HandleStatusChunk(sequence - MPlayer->MoverUpdateSequence,
                                                       *reinterpret_cast<const uint32_t*>(statusChunks));
            statusChunks += 4;
        }

        const uint8_t* orderIds = statusChunks;

        for (int32_t i = 0; i < MPlayer->NumMovers; i++)
        {
            MPlayer->MoverRoster[i]->GetPilot()->UpdateClientOrderQueue(*orderIds);
            orderIds++;
        }
    }

    MPlayer->MoverUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

auto HandleAppTurretUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);
    uint16_t sequence = *reinterpret_cast<const uint16_t*>(bytes + 2);

    if (!InMultiplayerMission() || sequence < MPlayer->TurretUpdateSequence)
    {
        return;
    }

    if (MPlayer->IsServer == 0)
    {
        for (int32_t i = 0; i < MPlayer->NumTurrets; i++)
        {
            MCTurret* turret = MPlayer->TurretRoster[i];
            Assert(turret != nullptr, 0, " handleAppTurretUpdate: No Turret ");
            auto targetIndex = static_cast<int8_t>(bytes[4 + i]);

            // -1 is no target; 0-127 a mover of the roster. The original's third case (a part id from
            // 0x80248 down) can't be reached with a signed byte, so building targets are misread (OB-106).
            if (targetIndex < 0)
            {
                turret->Target = nullptr;
            }
            else
            {
                turret->Target = MPlayer->MoverRoster[targetIndex];
            }
        }
    }

    MPlayer->TurretUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

auto HandleAppMoverWeaponFireUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer != 0)
    {
        return;
    }

    auto firstMover = static_cast<int8_t>(bytes[8]);
    auto numMovers = static_cast<int8_t>(bytes[9]);
    int32_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        uint8_t numChunks = bytes[10 + i];
        MPlayer->MoverRoster[firstMover + i]->AddWeaponFireChunks(
            1, std::span(reinterpret_cast<const uint32_t*>(bytes + 0x22 + chunkIndex * 4), numChunks));
        chunkIndex += numChunks;
    }
}

auto HandleAppTurretWeaponFireUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer != 0)
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
        MPlayer->TurretRoster[entry >> 2]->AddWeaponFireChunks(
            1, std::span(reinterpret_cast<const uint32_t*>(bytes + 9 + numTurrets + chunkIndex * 4),
                         static_cast<size_t>(numChunks)));
        chunkIndex += numChunks;
    }
}

auto HandleAppMoverCriticalHitUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer != 0)
    {
        return;
    }

    auto firstMover = static_cast<int8_t>(bytes[8]);
    auto numMovers = static_cast<int8_t>(bytes[9]);
    int32_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = MPlayer->MoverRoster[firstMover + i];
        uint32_t numCriticalHits = bytes[10 + i];
        Assert(numCriticalHits < 0x81, numCriticalHits, " handleAppMoverCritHits: bad numCH ");

        if (numCriticalHits != 0)
        {
            mover->AddCriticalHitChunks(1, std::span(bytes + 0x3a + chunkIndex, numCriticalHits));
            chunkIndex += static_cast<int32_t>(numCriticalHits);
        }

        uint32_t numRadio = bytes[0x22 + i];
        Assert(numRadio < 8, numRadio, " handleAppMoverCritHits: bad numRDO ");

        if (numRadio != 0)
        {
            mover->AddRadioChunks(1, std::span(bytes + 0x3a + chunkIndex, numRadio));
            chunkIndex += static_cast<int32_t>(numRadio);
        }
    }
}

auto HandleAppWeaponHitUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer != 0)
    {
        return;
    }

    for (int32_t i = 0; i < bytes[8]; i++)
    {
        MCWeaponHitChunk chunk;
        chunk.TargetType = 0;
        chunk.TargetId = 0;
        chunk.TargetBlockOrTrainNumber = 0;
        chunk.TargetVertexOrCarNumber = 0;
        chunk.TargetItemNumber = 0;
        chunk.Cause = 0;
        chunk.Damage = 0.0f;
        chunk.HitLocation = -1;
        chunk.EntryAngle = 0;
        chunk.Refit = 0;
        std::memcpy(&chunk.Data, bytes + 9 + i * 4, sizeof(chunk.Data));
        chunk.Unpack();

        if (chunk.Refit == 0)
        {
            MCWeaponShotInfo shotInfo;
            shotInfo.Attacker = nullptr;
            shotInfo.MasterId = chunk.Cause;
            shotInfo.Damage = chunk.Damage;
            shotInfo.HitLocation = chunk.HitLocation;
            shotInfo.EntryAngle = HitEntryAngles[chunk.EntryAngle];

            if (chunk.TargetType == 0)
            {
                MPlayer->MoverRoster[chunk.TargetId]->HandleWeaponHit(&shotInfo, 0);
            }
            else if (chunk.TargetType == 1 || chunk.TargetType == 2)
            {
                static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(chunk.TargetId))
                    ->HandleWeaponHit(&shotInfo, 0);
            }
            else
            {
                Fatal(0, " Multiplayer.handleAppWeaponHitUpdate: bad targetType for weaponHit ");
            }
        }
        else
        {
            MCMover* target = nullptr;

            if (chunk.TargetType == 0)
            {
                target = MPlayer->MoverRoster[chunk.TargetId];
            }
            else if (chunk.TargetType == 1 || chunk.TargetType == 2)
            {
                target = static_cast<MCMover*>(ObjectList()->FindObjectFromPart(chunk.TargetId));
            }
            else
            {
                Fatal(0, " Multiplayer.handleAppWeaponHitUpdate: bad targetType for refit ");
            }

            float pointsUsed = 0.0f;
            DoRefit(target, chunk.Damage, pointsUsed, chunk.Damage == -6.0f ? 1 : 0);
        }
    }
}

auto HandleAppWorldStateUpdate(uint32_t fromID, const void* msg) -> void
{
    auto* bytes = static_cast<const uint8_t*>(msg);

    if (!InMultiplayerMission() || MPlayer->IsServer != 0)
    {
        return;
    }

    for (int32_t i = 0; i < bytes[8]; i++)
    {
        MCWorldStateChunk chunk;
        chunk.Type = 0;
        chunk.TileRow = -1;
        chunk.TileCol = -1;
        chunk.ObjectWid = 0;
        chunk.BlockNum = 0;
        chunk.VertexNum = 0;
        chunk.Item = 0;
        chunk.Param1 = 0;
        chunk.Param2 = 0;
        std::memcpy(&chunk.Data, bytes + 10 + i * 4, sizeof(chunk.Data));
        chunk.Unpack();

        switch (chunk.Type)
        {
            case WSCHUNK_MINE:
            {
                int32_t tileR = chunk.TileRow / 3;
                int32_t tileC = chunk.TileCol / 3;
                int32_t layout = chunk.Param2;

                if (layout > 3)
                {
                    layout = 3;
                }

                MCMapTile& tile = GameMap()->Map[tileR * GameMap()->Width + tileC];

                if (chunk.Param1 == 1)
                {
                    tile.Overlay = (tile.Overlay & 0xffff9fff) | (layout << 13);
                }
                else
                {
                    tile.Overlay = (tile.Overlay & 0xffffe7ff) | (layout << 11);
                }

                if (chunk.Param2 > 3)
                {
                    MCVector3D position;
                    position = MapCellToWorldPos(chunk.TileRow, chunk.TileCol);
                    position.Z = Terrain()->GetTerrainElevation(position);

                    if (chunk.Param2 == 4)
                    {
                        CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
                    }
                    else if (chunk.Param2 == 5)
                    {
                        CreateExplosion(MineExplosion, position, MineSplashDamage,
                                        MineSplashRange * WorldUnitsPerMeter);
                    }
                }

                break;
            }

            case WSCHUNK_TERRAIN_FIRE:
            {
                auto* object = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(chunk.ObjectWid));

                if (object != nullptr && object->GetObjectType() != nullptr)
                {
                    switch (object->ObjectClass)
                    {
                        case MCObjectClass::Building:
                        {
                            static_cast<MCBuilding*>(object)->LightOnFire(static_cast<float>(chunk.Param1));
                            break;
                        }

                        case MCObjectClass::Tree:
                        {
                            static_cast<MCTree*>(object)->LightOnFire(static_cast<float>(chunk.Param1));
                            break;
                        }

                        case MCObjectClass::MiscTerrainObject:
                        {
                            static_cast<MCTerrainObject*>(object)->LightOnFire(static_cast<float>(chunk.Param1));
                            break;
                        }

                        case MCObjectClass::TreeBuilding:
                        {
                            static_cast<MCTreeBuilding*>(object)->LightOnFire(static_cast<float>(chunk.Param1));
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
                MCVector3D location;
                location = MapCellToWorldPos(chunk.TileRow, chunk.TileCol);
                location.Z = Terrain()->GetTerrainElevation(location);
                CallArtillery(chunk.Type - WSCHUNK_ARTILLERY, chunk.Param1, location, chunk.Param2, 0);
                break;
            }

            case WSCHUNK_MISSION_SCRIPT_MESSAGE:
            {
                Scenario->HandleMultiplayMessage(chunk.Param1, chunk.Param2);
                break;
            }

            case WSCHUNK_PILOT_KILL_STAT:
            {
                MCMover* mover = MPlayer->MoverRoster[chunk.Param1];

                if (mover != nullptr)
                {
                    mover->GetPilot()->NumKilled[chunk.Param2][1]++;

                    if (mover->GetPilot()->OnHomeTeam() != 0)
                    {
                        mover->GetPilot()->RadioMessage(KillRadioMessages[chunk.Param2], 0);
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

auto HandleAppPlayerUpdate(uint32_t fromID, const void* msg) -> void
{
    Assert(MPlayer->IsServer != 0, 0, " Sending player update to non-server ");
}

auto GetCheckSum(char* fileName) -> uint32_t
{
    MCFile file;

    if (file.Open(fileName) != 0)
    {
        return 0;
    }

    uint32_t length = file.GetLength();
    auto* contents = static_cast<uint8_t*>(std::malloc(length));

    if (contents == nullptr)
    {
        return 0;
    }

    file.Read(contents, length);
    // Only the file's first four bytes: the "checksum" tells little more than the file's existence.
    uint32_t checkSum = 0;
    std::memcpy(&checkSum, contents, length < 4 ? length : 4);
    std::free(contents);
    file.Close();
    return checkSum;
}

auto HandleAppFileInquiry(uint32_t fromID, const void* msg) -> void
{
    auto* inquiry = static_cast<const MCMPFileNameMessage*>(msg);
    // The inquiry's name starts at +0x8 (sendFileInquiry writes it there), not at MPFileNameMessage's +0xc.
    const char* fileName = reinterpret_cast<const char*>(inquiry) + 8;
    uint32_t checkSum = GetCheckSum(const_cast<char*>(fileName));
    auto* report = reinterpret_cast<MCMPFileNameMessage*>(MPlayer->MsgBuffer);

    for (int32_t i = 0; i < 6; i++)
    {
        report->Tagger.SendCount[i] = 0;
    }

    report->Header = 0;
    report->Header |= FIMSG_GUARANTEED;
    report->Header &= 0xfc00;
    report->Header |= MPMSG_FILE_REPORT;
    report->Unused = static_cast<int32_t>(checkSum);
    std::memcpy(report->FileName, fileName, std::strlen(fileName) + 1);
    MPlayer->SessionManager->SendMessageToPlayerGuaranteed(
        fromID, report, static_cast<uint32_t>(std::strlen(report->FileName) + 0xd), 1);
}

auto HandleAppFileReport(uint32_t fromID, const void* msg) -> void
{
    int haveFile = static_cast<const MCMPLongMessage*>(msg)->Value != 0 ? 1 : 0;

    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->FileReport(fromID, haveFile);
    }
}

auto HandleAppLoadMission(uint32_t fromID, const void* msg) -> void
{
    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->LoadMission(
            const_cast<char*>(static_cast<const MCMPFileNameMessage*>(msg)->FileName));
    }
}

auto HandleAppStart(uint32_t fromID, const void* msg) -> void
{
    Application->RemoveTimer(GlobalLogPtr->SessionScreen, 0);
    MPlayer->SessionManager->SendLatencyInfo();
    SoundSystem()->PlayBettySample(0x19);
    GlobalLogPtr->InitializeMultiplayer();
    char extension[] = ".MPK";
    GlobalLogPtr->LoadCampaign(const_cast<char*>(static_cast<const MCMPFileNameMessage*>(msg)->FileName), extension, 0,
                               0);
    GlobalLogPtr->SetUpBriefingScreen(0);
}

auto LostConnectionDialogExit() -> void
{
    if (LaunchedFromLobby == 0)
    {
        if (MPlayer != nullptr)
        {
            delete MPlayer;
            MPlayer = nullptr;
        }
    }
    else
    {
        KillTheGame();
    }
}

auto HandleLocalPlayerRemoved(uint32_t fromID, const void* msg) -> void
{
    LastConnectionType = static_cast<uint32_t>(MPlayer->SessionManager->CurrentConnection);

    if (PrepareScenarioReceived == 0 && MPlayer->InMission == 0)
    {
        MPlayer->InLogistics = 0;
        GlobalLogPtr->DestroyMultiplayer();

        if (GlobalLogPtr->CurrentScreen != GlobalLogPtr->MainScreen)
        {
            WhackTimer = 1;
        }

        GlobalLogPtr->CurrentScreen->ShowGuiWindow(0);
        GlobalLogPtr->CurrentScreen = GlobalLogPtr->MainScreen;
        GlobalLogPtr->LogisticsState = 1;
        GlobalLogPtr->ShowLogScreen(1, 1);
        MPlayer->LeaveSession();
    }
    else
    {
        MPlayer->InMission = 0;
        MPlayer->LeaveSession();
        Mission->EndScenario();
    }

    char text[512];
    CLoadString(ThisInstance, 0x369, text, 0xfe);
    MCReusableDialog* dialog = GlobalLogPtr->MessageDialog;
    dialog->SetText(text);
    dialog->SetTwoButton(0);
    dialog->Callback = nullptr;
    dialog->OkButton->Callback()->SetExec(LostConnectionDialogExit);
    char upArt[] = "bh_okay.tga";
    char downArt[] = "bg_okay.tga";
    dialog->OkButton->SetUpPicture(upArt);
    dialog->OkButton->SetDownPicture(downArt);
    dialog->OkButton->Disabled = 0;
    dialog->OkButton->Draw();
    dialog->Activate();
}

auto MultiPlayerSystemCallback(MCFidpMessage* msg, void* data) -> void
{
    auto* system = reinterpret_cast<DPMSG_GENERIC*>(msg->MessageBuffer);

    switch (system->dwType)
    {
        case DPSYS_CREATEPLAYERORGROUP:
        {
            if (reinterpret_cast<DPMSG_CREATEPLAYERORGROUP*>(system)->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                HandleSysCreatePlayer(system);
            }

            break;
        }

        case DPSYS_DESTROYPLAYERORGROUP:
        {
            auto* destroy = reinterpret_cast<DPMSG_DESTROYPLAYERORGROUP*>(system);

            if (destroy->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                MPlayer->PlayerLeftGame(destroy->dpId);
            }

            break;
        }

        case DPSYS_ADDPLAYERTOGROUP:
        {
            HandleSysAddPlayerToGroup(system);
            break;
        }

        case DPSYS_SESSIONLOST:
        {
            if (Scenario == nullptr || Scenario->StartingUp == 0)
            {
                HandleLocalPlayerRemoved(msg->FromID, nullptr);
            }
            else
            {
                BadSessionCounter++;

                if (BadSessionCounter > 10)
                {
                    KillTheGame();
                }
            }

            break;
        }
    }
}

auto MultiPlayerApplicationCallback(MCFidpMessage* msg, void* data) -> void
{
    const uint32_t fromID = msg->FromID;
    const void* message = msg->MessageBuffer;

    switch (*reinterpret_cast<const uint16_t*>(message) & FIMSG_TYPE_MASK)
    {
        case MPMSG_NEW_SERVER:
        {
            HandleAppNewServer(fromID, message);
            break;
        }

        case MPMSG_PLAYER_REMOVED:
        {
            HandleLocalPlayerRemoved(fromID, message);
            break;
        }

        case MPMSG_LATENCY:
        {
            if (MPlayer->SessionManager->ReadyToChooseServer() != 0)
            {
                MPlayer->SwitchServers();
            }

            break;
        }

        case MPMSG_CHAT:
        {
            MPlayer->ChatCallback(msg, nullptr);
            break;
        }

        case MPMSG_PLAYER_CHECK_IN:
        {
            HandleAppPlayerCheckIn(fromID, message);
            break;
        }

        case MPMSG_PLAYER_SETUP:
        {
            HandleAppPlayerSetup(fromID, message);
            break;
        }

        case MPMSG_PLAYER_CHECK_IN_RECEIPT:
        {
            HandleAppPlayerCheckInReceipt(fromID, message);
            break;
        }

        case MPMSG_START_PLANNING:
        {
            HandleAppStartPlanning(fromID, message);
            break;
        }

        case MPMSG_START_SCENARIO:
        {
            HandleAppStartScenario(fromID, message);
            break;
        }

        case MPMSG_END_SCENARIO:
        {
            HandleAppEndScenario(fromID, message);
            break;
        }

        case MPMSG_PLAYER_ORDER:
        {
            HandleAppPlayerOrder(fromID, message);
            break;
        }

        case MPMSG_PLAYER_MOVER_GROUP:
        {
            HandleAppPlayerMoverGroup(fromID, message);
            break;
        }

        case MPMSG_PLAYER_ARTILLERY:
        {
            HandleAppPlayerArtillery(fromID, message);
            break;
        }

        case MPMSG_MOVER_UPDATE:
        {
            HandleAppMoverUpdate(fromID, message);
            break;
        }

        case MPMSG_TURRET_UPDATE:
        {
            HandleAppTurretUpdate(fromID, message);
            break;
        }

        case MPMSG_MOVER_WEAPON_FIRE_UPDATE:
        {
            HandleAppMoverWeaponFireUpdate(fromID, message);
            break;
        }

        case MPMSG_TURRET_WEAPON_FIRE_UPDATE:
        {
            HandleAppTurretWeaponFireUpdate(fromID, message);
            break;
        }

        case MPMSG_MOVER_CRITICAL_HIT_UPDATE:
        {
            HandleAppMoverCriticalHitUpdate(fromID, message);
            break;
        }

        case MPMSG_WEAPON_HIT_UPDATE:
        {
            HandleAppWeaponHitUpdate(fromID, message);
            break;
        }

        case MPMSG_WORLD_STATE_UPDATE:
        {
            HandleAppWorldStateUpdate(fromID, message);
            break;
        }

        case MPMSG_DEPLOY_FORCE:
        {
            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandleDeployForceMessage(fromID, message);
            }

            break;
        }

        case MPMSG_REMOVE_FORCE:
        {
            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandleRemoveForceMessage(fromID, message);
            }

            break;
        }

        case MPMSG_PLAYER_UPDATE:
        {
            HandleAppPlayerUpdate(fromID, message);
            break;
        }

        case MPMSG_PREPARE_SCENARIO:
        {
            for (int32_t i = 0; i < 6; i++)
            {
                MPlayer->PlayerCheckedIn[i] = 0;
            }

            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandlePrepareScenarioMessage();
            }

            PrepareScenarioReceived = 1;
            break;
        }

        case MPMSG_READY_FOR_BATTLE:
        {
            HandleAppReadyForBattle(fromID, message);
            break;
        }

        case MPMSG_FILE_INQUIRY:
        {
            HandleAppFileInquiry(fromID, message);
            break;
        }

        case MPMSG_FILE_REPORT:
        {
            HandleAppFileReport(fromID, message);
            break;
        }

        case MPMSG_LOAD_MISSION:
        {
            HandleAppLoadMission(fromID, message);
            break;
        }

        case MPMSG_START:
        {
            HandleAppStart(fromID, message);
            break;
        }

        case MPMSG_JOIN_TEAM:
        {
            HandleAppJoinTeam(fromID, message);
            break;
        }

        case MPMSG_SWITCH_SCREEN:
        {
            HandleAppSwitchScreen(fromID, message);
            break;
        }

        case MPMSG_RP_UPDATE:
        {
            HandleAppRPUpdate(fromID, message);
            break;
        }

        case MPMSG_TECHBASE_CHANGE:
        {
            HandleAppTechbaseChange(fromID, message);
            break;
        }

        case MPMSG_SESSION_CHECK_IN:
        {
            int32_t playerNumber = MPlayer->SessionManager->GetPlayer(fromID)->PlayerNumber;
            Assert(playerNumber >= 0 && playerNumber <= 5, 0, "PNUM BAD");
            MPlayer->PlayerSessionCheckIn[playerNumber] = 1;

            if (GlobalLogPtr != nullptr && GlobalLogPtr->SessionScreen != nullptr)
            {
                GlobalLogPtr->SessionScreen->SomeoneCheckedIn();
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
