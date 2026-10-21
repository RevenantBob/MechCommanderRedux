#pragma once

// Original source: mcx\linkup\ficommonnetwork.h (the headers every linkup message starts with, shared with the game's
// MultiPlayer) and the message layouts SessionManager builds in place. The DirectPlay structures (DPNAME,
// DPSESSIONDESC2, the DPMSG_* system messages) come from the port's DirectPlay stand-in, platform/MCDirectPlay.h.
// DirectPlay ids (DPID, a DWORD) are uint32_t throughout.

#include "platform/MCDirectPlay.h"

/// <summary>
/// The players a linkup session numbers: the send counters of a guaranteed message have six slots, so a seventh player
/// could not be told its messages' order. Wire format.
/// </summary>
inline constexpr int32_t MaxLinkupPlayers = 6;

#pragma pack(push, 1)

/// <summary>
/// The per-player send counters of a guaranteed message: slot <c>n</c> holds the sequence number the message has for
/// player number <c>n</c>, so each receiver can put guaranteed messages back in order.
/// </summary>
/// <remarks>Fixed layout: 6 bytes on the wire.</remarks>
class MCMessageTagger
{
public:
    /// <summary>Zeroes the six counters.</summary>
    void Clear() { SendCount.fill(0); }

    std::array<uint8_t, MaxLinkupPlayers> SendCount{};
};

static_assert(sizeof(MCMessageTagger) == 6);

/// <summary>Bits 0-9 of <see cref="MCFIMessageHeader::Header"/>: the message type.</summary>
inline constexpr uint16_t FIMSG_TYPE_MASK = 0x03ff;
/// <summary>Set on a message sent to a whole group (the send counters of every member are filled in).</summary>
inline constexpr uint16_t FIMSG_GROUP_MESSAGE = 0x0800;
/// <summary>Set on a guaranteed message: it carries a <see cref="MCMessageTagger"/>, is verified and resent.</summary>
inline constexpr uint16_t FIMSG_GUARANTEED = 0x1000;

/// <summary>The linkup layer's own message types (bits 0-9 of the header word); the game's MultiPlayer uses 13 and up.</summary>
enum class MCLinkupMessageType : uint16_t
{
    /// <summary>The numbers of the guaranteed messages received from a player (<see cref="MCFIVerifyMessage"/>).</summary>
    Verify = 1,
    /// <summary>The server's numbering of the players (<see cref="MCFIPlayerNumbersMessage"/>).</summary>
    PlayerNumbers = 2,
    /// <summary>The players of a group (<see cref="MCFIPlayersInGroupMessage"/>).</summary>
    PlayersInGroup = 3,
    /// <summary>The host started the game.</summary>
    GameStarted = 5,
    /// <summary>A new server (<see cref="MCFIValueMessage"/>; passed on to the game).</summary>
    NewServer = 6,
    /// <summary>A file transfer begins. The game never sent files; the port drops it.</summary>
    BeginFileTransfer = 7,
    /// <summary>A piece of a file. The game never sent files; the port drops it.</summary>
    FileData = 8,
    /// <summary>The receiver was removed from the game (<see cref="MCFIValueMessage"/>; passed on to the game).</summary>
    PlayerRemoved = 9,
    /// <summary>The server's ping (<see cref="MCFIPingMessage"/>).</summary>
    Ping = 10,
    /// <summary>A player's physical memory (<see cref="MCFISystemInfoMessage"/>).</summary>
    SystemInfo = 11,
    /// <summary>A player's average latency (<see cref="MCFIValueMessage"/>; passed on to the game).</summary>
    Latency = 12
};

/// <summary>The header word of a message of <paramref name="type"/> with <paramref name="flags"/>.</summary>
constexpr uint16_t LinkupHeader(MCLinkupMessageType type, uint16_t flags = 0)
{
    return static_cast<uint16_t>(flags | static_cast<uint16_t>(type));
}

/// <summary>
/// The 16-bit word every linkup message starts with: the message type (bits 0-9) and the delivery flags.
/// </summary>
/// <remarks>
/// Fixed layout: 2 bytes on the wire. The original declared the type and flags as bitfields; the port keeps the word
/// and masks (<see cref="FIMSG_TYPE_MASK"/>, <see cref="FIMSG_GROUP_MESSAGE"/>, <see cref="FIMSG_GUARANTEED"/>), which
/// give the same bits on every compiler.
/// </remarks>
class MCFIMessageHeader
{
public:
    /// <summary>The message type (bits 0-9).</summary>
    uint16_t Type() const { return Header & FIMSG_TYPE_MASK; }

    uint16_t Header = 0;
};

static_assert(sizeof(MCFIMessageHeader) == 2);

/// <summary>The header of a guaranteed message: the type word and the per-player send counters.</summary>
/// <remarks>Fixed layout: 8 bytes on the wire.</remarks>
class MCFIGuaranteedMessageHeader : public MCFIMessageHeader
{
public:
    MCMessageTagger Tagger{};
};

static_assert(sizeof(MCFIGuaranteedMessageHeader) == 8);

/// <summary>
/// Message type 11 (guaranteed): a player's physical memory, sent to the server when a player joins so it can pick
/// the best machine as the next server.
/// </summary>
/// <remarks>Fixed layout: 0xc bytes on the wire (header word 0x100b).</remarks>
class MCFISystemInfoMessage : public MCFIGuaranteedMessageHeader
{
public:
    /// <summary>GlobalMemoryStatus's dwTotalPhys.</summary>
    uint32_t TotalPhysicalMemory = 0;
};

static_assert(sizeof(MCFISystemInfoMessage) == 0xc);

/// <summary>Message type 2 (guaranteed): the server's numbering of the players.</summary>
/// <remarks>Fixed layout: 0x21 bytes.</remarks>
struct MCFIPlayerNumbersMessage : MCFIGuaranteedMessageHeader
{
    /// <summary>The DPID of player number n (0 = no such player).</summary>
    std::array<uint32_t, MaxLinkupPlayers> PlayerIDs{};
    /// <summary>The sending server's own number.</summary>
    uint8_t ServerNumber = 0;
};

static_assert(sizeof(MCFIPlayerNumbersMessage) == 0x21);

/// <summary>Message type 3: the players of a group, as the server knows them.</summary>
/// <remarks>
/// Fixed layout: sent as 0x24 bytes (the group and six members) whatever the group holds; the receiver reads the six.
/// </remarks>
struct MCFIPlayersInGroupMessage : MCFIGuaranteedMessageHeader
{
    uint32_t GroupID = 0;
    std::array<uint32_t, MaxLinkupPlayers> PlayerIDs{};
};

static_assert(sizeof(MCFIPlayersInGroupMessage) == 0x24);

/// <summary>Message types 6 (new server), 9 (player removed) and 12 (latency): the header and one 32-bit value.</summary>
/// <remarks>Fixed layout: 0xc bytes.</remarks>
struct MCFIValueMessage : MCFIGuaranteedMessageHeader
{
    uint32_t Value = 0;
};

static_assert(sizeof(MCFIValueMessage) == 0xc);

/// <summary>
/// Message type 10: the server's ping, with the other players' numbers sorted by latency (the order the next server is
/// picked in).
/// </summary>
/// <remarks>Fixed layout: 9 + count bytes.</remarks>
struct MCFIPingMessage : MCFIGuaranteedMessageHeader
{
    uint8_t Count = 0;
    std::array<uint8_t, MaxLinkupPlayers> PlayerNumbers{};
};

static_assert(sizeof(MCFIPingMessage) == 0xf);

/// <summary>
/// Message type 1: the numbers of the guaranteed messages received from one player since the last verify. Each entry
/// is a MessageTagger with only the receiver's slot set.
/// </summary>
/// <remarks>
/// Fixed layout: 3 + count * 6 bytes on the wire. The count is a byte, so 256 entries hold every count it can reach
/// (the original kept a 0x2400-byte buffer per player).
/// </remarks>
struct MCFIVerifyMessage : MCFIMessageHeader
{
    /// <summary>The bytes the message takes on the wire.</summary>
    uint32_t WireSize() const { return Count * 6u + 3u; }

    uint8_t Count = 0;
    std::array<MCMessageTagger, 256> Entries{};
};

#pragma pack(pop)

/// <summary>
/// A name DirectPlay gave, cut to <paramref name="length"/> characters or at its first null, whichever comes first
/// (none: empty).
/// </summary>
inline std::string LinkupName(const std::optional<std::string>& name, size_t length)
{
    return name.has_value() ? name->substr(0, std::min(name->find('\0'), length)) : std::string();
}

/// <summary>Reads a message layout from the start of <paramref name="bytes"/> (zero-filled past their end).</summary>
template <class T> T ReadLinkupMessage(std::span<const uint8_t> bytes)
{
    static_assert(std::is_trivially_copyable_v<T>);
    T message{};
    std::memcpy(&message, bytes.data(), std::min(bytes.size(), sizeof(T)));
    return message;
}

/// <summary>
/// The first <paramref name="size"/> bytes of <paramref name="message"/>, a message layout: what the session manager's
/// sends take.
/// </summary>
template <class T> std::span<uint8_t> LinkupBytes(T& message, size_t size = sizeof(T))
{
    static_assert(std::is_trivially_copyable_v<T>);
    return std::span(reinterpret_cast<uint8_t*>(&message), sizeof(T)).first(size);
}

/// <summary>The header word at the start of <paramref name="message"/>.</summary>
inline uint16_t LinkupHeaderOf(std::span<const uint8_t> message)
{
    return ReadLinkupMessage<MCFIMessageHeader>(message).Header;
}

/// <summary>Sets the header word at the start of <paramref name="message"/>.</summary>
inline void SetLinkupHeader(std::span<uint8_t> message, uint16_t header)
{
    std::memcpy(message.first(sizeof(header)).data(), &header, sizeof(header));
}

/// <summary>
/// Sets the send counter of player number <paramref name="playerNumber"/> in the <see cref="MCMessageTagger"/> of the
/// guaranteed message <paramref name="message"/>.
/// </summary>
inline void SetLinkupSendCount(std::span<uint8_t> message, int32_t playerNumber, uint8_t count)
{
    message[static_cast<size_t>(static_cast<int32_t>(sizeof(MCFIMessageHeader)) + playerNumber)] = count;
}

/// <summary>
/// The send counter of player number <paramref name="playerNumber"/> in the <see cref="MCMessageTagger"/> of the
/// guaranteed message <paramref name="message"/>.
/// </summary>
inline uint8_t LinkupSendCount(std::span<const uint8_t> message, int32_t playerNumber)
{
    return message[static_cast<size_t>(static_cast<int32_t>(sizeof(MCFIMessageHeader)) + playerNumber)];
}
