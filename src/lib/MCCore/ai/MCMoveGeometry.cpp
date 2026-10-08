#include "stdafx.h"
#include "ai/MCMoveGeometry.h"
#include "ai/MCMoveSystem.h"
#include "lib/MCVector2D.h"
#include "main/main.h"
#include "object/MCBigGameObject.h"

std::array<int32_t, NumMoveLevels * OverlayWeightLevelSize> OverlayWeightTable{};
int32_t CurPlanet = 0;
int32_t SimpleMovePathRange = 7;

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DegreesToRadians = 0x1.1df46a2526c7ap-6;

    /// <summary>First overlay type of the gates.</summary>
    constexpr uint32_t FirstGateOverlay = 67;
    /// <summary>Last overlay type of the gates.</summary>
    constexpr uint32_t LastGateOverlay = 74;

    /// <summary>
    /// Per team alignment + 1 and gate overlay (67..74), the overlay the gate behaves as for that team, or -1 when
    /// it is closed to it.
    /// </summary>
    constexpr int32_t GateOverlayForAlignment[3][8] = {
        {71, 72, 73, 74, 67, 68, 69, 70},
        {-1, -1, -1, -1, -1, -1, -1, -1},
        {67, 68, 69, 70, 71, 72, 73, 74},
    };
}

auto GateOverlay(uint32_t overlay, int32_t alignment) -> int32_t
{
    if (overlay >= FirstGateOverlay && overlay <= LastGateOverlay)
    {
        return GateOverlayForAlignment[alignment + 1][overlay - FirstGateOverlay];
    }

    return static_cast<int32_t>(overlay);
}

auto WorldCoordToMapCoord(MCVector3D pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC) -> void
{
    WorldCoordToMapTile(pos, tileR, tileC);
    cellC = static_cast<int32_t>((static_cast<double>(pos.X) - TileColToWorld(tileC)) / MetersPerCell());
    cellR = static_cast<int32_t>((static_cast<double>(TileRowToWorld(tileR)) - pos.Y) / MetersPerCell());
}

auto WorldCoordToMapTile(MCVector3D pos, int32_t& tileR, int32_t& tileC) -> void
{
    tileC =
        static_cast<int32_t>(static_cast<double>(MCTerrain::OneOvermetersPerVertex) * pos.X + VerticesMapSideDivTwo());
    tileR =
        static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo()) - pos.Y) * MCTerrain::OneOvermetersPerVertex);
}

auto WorldCoordToMapCell(MCVector3D pos, int32_t& cellR, int32_t& cellC) -> void
{
    cellC = static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo()) + pos.X) /
                                 MCTerrain::MetersPerVertexDivMapcellDim);
    cellR = static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo()) - pos.Y) /
                                 MCTerrain::MetersPerVertexDivMapcellDim);
}

auto MapTileCellToWorldPos(int32_t tileR, int32_t tileC, int32_t cellR, int32_t cellC) -> MCVector3D
{
    const float x =
        static_cast<float>(static_cast<double>(TileColToWorld(tileC)) + CellOffsetToWorld(cellC) + HalfMapCell());
    const float y =
        static_cast<float>(static_cast<double>(TileRowToWorld(tileR)) - CellOffsetToWorld(cellR) - HalfMapCell());
    return MCVector3D(x, y, 0.0f);
}

auto MapCellToWorldPos(int32_t cellR, int32_t cellC) -> MCVector3D
{
    return MCVector3D(HalfMapCell() + CellColToWorld(cellC), CellRowToWorld(cellR) - HalfMapCell(), 0.0f);
}

auto RelativePositionToPoint(MCVector3D pos, float angle, float distance, uint32_t flags) -> MCVector3D
{
    const MCScenarioMap* map = GameMap();
    const bool reverse = (flags & 2) != 0;
    const double radians = angle * DegreesToRadians;
    const float reach = -(WorldUnitsPerMeter * distance);
    const float pointX = (static_cast<float>(std::sin(radians)) + 0.0f) * reach + pos.X;
    const float pointY = static_cast<float>(std::cos(radians) * reach) + pos.Y;

    // Walk from start toward end: from the point back toward pos, or (reverse) from pos out to the point.
    const MCVector2D start = reverse ? MCVector2D(pos.X, pos.Y) : MCVector2D(pointX, pointY);
    const MCVector2D end = reverse ? MCVector2D(pointX, pointY) : MCVector2D(pos.X, pos.Y);
    float stepX = end.X - start.X;
    float stepY = end.Y - start.Y;
    const float length = std::sqrt(stepX * stepX + stepY * stepY);

    if (length != 0.0f)
    {
        stepX = stepX / length;
        stepY = stepY / length;
    }

    const float stepLength = MCTerrain::MetersPerVertex * (1.0f / 3.0f) * 0.5f;
    stepX = stepX * stepLength;
    stepY = stepY * stepLength;

    if (std::sqrt(stepX * stepX + stepY * stepY) == 0.0f)
    {
        return MCVector3D(pos.X, pos.Y, 0.0f);
    }

    const MCVector2D span = end - start;
    const float totalDistance = std::sqrt(span.Y * span.Y + span.X * span.X);
    MCVector2D current = start;
    MCVector2D result = start;
    float travelled = 0.0f;

    // Port fix: the point can be off the map, where the original reads outside it. Off the map is impassable.
    auto passableAt = [&]() { return map->CellPassable(MCVector3D(current.X, current.Y, 0.0f)); };
    bool passable = passableAt();

    // Original behaviour (OB-032): the result trails the walk by a step, the last point before the one that ended
    // it (so walking in from an impassable point, the result is still impassable).
    while ((reverse ? passable : !passable) && travelled < totalDistance)
    {
        result = current;
        current.X = stepX + current.X;
        current.Y = stepY + current.Y;
        travelled =
            std::sqrt((current.X - start.X) * (current.X - start.X) + (current.Y - start.Y) * (current.Y - start.Y));
        passable = passableAt();
    }

    const float limit = WorldUnitsMapSide * 0.5f - MCTerrain::MetersPerVertex;
    result.X = std::clamp(result.X, -limit, limit);
    result.Y = std::clamp(result.Y, -limit, limit);
    const float elevation = map->GetTerrainElevation(MCVector3D(result.X, result.Y, 0.0f));
    return MCVector3D(result.X, result.Y, elevation);
}

auto CalcTileTypeFromIndex(int32_t tileIndex) -> int32_t
{
    // Forty bands of 79 texture indices (types 1..40), then the smaller special bands.
    for (int32_t band = 1; band <= 40; band++)
    {
        if (tileIndex < band * 0x4f)
        {
            return band;
        }
    }

    static constexpr struct
    {
        int32_t Limit = 0;
        int32_t Type = 0;
    } bands[] = {
        {0xc6e, 0x29}, {0xc84, 0x2a}, {0xc88, 0x2b}, {0xc96, 0x2c}, {0xca4, 0x2d}, {0xcb0, 0x2e},
        {0xcbc, 0x2f}, {0xcc8, 0x30}, {0xcd4, 0x31}, {0xce0, 0x32}, {0xcec, 0x33}, {0xcf4, 0x34},
        {0xcfc, 0x35}, {0xd15, 0x3a}, {0xd22, 0x36}, {0xd30, 0x38}, {0xd3e, 0x37},
    };

    for (const auto& band : bands)
    {
        if (tileIndex < band.Limit)
        {
            return band.Type;
        }
    }

    return tileIndex > 0xd65 ? 0 : 2;
}

auto CalcOverlayTypeFromIndex(int32_t overlayIndex) -> int32_t
{
    if (overlayIndex == 0x29)
    {
        return 0;
    }

    if ((overlayIndex > 0xd01 && overlayIndex < 0xd0a) || (overlayIndex > 0xe7d && overlayIndex < 0xe82))
    {
        return 0x3b;
    }

    if ((overlayIndex > 0xd09 && overlayIndex < 0xd0e) || (overlayIndex > 0xd65 && overlayIndex < 0xd6e))
    {
        return 0x3e;
    }

    if ((overlayIndex > 0xd0d && overlayIndex < 0xd12) || (overlayIndex > 0xd6d && overlayIndex < 0xd76))
    {
        return 0x3f;
    }

    if (overlayIndex < 0xd51 || (overlayIndex > 0xda7 && overlayIndex < 0xdbc))
    {
        return 0x3c;
    }

    if (overlayIndex < 0xd64 || (overlayIndex > 0xdbb && overlayIndex < 0xdce))
    {
        return 0x3d;
    }

    if (overlayIndex < 0xd82)
    {
        return 0x42;
    }

    if (overlayIndex < 0xd95)
    {
        return 0x40;
    }

    if (overlayIndex < 0xda8)
    {
        return 0x41;
    }

    if (overlayIndex < 0xdcd)
    {
        return 0;
    }

    if (overlayIndex < 0xde1)
    {
        if (overlayIndex < 0xdd1)
        {
            return 1;
        }

        if (overlayIndex < 0xdd4)
        {
            return 2;
        }

        return overlayIndex - 0xdd1;
    }

    if (overlayIndex < 0xdf4)
    {
        if (overlayIndex < 0xde4)
        {
            return 0x10;
        }

        if (overlayIndex < 0xde7)
        {
            return 0x11;
        }
    }
    else if (overlayIndex > 0xdf7)
    {
        if (overlayIndex < 0xe07)
        {
            return 0x25;
        }

        if (overlayIndex < 0xe16)
        {
            return 0x26;
        }

        if (overlayIndex < 0xe25)
        {
            return 0x27;
        }

        if (overlayIndex < 0xe34)
        {
            return 0x28;
        }

        if (overlayIndex < 0xe42)
        {
            return overlayIndex - 0xe0b;
        }

        if (overlayIndex < 0xe51)
        {
            return 0x37;
        }

        if (overlayIndex < 0xe60)
        {
            return 0x38;
        }

        if (overlayIndex < 0xe6f)
        {
            return 0x39;
        }

        return overlayIndex > 0xe7d ? 0 : 0x3a;
    }

    return overlayIndex - 0xdd5;
}

auto CellFacing(MCGameObject* object) -> int32_t
{
    if (object == nullptr)
    {
        return 0;
    }

    MCVector3D ahead = object->GetPosition();
    ahead.Y = static_cast<float>(static_cast<double>(ahead.Y) + 50.0);
    const float facing = object->RelFacingTo(ahead, -1);

    // Eight 45-degree sectors, from straight behind (-180) clockwise; the last half sector is behind again.
    static constexpr struct
    {
        float Limit = 0.0f;
        int32_t Direction = 0;
    } sectors[] = {{-157.5f, 4}, {-112.5f, 3}, {-67.5f, 2}, {-22.5f, 1},
                   {22.5f, 0},   {67.5f, 7},   {112.5f, 6}, {157.5f, 5}};

    for (const auto& sector : sectors)
    {
        if (facing < sector.Limit)
        {
            return sector.Direction;
        }
    }

    return 4;
}

auto CellDirToCell(int32_t fromTileR, int32_t fromTileC, int32_t fromCellR, int32_t fromCellC, int32_t toTileR,
                   int32_t toTileC, int32_t toCellR, int32_t toCellC) -> int32_t
{
    static constexpr int32_t deltaDir[3][3] = {{7, 0, 1}, {6, -1, 2}, {5, 4, 3}};
    const int32_t rowDelta = toTileR * MapCellDim + toCellR - fromTileR * MapCellDim - fromCellR + 1;
    const int32_t colDelta = toTileC * MapCellDim + toCellC - fromTileC * MapCellDim - fromCellC + 1;

    if (rowDelta < 0 || rowDelta > 2 || colDelta < 0 || colDelta > 2)
    {
        return -2;
    }

    const int32_t dir = deltaDir[rowDelta][colDelta];
    return dir == -1 ? -2 : dir;
}
