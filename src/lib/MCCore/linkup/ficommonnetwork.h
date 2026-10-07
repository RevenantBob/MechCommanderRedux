#pragma once

// Original source: mcx\linkup\ficommonnetwork.h: the message headers every linkup (DirectPlay) message starts with,
// shared by the linkup layer and the game's MultiPlayer. The DirectPlay structures the linkup classes use (DPNAME,
// DPSESSIONDESC2, the DPMSG_* system messages) come from the port's DirectPlay stand-in, platform/MCDirectPlay.h.
// DirectPlay ids (DPID, a DWORD) are uint32_t throughout.

#include "platform/MCDirectPlay.h"

#pragma pack(push, 1)

/// <summary>
/// The per-player send counters of a guaranteed message: slot <c>n</c> holds the sequence number the message has for
/// player number <c>n</c>, so each receiver can put guaranteed messages back in order.
/// </summary>
/// <remarks>Original source: <c>linkup\ficommonnetwork.h</c>, 6 bytes on the wire.</remarks>
class MCMessageTagger
{
public:
    /// <summary>Zeroes the six counters.</summary>
    void Clear()
    {
        for (int i = 0; i < 6; i++)
        {
            SendCount[i] = 0;
        }
    }

    uint8_t SendCount[6]{};
};

static_assert(sizeof(MCMessageTagger) == 6);

/// <summary>Bits 0-9 of <see cref="MCFIMessageHeader::Header"/>: the message type.</summary>
inline constexpr uint16_t FIMSG_TYPE_MASK = 0x03ff;
/// <summary>Set on a message sent to a whole group (the send counters of every member are filled in).</summary>
inline constexpr uint16_t FIMSG_GROUP_MESSAGE = 0x0800;
/// <summary>Set on a guaranteed message: it carries a <see cref="MCMessageTagger"/>, is verified and resent.</summary>
inline constexpr uint16_t FIMSG_GUARANTEED = 0x1000;

/// <summary>
/// The 16-bit word every linkup message starts with: the message type (bits 0-9) and the delivery flags. The linkup
/// layer handles types 1-12 itself (2 player numbers, 3 players in a group, 5 game started, 6 new server,
/// 7 begin file transfer, 8 file data, 9 player removed, 10 ping, 11 system info, 12 latency; 6 and 12 are passed on
/// to the game too); the game's MultiPlayer uses 13 and up.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\ficommonnetwork.h</c>, 2 bytes on the wire. The original declared the type and flags
/// as bitfields; the port keeps the word and masks (<see cref="FIMSG_TYPE_MASK"/>, <see cref="FIMSG_GROUP_MESSAGE"/>,
/// <see cref="FIMSG_GUARANTEED"/>), which give the same bits on every compiler.
/// </remarks>
class MCFIMessageHeader
{
public:
    uint16_t Header = 0;
};

static_assert(sizeof(MCFIMessageHeader) == 2);

/// <summary>The header of a guaranteed message: the type word and the per-player send counters.</summary>
/// <remarks>Original source: <c>linkup\ficommonnetwork.h</c>, 8 bytes on the wire.</remarks>
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
/// <remarks>Original source: <c>linkup\ficommonnetwork.h</c>, 0xc bytes on the wire (header word 0x100b).</remarks>
class MCFISystemInfoMessage : public MCFIGuaranteedMessageHeader
{
public:
    /// <summary>GlobalMemoryStatus's dwTotalPhys.</summary>
    uint32_t TotalPhysicalMemory = 0;
};

static_assert(sizeof(MCFISystemInfoMessage) == 0xc);

/// <summary>
/// Message type 7: announces a file transfer: the file's size, its transfer id and its name as "name\directory".
/// </summary>
/// <remarks>
/// Original source: <c>linkup\ficommonnetwork.h</c>. Variable length on the wire: 7 bytes plus the zero-terminated
/// name (FileTransferInfo::CreateBeginTransferMessage allocates strlen(name) + strlen(dir) + 0xb).
/// </remarks>
class MCFIBeginFileTransferMessage : public MCFIMessageHeader
{
public:
    uint32_t FileSize = 0;
    /// <summary>The transfer's id (SessionManager's next file id, 0-255).</summary>
    uint8_t FileID = 0;
    /// <summary>"name\directory", zero-terminated; the message runs on past this declaration.</summary>
    char FileName[1]{};
};

static_assert(sizeof(MCFIBeginFileTransferMessage) == 8);

/// <summary>
/// Message type 8 (guaranteed): one piece of a file transfer: the transfer id and up to 100 bytes of the file. A
/// piece whose last byte is 0 ends the transfer.
/// </summary>
/// <remarks>
/// Built in place by FileTransferInfo::PrepareNextMessage (header word 0x1008, 9 + bytes read). The name is the
/// port's; the original built it through raw pointers.
/// </remarks>
struct MCFIFileDataMessage : public MCFIGuaranteedMessageHeader
{
    uint8_t FileID = 0;
    uint8_t Data[100]{};
};

static_assert(sizeof(MCFIFileDataMessage) == 0x6d);

#pragma pack(pop)
