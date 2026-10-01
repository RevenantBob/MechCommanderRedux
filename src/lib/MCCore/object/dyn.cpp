#include "stdafx.h"
#include "object/dyn.h"
#include "lib/heap.h"
#include "object/objtype.h"

auto DynamicsType::operator new(size_t size) noexcept -> void*
{
    void* result = nullptr;
    UserHeap* heap = ObjectTypeManager::objectTypeCache;

    if (heap != nullptr && heap->heapSize != 0)
    {
        result = heap->malloc(static_cast<uint32_t>(size));
    }

    return result;
}

auto DynamicsType::operator delete(void* ptr) -> void
{
    UserHeap* heap = ObjectTypeManager::objectTypeCache;

    if (heap != nullptr && heap->heapSize != 0)
    {
        heap->free(ptr);
    }
}

auto DynamicsType::destroy() -> void
{
}

auto DynamicsType::createInstance() -> Dynamics*
{
    return new Dynamics;
}

auto Dynamics::operator new(size_t size) noexcept -> void*
{
    void* result = nullptr;
    UserHeap* heap = ObjectTypeManager::objectCache;

    if (heap != nullptr && heap->heapSize != 0)
    {
        result = heap->malloc(static_cast<uint32_t>(size));
    }

    return result;
}

auto Dynamics::operator delete(void* ptr) -> void
{
    UserHeap* heap = ObjectTypeManager::objectCache;

    if (heap != nullptr && heap->heapSize != 0)
    {
        heap->free(ptr);
    }
}

auto Dynamics::destroy() -> void
{
}

auto Dynamics::init(DynamicsType* dynType, GameObject* object) -> int32_t
{
    type = dynType;
    me = object;
    return 0;
}

auto Dynamics::update() -> int32_t
{
    return 0;
}
