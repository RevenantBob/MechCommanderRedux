#include "stdafx.h"
#include "object/baseobj.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "object/objtype.h"
#include "object/objwtch.h"

auto MCBaseObject::Destroy() -> void
{
    // Port fix: the original calls through objectWatchers unchecked; objects can outlive it at shutdown.
    if (ObjectWatchers != nullptr)
    {
        ObjectWatchers->RemoveObject(this);
    }
}

auto MCBaseObject::GetPositionFromHS(uint32_t) -> MCVector3D
{
    MCVector3D position;
    position.X = 0.0f;
    position.Y = 0.0f;
    position.Z = 0.0f;
    return position;
}
