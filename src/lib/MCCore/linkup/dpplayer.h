#pragma once

// Original source: mcx\linkup\dpplayer.cpp.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

class MCFidpMessage;

/// <summary>
/// A player of the linkup session as the SessionManager tracks it: its DirectPlay id and names, the guaranteed
/// messages sent to it and not yet verified, the guaranteed messages received from it out of order, its latency
/// history and the groups it belongs to.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\dpplayer.cpp</c>, 0x600 bytes. Guaranteed messages are
/// numbered per sender with a byte counter (wrapping at 256): <see cref="OutgoingSendCount"/> numbers the messages
/// sent to this player, <see cref="IncomingMessages"/> is indexed by the number of those received from it.
/// </remarks>
class MCFidpPlayer
{
public:
    /// <summary>An empty player (no id, empty names).</summary>
    MCFidpPlayer();
    /// <summary>
    /// The player <paramref name="id"/> named by <paramref name="name"/> (the short name up to 127 characters, the
    /// long one up to 255), with DirectPlay's player <paramref name="flags"/>.
    /// </summary>
    MCFidpPlayer(uint32_t& id, const DPNAME* name, uint32_t flags);
    /// <summary>Deletes the incoming messages still held, frees the group ids and empties both lists.</summary>
    virtual ~MCFidpPlayer();

    MCFidpPlayer(const MCFidpPlayer&) = delete;
    MCFidpPlayer& operator=(const MCFidpPlayer&) = delete;

    /// <summary>
    /// Queues a guaranteed message sent to this player until it is verified, stamping its send time (and its first
    /// send time, if it was never resent).
    /// </summary>
    void AddToVerifyList(MCFidpMessage* msg);

    /// <summary>
    /// Removes the message numbered <paramref name="sendCount"/> from the verify list (it was acknowledged) and, if
    /// it was sent only once, records its round trip in the latency history.
    /// </summary>
    /// <returns>The message, or null when none has that number.</returns>
    MCFidpMessage* RemoveFromVerifyList(uint8_t sendCount);

    /// <summary>The mean of the non-zero entries of the latency history, 500 ms when there are none.</summary>
    int32_t AverageLatency();

    /// <summary>Whether 128 or more messages to this player are waiting to be verified.</summary>
    int IsVerifyListFull();

    /// <summary>
    /// How far the send counter has run ahead of the oldest unverified message (modulo 256), 0 when none waits.
    /// </summary>
    int VerifyCountDifference();

    /// <summary>
    /// Stores a guaranteed message received from this player under its number <paramref name="sendCount"/>, unless
    /// it is outside the 128-message window or a duplicate.
    /// </summary>
    /// <returns>1 if stored, 0 if dropped.</returns>
    int HandleIncomingMessage(MCFidpMessage* msg, int sendCount);

    /// <summary>Advances the next expected incoming number past every message already held.</summary>
    void SetNextIncomingSendCount();

    /// <summary>Takes the next in-order incoming message, if it has arrived.</summary>
    /// <returns>The message, or null.</returns>
    MCFidpMessage* NextMessageToProcess();

    /// <summary>Records that the player joined group <paramref name="groupID"/>.</summary>
    void JoinGroup(uint32_t groupID);

    /// <summary>Records that the player left group <paramref name="groupID"/>.</summary>
    void LeaveGroup(uint32_t groupID);

    /// <summary>Deletes every player of <paramref name="list"/> and empties it.</summary>
    static void ClearList(MCFLinkedList<MCFidpPlayer>& list);

    /// <summary>Whether the player belongs to group <paramref name="groupID"/>.</summary>
    int IsInGroup(uint32_t groupID);

    /// <summary>The short name.</summary>
    char Name[128]{};
    /// <summary>The long name.</summary>
    char LongName[256]{};
    /// <summary>The player's DPID.</summary>
    uint32_t Id = 0;
    /// <summary>DirectPlay's player flags.</summary>
    uint32_t Flags = 0;
    /// <summary>Guaranteed messages sent to this player, waiting to be verified.</summary>
    MCFLinkedList<MCFidpMessage> VerifyList;
    /// <summary>Guaranteed messages received from this player, by their number, until processed in order.</summary>
    MCFidpMessage* IncomingMessages[256]{};
    /// <summary>The next slot of <see cref="Latencies"/> to write.</summary>
    int32_t LatencyIndex = 0;
    /// <summary>The last five measured round trips, in ms.</summary>
    int32_t Latencies[5]{};
    /// <summary>The last <see cref="AverageLatency"/>.</summary>
    int32_t LastAverageLatency = 0;
    /// <summary>Guards the verify list and the latency history (a CRITICAL_SECTION, 0x18 bytes, in the original).</summary>
    std::recursive_mutex CriticalSection;
    /// <summary>The number given to the last guaranteed message sent to this player (starts at 0xff).</summary>
    uint8_t OutgoingSendCount = 0;
    /// <summary>The number of the next incoming message to process.</summary>
    uint8_t NextIncomingToProcess = 0;
    /// <summary>The lowest incoming number not yet received.</summary>
    uint8_t NextIncomingSendCount = 0;
    /// <summary>How many incoming messages are held in <see cref="IncomingMessages"/>.</summary>
    int32_t NumIncomingMessages = 0;
    /// <summary>
    /// The player's number in the session (0-5, -1 until the server gives one): its slot in every message's
    /// <see cref="MCMessageTagger"/>.
    /// </summary>
    int32_t PlayerNumber = 0;
    /// <summary>The last measured round trip, in ms.</summary>
    uint32_t LastLatency = 0;
    /// <summary>The latency the player reported in its last latency message (1000 when it reported 0).</summary>
    uint32_t ReportedLatency = 0;
    /// <summary>Nonzero once the player has a number (guaranteed messages are only resent to such players).</summary>
    int32_t HasPlayerNumber = 0;
    /// <summary>Base delay before an unverified message is resent, in ms (1500).</summary>
    uint32_t ResendDelay = 0;
    /// <summary>The player's physical memory (from its FISystemInfoMessage).</summary>
    uint32_t TotalPhysicalMemory = 0;
    /// <summary>The ids of the groups the player is in (each a linkUpBlocks block).</summary>
    MCFLinkedList<uint32_t> Groups;
};
