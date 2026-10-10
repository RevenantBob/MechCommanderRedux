#include "stdafx.h"
#include "ai/MCMovePath.h"
#include "ai/MCMoveSystem.h"
#include "main/MCMissionGlobals.h"

auto MCMovePath::SetNumSteps(int32_t numSteps) -> void
{
    NumStepsWhenNotPaused = numSteps;
    NumSteps = numSteps;

    if (StepList.size() < static_cast<size_t>(numSteps) + 1)
    {
        StepList.resize(static_cast<size_t>(numSteps) + 1);
    }
}

auto MCMovePath::Clear() -> void
{
    if (NumSteps > 0)
    {
        Unmark();
    }

    Goal.Zero();
    NumSteps = 0;
    NumStepsWhenNotPaused = 0;
    CurStep = 0;
    Cost = 0;
    Marked = false;
    GlobalStep = -1;
}

auto MCMovePath::GetDistanceLeft(MCVector3D position, int32_t fromStep) const -> float
{
    if (fromStep == -1)
    {
        fromStep = CurStep;
    }

    const MCPathStep& step = StepList[static_cast<size_t>(fromStep)];
    return std::sqrt((position.X - step.Destination.X) * (position.X - step.Destination.X) +
                     (position.Z - step.Destination.Z) * (position.Z - step.Destination.Z) +
                     (position.Y - step.Destination.Y) * (position.Y - step.Destination.Y)) *
               MetersPerWorldUnit +
           step.DistanceToGoal;
}

auto MCMovePath::Mark() -> void
{
    if (Marked)
    {
        return;
    }

    MCScenarioMap* map = GameMap();

    for (int32_t i = 0; i < NumSteps; i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];
        map->PathMap[static_cast<size_t>(map->Width * step.TileR + step.TileC)]++;
    }

    Marked = true;
}

auto MCMovePath::Unmark() -> void
{
    if (!Marked)
    {
        return;
    }

    MCScenarioMap* map = GameMap();

    for (int32_t i = 0; i < NumSteps; i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];
        map->PathMap[static_cast<size_t>(map->Width * step.TileR + step.TileC)]--;
    }

    Marked = false;
}

auto MCMovePath::RangeEnd(int32_t start, int32_t range) const -> int32_t
{
    return std::min(start + range, NumStepsWhenNotPaused);
}

auto MCMovePath::Lock(int32_t start, int32_t range, uint32_t setting) -> void
{
    if (start == -1)
    {
        start = CurStep;
    }

    MCScenarioMap* map = GameMap();

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];
        map->SetCellPathLocked(step.TileR, step.TileC, step.CellR, step.CellC, setting);
    }
}

auto MCMovePath::IsLocked(int32_t start, int32_t range, int* reachedEnd) const -> bool
{
    if (start == -1)
    {
        start = CurStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = NumStepsWhenNotPaused <= start + range ? 1 : 0;
    }

    const MCScenarioMap* map = GameMap();

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];

        if (map->GetCellPathLocked(step.TileR, step.TileC, step.CellR, step.CellC))
        {
            return true;
        }
    }

    return false;
}

auto MCMovePath::IsBlocked(int32_t start, int32_t range, int* reachedEnd) const -> bool
{
    if (start == -1)
    {
        start = CurStep;
    }

    if (reachedEnd != nullptr)
    {
        *reachedEnd = NumStepsWhenNotPaused <= start + range ? 1 : 0;
    }

    const MCScenarioMap* map = GameMap();

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];

        if (map->TileAt(step.TileR, step.TileC).GetCellPassable(step.CellR, step.CellC) == 0)
        {
            return true;
        }
    }

    return false;
}

auto MCMovePath::CrossesBridge(int32_t start, int32_t range) const -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    const MCScenarioMap* map = GameMap();

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];

        if (OverlayIsBridge[map->TileAt(step.TileR, step.TileC).OverlayType()])
        {
            return GlobalMoveMap()->CalcArea(step.TileR, step.TileC);
        }
    }

    return -1;
}

auto MCMovePath::CrossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC) const -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];

        if (tileR == step.TileR && tileC == step.TileC)
        {
            return i;
        }
    }

    return -1;
}

auto MCMovePath::CrossesClosedGate(int32_t start, int32_t range) const -> int32_t
{
    if (start == -1)
    {
        start = CurStep;
    }

    const MCScenarioMap* map = GameMap();

    for (int32_t i = start; i < RangeEnd(start, range); i++)
    {
        const MCPathStep& step = StepList[static_cast<size_t>(i)];

        if (OverlayIsClosedGate[map->TileAt(step.TileR, step.TileC).OverlayType()])
        {
            return i;
        }
    }

    return -1;
}

auto MCMovePath::SetMoveChunk(const MCMoveChunk& chunk) -> void
{
    const int32_t stepCount = chunk.NumSteps;
    SetNumSteps(stepCount);

    for (int32_t i = 0; i < stepCount; i++)
    {
        MCPathStep& step = StepList[static_cast<size_t>(i)];
        const std::array<int32_t, 4>& pos = chunk.StepPos[static_cast<size_t>(i)];
        step.TileR = static_cast<int16_t>(pos[0]);
        step.TileC = static_cast<int16_t>(pos[1]);
        step.CellR = static_cast<int16_t>(pos[2]);
        step.CellC = static_cast<int16_t>(pos[3]);
        step.Destination = MCVector3D(static_cast<float>(static_cast<double>(CellOffsetToWorld(step.CellC)) +
                                                         TileColToWorld(step.TileC) + HalfMapCell()),
                                      static_cast<float>(static_cast<double>(TileRowToWorld(step.TileR)) -
                                                         CellOffsetToWorld(step.CellR) - HalfMapCell()),
                                      0.0f);
        step.DistanceToGoal = 0.0f;
        step.Direction = 0;
    }

    CurStep = 0;
    // With no steps the original reads numSteps, numStepsWhenNotPaused and curStep (all 0 by then) as the goal.
    Goal = stepCount > 0 ? StepList[static_cast<size_t>(stepCount - 1)].Destination : MCVector3D(0.0f, 0.0f, 0.0f);
    Target.Zero();
    Marked = false;
    Cost = -1;
    GlobalStep = -1;
}

auto MCMovePath::SetDestination(int32_t stepNumber, MCVector3D position) -> void
{
    StepList[static_cast<size_t>(stepNumber)].Destination = position;
}
