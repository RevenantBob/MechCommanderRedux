#include "stdafx.h"
#include "platform/MCBlockStore.h"
#include "platform/MCRenderer.h"

void* MCBlockStore::Allocate(size_t size)
{
    if (size == 0)
    {
        return nullptr;
    }

    auto data = std::make_unique<std::byte[]>(size);
    void* address = data.get();
    _Blocks.emplace(address, Block{std::move(data), size});
    return address;
}

char* MCBlockStore::CopyString(std::string_view text)
{
    auto* copy = static_cast<char*>(Allocate(text.size() + 1));
    std::memcpy(copy, text.data(), text.size());
    return copy;
}

bool MCBlockStore::Free(void* block)
{
    if (block == nullptr)
    {
        return false;
    }

    auto found = _Blocks.find(block);

    if (found == _Blocks.end())
    {
        return false;
    }

    MCRenderer::UnregisterData(block, found->second.Size);
    _Blocks.erase(found);
    return true;
}

void MCBlockStore::Clear()
{
    for (auto& [address, block] : _Blocks)
    {
        MCRenderer::UnregisterData(address, block.Size);
    }

    _Blocks.clear();
}
