#include "stdafx.h"
#include "network/MCMultiPlayerHandlers.h"
#include "network/MCMultiPlayer.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCRefit.h"
#include "ai/MCTacticalOrder.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCCommandParser.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "logistics/MCConnectMenu.h"
#include "logistics/MCLogToolButton.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSessionScreen.h"
#include "logistics/MCSplashScreen.h"
#include "main/MCGameSession.h"
#include "main/MCGameStrings.h"
#include "main/MCLogistics.h"
#include "main/MCMissionGlobals.h"
#include "mission/MCMission.h"
#include "mission/MCMissionResultsScreen.h"
#include "mission/MCScenario.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCBuilding.h"
#include "object/MCExplosion.h"
#include "object/MCExplosionType.h"
#include "object/MCForces.h"
#include "object/MCMechGameSystem.h"
#include "object/MCMechWarrior.h"
#include "object/MCMover.h"
#include "object/MCMoverGroup.h"
#include "object/MCObjectSystem.h"
#include "object/MCTerrainObject.h"
#include "object/MCTree.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTurret.h"
#include "object/MCWeaponShotInfo.h"
#include "sound/MCSoundSystem.h"
#include "terrain/MCTerrain.h"

// The handlers of the game's messages: what the server tells the clients (scenario start and end, the movers',
// turrets' and weapons' updates, the world's events), what the clients tell the server (check-ins, orders, artillery)
// and the session screen's own messages, and the SessionManager callbacks that dispatch to them.

namespace
{
    /// <summary>The entry angle of a weapon hit (WeaponHitChunk::EntryAngle) in degrees.</summary>
    constexpr std::array<float, 4> HitEntryAngles = {0.0f, 180.0f, -90.0f, 90.0f};

    /// <summary>The radio message a pilot sends for a kill, by kill kind.</summary>
    /// <remarks>
    /// The original's table has six entries; kinds 6 and 7 (allowed by BuildPilotKillStat) read on into the next
    /// data, 0 and the bytes of a string.
    /// </remarks>
    constexpr std::array<int32_t, 8> KillRadioMessages = {19, 19, 19, 19, 18, 17, 0, 0};

    /// <summary>The multiplayer game (the handlers only run with one).</summary>
    MCMultiPlayer& Game()
    {
        return *MultiPlayer();
    }

    /// <summary>Whether a mission is running a multiplayer game with company (every in-mission handler's check).</summary>
    bool InMultiplayerMission()
    {
        return Scenario() != nullptr && EventsToMissionResultsScreen == 0 && Game().NumPlayers() > 1;
    }

    /// <summary>The value at <paramref name="offset"/> of <paramref name="bytes"/> (zero past their end).</summary>
    template <class T> T Get(std::span<const uint8_t> bytes, size_t offset)
    {
        return bytes.size() > offset ? ReadLinkupMessage<T>(bytes.subspan(offset)) : T{};
    }

    /// <summary>The byte at <paramref name="offset"/> of <paramref name="bytes"/> (0 past their end).</summary>
    uint8_t ByteAt(std::span<const uint8_t> bytes, size_t offset)
    {
        return offset < bytes.size() ? bytes[offset] : 0;
    }

    /// <summary><paramref name="count"/> packed words from <paramref name="offset"/> of <paramref name="bytes"/>.</summary>
    std::vector<uint32_t> Words(std::span<const uint8_t> bytes, size_t offset, size_t count)
    {
        std::vector<uint32_t> words(count);

        for (size_t i = 0; i < count; i++)
        {
            words[i] = Get<uint32_t>(bytes, offset + i * 4);
        }

        return words;
    }

    /// <summary>
    /// <paramref name="count"/> bytes from <paramref name="offset"/> of <paramref name="bytes"/>, as many as there are.
    /// </summary>
    std::span<const uint8_t> BytesAt(std::span<const uint8_t> bytes, size_t offset, size_t count)
    {
        return offset < bytes.size() ? bytes.subspan(offset, std::min(count, bytes.size() - offset))
                                     : std::span<const uint8_t>();
    }

    /// <summary>Sets up the dialog's single OK button with the given exit.</summary>
    void ShowOkDialog(MCReusableDialog& dialog, std::string_view text, void (*exit)())
    {
        dialog.SetText(text);
        dialog.SetTwoButton(false);
        dialog.Callback = nullptr;
        dialog.OkButton->Callback()->SetExec(exit);
        char upArt[] = "bh_okay.tga";
        char downArt[] = "bg_okay.tga";
        dialog.OkButton->SetUpPicture(upArt);
        dialog.OkButton->SetDownPicture(downArt);
        dialog.OkButton->Disabled = false;
        dialog.OkButton->Draw();
        dialog.Activate();
    }
}

MCWeaponHitChunk EmptyWeaponHitChunk()
{
    MCWeaponHitChunk chunk;
    chunk.HitLocation = -1;
    return chunk;
}

void HandleSysCreatePlayer(const DPMSG_CREATEPLAYERORGROUP& msg)
{
    MCMultiPlayer& game = Game();

    if (game.SessionManager->CurrentConnection != MCNetProtocol::Lobby)
    {
        NumLanPlayers++;
    }

    if (game.IsServer)
    {
        game.SendPlayerSetup(msg.dpId, game.InnerSphereGroupID, game.ClanGroupID);
    }
}

void HandleSysAddPlayerToGroup(const DPMSG_ADDPLAYERTOGROUP& msg)
{
    MCMultiPlayer& game = Game();
    const MCFidpPlayer* player = game.SessionManager->GetPlayer(msg.dpIdPlayer);

    if (player != nullptr && player->PlayerNumber >= 0 && player->PlayerNumber < MaxLinkupPlayers)
    {
        game.PlayerTeams[player->PlayerNumber] = {msg.dpIdPlayer, static_cast<int32_t>(msg.dpIdGroup)};
    }
}

void HandleAppChat(MCFidpMessage& msg)
{
    const MCFidpPlayer* player = Game().SessionManager->GetPlayer(msg.FromID);
    // Port fix: a sender already gone from the session has no name; the original read through the null player.
    const std::string line = std::format("{}: {}", player != nullptr ? player->Name : std::string(),
                                         MessageText(msg.Bytes(), sizeof(MCMPChatMessage)));

    if (AblGetDebugger() != nullptr)
    {
        // The original formatted it into 256 bytes.
        AblGetDebugger()->Print(std::string_view(line).substr(0, 0xff));
    }
}

void HandleAppNewServer(uint32_t, std::span<const uint8_t> msg)
{
    Game().SetServer(static_cast<uint32_t>(ReadLinkupMessage<MCMPLongMessage>(msg).Value));
}

void HandleAppPlayerCheckIn(uint32_t fromID, std::span<const uint8_t> msg)
{
    const auto checkIn = ReadLinkupMessage<MCMPPlayerCheckInMessage>(msg);
    MCMultiPlayer& game = Game();

    if (GlobalLogPtr != nullptr && GlobalLogPtr->PlayerLights != nullptr)
    {
        GlobalLogPtr->PlayerLights->SetPlayerStatus(fromID, 2);
    }

    if (game.IsServer)
    {
        Assert(checkIn.CheckInId >= 0 && checkIn.CheckInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        game.PlayerCheckedIn[checkIn.CheckInId] = static_cast<int32_t>(fromID);

        if (game.AllPlayersCheckedIn())
        {
            game.HandleOwnMessages = true;
            game.SendStartScenario(game.PlayerCheckedIn, "");
            game.HandleOwnMessages = false;
        }
    }
}

void HandleAppPlayerSetup(uint32_t, std::span<const uint8_t> msg)
{
    const auto setup = ReadLinkupMessage<MCMPPlayerSetupMessage>(msg);
    MCMultiPlayer& game = Game();
    MCSessionManager& sessionManager = *game.SessionManager;

    if (game.CheckInId == -1)
    {
        game.CheckInId = sessionManager.MyPlayer->PlayerNumber;
    }

    game.InitUpdateFrequencies();
    game.HostID = sessionManager.ServerID;
    game.ServerID = game.HostID;
    game.AllPlayerGroupID = setup.AllPlayerGroupID;
    game.ClanGroupID = setup.ClanGroupID;
    game.InnerSphereGroupID = setup.InnerSphereGroupID;

    if (game.HomeTeam == 0)
    {
        sessionManager.AddPlayerToGroup(game.InnerSphereGroupID, 0);
        game.HomeTeamGroupID = game.InnerSphereGroupID;
        game.EnemyTeamGroupID = game.ClanGroupID;
    }
    else if (game.HomeTeam == 1)
    {
        sessionManager.AddPlayerToGroup(game.ClanGroupID, 0);
        game.HomeTeamGroupID = game.ClanGroupID;
        game.EnemyTeamGroupID = game.InnerSphereGroupID;
    }

    sessionManager.AddPlayerToGroup(game.AllPlayerGroupID, 0);
}

void HandleAppPlayerCheckInReceipt(uint32_t fromID, std::span<const uint8_t> msg)
{
    const int32_t checkInId = ReadLinkupMessage<MCMPLongMessage>(msg).Value;
    MCMultiPlayer& game = Game();
    Assert(checkInId >= 0 && checkInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
    game.PlayerCheckedIn[checkInId] = static_cast<int32_t>(fromID);

    if (game.AllPlayersCheckedIn())
    {
        game.HandleOwnMessages = true;
        game.SendStartPlanning();
        game.HandleOwnMessages = false;
    }
}

void HandleAppStartPlanning(uint32_t, std::span<const uint8_t>)
{
    Game().InLogistics = true;
}

void HandleAppReadyForBattle(uint32_t fromID, std::span<const uint8_t> msg)
{
    const auto ready = ReadLinkupMessage<MCMPPlayerCheckInMessage>(msg);
    MCMultiPlayer& game = Game();

    if (GlobalLogPtr == nullptr)
    {
        return;
    }

    GlobalLogPtr->PlayerLights->SetPlayerStatus(fromID, 2);

    if (game.IsHost)
    {
        Assert(ready.CheckInId >= 0 && ready.CheckInId <= 5, 0, "CHECKIN MESSAGE CRAPPED");
        game.PlayerCheckedIn[ready.CheckInId] = static_cast<int32_t>(fromID);

        if (game.AllPlayersCheckedIn())
        {
            game.SendPrepareScenario();
        }
    }
}

void HandleAppJoinTeam(uint32_t, std::span<const uint8_t> msg)
{
    const auto join = ReadLinkupMessage<MCMPJoinTeamMessage>(msg);

    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->AssignPlayer(join.PlayerID, join.Team, join.Slot, false);
    }
}

void HandleAppRPUpdate(uint32_t, std::span<const uint8_t> msg)
{
    const auto update = ReadLinkupMessage<MCMPTwoLongMessage>(msg);
    MCSessionScreen* sessionScreen = GlobalLogPtr->SessionScreen.get();

    if (sessionScreen == nullptr)
    {
        return;
    }

    if (update.Value2 == 1)
    {
        sessionScreen->SetTeam1RP(update.Value1);
    }

    if (update.Value2 == 2)
    {
        sessionScreen->SetTeam2RP(update.Value1);
    }

    sessionScreen->Draw();
}

void HandleAppTechbaseChange(uint32_t, std::span<const uint8_t> msg)
{
    const auto change = ReadLinkupMessage<MCMPTwoLongMessage>(msg);

    if (GlobalLogPtr != nullptr && GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->SetTeamTechBase(static_cast<int8_t>(change.Value1),
                                                     static_cast<int8_t>(change.Value2));
    }
}

void HandleAppSwitchScreen(uint32_t, std::span<const uint8_t> msg)
{
    if (ReadLinkupMessage<MCMPTwoLongMessage>(msg).Value1 == 1)
    {
        GlobalLogPtr->SetUpSessionScreen();
    }
}

void HandleAppStartScenario(uint32_t, std::span<const uint8_t> msg)
{
    const auto start = ReadLinkupMessage<MCMPStartScenarioMessage>(msg);
    MCMultiPlayer& game = Game();

    for (uint32_t i = 1; i <= Scenario()->NumParts(); i++)
    {
        const MCFidpPlayer* player = game.SessionManager->GetPlayerNumber(Scenario()->Parts[i].CommanderId);

        if (player != nullptr)
        {
            auto* mover = static_cast<MCMover*>(Scenario()->Parts[i].Object);
            mover->NetOwnerID = player->Id;
            // The original copied at most 255 characters into the name's buffer.
            mover->NetName = player->Name.substr(0, 0xff);
        }
    }

    for (int32_t i = 0; i < game.NumMovers; i++)
    {
        game.MoverRoster[i]->GetPilot()->EscapesThruEjection = (start.MoverFlags[i] & 1) != 0 ? 1 : 0;
    }

    game.InMission = true;
    game.PrepareScenarioReceived = false;

    if (AblGetDebugger() != nullptr)
    {
        AblGetDebugger()->Print(MessageText(msg, sizeof(MCMPStartScenarioMessage)));
    }
}

void HandleAppEndScenario(uint32_t, std::span<const uint8_t> msg)
{
    Game().ScenarioResult = ReadLinkupMessage<MCMPLongMessage>(msg).Value;
}

void HandleAppPlayerOrder(uint32_t, std::span<const uint8_t> msg)
{
    const auto message = ReadLinkupMessage<MCMPPlayerOrderMessage>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    MCCommander* commander = CommanderById(message.CheckInId);

    if (!Game().IsServer)
    {
        return;
    }

    MCTacticalOrder order;
    order.Reset();
    order.Data[0] = message.PackedOrder[0];
    order.Data[1] = message.PackedOrder[1];
    order.Unpack();
    MCVector3D wayPoint;
    wayPoint.X = std::bit_cast<float>(message.OrderParam1);
    wayPoint.Y = std::bit_cast<float>(message.OrderParam2);
    wayPoint.Z = MCTerrain::GetTerrainElevation(wayPoint);
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

    std::array<MCMover*, MCMultiPlayer::MaxLocalMovers> movers{};
    MCMover* point = nullptr;
    const int32_t numMovers = order.GetGroup(message.CheckInId, movers.data(), &point);
    std::array<MCVector3D, 72> jumpGoals{};
    // Bits 1-4: the order goes to group (bit - 1) of the commander too.
    auto groupFlagged = [&](int32_t groupId) { return (message.Flags & (2 << groupId)) != 0; };

    if (order.Code == MCTacticalOrderCode::JumpToPoint)
    {
        int32_t numGoals = numMovers;

        for (int32_t groupId = 0; groupId < 4; groupId++)
        {
            if (groupFlagged(groupId))
            {
                numGoals += commander->GetGroup(groupId)->NumMovers();
            }
        }

        CalcJumpGoals(order.GetWayPoint(0), numGoals, jumpGoals.data(), order.GetJumpTarget());
    }

    const int fromGroup = (message.Flags & 0x20) != 0 ? 1 : 0;

    if (numMovers > 0)
    {
        // Original behaviour (OB-105): the client sends its sort flag in bit 0, but the server reads bit 4, group 3's
        // bit.
        const bool sortMovers = (message.Flags & 0x10) != 0;

        if (sortMovers)
        {
            SortMoverList(std::span(movers.data(), static_cast<size_t>(numMovers)), order.GetWayPoint(0));
        }

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMover* mover = movers[i];

            if (mover == nullptr || mover == order.Target)
            {
                continue;
            }

            if (sortMovers)
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
        if (!groupFlagged(groupId))
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

void HandleAppPlayerMoverGroup(uint32_t, std::span<const uint8_t> msg)
{
    const auto message = ReadLinkupMessage<MCMPPlayerMoverGroupMessage>(msg);

    if (!InMultiplayerMission())
    {
        return;
    }

    const std::array<MCMover*, MCMultiPlayer::MaxLocalMovers>& playerMovers =
        Game().PlayerMoverRoster[message.CheckInId];
    uint32_t memberBits = message.Members >> 4;
    std::array<MCMover*, MCMultiPlayer::MaxLocalMovers> movers{};
    int32_t numMovers = 0;

    for (MCMover* mover : playerMovers)
    {
        if ((memberBits & 1) != 0)
        {
            movers[numMovers++] = mover;
        }

        memberBits >>= 1;
    }

    int32_t pointIndex = message.Members & 0xf;
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
    CommanderById(message.CheckInId)->SetGroup(message.GroupId, numMovers, movers.data(), pointIndex);
}

void HandleAppPlayerArtillery(uint32_t, std::span<const uint8_t> msg)
{
    const auto message = ReadLinkupMessage<MCMPPlayerArtilleryMessage>(msg);

    if (!InMultiplayerMission() || !Game().IsServer)
    {
        return;
    }

    MCArtilleryChunk chunk;
    chunk.CommanderId = -1;
    chunk.StrikeType = -1;
    chunk.CellRow = -1;
    chunk.CellCol = -1;
    chunk.Seconds = -1;
    chunk.Data = message.ArtilleryData;
    chunk.Unpack();
    MCVector3D location;
    location.X = message.TargetX;
    location.Y = message.TargetY;
    location.Z = MCTerrain::GetTerrainElevation(location);
    CallArtillery(chunk.CommanderId, chunk.StrikeType, location, chunk.Seconds, 0);
}

void HandleAppMoverUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();
    const auto sequence = Get<uint16_t>(msg, 2);

    if (!InMultiplayerMission() || sequence < game.MoverUpdateSequence)
    {
        return;
    }

    if (!game.IsServer)
    {
        const size_t statusOffset = 4 + static_cast<size_t>(game.NumMovers) * 4;
        const size_t orderOffset = statusOffset + static_cast<size_t>(game.NumMovers) * 4;

        for (int32_t i = 0; i < game.NumMovers; i++)
        {
            MCMover* mover = game.MoverRoster[i];
            Assert(mover != nullptr, 0, " handleAppMoveUpdate: No Mover ");
            mover->HandleMoveChunk(Get<uint32_t>(msg, 4 + static_cast<size_t>(i) * 4));
        }

        for (int32_t i = 0; i < game.NumMovers; i++)
        {
            game.MoverRoster[i]->HandleStatusChunk(sequence - game.MoverUpdateSequence,
                                                   Get<uint32_t>(msg, statusOffset + static_cast<size_t>(i) * 4));
        }

        for (int32_t i = 0; i < game.NumMovers; i++)
        {
            game.MoverRoster[i]->GetPilot()->UpdateClientOrderQueue(ByteAt(msg, orderOffset + static_cast<size_t>(i)));
        }
    }

    game.MoverUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

void HandleAppTurretUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();
    const auto sequence = Get<uint16_t>(msg, 2);

    if (!InMultiplayerMission() || sequence < game.TurretUpdateSequence)
    {
        return;
    }

    if (!game.IsServer)
    {
        for (int32_t i = 0; i < game.NumTurrets; i++)
        {
            MCTurret* turret = game.TurretRoster[i];
            Assert(turret != nullptr, 0, " handleAppTurretUpdate: No Turret ");
            const auto targetIndex = static_cast<int8_t>(ByteAt(msg, 4 + static_cast<size_t>(i)));

            // Original behaviour (OB-106): -1 is no target, 0-127 a mover of the roster. The original's third case (a
            // part id from 0x80248 down) can't be reached with a signed byte, so building targets are misread.
            turret->Target = targetIndex < 0 ? nullptr : game.MoverRoster[targetIndex];
        }
    }

    game.TurretUpdateSequence = static_cast<uint16_t>(sequence + 1);
}

void HandleAppMoverWeaponFireUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();

    if (!InMultiplayerMission() || game.IsServer)
    {
        return;
    }

    const auto firstMover = static_cast<int8_t>(ByteAt(msg, 8));
    const auto numMovers = static_cast<int8_t>(ByteAt(msg, 9));
    size_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        const uint8_t numChunks = ByteAt(msg, 10 + static_cast<size_t>(i));
        game.MoverRoster[firstMover + i]->AddWeaponFireChunks(1, Words(msg, 0x22 + chunkIndex * 4, numChunks));
        chunkIndex += numChunks;
    }
}

void HandleAppTurretWeaponFireUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();

    if (!InMultiplayerMission() || game.IsServer)
    {
        return;
    }

    const auto numTurrets = static_cast<int8_t>(ByteAt(msg, 8));
    size_t chunkIndex = 0;

    for (int32_t i = 0; i < numTurrets; i++)
    {
        // Each turret's byte: its roster index (bits 2-7) and its number of chunks (bits 0-1).
        const uint8_t entry = ByteAt(msg, 9 + static_cast<size_t>(i));
        const size_t numChunks = entry & 3;
        game.TurretRoster[entry >> 2]->AddWeaponFireChunks(
            1, Words(msg, 9 + static_cast<size_t>(numTurrets) + chunkIndex * 4, numChunks));
        chunkIndex += numChunks;
    }
}

void HandleAppMoverCriticalHitUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();

    if (!InMultiplayerMission() || game.IsServer)
    {
        return;
    }

    const auto firstMover = static_cast<int8_t>(ByteAt(msg, 8));
    const auto numMovers = static_cast<int8_t>(ByteAt(msg, 9));
    size_t chunkIndex = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = game.MoverRoster[firstMover + i];
        const uint32_t numCriticalHits = ByteAt(msg, 10 + static_cast<size_t>(i));
        Assert(numCriticalHits < 0x81, numCriticalHits, " handleAppMoverCritHits: bad numCH ");

        if (numCriticalHits != 0)
        {
            mover->AddCriticalHitChunks(1, BytesAt(msg, 0x3a + chunkIndex, numCriticalHits));
            chunkIndex += numCriticalHits;
        }

        const uint32_t numRadio = ByteAt(msg, 0x22 + static_cast<size_t>(i));
        Assert(numRadio < 8, numRadio, " handleAppMoverCritHits: bad numRDO ");

        if (numRadio != 0)
        {
            mover->AddRadioChunks(1, BytesAt(msg, 0x3a + chunkIndex, numRadio));
            chunkIndex += numRadio;
        }
    }
}

void HandleAppWeaponHitUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();

    if (!InMultiplayerMission() || game.IsServer)
    {
        return;
    }

    const uint8_t numChunks = ByteAt(msg, 8);

    for (size_t i = 0; i < numChunks; i++)
    {
        MCWeaponHitChunk chunk = EmptyWeaponHitChunk();
        chunk.Data = Get<uint32_t>(msg, 9 + i * 4);
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
                game.MoverRoster[chunk.TargetId]->HandleWeaponHit(&shotInfo, 0);
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

            continue;
        }

        MCMover* target = nullptr;

        if (chunk.TargetType == 0)
        {
            target = game.MoverRoster[chunk.TargetId];
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

namespace
{
    /// <summary>A mine chunk: the tile's mine layout (the Clan's or the Inner Sphere's), and an explosion when set off.</summary>
    void ApplyMine(const MCWorldStateChunk& chunk)
    {
        const int32_t tileR = chunk.TileRow / 3;
        const int32_t tileC = chunk.TileCol / 3;
        const int32_t layout = std::min(chunk.Param2, 3);
        MCMapTile& tile = GameMap()->Map[tileR * GameMap()->Width + tileC];

        if (chunk.Param1 == 1)
        {
            tile.Overlay = (tile.Overlay & 0xffff9fff) | (layout << 13);
        }
        else
        {
            tile.Overlay = (tile.Overlay & 0xffffe7ff) | (layout << 11);
        }

        if (chunk.Param2 <= 3)
        {
            return;
        }

        MCVector3D position = MapCellToWorldPos(chunk.TileRow, chunk.TileCol);
        position.Z = MCTerrain::GetTerrainElevation(position);

        if (chunk.Param2 == 4)
        {
            CreateExplosion(MineExplosion, position, 0.0f, 0.0f);
        }
        else if (chunk.Param2 == 5)
        {
            CreateExplosion(MineExplosion, position, MineSplashDamage, MineSplashRange * WorldUnitsPerMeter);
        }
    }

    /// <summary>A terrain fire chunk: the object it names is set on fire.</summary>
    void ApplyTerrainFire(const MCWorldStateChunk& chunk)
    {
        auto* object = static_cast<MCGameObject*>(ObjectList()->FindObjectFromPart(chunk.ObjectWid));

        if (object == nullptr || object->GetObjectType() == nullptr)
        {
            return;
        }

        const auto seconds = static_cast<float>(chunk.Param1);

        switch (object->ObjectClass)
        {
            case MCObjectClass::Building:
            {
                static_cast<MCBuilding*>(object)->LightOnFire(seconds);
                break;
            }

            case MCObjectClass::Tree:
            {
                static_cast<MCTree*>(object)->LightOnFire(seconds);
                break;
            }

            case MCObjectClass::MiscTerrainObject:
            {
                static_cast<MCTerrainObject*>(object)->LightOnFire(seconds);
                break;
            }

            case MCObjectClass::TreeBuilding:
            {
                static_cast<MCTreeBuilding*>(object)->LightOnFire(seconds);
                break;
            }

            default:
            {
                Fatal(0, " handleAppWorldStateUpdate: bad fire victim ");
            }
        }
    }
}

void HandleAppWorldStateUpdate(uint32_t, std::span<const uint8_t> msg)
{
    MCMultiPlayer& game = Game();

    if (!InMultiplayerMission() || game.IsServer)
    {
        return;
    }

    const uint8_t numChunks = ByteAt(msg, 8);

    for (size_t i = 0; i < numChunks; i++)
    {
        MCWorldStateChunk chunk;
        chunk.Data = Get<uint32_t>(msg, 10 + i * 4);
        chunk.Unpack();

        switch (chunk.Type)
        {
            case MCWorldStateChunk::Mine:
            {
                ApplyMine(chunk);
                break;
            }

            case MCWorldStateChunk::TerrainFire:
            {
                ApplyTerrainFire(chunk);
                break;
            }

            case MCWorldStateChunk::MissionScriptMessage:
            {
                Scenario()->HandleMultiplayMessage(chunk.Param1, chunk.Param2);
                break;
            }

            case MCWorldStateChunk::PilotKillStat:
            {
                if (MCMover* mover = game.MoverRoster[chunk.Param1]; mover != nullptr)
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
                if (chunk.Type < MCWorldStateChunk::Artillery || chunk.Type > MCWorldStateChunk::LastArtillery)
                {
                    Fatal(0, " Multiplayer.handleAppWorldStateUpdate: bad worldStateType ");
                    break;
                }

                MCVector3D location = MapCellToWorldPos(chunk.TileRow, chunk.TileCol);
                location.Z = MCTerrain::GetTerrainElevation(location);
                CallArtillery(chunk.Type - MCWorldStateChunk::Artillery, chunk.Param1, location, chunk.Param2, 0);
                break;
            }
        }
    }
}

void HandleAppPlayerUpdate(uint32_t, std::span<const uint8_t>)
{
    Assert(Game().IsServer, 0, " Sending player update to non-server ");
}

uint32_t GetCheckSum(std::string_view fileName)
{
    MCFile file;

    if (file.Open(fileName) != 0)
    {
        return 0;
    }

    std::vector<uint8_t> contents(file.GetLength());
    file.Read(contents.data(), static_cast<uint32_t>(contents.size()));
    file.Close();
    // Only the file's first four bytes: the "checksum" tells little more than the file's existence.
    uint32_t checkSum = 0;
    std::memcpy(&checkSum, contents.data(), std::min<size_t>(contents.size(), 4));
    return checkSum;
}

void HandleAppFileInquiry(uint32_t fromID, std::span<const uint8_t> msg)
{
    // The inquiry's name starts at +0x8 (SendFileInquiry writes it there), not at a file-name message's +0xc.
    const std::string fileName(MessageText(msg, sizeof(MCFIGuaranteedMessageHeader)));
    MCMultiPlayer& game = Game();
    constexpr size_t nameOffset = sizeof(MCMPFileNameMessage);
    auto* report = static_cast<MCMPFileNameMessage*>(game.StartGuaranteedMessage(MCMPMessageType::FileReport));
    report->Value = static_cast<int32_t>(GetCheckSum(fileName));
    const size_t length = std::min(fileName.size(), MCMultiPlayer::MsgBufferSize - nameOffset - 1);
    std::memcpy(game.MsgBuffer.data() + nameOffset, fileName.data(), length);
    game.MsgBuffer[nameOffset + length] = 0;
    game.SessionManager->SendMessageToPlayerGuaranteed(fromID, report, static_cast<uint32_t>(length + nameOffset + 1),
                                                       true);
}

void HandleAppFileReport(uint32_t fromID, std::span<const uint8_t> msg)
{
    const bool haveFile = ReadLinkupMessage<MCMPLongMessage>(msg).Value != 0;

    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->FileReport(fromID, haveFile);
    }
}

void HandleAppLoadMission(uint32_t, std::span<const uint8_t> msg)
{
    if (GlobalLogPtr->SessionScreen != nullptr)
    {
        GlobalLogPtr->SessionScreen->LoadMission(MessageText(msg, sizeof(MCMPFileNameMessage)));
    }
}

void HandleAppStart(uint32_t, std::span<const uint8_t> msg)
{
    GuiSystem()->RemoveTimer(GlobalLogPtr->SessionScreen.get(), 0);
    Game().SessionManager->SendLatencyInfo();
    SoundSystem()->PlayBettySample(0x19);
    GlobalLogPtr->InitializeMultiplayer();
    GlobalLogPtr->LoadCampaign(MessageText(msg, sizeof(MCMPFileNameMessage)), ".MPK", false, false);
    GlobalLogPtr->SetUpBriefingScreen(false);
}

void LostConnectionDialogExit()
{
    if (!LaunchedFromLobby)
    {
        MCGameContext::Current().SetMultiPlayer(nullptr);
    }
    else
    {
        KillTheGame();
    }
}

void HandleLocalPlayerRemoved()
{
    MCMultiPlayer& game = Game();
    LastConnectionType = game.SessionManager->CurrentConnection;

    if (!game.PrepareScenarioReceived && !game.InMission)
    {
        game.InLogistics = false;
        GlobalLogPtr->DestroyMultiplayer();

        if (GlobalLogPtr->CurrentScreen != GlobalLogPtr->MainScreen.get())
        {
            WhackTimer = true;
        }

        GlobalLogPtr->CurrentScreen->ShowGuiWindow(false);
        GlobalLogPtr->CurrentScreen = GlobalLogPtr->MainScreen.get();
        GlobalLogPtr->LogisticsState = 1;
        GlobalLogPtr->ShowLogScreen(true, true);
        game.LeaveSession();
    }
    else
    {
        game.InMission = false;
        game.LeaveSession();
        Mission()->EndScenario();
    }

    ShowOkDialog(*GlobalLogPtr->MessageDialog, LoadGameString(0x369, 0xfe), LostConnectionDialogExit);
}

void MultiPlayerSystemCallback(MCFidpMessage& msg)
{
    const uint8_t* buffer = msg.MessageBuffer();

    switch (reinterpret_cast<const DPMSG_GENERIC*>(buffer)->dwType)
    {
        case DPSYS_CREATEPLAYERORGROUP:
        {
            const auto* create = reinterpret_cast<const DPMSG_CREATEPLAYERORGROUP*>(buffer);

            if (create->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                HandleSysCreatePlayer(*create);
            }

            break;
        }

        case DPSYS_DESTROYPLAYERORGROUP:
        {
            const auto* destroy = reinterpret_cast<const DPMSG_DESTROYPLAYERORGROUP*>(buffer);

            if (destroy->dwPlayerType == DPPLAYERTYPE_PLAYER)
            {
                Game().PlayerLeftGame(destroy->dpId);
            }

            break;
        }

        case DPSYS_ADDPLAYERTOGROUP:
        {
            HandleSysAddPlayerToGroup(*reinterpret_cast<const DPMSG_ADDPLAYERTOGROUP*>(buffer));
            break;
        }

        case DPSYS_SESSIONLOST:
        {
            if (Scenario() == nullptr || Scenario()->StartingUp == 0)
            {
                HandleLocalPlayerRemoved();
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

        default:
        {
            break;
        }
    }
}

void MultiPlayerApplicationCallback(MCFidpMessage& msg)
{
    const uint32_t fromID = msg.FromID;
    const std::span<const uint8_t> bytes = msg.Bytes();
    MCMultiPlayer& game = Game();

    switch (static_cast<MCMPMessageType>(msg.Type()))
    {
        case MCMPMessageType::NewServer:
        {
            HandleAppNewServer(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerRemoved:
        {
            HandleLocalPlayerRemoved();
            break;
        }

        case MCMPMessageType::Latency:
        {
            if (game.SessionManager->ReadyToChooseServer())
            {
                game.SwitchServers();
            }

            break;
        }

        case MCMPMessageType::Chat:
        {
            game.ChatCallback(msg);
            break;
        }

        case MCMPMessageType::PlayerCheckIn:
        {
            HandleAppPlayerCheckIn(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerSetup:
        {
            HandleAppPlayerSetup(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerCheckInReceipt:
        {
            HandleAppPlayerCheckInReceipt(fromID, bytes);
            break;
        }

        case MCMPMessageType::StartPlanning:
        {
            HandleAppStartPlanning(fromID, bytes);
            break;
        }

        case MCMPMessageType::StartScenario:
        {
            HandleAppStartScenario(fromID, bytes);
            break;
        }

        case MCMPMessageType::EndScenario:
        {
            HandleAppEndScenario(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerOrder:
        {
            HandleAppPlayerOrder(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerMoverGroup:
        {
            HandleAppPlayerMoverGroup(fromID, bytes);
            break;
        }

        case MCMPMessageType::PlayerArtillery:
        {
            HandleAppPlayerArtillery(fromID, bytes);
            break;
        }

        case MCMPMessageType::MoverUpdate:
        {
            HandleAppMoverUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::TurretUpdate:
        {
            HandleAppTurretUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::MoverWeaponFireUpdate:
        {
            HandleAppMoverWeaponFireUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::TurretWeaponFireUpdate:
        {
            HandleAppTurretWeaponFireUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::MoverCriticalHitUpdate:
        {
            HandleAppMoverCriticalHitUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::WeaponHitUpdate:
        {
            HandleAppWeaponHitUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::WorldStateUpdate:
        {
            HandleAppWorldStateUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::DeployForce:
        {
            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandleDeployForceMessage(fromID, msg.MessageBuffer());
            }

            break;
        }

        case MCMPMessageType::RemoveForce:
        {
            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandleRemoveForceMessage(fromID, msg.MessageBuffer());
            }

            break;
        }

        case MCMPMessageType::PlayerUpdate:
        {
            HandleAppPlayerUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::PrepareScenario:
        {
            game.PlayerCheckedIn.fill(0);

            if (GlobalLogPtr != nullptr)
            {
                GlobalLogPtr->HandlePrepareScenarioMessage();
            }

            game.PrepareScenarioReceived = true;
            break;
        }

        case MCMPMessageType::ReadyForBattle:
        {
            HandleAppReadyForBattle(fromID, bytes);
            break;
        }

        case MCMPMessageType::FileInquiry:
        {
            HandleAppFileInquiry(fromID, bytes);
            break;
        }

        case MCMPMessageType::FileReport:
        {
            HandleAppFileReport(fromID, bytes);
            break;
        }

        case MCMPMessageType::LoadMission:
        {
            HandleAppLoadMission(fromID, bytes);
            break;
        }

        case MCMPMessageType::Start:
        {
            HandleAppStart(fromID, bytes);
            break;
        }

        case MCMPMessageType::JoinTeam:
        {
            HandleAppJoinTeam(fromID, bytes);
            break;
        }

        case MCMPMessageType::SwitchScreen:
        {
            HandleAppSwitchScreen(fromID, bytes);
            break;
        }

        case MCMPMessageType::RPUpdate:
        {
            HandleAppRPUpdate(fromID, bytes);
            break;
        }

        case MCMPMessageType::TechbaseChange:
        {
            HandleAppTechbaseChange(fromID, bytes);
            break;
        }

        case MCMPMessageType::SessionCheckIn:
        {
            const int32_t playerNumber = game.SessionManager->GetPlayer(fromID)->PlayerNumber;
            Assert(playerNumber >= 0 && playerNumber <= 5, 0, "PNUM BAD");
            game.PlayerSessionCheckIn[playerNumber] = 1;

            if (GlobalLogPtr != nullptr && GlobalLogPtr->SessionScreen != nullptr)
            {
                GlobalLogPtr->SessionScreen->SomeoneCheckedIn();
            }

            break;
        }

        default:
        {
            break;
        }
    }
}
