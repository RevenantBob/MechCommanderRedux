#include "stdafx.h"
#include "object/gvehctrl.h"

auto GroundVehicleControlData::init(int32_t) -> int32_t
{
    return 0;
}

auto GroundVehicleControlData::destroy() -> void
{
}

auto GroundVehicleControlData::reset() -> void
{
    unknown0C = -1;
    buttonState &= 0xfffffff8;
    turretRotate = 0;
    rotate = 0;
    pivot = 0;
}

auto GroundVehicleControlData::brake() -> int32_t
{
    throttle = 0;
    return 0;
}
