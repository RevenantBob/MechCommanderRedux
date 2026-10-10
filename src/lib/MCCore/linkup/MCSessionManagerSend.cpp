#include "stdafx.h"
#include "linkup/MCSessionManager.h"
#include "linkup/MCFidpGroup.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCFidpSession.h"
#include "linkup/MCFileTransferInfo.h"
#include "lib/MCFatal.h"
#include "lib/MCMsvcSort.h"
#include "platform/MCDirectPlay.h"

// The session manager's sends: plain, guaranteed (numbered per player and kept until verified), to a group or to the
// server, the messages queued until this machine has a player number, file announcements, latency and the server
// switch.

namespace
{
    /// <summary>The "removed from the game" message type.</summary>
    constexpr uint16_t PlayerRemovedType = static_cast<uint16_t>(MCLinkupMessageType::PlayerRemoved);
}

void MCSessionManager::SendSystemInformation()
{
    MCFISystemInfoMessage msg;
    msg.Header = LinkupHeader(MCLinkupMessageType::SystemInfo, FIMSG_GUARANTEED);
    msg.TotalPhysicalMemory = MCPort::TotalPhysicalMemory();

    if (!IsHost)
    {
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
    }
    else
    {
        MyPlayer->TotalPhysicalMemory = msg.TotalPhysicalMemory;
    }
}

int MCSessionManager::RemovePlayerFromGame(MCFidpPlayer* player)
{
    if (_RemovingPlayer || !player->HasPlayerNumber)
    {
        return -1;
    }

    _RemovingPlayer = true;
    MCFIValueMessage msg;
    msg.Header = LinkupHeader(MCLinkupMessageType::PlayerRemoved, FIMSG_GUARANTEED);
    msg.Value = player->Id;
    SendMessageToPlayerGuaranteed(player->Id, &msg, sizeof(msg), true);
    player->HasPlayerNumber = false;
    _RemovingPlayer = false;
    return 0;
}

void MCSessionManager::SetupMessageSendCounts(MCFIGuaranteedMessageHeader* header,
                                              std::span<MCFidpPlayer* const> players)
{
    for (MCFidpPlayer* player : players)
    {
        // Port fix: a member the session no longer knows is listed as null; skipped.
        if (player != nullptr && player != MyPlayer && player->HasPlayerNumber && player->PlayerNumber != -1 &&
            player->PlayerNumber < MaxLinkupPlayers)
        {
            player->OutgoingSendCount++;
            header->Tagger.SendCount[player->PlayerNumber] = player->OutgoingSendCount;
        }
    }

    header->Header |= FIMSG_GROUP_MESSAGE;
    header->Header |= FIMSG_GUARANTEED;
}

void MCSessionManager::StartGame()
{
    MCFIGuaranteedMessageHeader msg;
    msg.Header = LinkupHeader(MCLinkupMessageType::GameStarted, FIMSG_GUARANTEED);
    SendMessageToGroup(0, &msg, sizeof(msg));
    GameStarted = true;
}

void MCSessionManager::SendPing()
{
    // Port fix: cleared (the original sent the uninitialised rest of a stack buffer, its header included).
    MCFIPingMessage ping;
    ping.Header = LinkupHeader(MCLinkupMessageType::Ping);
    constexpr uint32_t pingHeaderSize = sizeof(MCFIGuaranteedMessageHeader) + 1;

    if (!IsHost)
    {
        SendMessageToGroup(0, &ping, pingHeaderSize);
        return;
    }

    std::vector<int32_t> numbers;

    for (const auto& player : Players)
    {
        // Port fix: bounded to the six numbers the message holds.
        if (player.get() != MyPlayer && numbers.size() < MaxLinkupPlayers)
        {
            numbers.push_back(player->PlayerNumber);
        }
    }

    Assert(numbers.size() == Players.size() - 1, 0, "nPlayers is incorrect");
    MCMsvcSort(std::span(numbers),
               [this](const int32_t& number1, const int32_t& number2)
               {
                   MCFidpPlayer* player1 = GetPlayerNumber(number1);
                   MCFidpPlayer* player2 = GetPlayerNumber(number2);

                   if (player2->AverageLatency() < player1->AverageLatency())
                   {
                       return 1;
                   }

                   return player1->AverageLatency() < player2->AverageLatency() ? -1 : 0;
               });
    ping.Count = static_cast<uint8_t>(numbers.size());

    for (size_t i = 0; i < numbers.size(); i++)
    {
        ping.PlayerNumbers[i] = static_cast<uint8_t>(numbers[i]);
    }

    SendMessageToGroup(0, &ping, ping.Count + pingHeaderSize);
}

void MCSessionManager::SendPlayersInGroupMessages(uint32_t playerID)
{
    for (const auto& group : Groups)
    {
        MCFIPlayersInGroupMessage msg;
        msg.Header = LinkupHeader(MCLinkupMessageType::PlayersInGroup);
        msg.GroupID = group->Id;

        // The message holds six members (all a session can have).
        for (size_t j = 0; j < group->Players.size() && j < msg.PlayerIDs.size(); j++)
        {
            msg.PlayerIDs[j] = group->Players[j];
        }

        if (playerID == 0)
        {
            SendMessageToGroup(0, &msg, sizeof(msg));
        }
        else
        {
            SendMessageToPlayerGuaranteed(playerID, &msg, sizeof(msg), true);
        }
    }
}

void MCSessionManager::SendPreIDGuaranteedMessages()
{
    const size_t numGroupMessages = PreIDGroupMessages.size();

    for (size_t i = 0; i < numGroupMessages && !PreIDGroupMessages.empty(); i++)
    {
        MCFidpMessage* msg = PreIDGroupMessages.front();
        SendMessageFromInfo(*msg);
        AddMessageToEmptyQueue(msg);
        PreIDGroupMessages.pop_front();
    }

    const size_t numServerMessages = PreIDServerMessages.size();

    for (size_t i = 0; i < numServerMessages && !PreIDServerMessages.empty(); i++)
    {
        MCFidpMessage* msg = PreIDServerMessages.front();
        auto* header = reinterpret_cast<MCFIGuaranteedMessageHeader*>(msg->MessageBuffer());

        // Original behaviour: these messages are not returned to the free queue.
        if ((header->Header & FIMSG_GUARANTEED) == 0)
        {
            SendMessageToServer(header, msg->MessageSize);
        }
        else
        {
            SendMessageToServerGuaranteed(header, msg->MessageSize);
        }

        PreIDServerMessages.pop_front();
    }
}

void MCSessionManager::SendMessageToGroup(uint32_t groupID, MCFIGuaranteedMessageHeader* header, uint32_t size)
{
    if (!HasPlayerNumber)
    {
        if (!LaunchedFromLobby)
        {
            std::lock_guard lock(CriticalSection);
            header->Header |= FIMSG_GROUP_MESSAGE;
            PreIDGroupMessages.push_back(CopyToFreeMessage(header, size, groupID));
        }

        return;
    }

    std::vector<MCFidpPlayer*> list;

    if (groupID == 0)
    {
        list.reserve(Players.size());

        for (const auto& player : Players)
        {
            list.push_back(player.get());
        }
    }
    else
    {
        list = GetPlayerListForGroup(groupID);
    }

    if (!_SessionLocked || !LaunchedFromLobby)
    {
        for (MCFidpPlayer* player : list)
        {
            // Port fix: a member the session no longer knows is listed as null; skipped.
            if (player != MyPlayer && player != nullptr)
            {
                SendMessageToPlayerGuaranteed(player->Id, header, size, true);
            }
        }

        return;
    }

    for (MCFidpPlayer* player : list)
    {
        if (player != MyPlayer && player != nullptr && player->IsVerifyListFull())
        {
            RemovePlayerFromGame(player);
        }
    }

    std::lock_guard lock(CriticalSection);
    SetupMessageSendCounts(header, list);

    if (SendPlainMessage(groupID, header, size) == 0)
    {
        for (MCFidpPlayer* player : list)
        {
            if (player != nullptr && player->HasPlayerNumber && player != MyPlayer)
            {
                header->Header &= ~FIMSG_GROUP_MESSAGE;
                player->AddToVerifyList(CopyToFreeMessage(header, size, player->Id));
            }
        }
    }
}

std::vector<MCFidpPlayer*> MCSessionManager::GetPlayerListForGroup(uint32_t groupID)
{
    MCFidpGroup* group = GetGroup(groupID);
    Assert(group != nullptr, 0, "Group does not exist");
    std::vector<MCFidpPlayer*> list;

    if (group != nullptr)
    {
        for (const uint32_t playerID : group->Players)
        {
            list.push_back(GetPlayer(playerID));
        }
    }

    return list;
}

uint32_t MCSessionManager::TallyLatencies()
{
    uint32_t total = 0;

    for (const auto& player : Players)
    {
        if (player.get() != MyPlayer)
        {
            total += player->AverageLatency();
        }
    }

    if (Players.size() == 1)
    {
        return 0;
    }

    return total / static_cast<uint32_t>(Players.size() - 1);
}

void MCSessionManager::SendLatencyInfo()
{
    if (!IsHost)
    {
        MCFIValueMessage msg;
        msg.Value = TallyLatencies();
        msg.Header = LinkupHeader(MCLinkupMessageType::Latency, FIMSG_GUARANTEED);
        SendMessageToServerGuaranteed(&msg, sizeof(msg));
        return;
    }

    MyPlayer->ReportedLatency = TallyLatencies();

    if (MyPlayer->ReportedLatency == 0)
    {
        MyPlayer->ReportedLatency = 1000;
    }
}

void MCSessionManager::SwitchServers()
{
    if (!IsHost || LaunchedFromLobby || Players.empty())
    {
        return;
    }

    uint32_t mostMemory = 100000;
    MCFidpPlayer* best = Players.front().get();

    for (const auto& player : Players)
    {
        if (mostMemory < player->TotalPhysicalMemory)
        {
            best = player.get();
            mostMemory = player->TotalPhysicalMemory;
        }
    }

    if (best != MyPlayer)
    {
        ServerID = best->Id;
        IsHost = false;
        _ServerMessage.Value = ServerID;
        SendMessageToGroup(0, &_ServerMessage, sizeof(_ServerMessage));
    }
}

void MCSessionManager::SendMessageToPlayerGuaranteed(uint32_t playerID, MCFIGuaranteedMessageHeader* header,
                                                     uint32_t size, bool firstSend)
{
    if (!HasPlayerNumber)
    {
        if (!LaunchedFromLobby)
        {
            std::lock_guard lock(CriticalSection);
            header->Header &= ~FIMSG_GROUP_MESSAGE;
            PreIDGroupMessages.push_back(CopyToFreeMessage(header, size, playerID));
        }

        return;
    }

    MCFidpPlayer* player = GetPlayer(playerID);

    if (player == MyPlayer || player == nullptr)
    {
        return;
    }

    if (header->Type() != PlayerRemovedType &&
        (!player->HasPlayerNumber || (player->IsVerifyListFull() && RemovePlayerFromGame(player) == 0)))
    {
        return;
    }

    std::lock_guard lock(CriticalSection);

    if (firstSend && player->PlayerNumber >= 0 && player->PlayerNumber < MaxLinkupPlayers)
    {
        player->OutgoingSendCount++;
        header->Tagger.SendCount[player->PlayerNumber] = player->OutgoingSendCount;
    }

    header->Header |= FIMSG_GUARANTEED;
    header->Header &= ~FIMSG_GROUP_MESSAGE;
    const uint32_t sendTime = MCPort::Milliseconds();
    const int32_t result = SendPlainMessage(playerID, header, size);
    MCFidpMessage* msg = CopyToFreeMessage(header, size, playerID);
    player->AddToVerifyList(msg);

    if (result != 0)
    {
        // Original behaviour: a failed send is backdated so it is resent at once; the time is timeGetTime's (ms) where
        // the verify list keeps performance-counter ticks.
        msg->SendTime = sendTime - player->ResendDelay * TicksPerMs();

        if (!msg->WasResent)
        {
            msg->FirstSendTime = msg->SendTime;
        }
    }
}

void MCSessionManager::SendMessageToServerGuaranteed(MCFIGuaranteedMessageHeader* header, uint32_t size)
{
    if (!HasPlayerNumber)
    {
        std::lock_guard lock(CriticalSection);
        header->Header &= ~FIMSG_GROUP_MESSAGE;
        PreIDServerMessages.push_back(CopyToFreeMessage(header, size, ServerID));
        return;
    }

    SendMessageToPlayerGuaranteed(ServerID, header, size, true);
}

void MCSessionManager::BroadcastMessage(MCFIMessageHeader* header, uint32_t size)
{
    SendPlainMessage(0, header, size);
}

void MCSessionManager::SendMessageToServer(MCFIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(CriticalSection);

    if (!HasPlayerNumber)
    {
        header->Header &= ~FIMSG_GROUP_MESSAGE;
        PreIDServerMessages.push_back(CopyToFreeMessage(header, size, ServerID));
    }
    else if (ServerID != MyPlayerID)
    {
        SendPlainMessage(ServerID, header, size);
    }
}

int32_t MCSessionManager::SendPlainMessage(uint32_t toID, const MCFIMessageHeader* header, uint32_t size)
{
    std::lock_guard lock(CriticalSection);
    return static_cast<int32_t>(DirectPlay->Send(MyPlayerID, toID, 0, header, std::min<uint32_t>(size, 0x200)));
}

int MCSessionManager::BroadcastFile(std::string_view fileName, std::optional<std::string_view> directory,
                                    std::function<void(const std::string& fileName)> callback)
{
    auto transfer = std::make_unique<MCFileTransferInfo>(_HomeDirectory, MyPlayerID, 0, fileName, directory, 0,
                                                         MCFileTransferDirection::Send);
    transfer->Callback = std::move(callback);
    transfer->FileID = _NextFileID;
    _NextFileID++;

    if (_NextFileID > 0xff)
    {
        _NextFileID = 0;
    }

    std::vector<uint8_t> begin = transfer->CreateBeginTransferMessage();
    OutgoingFiles.push_back(std::move(transfer));
    BroadcastMessage(reinterpret_cast<MCFIMessageHeader*>(begin.data()), static_cast<uint32_t>(begin.size()));
    // Original behaviour: when the id wraps the answer is -1.
    return _NextFileID - 1;
}

void MCSessionManager::SendMessageFromInfo(MCFidpMessage& msg)
{
    auto* header = reinterpret_cast<MCFIGuaranteedMessageHeader*>(msg.MessageBuffer());

    if (msg.ToID == 0)
    {
        SendMessageToGroup(0, header, msg.MessageSize);
    }
    else if ((header->Header & FIMSG_GROUP_MESSAGE) != 0)
    {
        SendMessageToGroup(msg.ToID, header, msg.MessageSize);
    }
    else if ((header->Header & FIMSG_GUARANTEED) == 0)
    {
        SendPlainMessage(msg.ToID, header, msg.MessageSize);
    }
    else
    {
        SendMessageToPlayerGuaranteed(msg.ToID, header, msg.MessageSize, true);
    }
}

std::optional<std::string> MCSessionManager::GetStats()
{
    if (CurrentSession == nullptr)
    {
        return std::nullopt;
    }

    if (!IsHost)
    {
        const MCFidpPlayer* server = GetPlayer(ServerID);

        if (server == nullptr)
        {
            return std::nullopt;
        }

        return std::format("Latency to server ({}) = {}", server->Name, server->LastLatency);
    }

    std::string stats = "Latencies -- ";

    for (const auto& player : Players)
    {
        if (player.get() != MyPlayer)
        {
            stats += std::format("<{}: {:4}> ", player->Name, player->LastLatency);
        }
    }

    return stats;
}
