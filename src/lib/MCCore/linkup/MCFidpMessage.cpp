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

uint32_t MCFidpMessage::SetMessageBuffer(const MCFIMessageHeader* header, uint32_t size)
{
    return SetMessageBuffer(std::span(reinterpret_cast<const uint8_t*>(header), size));
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
    const uint32_t result = directPlay.Receive(&FromID, &to, DPRECEIVE_ALL, _Buffer.data(), &size);
    Assert(result != DPERR_BUFFERTOOSMALL, BufferSize(), " DP Buffer too small ");

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
