#pragma once

/// <summary>
/// Makes mimalloc the process allocator: this unit replaces the global <c>operator new</c>/<c>delete</c>
/// (<c>mimalloc-new-delete.h</c>), and <see cref="MCInitializeAllocator"/> points SDL's allocations at it too.
/// </summary>
/// <remarks>
/// Call it first in <c>main</c>, before any SDL call: memory SDL allocated through its default functions must not be
/// freed through mimalloc. The call also makes the linker pull this unit (and so the new/delete replacements) out of
/// the MCCore library.
/// </remarks>
/// <returns>Nothing, or why SDL refused the functions or <c>new</c> isn't served by mimalloc. Failing is fatal.</returns>
std::expected<void, std::string> MCInitializeAllocator();
