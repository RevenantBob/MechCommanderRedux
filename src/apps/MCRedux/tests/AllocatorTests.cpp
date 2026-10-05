#include "stdafx.h"
#include "MCTest.h"
#include <mimalloc.h>

// The process allocator (platform/MCAllocator.h): main installed mimalloc before any test ran.

TEST_CASE("allocator: new, STL containers and SDL_malloc are served by mimalloc")
{
    const auto single = std::make_unique<int>(7);
    const auto array = std::make_unique<uint8_t[]>(1 << 20);
    std::vector<uint32_t> vector(1000);
    std::string text(200, 'x');

    CHECK(mi_is_in_heap_region(single.get()));
    CHECK(mi_is_in_heap_region(array.get()));
    CHECK(mi_is_in_heap_region(vector.data()));
    CHECK(mi_is_in_heap_region(text.data()));

    void* sdlBlock = SDL_malloc(64);
    REQUIRE(sdlBlock != nullptr);
    CHECK(mi_is_in_heap_region(sdlBlock));
    sdlBlock = SDL_realloc(sdlBlock, 1 << 16);
    REQUIRE(sdlBlock != nullptr);
    CHECK(mi_is_in_heap_region(sdlBlock));
    SDL_free(sdlBlock);
}
