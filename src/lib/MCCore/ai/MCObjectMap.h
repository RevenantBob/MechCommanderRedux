#pragma once

class MCGameObject;
class MCScenarioMap;

/// <summary>Where an object stands in the <see cref="MCObjectMap"/>: its tile and cell.</summary>
/// <remarks>Original: <c>struct _ObjectPosition</c> (GameObject::getObjPosition), a node of its tile row's list; the
/// list links went with the lists.</remarks>
struct MCObjectPosition
{
    MCGameObject* Object = nullptr;
    int32_t TileR = 0;
    int32_t TileC = 0;
    int32_t CellR = 0;
    int32_t CellC = 0;
    /// <summary>Row in map cells (tileR * 3 + cellR); set by <see cref="MCObjectMap::UpdateObject"/>, 0 before.</summary>
    int32_t MapCellR = 0;
    int32_t MapCellC = 0;
};

/// <summary>Which objects stand on which tile. It owns each object's <see cref="MCObjectPosition"/>.</summary>
/// <remarks>Original source: <c>ai\move.cpp</c>. The original kept a list per tile row, sorted by column; only the
/// count per tile was ever read, so the port keeps a list per row in any order.</remarks>
class MCObjectMap
{
public:
    /// <summary>An empty object map the size of <paramref name="map"/>.</summary>
    explicit MCObjectMap(const MCScenarioMap& map);
    /// <summary>Clears the position of every object still on the map.</summary>
    ~MCObjectMap();
    MCObjectMap(const MCObjectMap&) = delete;
    MCObjectMap& operator=(const MCObjectMap&) = delete;

    /// <summary>Adds an object at its current position.</summary>
    void AddObject(MCGameObject* object);
    /// <summary>Moves an object's record to its tile; removes it when it left the map.</summary>
    /// <returns>False when the object was removed.</returns>
    bool UpdateObject(MCGameObject* object);
    /// <summary>Takes an object off the map (its position becomes null).</summary>
    void RemoveObject(MCGameObject* object);
    /// <summary>How many objects stand on a tile.</summary>
    int32_t GetNumObjects(int32_t tileR, int32_t tileC) const;
    /// <summary>As <see cref="GetNumObjects"/>, without trees.</summary>
    int32_t GetNumSensorBlockingObjects(int32_t tileR, int32_t tileC) const;

private:
    /// <summary>The objects of a tile row that blocks (only the trees don't) or of any class.</summary>
    int32_t CountObjects(int32_t tileR, int32_t tileC, bool sensorBlockingOnly) const;
    /// <summary>Adds <paramref name="position"/> to its row's list (none for a row off the map).</summary>
    void Link(MCObjectPosition* position);
    /// <summary>Takes <paramref name="position"/> out of its row's list.</summary>
    void Unlink(MCObjectPosition* position);

    int32_t _Height = 0;
    int32_t _Width = 0;
    /// <summary>
    /// Every record, by address. An object added twice keeps its first record too, still counted on its tile, as the
    /// original's node was.
    /// </summary>
    std::unordered_map<MCObjectPosition*, std::unique_ptr<MCObjectPosition>> _Positions;
    /// <summary>Per tile row, the records of the objects on it.</summary>
    std::vector<std::vector<MCObjectPosition*>> _Rows;
};
