#pragma once

// Original source: mcx\linkup\dpmessage.cpp.

class MCDirectPlay;

/// <summary>
/// One linkup message buffer: the raw bytes of a received or outgoing message and its routing (sender, receiver)
/// and resend bookkeeping. SessionManager keeps a pool of 900 of them per session and passes them between its
/// queues; guaranteed messages sit in the receiver's FIDPPlayer verify list until acknowledged.
/// </summary>
/// <remarks>
/// Original source: <c>linkup\dpmessage.cpp</c>, 0x28 bytes. The buffer
/// (<see cref="messageBuffer"/>) starts with an <c>FIMessageHeader</c>.
/// </remarks>
class FIDPMessage
{
public:
    /// <summary>
    /// A message addressed to <paramref name="toID"/> with a buffer of <paramref name="bufferSize"/> bytes (from
    /// linkUpBlocks).
    /// </summary>
    /// <remarks>MCX.EXE @ 0x0074a4f0</remarks>
    FIDPMessage(uint32_t toID, uint32_t bufferSize);
    /// <summary>Frees the buffer.</summary>
    /// <remarks>MCX.EXE @ 0x0074a5d0 (vector deleting destructor 0x0074a5a0)</remarks>
    virtual ~FIDPMessage();

    FIDPMessage(const FIDPMessage&) = delete;
    FIDPMessage& operator=(const FIDPMessage&) = delete;

    /// <summary>Copies <paramref name="size"/> bytes (at most the buffer's size) into the buffer.</summary>
    /// <returns>The number of bytes copied.</returns>
    /// <remarks>MCX.EXE @ 0x0074a600</remarks>
    uint32_t SetMessageBuffer(void* data, uint32_t size);

    /// <summary>Resets the sender, the timestamps and the resend state (not the buffer).</summary>
    /// <remarks>MCX.EXE @ 0x0074a660</remarks>
    void Clear();

    /// <summary>
    /// Receives the next pending DirectPlay message (any sender, any receiver) into the buffer.
    /// <paramref name="directPlay"/> is <c>IDirectPlay3*</c> in the original.
    /// </summary>
    /// <returns>0 or the DirectPlay error (DPERR_BUFFERTOOSMALL asserts).</returns>
    /// <remarks>MCX.EXE @ 0x0074a6a0</remarks>
    int32_t ReceiveMessage(MCDirectPlay* directPlay);

    /// <summary>Bytes of the buffer in use.</summary>
    uint32_t messageSize = 0; // +0x4
    /// <summary>The receiver's DPID (0 = everyone, else a player or group).</summary>
    uint32_t toID = 0; // +0x8
    /// <summary>The sender's DPID.</summary>
    uint32_t fromID = 0; // +0xc
    /// <summary>Low 32 bits of the performance counter when the message was (last) sent.</summary>
    uint32_t sendTime = 0; // +0x10
    /// <summary>The buffer's capacity.</summary>
    uint32_t bufferSize = 0; // +0x14
    /// <summary>The message bytes (a linkUpBlocks block).</summary>
    uint8_t* messageBuffer = nullptr; // +0x18
    /// <summary>The <see cref="sendTime"/> of the first send (kept across resends).</summary>
    uint32_t firstSendTime = 0; // +0x1c
    /// <summary>Nonzero once the message was resent (latency is only measured on messages sent once).</summary>
    int32_t wasResent = 0; // +0x20
    /// <summary>
    /// How many times the message was sent: 1 when it enters the verify list, +1 per resend. A resend waits
    /// the player's resendDelay times this.
    /// </summary>
    int32_t timesSent = 0; // +0x24
};
