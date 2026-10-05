#include "stdafx.h"
#include "MCTest.h"
#include "platform/MCRenderer.h"
#include "sprite/sprtmgr.h"

TEST_CASE("sprites: the manager's data blocks are zeroed and go with it")
{
    SpriteManager manager;
    auto* table = static_cast<uint8_t*>(manager.mallocDataRAM(64));
    REQUIRE(table != nullptr);
    CHECK(std::ranges::all_of(std::span(table, 64), [](uint8_t value) { return value == 0; }));
    CHECK(manager.mallocDataRAM(0) == nullptr);

    // As the original's heap: freeing a block that isn't the manager's does nothing.
    int32_t notABlock = 0;
    manager.freeDataRAM(&notABlock);
    manager.freeDataRAM(nullptr);
    CHECK_EQ(manager.dataBlocks.size(), 1u);

    manager.freeDataRAM(table);
    CHECK(manager.dataBlocks.empty());

    // What is never freed (a type's animation states, say) goes when the manager is destroyed.
    manager.mallocDataRAM(32);
    manager.mallocDataRAM(48);
    manager.destroy();
    CHECK(manager.dataBlocks.empty());
}

TEST_CASE("sprites: shape blocks are registered with the renderers until freed")
{
    SpriteManager manager;
    auto* shape = static_cast<uint8_t*>(manager.mallocShapeRAM(256));
    REQUIRE(shape != nullptr);
    const MCDataBlock* block = MCRenderer::DataBlockOf(shape);
    REQUIRE(block != nullptr);
    CHECK(block->Begin == shape);
    CHECK(block->End == shape + 256);
    CHECK(block->Kind == MCDataKind::Shapes);

    manager.freeShapeRAM(shape);
    CHECK(MCRenderer::DataBlockOf(shape) == nullptr);

    // Shapes still cached when the mission ends are unregistered with the manager.
    auto* cached = static_cast<uint8_t*>(manager.mallocShapeRAM(128));
    REQUIRE(MCRenderer::DataBlockOf(cached) != nullptr);
    manager.destroy();
    CHECK(MCRenderer::DataBlockOf(cached) == nullptr);
    CHECK(manager.shapeBlocks.empty());
}
