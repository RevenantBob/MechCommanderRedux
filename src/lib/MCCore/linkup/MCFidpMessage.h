#pragma once

// Original source: mcx\linkup\dpmessage.cpp.

class MCDirectPlay;
class MCFIMessageHeader;

/// <summary>
/// One linkup message buffer: the raw bytes of a received or outgoing message and its routing (sender, receiver) and
/// resend bookkeeping. The SessionManager owns a pool of them and passes them between its queues; guaranteed messages
/// sit in the receiver's FIDPPlayer verify list until acknowledged.
/// </summary>
/// <remarks>The buffer starts with an <see cref="MCFIMessageHeader"/>.</remarks>
class MCFidpMessage
{
public:
    /// <summary>A message addressed to <paramref name="toID"/> with a zeroed buffer of <paramref name="bufferSize"/> bytes.</summary>
    MCFidpMessage(uint32_t toID, uint32_t bufferSize);

    MCFidpMessage(const MCFidpMessage&) = delete;
    MCFidpMessage& operator=(const MCFidpMessage&) = delete;

    /// <summary>Copies <paramref name="data"/> (at most the buffer's size) into the buffer.</summary>
    /// <returns>The number of bytes copied.</returns>
    uint32_t SetMessageBuffer(std::span<const uint8_t> data);

    /// <summary>Copies <paramref name="size"/> bytes of the message at <paramref name="header"/> into the buffer.</summary>
    /// <returns>The number of bytes copied.</returns>
    uint32_t SetMessageBuffer(const MCFIMessageHeader* header, uint32_t size);

    /// <summary>Resets the sender, the timestamps and the resend state (not the buffer).</summary>
    void Clear();

    /// <summary>Receives the next pending DirectPlay message (any sender, any receiver) into the buffer.</summary>
    /// <returns>0 or the DirectPlay error (DPERR_BUFFERTOOSMALL asserts).</returns>
    int32_t ReceiveMessage(MCDirectPlay& directPlay);

    /// <summary>The whole buffer.</summary>
    uint8_t* MessageBuffer() { return _Buffer.data(); }

    /// <summary>The whole buffer.</summary>
    const uint8_t* MessageBuffer() const { return _Buffer.data(); }

    /// <summary>The bytes in use.</summary>
    std::span<const uint8_t> Bytes() const { return std::span(_Buffer).first(MessageSize); }

    /// <summary>The header word's message type (bits 0-9).</summary>
    uint16_t Type() const;

    /// <summary>The buffer's capacity.</summary>
    uint32_t BufferSize() const { return static_cast<uint32_t>(_Buffer.size()); }

    /// <summary>Bytes of the buffer in use.</summary>
    uint32_t MessageSize = 0;
    /// <summary>The receiver's DPID (0 = everyone, else a player or group).</summary>
    uint32_t ToID = 0;
    /// <summary>The sender's DPID.</summary>
    uint32_t FromID = 0;
    /// <summary>Low 32 bits of the performance counter when the message was (last) sent.</summary>
    uint32_t SendTime = 0;
    /// <summary>The <see cref="SendTime"/> of the first send (kept across resends).</summary>
    uint32_t FirstSendTime = 0;
    /// <summary>Whether the message was resent (latency is only measured on messages sent once).</summary>
    bool WasResent = false;
    /// <summary>
    /// How many times the message was sent: 1 when it enters the verify list, +1 per resend. A resend waits the
    /// player's resend delay times this.
    /// </summary>
    int32_t TimesSent = 0;

private:
    /// <summary>The message bytes.</summary>
    std::vector<uint8_t> _Buffer;
};
