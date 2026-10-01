#pragma once

#include "lib/cvmath.h"

class Mover;
class MoverGroup;
class TacticalOrder;

/// <summary>
/// One point of a way path the player lays down (shift-clicks) before giving a patrol or traverse order; a
/// singly linked list, handed to <c>TacticalOrder::initWayPath</c>.
/// </summary>
/// <remarks>0x14 bytes, allocated with plain <c>new</c>.</remarks>
struct LocationNode
{
    vector_3d location; // +0x0
    /// <summary>Whether to run to this point.</summary>
    int run = 0;                  // +0xc
    LocationNode* next = nullptr; // +0x10
};

/// <summary>
/// The command parser: holds the "subjects" of the player's next order (up to 12 movers by part id and 4 lances)
/// and the way path being laid, and sends a <see cref="TacticalOrder"/> to each subject (or over the network).
/// </summary>
/// <remarks>Original source: <c>iface\parser.cpp</c>, 0x50 bytes, no vtable.</remarks>
class Parser
{
public:
    /// <summary>Returned by AddSubject when the subject list is full (0xBBBB0000 as a long).</summary>
    static constexpr int32_t TOO_MANY_SUBJECTS = static_cast<int32_t>(0xBBBB0000);

    /// <remarks>MCX.EXE @ 0x006d3530</remarks>
    Parser();

    /// <summary>Clears the subjects and the way path.</summary>
    /// <remarks>MCX.EXE @ 0x006d3580</remarks>
    void init();

    /// <summary>
    /// Adds the mover with part id <paramref name="partId"/> (once). <paramref name="addToExisting"/> (1 for a
    /// shift-click, 0 otherwise) is not read.
    /// </summary>
    /// <returns>0, or <see cref="TOO_MANY_SUBJECTS"/>.</returns>
    /// <remarks>MCX.EXE @ 0x006d35d0</remarks>
    int32_t AddSubject(int32_t partId, int addToExisting);
    /// <summary>Adds a lance (once), dropping its movers from the single subjects.</summary>
    /// <returns>0, or <see cref="TOO_MANY_SUBJECTS"/>.</returns>
    /// <remarks>MCX.EXE @ 0x006d3630</remarks>
    int32_t AddSubject(MoverGroup* group, int addToExisting);
    /// <summary>The part id of subject <paramref name="index"/>, 0 past 12.</summary>
    /// <remarks>MCX.EXE @ 0x006d3700</remarks>
    int32_t GetSubject(int16_t index);
    /// <returns>-1 (true) or 0.</returns>
    /// <remarks>MCX.EXE @ 0x006d3720</remarks>
    int IsSubject(int32_t partId);
    /// <returns>-1 (true) or 0.</returns>
    /// <remarks>MCX.EXE @ 0x006d3770</remarks>
    int IsSubject(MoverGroup* group);
    /// <summary>
    /// Removes the mover with part id <paramref name="partId"/>; if it is only a subject through its lance, the
    /// lance is replaced by its other movers.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d37b0</remarks>
    void RemoveSubject(int32_t partId);
    /// <remarks>MCX.EXE @ 0x006d3960</remarks>
    void RemoveSubject(MoverGroup* group);
    /// <remarks>MCX.EXE @ 0x006d39d0</remarks>
    void ClearSubjects();

    /// <summary>Appends a point to the way path.</summary>
    /// <returns>-1 (true), or 0 when out of memory.</returns>
    /// <remarks>MCX.EXE @ 0x006d3a00</remarks>
    int AddObjectLoc(vector_3d location, int run);

    /// <summary>
    /// Sends <paramref name="order"/> (with the way path, if any) to every subject; with
    /// <paramref name="sortMovers"/> the movers are ranked by distance to the goal first (<c>SortMoverList</c>).
    /// Jump orders get per-mover goals (<c>CalcJumpGoals</c>). In multiplayer the order goes to the server.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006d3b90</remarks>
    int SendTacOrder(TacticalOrder order, int sortMovers);
    /// <summary>Sends a patrol order along the way path.</summary>
    /// <remarks>MCX.EXE @ 0x006d4000</remarks>
    void PatrolUp();
    /// <summary>Sends a traverse order along the way path.</summary>
    /// <remarks>MCX.EXE @ 0x006d4090</remarks>
    void TraverseUp();

protected:
    /// <summary>Frees the way path.</summary>
    /// <remarks>MCX.EXE @ 0x006d4120</remarks>
    void ClearMovePath();

public:
    /// <summary>Part ids of the single movers ordered.</summary>
    int32_t subjects[12] = {}; // +0x0
    /// <summary>The lances ordered.</summary>
    MoverGroup* groupSubjects[4] = {}; // +0x30
    /// <summary>Never referenced.</summary>
    int32_t unknown40 = 0;         // +0x40
    int32_t unknown44 = 0;         // +0x44
    uint16_t numSubjects = 0;      // +0x48
    uint16_t numGroupSubjects = 0; // +0x4a
    /// <summary>The way path being laid.</summary>
    LocationNode* movePath = nullptr; // +0x4c
};

/// <summary>qsort order of (mover, distance) pairs by distance.</summary>
/// <remarks>MCX.EXE @ 0x006d3a70</remarks>
int CompareDistance(const void* a, const void* b);

/// <summary>
/// Ranks <paramref name="numMovers"/> movers by distance to <paramref name="goal"/>, storing each one's rank in
/// the mover (its formation position).
/// </summary>
/// <remarks>MCX.EXE @ 0x006d3ab0</remarks>
void SortMoverList(int32_t numMovers, Mover** movers, vector_3d goal);
