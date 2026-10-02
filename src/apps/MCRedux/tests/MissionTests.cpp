#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "ai/move.h"
#include "ai/tacordr.h"
#include "camera/camera.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "object/mech.h"
#include "object/mechctrl.h"
#include "object/mover.h"
#include "object/objtype.h"
#include "object/warrior.h"
#include "sprite/mactor.h"
#include "terrain/terrain.h"

namespace
{
    /// <summary>Whether map cell (<paramref name="row"/>, <paramref name="col"/>) is passable (off the map: no).</summary>
    bool CellPassable(int32_t row, int32_t col)
    {
        if (row < 0 || col < 0 || row >= GameMap->height * 3 || col >= GameMap->width * 3)
        {
            return false;
        }

        return GameMap->map[(row / 3) * GameMap->width + col / 3].getCellPassable(row % 3, col % 3) != 0;
    }

    /// <summary>The map cell under <paramref name="position"/>, as row and column.</summary>
    std::pair<int32_t, int32_t> CellAt(vector_3d position)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);
        return {tileR * 3 + cellR, tileC * 3 + cellC};
    }
}

/// <summary>
/// Mission 1 boots headless and its movers start on passable cells (the game itself puts nothing on forest or water).
/// </summary>
TEST_CASE_ISOLATED("game: mission 1 boots and every mover stands on a passable cell")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));

    for (int32_t partId = 0x200; partId < MAX_MOVER_PART_ID; partId++)
    {
        Mover* mover = getMoverFromPartId(partId);

        if (mover == nullptr)
        {
            continue;
        }

        const vector_3d position = mover->getPosition();
        const auto [row, col] = CellAt(position);

        // Vehicles may be parked in a building's cells; only the mechs are held to it.
        if (mover->objectClass == BATTLEMECH)
        {
            MCTest::Scope scope("part " + std::to_string(partId));
            CHECK(CellPassable(row, col));
        }
    }
}

namespace
{
    /// <summary>What one mover did in the cells it entered.</summary>
    struct MoverTrack
    {
        /// <summary>The cell it stood on last frame.</summary>
        std::pair<int32_t, int32_t> cell{-1, -1};
        /// <summary>Blocked cells entered in a row (a passable cell resets it).</summary>
        int32_t run = 0;
        /// <summary>The longest such run.</summary>
        int32_t longestRun = 0;
        /// <summary>Every blocked cell entered.</summary>
        int32_t blockedCells = 0;
        /// <summary>Its withdraw flag last frame.</summary>
        int32_t withdrawing = 0;
        /// <summary>The last frames.</summary>
        std::deque<std::string> trace;
        /// <summary>The frames up to the end of the longest run.</summary>
        std::vector<std::string> longestTrace;
    };

    /// <summary>One frame of a mover, for the trace.</summary>
    std::string DescribeMover(Mover* mover, std::pair<int32_t, int32_t> cell)
    {
        const vector_3d position = mover->getPosition();
        std::string line =
            std::format("    t={:.2f} pos ({:.1f},{:.1f}) cell ({},{}) p{}", scenarioTime, position.x, position.y,
                        cell.first, cell.second, CellPassable(cell.first, cell.second) ? 1 : 0);

        if (mover->objectClass == BATTLEMECH)
        {
            auto* actor = static_cast<MechActor*>(mover->appearance);
            line += std::format(" gesture {}/{} goal {} legs {}", actor->currentGesture, actor->currentStateGesture,
                                actor->gestureGoal, static_cast<int32_t>(static_cast<BattleMech*>(mover)->legStatus));
        }

        MechWarrior* pilot = mover->getPilot();

        if (pilot == nullptr)
        {
            return line;
        }

        MovePath* path = pilot->getMovePath();
        const int32_t numSteps = path != nullptr ? path->numStepsWhenNotPaused : -1;
        const int32_t curStep = path != nullptr ? path->curStep : -1;

        if (path != nullptr && curStep >= 0 && curStep < numSteps)
        {
            const PathStep& step = path->stepList[curStep];
            const int32_t stepRow = step.tileR * 3 + step.cellR;
            const int32_t stepCol = step.tileC * 3 + step.cellC;
            line += std::format(" step {}/{} -> ({},{}) p{} dest ({:.1f},{:.1f}) dir {} facing {:.1f}", curStep,
                                numSteps, stepRow, stepCol, CellPassable(stepRow, stepCol) ? 1 : 0, step.destination.x,
                                step.destination.y, static_cast<int8_t>(step.direction),
                                mover->relFacingTo(step.destination, -1));
        }
        else
        {
            line += std::format(" step {}/{}", curStep, numSteps);
        }

        // MechAIControl::update only moves a mech whose pilot can (not disabled, wounds under 6, status 0..2 or 4).
        line += std::format(" pilot wounds {:.1f} status {} disabled {} awake {}", pilot->wounds, pilot->status,
                            mover->isDisabled(), mover->getAwake());
        line +=
            std::format(" order {} pathType {} withdraw {} moveState {}/{}",
                        static_cast<int32_t>(pilot->curTacOrder.code), static_cast<int32_t>(pilot->moveOrders.pathType),
                        mover->unknown79C, pilot->moveOrders.moveState, pilot->moveOrders.moveStateGoal);

        if (mover->objectClass == BATTLEMECH)
        {
            auto* controlData = static_cast<MechControlData*>(mover->control->controlData);
            line += std::format(" rotate {} throttle {} pivot {}", static_cast<int32_t>(controlData->rotate),
                                static_cast<int32_t>(controlData->throttle), controlData->pivot);
            // updateMovement's early exits.
            line += std::format(" numSteps {} check {} u170 {:.1f} flags {}{}{} captured {} jump {}",
                                path != nullptr ? path->numSteps : -1, mover->pilotingCheckPending, mover->unknown170,
                                mover->disableThisFrame, mover->shutDownThisFrame, mover->startUpThisFrame,
                                mover->isCaptured(), static_cast<BattleMech*>(mover)->inJump);
        }

        return line;
    }
}

/// <summary>
/// The bug report: the lance fights mission 1's Uller (part 896); badly hurt, it walked off through forest and water
/// and off the map. With the test's dice its pilot is wounded to 4 mid-stride; the port stopped driving a mech at 3.5
/// wounds (the original's 6.0 misread), so the mech walked on in a straight line. Movers move only along passable
/// cells, so none, the Uller or anyone else, may cross blocked cells. Cutting a corner between two diagonal cells can
/// graze one blocked cell, so a run of one is allowed; two blocked cells in a row is walking through. The camera
/// follows the Uller, as the player did.
/// </summary>
TEST_CASE_ISOLATED("game: mission 1's battle keeps every mover on passable cells")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    Mover* uller = getMoverFromPartId(896);
    REQUIRE(uller != nullptr);
    REQUIRE(uller->objectClass == BATTLEMECH);
    REQUIRE_EQ(uller->getAlignment(), -1);

    // The player's attack command, as the interface sends it (icallbk.cpp: attack, any range, pursue).
    for (int32_t partId = 0x200; partId < 0x203; partId++)
    {
        Mover* mover = getMoverFromPartId(partId);
        REQUIRE(mover != nullptr);
        TacticalOrder order;
        order.init(ORDER_ORIGIN_PLAYER, TACTICAL_ORDER_ATTACK_OBJECT, 0);
        order.target = uller;
        order.attackParams.type = 1;
        order.attackParams.method = 0;
        order.attackParams.range = -1;
        order.attackParams.pursue = -1;
        mover->handleTacticalOrder(order, 1, 0);
        order.destroy();
    }

    constexpr float FrameSeconds = 1.0f / 15.0f;
    constexpr int32_t MaxFrames = 15 * 60 * 10;
    // After the Uller is gone, the rest of the battle runs on for this long.
    constexpr int32_t AfterFrames = 15 * 120;
    std::map<int32_t, MoverTrack> tracks;
    int32_t framesLeft = MaxFrames;
    bool ullerGone = false;
    // A mover walking through blocked cells soon walks off the map, which the game doesn't survive; the battle stops
    // at the first one.
    bool walkingThrough = false;

    while (framesLeft-- > 0 && !walkingThrough)
    {
        if (!ullerGone && (uller->isDestroyed() != 0 || uller->getPilot()->status == 2))
        {
            ullerGone = true;
            const vector_3d position = uller->getPosition();
            std::cout << "  t=" << scenarioTime << " the Uller is gone at (" << position.x << "," << position.y
                      << "), damage " << uller->totalDamageTaken << ", destroyed " << uller->isDestroyed()
                      << ", withdraw " << uller->unknown79C << "\n";
            framesLeft = std::min(framesLeft, AfterFrames);
        }

        if (!ullerGone)
        {
            eye->setPosition(uller->getPosition());
        }

        MCTestGame::RunFrame(FrameSeconds);

        for (int32_t partId = 0x200; partId < MAX_MOVER_PART_ID; partId++)
        {
            Mover* mover = getMoverFromPartId(partId);

            if (mover == nullptr || mover->isDestroyed() != 0)
            {
                continue;
            }

            MoverTrack& track = tracks[partId];
            const auto cell = CellAt(mover->getPosition());

            if (mover->unknown79C != track.withdrawing)
            {
                track.withdrawing = mover->unknown79C;
                std::cout << "  t=" << scenarioTime << " part " << partId << " withdraw " << track.withdrawing
                          << ", damage " << mover->totalDamageTaken << "\n";
            }

            track.trace.push_back(DescribeMover(mover, cell));

            if (track.trace.size() > 150)
            {
                track.trace.pop_front();
            }

            if (cell == track.cell)
            {
                continue;
            }

            const bool first = track.cell.first < 0;
            track.cell = cell;

            if (first || CellPassable(cell.first, cell.second))
            {
                track.run = 0;
                continue;
            }

            track.blockedCells++;
            track.run++;

            // The frames into the first walk through: up to its second blocked cell.
            if (track.run == 2 && track.longestRun < 2)
            {
                track.longestTrace.assign(track.trace.begin(), track.trace.end());
            }

            if (track.run == 4 && track.longestRun < 4)
            {
                walkingThrough = true;
                std::cout << "  part " << partId << " is walking through blocked cells:\n";

                for (const std::string& line : track.longestTrace)
                {
                    std::cout << line << "\n";
                }

                for (size_t i = track.trace.size() >= 20 ? track.trace.size() - 20 : 0; i < track.trace.size(); i++)
                {
                    std::cout << track.trace[i] << "\n";
                }

                std::cout << std::flush;
            }

            track.longestRun = std::max(track.longestRun, track.run);
        }
    }

    for (const auto& [partId, track] : tracks)
    {
        if (track.blockedCells == 0)
        {
            continue;
        }

        Mover* mover = getMoverFromPartId(partId);
        std::cout << "  part " << partId << " (class " << static_cast<int32_t>(mover->objectClass) << ", type "
                  << mover->getObjectType()->objTypeNum << ") entered " << track.blockedCells
                  << " blocked cells, longest run " << track.longestRun << "\n";
        MCTest::Scope scope("part " + std::to_string(partId));
        CHECK(track.longestRun < 2);
    }
}

/// <summary>
/// A pilot is alive and at the controls until 6 wounds (MechWarrior::injure disables the mech at 6; the controls drive
/// it while the pilot has fewer). So a mech whose pilot takes 4 wounds mid-walk still follows its path: it gets to the
/// goal, stops there, and crosses no blocked cells. One of the player's mechs (part 512) walks 600 units, as ordered.
/// </summary>
TEST_CASE_ISOLATED("game: a mech whose pilot is wounded but alive keeps following its path")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartMission(1));
    Mover* mover = getMoverFromPartId(0x200);
    REQUIRE(mover != nullptr);
    REQUIRE(mover->objectClass == BATTLEMECH);
    REQUIRE(mover->isDestroyed() == 0);
    MechWarrior* pilot = mover->getPilot();
    REQUIRE(pilot->wounds < 4.0f);

    // A goal 600 units away on a passable cell.
    const vector_3d start = mover->getPosition();
    vector_3d goal = start;
    bool found = false;

    for (int32_t degrees = 0; degrees < 360 && !found; degrees += 15)
    {
        const double radians = degrees * 3.14159265358979 / 180.0;
        goal = vector_3d(start.x + static_cast<float>(600.0 * std::cos(radians)),
                         start.y + static_cast<float>(600.0 * std::sin(radians)), start.z);
        const auto [row, col] = CellAt(goal);
        found = CellPassable(row, col);
    }

    REQUIRE(found);
    pilot->orderMoveToPoint(0, 1, ORDER_ORIGIN_PLAYER, goal, -1, 0);

    constexpr float FrameSeconds = 1.0f / 15.0f;

    // Three seconds in, it is walking; then the hit.
    for (int32_t frame = 0; frame < 45; frame++)
    {
        MCTestGame::RunFrame(FrameSeconds);
    }

    REQUIRE_EQ(pilot->injure(4.0f - pilot->wounds, 0), 0);
    CHECK_EQ(pilot->wounds, 4.0f);
    CHECK_EQ(pilot->status, 0);

    std::pair<int32_t, int32_t> lastCell = CellAt(mover->getPosition());
    int32_t run = 0;
    int32_t longestRun = 0;
    bool arrived = false;

    for (int32_t frame = 0; frame < 15 * 90 && !arrived; frame++)
    {
        MCTestGame::RunFrame(FrameSeconds);
        const auto cell = CellAt(mover->getPosition());

        if (cell != lastCell)
        {
            lastCell = cell;
            run = CellPassable(cell.first, cell.second) ? 0 : run + 1;
            longestRun = std::max(longestRun, run);
        }

        // Within a cell of the goal (world units, on the ground).
        const vector_3d at = mover->getPosition();
        arrived = std::hypot(at.x - goal.x, at.y - goal.y) < 64.0f;
    }

    // And stops there: five seconds on, it hasn't walked past.
    for (int32_t frame = 0; frame < 15 * 5 && arrived; frame++)
    {
        MCTestGame::RunFrame(FrameSeconds);
    }

    const vector_3d position = mover->getPosition();
    std::cout << "  from (" << start.x << "," << start.y << ") to (" << goal.x << "," << goal.y << "), ended at ("
              << position.x << "," << position.y << ") at t=" << scenarioTime << "\n";
    CHECK(arrived);
    CHECK(std::hypot(position.x - goal.x, position.y - goal.y) < 64.0f);
    CHECK(longestRun < 2);
}
