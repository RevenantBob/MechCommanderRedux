#include "stdafx.h"
#include "platform/MCBlockStore.h"
#include "platform/MCRenderer.h"

std::span<std::byte> MCBlockStore::Allocate(size_t size)
{
    if (size == 0)
    {
        return {};
    }

    auto data = std::make_unique<uint8_t[]>(size);
    const std::span<uint8_t> block(data.get(), size);
    _Blocks.emplace(MCAddressKey::OfData(block), Block{std::move(data), size});
    return std::as_writable_bytes(block);
}

std::span<char> MCBlockStore::CopyString(std::string_view text)
{
    const std::span<std::byte> block = Allocate(text.size() + 1);
    const std::span<char> copy(reinterpret_cast<char*>(block.data()), block.size());
    std::ranges::copy(text, copy.begin());
    return copy;
}

bool MCBlockStore::Free(MCAddressKey block)
{
    if (!block)
    {
        return false;
    }

    auto found = _Blocks.find(block);

    if (found == _Blocks.end())
    {
        return false;
    }

    MCRenderer::UnregisterData(std::span(found->second.Data.get(), found->second.Size));
    _Blocks.erase(found);
    return true;
}

void MCBlockStore::Clear()
{
    for (auto& [address, block] : _Blocks)
    {
        MCRenderer::UnregisterData(std::span(block.Data.get(), block.Size));
    }

    _Blocks.clear();
}
