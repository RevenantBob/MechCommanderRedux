#include "stdafx.h"
#include "platform/MCRegisteredBlock.h"

MCRegisteredBlock::MCRegisteredBlock(size_t size, MCDataKind kind)
{
    if (size == 0)
    {
        return;
    }

    _Data = std::make_unique<uint8_t[]>(size);
    _Size = size;
    MCRenderer::RegisterData(_Data.get(), size, kind);
}

MCRegisteredBlock::~MCRegisteredBlock()
{
    Release();
}

MCRegisteredBlock::MCRegisteredBlock(MCRegisteredBlock&& other) noexcept
    : _Data(std::move(other._Data)), _Size(std::exchange(other._Size, 0))
{
}

auto MCRegisteredBlock::operator=(MCRegisteredBlock&& other) noexcept -> MCRegisteredBlock&
{
    if (this != &other)
    {
        Release();
        _Data = std::move(other._Data);
        _Size = std::exchange(other._Size, 0);
    }

    return *this;
}

auto MCRegisteredBlock::Release() -> void
{
    if (_Data != nullptr)
    {
        MCRenderer::UnregisterData(_Data.get());
        _Data.reset();
    }

    _Size = 0;
}
