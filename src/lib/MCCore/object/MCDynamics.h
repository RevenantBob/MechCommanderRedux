#pragma once

class MCDynamics;
class MCGameObject;

/// <summary>
/// A mover type's movement limits, read from its FIT (turn rates, top speed, acceleration); makes the
/// <see cref="MCDynamics"/> each mover of the type moves with.
/// </summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>.</remarks>
class MCDynamicsType
{
public:
    virtual ~MCDynamicsType() = default;

    /// <summary>Makes the dynamics that moves <paramref name="object"/> within this type's limits.</summary>
    virtual std::unique_ptr<MCDynamics> CreateInstance(MCGameObject& object);
    /// <summary>1 for a mech's, 2 a ground vehicle's.</summary>
    virtual uint32_t GetDynamicsTypeClass() { return 0; }
};

/// <summary>Moves one mover each frame within its <see cref="MCDynamicsType"/>'s limits, from its control data.</summary>
/// <remarks>Original source: <c>object\dyn.cpp</c>, <c>object\dyn.h</c>.</remarks>
class MCDynamics
{
public:
    /// <summary>Moves <paramref name="object"/> within the limits of <paramref name="type"/>.</summary>
    MCDynamics(MCDynamicsType& type, MCGameObject& object) : Type(&type), Me(&object) {}
    virtual ~MCDynamics() = default;

    virtual int32_t Update() { return 0; }
    virtual int32_t Brake() { return 0; }
    /// <summary>1 for a mech's, 2 a ground vehicle's, 3 an elemental's.</summary>
    virtual uint32_t GetDynamicsClass() { return 0; }
    virtual float GetVelocity() { return 0.0f; }

    /// <summary>The type whose limits apply.</summary>
    MCDynamicsType* Type = nullptr;
    /// <summary>The object moved.</summary>
    MCGameObject* Me = nullptr;
};
