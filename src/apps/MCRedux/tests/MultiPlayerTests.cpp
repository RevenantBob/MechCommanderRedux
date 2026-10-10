#include "stdafx.h"
#include "MCTest.h"
#include "MCMock.h"
#include "main/MCGameContext.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "network/MCMultiPlayerHandlers.h"
#include "object/MCBattleMech.h"
#include "object/MCMechWarrior.h"
#include "object/MCTurret.h"

// The game's side of multiplayer: the packed world-state and weapon-hit chunks the server sends, the queues they wait
// in, and the handlers that apply the server's updates on a client.

namespace
{
    /// <summary>Gives <paramref name="scope"/> a new multiplayer game (no session).</summary>
    MCMultiPlayer& InstallGame(MCTestContextScope& scope)
    {
        scope.Context().SetMultiPlayer(std::make_unique<MCMultiPlayer>());
        return *scope.Context().MultiPlayer();
    }

    /// <summary>A mover whose network updates the test can watch.</summary>
    class FakeMover : public MCBattleMech
    {
    public:
        int32_t HandleMoveChunk(uint32_t chunk) override
        {
            MoveChunks(chunk);
            return 0;
        }

        int32_t HandleStatusChunk(int32_t updateAge, uint32_t chunk) override
        {
            StatusChunks(updateAge, chunk);
            return 0;
        }

        MCMechWarrior* GetPilot() override { return &FakePilot; }

        MCMock::Calls<uint32_t> MoveChunks;
        MCMock::Calls<int32_t, uint32_t> StatusChunks;
        MCMechWarrior FakePilot;
    };

    /// <summary>A client of a two-player LAN game in a running mission.</summary>
    struct ClientGame
    {
        ClientGame()
        {
            Scope.Context().SetScenario(std::make_unique<MCScenario>());
            Game = &InstallGame(Scope);
            NumLanPlayers = 2;
            Game->InMission = true;
        }

        ~ClientGame() { NumLanPlayers = 1; }

        MCTestContextScope Scope;
        MCMultiPlayer* Game = nullptr;
    };

    /// <summary>A message of the game's: its header word, then <paramref name="body"/>.</summary>
    std::vector<uint8_t> Message(MCMPMessageType type, std::initializer_list<uint8_t> body)
    {
        std::vector<uint8_t> bytes{static_cast<uint8_t>(type), 0};
        bytes.insert(bytes.end(), body);
        return bytes;
    }

    /// <summary>Appends a little-endian word.</summary>
    void PutWord(std::vector<uint8_t>& bytes, uint32_t word)
    {
        for (int i = 0; i < 4; i++)
        {
            bytes.push_back(static_cast<uint8_t>(word >> (8 * i)));
        }
    }

    /// <summary>A chunk packed, then unpacked again from its word alone.</summary>
    MCWorldStateChunk RoundTrip(MCWorldStateChunk& chunk)
    {
        chunk.Pack();
        MCWorldStateChunk received;
        received.Data = chunk.Data;
        received.Unpack();
        return received;
    }
}

/// <summary>
/// Each world event packs into one word, the 4-bit type at the bottom: a mine (column 4-13, row 14-23, state 24-26,
/// team 27), a terrain fire (item 4-6, vertex 7-15, block 16-23, seconds 24-31), an artillery strike (column, row,
/// seconds + 1 at 24-28, strike type 29-31), a script message (message 4-11, value + 32000 at 12-27) and a pilot's kill
/// (kind 4-6, mover 7-11).
/// </summary>
TEST_CASE("network: each world-state chunk packs into its word and unpacks to the same event")
{
    MCWorldStateChunk mine;
    mine.BuildMine(5, 7, 1, 3, 2);
    MCWorldStateChunk received = RoundTrip(mine);
    CHECK_EQ(mine.Data, (7u << 4) | (5u << 14) | (5u << 24) | (1u << 27));
    CHECK(received.EqualTo(mine));
    CHECK_EQ(received.Param2, 5);

    // A fire on terrain object 0x1000 + block 3 * 0xc80 + vertex 17 * 8 + item 5, for 40 seconds.
    MCBattleMech burning;
    burning.PartId = 0x1000 + 3 * 0xc80 + 17 * 8 + 5;
    MCWorldStateChunk fire;
    fire.BuildTerrainFire(burning, 40);
    received = RoundTrip(fire);
    CHECK_EQ(fire.Data, 1u | (5u << 4) | (17u << 7) | (3u << 16) | (40u << 24));
    CHECK(received.EqualTo(fire));
    CHECK_EQ(received.ObjectWid, burning.PartId);

    // OB-104 (kept): only 6 of the seconds' 8 bits are unpacked, so 100 seconds come back as 36.
    fire.BuildTerrainFire(burning, 100);
    CHECK_EQ(RoundTrip(fire).Param1, 100 - 64);

    // Commander 3's strike of type 6 at cell (100, 200), in 30 seconds; and one landing now (-1).
    MCWorldStateChunk strike;
    strike.Type = MCWorldStateChunk::Artillery + 3;
    strike.TileRow = 100;
    strike.TileCol = 200;
    strike.Param1 = 6;
    strike.Param2 = 30;
    received = RoundTrip(strike);
    CHECK_EQ(strike.Data, 5u | (200u << 4) | (100u << 14) | (31u << 24) | (6u << 29));
    CHECK(received.EqualTo(strike));
    strike.Param2 = -1;
    CHECK_EQ(RoundTrip(strike).Param2, -1);

    MCWorldStateChunk script;
    script.BuildMissionScriptMessage(200, -1234);
    received = RoundTrip(script);
    CHECK_EQ(script.Data, 8u | (200u << 4) | (static_cast<uint32_t>(32000 - 1234) << 12));
    CHECK_EQ(received.Param2, -1234);
    script.BuildMissionScriptMessage(0, 32000);
    CHECK_EQ(RoundTrip(script).Param2, 32000);

    MCWorldStateChunk kill;
    kill.BuildPilotKillStat(17, 4, 24);
    received = RoundTrip(kill);
    CHECK_EQ(kill.Data, 9u | (4u << 4) | (17u << 7));
    CHECK(received.EqualTo(kill));
}

/// <summary>
/// A hit on a mover packs, from the low bit: target type 0 (2 bits), damage in quarter points (10), the roster index
/// (12-16), cause + 7 (17-19), hit location + 2 (20-23), entry angle (24-25) and the refit flag (26).
/// </summary>
TEST_CASE("network: a weapon hit on a mover packs its damage in quarter points and its roster index")
{
    MCTestContextScope scope;
    MCMultiPlayer& game = InstallGame(scope);
    game.NumMovers = 4;

    MCWeaponHitChunk hit = EmptyWeaponHitChunk();
    hit.TargetType = 0;
    hit.TargetId = 3;
    hit.Cause = -2;
    hit.Damage = 12.25f;
    hit.HitLocation = 4;
    hit.EntryAngle = 2;
    CHECK_EQ(game.AddWeaponHitChunk(hit), 1);
    CHECK_EQ(hit.Data, 0u | (49u << 2) | (3u << 12) | (5u << 17) | (6u << 20) | (2u << 24));

    MCWeaponHitChunk received = EmptyWeaponHitChunk();
    received.Data = hit.Data;
    received.Unpack();
    CHECK(received.EqualTo(&hit) != 0);
    CHECK_EQ(received.Damage, 12.25f);
}

/// <summary>
/// The server's chunks wait in queues without a limit: world events with a tally per type (sent all at once, the
/// tally then cleared), weapon hits handed out a message's worth at a time, oldest first.
/// </summary>
TEST_CASE("network: chunks queue in order, world events tallied by type, hits taken a message's worth at a time")
{
    MCTestContextScope scope;
    MCMultiPlayer& game = InstallGame(scope);
    game.NumMovers = 4;

    for (int32_t i = 0; i < 1100; i++)
    {
        game.AddMineChunk(i % 1000, 7, 0, 1, 0);
    }

    CHECK_EQ(game.AddMissionScriptMessageChunk(3, 9), 1101);
    FakeMover mover;
    mover.NetRosterIndex = 1;
    CHECK_EQ(game.AddPilotKillStat(&mover, 2), 1102);
    CHECK_EQ(game.WorldStateChunkTally[MCWorldStateChunk::Mine], 1100);
    CHECK_EQ(game.WorldStateChunkTally[MCWorldStateChunk::MissionScriptMessage], 1);
    CHECK_EQ(game.WorldStateChunkTally[MCWorldStateChunk::PilotKillStat], 1);
    CHECK_EQ(game.WorldStateChunks[1] >> 14 & 0x3ff, 1u);

    for (int32_t i = 0; i < 3; i++)
    {
        MCWeaponHitChunk hit = EmptyWeaponHitChunk();
        hit.TargetId = i;
        hit.Damage = static_cast<float>(i + 1);
        game.AddWeaponHitChunk(hit);
    }

    const std::vector<uint32_t> queued = game.WeaponHitChunks;
    std::array<uint32_t, 2> first{};
    CHECK_EQ(game.GrabWeaponHitChunks(first), size_t{2});
    CHECK_EQ(first[0], queued[0]);
    CHECK_EQ(first[1], queued[1]);
    CHECK(game.WeaponHitChunks == (std::vector<uint32_t>{queued[2]}));

    // A new game starts with empty queues.
    game.ResetForNewGame();
    CHECK(game.WorldStateChunks.empty());
    CHECK(game.WeaponHitChunks.empty());
}

/// <summary>The multiplayer game is a context system: absent in a single-player game, a scope's own while it lasts.</summary>
TEST_CASE("network: the multiplayer game is the context's, and gone with it")
{
    CHECK(MultiPlayer() == nullptr);

    {
        MCTestContextScope scope;
        MCMultiPlayer& game = InstallGame(scope);
        CHECK(MultiPlayer() == &game);
        // A new game: nobody checked in, no team, no roles yet.
        CHECK_EQ(game.CheckInId, -1);
        CHECK_EQ(game.HomeTeam, -1);
        CHECK(!game.IsServer);
        CHECK_EQ(NumLanPlayers, 1);
        CHECK(game.SessionManager == nullptr);
        scope.Context().SetMultiPlayer(nullptr);
        CHECK(MultiPlayer() == nullptr);
    }

    CHECK(MultiPlayer() == nullptr);
}

/// <summary>
/// A client applies the server's mover update: each rostered mover gets its move chunk, its status chunk with the
/// update's age, and its pilot the id of the order it runs. An update older than the last one is dropped.
/// </summary>
TEST_CASE("network: a client hands each mover its move and status chunks and drops an older update")
{
    ClientGame client;
    MCMultiPlayer& game = *client.Game;
    FakeMover first;
    FakeMover second;
    game.AddToMoverRoster(&first);
    game.AddToMoverRoster(&second);
    CHECK_EQ(second.NetRosterIndex, 1);
    game.MoverUpdateSequence = 4;

    // Update 6: two move chunks, two status chunks, two order ids.
    std::vector<uint8_t> update = Message(MCMPMessageType::MoverUpdate, {6, 0});
    PutWord(update, 0x11111111);
    PutWord(update, 0x22222222);
    PutWord(update, 0x33333333);
    PutWord(update, 0x44444444);
    update.push_back(9);
    update.push_back(12);
    HandleAppMoverUpdate(1, update);
    CHECK_CALLED_WITH(first.MoveChunks, 0x11111111u);
    CHECK_CALLED_WITH(second.MoveChunks, 0x22222222u);
    // Two updates (4 and 5) were missed: the status is two updates old.
    CHECK_CALLED_WITH(first.StatusChunks, 2, 0x33333333u);
    CHECK_CALLED_WITH(second.StatusChunks, 2, 0x44444444u);
    CHECK_EQ(first.FakePilot.LastTacOrderId, 9);
    CHECK_EQ(second.FakePilot.LastTacOrderId, 12);
    CHECK_EQ(int{game.MoverUpdateSequence}, 7);

    // Update 5 arrives late: nothing happens.
    update[2] = 5;
    HandleAppMoverUpdate(1, update);
    CHECK_CALLED(first.MoveChunks, 1);
    CHECK_EQ(int{game.MoverUpdateSequence}, 7);

    // The server sends these; it doesn't apply them, but counts on from them.
    game.IsServer = true;
    update[2] = 8;
    HandleAppMoverUpdate(1, update);
    CHECK_CALLED(first.MoveChunks, 1);
    CHECK_EQ(int{game.MoverUpdateSequence}, 9);
}

/// <summary>
/// A client points each rostered turret at the mover the server's turret update names by roster index; 0xff is no
/// target. Outside a mission with company, the update is ignored.
/// </summary>
TEST_CASE("network: a client aims each turret at the rostered mover the server names")
{
    ClientGame client;
    MCMultiPlayer& game = *client.Game;
    FakeMover target;
    game.AddToMoverRoster(&target);
    MCTurret aiming;
    MCTurret idle;
    game.AddToTurretRoster(&aiming);
    game.AddToTurretRoster(&idle);
    CHECK_EQ(idle.NetRosterIndex, 1);
    idle.Target = &target;

    HandleAppTurretUpdate(1, Message(MCMPMessageType::TurretUpdate, {0, 0, 0, 0xff}));
    CHECK(aiming.Target == &target);
    CHECK(idle.Target == nullptr);
    CHECK_EQ(int{game.TurretUpdateSequence}, 1);

    // Alone in the session (the other player left), the game is no longer synchronised.
    NumLanPlayers = 1;
    HandleAppTurretUpdate(1, Message(MCMPMessageType::TurretUpdate, {1, 0, 0xff, 0}));
    CHECK(aiming.Target == &target);
    CHECK_EQ(int{game.TurretUpdateSequence}, 1);
}

/// <summary>The session messages a client keeps: the planning has started, and the scenario's result.</summary>
TEST_CASE("network: the start-planning and end-scenario messages set the game's state")
{
    ClientGame client;
    MCMultiPlayer& game = *client.Game;
    game.InMission = false;
    HandleAppStartPlanning(1, Message(MCMPMessageType::StartPlanning, {0, 0, 0, 0, 0, 0}));
    CHECK(game.InLogistics);

    std::vector<uint8_t> end = Message(MCMPMessageType::EndScenario, {0, 0, 0, 0, 0, 0});
    PutWord(end, 2);
    HandleAppEndScenario(1, end);
    CHECK_EQ(game.ScenarioResult, 2);
}
