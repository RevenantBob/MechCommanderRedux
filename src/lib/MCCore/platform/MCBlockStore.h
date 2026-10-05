#pragma once

#include "platform/MCAddressKey.h"

/// <summary>
/// Owns blocks of memory handed out to code that keeps raw pointers to them: each block comes zeroed, can be freed on
/// its own, and whatever is left goes when the store is cleared or destroyed. Freeing a block also drops any renderer
/// data registered in it.
/// </summary>
/// <remarks>
/// Port: stands in for the original's per-system heaps (ABL's, the object type cache) where a block has no single
/// owner to hold it: the heap was the owner, and many blocks were only ever freed with the heap. Phase 3 of the
/// modernization gives those blocks real owners.
/// </remarks>
class MCBlockStore
{
public:
    MCBlockStore() = default;

    /// <summary>
    /// Frees the blocks without touching the renderers: the stores are globals, and at exit the renderers' tables may
    /// be gone already. Call <see cref="Clear"/> first when blocks may hold registered data.
    /// </summary>
    ~MCBlockStore() = default;

    MCBlockStore(const MCBlockStore&) = delete;
    MCBlockStore& operator=(const MCBlockStore&) = delete;

    /// <summary>A zeroed block of <paramref name="size"/> bytes, or none for 0 bytes.</summary>
    std::span<std::byte> Allocate(size_t size);

    /// <summary>A zeroed array of <paramref name="count"/> <typeparamref name="T"/>s, or none for none.</summary>
    template <typename T> std::span<T> AllocateArray(size_t count)
    {
        static_assert(std::is_trivially_destructible_v<T>, "a block store never runs destructors");
        const std::span<std::byte> block = Allocate(count * sizeof(T));
        return block.empty() ? std::span<T>() : std::span<T>(std::launder(reinterpret_cast<T*>(block.data())), count);
    }

    /// <summary>A value-initialised <typeparamref name="T"/> in a new block.</summary>
    template <typename T> T& Make()
    {
        static_assert(std::is_trivially_destructible_v<T>, "a block store never runs destructors");
        return *std::construct_at(reinterpret_cast<T*>(Allocate(sizeof(T)).data()));
    }

    /// <summary>A copy of <paramref name="text"/> with its terminator (the span holds both).</summary>
    std::span<char> CopyString(std::string_view text);

    /// <summary>Frees the block at <paramref name="block"/>; addresses that aren't this store's blocks are ignored.</summary>
    /// <returns>Whether the block was this store's.</returns>
    bool Free(MCAddressKey block);

    /// <summary>Frees every block.</summary>
    void Clear();

    /// <summary>Whether a live block of this store starts at <paramref name="block"/>.</summary>
    bool Owns(MCAddressKey block) const { return _Blocks.contains(block); }

    /// <summary>How many blocks are live.</summary>
    size_t Count() const { return _Blocks.size(); }

private:
    /// <summary>A live block and its size.</summary>
    struct Block
    {
        std::unique_ptr<uint8_t[]> Data;
        size_t Size = 0;
    };

    /// <summary>The live blocks, by address.</summary>
    std::unordered_map<MCAddressKey, Block> _Blocks;
};
