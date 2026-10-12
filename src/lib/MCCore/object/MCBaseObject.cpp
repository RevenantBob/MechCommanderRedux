#include "stdafx.h"
#include "object/MCBaseObject.h"

auto MCBaseObject::GetPositionFromHS(uint32_t) -> MCVector3D
{
    MCVector3D position;
    position.X = 0.0f;
    position.Y = 0.0f;
    position.Z = 0.0f;
    return position;
}
