#include "stdafx.h"
#include "linkup/dpmessage.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"
#include "lib/heap.h"

void* FIDPMessage::operator new(size_t size) noexcept
{
    return linkUpHeap->malloc(static_cast<uint32_t>(size));
}

void FIDPMessage::operator delete(void* ptr)
{
    linkUpHeap->free(ptr);
}

FIDPMessage::FIDPMessage(uint32_t toID, uint32_t bufferSize)
{
    messageSize = 0;
    fromID = 0;
    firstSendTime = 0;
    sendTime = 0;
    timesSent = 0;
    Clear();
    messageBuffer = static_cast<uint8_t*>(linkUpHeap->malloc(bufferSize));
    Assert(messageBuffer != nullptr, 0, "Message buffer is null: malloc failed");
    this->bufferSize = bufferSize;
    this->toID = toID;
    wasResent = 0;
}

FIDPMessage::~FIDPMessage()
{
    if (messageBuffer != nullptr)
    {
        linkUpHeap->free(messageBuffer);
    }
}

uint32_t FIDPMessage::SetMessageBuffer(void* data, uint32_t size)
{
    messageSize = size < bufferSize ? size : bufferSize;
    std::memcpy(messageBuffer, data, messageSize);
    return messageSize;
}

void FIDPMessage::Clear()
{
    fromID = 0;
    sendTime = 0;
    timesSent = 0;
    wasResent = 0;
}

int32_t FIDPMessage::ReceiveMessage(MCDirectPlay* directPlay)
{
    fromID = 0;
    uint32_t to;
    uint32_t size = bufferSize;
    const uint32_t result = directPlay->Receive(&fromID, &to, DPRECEIVE_ALL, messageBuffer, &size);
    Assert(result != DPERR_BUFFERTOOSMALL, bufferSize, " DP Buffer too small ");

    if (result == DP_OK)
    {
        messageSize = size;
    }

    return static_cast<int32_t>(result);
}
