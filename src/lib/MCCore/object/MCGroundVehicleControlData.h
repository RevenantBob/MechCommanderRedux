#pragma once

#include "object/MCControl.h"

/// <summary>A ground vehicle's control requests for one frame: turret and turn rates, throttle, the button bits.</summary>
/// <remarks>Original source: <c>object\gvehctrl.cpp</c>, <c>object\gvehctrl.h</c>.</remarks>
class MCGroundVehicleControlData : public MCControlData
{
public:
    /// <summary>Clears the rates, button bits 0-2 and the pivot flag (the throttle and walk flag stay).</summary>
    void Reset() override
    {
        ButtonState &= 0xfffffff8;
        TurretRotate = 0;
        Rotate = 0;
        Pivot = 0;
    }

    /// <summary>Zeroes the throttle.</summary>
    int32_t Brake() override
    {
        Throttle = 0;
        return 0;
    }

    uint32_t GetControlDataClass() override { return 2; }

    /// <summary>Button bits; reset clears bits 0-2.</summary>
    uint32_t ButtonState = 0;
    /// <summary>Turret yaw rate request.</summary>
    int8_t TurretRotate = 0;
    /// <summary>Throttle; brake zeroes it (reset leaves it).</summary>
    int8_t Throttle = 0;
    /// <summary>Body yaw rate request.</summary>
    int8_t Rotate = 0;
    /// <summary>Nonzero while pivoting in place: GroundVehicleDynamics turns at maxVehiclePivotRate instead.
    /// Cleared by reset.</summary>
    int32_t Pivot = 0;
    /// <summary>Nonzero to move at gvWalkSpeed instead of the type's top speed. Not touched by reset.</summary>
    int32_t Walk = 0;
};
