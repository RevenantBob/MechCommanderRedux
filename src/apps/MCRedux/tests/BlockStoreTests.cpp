#include "stdafx.h"
#include "MCTest.h"
#include "platform/MCBlockStore.h"
#include "platform/MCRenderer.h"

// MCBlockStore: the owner of blocks that the original kept in its per-system heaps (ABL's memory, the object type
// cache, the smoke spheres, the global map's door lists).

namespace
{
    /// <summary>A record with a non-zero default, to tell value-initialisation from a raw zeroed block.</summary>
    struct Defaults
    {
        int32_t count = 0;
        int32_t id = -1;
        float scale = 1.0f;
    };
}

TEST_CASE("block store: blocks come zeroed and are freed one by one or all together")
{
    MCBlockStore store;
    auto* bytes = static_cast<uint8_t*>(store.Allocate(64));
    REQUIRE(bytes != nullptr);
    CHECK(std::ranges::all_of(std::span(bytes, 64), [](uint8_t value) { return value == 0; }));
    CHECK(store.Allocate(0) == nullptr);

    auto* numbers = store.AllocateArray<int32_t>(16);
    REQUIRE(numbers != nullptr);
    CHECK(std::ranges::all_of(std::span(numbers, 16), [](int32_t value) { return value == 0; }));
    CHECK(store.AllocateArray<int32_t>(0) == nullptr);
    CHECK_EQ(store.Count(), 2u);
    CHECK(store.Owns(bytes));

    // As the original's heaps: null, foreign blocks and blocks already freed are ignored.
    int32_t notABlock = 0;
    CHECK(!store.Free(&notABlock));
    CHECK(!store.Free(nullptr));
    CHECK(store.Free(bytes));
    CHECK(!store.Free(bytes));
    CHECK(!store.Owns(bytes));
    CHECK_EQ(store.Count(), 1u);

    // What nothing frees goes with Clear.
    store.Allocate(8);
    store.Clear();
    CHECK_EQ(store.Count(), 0u);
}

TEST_CASE("block store: Make value-initialises and CopyString keeps the terminator")
{
    MCBlockStore store;
    Defaults* record = store.Make<Defaults>();
    REQUIRE(record != nullptr);
    CHECK_EQ(record->count, 0);
    CHECK_EQ(record->id, -1);
    CHECK_EQ(record->scale, 1.0f);

    char* copy = store.CopyString("forward");
    REQUIRE(copy != nullptr);
    CHECK_EQ(std::string(copy), std::string("forward"));
    CHECK_EQ(copy[7], '\0');

    char* empty = store.CopyString("");
    REQUIRE(empty != nullptr);
    CHECK_EQ(empty[0], '\0');
    store.Clear();
}

TEST_CASE("block store: a shape registered in a block is unregistered when the block goes")
{
    MCBlockStore store;
    auto* shape = static_cast<uint8_t*>(store.Allocate(256));
    REQUIRE(shape != nullptr);
    MCRenderer::RegisterData(shape, 256, MCDataKind::Shapes);
    REQUIRE(MCRenderer::DataBlockOf(shape) != nullptr);
    store.Free(shape);
    CHECK(MCRenderer::DataBlockOf(shape) == nullptr);

    // Clear unregisters what is left, as the heap's destroy did.
    auto* kept = static_cast<uint8_t*>(store.Allocate(128));
    MCRenderer::RegisterData(kept, 128, MCDataKind::Shapes);
    REQUIRE(MCRenderer::DataBlockOf(kept) != nullptr);
    store.Clear();
    CHECK(MCRenderer::DataBlockOf(kept) == nullptr);
}
