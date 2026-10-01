#pragma once

#include "lib/cvmath.h"

class FitIniFile;
class GameObject;
class UserHeap;

// The collision system (original source: object\collsn.cpp). Each frame the objects are sorted into a grid of
// cells; each object is checked against those of its own and the next cells, and colliding pairs get their types'
// collision handlers. Names follow MechCommander 2's collsn.h, which kept this design.

/// <summary>A link of a collision grid cell's list.</summary>
/// <remarks>8 bytes.</remarks>
struct CollisionGridNode
{
    /// <summary>The object.</summary>
    GameObject* object; // +0x00
    /// <summary>The next in the cell.</summary>
    CollisionGridNode* next; // +0x04
};

/// <summary>Two objects that collided this frame.</summary>
/// <remarks>0x10 bytes.</remarks>
struct CollisionRecord
{
    /// <summary>One object (cleared once handled or destroyed).</summary>
    GameObject* obj1; // +0x00
    /// <summary>The other.</summary>
    GameObject* obj2; // +0x04
    /// <summary>When (seconds from now).</summary>
    float time; // +0x08
    /// <summary>The next pending record.</summary>
    CollisionRecord* next; // +0x0c
};

/// <summary>Two movers about to collide (for their pilots' collision avoidance).</summary>
/// <remarks>0x10 bytes.</remarks>
struct CollisionAlertRecord
{
    /// <summary>One mover.</summary>
    GameObject* object1; // +0x00
    /// <summary>The other.</summary>
    GameObject* object2; // +0x04
    /// <summary>Their squared distance at the closest point.</summary>
    float distance; // +0x08
    /// <summary>Seconds until then.</summary>
    float time; // +0x0c
};

/// <summary>This frame's collision alerts.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>; 0xc bytes.</remarks>
class GlobalCollisionAlert
{
public:
    /// <summary>Makes room for <paramref name="maxCollisionAlerts"/> alerts (systemHeap); 0 or 0xccef000a.</summary>
    /// <remarks>MCX.EXE @ 0x00656ad0 (the original's name is lost)</remarks>
    int32_t init(uint32_t maxCollisionAlerts);
    /// <summary>Frees the alerts.</summary>
    /// <remarks>MCX.EXE @ 0x00656b10</remarks>
    void destroy();
    /// <summary>Adds an alert; 0, or 0xccef000b when full.</summary>
    /// <remarks>MCX.EXE @ 0x00656b30</remarks>
    int32_t addRecord(GameObject* obj1, GameObject* obj2, float distance, float time);
    /// <summary>The next alert after <paramref name="startRecord"/> (from the start when null) involving
    /// <paramref name="object"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00656b90</remarks>
    CollisionAlertRecord* findAlert(GameObject* object, CollisionAlertRecord* startRecord);
    /// <summary>Clears every alert.</summary>
    /// <remarks>MCX.EXE @ 0x00656c00</remarks>
    void purgeRecords();

    /// <summary>The alerts.</summary>
    CollisionAlertRecord* collisionAlerts = nullptr; // +0x00
    /// <summary>Room for.</summary>
    uint32_t maxAlerts = 0; // +0x04
    /// <summary>Alerts this frame.</summary>
    uint32_t nextRecord = 0; // +0x08
};

/// <summary>The grid objects are sorted into each frame; objects bigger than a cell go on the giant list.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>; 0x48 bytes. Allocated from
/// <see cref="CollisionSystem::collisionHeap"/>.</remarks>
class CollisionGrid
{
public:
    /// <summary>Allocates from the collision heap (null when it isn't up).</summary>
    /// <remarks>MCX.EXE @ 0x00656c30</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into the collision heap.</summary>
    /// <remarks>MCX.EXE @ 0x00656c60</remarks>
    static void operator delete(void* ptr);

    /// <summary>
    /// The first time, sizes the grid from the system's settings and allocates it; every time, empties it and
    /// sets its origin. 0, or 0xccf00000 / 0xccf00001 without memory.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00656c80</remarks>
    int32_t init(vector_3d& newOrigin);
    /// <summary>Frees the grid.</summary>
    /// <remarks>MCX.EXE @ 0x00656dd0</remarks>
    void destroy();
    /// <summary>Adds <paramref name="object"/> to cell <paramref name="gridIndex"/>; 0, 0xccf00002 when out of
    /// nodes, 0xccf00003 for a bad cell.</summary>
    /// <remarks>MCX.EXE @ 0x00656e30</remarks>
    int32_t add(uint32_t gridIndex, GameObject* object);
    /// <summary>Adds a colliding object to its position's cell, or to the giant list when bigger than a cell.</summary>
    /// <remarks>MCX.EXE @ 0x00656e90</remarks>
    int32_t add(GameObject* object);
    /// <summary>Checks every object against the giant list, its own cell and the next cells (right, below).</summary>
    /// <remarks>MCX.EXE @ 0x00656fc0</remarks>
    void createGrid();
    /// <summary>Checks <paramref name="object"/> against every object of <paramref name="area"/> (skipping pairs
    /// that never collide, such as two bullets).</summary>
    /// <remarks>MCX.EXE @ 0x006570f0</remarks>
    void checkGrid(GameObject* object, CollisionGridNode* area);

    /// <summary>Cells across.</summary>
    uint32_t xGridWidth = 0; // +0x00
    /// <summary>Cells down.</summary>
    uint32_t yGridWidth = 0; // +0x04
    /// <summary>A cell's size in world units.</summary>
    uint32_t gridRadius = 0; // +0x08
    /// <summary>Nodes available.</summary>
    uint32_t maxObjects = 0; // +0x0c
    /// <summary>Objects bigger than a cell.</summary>
    CollisionGridNode* giantObjects = nullptr; // +0x10
    /// <summary>Each cell's list.</summary>
    CollisionGridNode** grid = nullptr; // +0x14
    /// <summary>The node pool.</summary>
    CollisionGridNode* nodes = nullptr; // +0x18
    /// <summary>The next free node.</summary>
    uint32_t nextAvailableNode = 0; // +0x1c
    /// <summary>The grid's origin.</summary>
    vector_3d gridOrigin; // +0x20
    /// <summary>Set once allocated.</summary>
    int gridIsGo = 0; // +0x2c
    /// <summary>Bytes of <see cref="grid"/>.</summary>
    uint32_t gridSize = 0; // +0x30
    /// <summary>Bytes of <see cref="nodes"/>.</summary>
    uint32_t nodeTableSize = 0; // +0x34
    /// <summary>Half the grid's width, in world units (positions are centred on 0).</summary>
    float gridXOffset = 0.0f; // +0x38
    /// <summary>Half the grid's height.</summary>
    float gridYOffset = 0.0f; // +0x3c
    /// <summary>The grid's width in world units.</summary>
    float gridXCheck = 0.0f; // +0x40
    /// <summary>The grid's height.</summary>
    float gridYCheck = 0.0f; // +0x44
};

/// <summary>The collision system: settings from the "CollisionSystem" FIT block, the grid, and the collisions
/// found.</summary>
/// <remarks>Original source: <c>object\collsn.cpp</c>. Allocated from systemHeap.</remarks>
class CollisionSystem
{
public:
    /// <summary>Allocates from systemHeap (null when it isn't up).</summary>
    /// <remarks>MCX.EXE @ 0x00657190</remarks>
    static void* operator new(size_t size) noexcept;
    /// <summary>Frees into systemHeap.</summary>
    /// <remarks>MCX.EXE @ 0x006571c0</remarks>
    static void operator delete(void* ptr);

    /// <summary>Reads the settings, makes the collision heap, the records, the grid and the alerts.</summary>
    /// <remarks>MCX.EXE @ 0x006571e0</remarks>
    int32_t init(FitIniFile* scenarioFile);
    /// <summary>Rebuilds the grid from every list's objects and checks it.</summary>
    /// <remarks>MCX.EXE @ 0x00657430</remarks>
    void checkObjects();
    /// <summary>Checks every pair of each side's mechs for collision alerts.</summary>
    /// <remarks>MCX.EXE @ 0x00657530</remarks>
    void checkAlarums();
    /// <summary>
    /// Whether two objects touch: two movers (classes below 5) by sharing a terrain vertex and cell, anything else
    /// by distance against their extents, then collision alerts.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00657680</remarks>
    void detectCollision(GameObject* obj1, GameObject* obj2);
    /// <summary>A mover against a static object (skipping ones its tile's overlay lets it pass).</summary>
    /// <remarks>MCX.EXE @ 0x006577e0</remarks>
    void detectStaticCollision(GameObject* obj1, GameObject* obj2);
    /// <summary>
    /// Seconds until two mechs (of a side, or with a vehicle) come closest, when within the warning distance;
    /// records an alert when they would touch and <paramref name="setAlert"/>.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00657910</remarks>
    float checkCollisionAlerts(GameObject* obj1, GameObject* obj2, int setAlert);
    /// <summary>Calls both objects' types' collision handlers; an object whose handler asks is removed and
    /// deleted.</summary>
    /// <remarks>MCX.EXE @ 0x00657d50</remarks>
    void checkExtents(GameObject* obj1, GameObject* obj2, float time);
    /// <summary>Handles the pending collision records.</summary>
    /// <remarks>MCX.EXE @ 0x00657e00</remarks>
    void processCollisions();
    /// <summary>Clears <paramref name="object"/> from the records after <paramref name="record"/>.</summary>
    /// <remarks>MCX.EXE @ 0x00657f10</remarks>
    void removeCollisions(GameObject* object, CollisionRecord* record);
    /// <summary>Always null in the original.</summary>
    /// <remarks>MCX.EXE @ 0x00657f40</remarks>
    CollisionRecord* findNextPending();
    /// <summary>Empty in the original.</summary>
    /// <remarks>MCX.EXE @ 0x00657f50</remarks>
    int32_t addPendingCollision(GameObject* obj1, GameObject* obj2, float time);
    /// <summary>Seconds until two moving objects touch within this frame; 0 when already touching.</summary>
    /// <remarks>MCX.EXE @ 0x00657f60</remarks>
    float timeToImpact(GameObject* obj1, GameObject* obj2);
    /// <summary>Deletes the grid, the collision heap and the alerts.</summary>
    /// <remarks>MCX.EXE @ 0x00658290</remarks>
    void destroy();

    /// <summary>The grid.</summary>
    CollisionGrid* collisionGrid = nullptr; // +0x00
    /// <summary>The collision records (maxCollisions, from the collision heap).</summary>
    CollisionRecord* collisionList = nullptr; // +0x04
    /// <summary>Cleared by init.</summary>
    int32_t unknown08 = 0; // +0x08
    /// <summary>The first pending record.</summary>
    CollisionRecord* firstPending = nullptr; // +0x0c
    /// <summary>Cleared by checkObjects.</summary>
    int32_t unknown10 = 0; // +0x10
    /// <summary>Cleared by init.</summary>
    int32_t unknown14 = 0; // +0x14
    /// <summary>Not written by collsn.cpp.</summary>
    int32_t unknown18 = 0; // +0x18
    /// <summary>FIT "MaxPending".</summary>
    uint32_t maxPending = 0; // +0x1c

    /// <summary>FIT "XGridSize": cells across.</summary>
    static uint32_t xGridSize;
    /// <summary>FIT "YGridSize": cells down.</summary>
    static uint32_t yGridSize;
    /// <summary>FIT "GridRadius": a cell's size.</summary>
    static uint32_t gridRadius;
    /// <summary>Grid nodes (FIT "MaxObjects", then forced to 1200).</summary>
    static uint32_t maxObjects;
    /// <summary>FIT "MaxCollisions".</summary>
    static uint32_t maxCollisions;
    /// <summary>Collisions found this frame.</summary>
    static uint32_t numCollisions;
    /// <summary>FIT "WarningDist": squared distance under which movers get collision alerts.</summary>
    static float warningDist;
    /// <summary>FIT "AlertTime".</summary>
    static float alertTime;
    /// <summary>The collision heap (FIT "CollisionHeapSize", forced to 0xffff).</summary>
    static UserHeap* collisionHeap;
};

/// <summary>The collision alerts.</summary>
extern GlobalCollisionAlert* globalCollisionAlert;
