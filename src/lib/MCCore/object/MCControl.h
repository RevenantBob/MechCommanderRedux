#pragma once

class MCDynamicsType;
class MCGameObject;

/// <summary>
/// What a mover's controller asks of it this frame (throttle, turning, firing): the base of the per-kind control
/// data (<see cref="MCMechControlData"/>, <see cref="MCGroundVehicleControlData"/>, <see cref="MCElementalControlData"/>).
/// </summary>
/// <remarks>Original source: <c>object\control.cpp</c>, <c>object\control.h</c>.</remarks>
class MCControlData
{
public:
    virtual ~MCControlData() = default;

    /// <summary>Clears the requests for the next frame.</summary>
    virtual void Reset() {}
    virtual int32_t Brake() { return 0; }
    /// <summary>1 for a mech's, 2 a ground vehicle's, 3 an elemental's.</summary>
    virtual uint32_t GetControlDataClass() { return 0; }
};

/// <summary>
/// What drives a mover each frame: its AI, the player, or the network. The subclasses fill the mover's
/// <see cref="MCControlData"/> in update.
/// </summary>
/// <remarks>Original source: <c>object\control.cpp</c>, <c>object\control.h</c>.</remarks>
class MCControl
{
public:
    /// <summary>Controls <paramref name="object"/>.</summary>
    explicit MCControl(MCGameObject& object) : Me(&object) {}
    virtual ~MCControl() = default;
    MCControl(const MCControl&) = delete;
    MCControl& operator=(const MCControl&) = delete;

    /// <summary>Fills the control data for this frame and drives the object.</summary>
    virtual int32_t Update() { return 0; }
    /// <summary>1 for player control, 2 AI, 3 network (the subclasses' values).</summary>
    virtual uint32_t GetControlClass() { return 0; }

    /// <summary>The object controlled.</summary>
    MCGameObject* Me = nullptr;
    /// <summary>The object's control data, filled by update.</summary>
    std::unique_ptr<MCControlData> ControlData;
};
