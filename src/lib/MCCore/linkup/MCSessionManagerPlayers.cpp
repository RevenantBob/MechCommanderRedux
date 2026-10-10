#include "stdafx.h"
#include "linkup/MCSessionManager.h"
#include "linkup/MCFidpGroup.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCFidpPlayer.h"
#include "linkup/MCFidpSession.h"
#include "linkup/MCFileTransferInfo.h"
#include "lib/MCFatal.h"
#include "platform/MCDirectPlay.h"

// Players and groups coming and going (DirectPlay's system messages), the numbering the server hands out, and the
// linkup layer's own application messages.

namespace
{
    /// <summary>
    /// Splits a begin-transfer message's "name\directory" as the original's strtok did: leading backslashes are
    /// skipped, and an empty rest is no directory.
    /// </summary>
    std::pair<std::string, std::optional<std::string>> SplitTransferName(std::string_view text)
    {
        const size_t start = text.find_first_not_of('\\');

        if (start == std::string_view::npos)
        {
            return {};
        }

        const size_t end = text.find('\\', start);

        if (end == std::string_view::npos)
        {
            return {std::string(text.substr(start)), std::nullopt};
        }

        const std::string_view rest = text.substr(end + 1);
        return {std::string(text.substr(start, end - start)),
                rest.empty() ? std::nullopt : std::optional<std::string>(rest)};
    }
}

bool MCSessionManager::ReadyToChooseServer() const
{
    return MyPlayer != nullptr && _LatencyReportsIn;
}

void MCSessionManager::ProcessSystemInfoMessage(const MCFISystemInfoMessage& msg, uint32_t fromID)
{
    if (MCFidpPlayer* player = GetPlayer(fromID); player != nullptr)
    {
        player->TotalPhysicalMemory = msg.TotalPhysicalMemory;
    }
}

void MCSessionManager::ProcessLatencyMessage(const MCFIValueMessage& msg, uint32_t fromID)
{
    MCFidpPlayer* player = GetPlayer(fromID);
    Assert(player != nullptr, 0, "ProcessLatencyMessage - null player");

    if (player != nullptr)
    {
        player->ReportedLatency = msg.Value;
    }

    _LatencyReportsIn = std::ranges::none_of(Players, [](const auto& listed) { return listed->ReportedLatency == 0; });
}

void MCSessionManager::GivePlayerAnID(MCFidpPlayer& player)
{
    // numbers[0] is a sentinel; numbers[1..count] are the listed players' numbers, sorted.
    std::array<int32_t, MaxLinkupPlayers + 1> numbers;
    numbers.fill(30000);
    numbers[0] = 0;
    // Port fix: at most six numbers fit (the original would write past the array with more players listed).
    const size_t count = std::min<size_t>(Players.size(), MaxLinkupPlayers);

    for (size_t i = 0; i < count; i++)
    {
        numbers[i + 1] = Players[i]->PlayerNumber;
    }

    std::sort(numbers.begin() + 1, numbers.begin() + 1 + static_cast<ptrdiff_t>(count));
    int32_t number = numbers[count] + 1;

    for (size_t i = 1; i < count; i++)
    {
        if (numbers[i] + 1 < numbers[i + 1])
        {
            number = numbers[i] + 1;
            break;
        }
    }

    player.PlayerNumber = number;
    player.HasPlayerNumber = true;
}

void MCSessionManager::AddPlayerOrGroup(uint32_t playerType, uint32_t id, const DPNAME& name, uint32_t flags)
{
    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        if (GetGroup(id) == nullptr)
        {
            Groups.push_back(std::make_unique<MCFidpGroup>(id, name, flags));
        }

        return;
    }

    if (GetPlayer(id) != nullptr)
    {
        return;
    }

    // Original behaviour: the numbers message below lists the players from the first one listed before this one,
    // so a player joining an empty list is sent no numbers.
    const bool listWasEmpty = Players.empty();
    auto added = std::make_unique<MCFidpPlayer>(id, name, flags);
    MCFidpPlayer* player = added.get();

    if (HasPlayerNumber && (IsHost || !LaunchedFromLobby))
    {
        GivePlayerAnID(*player);

        if (player->PlayerNumber < 0 || player->PlayerNumber >= MaxLinkupPlayers)
        {
            Fatal(player->PlayerNumber, "Could not connect to game.");
        }
    }

    Players.push_back(std::move(added));

    if (!IsHost && LaunchedFromLobby)
    {
        player->HasPlayerNumber = false;
    }

    if (LaunchedFromLobby)
    {
        for (int32_t i = 0; i < MaxLinkupPlayers; i++)
        {
            if (_NewPlayerNumbers[i] == id)
            {
                player->PlayerNumber = i;
                player->HasPlayerNumber = true;
            }
        }
    }

    if (CurrentSession != nullptr)
    {
        CurrentSession->SessionDesc.dwCurrentPlayers = static_cast<uint32_t>(Players.size());
    }

    if (!IsHost)
    {
        return;
    }

    // The server tells the new player (or, in a lobby game, everyone) who has which number.
    MCFIPlayerNumbersMessage numbers;
    numbers.Header = LinkupHeader(MCLinkupMessageType::PlayerNumbers, FIMSG_GUARANTEED);

    if (!listWasEmpty)
    {
        for (const auto& listed : Players)
        {
            // Port fix: an unnumbered player is skipped (the original wrote it at index -1, into the send counters).
            if (listed->PlayerNumber >= 0 && listed->PlayerNumber < MaxLinkupPlayers)
            {
                numbers.PlayerIDs[listed->PlayerNumber] = listed->Id;
            }

            if (listed->Id == MyPlayerID)
            {
                numbers.ServerNumber = static_cast<uint8_t>(listed->PlayerNumber);
            }
        }
    }

    if (!LaunchedFromLobby)
    {
        SendMessageToPlayerGuaranteed(player->Id, &numbers, sizeof(numbers), true);
    }
    else
    {
        SendMessageToGroup(0, &numbers, sizeof(numbers));
    }

    SendPlayersInGroupMessages(player->Id);
}

void MCSessionManager::PlayerOrGroupLeaving(uint32_t playerType, uint32_t id)
{
    if (playerType != DPPLAYERTYPE_PLAYER)
    {
        return;
    }

    MCFidpPlayer* player = GetPlayer(id);

    if (player == nullptr)
    {
        return;
    }

    player->HasPlayerNumber = false;

    if (player->Id != ServerID)
    {
        return;
    }

    // The server left: the next player by the server's latency order takes over.
    MCFidpPlayer* next = nullptr;

    // Port fix: bounded to the six numbers (the original ran on past the array when nobody was left).
    for (int32_t i = 0; next == nullptr && i < MaxLinkupPlayers; i++)
    {
        next = GetPlayerNumber(PlayersByLatency[i]);

        if (next != nullptr && !next->HasPlayerNumber)
        {
            next = nullptr;
        }
    }

    Assert(next != nullptr, 0, "No more players");

    if (next == nullptr)
    {
        return;
    }

    if (next == MyPlayer)
    {
        IsHost = true;
    }

    ServerID = next->Id;
}

void MCSessionManager::DeletePlayerOrGroup(uint32_t playerType, uint32_t id)
{
    if (playerType == DPPLAYERTYPE_PLAYER)
    {
        if (std::erase_if(Players, [&](const auto& player) { return player->Id == id; }) != 0 &&
            CurrentSession != nullptr)
        {
            CurrentSession->SessionDesc.dwCurrentPlayers = static_cast<uint32_t>(Players.size());
        }

        return;
    }

    // The group goes too (the original took it off the list and kept it).
    if (const auto found = std::ranges::find_if(Groups, [&](const auto& group) { return group->Id == id; });
        found != Groups.end())
    {
        Groups.erase(found);
    }
}

void MCSessionManager::PlayerJoinedGroup(uint32_t groupID, uint32_t playerID)
{
    MCFidpGroup* group = GetGroup(groupID);

    if (group != nullptr && group->AddPlayer(playerID))
    {
        if (MCFidpPlayer* player = GetPlayer(playerID); player != nullptr)
        {
            player->JoinGroup(groupID);
        }
    }
}

void MCSessionManager::HandlePreSystemMessage(MCFidpMessage& msg)
{
    const uint8_t* buffer = msg.MessageBuffer();
    const uint32_t type = reinterpret_cast<const DPMSG_GENERIC*>(buffer)->dwType;

    if (type == DPSYS_ADDPLAYERTOGROUP)
    {
        const auto* added = reinterpret_cast<const DPMSG_ADDPLAYERTOGROUP*>(buffer);
        Assert(GetGroup(added->dpIdGroup) != nullptr, 0, "group is null");
        PlayerJoinedGroup(added->dpIdGroup, added->dpIdPlayer);
    }
    else if (type == DPSYS_CREATEPLAYERORGROUP)
    {
        const auto* created = reinterpret_cast<const DPMSG_CREATEPLAYERORGROUP*>(buffer);
        AddPlayerOrGroup(created->dwPlayerType, created->dpId, created->dpnName, created->dwFlags);
    }
    else if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        const auto* destroyed = reinterpret_cast<const DPMSG_DESTROYPLAYERORGROUP*>(buffer);
        PlayerOrGroupLeaving(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_SETSESSIONDESC && LaunchedFromLobby)
    {
        const auto* changed = reinterpret_cast<const DPMSG_SETSESSIONDESC*>(buffer);

        if ((changed->dpDesc.dwFlags & DPSESSION_NEWPLAYERSDISABLED) != 0)
        {
            _SessionLocked = true;
        }
    }
}

void MCSessionManager::HandlePostSystemMessage(MCFidpMessage& msg)
{
    const uint8_t* buffer = msg.MessageBuffer();
    const uint32_t type = reinterpret_cast<const DPMSG_GENERIC*>(buffer)->dwType;

    if (type == DPSYS_DESTROYPLAYERORGROUP)
    {
        const auto* destroyed = reinterpret_cast<const DPMSG_DESTROYPLAYERORGROUP*>(buffer);
        DeletePlayerOrGroup(destroyed->dwPlayerType, destroyed->dpId);
    }
    else if (type == DPSYS_DELETEPLAYERFROMGROUP)
    {
        const auto* removed = reinterpret_cast<const DPMSG_ADDPLAYERTOGROUP*>(buffer);
        MCFidpGroup* group = GetGroup(removed->dpIdGroup);

        // Port fix: a group that no longer exists is skipped (the original called through a null group).
        if (group != nullptr && group->RemovePlayer(removed->dpIdPlayer))
        {
            if (MCFidpPlayer* player = GetPlayer(removed->dpIdPlayer); player != nullptr)
            {
                player->LeaveGroup(removed->dpIdGroup);
            }
        }
    }
}

void MCSessionManager::ApplyLobbyPlayerNumbers(const MCFIPlayerNumbersMessage& numbers)
{
    for (int32_t i = 0; i < MaxLinkupPlayers; i++)
    {
        if (numbers.PlayerIDs[i] == 0)
        {
            continue;
        }

        if (MCFidpPlayer* player = GetPlayer(numbers.PlayerIDs[i]); player == nullptr)
        {
            _NewPlayerNumbers[i] = numbers.PlayerIDs[i];
        }
        else
        {
            player->PlayerNumber = i;
            player->HasPlayerNumber = true;
        }
    }
}

void MCSessionManager::HandleFileData(const MCFidpMessage& msg)
{
    const std::span<const uint8_t> piece = msg.Bytes();
    const uint8_t fileID =
        piece.size() > sizeof(MCFIGuaranteedMessageHeader) ? piece[sizeof(MCFIGuaranteedMessageHeader)] : 0;
    const auto found = std::ranges::find_if(IncomingFiles, [&](const auto& transfer)
                                            { return static_cast<uint32_t>(transfer->FileID) == fileID; });

    if (found == IncomingFiles.end() || !(*found)->AddBytes(piece))
    {
        return;
    }

    std::unique_ptr<MCFileTransferInfo> done = std::move(*found);
    IncomingFiles.erase(found);

    if (FileReceivedCallback)
    {
        FileReceivedCallback(done->FileName);
    }
}

void MCSessionManager::HandleApplicationMessage(MCFidpMessage* msg)
{
    bool passOn = true;
    const std::span<const uint8_t> bytes = msg->Bytes();

    switch (static_cast<MCLinkupMessageType>(msg->Type()))
    {
        case MCLinkupMessageType::PlayerNumbers:
        {
            if (LaunchedFromLobby)
            {
                ApplyLobbyPlayerNumbers(ReadLinkupMessage<MCFIPlayerNumbersMessage>(bytes));
            }

            break;
        }

        case MCLinkupMessageType::PlayersInGroup:
        {
            const auto members = ReadLinkupMessage<MCFIPlayersInGroupMessage>(bytes);

            if (GetGroup(members.GroupID) != nullptr)
            {
                for (const uint32_t playerID : members.PlayerIDs)
                {
                    if (playerID == 0)
                    {
                        break;
                    }

                    PlayerJoinedGroup(members.GroupID, playerID);
                }
            }

            passOn = false;
            break;
        }

        case MCLinkupMessageType::GameStarted:
        {
            GameStarted = true;
            break;
        }

        case MCLinkupMessageType::NewServer:
        {
            ServerID = ReadLinkupMessage<MCFIValueMessage>(bytes).Value;
            IsHost = ServerID == MyPlayerID;
            break;
        }

        case MCLinkupMessageType::BeginFileTransfer:
        {
            const auto begin = ReadLinkupMessage<MCFIBeginFileTransferMessage>(bytes);
            constexpr size_t nameOffset = sizeof(MCFIBeginFileTransferMessage);
            const std::string_view text =
                bytes.size() > nameOffset
                    ? std::string_view(
                          reinterpret_cast<const char*>(bytes.data()) + nameOffset,
                          strnlen(reinterpret_cast<const char*>(bytes.data()) + nameOffset, bytes.size() - nameOffset))
                    : std::string_view();
            const auto [fileName, directory] = SplitTransferName(text);
            auto transfer = std::make_unique<MCFileTransferInfo>(
                _HomeDirectory, msg->FromID, MyPlayerID, fileName,
                directory.has_value() ? std::optional<std::string_view>(*directory) : std::nullopt, begin.FileSize,
                MCFileTransferDirection::Receive);
            transfer->FileID = begin.FileID;
            IncomingFiles.push_back(std::move(transfer));
            passOn = false;
            break;
        }

        case MCLinkupMessageType::FileData:
        {
            HandleFileData(*msg);
            passOn = false;
            break;
        }

        case MCLinkupMessageType::Ping:
        {
            HandlePingUpdate(*msg);
            passOn = false;
            break;
        }

        case MCLinkupMessageType::SystemInfo:
        {
            ProcessSystemInfoMessage(ReadLinkupMessage<MCFISystemInfoMessage>(bytes), msg->FromID);
            passOn = false;
            break;
        }

        case MCLinkupMessageType::Latency:
        {
            ProcessLatencyMessage(ReadLinkupMessage<MCFIValueMessage>(bytes), msg->FromID);
            break;
        }

        default:
        {
            break;
        }
    }

    if (passOn && ApplicationCallback)
    {
        // Port: the original left the critical section around the callback (for its receive thread, which never
        // existed); the port has one thread, and its callers don't always hold the lock, so it is left alone.
        ApplicationCallback(*msg);
    }

    AddMessageToEmptyQueue(msg);
}

void MCSessionManager::HandlePingUpdate(const MCFidpMessage& msg)
{
    if (msg.FromID != ServerID)
    {
        return;
    }

    const auto ping = ReadLinkupMessage<MCFIPingMessage>(msg.Bytes());

    // Port fix: bounded to the six numbers the message holds.
    for (int32_t i = 0; i < ping.Count && i < MaxLinkupPlayers; i++)
    {
        PlayersByLatency[i] = ping.PlayerNumbers[i];
    }
}
