#include "stdafx.h"
#include "linkup/dpplayer.h"
#include "linkup/dpmessage.h"
#include "linkup/sessionmanager.h"
#include "lib/aerror.h"

MCFidpPlayer::MCFidpPlayer()
{
    NumIncomingMessages = 0;
    LastLatency = 0;
    LastAverageLatency = 0;
    ReportedLatency = 0;
    TotalPhysicalMemory = 0;
    LatencyIndex = 0;

    for (int i = 0; i < 5; i++)
    {
        Latencies[i] = 0;
    }

    ResendDelay = 1500;
    OutgoingSendCount = 0xff;
    Id = 0;
    Name[0] = '\0';
    LongName[0] = '\0';
    Flags = 0;
    NextIncomingToProcess = 0;
    NextIncomingSendCount = 0;
    PlayerNumber = -1;
    HasPlayerNumber = 1;

    for (int i = 0; i < 256; i++)
    {
        IncomingMessages[i] = nullptr;
    }
}

MCFidpPlayer::MCFidpPlayer(uint32_t& id, const DPNAME* name, uint32_t flags)
{
    NumIncomingMessages = 0;
    LastLatency = 0;
    LastAverageLatency = 0;
    ReportedLatency = 0;
    TotalPhysicalMemory = 0;
    LatencyIndex = 0;

    for (int i = 0; i < 5; i++)
    {
        Latencies[i] = 0;
    }

    ResendDelay = 1500;
    OutgoingSendCount = 0xff;
    this->Id = id;

    // Port fix: the buffers start cleared, so a name strncpy cuts short is still terminated (the heap block the
    // original got was not cleared).
    std::memset(this->Name, 0, sizeof(this->Name));
    std::memset(LongName, 0, sizeof(LongName));

    if (name->lpszShortNameA == nullptr)
    {
        this->Name[0] = '\0';
    }
    else
    {
        std::strncpy(this->Name, name->lpszShortNameA, 0x7f);
    }

    if (name->lpszLongNameA == nullptr)
    {
        LongName[0] = '\0';
    }
    else
    {
        std::strncpy(LongName, name->lpszLongNameA, 0xff);
    }

    this->Flags = flags;
    PlayerNumber = -1;
    HasPlayerNumber = 1;

    for (int i = 0; i < 256; i++)
    {
        IncomingMessages[i] = nullptr;
    }

    NextIncomingToProcess = 0;
    NextIncomingSendCount = 0;
}

MCFidpPlayer::~MCFidpPlayer()
{
    for (int i = 0; i < 256; i++)
    {
        if (IncomingMessages[i] != nullptr)
        {
            delete IncomingMessages[i];
        }
    }

    const int numGroups = Groups.Count;
    Groups.Current = Groups.HeadLink;

    for (int i = 0; i < numGroups; i++)
    {
        uint32_t* groupID = Groups.ReadAndNext();
        LinkUpBlocks->Free(groupID);
    }

    while (Groups.HeadLink != nullptr)
    {
        Groups.Del(Groups.HeadLink->Data);
    }

    while (VerifyList.HeadLink != nullptr)
    {
        VerifyList.Del(VerifyList.HeadLink->Data);
    }
}

void MCFidpPlayer::AddToVerifyList(MCFidpMessage* msg)
{
    std::lock_guard lock(CriticalSection);
    msg->SendTime = static_cast<uint32_t>(MCPort::PerformanceCounter());

    if (msg->WasResent == 0)
    {
        msg->FirstSendTime = msg->SendTime;
    }

    VerifyList.Add(msg);
    msg->TimesSent = 1;
}

int32_t MCFidpPlayer::AverageLatency()
{
    std::lock_guard lock(CriticalSection);
    int32_t total = 0;
    int measured = 0;

    for (int i = 0; i < 5; i++)
    {
        total += Latencies[i];

        if (Latencies[i] > 0)
        {
            measured++;
        }
    }

    if (measured < 1)
    {
        LastAverageLatency = 500;
    }
    else
    {
        LastAverageLatency = total / measured;
    }

    return LastAverageLatency;
}

MCFidpMessage* MCFidpPlayer::RemoveFromVerifyList(uint8_t sendCount)
{
    std::lock_guard lock(CriticalSection);
    VerifyList.Current = VerifyList.HeadLink;
    const int numMessages = VerifyList.Count;
    MCFidpMessage* msg = nullptr;
    int i = 0;

    for (; i < numMessages; i++)
    {
        msg = VerifyList.Current->Data;

        if (msg->MessageBuffer[2 + PlayerNumber] == sendCount)
        {
            break;
        }

        // Original behaviour: the cursor stops on the last link instead of running off the list.
        if (VerifyList.Current != nullptr && VerifyList.Current != VerifyList.Tail)
        {
            VerifyList.Current = VerifyList.Current->Next;
        }
    }

    if (i >= numMessages)
    {
        return nullptr;
    }

    const uint32_t now = static_cast<uint32_t>(MCPort::PerformanceCounter());

    if (msg->WasResent == 0)
    {
        LastLatency = (now - msg->SendTime) / TicksPerMS;
        Latencies[LatencyIndex] = static_cast<int32_t>(LastLatency);
        LatencyIndex++;

        if (LatencyIndex > 4)
        {
            LatencyIndex = 0;
        }
    }

    VerifyList.Del(VerifyList.Current->Data);
    return msg;
}

int MCFidpPlayer::IsVerifyListFull()
{
    return VerifyCountDifference() > 0x7f;
}

int MCFidpPlayer::VerifyCountDifference()
{
    std::lock_guard lock(CriticalSection);
    VerifyList.Current = VerifyList.HeadLink;
    MCFidpMessage* oldest = VerifyList.HeadLink != nullptr ? VerifyList.HeadLink->Data : nullptr;

    if (oldest == nullptr)
    {
        return 0;
    }

    const uint8_t oldestCount = oldest->MessageBuffer[2 + PlayerNumber];
    const uint8_t nextCount = static_cast<uint8_t>(OutgoingSendCount + 1);

    if (nextCount < oldestCount)
    {
        return nextCount + (0x100 - oldestCount);
    }

    return nextCount - oldestCount;
}

int MCFidpPlayer::HandleIncomingMessage(MCFidpMessage* msg, int sendCount)
{
    Assert((*reinterpret_cast<uint16_t*>(msg->MessageBuffer) & FIMSG_GUARANTEED) != 0, 0,
           "Should not call HandleIncomingMessage for non-guaranteed");
    int window;

    if (sendCount < NextIncomingSendCount)
    {
        window = 0x100 - NextIncomingSendCount;
    }
    else
    {
        window = -NextIncomingSendCount;
    }

    window = sendCount + window;

    if (window < 0 || window > 0x7f || IncomingMessages[sendCount] != nullptr)
    {
        return 0;
    }

    IncomingMessages[sendCount] = msg;
    NumIncomingMessages++;

    if (sendCount == NextIncomingSendCount)
    {
        SetNextIncomingSendCount();
    }

    return 1;
}

void MCFidpPlayer::SetNextIncomingSendCount()
{
    NextIncomingSendCount++;
    const uint8_t start = NextIncomingSendCount;

    while (IncomingMessages[NextIncomingSendCount] != nullptr)
    {
        NextIncomingSendCount++;
        Assert(NextIncomingSendCount != start, 0, "Guaranteed message send_count problem2");
    }
}

MCFidpMessage* MCFidpPlayer::NextMessageToProcess()
{
    if (IncomingMessages[NextIncomingToProcess] == nullptr)
    {
        return nullptr;
    }

    MCFidpMessage* msg = IncomingMessages[NextIncomingToProcess];
    IncomingMessages[NextIncomingToProcess] = nullptr;
    NextIncomingToProcess++;
    NumIncomingMessages--;
    return msg;
}

void MCFidpPlayer::JoinGroup(uint32_t groupID)
{
    uint32_t* id = static_cast<uint32_t*>(LinkUpBlocks->Allocate(sizeof(uint32_t)));
    *id = groupID;
    Groups.Add(id);
}

void MCFidpPlayer::LeaveGroup(uint32_t groupID)
{
    uint32_t* found = nullptr;

    for (MCFLink<uint32_t>* link = Groups.HeadLink; link != nullptr; link = link->Next)
    {
        if (*link->Data == groupID)
        {
            found = link->Data;
            break;
        }
    }

    if (found == nullptr)
    {
        return;
    }

    Groups.Del(found);
    LinkUpBlocks->Free(found);
}

void MCFidpPlayer::ClearList(MCFLinkedList<MCFidpPlayer>& list)
{
    const int numPlayers = list.Count;
    list.Current = list.HeadLink;

    for (int i = 0; i < numPlayers; i++)
    {
        MCFidpPlayer* player = list.Current->Data;
        list.Del(player);
        delete player;
    }

    Assert(list.Count == 0, 0, nullptr);
}

int MCFidpPlayer::IsInGroup(uint32_t groupID)
{
    for (MCFLink<uint32_t>* link = Groups.HeadLink; link != nullptr; link = link->Next)
    {
        if (*link->Data == groupID)
        {
            return 1;
        }
    }

    return 0;
}
