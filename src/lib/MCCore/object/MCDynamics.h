#pragma once

class MCDynamics;
class MCFitIniFile;
class MCGameObject;

/// <summary>
/// A mover type's movement limits, read from its FIT (turn rates, top speed, acceleration); makes the
/// <see cref="MCDynamics"/> each mover of the type moves with.
/// </summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>.</remarks>
class MCDynamicsType
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~MCDynamicsType() { Destroy(); }

    /// <summary>Reads the type's dynamics block from its FIT; 0 on success, else the FitIniFile error.</summary>
    virtual int32_t Init(MCFitIniFile* objFile) { return 0; }
    virtual void Destroy();
    /// <summary>Makes a <see cref="MCDynamics"/> of this type (the subclasses make theirs).</summary>
    virtual MCDynamics* CreateInstance();
    /// <summary>1 for a mech's, 2 a ground vehicle's.</summary>
    virtual uint32_t GetDynamicsTypeClass() { return 0; }
};

/// <summary>Moves one mover each frame within its <see cref="MCDynamicsType"/>'s limits, from its control data.</summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>; 0xc bytes.</remarks>
class MCDynamics
{
public:
    /// <summary>The original's destructor is not virtual: it resets the vtable and calls destroy.</summary>
    ~MCDynamics() { Destroy(); }

    /// <summary>Remembers its type and the object it moves.</summary>
    virtual int32_t Init(MCDynamicsType* dynType, MCGameObject* object);
    virtual void Destroy();
    virtual int32_t Update();
    virtual int32_t Brake() { return 0; }
    /// <summary>1 for a mech's, 2 a ground vehicle's, 3 an elemental's.</summary>
    virtual uint32_t GetDynamicsClass() { return 0; }
    virtual float GetVelocity() { return 0.0f; }

    /// <summary>The type whose limits apply.</summary>
    MCDynamicsType* Type = nullptr;
    /// <summary>The object moved.</summary>
    MCGameObject* Me = nullptr;
};
