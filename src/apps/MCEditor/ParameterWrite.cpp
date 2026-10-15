#include "stdafx.h"
#include "Parameter.h"
#include "DaoDatabase.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "LookOutPtRecord.h"
#include "MoverObject.h"
#include "ObjectiveRecord.h"
#include "StructureRecord.h"
#include "UnitRecord.h"
#include "resource.h"

// The values' ObjectiveScripting writers (virtual table slot 3: 0x0046c2c0, 0x0046cda0, 0x0046d060, 0x0046d4e0; the
// orders' kinds write nothing): each completes the objective's row and adds a copy of it to the table, with the
// lookout points and captureable marks the objective implies.

namespace
{
    /// <summary>ActionParm4 of a row checked after the last action, and of one checked any time.</summary>
    const char* const AFTER_LAST_ACTION = "After Last Action";
    const char* const ANYTIME = "Anytime";
    /// <summary>The lookout point a guard objective or an area makes: its radius and reveal radius.</summary>
    inline constexpr double LOOKOUT_RADIUS = 10000.0;
    inline constexpr double LOOKOUT_REVEAL_RADIUS = 75.0;
    /// <summary>The building type a captureable structure's new Structure row gets.</summary>
    const char* const CAPTURE_BUILDING_TYPE = "Mech Bay";

    /// <summary><paramref name="value"/> in decimal, as _itoa writes it.</summary>
    CString decimal(long value)
    {
        CString text;
        text.Format("%ld", value);
        return text;
    }

    /// <summary>Adds a copy of <paramref name="record"/> (but its ActionEntry) to ObjectiveScripting.</summary>
    void addRow(DaoDatabase& database, const ObjectiveRecord& record)
    {
        ObjectiveRecord row(&database);
        row.Open(dbOpenDynaset, nullptr, 0);
        row.AddNew();
        row.copyFields(record);
        row.Update();
        row.Close();
    }

    /// <summary>
    /// Adds a lookout point of mission <paramref name="missionId"/> at (<paramref name="x"/>, <paramref name="y"/>)
    /// for <paramref name="sideSign"/>, its radius <see cref="LOOKOUT_RADIUS"/>.
    /// </summary>
    void addLookout(DaoDatabase& database, long missionId, int32_t x, int32_t y, int32_t sideSign)
    {
        LookOutPtRecord lookout(&database);
        lookout.Open(dbOpenDynaset, nullptr, 0);
        lookout.AddNew();
        lookout.m_MissionID = missionId;
        lookout.m_Radius = LOOKOUT_RADIUS;
        lookout.m_RevealRadius = LOOKOUT_REVEAL_RADIUS;
        lookout.m_XCoordinate = x;
        lookout.m_YCoordinate = y;
        lookout.m_Side = sideSign;
        lookout.Update();
        lookout.Close();
    }

    /// <summary>Reports a database error while writing an objective (string 0xe010) and deletes it.</summary>
    void reportObjectiveError(DaoException* e)
    {
        CString format;
        format.LoadString(IDS_COULDNT_ADD_AN_OBJECTIVE);
        CString text;
        text.Format(format, static_cast<LPCTSTR>(e->m_strDescription));
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, text, caption, MB_OK);
        e->Delete();
    }
}

bool Parameter::writeScripting([[maybe_unused]] int32_t index, [[maybe_unused]] DaoDatabase& database,
                               [[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                               [[maybe_unused]] int32_t kind, [[maybe_unused]] int32_t sideSign)
{
    return false;
}

bool LocationParameter::writeScripting([[maybe_unused]] int32_t index, [[maybe_unused]] DaoDatabase& database,
                                       [[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                                       [[maybe_unused]] int32_t kind, [[maybe_unused]] int32_t sideSign)
{
    return true;
}

bool AreaParameter::writeScripting([[maybe_unused]] int32_t index, DaoDatabase& database, ObjectiveRecord& record,
                                   EditorMap& map, [[maybe_unused]] int32_t kind, int32_t sideSign)
{
    if (extractionZone)
    {
        record.m_Action = "Extraction Zone";
    }

    record.m_ActionParm1 = decimal(x);
    record.m_ActionParm2 = decimal(y);
    record.m_ActionParm3 = decimal(radius);

    if (record.m_ActionParm4 != AFTER_LAST_ACTION)
    {
        record.m_ActionParm4 = ANYTIME;
    }

    record.m_ActionParm5 = "ALL";
    addRow(database, record);
    int32_t lookoutX = 0;
    int32_t lookoutY = 0;
    getPosition(&lookoutX, &lookoutY, map);
    addLookout(database, record.m_MissionID, lookoutX, lookoutY, sideSign);
    return true;
}

bool NumberParameter::writeScripting([[maybe_unused]] int32_t index, DaoDatabase& database, ObjectiveRecord& record,
                                     [[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t kind,
                                     [[maybe_unused]] int32_t sideSign)
{
    // Original behaviour: the parameters after the first are emptied, ActionParm4's "After Last Action" too.
    record.m_ActionParm5 = "";
    record.m_ActionParm4 = record.m_ActionParm5;
    record.m_ActionParm3 = record.m_ActionParm4;
    record.m_ActionParm2 = record.m_ActionParm3;

    if (value == PARAMETER_ALL)
    {
        record.m_ActionParm1 = "All";
    }
    else
    {
        record.m_ActionParm1 = "Any";
        record.m_ActionParm3 = decimal(value);
    }

    addRow(database, record);
    return true;
}

bool TargetParameter::writeScripting(int32_t index, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map,
                                     int32_t kind, int32_t sideSign)
{
    if (record.m_ActionParm4 == AFTER_LAST_ACTION)
    {
        record.m_ActionParm5 = AFTER_LAST_ACTION;
    }

    if (structure)
    {
        // The structure on the target's cell, by its block, vertex and number on the vertex.
        // EB-116 fixed: the original numbers (and, for a capture, reads the side of) a null object when no structure
        // is on the cell; the port writes no rows for it.
        const EditorObject* object = map.objectCoveringCell(cellCol, cellRow);

        if (object == nullptr)
        {
            return false;
        }

        int32_t block = 0;
        int32_t vertex = 0;
        int32_t number = 0;
        map.structureIndex(*object, &block, &vertex, &number);
        record.m_ActionParm4 = decimal(number);
        record.m_ActionParm1 = decimal(block);
        record.m_ActionParm2 = decimal(vertex);
        record.m_ActionParm3 = "NO";

        try
        {
            addRow(database, record);
        }
        catch (DaoException* e)
        {
            reportObjectiveError(e);
        }

        if (kind == SCRIPT_CAPTURE)
        {
            // The structure is marked captureable, its row added if it has none.
            try
            {
                StructureRecord structures(&database);
                structures.Open(dbOpenDynaset, nullptr, 0);
                CString filter;
                filter.Format("[MissionID] = %d AND [Block] = %d AND [Vertex] = %d AND [Number] = %d",
                              record.m_MissionID, block, vertex, number);

                if (structures.FindFirst(filter))
                {
                    structures.Edit();
                    structures.m_Captureable = TRUE;
                    structures.Update();
                }
                else
                {
                    structures.AddNew();
                    structures.m_Block = block;
                    structures.m_Vertex = vertex;
                    structures.m_Number = number;
                    structures.m_MissionID = record.m_MissionID;
                    structures.m_BuildingType = CAPTURE_BUILDING_TYPE;
                    CString identifier;
                    identifier.Format("%s_%03ld%03ld%03ld", static_cast<LPCTSTR>(structures.m_BuildingType),
                                      static_cast<long>(block), static_cast<long>(vertex), static_cast<long>(number));
                    identifier.Replace(' ', '_');
                    structures.m_Identifier = identifier;
                    structures.m_Alignment = object->side;
                    structures.m_Captureable = TRUE;
                    structures.Update();
                    structures.Close();
                }
            }
            catch (DaoException* e)
            {
                reportObjectiveError(e);
            }
        }
    }
    else if (index == 0)
    {
        // A unit target writes its force group once, for the first target.
        // EB-117 fixed: the original reads through a null unit when none is on the target's cell; the port writes no
        // rows for it.
        const auto* unit = static_cast<const MoverObject*>(map.moverAt(cellCol, cellRow));

        if (unit == nullptr)
        {
            return false;
        }

        const long group = unit->forceGroup + 1;
        UnitRecord units(&database);
        CString sql;
        sql.Format("SELECT * FROM [MissionAssignments] WHERE[ForceGroup] = %ld AND [MissionID] = %ld AND "
                   "[Alignment] = '%s'",
                   group, record.m_MissionID, unit->side == 1 ? "Clan" : "Other");
        units.Open(dbOpenDynaset, sql, 0);
        units.MoveLast();

        if (units.IsEOF() || units.IsBOF())
        {
            return false;
        }

        const long count = units.GetRecordCount();
        ObjectiveRecord row(&database);
        row.Open(dbOpenDynaset, nullptr, 0);
        row.AddNew();
        row.copyFields(record);
        row.m_ActionParm1 = "Lance/Star";
        row.m_ActionParm2 = decimal(group);
        row.m_ActionParm3 = decimal(kind == SCRIPT_GUARD ? unit->role : count);
        row.Update();

        if (kind == SCRIPT_CAPTURE)
        {
            // The group's units are marked captureable.
            for (units.MoveFirst(); !units.IsEOF(); units.MoveNext())
            {
                units.Edit();
                units.m_Captureable = TRUE;
                units.Update();
            }
        }
    }

    if (kind == SCRIPT_GUARD)
    {
        int32_t x = 0;
        int32_t y = 0;
        getPosition(&x, &y, map);
        addLookout(database, record.m_MissionID, x, y, sideSign);
    }

    return false;
}
