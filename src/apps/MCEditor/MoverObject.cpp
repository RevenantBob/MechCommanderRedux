#include "stdafx.h"
#include "MoverObject.h"
#include "DaoDatabase.h"
#include "EditorPalette.h"
#include "EditorUtil.h"
#include "MechRecord.h"
#include "ObjectLibrary.h"
#include "PilotRecord.h"
#include "UnitBrain.h"

namespace
{
    /// <summary>A mech type's id in the Mechs table: its TedIds.csv profile plus this.</summary>
    inline constexpr long MECH_ID_BASE = 100;
    /// <summary>The fade table row of paint scheme row 0: a scheme's row is its PaintScheme plus this.</summary>
    inline constexpr long PAINT_SCHEME_FIRST_ROW = 0x20;
    /// <summary>Entries per fade table row.</summary>
    inline constexpr size_t FADE_ROW_SIZE = 0x100;
    /// <summary>The weight classes' top tonnages: a mech over a limit is in the next class.</summary>
    inline constexpr long LIGHT_TONNAGE = 0x27;
    inline constexpr long MEDIUM_TONNAGE = 0x37;
    inline constexpr long HEAVY_TONNAGE = 0x4b;

    /// <summary>The fade table row the movers of paint scheme <paramref name="scheme"/> are remapped through.</summary>
    /// <remarks>The original's row can't be null; the port answers null when there is no fade table.</remarks>
    const uint8_t* paintSchemeRow(int32_t scheme)
    {
        if (editorPalette->fadeTable == nullptr)
        {
            return nullptr;
        }

        return editorPalette->fadeTable +
               static_cast<size_t>(PaintSchemes[scheme] + PAINT_SCHEME_FIRST_ROW) * FADE_ROW_SIZE;
    }
}

const char* const PaintSchemeNames[PAINT_SCHEME_COUNT] = {"R/W", "W/R", "G/N", "O/G", "T/G", "B/R", "B/W",
                                                          "G/B", "G/R", "B/Y", "C/G", "C/B", "N/R", "N/Y",
                                                          "T/G", "T/C", "R/G", "R/Y", "Y/B"};

long PaintSchemes[PAINT_SCHEME_COUNT] = {};

std::list<std::pair<long, long>> MechTonnages;

void loadMoverTables(DaoDatabase& database)
{
    PilotRecord pilots(&database);
    pilots.Open(dbOpenDynaset, nullptr, 0);

    for (int32_t i = 0; i < PAINT_SCHEME_COUNT; i++)
    {
        try
        {
            pilots.MoveFirst();

            while (!pilots.IsEOF())
            {
                if (pilots.m_Name.Find(PaintSchemeNames[i]) != -1)
                {
                    PaintSchemes[i] = pilots.m_PaintScheme;
                }

                pilots.MoveNext();
            }
        }
        catch (DaoException* e)
        {
            // Original behaviour (EB-053): "Couldn't initialize the color scemem %s" is formatted with the scheme
            // number as the string, and never shown.
            e->Delete();
        }
    }

    if (!MechTonnages.empty())
    {
        return;
    }

    MechRecord mechs(&database);

    try
    {
        mechs.Open(dbOpenDynaset, nullptr, 0);
    }
    catch (DaoException* e)
    {
        // Original behaviour: the exception isn't deleted, and the searches below run on the closed recordset.
        MessageBoxA(nullptr, e->m_strDescription, nullptr, MB_OK);
    }

    for (const ObjectGroup& group : objectLibrary->moverGroups)
    {
        for (const ObjectType& groupType : group.types)
        {
            // The type is found again through its first frame's id.
            const uint32_t id = groupType.frames.front().slots[0].id;
            const ObjectType& type = objectLibrary->moverGroups[objectIdGroup(id)].types[objectIdType(id)];
            const long mechId = type.profile + MECH_ID_BASE;
            CString filter;
            filter.Format("[ID] = %ld", mechId);

            if (mechs.FindFirst(filter))
            {
                MechTonnages.emplace_back(mechId, mechs.m_CurTonnage);
            }
        }
    }
}

MoverObject::MoverObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side)
{
    this->id = id;
    this->cellCol = cellCol;
    this->cellRow = cellRow;
    selected = false;
    this->side = side;
    brain = new AttackBrain(BRAIN_ATTACK, DEFAULT_ENGAGE_RADIUS);
    paintScheme = 0;
}

MoverObject::MoverObject(const MoverObject& other) : EditorObject(other)
{
    brain = other.brain->clone();
    paintScheme = other.paintScheme;
    rank = other.rank;
    // Original behaviour (EB-066): the copy's force group and role aren't set (the heap's garbage in the original); the
    // port's are 0.
    forceGroup = 0;
    role = 0;
}

MoverObject::~MoverObject()
{
    delete brain;
}

void MoverObject::drawSlot(ShapeSlot& slot, uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y,
                           int32_t size) const
{
    const uint8_t* remap = paintSchemeRow(paintScheme);
    uint8_t** shapes[] = {&slot.shape, &slot.halfShape, &slot.icon};

    if (*shapes[size] == nullptr)
    {
        if (size == 0)
        {
            slot.shape = objectLibrary->getShape(slot.id);
        }
        else if (size == 1)
        {
            slot.halfShape = objectLibrary->getHalfShape(slot.id);
        }
        else if (size == 2)
        {
            objectLibrary->makeIcon(slot.id);
        }
    }

    if (remap != nullptr)
    {
        drawShapeRemapped(buffer, width, height, *shapes[size], 0, x, y, remap);
    }
    else
    {
        drawShape(buffer, width, height, *shapes[size], 0, x, y);
    }

    if (slot.delta && size != 2)
    {
        drawShape(buffer, width, height, *shapes[size], 1, x, y);
    }
}

void MoverObject::draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size)
{
    drawSlot(*shapeSlot, buffer, width, height, x, y, size);
}

void MoverObject::setPilot(DaoDatabase& database, long pilotId)
{
    PilotRecord pilots(&database);
    pilots.Open(dbOpenDynaset, nullptr, 0);
    CString filter;
    filter.Format("[MWID] = %ld", pilotId);
    pilots.FindFirst(filter);
    rank = pilots.m_Rank;

    for (int32_t i = 0; i < PAINT_SCHEME_COUNT; i++)
    {
        if (std::strstr(pilots.m_Name, PaintSchemeNames[i]) != nullptr)
        {
            paintScheme = i;
            break;
        }
    }
}

long MoverObject::findPilotId(DaoDatabase& database)
{
    PilotRecord pilots(&database);
    CString sql;
    sql.Format("SELECT * FROM [NEWMW] WHERE [RANK] = %ld", static_cast<long>(rank));
    pilots.Open(dbOpenDynaset, sql, 0);
    pilots.MoveFirst();

    while (!pilots.IsEOF())
    {
        const CString name = pilots.m_Name;

        if (std::strstr(name, PaintSchemeNames[paintScheme]) != nullptr)
        {
            return pilots.m_MWID;
        }

        pilots.MoveNext();
    }

    CString rankName;
    rankName.LoadString(static_cast<UINT>(IDS_RANK_VEHICLE_CREW + rank));
    CString schemeName;
    schemeName.LoadString(static_cast<UINT>(IDS_RED_WHITE + paintScheme));
    const ObjectType& type = objectLibrary->moverGroups[objectIdGroup(id)].types[objectIdType(id)];
    CString message;
    message.Format(IDS_THE_S_COULD_NOT, type.name.c_str(), static_cast<LPCTSTR>(schemeName),
                   static_cast<LPCTSTR>(rankName));
    CString caption;
    caption.LoadString(IDS_MCEDITOR);
    MessageBoxA(nullptr, message, caption, MB_OK);
    return 0;
}

void MoverObject::resetBrain()
{
    delete brain;
    brain = new AttackBrain(BRAIN_ATTACK, DEFAULT_ENGAGE_RADIUS);
}

VehicleObject::VehicleObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side)
    : MoverObject(id, cellCol, cellRow, side)
{
    shapeSlot = objectLibrary->getShapeSlot(id);
    rank = 0;
    topSlot = nullptr;
    drawTop = true;
}

void VehicleObject::draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, int32_t size)
{
    drawSlot(*shapeSlot, buffer, width, height, x, y, size);

    if (!drawTop)
    {
        return;
    }

    if (topSlot == nullptr)
    {
        if (!objectLibrary->isMoverDestroyable(id))
        {
            drawTop = false;
            return;
        }

        topSlot = objectLibrary->getShapeSlot(id | 1);
    }

    drawSlot(*topSlot, buffer, width, height, x, y, size);
}

EditorObject* VehicleObject::clone()
{
    auto* copy = new VehicleObject(*this);
    copy->topSlot = nullptr;
    copy->drawTop = true;
    return copy;
}

MechObject::MechObject(uint32_t id, int32_t cellCol, int32_t cellRow, int32_t side, int32_t variant)
    : MoverObject(id, cellCol, cellRow, side), variant(variant)
{
    shapeSlot = objectLibrary->getShapeSlot(id);
    rank = 1;
    const ObjectType& type = objectLibrary->moverGroups[objectIdGroup(id)].types[objectIdType(id)];
    const long mechId = type.profile + MECH_ID_BASE;

    for (const auto& [tonnageId, tonnage] : MechTonnages)
    {
        if (tonnageId != mechId)
        {
            continue;
        }

        if (tonnage != 0)
        {
            if (tonnage > LIGHT_TONNAGE)
            {
                rank = 2;
            }

            if (tonnage > MEDIUM_TONNAGE)
            {
                rank = 3;
            }

            if (tonnage > HEAVY_TONNAGE)
            {
                rank = 4;
            }
        }

        break;
    }
}

EditorObject* MechObject::clone()
{
    return new MechObject(*this);
}
