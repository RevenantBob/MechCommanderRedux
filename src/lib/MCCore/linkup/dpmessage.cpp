#include "stdafx.h"
#include "linkup/dpmessage.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"

MCFidpMessage::MCFidpMessage(uint32_t toID, uint32_t bufferSize)
{
    MessageSize = 0;
    FromID = 0;
    FirstSendTime = 0;
    SendTime = 0;
    TimesSent = 0;
    Clear();
    MessageBuffer = static_cast<uint8_t*>(LinkUpBlocks->Allocate(bufferSize));
    Assert(MessageBuffer != nullptr, 0, "Message buffer is null: malloc failed");
    this->BufferSize = bufferSize;
    this->ToID = toID;
    WasResent = 0;
}

MCFidpMessage::~MCFidpMessage()
{
    if (MessageBuffer != nullptr)
    {
        LinkUpBlocks->Free(MessageBuffer);
    }
}

uint32_t MCFidpMessage::SetMessageBuffer(void* data, uint32_t size)
{
    MessageSize = size < BufferSize ? size : BufferSize;
    std::memcpy(MessageBuffer, data, MessageSize);
    return MessageSize;
}

void MCFidpMessage::Clear()
{
    FromID = 0;
    SendTime = 0;
    TimesSent = 0;
    WasResent = 0;
}

int32_t MCFidpMessage::ReceiveMessage(MCDirectPlay* directPlay)
{
    FromID = 0;
    uint32_t to;
    uint32_t size = BufferSize;
    const uint32_t result = directPlay->Receive(&FromID, &to, DPRECEIVE_ALL, MessageBuffer, &size);
    Assert(result != DPERR_BUFFERTOOSMALL, BufferSize, " DP Buffer too small ");

    if (result == DP_OK)
    {
        MessageSize = size;
    }

    return static_cast<int32_t>(result);
}
