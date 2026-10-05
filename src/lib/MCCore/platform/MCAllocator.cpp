#include "stdafx.h"
#include "platform/MCAllocator.h"
#include <mimalloc-new-delete.h>

std::expected<void, std::string> MCInitializeAllocator()
{
    if (!SDL_SetMemoryFunctions(mi_malloc, mi_calloc, mi_realloc, mi_free))
    {
        return std::unexpected(std::format("SDL_SetMemoryFunctions failed: {}", SDL_GetError()));
    }

    // The new/delete replacements only win when the linker took them instead of the CRT's: check that it did.
    const auto probe = std::make_unique<int>();

    if (!mi_is_in_heap_region(probe.get()))
    {
        return std::unexpected("operator new isn't served by mimalloc (the CRT's won at link time)");
    }

    return {};
}
