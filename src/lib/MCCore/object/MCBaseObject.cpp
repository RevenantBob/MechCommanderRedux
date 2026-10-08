#include "stdafx.h"
#include "object/MCBaseObject.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectWatcher.h"

MCBaseObject::~MCBaseObject()
{
    // The watchers can be gone already when objects outlive the object system at shutdown.
    if (MCObjectWatcherList* watchers = ObjectWatchers())
    {
        watchers->RemoveObject(this);
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
