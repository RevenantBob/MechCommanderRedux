#pragma once

class MCGameObject;

/// <summary>
/// The square grid of cells the colliding objects are sorted into each frame: each object is checked against those
/// of its own cell, the cells to its right and below, and the objects bigger than a cell (the giants).
/// </summary>
/// <remarks>
/// Original source: <c>object\collsn.cpp</c> (<c>CollisionGrid</c>). The original chained each cell's objects newest
/// first through a pool of 1200 nodes; a cell here keeps them in the order they came and is walked backwards, which
/// checks the pairs in the original's order.
/// </remarks>
class MCCollisionGrid
{
public:
    /// <summary>A grid <paramref name="cellsAcross"/> cells a side, each <paramref name="cellSize"/> world units, on the
    /// map's centre.</summary>
    MCCollisionGrid(uint32_t cellsAcross, uint32_t cellSize);

    /// <summary>Empties every cell and the giants.</summary>
    void Clear();
    /// <summary>
    /// Adds a colliding object to its position's cell (clamped to the grid), or to the giants when its extent is
    /// bigger than a cell. Objects with collisions off are left out.
    /// </summary>
    void Add(MCGameObject* object);
    /// <summary>
    /// Hands every pair that may touch to <paramref name="detect"/>: the giants among themselves, then cell by cell
    /// each object against the giants, the rest of its cell and the cells right, down-right and down, skipping pairs
    /// that never collide (turrets, gates, train cars and explosions among themselves).
    /// </summary>
    void CheckPairs(const std::function<void(MCGameObject*, MCGameObject*)>& detect) const;
    /// <summary>The objects of the cell at <paramref name="row"/>, <paramref name="col"/>, in the order they came.</summary>
    std::span<MCGameObject* const> Cell(uint32_t row, uint32_t col) const { return _Cells[row * _CellsAcross + col]; }
    /// <summary>The objects bigger than a cell, in the order they came.</summary>
    std::span<MCGameObject* const> Giants() const { return _Giants; }
    /// <summary>A cell's size in world units.</summary>
    uint32_t CellSize() const { return _CellSize; }

private:
    /// <summary>
    /// Checks <paramref name="object"/> against the first <paramref name="count"/> objects of
    /// <paramref name="area"/>, newest first.
    /// </summary>
    static void CheckArea(MCGameObject* object, std::span<MCGameObject* const> area, size_t count,
                          const std::function<void(MCGameObject*, MCGameObject*)>& detect);

    uint32_t _CellsAcross = 0;
    uint32_t _CellSize = 0;
    /// <summary>Half the grid's width in world units (positions are centred on 0).</summary>
    float _XOffset = 0.0f;
    float _YOffset = 0.0f;
    /// <summary>The grid's width in world units.</summary>
    float _XCheck = 0.0f;
    float _YCheck = 0.0f;
    std::vector<std::vector<MCGameObject*>> _Cells;
    std::vector<MCGameObject*> _Giants;
};
