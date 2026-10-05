#pragma once

class Dynamics;
class FitIniFile;
class GameObject;

/// <summary>
/// A mover type's movement limits, read from its FIT (turn rates, top speed, acceleration); makes the
/// <see cref="Dynamics"/> each mover of the type moves with.
/// </summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>. Allocated from
/// <c>ObjectTypeManager::objectTypeCache</c>.</remarks>
class DynamicsType
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~DynamicsType() { destroy(); }

    /// <summary>Reads the type's dynamics block from its FIT; 0 on success, else the FitIniFile error.</summary>
    virtual int32_t init(FitIniFile* objFile) { return 0; }
    /// <remarks>MCX.EXE @ 0x0065a3e0</remarks>
    virtual void destroy();
    /// <summary>Makes a <see cref="Dynamics"/> of this type (the subclasses make theirs).</summary>
    /// <remarks>MCX.EXE @ 0x0065a3f0</remarks>
    virtual Dynamics* createInstance();
    /// <summary>1 for a mech's, 2 a ground vehicle's.</summary>
    /// <remarks>MCX.EXE @ 0x0065a970 (inline in <c>object\dyn.h</c>)</remarks>
    virtual uint32_t getDynamicsTypeClass() { return 0; }
};

/// <summary>Moves one mover each frame within its <see cref="DynamicsType"/>'s limits, from its control data.</summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>; 0xc bytes.</remarks>
class Dynamics
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~Dynamics() { destroy(); }

    /// <summary>Remembers its type and the object it moves.</summary>
    /// <remarks>MCX.EXE @ 0x0065a4b0</remarks>
    virtual int32_t init(DynamicsType* dynType, GameObject* object);
    /// <remarks>MCX.EXE @ 0x0065a4a0</remarks>
    virtual void destroy();
    /// <remarks>MCX.EXE @ 0x0065a4d0</remarks>
    virtual int32_t update();
    /// <remarks>MCX.EXE @ 0x0065a410 (inline in <c>object\dyn.h</c>)</remarks>
    virtual int32_t brake() { return 0; }
    /// <summary>1 for a mech's, 2 a ground vehicle's, 3 an elemental's.</summary>
    /// <remarks>MCX.EXE @ 0x0065a420 (inline in <c>object\dyn.h</c>)</remarks>
    virtual uint32_t getDynamicsClass() { return 0; }
    /// <remarks>MCX.EXE @ 0x0065a430 (inline in <c>object\dyn.h</c>)</remarks>
    virtual float getVelocity() { return 0.0f; }

    /// <summary>The type whose limits apply.</summary>
    DynamicsType* type = nullptr; // +0x04
    /// <summary>The object moved.</summary>
    GameObject* me = nullptr; // +0x08
};
