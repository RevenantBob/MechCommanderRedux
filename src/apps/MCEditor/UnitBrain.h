#pragma once

#include <istream>
#include <ostream>
#include <vector>

#include "Parameter.h"

class EditorMap;
class EditorObject;

// A unit's orders: the warrior brain it runs in the game ("DredAttack01" and so on) and the values the brain reads
// from the mission's warrior FIT ([Warrior<n>Cell<i>] and [Warrior<n>Static<i>]). Each kind is a class with a 7-slot
// virtual table (the base at 0x004fe4c8, the code at 0x00439b50-0x0043f6e0). Their source file name wasn't kept; the
// names are the port's, after the brains. Slot 2, writing the orders to the warrior FIT, is in UnitBrainWrite.cpp.

/// <summary>The brain kinds (each order's <c>type</c>, the index into <see cref="BRAIN_NAMES"/>).</summary>
enum BrainType : int32_t
{
    BRAIN_ATTACK = 0,
    BRAIN_SENTRY = 1,
    BRAIN_GUARD = 2,
    BRAIN_GOTO = 3,
    BRAIN_CIRCLE = 4,
    BRAIN_PATROL = 5,
    BRAIN_DESTROY = 6,
    BRAIN_KILL = 7,
    BRAIN_AMBUSH = 8,
    BRAIN_ESCORT = 9,
    BRAIN_NULL = 10,
    /// <summary>The number of kinds.</summary>
    BRAIN_COUNT = 11
};

/// <summary>The brains' ABL module names (0x00527f4c), by <see cref="BrainType"/>.</summary>
extern const char* const BRAIN_NAMES[BRAIN_COUNT];

/// <summary>The engage radius a new unit's orders start with.</summary>
inline constexpr long DEFAULT_ENGAGE_RADIUS = 0x96;

/// <summary>
/// A unit's orders: the brain kind, its description, and the values every order has (the order dialog's engage
/// radius, move speed and tactic, brain memory cells 2, 9 and 28), with a table of all its values.
/// </summary>
/// <remarks>0x40 bytes with the virtual table (0x004fe4c8).</remarks>
class UnitBrain
{
public:
    /// <summary>Orders of kind <paramref name="type"/> with no description and default values.</summary>
    explicit UnitBrain(int32_t type);
    /// <summary>A copy of <paramref name="other"/>'s kind, description and common values.</summary>
    /// <remarks>0x00439b50; the derived classes build the table.</remarks>
    UnitBrain(const UnitBrain& other);
    UnitBrain& operator=(const UnitBrain&) = delete;
    virtual ~UnitBrain() = default;

    /// <summary>The brain's ABL module name.</summary>
    /// <remarks>Virtual table slot 1.</remarks>
    virtual const char* brainName() const;
    /// <summary>
    /// Writes the orders as warrior <paramref name="warrior"/>'s section of the warrior FIT <paramref name="stream"/>:
    /// its header, its brain cells and its static variables, finding the units and structures they name on
    /// <paramref name="map"/>.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>Virtual table slot 2 (UnitBrainWrite.cpp).</remarks>
    virtual bool write(std::ostream& stream, EditorMap& map, int32_t warrior) = 0;
    /// <summary>
    /// Reads the values of warrior <paramref name="warrior"/> from the warrior FIT <paramref name="stream"/>, finding
    /// the units and structures they name on <paramref name="map"/>.
    /// </summary>
    /// <returns>Whether every value was found.</returns>
    /// <remarks>Virtual table slot 3.</remarks>
    virtual bool read(std::istream& stream, EditorMap& map, int32_t warrior) = 0;
    /// <summary>The number of values in <see cref="table"/>.</summary>
    /// <remarks>Virtual table slot 4.</remarks>
    virtual int32_t parameterCount() const = 0;
    /// <summary>Value <paramref name="index"/> of the table.</summary>
    /// <remarks>Virtual table slot 5.</remarks>
    virtual Parameter* parameter(int32_t index);
    /// <summary>A new copy of the orders.</summary>
    /// <remarks>Virtual table slot 6.</remarks>
    virtual UnitBrain* clone() const = 0;

    /// <summary>
    /// Draws each value's marks (a path's flags) into <paramref name="buffer"/>, the view from
    /// <paramref name="origin"/>.
    /// </summary>
    /// <returns>Whether every value drew.</returns>
    /// <remarks>0x0043a730.</remarks>
    bool drawParameters(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
                        int32_t zoom);
    /// <summary>Whether a value of the orders refers to <paramref name="object"/>.</summary>
    /// <remarks>0x0043a670.</remarks>
    bool referencesObject(const EditorObject& object, EditorMap& map);
    /// <summary>
    /// Moves the first value that refers to <paramref name="object"/> to cell (<paramref name="col"/>,
    /// <paramref name="row"/>).
    /// </summary>
    /// <returns>Whether it moved; false when no value refers to the object.</returns>
    /// <remarks>0x0043a6c0. Only the first referring value moves.</remarks>
    bool moveReferences(const EditorObject& object, EditorMap& map, int32_t col, int32_t row);

    /// <summary>The kind (<see cref="BrainType"/>).</summary>
    int32_t type;
    /// <summary>The description the order dialog shows (a string resource).</summary>
    CString text;
    /// <summary>The values, in the order dialog's order; points into the members.</summary>
    std::vector<Parameter*> table;
    /// <summary>The engage radius (brain cell 2, written as a float).</summary>
    NumberParameter engageRadius;
    /// <summary>Whether the unit runs (brain cell 9): the order dialog's Move Speed, Walk or Run.</summary>
    BoolParameter run;
    /// <summary>The tactic (brain cell 28): the order dialog's Tactic, an index into its list.</summary>
    LongParameter tactic;

protected:
    /// <summary>Reads cells 1-3 into the common values.</summary>
    /// <returns>Whether all three were found.</returns>
    /// <remarks>The derived reads differ in how they combine the results; this is the attack and sentry one.</remarks>
    bool readCommon(std::istream& stream, int32_t warrior);
};

/// <summary>Attack: fights enemies in its engage radius (the default orders).</summary>
/// <remarks>0x40 bytes, virtual table 0x004fe5d4; constructor 0x0043b1f0.</remarks>
class AttackBrain : public UnitBrain
{
public:
    /// <summary>Attack orders of kind <paramref name="type"/> with engage radius <paramref name="radius"/>.</summary>
    AttackBrain(int32_t type, long radius);
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043b380.</remarks>
    AttackBrain(const AttackBrain& other);

    /// <summary>The common values.</summary>
    /// <remarks>0x0043b520.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043b400.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>3.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;
};

/// <summary>Sentry: fights enemies in the engage radius of its starting position.</summary>
/// <remarks>0x40 bytes, virtual table 0x004fe5b8; made inline by the factory.</remarks>
class SentryBrain : public UnitBrain
{
public:
    /// <summary>Sentry orders.</summary>
    SentryBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043b710.</remarks>
    SentryBrain(const SentryBrain& other);

    /// <summary>The common values.</summary>
    /// <remarks>0x0043ba30, the same code as the attack's.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043b910.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>3.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;
};

/// <summary>Ambush: stays hidden from sensors until engaged.</summary>
/// <remarks>0x40 bytes, virtual table 0x004fe59c; constructor 0x0043e630.</remarks>
class AmbushBrain : public AttackBrain
{
public:
    /// <summary>Ambush orders.</summary>
    AmbushBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043e7e0.</remarks>
    AmbushBrain(const AmbushBrain& other);

    /// <inheritdoc/>
    UnitBrain* clone() const override;
};

/// <summary>Guard: fights enemies in the engage radius of a structure.</summary>
/// <remarks>0x5c bytes, virtual table 0x004fe5f0; constructor 0x0043bc20.</remarks>
class GuardBrain : public UnitBrain
{
public:
    /// <summary>Guard orders with no structure.</summary>
    GuardBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043bf60.</remarks>
    GuardBrain(const GuardBrain& other);

    /// <summary>The common values, and the structure from static 1 (its structure id).</summary>
    /// <returns>Whether the structure id was found.</returns>
    /// <remarks>0x0043c2b0.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043c040.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>4.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;

    /// <summary>The structure to guard.</summary>
    TargetParameter structure;
};

/// <summary>Goto, circle and patrol: moves along a path after a delay.</summary>
/// <remarks>0x78 bytes, virtual table 0x004fe640; constructor 0x0043c470.</remarks>
class PathBrain : public UnitBrain
{
public:
    /// <summary>Path orders of kind <paramref name="type"/> (<see cref="BRAIN_GOTO"/> to <see cref="BRAIN_PATROL"/>).</summary>
    explicit PathBrain(int32_t type);
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043c9e0.</remarks>
    PathBrain(const PathBrain& other);

    /// <summary>The common values, move &amp; fire (cell 4), the path (static 1) and the delay (static 3).</summary>
    /// <remarks>0x0043ce10.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043cc10.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>6.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;

    /// <summary>Whether the unit keeps firing while it moves (brain cell 31).</summary>
    BoolParameter moveAndFire;
    /// <summary>The delay before it starts, in seconds.</summary>
    NumberParameter delay;
    /// <summary>The path.</summary>
    PathParameter path;
};

/// <summary>Destroy: moves to a structure after a delay and attacks it.</summary>
/// <remarks>0x7c bytes, virtual table 0x004fe690; constructor 0x0043d160.</remarks>
class DestroyBrain : public UnitBrain
{
public:
    /// <summary>Destroy orders with no structure.</summary>
    DestroyBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043d400.</remarks>
    DestroyBrain(const DestroyBrain& other);

    /// <summary>
    /// The common values, move &amp; fire (cell 4), the structure at the location of static 1 and the delay
    /// (static 3).
    /// </summary>
    /// <remarks>0x0043d8a0.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043d590.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>6.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;

    /// <summary>Whether the unit keeps firing while it moves (brain cell 31).</summary>
    BoolParameter moveAndFire;
    /// <summary>The delay before it starts, in seconds.</summary>
    NumberParameter delay;
    /// <summary>The structure to destroy.</summary>
    TargetParameter structure;
};

/// <summary>Kill: moves toward a unit after a delay and attacks it.</summary>
/// <remarks>0x7c bytes, virtual table 0x004fe6ac; constructor 0x0043dc80.</remarks>
class KillBrain : public UnitBrain
{
public:
    /// <summary>Kill orders of kind <paramref name="type"/> with no target.</summary>
    explicit KillBrain(int32_t type);
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043df20.</remarks>
    KillBrain(const KillBrain& other);

    /// <summary>
    /// The common values, move &amp; fire (cell 4), the unit at the location of static 4 and the delay (static 5).
    /// </summary>
    /// <remarks>0x0043e350.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043e0b0.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>6.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;

    /// <summary>Whether the unit keeps firing while it moves (brain cell 31).</summary>
    BoolParameter moveAndFire;
    /// <summary>The delay before it starts, in seconds.</summary>
    NumberParameter delay;
    /// <summary>The unit to kill.</summary>
    TargetParameter unit;
};

/// <summary>Escort: follows a unit, keeping to one side of it.</summary>
/// <remarks>0x7c bytes, virtual table 0x004fe6c8; constructor 0x0043e980.</remarks>
class EscortBrain : public UnitBrain
{
public:
    /// <summary>Escort orders with no unit.</summary>
    EscortBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043ed10.</remarks>
    EscortBrain(const EscortBrain& other);

    /// <summary>
    /// The common values, move &amp; fire (cell 4), the unit whose side, force group and role are statics 1-3 and the
    /// direction (static 4).
    /// </summary>
    /// <remarks>0x0043f110.</remarks>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043eeb0.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>6.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;

    /// <summary>Whether the unit keeps firing while it moves (brain cell 31).</summary>
    BoolParameter moveAndFire;
    /// <summary>The unit to follow.</summary>
    TargetParameter unit;
    /// <summary>The side of the followed unit to keep to.</summary>
    DirectionParameter direction;
};

/// <summary>Null: doesn't move or attack.</summary>
/// <remarks>0x40 bytes, virtual table 0x004fe580; constructor 0x0043f4b0. It has no table.</remarks>
class NullBrain : public UnitBrain
{
public:
    /// <summary>Null orders.</summary>
    NullBrain();
    /// <summary>A copy of <paramref name="other"/>.</summary>
    /// <remarks>0x0043f620.</remarks>
    NullBrain(const NullBrain& other);

    /// <summary>Nothing to read.</summary>
    /// <returns>true.</returns>
    bool read(std::istream& stream, EditorMap& map, int32_t warrior) override;
    /// <inheritdoc/>
    /// <remarks>0x0043f640.</remarks>
    bool write(std::ostream& stream, EditorMap& map, int32_t warrior) override;
    /// <returns>0.</returns>
    int32_t parameterCount() const override;
    /// <inheritdoc/>
    UnitBrain* clone() const override;
};

/// <summary>New orders of kind <paramref name="type"/>, their engage radius <see cref="DEFAULT_ENGAGE_RADIUS"/>.</summary>
/// <returns>Null for a kind out of range.</returns>
/// <remarks>0x0043a0c0.</remarks>
UnitBrain* createBrain(int32_t type);

/// <summary>
/// The orders of the brain named <paramref name="brainName"/> (any case), read for warrior <paramref name="warrior"/>
/// from <paramref name="stream"/>.
/// </summary>
/// <returns>Null for an unknown brain; the orders also when their values didn't all read.</returns>
/// <remarks>0x00439f90.</remarks>
UnitBrain* readBrain(const char* brainName, std::istream& stream, EditorMap& map, int32_t warrior);
