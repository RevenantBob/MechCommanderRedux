#pragma once

#include <list>

class DaoDatabase;
class EditorMap;
class EditorObject;
class ObjectiveRecord;

// The values an order (UnitBrain) or an objective is set up with: a number, a target, a location, a path and so on.
// Each kind is a class with a 13-slot virtual table (the base at 0x004fd990, methods at 0x0042e5a0-0x0042e720, the
// kinds' code at 0x00439c60-0x00439ee0, 0x0043bd60-0x0043be30, 0x0043c600-0x0043c7d0, 0x0043eaf0-0x0043ebc0,
// 0x0046ab30-0x0046ac10 and 0x0046bd30-0x0046dc20). Their source file name wasn't kept; the names are the port's.

/// <summary>The script kinds of an objective's rows (<see cref="Parameter::writeScripting"/>'s kind).</summary>
inline constexpr int32_t SCRIPT_CAPTURE = 4;
inline constexpr int32_t SCRIPT_GUARD = 5;

/// <summary>The box around an objective's target: colour, arm length and margin.</summary>
inline constexpr COLORREF TARGET_BOX_COLOR = RGB(0xfe, 0x9a, 0x23);
inline constexpr int32_t TARGET_BOX_SIZE = 0x1e;
inline constexpr int32_t TARGET_BOX_MARGIN = 10;

/// <summary>The number value meaning "All" (0x00502244), and an unset number.</summary>
inline constexpr long PARAMETER_ALL = 0x7fffffff;

/// <summary>The parameter kinds (each class's <c>type</c>).</summary>
enum ParameterType : int32_t
{
    /// <summary>A structure target (<see cref="TargetParameter"/>).</summary>
    PARAMETER_STRUCTURE = 0,
    /// <summary>A unit target (<see cref="TargetParameter"/>).</summary>
    PARAMETER_UNIT = 1,
    /// <summary>An area (<see cref="AreaParameter"/>).</summary>
    PARAMETER_AREA = 2,
    /// <summary>A number (<see cref="NumberParameter"/>).</summary>
    PARAMETER_NUMBER = 3,
    /// <summary>A location (<see cref="LocationParameter"/>).</summary>
    PARAMETER_LOCATION = 6,
    /// <summary>A path (<see cref="PathParameter"/>).</summary>
    PARAMETER_PATH = 7,
    /// <summary>A yes/no value (<see cref="BoolParameter"/>).</summary>
    PARAMETER_BOOL = 8,
    /// <summary>A whole number (<see cref="LongParameter"/>).</summary>
    PARAMETER_LONG = 9,
    /// <summary>A direction (<see cref="DirectionParameter"/>).</summary>
    PARAMETER_DIRECTION = 10,
};

/// <summary>A value of an order or objective: its kind and the name the editor shows for it.</summary>
/// <remarks>0xc bytes with the virtual table (0x004fd990).</remarks>
class Parameter
{
public:
    /// <summary>A parameter of kind <paramref name="type"/> with no name.</summary>
    explicit Parameter(int32_t type);
    virtual ~Parameter() = default;

    /// <summary>A new parameter with the same values.</summary>
    /// <remarks>Virtual table slot 1.</remarks>
    virtual Parameter* clone() const = 0;
    /// <summary>Whether <paramref name="other"/> is of the same kind with the same values.</summary>
    /// <remarks>Virtual table slot 2.</remarks>
    virtual bool equals(const Parameter& other) const = 0;
    /// <summary>
    /// Writes the value as the objective's <paramref name="index"/>th ObjectiveScripting row: completes
    /// <paramref name="record"/> (the objective's row so far) and adds a copy of it, with the rows the value implies
    /// (a lookout point, a captureable structure or unit).
    /// </summary>
    /// <param name="kind">The objective's script kind: <c>SCRIPT_CAPTURE</c>, <c>SCRIPT_GUARD</c> or 0.</param>
    /// <param name="sideSign">-1 for the player's objective (side 1), 1 for the other side's.</param>
    /// <returns>Whether it wrote; false here (orders' values aren't written as rows).</returns>
    /// <remarks>Virtual table slot 3 (ParameterWrite.cpp).</remarks>
    virtual bool writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map,
                                int32_t kind, int32_t sideSign);
    /// <summary>
    /// Reads the value from <paramref name="record"/>, an objective's <paramref name="index"/>th script row (finding
    /// a target on <paramref name="map"/> or in <paramref name="database"/>).
    /// </summary>
    /// <returns>Whether it was read.</returns>
    /// <remarks>Virtual table slot 4.</remarks>
    virtual bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) = 0;
    /// <summary>The map position of the value, for those that have one.</summary>
    /// <returns>Whether it has one.</returns>
    /// <remarks>Virtual table slot 5.</remarks>
    virtual bool getPosition(int32_t* x, int32_t* y, EditorMap& map) = 0;
    /// <summary>Whether the value has been given.</summary>
    /// <remarks>Virtual table slot 6.</remarks>
    virtual bool isSet() const = 0;
    /// <summary>Whether the value refers to <paramref name="object"/> (its target or its cell).</summary>
    /// <remarks>Virtual table slot 7; false here.</remarks>
    virtual bool referencesObject(const EditorObject& object, EditorMap& map);
    /// <summary>
    /// Follows <paramref name="object"/> to cell (<paramref name="col"/>, <paramref name="row"/>), where it is being
    /// moved.
    /// </summary>
    /// <returns>Whether the value moved.</returns>
    /// <remarks>Virtual table slot 8; false here.</remarks>
    virtual bool moveReference(const EditorObject& object, EditorMap& map, int32_t col, int32_t row);
    /// <summary>Lets go of what the value put on <paramref name="map"/> (its objective is being deleted).</summary>
    /// <returns>true.</returns>
    /// <remarks>Virtual table slot 11 (0x0042e5e0); nothing here.</remarks>
    virtual bool release(EditorMap& map);
    /// <summary>
    /// Draws the value's marks on <paramref name="dc"/> (<paramref name="width"/> x <paramref name="height"/>, the view
    /// from <paramref name="origin"/>).
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>Virtual table slot 9; only targets draw anything.</remarks>
    virtual bool drawMarks(EditorMap& map, const POINT& origin, HDC dc, int32_t width, int32_t height, int32_t zoom);
    /// <summary>
    /// Draws the value's marks into <paramref name="buffer"/> (<paramref name="width"/> x <paramref name="height"/>,
    /// the view from <paramref name="origin"/>).
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>Virtual table slot 10; only paths draw anything.</remarks>
    virtual bool draw(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
                      int32_t zoom);
    /// <summary>Clears the value.</summary>
    /// <remarks>Virtual table slot 12; nothing for most kinds.</remarks>
    virtual void reset();

    /// <summary>The kind (<see cref="ParameterType"/>).</summary>
    int32_t type;
    /// <summary>The name the editor shows (a string resource), empty for most.</summary>
    CString name;
};

/// <summary>A map position (world units, y up), unset at (-1, -1).</summary>
/// <remarks>0x14 bytes, virtual table 0x004fd9c4; the points of a path are these.</remarks>
class LocationParameter : public Parameter
{
public:
    /// <summary>An unset location of kind <paramref name="type"/>.</summary>
    explicit LocationParameter(int32_t type = PARAMETER_LOCATION);

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <inheritdoc/>
    bool equals(const Parameter& other) const override;
    /// <summary>Nothing to write.</summary>
    /// <returns>true.</returns>
    bool writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map, int32_t kind,
                        int32_t sideSign) override;
    /// <summary>Nothing to read.</summary>
    /// <returns>true.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>The position.</summary>
    /// <returns>true.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <summary>Whether neither coordinate is -1.</summary>
    bool isSet() const override;
    /// <summary>Moves the position to the centre of cell (<paramref name="col"/>, <paramref name="row"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046cdb0.</remarks>
    bool moveReference(const EditorObject& object, EditorMap& map, int32_t col, int32_t row) override;
    /// <summary>Unsets the position.</summary>
    void reset() override;

    /// <summary>The map x.</summary>
    int32_t x = -1;
    /// <summary>The map y.</summary>
    int32_t y = -1;
};

/// <summary>An area: a centre (as <see cref="LocationParameter"/>), a radius and whether it's an extraction zone.</summary>
/// <remarks>0x1c bytes, virtual table 0x00502254 (an objective's "Move to Area").</remarks>
class AreaParameter : public LocationParameter
{
public:
    /// <summary>An unset area of radius 75.</summary>
    AreaParameter();

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is an area with the same name, centre, radius and zone flag.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>
    /// The centre and radius as ActionParm1-3 (an extraction zone's action "Extraction Zone"), ActionParm4
    /// "Anytime" unless "After Last Action", ActionParm5 "ALL"; then a lookout point at the position (radius
    /// 10000, reveal radius 75) for <paramref name="sideSign"/>.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046d060.</remarks>
    bool writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map, int32_t kind,
                        int32_t sideSign) override;
    /// <summary>The centre and radius from the row's first three values; an extraction zone when its action says so.</summary>
    /// <returns>true.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>Whether the centre lies on <paramref name="object"/>'s cell.</summary>
    /// <remarks>0x0046ce70.</remarks>
    bool referencesObject(const EditorObject& object, EditorMap& map) override;
    /// <summary>Removes the object on the centre's cell (the area's marker) unless an objective refers to it.</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046cec0.</remarks>
    bool release(EditorMap& map) override;

    /// <summary>The radius in world units.</summary>
    int32_t radius = 0x4b;
    /// <summary>Whether the area is an extraction zone.</summary>
    bool extractionZone = false;
};

/// <summary>A number, or <see cref="PARAMETER_ALL"/>.</summary>
/// <remarks>0x10 bytes, virtual table 0x004fe54c: an order's engage radius and delay, an objective's count.</remarks>
class NumberParameter : public Parameter
{
public:
    /// <summary>A number parameter worth <paramref name="value"/>.</summary>
    explicit NumberParameter(long value = PARAMETER_ALL);

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a number with the same value (the name isn't compared).</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>
    /// The count: ActionParm1 "All", or "Any" with the number as ActionParm3; the other parameters empty.
    /// </summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046d4e0.</remarks>
    bool writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map, int32_t kind,
                        int32_t sideSign) override;
    /// <summary>"All" when the row's first value is "All", else the row's third value.</summary>
    /// <returns>true.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>None.</summary>
    /// <returns>false.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <returns>true.</returns>
    bool isSet() const override;

    /// <summary>The value.</summary>
    long value;
};

/// <summary>A yes/no value.</summary>
/// <remarks>0x10 bytes, virtual table 0x004fe518.</remarks>
class BoolParameter : public Parameter
{
public:
    /// <summary>A false value.</summary>
    BoolParameter();

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a yes/no value with the same name and value.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>Not read from objectives.</summary>
    /// <returns>false.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>None.</summary>
    /// <returns>false.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <returns>true.</returns>
    bool isSet() const override;

    /// <summary>The value.</summary>
    bool value = false;
};

/// <summary>A whole number.</summary>
/// <remarks>0x10 bytes, virtual table 0x004fe4e4.</remarks>
class LongParameter : public Parameter
{
public:
    /// <summary>A zero value.</summary>
    LongParameter();

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a whole number with the same name and value.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>Not read from objectives.</summary>
    /// <returns>false.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>None.</summary>
    /// <returns>false.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <returns>true.</returns>
    bool isSet() const override;

    /// <summary>The value.</summary>
    long value = 0;
};

/// <summary>
/// A target: a unit (<see cref="PARAMETER_UNIT"/>) or a structure (<see cref="PARAMETER_STRUCTURE"/>), by its cell and
/// object id; unset at -1.
/// </summary>
/// <remarks>0x1c bytes, virtual table 0x004fe60c.</remarks>
class TargetParameter : public Parameter
{
public:
    /// <summary>An unset target of kind <paramref name="type"/>, a structure when <paramref name="structure"/>.</summary>
    TargetParameter(int32_t type, bool structure);

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a target (either kind) on the same cell with the same id.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>
    /// A structure: its block, vertex and number (ActionParm1, 2, 4; ActionParm3 "NO"), marked captureable for a capture
    /// objective. A unit (the first target only): its force group as a "Lance/Star" and the unit's role (guard) or
    /// the group's size, its units marked captureable for a capture objective. A guard objective adds a lookout
    /// point at the target.
    /// </summary>
    /// <returns>false.</returns>
    /// <remarks>0x0046c2c0.</remarks>
    bool writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map, int32_t kind,
                        int32_t sideSign) override;
    /// <summary>
    /// Finds the target the row names. A unit: the <paramref name="index"/>th role of the row's force group of the
    /// objective's side in MissionAssignments, at the mover on its cell; a structure: the row's numbered object of its
    /// map block and vertex.
    /// </summary>
    /// <returns>Whether it was found.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>The map position of the target's cell.</summary>
    /// <returns>Whether the target is set.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <summary>Whether the id is set.</summary>
    bool isSet() const override;
    /// <summary>Unsets the target.</summary>
    void reset() override;
    /// <summary>
    /// Whether the target is <paramref name="object"/>: on its cell, and the object found there (or the mover one cell
    /// down-right) is the same as it.
    /// </summary>
    /// <remarks>0x0046cbf0.</remarks>
    bool referencesObject(const EditorObject& object, EditorMap& map) override;
    /// <summary>Moves the target's cell to (<paramref name="col"/>, <paramref name="row"/>).</summary>
    /// <returns>true.</returns>
    /// <remarks>0x0046ccc0.</remarks>
    bool moveReference(const EditorObject& object, EditorMap& map, int32_t col, int32_t row) override;
    /// <summary>Draws a box around the object on the target's cell, if there is one.</summary>
    /// <remarks>0x0046da90.</remarks>
    bool drawMarks(EditorMap& map, const POINT& origin, HDC dc, int32_t width, int32_t height, int32_t zoom) override;

    /// <summary>The cell column of the target.</summary>
    int32_t cellCol = -1;
    /// <summary>The cell row of the target.</summary>
    int32_t cellRow = -1;
    /// <summary>The target's object id.</summary>
    int32_t objectId = -1;
    /// <summary>Whether the target is a structure.</summary>
    bool structure;
};

/// <summary>A path: the points a unit moves along.</summary>
/// <remarks>0x18 bytes, virtual table 0x004fe65c.</remarks>
class PathParameter : public Parameter
{
public:
    /// <summary>An empty path.</summary>
    PathParameter();

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a path through the same points.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>Not read from objectives.</summary>
    /// <returns>false.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>None.</summary>
    /// <returns>false.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <summary>Whether the path has points.</summary>
    bool isSet() const override;
    /// <summary>
    /// Draws a flag (bitmap 202, its magenta transparent) centred on each point's cell that lies inside the bitmap.
    /// </summary>
    bool draw(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
              int32_t zoom) override;
    /// <summary>Removes the points.</summary>
    void reset() override;

    /// <summary>The points, in order.</summary>
    std::list<LocationParameter> points;
};

/// <summary>A direction (an escort's side of the escorted unit).</summary>
/// <remarks>0x10 bytes, virtual table 0x004fe6e4.</remarks>
class DirectionParameter : public Parameter
{
public:
    /// <summary>Direction 0.</summary>
    DirectionParameter();

    /// <inheritdoc/>
    Parameter* clone() const override;
    /// <summary>Whether <paramref name="other"/> is a direction with the same name and value.</summary>
    bool equals(const Parameter& other) const override;
    /// <summary>Not read from objectives.</summary>
    /// <returns>false.</returns>
    bool load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index) override;
    /// <summary>None.</summary>
    /// <returns>false.</returns>
    bool getPosition(int32_t* x, int32_t* y, EditorMap& map) override;
    /// <returns>true.</returns>
    bool isSet() const override;

    /// <summary>The direction.</summary>
    long value = 0;
};
