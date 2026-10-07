#pragma once

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"

class MCFitIniFile;
class MCGameObject;

// The collision system (original source: object\collsn.cpp). Each frame the objects are sorted into a grid of
// cells; each object is checked against those of its own and the next cells, and colliding pairs get their types'
// collision handlers. Names follow MechCommander 2's collsn.h, which kept this design.

/// <summary>A link of a collision grid cell's list.</summary>
/// <remarks>8 bytes.</remarks>
struct MCCollisionGridNode
{
    /// <summary>The object.</summary>
    MCGameObject* Object = nullptr;
    /// <summary>The next in the cell.</summary>
    MCCollisionGridNode* Next = nullptr;
};

/// <summary>Two objects that collided this frame.</summary>
/// <remarks>0x10 bytes.</remarks>
struct MCCollisionRecord
{
    /// <summary>One object (cleared once handled or destroyed).</summary>
    MCGameObject* Obj1 = nullptr;
    /// <summary>The other.</summary>
    MCGameObject* Obj2 = nullptr;
    /// <summary>When (seconds from now).</summary>
    float Time = 0;
    /// <summary>The next pending record.</summary>
    MCCollisionRecord* Next = nullptr;
};

/// <summary>Two movers about to collide (for their pilots' collision avoidance).</summary>
/// <remarks>0x10 bytes.</remarks>
struct MCCollisionAlertRecord
{
    /// <summary>One mover.</summary>
    MCGameObject* Object1 = nullptr;
    /// <summary>The other.</summary>
    MCGameObject* Object2 = nullptr;
    /// <summary>Their squared distance at the closest point.</summary>
    float Distance = 0;
    /// <summary>Seconds until then.</summary>
    float Time = 0;
};

/// <summary>This frame's collision alerts.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>; 0xc bytes.</remarks>
class MCGlobalCollisionAlert
{
public:
    /// <summary>Makes room for <paramref name="maxCollisionAlerts"/> alerts; 0 or 0xccef000a.</summary>
    int32_t Init(uint32_t maxCollisionAlerts);
    /// <summary>Frees the alerts.</summary>
    void Destroy();
    /// <summary>Adds an alert; 0, or 0xccef000b when full.</summary>
    int32_t AddRecord(MCGameObject* obj1, MCGameObject* obj2, float distance, float time);
    /// <summary>The next alert after <paramref name="startRecord"/> (from the start when null) involving
    /// <paramref name="object"/>.</summary>
    MCCollisionAlertRecord* FindAlert(MCGameObject* object, MCCollisionAlertRecord* startRecord);
    /// <summary>Clears every alert.</summary>
    void PurgeRecords();

    /// <summary>The alerts.</summary>
    std::unique_ptr<MCCollisionAlertRecord[]> CollisionAlerts;
    /// <summary>Room for.</summary>
    uint32_t MaxAlerts = 0;
    /// <summary>Alerts this frame.</summary>
    uint32_t NextRecord = 0;
};

/// <summary>The grid objects are sorted into each frame; objects bigger than a cell go on the giant list.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>; 0x48 bytes.</remarks>
class MCCollisionGrid
{
public:
    /// <summary>
    /// The first time, sizes the grid from the system's settings and allocates it; every time, empties it and
    /// sets its origin. 0, or 0xccf00000 / 0xccf00001 without memory.
    /// </summary>
    int32_t Init(MCVector3D& newOrigin);
    /// <summary>Frees the grid.</summary>
    void Destroy();
    /// <summary>Adds <paramref name="object"/> to cell <paramref name="gridIndex"/>; 0, 0xccf00002 when out of
    /// nodes, 0xccf00003 for a bad cell.</summary>
    int32_t Add(uint32_t gridIndex, MCGameObject* object);
    /// <summary>Adds a colliding object to its position's cell, or to the giant list when bigger than a cell.</summary>
    int32_t Add(MCGameObject* object);
    /// <summary>Checks every object against the giant list, its own cell and the next cells (right, below).</summary>
    void CreateGrid();
    /// <summary>Checks <paramref name="object"/> against every object of <paramref name="area"/> (skipping pairs
    /// that never collide, such as two bullets).</summary>
    void CheckGrid(MCGameObject* object, MCCollisionGridNode* area);

    /// <summary>Cells across.</summary>
    uint32_t XGridWidth = 0;
    /// <summary>Cells down.</summary>
    uint32_t YGridWidth = 0;
    /// <summary>A cell's size in world units.</summary>
    uint32_t GridRadius = 0;
    /// <summary>Nodes available.</summary>
    uint32_t MaxObjects = 0;
    /// <summary>Objects bigger than a cell.</summary>
    MCCollisionGridNode* GiantObjects = nullptr;
    /// <summary>Each cell's list.</summary>
    std::unique_ptr<MCCollisionGridNode*[]> Grid;
    /// <summary>The node pool.</summary>
    std::unique_ptr<MCCollisionGridNode[]> Nodes;
    /// <summary>The next free node.</summary>
    uint32_t NextAvailableNode = 0;
    /// <summary>The grid's origin.</summary>
    MCVector3D GridOrigin;
    /// <summary>Set once allocated.</summary>
    int GridIsGo = 0;
    /// <summary>Bytes of <see cref="Grid"/>.</summary>
    uint32_t GridSize = 0;
    /// <summary>Bytes of <see cref="Nodes"/>.</summary>
    uint32_t NodeTableSize = 0;
    /// <summary>Half the grid's width, in world units (positions are centred on 0).</summary>
    float GridXOffset = 0.0f;
    /// <summary>Half the grid's height.</summary>
    float GridYOffset = 0.0f;
    /// <summary>The grid's width in world units.</summary>
    float GridXCheck = 0.0f;
    /// <summary>The grid's height.</summary>
    float GridYCheck = 0.0f;
};

/// <summary>The collision system: settings from the "CollisionSystem" FIT block, the grid, and the collisions
/// found.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>.</remarks>
class MCCollisionSystem
{
public:
    /// <summary>Reads the settings, makes the records, the grid and the alerts.</summary>
    int32_t Init(MCFitIniFile* scenarioFile);
    /// <summary>Rebuilds the grid from every list's objects and checks it.</summary>
    void CheckObjects();
    /// <summary>Checks every pair of each side's mechs for collision alerts.</summary>
    void CheckAlarums();
    /// <summary>
    /// Whether two objects touch: two movers (classes below 5) by sharing a terrain vertex and cell, anything else
    /// by distance against their extents, then collision alerts.
    /// </summary>
    void DetectCollision(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>A mover against a static object (skipping ones its tile's overlay lets it pass).</summary>
    void DetectStaticCollision(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>
    /// Seconds until two mechs (of a side, or with a vehicle) come closest, when within the warning distance;
    /// records an alert when they would touch and <paramref name="setAlert"/>.
    /// </summary>
    float CheckCollisionAlerts(MCGameObject* obj1, MCGameObject* obj2, int setAlert);
    /// <summary>Calls both objects' types' collision handlers; an object whose handler asks is removed and
    /// deleted.</summary>
    void CheckExtents(MCGameObject* obj1, MCGameObject* obj2, float time);
    /// <summary>Handles the pending collision records.</summary>
    void ProcessCollisions();
    /// <summary>Clears <paramref name="object"/> from the records after <paramref name="record"/>.</summary>
    void RemoveCollisions(MCGameObject* object, MCCollisionRecord* record);
    /// <summary>Always null in the original.</summary>
    MCCollisionRecord* FindNextPending();
    /// <summary>Empty in the original.</summary>
    int32_t AddPendingCollision(MCGameObject* obj1, MCGameObject* obj2, float time);
    /// <summary>Seconds until two moving objects touch within this frame; 0 when already touching.</summary>
    float TimeToImpact(MCGameObject* obj1, MCGameObject* obj2);
    /// <summary>Deletes the grid, the records and the alerts.</summary>
    void Destroy();

    /// <summary>The grid.</summary>
    MCCollisionGrid* CollisionGrid = nullptr;
    /// <summary>The collision records (maxCollisions).</summary>
    std::unique_ptr<MCCollisionRecord[]> CollisionList;
    /// <summary>The first pending record.</summary>
    MCCollisionRecord* FirstPending = nullptr;
    /// <summary>FIT "MaxPending".</summary>
    uint32_t MaxPending = 0;

    /// <summary>FIT "XGridSize": cells across.</summary>
    static uint32_t XGridSize;
    /// <summary>FIT "YGridSize": cells down.</summary>
    static uint32_t YGridSize;
    /// <summary>FIT "GridRadius": a cell's size.</summary>
    static uint32_t GridRadius;
    /// <summary>Grid nodes (FIT "MaxObjects", then forced to 1200).</summary>
    static uint32_t MaxObjects;
    /// <summary>FIT "MaxCollisions".</summary>
    static uint32_t MaxCollisions;
    /// <summary>Collisions found this frame.</summary>
    static uint32_t NumCollisions;
    /// <summary>FIT "WarningDist": squared distance under which movers get collision alerts.</summary>
    static float WarningDist;
    /// <summary>FIT "AlertTime".</summary>
    static float AlertTime;
};

/// <summary>The collision alerts.</summary>
extern MCGlobalCollisionAlert* GlobalCollisionAlert;
