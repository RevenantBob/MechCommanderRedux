#pragma once

// Original source: mcx\linkup\dpplayer.cpp.

#include "linkup/ficommonnetwork.h"
#include "linkup/linkedlist.h"

class FIDPMessage;

/// <summary>
/// A player of the linkup session as the SessionManager tracks it: its DirectPlay id and names, the guaranteed
/// messages sent to it and not yet verified, the guaranteed messages received from it out of order, its latency
/// history and the groups it belongs to.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\dpplayer.cpp</c>, 0x600 bytes. Guaranteed messages are
/// numbered per sender with a byte counter (wrapping at 256): <see cref="outgoingSendCount"/> numbers the messages
/// sent to this player, <see cref="incomingMessages"/> is indexed by the number of those received from it.
/// </remarks>
class FIDPPlayer
{
public:
    /// <summary>An empty player (no id, empty names).</summary>
    /// <remarks>MCX.EXE @ 0x0074a760</remarks>
    FIDPPlayer();
    /// <summary>
    /// The player <paramref name="id"/> named by <paramref name="name"/> (the short name up to 127 characters, the
    /// long one up to 255), with DirectPlay's player <paramref name="flags"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074a940</remarks>
    FIDPPlayer(uint32_t& id, const DPNAME* name, uint32_t flags);
    /// <summary>Deletes the incoming messages still held, frees the group ids and empties both lists.</summary>
    /// <remarks>MCX.EXE @ 0x0074ab40 (vector deleting destructor 0x0074a910)</remarks>
    virtual ~FIDPPlayer();

    FIDPPlayer(const FIDPPlayer&) = delete;
    FIDPPlayer& operator=(const FIDPPlayer&) = delete;

    /// <summary>
    /// Queues a guaranteed message sent to this player until it is verified, stamping its send time (and its first
    /// send time, if it was never resent).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074af50</remarks>
    void AddToVerifyList(FIDPMessage* msg);

    /// <summary>
    /// Removes the message numbered <paramref name="sendCount"/> from the verify list (it was acknowledged) and, if
    /// it was sent only once, records its round trip in the latency history.
    /// </summary>
    /// <returns>The message, or null when none has that number.</returns>
    /// <remarks>MCX.EXE @ 0x0074b110. Unnamed in the symbols (the name is the port's).</remarks>
    FIDPMessage* RemoveFromVerifyList(uint8_t sendCount);

    /// <summary>The mean of the non-zero entries of the latency history, 500 ms when there are none.</summary>
    /// <remarks>MCX.EXE @ 0x0074b060</remarks>
    int32_t AverageLatency();

    /// <summary>Whether 128 or more messages to this player are waiting to be verified.</summary>
    /// <remarks>MCX.EXE @ 0x0074b3f0</remarks>
    int IsVerifyListFull();

    /// <summary>
    /// How far the send counter has run ahead of the oldest unverified message (modulo 256), 0 when none waits.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074b420</remarks>
    int VerifyCountDifference();

    /// <summary>
    /// Stores a guaranteed message received from this player under its number <paramref name="sendCount"/>, unless
    /// it is outside the 128-message window or a duplicate.
    /// </summary>
    /// <returns>1 if stored, 0 if dropped.</returns>
    /// <remarks>MCX.EXE @ 0x0074b520</remarks>
    int HandleIncomingMessage(FIDPMessage* msg, int sendCount);

    /// <summary>Advances the next expected incoming number past every message already held.</summary>
    /// <remarks>MCX.EXE @ 0x0074b600</remarks>
    void SetNextIncomingSendCount();

    /// <summary>Takes the next in-order incoming message, if it has arrived.</summary>
    /// <returns>The message, or null.</returns>
    /// <remarks>MCX.EXE @ 0x0074b690</remarks>
    FIDPMessage* NextMessageToProcess();

    /// <summary>Records that the player joined group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074b720</remarks>
    void JoinGroup(uint32_t groupID);

    /// <summary>Records that the player left group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074b7f0</remarks>
    void LeaveGroup(uint32_t groupID);

    /// <summary>Deletes every player of <paramref name="list"/> and empties it.</summary>
    /// <remarks>MCX.EXE @ 0x0074ba00</remarks>
    static void ClearList(FLinkedList<FIDPPlayer>& list);

    /// <summary>Whether the player belongs to group <paramref name="groupID"/>.</summary>
    /// <remarks>MCX.EXE @ 0x0074bbf0</remarks>
    int IsInGroup(uint32_t groupID);

    /// <summary>The short name.</summary>
    char name[128]{}; // +0x4
    /// <summary>The long name.</summary>
    char longName[256]{}; // +0x84
    /// <summary>The player's DPID.</summary>
    uint32_t id = 0; // +0x184
    /// <summary>DirectPlay's player flags.</summary>
    uint32_t flags = 0; // +0x188
    /// <summary>Guaranteed messages sent to this player, waiting to be verified.</summary>
    FLinkedList<FIDPMessage> verifyList; // +0x18c
    /// <summary>Guaranteed messages received from this player, by their number, until processed in order.</summary>
    FIDPMessage* incomingMessages[256]{}; // +0x19c
    /// <summary>The next slot of <see cref="latencies"/> to write.</summary>
    int32_t latencyIndex = 0; // +0x59c
    /// <summary>The last five measured round trips, in ms.</summary>
    int32_t latencies[5]{}; // +0x5a0
    /// <summary>The last <see cref="AverageLatency"/>.</summary>
    int32_t averageLatency = 0; // +0x5b4
    /// <summary>Guards the verify list and the latency history (a CRITICAL_SECTION, 0x18 bytes, in the original).</summary>
    std::recursive_mutex criticalSection; // +0x5b8
    /// <summary>The number given to the last guaranteed message sent to this player (starts at 0xff).</summary>
    uint8_t outgoingSendCount = 0; // +0x5d0
    /// <summary>The number of the next incoming message to process.</summary>
    uint8_t nextIncomingToProcess = 0; // +0x5d1
    /// <summary>The lowest incoming number not yet received.</summary>
    uint8_t nextIncomingSendCount = 0; // +0x5d2
    /// <summary>How many incoming messages are held in <see cref="incomingMessages"/>.</summary>
    int32_t numIncomingMessages = 0; // +0x5d4
    /// <summary>
    /// The player's number in the session (0-5, -1 until the server gives one): its slot in every message's
    /// <see cref="MessageTagger"/>.
    /// </summary>
    int32_t playerNumber = 0; // +0x5d8
    /// <summary>The last measured round trip, in ms.</summary>
    uint32_t lastLatency = 0; // +0x5dc
    /// <summary>The latency the player reported in its last latency message (1000 when it reported 0).</summary>
    uint32_t reportedLatency = 0; // +0x5e0
    /// <summary>Nonzero once the player has a number (guaranteed messages are only resent to such players).</summary>
    int32_t hasPlayerNumber = 0; // +0x5e4
    /// <summary>Base delay before an unverified message is resent, in ms (1500).</summary>
    uint32_t resendDelay = 0; // +0x5e8
    /// <summary>The player's physical memory (from its FISystemInfoMessage).</summary>
    uint32_t totalPhysicalMemory = 0; // +0x5ec
    /// <summary>The ids of the groups the player is in (each a linkUpBlocks block).</summary>
    FLinkedList<uint32_t> groups; // +0x5f0
};
