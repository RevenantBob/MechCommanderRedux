#pragma once

#include "lib/MCVector3D.h"

class MCMoveChunk;

/// <summary>One step of a <see cref="MCMovePath"/>: a cell and its world position.</summary>
/// <remarks>The original name isn't known; MechCommander 2 calls it PathStep.</remarks>
struct MCPathStep
{
    int16_t TileR = 0;
    int16_t TileC = 0;
    int16_t CellR = 0;
    int16_t CellC = 0;
    /// <summary>Distance from this step to the end of the path, in meters.</summary>
    float DistanceToGoal = 0;
    MCVector3D Destination;
    /// <summary>
    /// The offset (index of <see cref="CellShiftRow"/>) stepped from the previous step: 0-7 a neighbour, above 7 a
    /// jump. Read as a signed char; MoveChunk::build sends it as the chunk's step direction.
    /// </summary>
    uint8_t Direction = 0;
};

/// <summary>A mover's cell path.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. The original held 200 steps; the port's list grows.</remarks>
class MCMovePath
{
public:
    /// <summary>Sets the step counts (and makes room for the steps).</summary>
    void SetNumSteps(int32_t numSteps);
    /// <summary>Empties the path (unmarking it first).</summary>
    void Clear();
    /// <summary>Meters from a position through step <paramref name="fromStep"/> (-1: the current) to the goal.</summary>
    float GetDistanceLeft(MCVector3D position, int32_t fromStep = -1) const;
    /// <summary>Counts the path's tiles in the scenario map's path map.</summary>
    void Mark();
    /// <summary>Takes the path's tiles back out of the scenario map's path map.</summary>
    void Unmark();
    /// <summary>Sets the path lock of <paramref name="range"/> steps from <paramref name="start"/> (-1: current).</summary>
    void Lock(int32_t start, int32_t range, uint32_t setting);
    /// <summary>
    /// Whether a step of the range is path locked; <paramref name="reachedEnd"/> tells whether the range ran to the
    /// end of the path.
    /// </summary>
    bool IsLocked(int32_t start, int32_t range, int* reachedEnd = nullptr) const;
    /// <summary>Whether a step of the range is impassable (see <see cref="IsLocked"/>).</summary>
    bool IsBlocked(int32_t start, int32_t range, int* reachedEnd = nullptr) const;
    /// <summary>The area of the first bridge tile among the steps, or -1.</summary>
    int32_t CrossesBridge(int32_t start, int32_t range) const;
    /// <summary>The first step on tile (tileR, tileC), or -1.</summary>
    int32_t CrossesTile(int32_t start, int32_t range, int32_t tileR, int32_t tileC) const;
    /// <summary>The first step on a closed gate overlay, or -1.</summary>
    int32_t CrossesClosedGate(int32_t start, int32_t range) const;
    /// <summary>Rebuilds the path from a received chunk.</summary>
    void SetMoveChunk(const MCMoveChunk& chunk);
    /// <summary>Sets a step's world position.</summary>
    void SetDestination(int32_t stepNumber, MCVector3D position);

    /// <summary>World position of the last step.</summary>
    MCVector3D Goal;
    /// <summary>Copied from <see cref="MCMoveMap::Target"/> when the path is calculated.</summary>
    MCVector3D Target;
    int32_t NumSteps = 0;
    int32_t NumStepsWhenNotPaused = 0;
    int32_t CurStep = 0;
    int32_t Cost = 0;
    /// <summary>
    /// The steps. The list only grows, and holds a step past the longest path yet: the path finder reads the step
    /// after a path's last (left from an earlier, longer path) when the last step is a jump, as the original did.
    /// </summary>
    std::vector<MCPathStep> StepList;
    bool Marked = false;
    /// <summary>The <see cref="MCGlobalPathStep"/> this path walks, -1 for none.</summary>
    int32_t GlobalStep = 0;

private:
    /// <summary>The end of a range of steps: start + range, at most the steps when not paused.</summary>
    int32_t RangeEnd(int32_t start, int32_t range) const;
};
