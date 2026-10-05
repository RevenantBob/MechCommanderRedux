#pragma once

#include "object/control.h"

/// <summary>A ground vehicle's control requests for one frame: turret and turn rates, throttle, the button bits.</summary>
/// <remarks>Original source: <c>object\gvehctrl.cpp</c>, <c>object\gvehctrl.h</c>; 0x18 bytes.</remarks>
class GroundVehicleControlData : public ControlData
{
public:
    /// <remarks>MCX.EXE @ 0x00668c20</remarks>
    int32_t init(int32_t unused) override;
    /// <remarks>MCX.EXE @ 0x00668c30</remarks>
    void destroy() override;
    /// <summary>Clears the rates, button bits 0-2 and the pivot flag; +0x0c back to -1.</summary>
    /// <remarks>MCX.EXE @ 0x00668c40</remarks>
    void reset() override;
    /// <summary>Zeroes the throttle.</summary>
    /// <remarks>MCX.EXE @ 0x00668c60</remarks>
    int32_t brake() override;
    /// <remarks>MCX.EXE @ 0x0066ab50 (inline in <c>object\gvehctrl.h</c>)</remarks>
    uint32_t getControlDataClass() override { return 2; }

    /// <summary>Button bits; reset clears bits 0-2.</summary>
    uint32_t buttonState = 0; // +0x04
    /// <summary>Turret yaw rate request.</summary>
    int8_t turretRotate = 0; // +0x08
    /// <summary>Throttle; brake zeroes it (reset leaves it).</summary>
    int8_t throttle = 0; // +0x09
    /// <summary>Body yaw rate request.</summary>
    int8_t rotate = 0; // +0x0a
    /// <summary>Nonzero while pivoting in place: GroundVehicleDynamics turns at maxVehiclePivotRate instead.
    /// Cleared by reset.</summary>
    int32_t pivot = 0; // +0x10
    /// <summary>Nonzero to move at gvWalkSpeed instead of the type's top speed. Not touched by reset.</summary>
    int32_t walk = 0; // +0x14
};
