#include "stdafx.h"
#include "MCTest.h"
#include "platform/MCRenderer.h"
#include "sprite/sprtmgr.h"

TEST_CASE("sprites: the manager's data blocks are zeroed and go with it")
{
    MCSpriteManager manager;
    auto* table = static_cast<uint8_t*>(manager.MallocDataRam(64));
    REQUIRE(table != nullptr);
    CHECK(std::ranges::all_of(std::span(table, 64), [](uint8_t value) { return value == 0; }));
    CHECK(manager.MallocDataRam(0) == nullptr);

    // As the original's heap: freeing a block that isn't the manager's does nothing.
    int32_t notABlock = 0;
    manager.FreeDataRam(&notABlock);
    manager.FreeDataRam(nullptr);
    CHECK_EQ(manager.DataBlocks.size(), 1u);

    manager.FreeDataRam(table);
    CHECK(manager.DataBlocks.empty());

    // What is never freed (a type's animation states, say) goes when the manager is destroyed.
    manager.MallocDataRam(32);
    manager.MallocDataRam(48);
    manager.Destroy();
    CHECK(manager.DataBlocks.empty());
}

TEST_CASE("sprites: shape blocks are registered with the renderers until freed")
{
    MCSpriteManager manager;
    auto* shape = static_cast<uint8_t*>(manager.MallocShapeRam(256));
    REQUIRE(shape != nullptr);
    const MCDataBlock* block = MCRenderer::DataBlockOf(shape);
    REQUIRE(block != nullptr);
    CHECK(block->Begin == shape);
    CHECK(block->End == shape + 256);
    CHECK(block->Kind == MCDataKind::Shapes);

    manager.FreeShapeRam(shape);
    CHECK(MCRenderer::DataBlockOf(shape) == nullptr);

    // Shapes still cached when the mission ends are unregistered with the manager.
    auto* cached = static_cast<uint8_t*>(manager.MallocShapeRam(128));
    REQUIRE(MCRenderer::DataBlockOf(cached) != nullptr);
    manager.Destroy();
    CHECK(MCRenderer::DataBlockOf(cached) == nullptr);
    CHECK(manager.ShapeBlocks.empty());
}
