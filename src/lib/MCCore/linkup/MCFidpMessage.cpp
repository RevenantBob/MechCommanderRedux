#include "stdafx.h"
#include "linkup/MCFidpMessage.h"
#include "linkup/MCLinkupMessages.h"
#include "lib/MCFatal.h"

MCFidpMessage::MCFidpMessage(uint32_t toID, uint32_t bufferSize) : ToID(toID), _Buffer(bufferSize)
{
}

uint32_t MCFidpMessage::SetMessageBuffer(std::span<const uint8_t> data)
{
    MessageSize = static_cast<uint32_t>(std::min(data.size(), _Buffer.size()));
    std::memcpy(_Buffer.data(), data.data(), MessageSize);
    return MessageSize;
}

void MCFidpMessage::Clear()
{
    FromID = 0;
    SendTime = 0;
    TimesSent = 0;
    WasResent = false;
}

int32_t MCFidpMessage::ReceiveMessage(MCDirectPlay& directPlay)
{
    FromID = 0;
    uint32_t to = 0;
    uint32_t size = BufferSize();
    uint32_t result = directPlay.Receive(FromID, to, DPRECEIVE_ALL, std::span(_Buffer).first(size), size);

    // A message bigger than the buffer (the game's are at most 0x200 bytes, so only another machine sends one) is taken
    // off the queue and dropped. The original asserted and left it at the head, and nothing behind it was received
    // again.
    while (result == DPERR_BUFFERTOOSMALL)
    {
        std::vector<uint8_t> discarded(size);
        directPlay.Receive(FromID, to, DPRECEIVE_ALL, discarded, size);
        SDL_Log("MCFidpMessage: dropped a %u-byte message from player %u", size, FromID);
        FromID = 0;
        size = BufferSize();
        result = directPlay.Receive(FromID, to, DPRECEIVE_ALL, std::span(_Buffer).first(size), size);
    }

    if (result == DP_OK)
    {
        MessageSize = size;
    }

    return static_cast<int32_t>(result);
}

uint16_t MCFidpMessage::Type() const
{
    uint16_t header = 0;
    std::memcpy(&header, _Buffer.data(), std::min(_Buffer.size(), sizeof(header)));
    return header & FIMSG_TYPE_MASK;
}
