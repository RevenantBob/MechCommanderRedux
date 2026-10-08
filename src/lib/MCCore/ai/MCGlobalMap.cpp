#include "stdafx.h"
#include "ai/MCGlobalMap.h"
#include "ai/MCMoveMap.h"
#include "ai/MCMovePath.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCScenarioMap.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCPriorityQueue.h"

namespace
{
    /// <summary>Whether the area fill treats wall overlays (60) as blocking (always, in MCX).</summary>
    constexpr bool BlockWallTiles = true;

    /// <summary>The overlays of the north-south and east-west road bridges, and of the railroad bridges.</summary>
    constexpr uint32_t NorthSouthBridge = 0x25;
    constexpr uint32_t EastWestBridge = 0x27;
    constexpr uint32_t NorthSouthRailroadBridge = 0x37;
    constexpr uint32_t EastWestRailroadBridge = 0x39;

    /// <summary>Reads a little-endian field from a file record and steps past it.</summary>
    template <typename T> T ReadRecordField(const uint8_t*& cursor)
    {
        T value;
        std::memcpy(&value, cursor, sizeof(T));
        cursor += sizeof(T);
        return value;
    }

    /// <summary>Writes a field into a file record and steps past it.</summary>
    template <typename T> void WriteRecordField(uint8_t*& cursor, T value)
    {
        std::memcpy(cursor, &value, sizeof(T));
        cursor += sizeof(T);
    }

    /// <summary>Decodes an area record (its door list position is left for the caller).</summary>
    MCGlobalMapArea DecodeArea(const uint8_t* record)
    {
        MCGlobalMapArea area;
        area.SectorR = ReadRecordField<int16_t>(record);
        area.SectorC = ReadRecordField<int16_t>(record);
        ReadRecordField<uint32_t>(record);
        area.Type = ReadRecordField<int32_t>(record);
        area.NumDoors = ReadRecordField<int8_t>(record);
        area.Open = ReadRecordField<int32_t>(record);
        // Two editor-only words nothing reads.
        ReadRecordField<int32_t>(record);
        ReadRecordField<int32_t>(record);
        area.Closed = ReadRecordField<int32_t>(record);
        // The record ends with three editor-only words nothing reads.
        return area;
    }

    /// <summary>Encodes an area as its file record (the doors pointer written as 0).</summary>
    void EncodeArea(const MCGlobalMapArea& area, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, area.SectorR);
        WriteRecordField<int16_t>(record, area.SectorC);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.Type);
        WriteRecordField<int8_t>(record, static_cast<int8_t>(area.NumDoors));
        WriteRecordField<int32_t>(record, area.Open);
        // The editor-only words, with the values the editor's calcAreas gives them.
        WriteRecordField<int32_t>(record, -1);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.Closed);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, 0);
        WriteRecordField<int32_t>(record, 0);
    }

    /// <summary>Decodes a door record (its link list positions are left for the caller).</summary>
    MCGlobalMapDoor DecodeDoor(const uint8_t* record)
    {
        MCGlobalMapDoor door;
        door.Row = ReadRecordField<int16_t>(record);
        door.Col = ReadRecordField<int16_t>(record);
        door.CellR = ReadRecordField<uint8_t>(record);
        door.CellC = ReadRecordField<uint8_t>(record);
        door.Length = ReadRecordField<int8_t>(record);
        door.Open = ReadRecordField<int32_t>(record);
        door.Area[0] = ReadRecordField<int16_t>(record);
        door.Area[1] = ReadRecordField<int16_t>(record);
        door.AreaCost[0] = ReadRecordField<int16_t>(record);
        door.AreaCost[1] = ReadRecordField<int16_t>(record);
        door.Direction[0] = ReadRecordField<int8_t>(record);
        door.Direction[1] = ReadRecordField<int8_t>(record);
        door.NumLinks[0] = ReadRecordField<int8_t>(record);
        door.NumLinks[1] = ReadRecordField<int8_t>(record);
        ReadRecordField<uint32_t>(record);
        ReadRecordField<uint32_t>(record);
        door.Cost = ReadRecordField<int32_t>(record);
        door.Parent = ReadRecordField<int32_t>(record);
        door.FromAreaIndex = ReadRecordField<int32_t>(record);
        door.Flags = ReadRecordField<uint32_t>(record);
        door.G = ReadRecordField<int32_t>(record);
        door.HPrime = ReadRecordField<int32_t>(record);
        door.FPrime = ReadRecordField<int32_t>(record);
        return door;
    }

    /// <summary>Encodes a door as its file record (the link pointers written as 0).</summary>
    void EncodeDoor(const MCGlobalMapDoor& door, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, door.Row);
        WriteRecordField<int16_t>(record, door.Col);
        WriteRecordField<uint8_t>(record, door.CellR);
        WriteRecordField<uint8_t>(record, door.CellC);
        WriteRecordField<int8_t>(record, door.Length);
        WriteRecordField<int32_t>(record, door.Open);
        WriteRecordField<int16_t>(record, door.Area[0]);
        WriteRecordField<int16_t>(record, door.Area[1]);
        WriteRecordField<int16_t>(record, door.AreaCost[0]);
        WriteRecordField<int16_t>(record, door.AreaCost[1]);
        WriteRecordField<int8_t>(record, door.Direction[0]);
        WriteRecordField<int8_t>(record, door.Direction[1]);
        WriteRecordField<int8_t>(record, door.NumLinks[0]);
        WriteRecordField<int8_t>(record, door.NumLinks[1]);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, door.Cost);
        WriteRecordField<int32_t>(record, door.Parent);
        WriteRecordField<int32_t>(record, door.FromAreaIndex);
        WriteRecordField<uint32_t>(record, door.Flags);
        WriteRecordField<int32_t>(record, door.G);
        WriteRecordField<int32_t>(record, door.HPrime);
        WriteRecordField<int32_t>(record, door.FPrime);
    }

    /// <summary>Reads <paramref name="count"/> raw (packed, fixed layout) records into a vector.</summary>
    template <typename T> std::vector<T> ReadRaw(MCFile& file, int32_t count)
    {
        std::vector<T> records(static_cast<size_t>(std::max(count, 0)));
        file.Read(std::span(reinterpret_cast<uint8_t*>(records.data()), records.size() * sizeof(T)));
        return records;
    }

    /// <summary>Writes raw (packed, fixed layout) records.</summary>
    template <typename T> void WriteRaw(MCFile& file, std::span<const T> records)
    {
        file.Write(std::span(reinterpret_cast<const uint8_t*>(records.data()), records.size_bytes()));
    }

    /// <summary>Makes a temporary door joining <paramref name="areaIndex"/> to itself (the start or goal door).</summary>
    /// <remarks>The field setup shared by GlobalMap::setStartDoor and setGoalDoor (inlined in both).</remarks>
    void InitTempDoor(MCGlobalMapDoor& door, int32_t areaIndex, int32_t numLinks)
    {
        door.Direction = {-1, -1};
        door.Area = {static_cast<int16_t>(areaIndex), static_cast<int16_t>(areaIndex)};
        door.Row = 0;
        door.Col = 0;
        door.CellR = 0;
        door.CellC = 0;
        door.Length = 0;
        door.Open = 1;
        door.AreaCost = {1, 1};
        door.NumLinks = {static_cast<int8_t>(numLinks), 0};
    }
}

MCGlobalMap::MCGlobalMap(MCFile& mapFile, MCPriorityQueue& openList) : _OpenList(&openList)
{
    const int32_t version = mapFile.ReadLong();

    if (version != FileVersion)
    {
        Fatal(version, " Bad version number in Global Map ");
    }

    // Header words 1 and 2: written by the editor (always 0), read by nothing.
    mapFile.ReadLong();
    mapFile.ReadLong();
    Height = mapFile.ReadLong();
    Width = mapFile.ReadLong();
    SectorDim = mapFile.ReadLong();
    SectorHeight = mapFile.ReadLong();
    SectorWidth = mapFile.ReadLong();
    NumAreas = mapFile.ReadLong();
    NumDoors = mapFile.ReadLong();
    NumDoorInfos = mapFile.ReadLong();
    NumDoorLinks = mapFile.ReadLong();

    if (NumAreas < 256)
    {
        SmallAreaMap = ReadRaw<uint8_t>(mapFile, Width * Height);
    }
    else
    {
        AreaMap = ReadRaw<int16_t>(mapFile, Width * Height);
    }

    DoorInfos = ReadRaw<MCDoorInfo>(mapFile, NumDoorInfos);

    // Port fix: room for the spare area SetTempArea writes (the original allocated exactly numAreas here).
    Areas.resize(static_cast<size_t>(NumAreas) + 1);
    const std::vector<uint8_t> areaRecords = ReadRaw<uint8_t>(mapFile, NumAreas * AreaRecordSize);
    int32_t infoIndex = 0;

    for (int32_t i = 0; i < NumAreas; i++)
    {
        MCGlobalMapArea& area = Areas[static_cast<size_t>(i)];
        area = DecodeArea(areaRecords.data() + static_cast<size_t>(i) * AreaRecordSize);
        area.FirstDoorInfo = infoIndex;
        infoIndex += area.NumDoors;
    }

    DoorLinks = ReadRaw<MCDoorLink>(mapFile, NumDoorLinks);

    const int32_t totalDoors = NumDoors + 2;
    Doors.resize(static_cast<size_t>(totalDoors));
    const std::vector<uint8_t> doorRecords = ReadRaw<uint8_t>(mapFile, totalDoors * DoorRecordSize);
    int32_t linkIndex = 0;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        MCGlobalMapDoor& door = Doors[static_cast<size_t>(i)];
        door = DecodeDoor(doorRecords.data() + static_cast<size_t>(i) * DoorRecordSize);

        for (size_t side = 0; side < 2; side++)
        {
            door.FirstLink[side] = linkIndex;
            Assert(door.NumLinks[side] + 2 > 1, 0, " Bad Door Links Count ");
            linkIndex += door.NumLinks[side] + 2;
        }
    }

    PathCostTable = ReadRaw<uint8_t>(mapFile, NumAreas * NumAreas);
}

MCGlobalMap::MCGlobalMap(const MCScenarioMap& map, MCMoveMap& pathFinder, MCPriorityQueue& openList)
    : _OpenList(&openList), _PathFinder(&pathFinder), _ScenarioMap(&map)
{
    // As the original: the map's height goes to the width (the maps are square).
    InitAreaMap(map.Height, map.Width);
    CalcAreas(map);
    CalcBridges(map);
    CalcGlobalDoors(map);
    CalcAreaDoors();
    CalcDoorLinks();

    if (NumAreas < 256)
    {
        SmallAreaMap.resize(AreaMap.size());
        std::ranges::transform(AreaMap, SmallAreaMap.begin(),
                               [](int16_t area) { return area < 0 ? uint8_t{0xff} : static_cast<uint8_t>(area); });
        AreaMap.clear();
    }

    _PathFinder = nullptr;
    _ScenarioMap = nullptr;
}

auto MCGlobalMap::InitAreaMap(int32_t newWidth, int32_t newHeight) -> void
{
    Width = newWidth;
    Height = newHeight;
    AreaMap.assign(static_cast<size_t>(newWidth * newHeight), int16_t{-1});
    SectorDim = DefaultSectorDim;

    if (Width % SectorDim != 0 || Height % SectorDim != 0)
    {
        Fatal(0, "Scenario Map Dimensions must be multiples of SectorDim");
    }

    NumAreas = 0;
    NumDoors = 0;
    SectorWidth = newWidth / SectorDim;
    SectorHeight = newWidth / SectorDim;
}

auto MCGlobalMap::Write(MCFile& mapFile) -> void
{
    mapFile.WriteLong(FileVersion);
    mapFile.WriteLong(0);
    mapFile.WriteLong(0);
    mapFile.WriteLong(Height);
    mapFile.WriteLong(Width);
    mapFile.WriteLong(SectorDim);
    mapFile.WriteLong(SectorHeight);
    mapFile.WriteLong(SectorWidth);
    mapFile.WriteLong(NumAreas);
    mapFile.WriteLong(NumDoors);
    mapFile.WriteLong(NumDoorInfos);
    mapFile.WriteLong(NumDoorLinks);

    if (!SmallAreaMap.empty())
    {
        WriteRaw<uint8_t>(mapFile, SmallAreaMap);
    }
    else
    {
        WriteRaw<int16_t>(mapFile, AreaMap);
    }

    for (int32_t i = 0; i < NumAreas; i++)
    {
        WriteRaw<MCDoorInfo>(mapFile, AreaDoors(i));
    }

    std::vector<uint8_t> records(static_cast<size_t>(NumAreas) * AreaRecordSize);

    for (int32_t i = 0; i < NumAreas; i++)
    {
        EncodeArea(Areas[static_cast<size_t>(i)], records.data() + static_cast<size_t>(i) * AreaRecordSize);
    }

    WriteRaw<uint8_t>(mapFile, records);

    const int32_t totalDoors = NumDoors + 2;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        for (int32_t side = 0; side < 2; side++)
        {
            Assert(Doors[static_cast<size_t>(i)].NumLinks[static_cast<size_t>(side)] + 2 > 1, 0,
                   " Bad Door Links Count ");
            WriteRaw<MCDoorLink>(mapFile, DoorSideLinks(i, side));
        }
    }

    records.assign(static_cast<size_t>(totalDoors) * DoorRecordSize, 0);

    for (int32_t i = 0; i < totalDoors; i++)
    {
        EncodeDoor(Doors[static_cast<size_t>(i)], records.data() + static_cast<size_t>(i) * DoorRecordSize);
    }

    WriteRaw<uint8_t>(mapFile, records);
    CalcPathCostTable();
    WriteRaw<uint8_t>(mapFile, PathCostTable);
}

auto MCGlobalMap::AreaDoors(int32_t area) -> std::span<MCDoorInfo>
{
    const MCGlobalMapArea& info = Areas[static_cast<size_t>(area)];
    return std::span(DoorInfos).subspan(static_cast<size_t>(info.FirstDoorInfo), static_cast<size_t>(info.NumDoors));
}

auto MCGlobalMap::DoorSideLinks(int32_t door, int32_t side) -> std::span<MCDoorLink>
{
    const MCGlobalMapDoor& info = Doors[static_cast<size_t>(door)];
    return std::span(DoorLinks).subspan(static_cast<size_t>(info.FirstLink[static_cast<size_t>(side)]),
                                        static_cast<size_t>(info.NumLinks[static_cast<size_t>(side)] + 2));
}

auto MCGlobalMap::SetTempArea(int32_t tileR, int32_t tileC) -> int32_t
{
    MCGlobalMapArea& area = Areas[static_cast<size_t>(NumAreas)];
    area.NumDoors = 0;
    area.SectorR = static_cast<int16_t>(tileR / SectorDim);
    area.SectorC = static_cast<int16_t>(tileC / SectorDim);
    area.Open = 1;
    return NumAreas;
}

auto MCGlobalMap::InFillSector(int32_t row, int32_t col) const -> bool
{
    return row >= _MinTileR && row < _MaxTileR && col >= _MinTileC && col < _MaxTileC;
}

auto MCGlobalMap::SetFillSector(int32_t sectorR, int32_t sectorC) -> void
{
    _MinTileR = SectorDim * sectorR;
    _MaxTileR = SectorDim + _MinTileR;
    _MinTileC = SectorDim * sectorC;
    _MaxTileC = SectorDim + _MinTileC;
}

auto MCGlobalMap::FillNorthSouthBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area) -> int32_t
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[static_cast<size_t>(row * Width + col)] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && map.TileAt(next, col).OverlayType() == NorthSouthBridge &&
            AreaMap[static_cast<size_t>(next * Width + col)] == -1)
        {
            FillNorthSouthBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillEastWestBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area) -> int32_t
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[static_cast<size_t>(row * Width + col)] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && map.TileAt(row, next).OverlayType() == EastWestBridge &&
            AreaMap[static_cast<size_t>(row * Width + next)] == -1)
        {
            FillEastWestBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillNorthSouthRailroadBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area)
    -> int32_t
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[static_cast<size_t>(row * Width + col)] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && map.TileAt(next, col).OverlayType() == NorthSouthRailroadBridge &&
            AreaMap[static_cast<size_t>(next * Width + col)] == -1)
        {
            FillNorthSouthRailroadBridgeArea(map, next, col, area);
        }
    }

    return 1;
}

auto MCGlobalMap::FillEastWestRailroadBridgeArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area)
    -> int32_t
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    AreaMap[static_cast<size_t>(row * Width + col)] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && map.TileAt(row, next).OverlayType() == EastWestRailroadBridge &&
            AreaMap[static_cast<size_t>(row * Width + next)] == -1)
        {
            FillEastWestRailroadBridgeArea(map, row, next, area);
        }
    }

    return 1;
}

namespace
{
    /// <summary>Whether a tile's overlay blocks left-right crossing: a full row or column of blocked cells.</summary>
    bool IsLRBlocked(const MCMapTile& tile)
    {
        auto blocked = [&tile](int32_t cell) { return tile.GetCellPassable(cell / 3, cell % 3) == 0; };

        for (int32_t i = 0; i < 3; i++)
        {
            if ((blocked(i * 3) && blocked(i * 3 + 1) && blocked(i * 3 + 2)) ||
                (blocked(i) && blocked(i + 3) && blocked(i + 6)))
            {
                return true;
            }
        }

        return false;
    }
}

auto MCGlobalMap::FillArea(const MCScenarioMap& map, int32_t row, int32_t col, int32_t area) -> int32_t
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    const MCMapTile& tile = map.TileAt(row, col);
    const uint32_t overlay = tile.OverlayType();

    if (overlay == EastWestBridge || overlay == NorthSouthBridge || overlay == EastWestRailroadBridge ||
        overlay == NorthSouthRailroadBridge)
    {
        return 0;
    }

    if (CurPlanet == 1 && OverlayIsDirtRoad[overlay])
    {
        return 0;
    }

    const uint32_t terrain = tile.Cells & 0x7f;
    bool open = false;

    if ((tile.Cells & 0x55554000) != 0 && !IsLRBlocked(tile) && overlay != 0x3e && terrain != 0x2a && terrain != 0x29)
    {
        open = !BlockWallTiles || overlay != 0x3c;
    }

    if (!open)
    {
        AreaMap[static_cast<size_t>(Width * row + col)] = -2;
        return 0;
    }

    AreaMap[static_cast<size_t>(Width * row + col)] = static_cast<int16_t>(area);

    for (const std::array<int32_t, 2>& adj : AdjTile)
    {
        const int32_t nextRow = adj[0] + row;
        const int32_t nextCol = adj[1] + col;

        if (InFillSector(nextRow, nextCol) && AreaMap[static_cast<size_t>(Width * nextRow + nextCol)] == -1)
        {
            FillArea(map, nextRow, nextCol, area);
        }
    }

    return 1;
}

auto MCGlobalMap::CalcSectorAreas(const MCScenarioMap& map, int32_t sectorR, int32_t sectorC) -> void
{
    SetFillSector(sectorR, sectorC);

    for (int32_t row = _MinTileR; row < _MaxTileR; row++)
    {
        for (int32_t col = _MinTileC; col < _MaxTileC; col++)
        {
            if (AreaMap[static_cast<size_t>(Width * row + col)] != -1)
            {
                continue;
            }

            int32_t filled;

            switch (map.TileAt(row, col).OverlayType())
            {
                case NorthSouthBridge:
                    filled = FillNorthSouthBridgeArea(map, row, col, NumAreas);
                    break;
                case EastWestBridge:
                    filled = FillEastWestBridgeArea(map, row, col, NumAreas);
                    break;
                case NorthSouthRailroadBridge:
                    filled = FillNorthSouthRailroadBridgeArea(map, row, col, NumAreas);
                    break;
                case EastWestRailroadBridge:
                    filled = FillEastWestRailroadBridgeArea(map, row, col, NumAreas);
                    break;
                default:
                    filled = FillArea(map, row, col, NumAreas);
                    break;
            }

            if (filled != 0)
            {
                NumAreas++;
            }
        }
    }
}

auto MCGlobalMap::CalcAreas(const MCScenarioMap& map) -> void
{
    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            CalcSectorAreas(map, sectorR, sectorC);
        }
    }

    if (NumAreas > 10000)
    {
        Fatal(0, " Too many GlobalMapAreas ");
    }

    // One spare area past the last, for SetTempArea.
    Areas.assign(static_cast<size_t>(NumAreas) + 1, MCGlobalMapArea{.Open = 1});

    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            SetFillSector(sectorR, sectorC);

            for (int32_t row = _MinTileR; row < _MaxTileR; row++)
            {
                for (int32_t col = _MinTileC; col < _MaxTileC; col++)
                {
                    const int16_t area = AreaMap[static_cast<size_t>(Width * row + col)];

                    if (area >= 0)
                    {
                        Areas[static_cast<size_t>(area)].SectorR = static_cast<int16_t>(sectorR);
                        Areas[static_cast<size_t>(area)].SectorC = static_cast<int16_t>(sectorC);
                    }
                }
            }
        }
    }
}

auto MCGlobalMap::CalcBridges(const MCScenarioMap& map) -> void
{
    for (int32_t row = 0; row < Height; row++)
    {
        for (int32_t col = 0; col < Width; col++)
        {
            const uint32_t overlay = map.TileAt(row, col).OverlayType();
            const size_t area = static_cast<size_t>(AreaMap[static_cast<size_t>(Width * row + col)]);

            if (overlay == NorthSouthBridge || overlay == NorthSouthRailroadBridge)
            {
                Areas[area].Type = 1;
            }
            else if (overlay == EastWestBridge || overlay == EastWestRailroadBridge)
            {
                Areas[area].Type = 2;
            }
        }
    }
}

auto MCGlobalMap::AddDoor(std::vector<MCGlobalMapDoor>& buildList, int32_t area1, int32_t area2, int32_t row,
                          int32_t col, int32_t cellR, int32_t cellC, int32_t length, int32_t direction) const -> void
{
    for (const MCGlobalMapDoor& door : buildList)
    {
        if (door.Row == row && door.Col == col && door.CellR == cellR && door.CellC == cellC && door.Length == length &&
            door.Direction[0] == direction)
        {
            return;
        }
    }

    MCGlobalMapDoor& door = buildList.emplace_back();
    door.Row = static_cast<int16_t>(row);
    door.Col = static_cast<int16_t>(col);
    door.CellR = static_cast<uint8_t>(cellR);
    door.CellC = static_cast<uint8_t>(cellC);
    door.Length = static_cast<int8_t>(length);
    door.Open = 1;
    door.Area = {static_cast<int16_t>(area1), static_cast<int16_t>(area2)};
    door.AreaCost = {1, 1};
    door.Direction = {static_cast<int8_t>(direction), static_cast<int8_t>((direction + 2) % 4)};
}

auto MCGlobalMap::CalcGlobalDoors(const MCScenarioMap& map) -> void
{
    // The doors found (no limit: the original's build list held 5000), then the two temporary doors.
    std::vector<MCGlobalMapDoor> buildList;
    const int32_t cellMapSide = SectorDim * MapCellDim;
    std::vector<int16_t> cellMap(static_cast<size_t>(cellMapSide * cellMapSide));

    for (int32_t sectorR = 0; sectorR < SectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < SectorWidth; sectorC++)
        {
            // Direction 1 looks east, 2 south (AdjTile), for the cells an area can cross into its neighbour by.
            for (int32_t dir = 1; dir < 3; dir++)
            {
                std::ranges::fill(cellMap, int16_t{-1});
                SetFillSector(sectorR, sectorC);
                const int32_t minCellR = _MinTileR * MapCellDim;
                const int32_t maxCellR = _MaxTileR * MapCellDim;
                const int32_t minCellC = _MinTileC * MapCellDim;
                const int32_t maxCellC = _MaxTileC * MapCellDim;
                auto cell = [&](int32_t cellRow, int32_t cellCol) -> int16_t&
                { return cellMap[static_cast<size_t>((cellRow - minCellR) * cellMapSide + (cellCol - minCellC))]; };
                auto areaAt = [this](int32_t row, int32_t col)
                { return AreaMap[static_cast<size_t>(Width * row + col)]; };

                for (int32_t row = _MinTileR; row < _MaxTileR; row++)
                {
                    for (int32_t col = _MinTileC; col < _MaxTileC; col++)
                    {
                        const int32_t area = areaAt(row, col);

                        if (area < 0)
                        {
                            continue;
                        }

                        const int32_t nextRow = AdjTile[static_cast<size_t>(dir)][0] + row;
                        const int32_t nextCol = AdjTile[static_cast<size_t>(dir)][1] + col;

                        if (nextRow < 0 || nextRow >= Height || nextCol < 0 || nextCol >= Width)
                        {
                            continue;
                        }

                        const int16_t nextArea = areaAt(nextRow, nextCol);

                        if (nextArea < 0 || area == nextArea)
                        {
                            continue;
                        }

                        // Bridges only join areas along their own direction.
                        const int32_t type = Areas[static_cast<size_t>(area)].Type;
                        const int32_t nextType = Areas[static_cast<size_t>(nextArea)].Type;
                        const int32_t crossType = dir == 1 ? 2 : 1;

                        if ((type != 0 && type != crossType) || (nextType != 0 && nextType != crossType))
                        {
                            continue;
                        }

                        const MCMapTile& tile = map.TileAt(row, col);
                        const MCMapTile& nextTile = map.TileAt(nextRow, nextCol);
                        const int32_t baseRow = row * MapCellDim;
                        const int32_t baseCol = col * MapCellDim;

                        for (int32_t i = 0; i < MapCellDim; i++)
                        {
                            if (dir == 1)
                            {
                                if (tile.GetCellPassable(i, 2) != 0 && nextTile.GetCellPassable(i, 0) != 0)
                                {
                                    cell(baseRow + i, baseCol + 2) = nextArea;
                                }
                            }
                            else
                            {
                                if (tile.GetCellPassable(2, i) != 0 && nextTile.GetCellPassable(0, i) != 0)
                                {
                                    cell(baseRow + 2, baseCol + i) = nextArea;
                                }
                            }
                        }
                    }
                }

                if (dir == 1)
                {
                    // Runs down each cell column, from the east edge.
                    for (int32_t cellCol = maxCellC - 1; cellCol >= minCellC; cellCol--)
                    {
                        int32_t cellRow = minCellR;

                        while (cellRow < maxCellR)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellRow++;
                                continue;
                            }

                            const int32_t tileC = cellCol / 3;
                            const int32_t area = areaAt(cellRow / 3, tileC);
                            int32_t length = 0;

                            while (cellRow < maxCellR && areaAt(cellRow / 3, tileC) == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellRow++;
                            }

                            AddDoor(buildList, area, nextArea, (cellRow - length) / 3, tileC, (cellRow - length) % 3,
                                    cellCol % 3, length, 1);
                        }
                    }
                }
                else
                {
                    // Runs along each cell row, from the south edge.
                    for (int32_t cellRow = maxCellR - 1; cellRow >= minCellR; cellRow--)
                    {
                        int32_t cellCol = minCellC;

                        while (cellCol < maxCellC)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellCol++;
                                continue;
                            }

                            const int32_t tileR = cellRow / 3;
                            const int32_t area = areaAt(tileR, cellCol / 3);
                            int32_t length = 0;

                            while (cellCol < maxCellC && areaAt(tileR, cellCol / 3) == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellCol++;
                            }

                            AddDoor(buildList, area, nextArea, tileR, (cellCol - length) / 3, cellRow % 3,
                                    (cellCol - length) % 3, length, dir);
                        }
                    }
                }
            }
        }
    }

    NumDoors = static_cast<int32_t>(buildList.size());
    buildList.resize(buildList.size() + 2);
    Doors = std::move(buildList);
}

auto MCGlobalMap::CalcAreaDoors() -> void
{
    NumDoorInfos = 0;
    DoorInfos.clear();

    for (int32_t i = 0; i < NumAreas; i++)
    {
        MCGlobalMapArea& area = Areas[static_cast<size_t>(i)];
        area.FirstDoorInfo = static_cast<int32_t>(DoorInfos.size());
        int32_t count = 0;

        for (int32_t door = 0; door < NumDoors; door++)
        {
            const MCGlobalMapDoor& info = Doors[static_cast<size_t>(door)];

            if (info.Area[0] == i || info.Area[1] == i)
            {
                DoorInfos.push_back(MCDoorInfo{static_cast<int16_t>(door), static_cast<int8_t>(info.Area[1] == i)});
                count++;
            }
        }

        // The file stores the count in a byte.
        area.NumDoors = static_cast<int8_t>(count);
        NumDoorInfos += area.NumDoors;
    }
}

auto MCGlobalMap::CalcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor) -> int32_t
{
    if (CurPlanet == 1)
    {
        // Dirt roads (overlays 1..15) cost nothing to cross, at every move level.
        for (int32_t level = 0; level < NumMoveLevels; level++)
        {
            const auto first = OverlayWeightTable.begin() + level * OverlayWeightLevelSize + MapCellDim * MapCellDim;
            std::fill(first, first + 15 * MapCellDim * MapCellDim, 0);
        }
    }

    // The middle cell of a door, on the side facing thruArea.
    auto doorCell = [this, thruArea](int32_t doorIndex, int32_t& cellRow, int32_t& cellCol) -> bool
    {
        const MCGlobalMapDoor& door = Doors[static_cast<size_t>(doorIndex)];

        if (door.Area[0] != thruArea && door.Area[1] != thruArea)
        {
            return false;
        }

        const int32_t side = door.Area[1] == thruArea ? 1 : 0;
        const bool alongRows = door.Direction[0] == 1;
        cellCol = door.Col * 3 + door.CellC + (alongRows ? side : door.Length / 2);
        cellRow = door.CellR + door.Row * 3 + (alongRows ? door.Length / 2 : side);
        return true;
    };

    int32_t startRow = 0;
    int32_t startCol = 0;

    if (!doorCell(startDoor, startRow, startCol))
    {
        return -1;
    }

    int32_t goalRow = 0;
    int32_t goalCol = 0;

    if (!doorCell(goalDoor, goalRow, goalCol))
    {
        return -2;
    }

    const MCVector3D goalPos = MapCellCentre(goalRow, goalCol);
    MCMovePath path;
    path.Clear();
    const MCGlobalMapArea& area = Areas[static_cast<size_t>(thruArea)];
    const int32_t uLr = area.SectorR * SectorDim;
    const int32_t uLc = area.SectorC * SectorDim;
    _PathFinder->ClearBridgeTiles = true;
    _PathFinder->SetUp(*_ScenarioMap, uLr, uLc, SectorDim, SectorDim, nullptr, (startRow / 3 - uLr) * 3 + startRow % 3,
                       (startCol / 3 - uLc) * 3 + startCol % 3, goalPos, (goalRow / 3 - uLr) * 3 + goalRow % 3,
                       (goalCol / 3 - uLc) * 3 + goalCol % 3, nullptr, 10, 0, 8, 0);
    int32_t goalCell[2] = {};
    _PathFinder->CalcPath(&path, nullptr, goalCell);
    _PathFinder->ClearBridgeTiles = false;
    return path.NumSteps == 0 ? 9999 : path.Cost;
}

auto MCGlobalMap::CalcDoorLinks() -> void
{
    int32_t maxAreaDoors = 0;
    NumDoorLinks = 0;
    DoorLinks.clear();

    for (int32_t doorIndex = 0; doorIndex < NumDoors; doorIndex++)
    {
        for (int32_t side = 0; side < 2; side++)
        {
            const int32_t area = Doors[static_cast<size_t>(doorIndex)].Area[static_cast<size_t>(side)];
            const int32_t areaDoors = Areas[static_cast<size_t>(area)].NumDoors;
            const int8_t numLinks = static_cast<int8_t>(areaDoors - 1);
            Doors[static_cast<size_t>(doorIndex)].NumLinks[static_cast<size_t>(side)] = numLinks;
            Doors[static_cast<size_t>(doorIndex)].FirstLink[static_cast<size_t>(side)] =
                static_cast<int32_t>(DoorLinks.size());
            size_t link = DoorLinks.size();
            DoorLinks.resize(DoorLinks.size() + static_cast<size_t>(numLinks + 2));
            NumDoorLinks += numLinks + 2;

            for (const MCDoorInfo& info : AreaDoors(area))
            {
                const int16_t otherIndex = info.DoorIndex;

                if (otherIndex == doorIndex)
                {
                    continue;
                }

                const int8_t otherSide = Doors[static_cast<size_t>(otherIndex)].Area[1] == area ? 1 : 0;
                DoorLinks[link++] = MCDoorLink{otherIndex, otherSide, CalcLinkCost(doorIndex, area, otherIndex)};
            }

            maxAreaDoors = std::max(maxAreaDoors, areaDoors);
        }
    }

    // The temporary start and goal doors link out of any area, so they get room for the most doors an area has.
    for (int32_t doorIndex = NumDoors; doorIndex < NumDoors + 2; doorIndex++)
    {
        MCGlobalMapDoor& door = Doors[static_cast<size_t>(doorIndex)];
        door.NumLinks = {static_cast<int8_t>(maxAreaDoors), 0};

        for (size_t side = 0; side < 2; side++)
        {
            door.FirstLink[side] = static_cast<int32_t>(DoorLinks.size());
            DoorLinks.resize(DoorLinks.size() + static_cast<size_t>(door.NumLinks[side] + 2));
            NumDoorLinks += door.NumLinks[side] + 2;
        }
    }
}

auto MCGlobalMap::CalcPathCostTable() -> void
{
    PathCostTable.assign(static_cast<size_t>(NumAreas * NumAreas), 0);
    std::array<MCGlobalPathStep, MaxPathSteps> path{};

    for (int32_t startArea = 0; startArea < NumAreas; startArea++)
    {
        for (int32_t goalArea = 0; goalArea < NumAreas; goalArea++)
        {
            const size_t entry = static_cast<size_t>(startArea * NumAreas + goalArea);
            PathCostTable[entry] =
                startArea == goalArea ? 0 : static_cast<uint8_t>(CalcPath(startArea, goalArea, path));
        }
    }
}

auto MCGlobalMap::ExitDirection(int32_t doorIndex, int32_t fromArea) const -> int32_t
{
    const MCGlobalMapDoor& door = Doors[static_cast<size_t>(doorIndex)];

    if (door.Area[0] == fromArea)
    {
        return door.Direction[0];
    }

    if (door.Area[1] == fromArea)
    {
        return door.Direction[1];
    }

    return -1;
}

auto MCGlobalMap::GetDoorWorldPos(const int32_t* prevGoalCell) const -> MCVector3D
{
    MCVector3D position = MapCellCentre(prevGoalCell[0], prevGoalCell[1]);
    position.Z = GameMap()->GetTerrainElevation(MCVector3D(position.X, position.Y, 0.0f));
    return position;
}

auto MCGlobalMap::LinkTempDoor(int32_t tempDoor, int32_t areaIndex) -> void
{
    const std::span<MCDoorLink> links = DoorSideLinks(tempDoor, 0);
    const std::span<const MCDoorInfo> doors = AreaDoors(areaIndex);
    const int32_t numLinks = Doors[static_cast<size_t>(tempDoor)].NumLinks[0];

    for (int32_t i = 0; i < numLinks; i++)
    {
        const MCDoorInfo& info = doors[static_cast<size_t>(i)];
        links[static_cast<size_t>(i)] = MCDoorLink{info.DoorIndex, info.DoorSide, 1};
        Assert(info.DoorIndex >= 0 && info.DoorIndex < NumDoors + 2, static_cast<uint32_t>(info.DoorIndex),
               " GlobalMap.setGoalDoor: bad doorIndex ");

        // The area's door links back to the temporary door, in the spare room past its links.
        MCGlobalMapDoor& areaDoor = Doors[static_cast<size_t>(info.DoorIndex)];
        const size_t side = static_cast<size_t>(info.DoorSide);
        DoorSideLinks(info.DoorIndex, info.DoorSide)[static_cast<size_t>(areaDoor.NumLinks[side])] =
            MCDoorLink{static_cast<int16_t>(tempDoor), 0, 1};
        areaDoor.NumLinks[side]++;
    }
}

auto MCGlobalMap::SetStartDoor(int32_t startArea) -> void
{
    MCGlobalMapDoor& startDoor = Doors[static_cast<size_t>(NumDoors)];
    InitTempDoor(startDoor, startArea, Areas[static_cast<size_t>(startArea)].NumDoors);
    startDoor.FromAreaIndex = 1;
    LinkTempDoor(NumDoors, startArea);
}

auto MCGlobalMap::ResetStartDoor(int32_t startArea) -> void
{
    const int32_t numLinks = Doors[static_cast<size_t>(NumDoors)].NumLinks[0];
    const std::span<const MCDoorInfo> doors = AreaDoors(startArea);

    for (int32_t i = 0; i < numLinks; i++)
    {
        const MCDoorInfo& info = doors[static_cast<size_t>(i)];
        Doors[static_cast<size_t>(info.DoorIndex)].NumLinks[static_cast<size_t>(info.DoorSide)]--;
    }
}

auto MCGlobalMap::SetGoalDoor(int32_t goalArea) -> void
{
    if (goalArea < 0 || goalArea >= NumAreas)
    {
        Fatal(0, std::format(" GlobalMap.setGoalDoor: bad goalArea ({} of {}) ", goalArea, NumAreas));
    }

    const MCGlobalMapArea& area = Areas[static_cast<size_t>(goalArea)];
    GoalSectorR = area.SectorR;
    GoalSectorC = area.SectorC;
    InitTempDoor(Doors[static_cast<size_t>(NumDoors + 1)], goalArea, area.NumDoors);
    LinkTempDoor(NumDoors + 1, goalArea);
}

auto MCGlobalMap::ResetGoalDoor(int32_t goalArea) -> void
{
    const int32_t numLinks = Doors[static_cast<size_t>(NumDoors + 1)].NumLinks[0];
    const std::span<const MCDoorInfo> doors = AreaDoors(goalArea);

    for (int32_t i = 0; i < numLinks; i++)
    {
        const MCDoorInfo& info = doors[static_cast<size_t>(i)];
        Doors[static_cast<size_t>(info.DoorIndex)].NumLinks[static_cast<size_t>(info.DoorSide)]--;
    }
}

auto MCGlobalMap::CalcHPrime(int32_t door) const -> int32_t
{
    Assert(door >= 0 && door < NumDoors + 2, 0xffffffff, " CalcHPrime: Bad Door ");
    const MCGlobalMapDoor& info = Doors[static_cast<size_t>(door)];
    const MCGlobalMapArea& area0 = Areas[static_cast<size_t>(info.Area[0])];
    const MCGlobalMapArea& area1 = Areas[static_cast<size_t>(info.Area[1])];
    const int32_t sectorR = (area1.SectorR + area0.SectorR) / 2;
    const int32_t sectorC = (area0.SectorC + area1.SectorC) / 2;
    return std::abs(GoalSectorR - sectorR) + std::abs(GoalSectorC - sectorC);
}

auto MCGlobalMap::CalcPath(int32_t startArea, int32_t goalArea, std::span<MCGlobalPathStep> path) -> int32_t
{
    if (startArea == -1 || goalArea == -1)
    {
        return -1;
    }

    // Each door goes on the open list at most once.
    _OpenList->Reserve(NumDoors + 2);
    const int32_t startDoor = NumDoors;
    const int32_t goalDoor = NumDoors + 1;

    for (MCGlobalMapDoor& door : Doors)
    {
        door.Cost = 1;
        door.Parent = -1;
        door.FromAreaIndex = -1;
        door.Flags = 0;
        door.G = 0;
        door.HPrime = -1;
        door.FPrime = 0;
    }

    SetStartDoor(startArea);
    SetGoalDoor(goalArea);
    _OpenList->Clear();
    _OpenList->Insert(MCPQNode{.Key = 0, .Id = startDoor});
    Doors[static_cast<size_t>(startDoor)].Flags |= 1;
    bool goalFound = false;

    while (!_OpenList->IsEmpty())
    {
        const int32_t curIndex = _OpenList->Pop().Id;
        MCGlobalMapDoor& current = Doors[static_cast<size_t>(curIndex)];
        const int32_t g = current.G;
        current.Flags = (current.Flags & ~1u) | 2;

        if (curIndex == goalDoor)
        {
            goalFound = true;
            break;
        }

        const int32_t side = 1 - current.FromAreaIndex;
        const int32_t thruArea = current.Area[static_cast<size_t>(side)];
        const int32_t numLinks = current.NumLinks[static_cast<size_t>(side)];
        const std::span<const MCDoorLink> links = DoorSideLinks(curIndex, side);

        for (int32_t i = 0; i < numLinks; i++)
        {
            const MCDoorLink& link = links[static_cast<size_t>(i)];
            const int32_t succIndex = link.DoorIndex;
            Assert(succIndex >= 0 && succIndex < NumDoors + 2, 0, " Bad Door Index ");
            const int32_t linkCost = link.Cost;
            MCGlobalMapDoor& successor = Doors[static_cast<size_t>(succIndex)];

            if (successor.Open == 0 || linkCost >= 10000)
            {
                continue;
            }

            if (successor.HPrime == -1)
            {
                successor.HPrime = CalcHPrime(succIndex);
            }

            const int32_t newG = g + linkCost;
            const int32_t succSide = successor.Area[1] == thruArea ? 1 : 0;

            if ((successor.Flags & 1) == 0)
            {
                if ((successor.Flags & 2) == 0)
                {
                    successor.FromAreaIndex = succSide;
                    successor.Parent = curIndex;
                    successor.G = newG;
                    successor.FPrime = newG + successor.HPrime;
                    successor.Cost = linkCost;
                    _OpenList->Insert(MCPQNode{.Key = successor.FPrime, .Id = succIndex});
                    successor.Flags |= 1;
                }
                else if (newG < successor.G)
                {
                    // A cheaper way to a closed door: reparent it and push the saving on.
                    successor.Cost = linkCost;
                    successor.Parent = curIndex;
                    successor.FromAreaIndex = succSide;
                    PropogateCost(succIndex, linkCost, succSide, g);
                }
            }
            else if (newG < successor.G)
            {
                successor.FromAreaIndex = succSide;
                successor.Cost = linkCost;
                successor.FPrime = successor.HPrime + newG;
                successor.Parent = curIndex;
                successor.G = newG;
                const int32_t itemIndex = _OpenList->Find(succIndex);

                if (itemIndex == 0)
                {
                    DebugOpenList(*_OpenList,
                                  std::format("GlobalMap.calcPath: Cannot find globalmap door [{}, {}, {}, {}] for "
                                              "change\n",
                                              succIndex, i, succSide, linkCost));
                    Fatal(0, "GlobalMap.calcPath: Save OPENLIST.DBG file for Glenn!");
                }

                _OpenList->Change(itemIndex, successor.FPrime);
            }
        }
    }

    ResetStartDoor(startArea);
    ResetGoalDoor(goalArea);

    if (!goalFound)
    {
        return 0;
    }

    int32_t count = 1;

    for (int32_t door = goalDoor; door != startDoor; door = Doors[static_cast<size_t>(door)].Parent)
    {
        count++;
    }

    const int32_t numSteps = count - 1;
    Assert(numSteps < static_cast<int32_t>(path.size()), static_cast<uint32_t>(numSteps),
           " Too Many Long Range Move Steps ");
    int32_t costToGoal = 0;
    int32_t door = goalDoor;

    for (int32_t i = numSteps - 1; i >= 0; i--)
    {
        const MCGlobalMapDoor& step = Doors[static_cast<size_t>(door)];

        // Port fix: the original writes past the caller's steps when the assert above fails.
        if (i < static_cast<int32_t>(path.size()))
        {
            path[static_cast<size_t>(i)].ThruArea = step.Area[static_cast<size_t>(step.FromAreaIndex)];
            path[static_cast<size_t>(i)].GoalDoor = door;
            path[static_cast<size_t>(i)].CostToGoal = costToGoal;
        }

        costToGoal += step.Cost;
        door = step.Parent;
    }

    if (!PathCostTable.empty())
    {
        uint8_t& entry = PathCostTable[static_cast<size_t>(NumAreas * startArea + goalArea)];

        if (entry != numSteps)
        {
            entry = count > 0xff ? 0xff : static_cast<uint8_t>(numSteps);
        }
    }

    return numSteps;
}

auto MCGlobalMap::PropogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g) -> void
{
    Assert(door >= 0 && door < NumDoors + 2 && (fromSide == 0 || fromSide == 1) && g >= 0, 0xffffffff,
           " Bad Door Propogate ");
    const int32_t newG = cost + g;
    MCGlobalMapDoor& current = Doors[static_cast<size_t>(door)];

    if (newG >= current.G)
    {
        return;
    }

    current.G = newG;
    current.FPrime = current.HPrime + newG;

    if ((current.Flags & 1) != 0)
    {
        if (_OpenList->Find(door) == 0)
        {
            DebugOpenList(*_OpenList,
                          std::format("GlobalMap.propogateCost: Cannot find globalmap door [{}, {}, {}, {}] for "
                                      "change\n",
                                      door, cost, fromSide, g));
            Fatal(0, "GlobalMap.propogateCost: Save OPENLIST.DBG file for Glenn!");
        }

        // Original behaviour (OB-025): passes the door number where PriorityQueue::change wants the heap index.
        _OpenList->Change(door, current.FPrime);
        return;
    }

    const int32_t side = 1 - fromSide;
    const int32_t numLinks = current.NumLinks[static_cast<size_t>(side)];
    const std::span<const MCDoorLink> links = DoorSideLinks(door, side);

    for (int32_t i = 0; i < numLinks; i++)
    {
        const MCDoorLink& link = links[static_cast<size_t>(i)];
        const int32_t nextIndex = link.DoorIndex;
        Assert(nextIndex >= 0 && nextIndex < NumDoors + 2, 0, " Bad Door Index ");
        const int32_t linkCost = link.Cost;
        MCGlobalMapDoor& next = Doors[static_cast<size_t>(nextIndex)];
        const int32_t nextSide = next.Area[1] == current.Area[static_cast<size_t>(side)] ? 1 : 0;

        if (next.Open == 0 || linkCost >= 10000 || next.HPrime == -1)
        {
            continue;
        }

        if (door == next.Parent)
        {
            // Original behaviour (OB-026): passes this door's exit side, not the next door's entry side.
            PropogateCost(nextIndex, linkCost, side, current.G);
        }
        else if (current.G + linkCost < next.G)
        {
            next.Cost = linkCost;
            next.Parent = door;
            next.FromAreaIndex = nextSide;
            PropogateCost(nextIndex, linkCost, nextSide, current.G);
        }
    }
}

auto MCGlobalMap::CalcPath(MCVector3D start, MCVector3D goal, std::span<MCGlobalPathStep> path) -> int32_t
{
    const MCScenarioMap* map = GameMap();
    int32_t startR = 0;
    int32_t startC = 0;
    map->WorldToMapTilePos(start, startR, startC);
    int32_t goalR = 0;
    int32_t goalC = 0;
    map->WorldToMapTilePos(goal, goalR, goalC);
    const int32_t goalArea = CalcArea(goalR, goalC);
    const int32_t startArea = CalcArea(startR, startC);
    return CalcPath(startArea, goalArea, path);
}

auto MCGlobalMap::GetPathCost(int32_t startArea, int32_t goalArea) const -> int32_t
{
    if (startArea < 0 || goalArea < 0)
    {
        return 0;
    }

    return PathCostTable[static_cast<size_t>(NumAreas * startArea + goalArea)];
}

auto MCGlobalMap::OpenDoor(int32_t door) -> void
{
    Doors[static_cast<size_t>(door)].Open = 1;
}

auto MCGlobalMap::CloseDoor(int32_t door) -> void
{
    Doors[static_cast<size_t>(door)].Open = 0;
}

auto MCGlobalMap::CloseArea(int32_t area) -> void
{
    Areas[static_cast<size_t>(area)].Closed = 1;

    for (const MCDoorInfo& info : AreaDoors(area))
    {
        CloseDoor(info.DoorIndex);
    }

    for (int32_t i = 0; i < NumAreas; i++)
    {
        PathCostTable[static_cast<size_t>(NumAreas * i + area)] = 0;
        PathCostTable[static_cast<size_t>(NumAreas * area + i)] = 0;
    }
}

auto MCGlobalMap::Print(std::string_view fileName, int32_t uLr, int32_t uLc, int32_t printHeight,
                        int32_t printWidth) const -> void
{
    // Port fix: the original tests the other way round (it prints only when areaMap is null, and then reads through
    // the null pointer).
    if (AreaMap.empty())
    {
        return;
    }

    MCFile debugFile;
    debugFile.Create(fileName);
    debugFile.WriteString(std::format("ULr: {}, ULc: {}, h: {}, w: {}\n", uLr, uLc, printHeight, printWidth));

    for (int32_t row = uLr; row < uLr + printHeight; row++)
    {
        std::string line;

        for (int32_t col = uLc; col < uLc + printWidth; col++)
        {
            const int16_t area = AreaMap[static_cast<size_t>(Width * row + col)];

            if (area == -2)
            {
                line += ">< ";
            }
            else if (area == -1)
            {
                line += "** ";
            }
            else
            {
                line += std::format("{:02x} ", area);
            }
        }

        line += "\n";
        debugFile.WriteString(line);
    }

    debugFile.WriteString("\n");
    debugFile.Close();
}

auto MCGlobalMap::CalcArea(int32_t tileR, int32_t tileC) const -> int32_t
{
    // Port fix: the original reads outside the area map for a goal off the map. Off the map is in no area.
    if (tileR < 0 || tileR >= Height || tileC < 0 || tileC >= Width)
    {
        return -1;
    }

    const size_t index = static_cast<size_t>(Width * tileR + tileC);

    if (SmallAreaMap.empty())
    {
        const int32_t area = AreaMap[index];
        return area < 0 ? -1 : area;
    }

    const int32_t area = SmallAreaMap[index];
    return area == 0xff ? -1 : area;
}
