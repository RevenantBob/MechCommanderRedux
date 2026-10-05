#include "stdafx.h"
#include "linkup/dpplayer.h"
#include "linkup/dpmessage.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"

FIDPPlayer::FIDPPlayer()
{
    numIncomingMessages = 0;
    lastLatency = 0;
    averageLatency = 0;
    reportedLatency = 0;
    totalPhysicalMemory = 0;
    latencyIndex = 0;

    for (int i = 0; i < 5; i++)
    {
        latencies[i] = 0;
    }

    resendDelay = 1500;
    outgoingSendCount = 0xff;
    id = 0;
    name[0] = '\0';
    longName[0] = '\0';
    flags = 0;
    nextIncomingToProcess = 0;
    nextIncomingSendCount = 0;
    playerNumber = -1;
    hasPlayerNumber = 1;

    for (int i = 0; i < 256; i++)
    {
        incomingMessages[i] = nullptr;
    }
}

FIDPPlayer::FIDPPlayer(uint32_t& id, const DPNAME* name, uint32_t flags)
{
    numIncomingMessages = 0;
    lastLatency = 0;
    averageLatency = 0;
    reportedLatency = 0;
    totalPhysicalMemory = 0;
    latencyIndex = 0;

    for (int i = 0; i < 5; i++)
    {
        latencies[i] = 0;
    }

    resendDelay = 1500;
    outgoingSendCount = 0xff;
    this->id = id;

    // Port fix: the buffers start cleared, so a name strncpy cuts short is still terminated (the heap block the
    // original got was not cleared).
    std::memset(this->name, 0, sizeof(this->name));
    std::memset(longName, 0, sizeof(longName));

    if (name->lpszShortNameA == nullptr)
    {
        this->name[0] = '\0';
    }
    else
    {
        std::strncpy(this->name, name->lpszShortNameA, 0x7f);
    }

    if (name->lpszLongNameA == nullptr)
    {
        longName[0] = '\0';
    }
    else
    {
        std::strncpy(longName, name->lpszLongNameA, 0xff);
    }

    this->flags = flags;
    playerNumber = -1;
    hasPlayerNumber = 1;

    for (int i = 0; i < 256; i++)
    {
        incomingMessages[i] = nullptr;
    }

    nextIncomingToProcess = 0;
    nextIncomingSendCount = 0;
}

FIDPPlayer::~FIDPPlayer()
{
    for (int i = 0; i < 256; i++)
    {
        if (incomingMessages[i] != nullptr)
        {
            delete incomingMessages[i];
        }
    }

    const int numGroups = groups.count;
    groups.current = groups.head;

    for (int i = 0; i < numGroups; i++)
    {
        uint32_t* groupID = groups.ReadAndNext();
        linkUpBlocks->Free(groupID);
    }

    while (groups.head != nullptr)
    {
        groups.Del(groups.head->data);
    }

    while (verifyList.head != nullptr)
    {
        verifyList.Del(verifyList.head->data);
    }
}

void FIDPPlayer::AddToVerifyList(FIDPMessage* msg)
{
    std::lock_guard lock(criticalSection);
    msg->sendTime = static_cast<uint32_t>(MCPort::PerformanceCounter());

    if (msg->wasResent == 0)
    {
        msg->firstSendTime = msg->sendTime;
    }

    verifyList.Add(msg);
    msg->timesSent = 1;
}

int32_t FIDPPlayer::AverageLatency()
{
    std::lock_guard lock(criticalSection);
    int32_t total = 0;
    int measured = 0;

    for (int i = 0; i < 5; i++)
    {
        total += latencies[i];

        if (latencies[i] > 0)
        {
            measured++;
        }
    }

    if (measured < 1)
    {
        averageLatency = 500;
    }
    else
    {
        averageLatency = total / measured;
    }

    return averageLatency;
}

FIDPMessage* FIDPPlayer::RemoveFromVerifyList(uint8_t sendCount)
{
    std::lock_guard lock(criticalSection);
    verifyList.current = verifyList.head;
    const int numMessages = verifyList.count;
    FIDPMessage* msg = nullptr;
    int i = 0;

    for (; i < numMessages; i++)
    {
        msg = verifyList.current->data;

        if (msg->messageBuffer[2 + playerNumber] == sendCount)
        {
            break;
        }

        // Original behaviour: the cursor stops on the last link instead of running off the list.
        if (verifyList.current != nullptr && verifyList.current != verifyList.tail)
        {
            verifyList.current = verifyList.current->next;
        }
    }

    if (i >= numMessages)
    {
        return nullptr;
    }

    const uint32_t now = static_cast<uint32_t>(MCPort::PerformanceCounter());

    if (msg->wasResent == 0)
    {
        lastLatency = (now - msg->sendTime) / TicksPerMS;
        latencies[latencyIndex] = static_cast<int32_t>(lastLatency);
        latencyIndex++;

        if (latencyIndex > 4)
        {
            latencyIndex = 0;
        }
    }

    verifyList.Del(verifyList.current->data);
    return msg;
}

int FIDPPlayer::IsVerifyListFull()
{
    return VerifyCountDifference() > 0x7f;
}

int FIDPPlayer::VerifyCountDifference()
{
    std::lock_guard lock(criticalSection);
    verifyList.current = verifyList.head;
    FIDPMessage* oldest = verifyList.head != nullptr ? verifyList.head->data : nullptr;

    if (oldest == nullptr)
    {
        return 0;
    }

    const uint8_t oldestCount = oldest->messageBuffer[2 + playerNumber];
    const uint8_t nextCount = static_cast<uint8_t>(outgoingSendCount + 1);

    if (nextCount < oldestCount)
    {
        return nextCount + (0x100 - oldestCount);
    }

    return nextCount - oldestCount;
}

int FIDPPlayer::HandleIncomingMessage(FIDPMessage* msg, int sendCount)
{
    Assert((*reinterpret_cast<uint16_t*>(msg->messageBuffer) & FIMSG_GUARANTEED) != 0, 0,
           "Should not call HandleIncomingMessage for non-guaranteed");
    int window;

    if (sendCount < nextIncomingSendCount)
    {
        window = 0x100 - nextIncomingSendCount;
    }
    else
    {
        window = -nextIncomingSendCount;
    }

    window = sendCount + window;

    if (window < 0 || window > 0x7f || incomingMessages[sendCount] != nullptr)
    {
        return 0;
    }

    incomingMessages[sendCount] = msg;
    numIncomingMessages++;

    if (sendCount == nextIncomingSendCount)
    {
        SetNextIncomingSendCount();
    }

    return 1;
}

void FIDPPlayer::SetNextIncomingSendCount()
{
    nextIncomingSendCount++;
    const uint8_t start = nextIncomingSendCount;

    while (incomingMessages[nextIncomingSendCount] != nullptr)
    {
        nextIncomingSendCount++;
        Assert(nextIncomingSendCount != start, 0, "Guaranteed message send_count problem2");
    }
}

FIDPMessage* FIDPPlayer::NextMessageToProcess()
{
    if (incomingMessages[nextIncomingToProcess] == nullptr)
    {
        return nullptr;
    }

    FIDPMessage* msg = incomingMessages[nextIncomingToProcess];
    incomingMessages[nextIncomingToProcess] = nullptr;
    nextIncomingToProcess++;
    numIncomingMessages--;
    return msg;
}

void FIDPPlayer::JoinGroup(uint32_t groupID)
{
    uint32_t* id = static_cast<uint32_t*>(linkUpBlocks->Allocate(sizeof(uint32_t)));
    *id = groupID;
    groups.Add(id);
}

void FIDPPlayer::LeaveGroup(uint32_t groupID)
{
    uint32_t* found = nullptr;

    for (FLink<uint32_t>* link = groups.head; link != nullptr; link = link->next)
    {
        if (*link->data == groupID)
        {
            found = link->data;
            break;
        }
    }

    if (found == nullptr)
    {
        return;
    }

    groups.Del(found);
    linkUpBlocks->Free(found);
}

void FIDPPlayer::ClearList(FLinkedList<FIDPPlayer>& list)
{
    const int numPlayers = list.count;
    list.current = list.head;

    for (int i = 0; i < numPlayers; i++)
    {
        FIDPPlayer* player = list.current->data;
        list.Del(player);
        delete player;
    }

    Assert(list.count == 0, 0, nullptr);
}

int FIDPPlayer::IsInGroup(uint32_t groupID)
{
    for (FLink<uint32_t>* link = groups.head; link != nullptr; link = link->next)
    {
        if (*link->data == groupID)
        {
            return 1;
        }
    }

    return 0;
}
