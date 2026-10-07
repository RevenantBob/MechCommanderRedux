#include "stdafx.h"
#include "engine/celement.h"

std::vector<std::unique_ptr<MCElement, MCElementPool::Deleter>> MCElementPool::Elements;
int32_t MCElementPool::ElementCount = 0;

MCElement::MCElement(int32_t depth)
{
    Depth = static_cast<float>(depth);
}

MCElement::MCElement(float depth)
{
    const int16_t whole = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(depth))));
    Depth = static_cast<float>(static_cast<int32_t>(whole));
}

auto MCElementPool::Deleter::operator()(MCElement* element) const -> void
{
    element->~MCElement();
    delete[] reinterpret_cast<std::byte*>(element);
}

auto MCElementPool::Reset() -> void
{
    ElementCount = 0;
    Elements.clear();
}

auto MCElementPool::Init(int32_t) -> int32_t
{
    Reset();
    return 0;
}

auto MCElementPool::Free() -> void
{
    Reset();
    Elements.shrink_to_fit();
}
