#pragma once

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

    /// <summary>A zeroed block of <paramref name="size"/> bytes, or null for 0 bytes.</summary>
    void* Allocate(size_t size);

    /// <summary>A zeroed array of <paramref name="count"/> <typeparamref name="T"/>s, or null for none.</summary>
    template <typename T> T* AllocateArray(size_t count)
    {
        static_assert(std::is_trivially_destructible_v<T>, "a block store never runs destructors");
        return static_cast<T*>(Allocate(count * sizeof(T)));
    }

    /// <summary>A value-initialised <typeparamref name="T"/> in a new block.</summary>
    template <typename T> T* Make()
    {
        static_assert(std::is_trivially_destructible_v<T>, "a block store never runs destructors");
        return std::construct_at(static_cast<T*>(Allocate(sizeof(T))));
    }

    /// <summary>A copy of <paramref name="text"/> with its terminator.</summary>
    char* CopyString(std::string_view text);

    /// <summary>Frees a block; null and blocks that aren't this store's are ignored.</summary>
    /// <returns>Whether the block was this store's.</returns>
    bool Free(void* block);

    /// <summary>Frees every block.</summary>
    void Clear();

    /// <summary>Whether <paramref name="block"/> is a live block of this store.</summary>
    bool Owns(const void* block) const { return _Blocks.contains(const_cast<void*>(block)); }

    /// <summary>How many blocks are live.</summary>
    size_t Count() const { return _Blocks.size(); }

private:
    /// <summary>A live block and its size.</summary>
    struct Block
    {
        std::unique_ptr<std::byte[]> Data;
        size_t Size = 0;
    };

    /// <summary>The live blocks, by address.</summary>
    std::unordered_map<void*, Block> _Blocks;
};
