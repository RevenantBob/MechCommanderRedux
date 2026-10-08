#pragma once

#include "object/MCCollisionGrid.h"

class MCFitIniFile;
class MCGameObject;

/// <summary>
/// The collision system: each frame the objects of the first three lists are sorted into the grid, every pair that
/// may touch is checked, and touching pairs get their types' collision handlers. A game system of
/// <see cref="MCGameContext"/> (CollisionSystem()), made by the scenario and removed at its end.
/// </summary>
/// <remarks>
/// Original source: <c>object\collsn.cpp</c>. Names follow MechCommander 2's collsn.h, which kept this design. The
/// original also kept collision records, pending collisions and collision alerts for movers about to run into each
/// other; nothing read them, so they are gone.
/// </remarks>
class MCCollisionSystem
{
public:
    /// <summary>A grid of <paramref name="cellsAcross"/> by <paramref name="cellsAcross"/> cells of
    /// <paramref name="gridRadius"/> world units.</summary>
    MCCollisionSystem(uint32_t cellsAcross, uint32_t gridRadius) : Grid(cellsAcross, gridRadius) {}

    /// <summary>
    /// A system set up from the scenario's "CollisionSystem" block (XGridSize, GridRadius); the error names the
    /// missing entry.
    /// </summary>
    /// <remarks>The original sized both sides from XGridSize (YGridSize, read, went unused; the retail missions give
    /// both the same).</remarks>
    static std::expected<std::unique_ptr<MCCollisionSystem>, std::string> Create(MCFitIniFile& scenarioFile);

    /// <summary>Rebuilds the grid from the objects of the first three lists (each also handles its static
    /// collisions) and checks it.</summary>
    void CheckObjects();
    /// <summary>
    /// Whether two objects touch: two movers (classes below explosions) by sharing a terrain vertex and cell, anything
    /// else by distance against their extents; touching pairs get <see cref="CheckExtents"/>.
    /// </summary>
    void DetectCollision(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>An object against a static one (a mover passes those its own cell lets it through).</summary>
    void DetectStaticCollision(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>Calls both objects' types' collision handlers; an object whose destruction handler asks is removed and
    /// deleted.</summary>
    void CheckExtents(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>A grid cell's size in world units (FIT "GridRadius").</summary>
    uint32_t GridRadius() const { return Grid.CellSize(); }

    /// <summary>The grid.</summary>
    MCCollisionGrid Grid;
};

/// <summary>The mission's collision system (null outside a mission).</summary>
MCCollisionSystem* CollisionSystem();
