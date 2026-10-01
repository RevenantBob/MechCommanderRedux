#include "stdafx.h"
#include "engine/celement.h"
#include "lib/aerror.h"
#include "lib/heap.h"

HeapManager* ElementPool::poolHeap = nullptr;
int32_t ElementPool::size = 0;
int32_t ElementPool::dataEdge = 0;
int32_t ElementPool::elementCount = 0;

Element::Element(int32_t _depth)
{
    depth = static_cast<float>(_depth);
    unknown08 = 1;
}

Element::Element(float _depth)
{
    const int16_t whole = static_cast<int16_t>(static_cast<int32_t>(std::floor(static_cast<double>(_depth))));
    unknown08 = 1;
    depth = static_cast<float>(static_cast<int32_t>(whole));
}

auto Element::operator new(size_t size) noexcept -> void*
{
    void* element = ElementPool::malloc(static_cast<int32_t>(size));

    if (element == nullptr)
    {
        Fatal(0xeeeb0003, nullptr, nullptr);
    }

    return element;
}

auto ElementPool::malloc(int32_t allocSize) -> uint8_t*
{
    dataEdge -= allocSize;
    elementCount++;

    if (dataEdge < 0)
    {
        return nullptr;
    }

    return poolHeap->getHeapPtr() + dataEdge;
}

auto ElementPool::reset() -> void
{
    elementCount = 0;
    dataEdge = size;
}

auto ElementPool::init(int32_t poolSize) -> int32_t
{
    size = poolSize;
    poolHeap = new HeapManager();

    if (poolHeap == nullptr)
    {
        return -0x1114fffe;
    }

    int32_t result = poolHeap->createHeap(static_cast<uint32_t>(poolSize));

    if (result == 0)
    {
        result = poolHeap->commitHeap(static_cast<uint32_t>(poolSize));

        if (result == 0)
        {
            reset();
            result = 0;
        }
    }

    return result;
}

auto ElementPool::free() -> void
{
    delete poolHeap;
    poolHeap = nullptr;
}
