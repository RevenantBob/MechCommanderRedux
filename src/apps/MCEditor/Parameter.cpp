#include "stdafx.h"
#include "Parameter.h"
#include "DaoDatabase.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "EditorPalette.h"
#include "EditorTile.h"
#include "Objective.h"
#include "ObjectiveMainRecord.h"
#include "ObjectiveRecord.h"
#include "UnitRecord.h"
#include "X87Extended.h"

namespace
{
    /// <summary>The objective side value of the second player (ObjectiveMainRecord's Alignment).</summary>
    inline constexpr long SIDE_OTHER = 2;
    /// <summary>Tiles per side of a map block, as the structure ids count them.</summary>
    inline constexpr int32_t BLOCK_TILES = 20;
    /// <summary>The path flag bitmap's side in pixels.</summary>
    inline constexpr int32_t FLAG_SIDE = 0x20;
    /// <summary>Half of <see cref="FLAG_SIDE"/>: the flag is centred on its point.</summary>
    inline constexpr int32_t FLAG_HALF = 0x10;
    /// <summary>The transparent colour of the flag's pixels (magenta in the bitmap).</summary>
    inline constexpr uint8_t FLAG_CLEAR = 0xff;
    /// <summary>A bitmap colour counts as the flag's magenta above this red and blue and below <see cref="MAGENTA_LOW"/> green.</summary>
    inline constexpr BYTE MAGENTA_HIGH = 0xfa;
    inline constexpr BYTE MAGENTA_LOW = 5;
    /// <summary>The scan lines the original asks GetDIBits for (more than the flag has).</summary>
    inline constexpr UINT FLAG_SCAN_LINES = 0x400;
    /// <summary>The bytes of the editor palette's BITMAPINFO the flag's copy takes: the header and 256 colours.</summary>
    inline constexpr size_t FLAG_INFO_SIZE = sizeof(BITMAPINFOHEADER) + 0x100 * sizeof(RGBQUAD);

    /// <summary>The path flag, in palette indices (0x008460c0), read once from bitmap 202.</summary>
    uint8_t FlagPixels[FLAG_SIDE * FLAG_SIDE];
    /// <summary>Whether <see cref="FlagPixels"/> has been read (0x00856cd4).</summary>
    bool FlagLoaded = false;

    /// <summary>Reads bitmap 202 into <see cref="FlagPixels"/> through the editor palette, its magenta made clear.</summary>
    void loadFlag()
    {
        const HDC dc = GetDC(nullptr);
        const HMODULE module = GetModuleHandleA(nullptr);
        auto* info = static_cast<BITMAPINFO*>(operator new(FLAG_INFO_SIZE));
        std::memcpy(info, editorPalette->bitmapInfo, FLAG_INFO_SIZE);
        info->bmiHeader.biWidth = FLAG_SIDE;
        info->bmiHeader.biHeight = -FLAG_SIDE;
        const auto bitmap = static_cast<HBITMAP>(
            LoadImageA(module, MAKEINTRESOURCEA(IDB_FLAG), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        GetDIBits(dc, bitmap, 0, FLAG_SCAN_LINES, FlagPixels, info, DIB_RGB_COLORS);

        for (uint8_t& pixel : FlagPixels)
        {
            const RGBQUAD colour = info->bmiColors[pixel];

            if (colour.rgbRed > MAGENTA_HIGH && colour.rgbBlue > MAGENTA_HIGH && colour.rgbGreen < MAGENTA_LOW)
            {
                pixel = FLAG_CLEAR;
            }
        }

        ReleaseDC(nullptr, dc);
        operator delete(info);
        FlagLoaded = true;
        DeleteObject(bitmap);
    }
}

Parameter::Parameter(int32_t type) : type(type)
{
}

bool Parameter::draw([[maybe_unused]] EditorMap& map, [[maybe_unused]] const POINT& origin,
                     [[maybe_unused]] uint8_t* buffer, [[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height,
                     [[maybe_unused]] int32_t zoom)
{
    return true;
}

bool Parameter::referencesObject([[maybe_unused]] const EditorObject& object, [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool Parameter::moveReference([[maybe_unused]] const EditorObject& object, [[maybe_unused]] EditorMap& map,
                              [[maybe_unused]] int32_t col, [[maybe_unused]] int32_t row)
{
    return false;
}

bool Parameter::release([[maybe_unused]] EditorMap& map)
{
    return true;
}

bool Parameter::drawMarks([[maybe_unused]] EditorMap& map, [[maybe_unused]] const POINT& origin,
                          [[maybe_unused]] HDC dc, [[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height,
                          [[maybe_unused]] int32_t zoom)
{
    return true;
}

void Parameter::reset()
{
}

LocationParameter::LocationParameter(int32_t type) : Parameter(type)
{
}

Parameter* LocationParameter::clone() const
{
    return new LocationParameter(*this);
}

bool LocationParameter::equals(const Parameter& other) const
{
    if (other.type != PARAMETER_LOCATION || other.name != name)
    {
        return false;
    }

    const auto& location = static_cast<const LocationParameter&>(other);
    return location.x == x && location.y == y;
}

bool LocationParameter::load([[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                             [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    return true;
}

bool LocationParameter::getPosition(int32_t* x, int32_t* y, [[maybe_unused]] EditorMap& map)
{
    *x = this->x;
    *y = this->y;
    return true;
}

bool LocationParameter::isSet() const
{
    return x != -1 && y != -1;
}

bool LocationParameter::moveReference([[maybe_unused]] const EditorObject& object, EditorMap& map, int32_t col,
                                      int32_t row)
{
    map.cellToWorld(col, row, &x, &y);
    return true;
}

bool AreaParameter::referencesObject(const EditorObject& object, EditorMap& map)
{
    int32_t col;
    int32_t row;
    map.worldToCell(x, y, &col, &row);
    return object.cellCol == col && object.cellRow == row;
}

bool AreaParameter::release(EditorMap& map)
{
    int32_t col;
    int32_t row;
    map.worldToCell(x, y, &col, &row);
    EditorObject* marker = map.objectAt(col, row);

    // Original behaviour: with no object on the cell, the objectives are asked about a null object (a crash once one
    // has a value that looks at it); the port asks nothing.
    if (marker == nullptr)
    {
        return true;
    }

    for (Objective* objective : map.objectives)
    {
        if (objective->referencesObject(*marker, map))
        {
            return true;
        }
    }

    map.removeAnyObject(*marker, false);
    return true;
}

void LocationParameter::reset()
{
    x = -1;
    y = -1;
}

AreaParameter::AreaParameter() : LocationParameter(PARAMETER_AREA)
{
}

Parameter* AreaParameter::clone() const
{
    return new AreaParameter(*this);
}

bool AreaParameter::equals(const Parameter& other) const
{
    if (other.type != PARAMETER_AREA || other.name != name)
    {
        return false;
    }

    const auto& area = static_cast<const AreaParameter&>(other);
    return area.x == x && area.y == y && area.extractionZone == extractionZone && area.radius == radius;
}

bool AreaParameter::load(ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                         [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    const long centreX = std::atol(record.m_ActionParm1);
    const long centreY = std::atol(record.m_ActionParm2);
    radius = std::atol(record.m_ActionParm3);

    if (record.m_Action == "Extraction Zone")
    {
        extractionZone = true;
    }

    x = centreX;
    y = centreY;
    return true;
}

NumberParameter::NumberParameter(long value) : Parameter(PARAMETER_NUMBER), value(value)
{
}

Parameter* NumberParameter::clone() const
{
    return new NumberParameter(*this);
}

bool NumberParameter::equals(const Parameter& other) const
{
    return other.type == PARAMETER_NUMBER && static_cast<const NumberParameter&>(other).value == value;
}

bool NumberParameter::load(ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                           [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    value = record.m_ActionParm1 == "All" ? PARAMETER_ALL : std::atol(record.m_ActionParm3);
    return true;
}

bool NumberParameter::getPosition([[maybe_unused]] int32_t* x, [[maybe_unused]] int32_t* y,
                                  [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool NumberParameter::isSet() const
{
    return true;
}

BoolParameter::BoolParameter() : Parameter(PARAMETER_BOOL)
{
}

Parameter* BoolParameter::clone() const
{
    return new BoolParameter(*this);
}

bool BoolParameter::equals(const Parameter& other) const
{
    return other.type == PARAMETER_BOOL && other.name == name &&
           static_cast<const BoolParameter&>(other).value == value;
}

bool BoolParameter::load([[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                         [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    return false;
}

bool BoolParameter::getPosition([[maybe_unused]] int32_t* x, [[maybe_unused]] int32_t* y,
                                [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool BoolParameter::isSet() const
{
    return true;
}

LongParameter::LongParameter() : Parameter(PARAMETER_LONG)
{
}

Parameter* LongParameter::clone() const
{
    return new LongParameter(*this);
}

bool LongParameter::equals(const Parameter& other) const
{
    return other.type == PARAMETER_LONG && other.name == name &&
           static_cast<const LongParameter&>(other).value == value;
}

bool LongParameter::load([[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                         [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    return false;
}

bool LongParameter::getPosition([[maybe_unused]] int32_t* x, [[maybe_unused]] int32_t* y,
                                [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool LongParameter::isSet() const
{
    return true;
}

TargetParameter::TargetParameter(int32_t type, bool structure) : Parameter(type), structure(structure)
{
}

Parameter* TargetParameter::clone() const
{
    return new TargetParameter(*this);
}

bool TargetParameter::equals(const Parameter& other) const
{
    if (other.type != PARAMETER_STRUCTURE && other.type != PARAMETER_UNIT)
    {
        return false;
    }

    const auto& target = static_cast<const TargetParameter&>(other);
    return target.cellCol == cellCol && target.cellRow == cellRow && target.objectId == objectId;
}

bool TargetParameter::load(ObjectiveRecord& record, EditorMap& map, DaoDatabase& database, int32_t index)
{
    const CString action = record.m_Action;

    if (std::strstr(action, "Vehicle") != nullptr || std::strstr(action, "Enemy") != nullptr)
    {
        structure = false;
    }
    else if (std::strstr(action, "Structure") != nullptr)
    {
        structure = true;
    }

    if (structure)
    {
        // The row names the structure by its map block and vertex (tile) and its place among that tile's objects.
        const long block = std::atol(record.m_ActionParm1);
        const long vertex = std::atol(record.m_ActionParm2);
        const long number = std::atol(record.m_ActionParm4);
        const auto blocksPerRow = static_cast<int32_t>(static_cast<uint32_t>(map.maxRow + 1) / BLOCK_TILES);
        const int32_t tileCol = BLOCK_TILES * (block % blocksPerRow) + vertex % BLOCK_TILES;
        const int32_t tileRow = BLOCK_TILES * (block / blocksPerRow) + vertex / BLOCK_TILES;
        long count = 0;

        // The original walks the placed objects, then the drop zones.
        for (const std::list<EditorObject*>* list : {&map.objects, &map.dropZones})
        {
            for (const EditorObject* object : *list)
            {
                if (object->cellCol / MAP_CELLS_PER_TILE == tileCol && object->cellRow / MAP_CELLS_PER_TILE == tileRow)
                {
                    if (count == number)
                    {
                        cellCol = object->cellCol;
                        cellRow = object->cellRow;
                        objectId = static_cast<int32_t>(object->id);
                        return true;
                    }

                    count++;
                }
            }
        }

        return false;
    }

    // A unit: the objective's side picks the alignment (a guard objective's units are on the objective's side).
    ObjectiveMainRecord objective(&database);
    objective.Open(dbOpenDynaset, nullptr, 0);
    CString filter;
    filter.Format("[ObjectiveID] = %ld", record.m_ObjectiveID);
    objective.FindFirst(filter);
    CString alignment;

    if (std::strstr(record.m_Action, "Guard") != nullptr)
    {
        alignment = objective.m_Alignment != SIDE_OTHER ? "Clan" : "Other";
    }
    else
    {
        alignment = objective.m_Alignment != SIDE_OTHER ? "Other" : "Clan";
    }

    const long forceGroup = std::atol(record.m_ActionParm2);
    UnitRecord units(&database);
    filter.Format("SELECT * FROM [MissionAssignments] WHERE [MissionID] = %ld AND [ForceGroup] = %ld AND "
                  "[Alignment] = '%s'",
                  record.m_MissionID, forceGroup, static_cast<LPCTSTR>(alignment));
    units.Open(dbOpenDynaset, filter, 0);
    filter.Format("[Role] = %d", index);

    if (units.FindFirst(filter))
    {
        int32_t col;
        int32_t row;
        map.worldToCell(static_cast<int32_t>(units.m_XCoordinate), static_cast<int32_t>(units.m_YCoordinate), &col,
                        &row);

        if (const EditorObject* mover = map.moverAt(col, row); mover != nullptr)
        {
            cellCol = col;
            cellRow = row;
            objectId = static_cast<int32_t>(mover->id);
            return true;
        }
    }

    return false;
}

bool TargetParameter::getPosition(int32_t* x, int32_t* y, EditorMap& map)
{
    if (!isSet())
    {
        return false;
    }

    map.cellToWorld(cellCol, cellRow, x, y);
    return true;
}

bool TargetParameter::isSet() const
{
    return objectId != -1;
}

void TargetParameter::reset()
{
    cellCol = -1;
    cellRow = -1;
    objectId = -1;
}

bool TargetParameter::referencesObject(const EditorObject& object, EditorMap& map)
{
    const EditorObject* target = map.objectAt(cellCol, cellRow);
    return target != nullptr && cellCol == object.cellCol && cellRow == object.cellRow && target->isSameAs(object);
}

bool TargetParameter::moveReference([[maybe_unused]] const EditorObject& object, [[maybe_unused]] EditorMap& map,
                                    int32_t col, int32_t row)
{
    cellCol = col;
    cellRow = row;
    return true;
}

bool TargetParameter::drawMarks(EditorMap& map, const POINT& origin, HDC dc, [[maybe_unused]] int32_t width,
                                [[maybe_unused]] int32_t height, int32_t zoom)
{
    if (const EditorObject* target = map.objectAt(cellCol, cellRow); target != nullptr)
    {
        map.drawObjectBox(origin, dc, *target, zoom, TARGET_BOX_COLOR, TARGET_BOX_SIZE, TARGET_BOX_MARGIN);
    }

    return true;
}

PathParameter::PathParameter() : Parameter(PARAMETER_PATH)
{
}

Parameter* PathParameter::clone() const
{
    return new PathParameter(*this);
}

bool PathParameter::equals(const Parameter& other) const
{
    if (other.type != PARAMETER_PATH)
    {
        return false;
    }

    const auto& path = static_cast<const PathParameter&>(other);
    auto theirs = path.points.begin();

    for (const LocationParameter& point : points)
    {
        // EB-060 fixed: the original walks on past the end of a shorter path (reading its list head); the port counts
        // that as different.
        if (theirs == path.points.end())
        {
            return false;
        }

        if (point.x != theirs->x || point.y != theirs->y)
        {
            return false;
        }

        ++theirs;
    }

    return true;
}

bool PathParameter::load([[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                         [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    return false;
}

bool PathParameter::getPosition([[maybe_unused]] int32_t* x, [[maybe_unused]] int32_t* y,
                                [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool PathParameter::isSet() const
{
    return !points.empty();
}

bool PathParameter::draw(EditorMap& map, const POINT& origin, uint8_t* buffer, int32_t width, int32_t height,
                         [[maybe_unused]] int32_t zoom)
{
    using X = X87Extended;

    if (!FlagLoaded)
    {
        loadFlag();
    }

    for (const LocationParameter& point : points)
    {
        int32_t col;
        int32_t row;
        map.worldToCell(point.x, point.y, &col, &row);
        // The cell's corner in map units, computed as the original does (the x through a float).
        const auto x = static_cast<int32_t>((X(tileWorldWidth * col) / X(MAP_CELLS_PER_TILE)).toFloat());
        const auto y = static_cast<int32_t>((X(tileWorldHeight * row) / X(MAP_CELLS_PER_TILE)).truncate());
        int32_t viewX;
        int32_t viewY;
        map.mapToView(x, y, map.terrainHeight(x, y), &viewX, &viewY);
        const int32_t centreX = viewX - origin.x;
        const int32_t centreY = viewY - origin.y;

        if (centreX <= 0 || centreX >= width || centreY <= 0 || centreY >= height)
        {
            continue;
        }

        int32_t left = centreX - FLAG_HALF;
        int32_t top = centreY - FLAG_HALF;
        const int32_t right = (std::min)(centreX + FLAG_HALF, width);
        const int32_t bottom = (std::min)(centreY + FLAG_HALF, height);
        size_t target = static_cast<size_t>((std::max)(left, 0)) + static_cast<size_t>((std::max)(top, 0)) * width;
        const uint8_t* source = FlagPixels;

        if (top < 0)
        {
            source += -top * FLAG_SIDE;
            top = 0;
        }

        if (left < 0)
        {
            source -= left;
            left = 0;
        }

        for (int32_t rowY = top; rowY < bottom; rowY++)
        {
            for (int32_t i = 0; i < right - left; i++)
            {
                if (source[i] != FLAG_CLEAR)
                {
                    buffer[target + i] = source[i];
                }
            }

            target += width;
            source += FLAG_SIDE;
        }
    }

    return true;
}

void PathParameter::reset()
{
    points.clear();
}

DirectionParameter::DirectionParameter() : Parameter(PARAMETER_DIRECTION)
{
}

Parameter* DirectionParameter::clone() const
{
    return new DirectionParameter(*this);
}

bool DirectionParameter::equals(const Parameter& other) const
{
    return other.type == PARAMETER_DIRECTION && other.name == name &&
           static_cast<const DirectionParameter&>(other).value == value;
}

bool DirectionParameter::load([[maybe_unused]] ObjectiveRecord& record, [[maybe_unused]] EditorMap& map,
                              [[maybe_unused]] DaoDatabase& database, [[maybe_unused]] int32_t index)
{
    return false;
}

bool DirectionParameter::getPosition([[maybe_unused]] int32_t* x, [[maybe_unused]] int32_t* y,
                                     [[maybe_unused]] EditorMap& map)
{
    return false;
}

bool DirectionParameter::isSet() const
{
    return true;
}
