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
    const std::span<std::byte> bytes = store.Allocate(64);
    REQUIRE_EQ(bytes.size(), 64u);
    CHECK(std::ranges::all_of(bytes, [](std::byte value) { return value == std::byte{0}; }));
    CHECK(store.Allocate(0).empty());

    const std::span<int32_t> numbers = store.AllocateArray<int32_t>(16);
    REQUIRE_EQ(numbers.size(), 16u);
    CHECK(std::ranges::all_of(numbers, [](int32_t value) { return value == 0; }));
    CHECK(store.AllocateArray<int32_t>(0).empty());
    CHECK_EQ(store.Count(), 2u);
    const MCAddressKey block = MCAddressKey::OfData(bytes);
    CHECK(store.Owns(block));

    // As the original's heaps: null, foreign blocks and blocks already freed are ignored.
    int32_t notABlock = 0;
    CHECK(!store.Free(MCAddressKey::Of(notABlock)));
    CHECK(!store.Free(MCAddressKey()));
    CHECK(store.Free(block));
    CHECK(!store.Free(block));
    CHECK(!store.Owns(block));
    CHECK_EQ(store.Count(), 1u);

    // What nothing frees goes with Clear.
    store.Allocate(8);
    store.Clear();
    CHECK_EQ(store.Count(), 0u);
}

TEST_CASE("block store: Make value-initialises and CopyString keeps the terminator")
{
    MCBlockStore store;
    const Defaults& record = store.Make<Defaults>();
    CHECK_EQ(record.count, 0);
    CHECK_EQ(record.id, -1);
    CHECK_EQ(record.scale, 1.0f);

    const std::span<char> copy = store.CopyString("forward");
    REQUIRE_EQ(copy.size(), 8u);
    CHECK_EQ(std::string(copy.data()), std::string("forward"));
    CHECK_EQ(copy[7], '\0');

    const std::span<char> empty = store.CopyString("");
    REQUIRE_EQ(empty.size(), 1u);
    CHECK_EQ(empty[0], '\0');
    store.Clear();
}

TEST_CASE("block store: a shape registered in a block is unregistered when the block goes")
{
    MCBlockStore store;
    const std::span<std::byte> shape = store.Allocate(256);
    REQUIRE(!shape.empty());
    const MCAddressKey shapeAt = MCAddressKey::OfData(shape);
    MCRenderer::RegisterData(std::span(reinterpret_cast<const uint8_t*>(shape.data()), 256), MCDataKind::Shapes);
    REQUIRE(MCRenderer::DataBlockOf(shapeAt).has_value());
    store.Free(shapeAt);
    CHECK(!MCRenderer::DataBlockOf(shapeAt).has_value());

    // Clear unregisters what is left, as the heap's destroy did.
    const std::span<std::byte> kept = store.Allocate(128);
    const MCAddressKey keptAt = MCAddressKey::OfData(kept);
    MCRenderer::RegisterData(std::span(reinterpret_cast<const uint8_t*>(kept.data()), 128), MCDataKind::Shapes);
    REQUIRE(MCRenderer::DataBlockOf(keptAt).has_value());
    store.Clear();
    CHECK(!MCRenderer::DataBlockOf(keptAt).has_value());
}
