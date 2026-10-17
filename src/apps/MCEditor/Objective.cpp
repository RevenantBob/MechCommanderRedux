#include "stdafx.h"
#include "Objective.h"
#include "DaoDatabase.h"
#include "MissionRecordSet.h"
#include "ObjectiveRecord.h"
#include "EditorMap.h"
#include "Parameter.h"

const char* const OBJECTIVE_ACTIONS[OBJECTIVE_COUNT] = {
    "Destroy Enemy",   "Destroy Enemy",          "Destroy Structure", "Guard Vehicle",
    "Guard Structure", "Capture Enemy Vehicles", "Capture Structure", "Move to Area"};

namespace
{
    /// <summary>The first objective kind name's string (2000, "Destroy All Enemies").</summary>
    inline constexpr UINT OBJECTIVE_NAME_BASE = IDS_DESTROY_ALL_ENEMIES;
    /// <summary>The radius a new area takes.</summary>
    inline constexpr int32_t DEFAULT_AREA_RADIUS = 0x4b;
    /// <summary>The number of value slots of an objective.</summary>
    inline constexpr int32_t PARAMETER_SLOTS = 3;
    /// <summary>ActionParm4 of an objective checked after the last action.</summary>
    const char* const AFTER_LAST_ACTION = "After Last Action";
}

bool guardWarningShown = false;

Objective::Objective() : type(OBJECTIVE_NONE), primary(false)
{
}

Objective::Objective(int32_t type) : type(type)
{
    switch (type)
    {
        case OBJECTIVE_DESTROY_ALL:
        {
            parameters[0] = new NumberParameter(PARAMETER_ALL);
            break;
        }
        case OBJECTIVE_DESTROY_UNITS:
        case OBJECTIVE_GUARD_UNITS:
        case OBJECTIVE_CAPTURE_UNITS:
        {
            parameters[0] = new TargetParameter(PARAMETER_UNIT, false);
            parameters[0]->name.LoadString(IDS_UNITS);
            parameters[1] = new TargetParameter(PARAMETER_UNIT, false);
            parameters[2] = new TargetParameter(PARAMETER_UNIT, false);
            break;
        }
        case OBJECTIVE_DESTROY_BUILDINGS:
        case OBJECTIVE_GUARD_BUILDINGS:
        case OBJECTIVE_CAPTURE_BUILDINGS:
        {
            parameters[0] = new TargetParameter(PARAMETER_STRUCTURE, true);
            parameters[0]->name.LoadString(IDS_BUILDINGS);
            parameters[1] = new TargetParameter(PARAMETER_STRUCTURE, true);
            parameters[2] = new TargetParameter(PARAMETER_STRUCTURE, true);
            break;
        }
        case OBJECTIVE_MOVE_TO_AREA:
        {
            auto* area = new AreaParameter();
            area->radius = DEFAULT_AREA_RADIUS;
            parameters[0] = area;
            parameters[0]->name.LoadString(IDS_LOCATION);
            break;
        }
        default:
        {
            break;
        }
    }

    hasMarker = false;
    afterLastAction = 0;
    name.LoadString(OBJECTIVE_NAME_BASE + static_cast<UINT>(type));
    side = 2;
    primary = true;
}

Objective::Objective(const Objective& other)
    : type(other.type)
    , afterLastAction(other.afterLastAction)
    , hasMarker(other.hasMarker)
    , name(other.name)
    , side(other.side)
    , primary(other.primary)
{
    for (int32_t i = 0; i < PARAMETER_SLOTS; i++)
    {
        parameters[i] = other.parameters[i] != nullptr ? other.parameters[i]->clone() : nullptr;
    }
}

Objective& Objective::operator=(const Objective& other)
{
    if (&other == this)
    {
        return *this;
    }

    for (Parameter*& parameter : parameters)
    {
        delete parameter;
        parameter = nullptr;
    }

    for (int32_t i = 0; i < PARAMETER_SLOTS; i++)
    {
        parameters[i] = other.parameters[i] != nullptr ? other.parameters[i]->clone() : nullptr;
    }

    type = other.type;
    afterLastAction = other.afterLastAction;
    hasMarker = other.hasMarker;
    name = other.name;
    side = other.side;
    primary = other.primary;
    return *this;
}

Objective::~Objective()
{
    for (Parameter*& parameter : parameters)
    {
        delete parameter;
        parameter = nullptr;
    }
}

void Objective::setParameter(int32_t index, Parameter* parameter)
{
    // EB-067 fixed: when the slot holds a value, the original deletes it and stores the slot's index in its place,
    // dropping the new value; the port stores the new value.
    delete parameters[index];
    parameters[index] = parameter;
}

bool Objective::loadParameters(const CString& filter, DaoDatabase& database, ObjectiveRecord& record, EditorMap& map)
{
    if (!record.FindFirst(filter))
    {
        return false;
    }

    afterLastAction = record.m_ActionParm4 == "After Last Action" || record.m_ActionParm5 == "After Last Action";
    bool loaded = true;
    int32_t index = 0;

    do
    {
        // EB-068 fixed: the original gives each row to the next slot without a limit, calling through the fields after
        // the three slots for a fourth row; the port reads the rows past the third into no value.
        if (index < PARAMETER_SLOTS && parameters[index] != nullptr)
        {
            if (!parameters[index]->load(record, map, database, index))
            {
                loaded = false;
            }
        }

        index++;
    } while (record.FindNext(filter));

    if (type == OBJECTIVE_GUARD_UNITS || type == OBJECTIVE_DESTROY_UNITS || type == OBJECTIVE_CAPTURE_UNITS)
    {
        // Original behaviour: the second and third units of a lance come from the last row, as the next roles.
        const int32_t lastIndex = index - 1;
        parameters[1]->load(record, map, database, lastIndex + 1);
        parameters[2]->load(record, map, database, lastIndex + 2);
    }

    return loaded;
}

bool Objective::writeScripting(DaoDatabase& database, long missionId, long objectiveId, long number, EditorMap& map,
                               bool afterLast)
{
    if (type == -1)
    {
        return false;
    }

    MissionRecordSet mission(&database);
    // The row so far; each value completes and adds a copy of it.
    ObjectiveRecord record(&database);
    record.Open(dbOpenDynaset, nullptr, 0);
    record.m_MissionID = missionId;
    record.m_ObjectiveID = objectiveId;
    record.m_Objective = number;
    record.m_Action = OBJECTIVE_ACTIONS[type];
    record.m_ActionSequence = number;
    record.m_ActionParm4 = afterLast ? AFTER_LAST_ACTION : "";
    int32_t kind = 0;

    switch (type)
    {
        case OBJECTIVE_GUARD_UNITS:
        case OBJECTIVE_GUARD_BUILDINGS:
        {
            CString sql;
            sql.Format("SELECT * FROM [NewMissions] WHERE [MissionID] = %ld", missionId);
            mission.Open(dbOpenDynaset, sql, 0);

            if (mission.m_TimeLimit == 0 && !guardWarningShown)
            {
                CString text;
                text.LoadString(IDS_WARNING_THIS_MISSION_CONTAINS);
                CString caption;
                caption.LoadString(IDS_MCEDITOR);
                MessageBoxA(nullptr, text, caption, MB_OK);
                guardWarningShown = true;
            }

            kind = SCRIPT_GUARD;
            break;
        }
        case OBJECTIVE_CAPTURE_UNITS:
        case OBJECTIVE_CAPTURE_BUILDINGS:
            kind = SCRIPT_CAPTURE;
            break;
    }

    const int32_t sideSign = side != 1 ? 1 : -1;

    for (int32_t i = 0; i < PARAMETER_SLOTS; i++)
    {
        if (parameters[i] != nullptr && parameters[i]->isSet())
        {
            parameters[i]->writeScripting(i, database, record, map, kind, sideSign);
        }
    }

    int32_t x = 0;
    int32_t y = 0;

    if (hasMarker && parameters[0] != nullptr && parameters[0]->getPosition(&x, &y, map))
    {
        record.AddNew();
        record.m_MissionID = missionId;
        record.m_ObjectiveID = objectiveId;
        record.m_Objective = number;
        record.m_Action = "Set Objective Marker";
        record.m_ActionParm1.Format("%ld", static_cast<long>(x));
        record.m_ActionParm2.Format("%ld", static_cast<long>(y));
        record.m_ActionParm4 = "Anytime";
        record.m_ActionParm3 = "";
        record.Update();
        record.Close();
    }

    return true;
}

bool Objective::draw(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height, int32_t zoom)
{
    bool drawn = true;

    for (Parameter* parameter : parameters)
    {
        if (parameter != nullptr && !parameter->draw(map, origin, buffer, width, height, zoom))
        {
            drawn = false;
        }
    }

    return drawn;
}

bool Objective::release(EditorMap& map)
{
    bool released = true;

    for (Parameter* parameter : parameters)
    {
        if (parameter != nullptr && !parameter->release(map))
        {
            released = false;
        }
    }

    return released;
}

bool Objective::referencesObject(const EditorObject& object, EditorMap& map)
{
    for (Parameter* parameter : parameters)
    {
        if (parameter != nullptr && parameter->referencesObject(object, map))
        {
            return true;
        }
    }

    return false;
}

bool Objective::moveReferences(const EditorObject& object, EditorMap& map, int32_t col, int32_t row)
{
    bool moved = true;

    for (Parameter* parameter : parameters)
    {
        if (parameter != nullptr && parameter->referencesObject(object, map) &&
            !parameter->moveReference(object, map, col, row))
        {
            moved = false;
        }
    }

    return moved;
}

bool Objective::drawMarks(EditorMap& map, const POINT& origin, HDC dc, int32_t width, int32_t height,
                          [[maybe_unused]] bool unused, int32_t zoom)
{
    bool drawn = true;

    for (Parameter* parameter : parameters)
    {
        if (parameter != nullptr && !parameter->drawMarks(map, origin, dc, width, height, zoom))
        {
            drawn = false;
        }
    }

    return drawn;
}

Objective* loadObjective(DaoDatabase& database, long missionId, long objectiveNumber, EditorMap& map)
{
    try
    {
        ObjectiveRecord record(&database);
        record.Open(dbOpenDynaset, nullptr, 0);
        CString filter;
        filter.Format("[Objective] = %d AND [MissionID] = %d AND [Action] <> 'Set Objective Marker'", objectiveNumber,
                      missionId);
        CString markerFilter;
        markerFilter.Format("[Objective] = %d AND [MissionID] = %d AND [Action] = 'Set Objective Marker'",
                            objectiveNumber, missionId);
        const bool hasMarker = record.FindFirst(markerFilter) != FALSE;

        if (!record.FindFirst(filter))
        {
            return nullptr;
        }

        const CString action = record.m_Action;
        int32_t type = -1;

        for (int32_t i = 0; i < OBJECTIVE_COUNT; i++)
        {
            if (action == OBJECTIVE_ACTIONS[i])
            {
                type = i;
                break;
            }
        }

        if (action == "Destroy Enemy")
        {
            if (record.m_ActionParm1 == "Lance/Star")
            {
                type = OBJECTIVE_DESTROY_UNITS;
            }
        }
        else if (action == "Extraction Zone")
        {
            type = OBJECTIVE_MOVE_TO_AREA;
        }

        if (type == -1)
        {
            return nullptr;
        }

        auto* objective = new Objective(type);
        CString actionFilter;
        // Original behaviour: no space before "AND".
        actionFilter.Format("AND [Action] = '%s'", static_cast<LPCTSTR>(action));
        filter += actionFilter;

        if (!objective->loadParameters(filter, database, record, map))
        {
            CString text;
            text.LoadString(IDS_SOME_MISSION_DATA_MAY);
            CString caption;
            caption.LoadString(IDS_MCEDITOR);
            MessageBoxA(nullptr, text, caption, MB_OK);
            // Original behaviour: the objective isn't deleted (EB-069: the port deletes it).
            delete objective;
            return nullptr;
        }

        objective->hasMarker = hasMarker;
        return objective;
    }
    catch (DaoException* e)
    {
        CString format;
        format.LoadString(IDS_A_DATABASE_QUERY_FAILED);
        CString text;
        text.Format(format, static_cast<LPCTSTR>(e->m_strDescription));
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, text, caption, MB_OK);
        e->Delete();
        return nullptr;
    }
}
