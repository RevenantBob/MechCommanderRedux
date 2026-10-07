#pragma once

#include "platform/MCRenderer.h"

/// <summary>
/// A zeroed block of bytes registered with the renderers as draw data (<see cref="MCRenderer::RegisterData"/>) for as
/// long as it lives: the owner of shape tables and colour tables that a renderer may keep something made from.
/// </summary>
class MCRegisteredBlock
{
public:
    /// <summary>No block.</summary>
    MCRegisteredBlock() = default;

    /// <summary>A zeroed block of <paramref name="size"/> bytes holding <paramref name="kind"/>; none for 0 bytes.</summary>
    MCRegisteredBlock(size_t size, MCDataKind kind);

    /// <summary>Unregisters and frees the block.</summary>
    ~MCRegisteredBlock();

    MCRegisteredBlock(MCRegisteredBlock&& other) noexcept;
    MCRegisteredBlock& operator=(MCRegisteredBlock&& other) noexcept;
    MCRegisteredBlock(const MCRegisteredBlock&) = delete;
    MCRegisteredBlock& operator=(const MCRegisteredBlock&) = delete;

    /// <summary>The bytes (null for no block).</summary>
    uint8_t* Data() const { return _Data.get(); }

    /// <summary>The number of bytes.</summary>
    size_t Size() const { return _Size; }

    /// <summary>The bytes as a span.</summary>
    std::span<uint8_t> Bytes() const { return {_Data.get(), _Size}; }

    /// <summary>Whether there is a block.</summary>
    bool Empty() const { return _Data == nullptr; }

private:
    /// <summary>Unregisters and frees the block, leaving none.</summary>
    void Release();

    /// <summary>The bytes.</summary>
    std::unique_ptr<uint8_t[]> _Data;
    /// <summary>The number of bytes.</summary>
    size_t _Size = 0;
};
