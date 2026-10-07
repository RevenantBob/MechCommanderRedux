#pragma once

#include "ai/move.h"

/// <summary>
/// A flat N x N tile scenario map built in memory (no map file), installed as <c>GameMap</c> with the terrain
/// geometry to match: one block of N vertices a side, 128 m a vertex, every movement cell passable until the test
/// blocks it. For movement and line-of-sight tests that only need the map. The previous map and geometry come back
/// when it goes.
/// </summary>
class MCTinyMap
{
public:
    /// <summary>The metres a tile side spans.</summary>
    static constexpr float MetersPerTile = 128.0f;

    /// <summary>Builds and installs a map of <paramref name="tiles"/> x <paramref name="tiles"/> tiles.</summary>
    explicit MCTinyMap(int32_t tiles);
    ~MCTinyMap();
    MCTinyMap(const MCTinyMap&) = delete;
    MCTinyMap& operator=(const MCTinyMap&) = delete;

    /// <summary>Tiles a side.</summary>
    int32_t Tiles() const { return _Tiles; }

    /// <summary>Movement cells a side.</summary>
    int32_t Cells() const { return _Tiles * MAPCELL_DIM; }

    /// <summary>Makes the movement cell at <paramref name="row"/>, <paramref name="col"/> (map cells) impassable.</summary>
    void Block(int32_t row, int32_t col);

    /// <summary>Whether the movement cell is passable, as the map says.</summary>
    bool Passable(int32_t row, int32_t col) const;

    /// <summary>The world position of a movement cell's centre.</summary>
    MCVector3D CellCentre(int32_t row, int32_t col) const;

private:
    int32_t _Tiles;
    MCScenarioMap* _PreviousMap;
    float _PreviousWorldUnitsMapSide;
    int32_t _PreviousVerticesBlockSide;
    int32_t _PreviousBlocksMapSide;
    float _PreviousMetersPerVertex;
    float _PreviousOneOverMetersPerVertex;
    float _PreviousMetersBlockSide;
    float _PreviousMetersPerVertexDivMapCell;
};
