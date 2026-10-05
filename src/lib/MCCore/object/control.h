#pragma once

class DynamicsType;
class GameObject;

/// <summary>
/// What a mover's controller asks of it this frame (throttle, turning, firing): the base of the per-kind control
/// data (<see cref="MechControlData"/>, <see cref="GroundVehicleControlData"/>, <see cref="ElementalControlData"/>).
/// </summary>
/// <remarks>Original source: <c>object\control.cpp</c>, <c>object\control.h</c>.</remarks>
class ControlData
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~ControlData() { destroy(); }

    /// <remarks>MCX.EXE @ 0x00659a40</remarks>
    virtual int32_t init(int32_t unused);
    /// <remarks>MCX.EXE @ 0x00659a30</remarks>
    virtual void destroy();
    /// <summary>Clears the requests for the next frame.</summary>
    /// <remarks>MCX.EXE @ 0x00659a50</remarks>
    virtual void reset();
    /// <remarks>MCX.EXE @ 0x0065c060 (inline in <c>object\control.h</c>)</remarks>
    virtual int32_t brake() { return 0; }
    /// <summary>1 for a mech's, 2 a ground vehicle's, 3 an elemental's.</summary>
    virtual uint32_t getControlDataClass() { return 0; }
};

/// <summary>
/// What drives a mover each frame: its AI, the player, or the network. The subclasses fill the mover's
/// <see cref="ControlData"/> in update.
/// </summary>
/// <remarks>Original source: <c>object\control.cpp</c>, <c>object\control.h</c>.</remarks>
class Control
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~Control() { destroy(); }

    /// <summary>Remembers the object it controls.</summary>
    /// <remarks>MCX.EXE @ 0x00659ac0</remarks>
    virtual int32_t init(GameObject* object, int32_t unused);
    /// <remarks>MCX.EXE @ 0x00659ab0</remarks>
    virtual void destroy();
    /// <remarks>MCX.EXE @ 0x00659ad0</remarks>
    virtual int32_t update();
    /// <summary>1 for player control, 2 AI, 3 network (the subclasses' values).</summary>
    /// <remarks>MCX.EXE @ 0x0065c070 (inline in <c>object\control.h</c>)</remarks>
    virtual uint32_t getControlClass() { return 0; }

    /// <summary>The object controlled.</summary>
    GameObject* me = nullptr; // +0x04
    /// <summary>The object's control data, filled by update.</summary>
    ControlData* controlData = nullptr; // +0x08
};
