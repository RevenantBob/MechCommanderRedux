#include "stdafx.h"
#include "object/baseobj.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
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
