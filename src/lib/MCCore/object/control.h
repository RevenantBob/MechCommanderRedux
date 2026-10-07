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
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~MCControlData() { Destroy(); }

    virtual int32_t Init(int32_t unused);
    virtual void Destroy();
    /// <summary>Clears the requests for the next frame.</summary>
    virtual void Reset();
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
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~MCControl() { Destroy(); }

    /// <summary>Remembers the object it controls.</summary>
    virtual int32_t Init(MCGameObject* object, int32_t unused);
    virtual void Destroy();
    virtual int32_t Update();
    /// <summary>1 for player control, 2 AI, 3 network (the subclasses' values).</summary>
    virtual uint32_t GetControlClass() { return 0; }

    /// <summary>The object controlled.</summary>
    MCGameObject* Me = nullptr;
    /// <summary>The object's control data, filled by update.</summary>
    MCControlData* ControlData = nullptr;
};
