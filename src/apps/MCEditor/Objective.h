#pragma once

class DaoDatabase;
class EditorMap;
class EditorObject;
class ObjectiveRecord;
class Parameter;

// A mission objective as the editor keeps it: its kind, up to three values (the targets, the count, the area) and the
// settings of its Objectives row (0x0046a6b0-0x0046ba70; virtual table 0x00502250). Its source file name wasn't kept;
// the names are the port's.
//
/// <summary>Whether the save has warned of a guard objective without a time limit (0x008460bc).</summary>
extern bool guardWarningShown;

/// <summary>The objective kinds (each objective's <c>type</c>).</summary>
enum ObjectiveType : int32_t
{
    /// <summary>An objectives dialog's empty row ("None").</summary>
    OBJECTIVE_NONE = -1,
    OBJECTIVE_DESTROY_ALL = 0,
    OBJECTIVE_DESTROY_UNITS = 1,
    OBJECTIVE_DESTROY_BUILDINGS = 2,
    OBJECTIVE_GUARD_UNITS = 3,
    OBJECTIVE_GUARD_BUILDINGS = 4,
    OBJECTIVE_CAPTURE_UNITS = 5,
    OBJECTIVE_CAPTURE_BUILDINGS = 6,
    OBJECTIVE_MOVE_TO_AREA = 7,
    /// <summary>The number of kinds.</summary>
    OBJECTIVE_COUNT = 8
};

/// <summary>
/// Each kind's ObjectiveScripting action (0x0052e65c); "Destroy Enemy" is both 0 (all enemies) and 1 (a lance).
/// </summary>
extern const char* const OBJECTIVE_ACTIONS[OBJECTIVE_COUNT];

/// <summary>An objective: its kind and values, its name and its Objectives row's settings.</summary>
/// <remarks>0x28 bytes with the virtual table.</remarks>
class Objective
{
public:
    /// <summary>An objective of no kind (<see cref="OBJECTIVE_NONE"/>), unnamed, with no values.</summary>
    /// <remarks>
    /// 0x0046a6b0. EB-126 fixed: the original leaves the Primary box's byte unset (heap contents); the port makes it
    /// false.
    /// </remarks>
    Objective();
    /// <summary>
    /// An objective of kind <paramref name="type"/> with its unset values: a count of "All", three unit or three
    /// structure targets, or an area of radius 75; named after the kind (string 2000 + kind).
    /// </summary>
    /// <remarks>0x0046a880.</remarks>
    explicit Objective(int32_t type);
    /// <summary>A copy of <paramref name="other"/> with copies of its values.</summary>
    /// <remarks>0x0046a760.</remarks>
    Objective(const Objective& other);
    /// <summary>Replaces the values and settings with copies of <paramref name="other"/>'s.</summary>
    /// <remarks>0x0046a830.</remarks>
    Objective& operator=(const Objective& other);
    /// <summary>Deletes the values.</summary>
    /// <remarks>0x0046a6f0.</remarks>
    ~Objective();

    /// <summary>Puts <paramref name="parameter"/> in slot <paramref name="index"/>, deleting the one there.</summary>
    /// <remarks>0x0046a680.</remarks>
    void setParameter(int32_t index, Parameter* parameter);
    /// <summary>
    /// Reads the values from the ObjectiveScripting rows <paramref name="filter"/> selects in
    /// <paramref name="record"/>, one row per value; whether the objective counts after the last action comes from the
    /// first row's fourth and fifth values.
    /// </summary>
    /// <returns>Whether every row's value read.</returns>
    /// <remarks>0x0046b210.</remarks>
    bool loadParameters(const CString& filter, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map);
    /// <summary>
    /// Writes the objective's ObjectiveScripting rows: one per value that is set (<see cref="Parameter::writeScripting"/>,
    /// its action the kind's, ActionParm4 "After Last Action" when <paramref name="afterLast"/>), then, with a marker, a
    /// "Set Objective Marker" row at the first value's position. A guard objective in a mission without a time limit
    /// warns once per save (<see cref="guardWarningShown"/>).
    /// </summary>
    /// <param name="objectiveId">The objective's Objectives row (ObjectiveID).</param>
    /// <param name="number">The objective's number in the mission (Objective and ActionSequence).</param>
    /// <returns>false for an objective with no kind.</returns>
    /// <remarks>0x0046b360.</remarks>
    bool writeScripting(DaoDatabase& database, long missionId, long objectiveId, long number, EditorMap& map,
                        bool afterLast);
    /// <summary>Draws each value's marks into <paramref name="buffer"/>, the view from <paramref name="origin"/>.</summary>
    /// <returns>Whether every value drew.</returns>
    /// <remarks>0x0046b900.</remarks>
    bool draw(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height, int32_t zoom);
    /// <summary>Lets go of what each value put on <paramref name="map"/> (the objective is being deleted).</summary>
    /// <returns>Whether every value let go.</returns>
    /// <remarks>0x0046ba70, the objective's only virtual table slot.</remarks>
    bool release(EditorMap& map);
    /// <summary>Whether a value of the objective refers to <paramref name="object"/>.</summary>
    /// <remarks>0x0046b8c0.</remarks>
    bool referencesObject(const EditorObject& object, EditorMap& map);
    /// <summary>
    /// Moves every value that refers to <paramref name="object"/> to cell (<paramref name="col"/>,
    /// <paramref name="row"/>).
    /// </summary>
    /// <returns>Whether every one moved.</returns>
    /// <remarks>0x0046ba00.</remarks>
    bool moveReferences(const EditorObject& object, EditorMap& map, int32_t col, int32_t row);
    /// <summary>Draws each value's marks on <paramref name="dc"/> (<see cref="Parameter::drawMarks"/>).</summary>
    /// <param name="unused">Passed by the marks pass (false) and not used.</param>
    /// <returns>Whether every value drew.</returns>
    /// <remarks>0x0046b980.</remarks>
    bool drawMarks(EditorMap& map, const POINT& origin, HDC dc, int32_t width, int32_t height, bool unused,
                   int32_t zoom);

    /// <summary>The values (null where the kind has none).</summary>
    Parameter* parameters[3] = {};
    /// <summary>The kind (<see cref="ObjectiveType"/>).</summary>
    int32_t type;
    /// <summary>Whether the objective is checked after the last action ("After Last Action").</summary>
    int32_t afterLastAction = 0;
    /// <summary>Whether the objective has a "Set Objective Marker" row.</summary>
    bool hasMarker = false;
    /// <summary>The name: the kind's at first, then the row's Description.</summary>
    CString name;
    /// <summary>The side it belongs to (the row's Alignment).</summary>
    int32_t side = 2;
    /// <summary>The row's Primary box: Priority 0 when checked, else 1; constructed true.</summary>
    bool primary = true;
};

/// <summary>
/// The objective <paramref name="objectiveNumber"/> of mission <paramref name="missionId"/>, from its
/// ObjectiveScripting rows in <paramref name="database"/>: its kind from the first row's action, its values from its
/// rows.
/// </summary>
/// <returns>Null when it has no rows, an unknown action or values that don't read (after a message box).</returns>
/// <remarks>0x0046ac20.</remarks>
Objective* loadObjective(DaoDatabase& database, long missionId, long objectiveNumber, EditorMap& map);
