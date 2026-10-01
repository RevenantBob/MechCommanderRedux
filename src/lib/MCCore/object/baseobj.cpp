#include "stdafx.h"
#include "object/baseobj.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/heap.h"
#include "object/objtype.h"
#include "object/objwtch.h"

auto BaseObject::destroy() -> void
{
    // Port fix: the original calls through objectWatchers unchecked; objects can outlive it at shutdown.
    if (objectWatchers != nullptr)
    {
        objectWatchers->removeObject(this);
    }
}

auto BaseObject::getPositionFromHS(uint32_t) -> vector_3d
{
    vector_3d position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    return position;
}

auto BaseObject::operator new(size_t size) noexcept -> void*
{
    void* result = ObjectTypeManager::objectCache->malloc(static_cast<uint32_t>(size));

    if (result == nullptr)
    {
        Fatal(static_cast<int32_t>(0xeeeffeee), " Too many Objects in World! ");
    }

    return result;
}

auto BaseObject::operator delete(void* ptr) -> void
{
    ObjectTypeManager::objectCache->free(ptr);
}
