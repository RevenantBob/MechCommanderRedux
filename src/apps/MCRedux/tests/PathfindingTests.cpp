#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "ai/move.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "object/gameobj.h"
#include "object/object.h"
#include "object/objque.h"
#include "terrain/terrain.h"
#include <random>

/// <summary>
/// Mission 1's map (M0101) run through the game's own path finders. A planned path may only step onto cells the map
/// marks passable (forest and water are not), one cell at a time: a mech follows its path step by step, so a path
/// with a blocked or distant step walks it through terrain. The routes are planned the way MechWarrior::calcMovePath
/// does: within SimplePathTileRange tiles a local window; farther, the global map's door path, then one leg per area
/// (Mover::calcMovePath's sector windows).
/// </summary>
namespace
{
    /// <summary>A path step: a cell of the scenario map.</summary>
    struct Cell
    {
        int32_t Row = 0;
        int32_t Col = 0;
    };

    /// <summary>Why a planned route failed the checks, empty if it passed.</summary>
    struct RouteCheck
    {
        int32_t Legs = 0;
        int32_t Steps = 0;
        std::string Problem;
    };

    /// <summary>
    /// Loads M0101's scenario map and global map, the terrain constants and gamesys.fit's path finding settings,
    /// as Scenario::init does, once per run.
    /// </summary>
    class MissionMap
    {
    public:
        static MissionMap& Get()
        {
            static MissionMap map;
            return map;
        }

        bool Ready = false;
        std::string Error;

        /// <summary>The move level of a mech running at <see cref="RunSpeed"/> (Mover::calcMovePath's formula).</summary>
        int32_t MoveLevel = 0;

        /// <summary>Overlay weights of a mech (BattleMech's overlayWeightClass 1).</summary>
        int32_t* OverlayWeights = nullptr;

        uint32_t Passable(int32_t row, int32_t col) const
        {
            if (row < 0 || col < 0 || row >= GameMap->Height * MAPCELL_DIM || col >= GameMap->Width * MAPCELL_DIM)
            {
                return 0;
            }

            return GameMap->Map[(row / MAPCELL_DIM) * GameMap->Width + col / MAPCELL_DIM].GetCellPassable(
                row % MAPCELL_DIM, col % MAPCELL_DIM);
        }

        /// <summary>The world position of a cell's centre.</summary>
        static MCVector3D CellCentre(int32_t row, int32_t col)
        {
            const float x = (static_cast<float>(col) + 0.5f) * MetersPerCell - WorldUnitsMapSide * 0.5f;
            const float y = (WorldUnitsMapSide * 0.5f - static_cast<float>(row) * MetersPerCell) - MetersPerCell * 0.5f;
            return MCVector3D(x, y, 0.0f);
        }

        static Cell CellOf(MCVector3D position)
        {
            int32_t tileR = 0;
            int32_t tileC = 0;
            int32_t cellR = 0;
            int32_t cellC = 0;
            GameMap->WorldToMapPos(position, tileR, tileC, cellR, cellC);
            return Cell{tileR * MAPCELL_DIM + cellR, tileC * MAPCELL_DIM + cellC};
        }

    private:
        static constexpr float RunSpeed = 25.0f;

        MissionMap()
        {
            Error = Load();
            Ready = Error.empty();
        }

        std::string Load()
        {
            MCTestGame::OpenFastFiles();

            // data\terrain\m0101.fit [TerrainData].
            MCTerrain::VerticesBlockSide = 20;
            MCTerrain::BlocksMapSide = 6;
            MCTerrain::MetersPerVertex = 128.0f;
            MCTerrain::OneOvermetersPerVertex = 1.0f / MCTerrain::MetersPerVertex;
            MCTerrain::MetersBlockSide = static_cast<float>(MCTerrain::VerticesBlockSide) * MCTerrain::MetersPerVertex;
            MCTerrain::MetersPerVertexDivMapcellDim = MCTerrain::MetersPerVertex * (1.0f / 3.0f);
            WorldUnitsMapSide = static_cast<float>(MCTerrain::BlocksMapSide) * MCTerrain::MetersBlockSide;

            // data\missions\gamesys.fit [Pathfinding] (loadMoverGameSystem). MIS0101 has no [Planet] block, so the
            // overlay costs stay as read.
            MCFitIniFile gameSystem;

            if (gameSystem.Open("data\\missions\\gamesys.fit") != 0 || gameSystem.SeekBlock("Pathfinding") != 0)
            {
                return "gamesys.fit [Pathfinding] not found";
            }

            if (gameSystem.ReadIdLong("SimplePathTileRange", SimpleMovePathRange) != 0 ||
                gameSystem.ReadIdLongArray("OverlayCellCosts", OverlayWeightTable,
                                           NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE) != 0)
            {
                return "gamesys.fit path finding values missing";
            }

            gameSystem.Close();

            for (int32_t i = 0; i < NUM_OVERLAY_TYPES; i++)
            {
                OverlayWeightIndex[i] = i * MAPCELL_DIM * MAPCELL_DIM;
            }

            OverlayWeights = &OverlayWeightTable[1 * OVERLAY_WEIGHT_LEVEL_SIZE];

            GameMap = new MCScenarioMap;
            MCFile mapFile;

            if (mapFile.Open("data\\terrain\\m0101.dat") != 0)
            {
                return "m0101.dat not found";
            }

            GameMap->Init(&mapFile);
            mapFile.Close();

            PathFindMap = new MCMoveMap;
            PathFindMap->Init(SimpleMovePathRange * 2 + 1, SimpleMovePathRange * 2 + 1);

            GlobalMoveMap = new MCGlobalMap;
            MCFile globalFile;

            if (globalFile.Open("data\\terrain\\m0101.gmm") != 0)
            {
                return "m0101.gmm not found";
            }

            GlobalMoveMap->Init(&globalFile);
            globalFile.Close();

            // The mover the path finders ask about (class, alignment): a Clan mech. Never destroyed (its destructor
            // releases an object type through the object type manager, which the tests don't have).
            alignas(MCGameObject) static unsigned char moverStorage[sizeof(MCGameObject)];
            auto* mover = ::new (static_cast<void*>(moverStorage)) MCGameObject;
            mover->ObjectClass = BATTLEMECH;
            mover->Alignment = -1;
            MovingObject = mover;

            // No mechs stand on the map (MoveMap::markGoalCells and placeMovers walk these lists).
            static MCObjectQueueNode innerSphereMechs("innerSphereMechs");
            static MCObjectQueueNode clanMechs("clanMechs");
            InnerSphereMechList = &innerSphereMechs;
            ClanMechList = &clanMechs;

            MoveLevel = static_cast<int32_t>(static_cast<double>(MetersPerWorldUnit) *
                                             MCTerrain::MetersPerVertexDivMapcellDim / RunSpeed * 50.0);
            return {};
        }
    };

    /// <summary>
    /// Checks one leg: every step passable, each one cell from the one before (no jumps), the first one cell from
    /// <paramref name="start"/>.
    /// </summary>
    std::string CheckLeg(const MissionMap& map, const MCMovePath& path, int32_t numSteps, Cell start)
    {
        Cell previous = start;

        for (int32_t i = 0; i < numSteps; i++)
        {
            const MCPathStep& step = path.StepList[i];
            const Cell cell{step.TileR * MAPCELL_DIM + step.CellR, step.TileC * MAPCELL_DIM + step.CellC};
            const int32_t direction = static_cast<int8_t>(step.Direction);
            char text[200];

            if (map.Passable(cell.Row, cell.Col) == 0)
            {
                std::snprintf(text, sizeof(text), "step %d (%d,%d) is blocked", i, cell.Row, cell.Col);
                return text;
            }

            if (direction < 0 || direction > 7)
            {
                std::snprintf(text, sizeof(text), "step %d (%d,%d) has direction %d (a jump)", i, cell.Row, cell.Col,
                              direction);
                return text;
            }

            if (std::abs(cell.Row - previous.Row) > 1 || std::abs(cell.Col - previous.Col) > 1)
            {
                std::snprintf(text, sizeof(text), "step %d (%d,%d) is not next to (%d,%d)", i, cell.Row, cell.Col,
                              previous.Row, previous.Col);
                return text;
            }

            // The step's destination is its own cell's centre.
            const Cell destination = MissionMap::CellOf(step.Destination);

            if (destination.Row != cell.Row || destination.Col != cell.Col)
            {
                std::snprintf(text, sizeof(text), "step %d (%d,%d) heads for (%d,%d)", i, cell.Row, cell.Col,
                              destination.Row, destination.Col);
                return text;
            }

            previous = cell;
        }

        return {};
    }

    /// <summary>A local path within the SimplePathTileRange window (Mover::calcMovePath, path type 1).</summary>
    int32_t PlanLocal(const MissionMap& map, MCMovePath& path, MCVector3D start, MCVector3D goal)
    {
        int32_t startTileR = 0;
        int32_t startTileC = 0;
        int32_t startCellR = 0;
        int32_t startCellC = 0;
        GameMap->WorldToMapPos(start, startTileR, startTileC, startCellR, startCellC);
        int32_t goalTileR = 0;
        int32_t goalTileC = 0;
        int32_t goalCellR = 0;
        int32_t goalCellC = 0;
        GameMap->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
        path.Clear();
        const int32_t ULr = std::max(startTileR - SimpleMovePathRange, 0);
        const int32_t ULc = std::max(startTileC - SimpleMovePathRange, 0);
        const int32_t dim = SimpleMovePathRange * 2 + 1;
        PathFindMap->SetUp(GameMap, ULr, ULc, dim, dim, &start, (startTileR - ULr) * MAPCELL_DIM + startCellR,
                           (startTileC - ULc) * MAPCELL_DIM + startCellC, goal,
                           (goalTileR - ULr) * MAPCELL_DIM + goalCellR, (goalTileC - ULc) * MAPCELL_DIM + goalCellC,
                           map.OverlayWeights, map.MoveLevel, 0, 8, 0x80);
        int32_t goalCell[2] = {};
        return PathFindMap->CalcPath(&path, nullptr, goalCell);
    }

    /// <summary>
    /// Plans and checks a whole route as MechWarrior::calcMovePath walks it: a local path when the goal is within
    /// SimplePathTileRange tiles, else the global door path and one leg per area, each leg starting where the last
    /// one ended.
    /// </summary>
    RouteCheck PlanRoute(const MissionMap& map, MCVector3D start, MCVector3D goal)
    {
        RouteCheck check;
        static MCMovePath path{};
        const Cell startCell = MissionMap::CellOf(start);
        const Cell goalCell = MissionMap::CellOf(goal);
        const bool simple = std::abs(goalCell.Row / MAPCELL_DIM - startCell.Row / MAPCELL_DIM) <= SimpleMovePathRange &&
                            std::abs(goalCell.Col / MAPCELL_DIM - startCell.Col / MAPCELL_DIM) <= SimpleMovePathRange;

        if (simple)
        {
            const int32_t numSteps = PlanLocal(map, path, start, goal);
            check.Legs = numSteps > 0 ? 1 : 0;
            check.Steps = numSteps;
            check.Problem = CheckLeg(map, path, numSteps, startCell);
            return check;
        }

        const int32_t startArea = GlobalMoveMap->CalcArea(startCell.Row / MAPCELL_DIM, startCell.Col / MAPCELL_DIM);
        const int32_t goalArea = GlobalMoveMap->CalcArea(goalCell.Row / MAPCELL_DIM, goalCell.Col / MAPCELL_DIM);

        if (startArea < 0 || goalArea < 0)
        {
            return check;
        }

        static MCGlobalPathStep globalPath[MAX_GLOBAL_PATH];
        const int32_t numGlobalSteps = GlobalMoveMap->CalcPath(startArea, goalArea, globalPath);

        if (numGlobalSteps <= 0)
        {
            return check;
        }

        MCVector3D legStart = start;
        Cell legStartCell = startCell;

        for (int32_t step = 0; step < numGlobalSteps; step++)
        {
            MCGlobalPathStep& globalStep = globalPath[step];
            int32_t numSteps = 0;
            path.Clear();
            int32_t tileR = 0;
            int32_t tileC = 0;
            int32_t cellR = 0;
            int32_t cellC = 0;
            GameMap->WorldToMapPos(legStart, tileR, tileC, cellR, cellC);
            const int32_t sectorDim = GlobalMoveMap->SectorDim;

            if (step < numGlobalSteps - 1)
            {
                // Mover::calcMovePath (door leg): the sector of the area crossed.
                const MCGlobalMapArea& area = GlobalMoveMap->Areas[globalStep.ThruArea];
                const int32_t ULr = area.SectorR * sectorDim;
                const int32_t ULc = area.SectorC * sectorDim;

                if (PathFindMap->SetUp(GameMap, ULr, ULc, sectorDim, sectorDim, &legStart,
                                       (tileR - ULr) * MAPCELL_DIM + cellR, (tileC - ULc) * MAPCELL_DIM + cellC,
                                       globalStep.ThruArea, globalStep.GoalDoor, goal, map.OverlayWeights,
                                       map.MoveLevel, 0, 8, 0x80) == -1)
                {
                    check.Problem = "leg " + std::to_string(step) + ": door setUp failed";
                    return check;
                }

                MCVector3D legGoal;
                numSteps = PathFindMap->CalcPath(&path, &legGoal, globalStep.GoalCell);
            }
            else
            {
                // Mover::calcMovePath (path type 2): the start's sector, to the final goal.
                const int32_t ULr = (tileR / sectorDim) * sectorDim;
                const int32_t ULc = (tileC / sectorDim) * sectorDim;
                int32_t goalTileR = 0;
                int32_t goalTileC = 0;
                int32_t goalCellR = 0;
                int32_t goalCellC = 0;
                GameMap->WorldToMapPos(goal, goalTileR, goalTileC, goalCellR, goalCellC);
                PathFindMap->SetUp(
                    GameMap, ULr, ULc, sectorDim, sectorDim, &legStart, (tileR - ULr) * MAPCELL_DIM + cellR,
                    (tileC - ULc) * MAPCELL_DIM + cellC, goal, (goalTileR - ULr) * MAPCELL_DIM + goalCellR,
                    (goalTileC - ULc) * MAPCELL_DIM + goalCellC, map.OverlayWeights, map.MoveLevel, 0, 8, 0x80);
                numSteps = PathFindMap->CalcPath(&path, nullptr, globalStep.GoalCell);
            }

            if (numSteps < 1)
            {
                // MechWarrior::calcMovePath stops here (alarm -8): the mech stands.
                return check;
            }

            check.Legs++;
            check.Steps += numSteps;
            const std::string problem = CheckLeg(map, path, numSteps, legStartCell);

            if (!problem.empty())
            {
                check.Problem =
                    "leg " + std::to_string(step) + " of " + std::to_string(numGlobalSteps) + ": " + problem;
                return check;
            }

            // The next leg starts at the cell this one ended in (GlobalMap::getDoorWorldPos).
            legStart = GlobalMoveMap->GetDoorWorldPos(-1, -1, globalStep.GoalCell);
            legStartCell = Cell{globalStep.GoalCell[0], globalStep.GoalCell[1]};
            const MCPathStep& last = path.StepList[numSteps - 1];
            const Cell lastCell{last.TileR * MAPCELL_DIM + last.CellR, last.TileC * MAPCELL_DIM + last.CellC};

            if (step < numGlobalSteps - 1 && (lastCell.Row != legStartCell.Row || lastCell.Col != legStartCell.Col))
            {
                check.Problem = "leg " + std::to_string(step) + " ends at (" + std::to_string(lastCell.Row) + "," +
                                std::to_string(lastCell.Col) + ") but the next starts at (" +
                                std::to_string(legStartCell.Row) + "," + std::to_string(legStartCell.Col) + ")";
                return check;
            }
        }

        return check;
    }

    /// <summary>Whether the map loaded; reports why not.</summary>
    bool MapReady()
    {
        if (!MCTestGame::Available())
        {
            return false;
        }

        MissionMap& map = MissionMap::Get();

        if (!map.Ready)
        {
            std::cout << "  map not loaded: " << map.Error << "\n";
        }

        return map.Ready;
    }
}

TEST_CASE("game: pathfinding mission 1's forest and water are blocked cells")
{
    if (!MapReady())
    {
        return;
    }

    const MissionMap& map = MissionMap::Get();
    REQUIRE_EQ(GameMap->Width, 120);
    REQUIRE_EQ(GlobalMoveMap->SectorDim, 10);

    // The Uller's start (MIS0101 Part1) is open ground.
    const Cell uller = MissionMap::CellOf(MCVector3D(2834.0f, 2790.0f, 0.0f));
    CHECK_EQ(map.Passable(uller.Row, uller.Col), 1u);

    // Every global map door is a run of passable cells (a door the leg paths aim at).
    for (int32_t door = 0; door < GlobalMoveMap->NumDoors; door++)
    {
        const MCGlobalMapDoor& d = GlobalMoveMap->Doors[door];
        const int32_t row = d.Row * MAPCELL_DIM + d.CellR;
        const int32_t col = d.Col * MAPCELL_DIM + d.CellC;
        MCTest::Scope scope("door " + std::to_string(door) + " at (" + std::to_string(row) + "," + std::to_string(col) +
                            ")");
        CHECK_EQ(map.Passable(row, col), 1u);
    }
}

TEST_CASE("game: pathfinding the Uller's withdraw routes stay on passable cells")
{
    if (!MapReady())
    {
        return;
    }

    const MissionMap& map = MissionMap::Get();
    const MCVector3D start(2834.0f, 2790.0f, 0.0f);
    int32_t planned = 0;

    // Goals in every direction and at every range, on open cells (calcMoveGoal moves a goal off a blocked cell).
    for (int32_t angle = 0; angle < 360; angle += 15)
    {
        for (float range = 500.0f; range <= 9000.0f; range += 500.0f)
        {
            const double radians = angle * 3.14159265358979 / 180.0;
            const MCVector3D goal(start.X + static_cast<float>(std::cos(radians) * range),
                                  start.Y + static_cast<float>(std::sin(radians) * range), 0.0f);
            const Cell goalCell = MissionMap::CellOf(goal);

            if (map.Passable(goalCell.Row, goalCell.Col) == 0)
            {
                continue;
            }

            MCTest::Scope scope("angle " + std::to_string(angle) + " range " + std::to_string(static_cast<int>(range)));
            const RouteCheck check = PlanRoute(map, start, MissionMap::CellCentre(goalCell.Row, goalCell.Col));
            CHECK(check.Problem.empty());

            if (!check.Problem.empty())
            {
                std::cout << "    " << check.Problem << "\n";
            }

            planned += check.Legs > 0 ? 1 : 0;
        }
    }

    std::cout << "  routes planned: " << planned << "\n";
    CHECK(planned > 0);
}

TEST_CASE("game: pathfinding random long routes across mission 1 stay on passable cells")
{
    if (!MapReady())
    {
        return;
    }

    const MissionMap& map = MissionMap::Get();
    std::vector<Cell> open;

    for (int32_t row = 0; row < GameMap->Height * MAPCELL_DIM; row++)
    {
        for (int32_t col = 0; col < GameMap->Width * MAPCELL_DIM; col++)
        {
            if (map.Passable(row, col) != 0)
            {
                open.push_back(Cell{row, col});
            }
        }
    }

    REQUIRE(!open.empty());
    std::mt19937 random(1999);
    std::uniform_int_distribution<size_t> pick(0, open.size() - 1);
    int32_t planned = 0;
    int32_t failed = 0;

    for (int32_t i = 0; i < 3000 && failed < 10; i++)
    {
        const Cell from = open[pick(random)];
        const Cell to = open[pick(random)];
        const RouteCheck check =
            PlanRoute(map, MissionMap::CellCentre(from.Row, from.Col), MissionMap::CellCentre(to.Row, to.Col));

        if (!check.Problem.empty())
        {
            failed++;
            MCTest::Scope scope("(" + std::to_string(from.Row) + "," + std::to_string(from.Col) + ") to (" +
                                std::to_string(to.Row) + "," + std::to_string(to.Col) + ")");
            std::cout << "    " << check.Problem << "\n";
            CHECK(check.Problem.empty());
        }

        planned += check.Legs > 0 ? 1 : 0;
    }

    std::cout << "  routes planned: " << planned << "\n";
    CHECK(planned > 0);
}

TEST_CASE("game: pathfinding a global map written back matches the editor's file")
{
    if (!MapReady())
    {
        return;
    }

    // The retail file, as MCEditor wrote it.
    MCFile original;
    REQUIRE_EQ(original.Open("data\\terrain\\m0101.gmm"), 0);
    std::vector<uint8_t> expected(original.GetLength());
    original.Read(expected.data(), static_cast<int32_t>(expected.size()));
    original.Close();

    // A map of its own: write recomputes the path cost table, which runs the door search.
    MCGlobalMap map;
    MCFile in;
    REQUIRE_EQ(in.Open("data\\terrain\\m0101.gmm"), 0);
    REQUIRE_EQ(map.Init(&in), 0);
    in.Close();

    const std::string path = (std::filesystem::temp_directory_path() / "mc_pathfinding_m0101.gmm").string();
    {
        MCFile out;
        REQUIRE_EQ(out.Create(path.c_str()), 0);
        REQUIRE_EQ(map.Write(&out), 0);
        out.Close();
    }

    std::ifstream written(path, std::ios::binary);
    const std::vector<uint8_t> actual((std::istreambuf_iterator<char>(written)), std::istreambuf_iterator<char>());
    written.close();
    std::filesystem::remove(path);
    REQUIRE_EQ(actual.size(), expected.size());

    // Every byte matches, except what the editor wrote from its own memory: each area record's doors pointer (+0x4)
    // and uninitialised word at +0x15, and each door record's two link pointers (+0x17), which write gives as 0.
    // Header words 1 and 2 (0) and the area words at +0x11 (-1), +0x1d .. +0x25 (0) are the editor's values, which
    // nothing reads.
    const size_t areaMapSize = static_cast<size_t>(map.Width) * map.Height * (map.NumAreas < 256 ? 1 : 2);
    const size_t areasStart = 48 + areaMapSize + static_cast<size_t>(map.NumDoorInfos) * 3;
    const size_t areasEnd = areasStart + static_cast<size_t>(map.NumAreas) * GLOBALMAP_AREA_RECORD_SIZE;
    const size_t doorsStart = areasEnd + static_cast<size_t>(map.NumDoorLinks) * 7;
    const size_t doorsEnd = doorsStart + static_cast<size_t>(map.NumDoors + 2) * GLOBALMAP_DOOR_RECORD_SIZE;
    int32_t differences = 0;

    for (size_t i = 0; i < expected.size(); i++)
    {
        if (i >= areasStart && i < areasEnd)
        {
            const size_t field = (i - areasStart) % GLOBALMAP_AREA_RECORD_SIZE;

            if ((field >= 0x4 && field < 0x8) || (field >= 0x15 && field < 0x19))
            {
                continue;
            }
        }

        if (i >= doorsStart && i < doorsEnd)
        {
            const size_t field = (i - doorsStart) % GLOBALMAP_DOOR_RECORD_SIZE;

            if (field >= 0x17 && field < 0x1f)
            {
                continue;
            }
        }

        if (actual[i] != expected[i] && differences++ < 10)
        {
            MCTest::Scope scope("byte " + std::to_string(i));
            CHECK_EQ(static_cast<int32_t>(actual[i]), static_cast<int32_t>(expected[i]));
        }
    }

    CHECK_EQ(differences, 0);
    map.Destroy();
}
