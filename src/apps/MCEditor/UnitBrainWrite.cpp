#include "stdafx.h"
#include "UnitBrain.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "MoverObject.h"

// The orders' writers (virtual table slot 2, 0x0043b400-0x0043f640) and the warrior FIT helpers they share
// (0x0043a7b0-0x0043aa60): each unit's warrior section in the mission's warrior FIT
// (Data\Missions\Warriors\<mission>Params.fit), which <c>read</c> reads back.

namespace
{
    /// <summary>The first structure id (0x1000); an id counts up by 8 per tile and 1 per object on the tile.</summary>
    inline constexpr long STRUCTURE_ID_BASE = 0x1000;
    /// <summary>Structure ids per tile, and tiles per map block.</summary>
    inline constexpr long STRUCTURE_IDS_PER_TILE = 8;
    inline constexpr long BLOCK_VERTICES = 400;
    /// <summary>The kinds of static variable: a long, a float, a list of floats.</summary>
    inline constexpr int32_t STATIC_LONG = 0;
    inline constexpr int32_t STATIC_FLOATS = 3;
    /// <summary>A brain cell's memory type: a long or a float.</summary>
    inline constexpr int32_t MEMORY_LONG = 0;
    inline constexpr int32_t MEMORY_FLOAT = 1;
    /// <summary>The brain cells of the common values and of move &amp; fire.</summary>
    inline constexpr int32_t ENGAGE_RADIUS_CELL = 2;
    inline constexpr int32_t RUN_CELL = 9;
    inline constexpr int32_t TACTIC_CELL = 0x1c;
    inline constexpr int32_t MOVE_AND_FIRE_CELL = 0x1f;

    /// <summary>
    /// The warrior's header: "[Warrior<paramref name="warrior"/>]" and its numbers of cells and static variables.
    /// </summary>
    /// <remarks>0x0043a7b0.</remarks>
    void writeWarriorHeader(std::ostream& stream, int32_t warrior, int32_t cells, int32_t statics)
    {
        stream << "[Warrior" << warrior << "]\n";
        stream << "l NumCells = " << cells << "\n";
        stream << "l NumStaticVars = " << statics << "\n";
    }

    /// <summary>
    /// The header of the warrior's cell <paramref name="index"/> (from 1; written from 0): brain cell
    /// <paramref name="cell"/> of memory type <paramref name="memoryType"/>.
    /// </summary>
    /// <remarks>0x0043a830.</remarks>
    void writeCellHeader(std::ostream& stream, int32_t warrior, int32_t index, int32_t cell, int32_t memoryType)
    {
        stream << '\n';
        stream << "[Warrior" << warrior << "Cell" << index - 1 << "]\n";
        stream << "l Cell = " << cell << "\n";
        stream << "l MemType = " << memoryType;
        stream << '\n';
    }

    /// <summary>
    /// The header of the warrior's static variable <paramref name="index"/> (from 1; written from 0): its type and
    /// name.
    /// </summary>
    /// <remarks>0x0043a8d0.</remarks>
    void writeStaticHeader(std::ostream& stream, int32_t warrior, int32_t index, int32_t type, const char* name)
    {
        stream << '\n';
        stream << "[Warrior" << warrior << "Static" << index - 1 << "]\n";
        stream << "l Type = " << type;
        stream << '\n';
        stream << "st Name = \"" << name << "\"";
        stream << '\n';
    }

    /// <summary>A long value: in cell <paramref name="cell"/>, or the static variable <paramref name="name"/>.</summary>
    /// <remarks>0x0043a980.</remarks>
    void writeLong(std::ostream& stream, int32_t warrior, int32_t index, int32_t cell, long value,
                   const char* name = nullptr)
    {
        if (name == nullptr)
        {
            writeCellHeader(stream, warrior, index, cell, MEMORY_LONG);
        }
        else
        {
            writeStaticHeader(stream, warrior, index, STATIC_LONG, name);
        }

        stream << "l Value = " << value << "\n";
    }

    /// <summary>A float value in cell <paramref name="cell"/>.</summary>
    /// <remarks>0x0043a9f0 (its static form isn't used).</remarks>
    void writeFloat(std::ostream& stream, int32_t warrior, int32_t index, int32_t cell, float value)
    {
        writeCellHeader(stream, warrior, index, cell, MEMORY_FLOAT);
        stream << "f Value = " << value << "\n";
    }

    /// <summary>The static variable <paramref name="name"/>: a list of floats.</summary>
    /// <remarks>0x0043aa60.</remarks>
    void writeFloats(std::ostream& stream, int32_t warrior, int32_t index, const std::vector<float>& values,
                     const char* name)
    {
        writeStaticHeader(stream, warrior, index, STATIC_FLOATS, name);
        const auto count = static_cast<int32_t>(values.size());
        stream << "l NumValues = " << count;
        stream << '\n';
        stream << "f[" << count << "] Values = \n";

        for (const float value : values)
        {
            stream << value << ",\n";
        }
    }

    /// <summary>The common values: the engage radius (a float), run and the tactic, cells 1-3.</summary>
    void writeCommon(std::ostream& stream, const UnitBrain& brain, int32_t warrior)
    {
        writeFloat(stream, warrior, 1, ENGAGE_RADIUS_CELL, static_cast<float>(brain.engageRadius.value));
        writeLong(stream, warrior, 2, RUN_CELL, brain.run.value ? 1 : 0);
        writeLong(stream, warrior, 3, TACTIC_CELL, brain.tactic.value);
    }

    /// <summary>
    /// The structure id of the structure on <paramref name="target"/>'s cell: the object covering the cell (among the
    /// objects, then the drop zones), numbered by its block, vertex and place on the vertex.
    /// </summary>
    /// <remarks>
    /// EB-114 fixed: the original numbers a null object (reading through it) when there is no structure on the cell,
    /// as for a guard or destroy order whose structure was never set; the port writes the first id.
    /// </remarks>
    long structureId(EditorMap& map, const TargetParameter& target)
    {
        const EditorObject* object = map.objectCoveringCell(target.cellCol, target.cellRow);

        if (object == nullptr)
        {
            return STRUCTURE_ID_BASE;
        }

        int32_t block = 0;
        int32_t vertex = 0;
        int32_t number = 0;
        map.structureIndex(*object, &block, &vertex, &number);
        return number + STRUCTURE_ID_BASE + (block * BLOCK_VERTICES + vertex) * STRUCTURE_IDS_PER_TILE;
    }

    /// <summary>
    /// The side, force group and role of the unit on <paramref name="target"/>'s cell, as statics 1-3 ("VehicleForce",
    /// "VehicleGroup", "VehicleMember").
    /// </summary>
    /// <remarks>
    /// EB-115 fixed: the original reads through a null unit when there is none on the cell, as for a kill or escort
    /// order whose unit was never set; the port writes zeros.
    /// </remarks>
    void writeUnitTarget(std::ostream& stream, EditorMap& map, const TargetParameter& target, int32_t warrior)
    {
        const auto* unit = static_cast<const MoverObject*>(map.moverAt(target.cellCol, target.cellRow));
        writeLong(stream, warrior, 1, 0, unit != nullptr ? unit->side : 0, "VehicleForce");
        writeLong(stream, warrior, 2, 0, unit != nullptr ? unit->forceGroup : 0, "VehicleGroup");
        writeLong(stream, warrior, 3, 0, unit != nullptr ? unit->role : 0, "VehicleMember");
    }

    /// <summary>The map position of <paramref name="target"/>'s cell, as two floats.</summary>
    std::vector<float> targetLocation(EditorMap& map, const TargetParameter& target)
    {
        int32_t x = 0;
        int32_t y = 0;
        map.cellToWorld(target.cellCol, target.cellRow, &x, &y);
        return {static_cast<float>(x), static_cast<float>(y)};
    }
}

bool AttackBrain::write(std::ostream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 3, 0);
    writeCommon(stream, *this, warrior);
    return true;
}

bool SentryBrain::write(std::ostream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 3, 0);
    writeCommon(stream, *this, warrior);
    return true;
}

bool GuardBrain::write(std::ostream& stream, EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 3, 1);
    writeCommon(stream, *this, warrior);
    writeLong(stream, warrior, 1, 0, structureId(map, structure), "StructureID");
    return true;
}

bool PathBrain::write(std::ostream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 4, 3);
    writeCommon(stream, *this, warrior);
    writeLong(stream, warrior, 4, MOVE_AND_FIRE_CELL, moveAndFire.value ? 1 : 0);
    // The path: each point's x and y.
    std::vector<float> values;

    for (const LocationParameter& point : path.points)
    {
        values.push_back(static_cast<float>(point.x));
        values.push_back(static_cast<float>(point.y));
    }

    writeFloats(stream, warrior, 1, values, "Path1");
    writeLong(stream, warrior, 2, 0, static_cast<long>(path.points.size()), "NumPoints");
    writeLong(stream, warrior, 3, 0, delay.value, "StartTime");
    return true;
}

bool DestroyBrain::write(std::ostream& stream, EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 4, 3);
    writeCommon(stream, *this, warrior);
    writeLong(stream, warrior, 4, MOVE_AND_FIRE_CELL, moveAndFire.value ? 1 : 0);
    const long id = structureId(map, structure);
    writeFloats(stream, warrior, 1, targetLocation(map, structure), "Location");
    writeLong(stream, warrior, 2, 0, id, "StructureID");
    writeLong(stream, warrior, 3, 0, delay.value, "StartTime");
    return true;
}

bool KillBrain::write(std::ostream& stream, EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 4, 5);
    writeCommon(stream, *this, warrior);
    writeLong(stream, warrior, 4, MOVE_AND_FIRE_CELL, moveAndFire.value ? 1 : 0);
    const std::vector<float> location = targetLocation(map, unit);
    writeUnitTarget(stream, map, unit, warrior);
    writeFloats(stream, warrior, 4, location, "Location");
    writeLong(stream, warrior, 5, 0, delay.value, "StartTime");
    return true;
}

bool EscortBrain::write(std::ostream& stream, EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 4, 4);
    writeCommon(stream, *this, warrior);
    writeLong(stream, warrior, 4, MOVE_AND_FIRE_CELL, moveAndFire.value ? 1 : 0);
    writeUnitTarget(stream, map, unit, warrior);
    writeLong(stream, warrior, 4, 0, direction.value, "Direction");
    return true;
}

bool NullBrain::write(std::ostream& stream, [[maybe_unused]] EditorMap& map, int32_t warrior)
{
    writeWarriorHeader(stream, warrior, 0, 0);
    return true;
}
