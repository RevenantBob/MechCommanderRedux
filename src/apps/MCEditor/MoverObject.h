#pragma once

#include <list>

#include "EditorObject.h"

class DaoDatabase;
class UnitBrain;

// The module of the placed mechs and vehicles (0x00477950-0x00478cb0: the classes with the virtual tables 0x00502d18,
// 0x00502d30 and 0x00502d48, and the tables they colour and size themselves by). Its source file name wasn't kept;
// the names are the port's.

/// <summary>The number of paint schemes a mech or vehicle can take.</summary>
inline constexpr int32_t PAINT_SCHEME_COUNT = 19;

/// <summary>
/// The paint schemes' names as the pilots' names carry them ("R/W" in "Clan Regular (R/W)"); "T/G" is there twice.
/// </summary>
extern const char* const PaintSchemeNames[PAINT_SCHEME_COUNT];

/// <summary>
/// Each paint scheme's row of the fade table: the PaintScheme of the last pilot (NewMW) whose name holds the scheme's
/// name, 0 when none does.
/// </summary>
extern long PaintSchemes[PAINT_SCHEME_COUNT];

/// <summary>Each mech's tonnage (Mechs' CurTonnage) by its id (Mechs' ID: the object type's profile + 100).</summary>
/// <remarks>Filled once; a mech's weight class (1-4) comes from it.</remarks>
extern std::list<std::pair<long, long>> MechTonnages;

/// <summary>
/// Reads <see cref="PaintSchemes"/> from the pilots and, the first time, <see cref="MechTonnages"/> from the mechs of
/// <paramref name="database"/>.
/// </summary>
/// <remarks>
/// 0x00477cc0, called by <c>EditorPalette::setFadeTable</c>. A database error while reading the pilots skips that
/// scheme; one opening the mechs is shown in a message box. Opening the pilots must not fail (the error goes up).
/// </remarks>
void loadMoverTables(DaoDatabase& database);

/// <summary>
/// A placed mech or vehicle: its look, its orders, its place in its side's forces, its pilot's rank and the paint
/// scheme it is drawn in.
/// </summary>
/// <remarks>
/// 0x30 bytes, virtual table 0x00502d18 (clone pure). <see cref="EditorObject::side"/> is its side (1 Clan, 2 the
/// other).
/// </remarks>
class MoverObject : public EditorObject
{
public:
    /// <summary>
    /// The mover <paramref name="id"/> on cell (<paramref name="cellCol"/>, <paramref name="cellRow"/>) of side
    /// <paramref name="side"/>, with attack orders, paint scheme 0 and no shape slot yet.
    /// </summary>
    /// <remarks>0x00477a20.</remarks>
    MoverObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side);
    /// <summary>A copy of <paramref name="other"/> with a copy of its orders.</summary>
    /// <remarks>0x00477bf0.</remarks>
    MoverObject(const MoverObject& other);
    MoverObject& operator=(const MoverObject&) = delete;
    /// <summary>Deletes the orders.</summary>
    /// <remarks>0x00477c70.</remarks>
    ~MoverObject() override;

    /// <summary>
    /// Draws the slot's shape in the paint scheme's colours (the fade table's row for it), then its delta (but for the
    /// icon) as it is.
    /// </summary>
    /// <remarks>0x00477ac0.</remarks>
    void draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size) override;

    /// <summary>
    /// Takes the rank and the paint scheme of pilot <paramref name="pilotId"/> (NewMW's MWID): the first scheme whose
    /// name the pilot's name holds.
    /// </summary>
    /// <remarks>0x00478a80. The pilot isn't checked for: a missing one gives the current record's values.</remarks>
    void setPilot(DaoDatabase& database, long pilotId);
    /// <summary>
    /// The MWID of the first pilot of <see cref="rank"/> whose name holds the paint scheme's name; 0, after a message
    /// box, when there is none.
    /// </summary>
    /// <remarks>0x004786c0.</remarks>
    long findPilotId(DaoDatabase& database);
    /// <summary>Replaces the orders with new attack orders.</summary>
    /// <remarks>0x00478c10.</remarks>
    void resetBrain();

    /// <summary>The orders.</summary>
    UnitBrain* brain = nullptr;
    /// <summary>The force group (MissionAssignments' ForceGroup less one).</summary>
    int32_t forceGroup = 0;
    /// <summary>The role in the force group.</summary>
    int32_t role = 0;
    /// <summary>The pilot's rank (string IDS_RANK_VEHICLE_CREW + rank names it): a mech starts at its weight class.</summary>
    int32_t rank = 0;
    /// <summary>The paint scheme (an index into <see cref="PaintSchemeNames"/>).</summary>
    int32_t paintScheme = 0;

protected:
    /// <summary>
    /// Draws <paramref name="slot"/>'s shape of <paramref name="size"/> remapped through the paint scheme's fade row,
    /// unpacking it first if needed, then its delta unchanged (but for the icon).
    /// </summary>
    void drawSlot(ShapeSlot& slot, uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y,
                  int32_t size) const;
};

/// <summary>A placed vehicle: the mover and, for a vehicle with destroyed looks, its turret drawn over it.</summary>
/// <remarks>0x38 bytes, virtual table 0x00502d30.</remarks>
class VehicleObject : public MoverObject
{
public:
    /// <summary>The vehicle <paramref name="id"/> (its shape slot from the library) at rank 0.</summary>
    /// <remarks>0x00478040.</remarks>
    VehicleObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side);

    /// <summary>
    /// Draws the vehicle, then its second slot (the id's destroyed look, found on first use) over it; a vehicle with
    /// no destroyed looks stops drawing the second slot.
    /// </summary>
    /// <remarks>0x00478230.</remarks>
    void draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size) override;
    /// <summary>A copy, its second slot looked up again.</summary>
    /// <remarks>0x00478100.</remarks>
    EditorObject* clone() override;

    /// <summary>The second slot drawn over the vehicle, once found.</summary>
    ShapeSlot* topSlot = nullptr;
    /// <summary>Whether the second slot is drawn.</summary>
    bool drawTop = true;
};

/// <summary>A placed mech: its variant, and a pilot rank from its weight class.</summary>
/// <remarks>0x34 bytes, virtual table 0x00502d48.</remarks>
class MechObject : public MoverObject
{
public:
    /// <summary>
    /// The mech <paramref name="id"/> (its shape slot from the library), variant <paramref name="variant"/>, its rank
    /// its weight class: 1, 2 over 39 tons, 3 over 55, 4 over 75 (the tonnage from <see cref="MechTonnages"/>).
    /// </summary>
    /// <remarks>0x00478480.</remarks>
    MechObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side, int32_t variant);

    /// <inheritdoc/>
    /// <remarks>0x004785b0.</remarks>
    EditorObject* clone() override;

    /// <summary>The variant (0-2, from the fourth digit of the mech's id: 1, 4 or 3).</summary>
    int32_t variant;
};
