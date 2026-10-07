#pragma once

#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"

class MCMover;
class MCMoverGroup;
class MCTacticalOrder;

/// <summary>
/// One point of a way path the player lays down (shift-clicks) before giving a patrol or traverse order; a
/// singly linked list, handed to <c>TacticalOrder::initWayPath</c>.
/// </summary>
/// <remarks>0x14 bytes, allocated with plain <c>new</c>.</remarks>
struct MCLocationNode
{
    MCVector3D Location;
    /// <summary>Whether to run to this point.</summary>
    int Run = 0;
    MCLocationNode* Next = nullptr;
};

/// <summary>
/// The command parser: holds the "subjects" of the player's next order (up to 12 movers by part id and 4 lances)
/// and the way path being laid, and sends a <see cref="MCTacticalOrder"/> to each subject (or over the network).
/// </summary>
/// <remarks>Original source: <c>iface\parser.cpp</c>, 0x50 bytes, no vtable.</remarks>
class MCParser
{
public:
    /// <summary>Returned by AddSubject when the subject list is full (0xBBBB0000 as a long).</summary>
    static constexpr int32_t TOO_MANY_SUBJECTS = static_cast<int32_t>(0xBBBB0000);

    MCParser();

    /// <summary>Clears the subjects and the way path.</summary>
    void Init();

    /// <summary>
    /// Adds the mover with part id <paramref name="partId"/> (once). <paramref name="addToExisting"/> (1 for a
    /// shift-click, 0 otherwise) is not read.
    /// </summary>
    /// <returns>0, or <see cref="TOO_MANY_SUBJECTS"/>.</returns>
    int32_t AddSubject(int32_t partId, int addToExisting);
    /// <summary>Adds a lance (once), dropping its movers from the single subjects.</summary>
    /// <returns>0, or <see cref="TOO_MANY_SUBJECTS"/>.</returns>
    int32_t AddSubject(MCMoverGroup* group, int addToExisting);
    /// <summary>The part id of subject <paramref name="index"/>, 0 past 12.</summary>
    int32_t GetSubject(int16_t index);
    /// <returns>-1 (true) or 0.</returns>
    int IsSubject(int32_t partId);
    /// <returns>-1 (true) or 0.</returns>
    int IsSubject(MCMoverGroup* group);
    /// <summary>
    /// Removes the mover with part id <paramref name="partId"/>; if it is only a subject through its lance, the
    /// lance is replaced by its other movers.
    /// </summary>
    void RemoveSubject(int32_t partId);
    void RemoveSubject(MCMoverGroup* group);
    void ClearSubjects();

    /// <summary>Appends a point to the way path.</summary>
    /// <returns>-1 (true), or 0 when out of memory.</returns>
    int AddObjectLoc(MCVector3D location, int run);

    /// <summary>
    /// Sends <paramref name="order"/> (with the way path, if any) to every subject; with
    /// <paramref name="sortMovers"/> the movers are ranked by distance to the goal first (<c>SortMoverList</c>).
    /// Jump orders get per-mover goals (<c>CalcJumpGoals</c>). In multiplayer the order goes to the server.
    /// </summary>
    int SendTacOrder(MCTacticalOrder order, int sortMovers);
    /// <summary>Sends a patrol order along the way path.</summary>
    void PatrolUp();
    /// <summary>Sends a traverse order along the way path.</summary>
    void TraverseUp();

protected:
    /// <summary>Frees the way path.</summary>
    void ClearMovePath();

public:
    /// <summary>Part ids of the single movers ordered.</summary>
    int32_t Subjects[12] = {};
    /// <summary>The lances ordered.</summary>
    MCMoverGroup* GroupSubjects[4] = {};
    uint16_t NumSubjects = 0;
    uint16_t NumGroupSubjects = 0;
    /// <summary>The way path being laid.</summary>
    MCLocationNode* MovePath = nullptr;
};

/// <summary>qsort order of (mover, distance) pairs by distance.</summary>
int CompareDistance(const void* a, const void* b);

/// <summary>
/// Ranks <paramref name="numMovers"/> movers by distance to <paramref name="goal"/>, storing each one's rank in
/// the mover (its formation position).
/// </summary>
void SortMoverList(int32_t numMovers, MCMover** movers, MCVector3D goal);
