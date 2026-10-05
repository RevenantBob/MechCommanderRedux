#include "stdafx.h"
#include "engine/celement.h"

std::vector<std::unique_ptr<Element, ElementPool::Deleter>> ElementPool::elements;
int32_t ElementPool::elementCount = 0;

Element::Element(int32_t _depth)
{
    depth = static_cast<float>(_depth);
}

Element::Element(float _depth)
{
    const int16_t whole = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(_depth))));
    depth = static_cast<float>(static_cast<int32_t>(whole));
}

auto ElementPool::Deleter::operator()(Element* element) const -> void
{
    element->~Element();
    delete[] reinterpret_cast<std::byte*>(element);
}

auto ElementPool::reset() -> void
{
    elementCount = 0;
    elements.clear();
}

auto ElementPool::init(int32_t) -> int32_t
{
    reset();
    return 0;
}

auto ElementPool::free() -> void
{
    reset();
    elements.shrink_to_fit();
}
