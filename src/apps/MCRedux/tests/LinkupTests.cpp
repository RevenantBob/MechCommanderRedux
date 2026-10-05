#include "stdafx.h"
#include "MCTest.h"
#include "linkup/dpmessage.h"
#include "linkup/sessionmanager.h"

// Linkup's memory once the linkup heap is gone: the blocks with no single owner live in linkUpBlocks.

TEST_CASE("linkup: a message's buffer is a zeroed linkup block, freed with the message")
{
    InitLinkUpBlocks();
    REQUIRE(linkUpBlocks != nullptr);
    MCBlockStore* store = linkUpBlocks.get();
    InitLinkUpBlocks();
    CHECK(linkUpBlocks.get() == store);
    const size_t before = store->Count();

    auto* message = new FIDPMessage(7, 64);
    REQUIRE(message->messageBuffer != nullptr);
    CHECK(store->Owns(message->messageBuffer));
    CHECK_EQ(store->Count(), before + 1);
    CHECK(std::ranges::all_of(std::span(message->messageBuffer, 64), [](uint8_t value) { return value == 0; }));

    uint8_t data[100] = {};
    data[0] = 0x42;
    CHECK_EQ(message->SetMessageBuffer(data, sizeof(data)), 64u);
    CHECK_EQ(message->messageBuffer[0], 0x42);

    delete message;
    CHECK_EQ(store->Count(), before);

    // What is never freed goes when the store is destroyed.
    store->Allocate(16);
    DestroyLinkUpBlocks();
    CHECK(linkUpBlocks == nullptr);
}
