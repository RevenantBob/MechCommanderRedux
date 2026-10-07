#include "stdafx.h"
#include "object/gvehctrl.h"

auto MCGroundVehicleControlData::Init(int32_t) -> int32_t
{
    return 0;
}

auto MCGroundVehicleControlData::Destroy() -> void
{
}

auto MCGroundVehicleControlData::Reset() -> void
{
    ButtonState &= 0xfffffff8;
    TurretRotate = 0;
    Rotate = 0;
    Pivot = 0;
}

auto MCGroundVehicleControlData::Brake() -> int32_t
{
    Throttle = 0;
    return 0;
}
