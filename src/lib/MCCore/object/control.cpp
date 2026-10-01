#include "stdafx.h"
#include "object/control.h"
#include "lib/heap.h"
#include "object/objtype.h"

namespace
{
    /// <summary>Whether the object heap is up (the controls' allocations test it first).</summary>
    bool ObjectCacheReady()
    {
        return ObjectTypeManager::objectCache != nullptr && ObjectTypeManager::objectCache->heapSize != 0;
    }
}

auto ControlData::operator new(size_t size) noexcept -> void*
{
    void* result = nullptr;

    if (ObjectCacheReady())
    {
        result = ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));
    }

    return result;
}

auto ControlData::operator delete(void* ptr) -> void
{
    if (ObjectCacheReady())
    {
        ObjectTypeManager::objectCache->free(ptr);
    }
}

auto ControlData::destroy() -> void
{
}

auto ControlData::init(int32_t) -> int32_t
{
    return 0;
}

auto ControlData::reset() -> void
{
}

auto Control::operator new(size_t size) noexcept -> void*
{
    void* result = nullptr;

    if (ObjectCacheReady())
    {
        result = ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));
    }

    return result;
}

auto Control::operator delete(void* ptr) -> void
{
    if (ObjectCacheReady())
    {
        ObjectTypeManager::objectCache->free(ptr);
    }
}

auto Control::destroy() -> void
{
}

auto Control::init(GameObject* object, int32_t) -> int32_t
{
    me = object;
    return 0;
}

auto Control::update() -> int32_t
{
    return 0;
}
