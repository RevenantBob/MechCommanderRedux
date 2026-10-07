#pragma once

#include "ai/tacordr.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"

class MCGameObject;
class MCMechWarrior;
class MCMover;

/// <summary>Maximum movers in a group.</summary>
constexpr int32_t MAX_MOVERGROUP_COUNT = 12;

/// <summary>
/// A lance: up to twelve movers under one id, one of them the point. Orders given to the group go to every member's
/// pilot.
/// </summary>
/// <remarks>Original source: <c>object\group.cpp</c>, <c>object\group.h</c>; 0x44 bytes.</remarks>
class MCMoverGroup
{
public:
    /// <summary>No id, no members, no point.</summary>
    virtual void Init();
    virtual void Destroy();
    virtual int32_t GetId() { return Id; }
    virtual void SetId(int32_t newId) { Id = newId; }
    /// <summary>Adds <paramref name="mover"/> and tells it its group; fatal when full.</summary>
    virtual int Add(MCMover* mover);
    /// <summary>
    /// Removes <paramref name="mover"/> (the last member fills its slot). Removing the point disbands the group.
    /// </summary>
    virtual int Remove(MCMover* mover);
    virtual int IsMember(MCMover* mover);
    /// <summary>Tells every member it has no group, clears the interface's point mark, and empties the group (the
    /// original's name is lost; MechCommander 2's is <c>disband</c>).</summary>
    virtual void Disband();
    /// <summary>Makes member <paramref name="mover"/> the point (and the interface's point mark).</summary>
    virtual int32_t SetPoint(MCMover* mover);
    virtual MCMover* GetPoint() { return Point; }
    virtual void SetDisbandOnNoPoint(int setting) { DisbandOnNoPoint = setting; }
    virtual int GetDisbandOnNoPoint() { return DisbandOnNoPoint; }
    /// <summary>Copies the members to <paramref name="moverList"/>; returns how many.</summary>
    virtual int32_t GetMovers(MCMover** moverList);
    /// <summary>Gives <paramref name="tacOrder"/> to every member (or, jumping, per-member goals).</summary>
    virtual int32_t HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, MCVector3D* destinations,
                                        int queueGroupOrder);

    /// <summary>
    /// Makes the first member (other than the point, when <paramref name="excludePoint"/>) whose pilot has fewer
    /// than 6 wounds the point; none when no member qualifies.
    /// </summary>
    MCMover* SelectPoint(int excludePoint);
    MCMechWarrior* GetPointPilot();
    /// <summary>
    /// Adds the members to <paramref name="counts"/>: [status 0..5] by status, [6] pilot ejected, [7] asleep, [8]
    /// gone (as <c>Team::statusCount</c>, without its checks). The original's name is lost.
    /// </summary>
    void StatusCount(int32_t* counts);
    /// <summary>Adds every member to the interface's mech list.</summary>
    void AddToGui(int visible);
    /// <summary>The members' jump goals around <paramref name="goal"/> (CalcJumpGoals).</summary>
    int32_t CalcMemberJumpGoals(MCVector3D goal, MCVector3D* goalList, MCGameObject* dfaTarget);
    int32_t OrderMoveToPoint(int setTacOrder, int32_t origin, MCVector3D location, uint32_t params);
    int32_t OrderMoveToObject(int setTacOrder, int32_t origin, MCGameObject* target, uint32_t params);
    int32_t OrderTraversePath(int32_t origin, MCWayPath* wayPath, uint32_t params);
    /// <summary>The original's name is lost (its assert says <c>orderPatrolPath</c>).</summary>
    int32_t OrderPatrolPath(int32_t origin, MCWayPath* wayPath);
    int32_t OrderPowerDown(int32_t origin);
    int32_t OrderPowerUp(int32_t origin);
    int32_t OrderAttackObject(int32_t origin, MCGameObject* target, int32_t attackType, int32_t attackMethod,
                              int32_t attackRange, int32_t aimLocation, uint32_t params);
    int32_t OrderWithdraw(int32_t origin, MCVector3D location);
    int32_t OrderEject(int32_t origin);
    /// <summary>Triggers alarm <paramref name="alarmCode"/> in every member's pilot.</summary>
    void TriggerAlarm(int32_t alarmCode, uint32_t triggerId);
    /// <summary>Alarm 4 for every member.</summary>
    int32_t HandleMateCrippled(uint32_t mateId);
    int32_t HandleMateDisabled(uint32_t mateId);
    int32_t HandleMateDestroyed(uint32_t mateId);
    int32_t HandleMateEjected(uint32_t mateId);
    void HandleMateFiredWeapon(uint32_t mateId);
    /// <summary>Member <paramref name="index"/>.</summary>
    MCMover* GetMover(int32_t index) { return Movers[index]; }

    /// <summary>The group's id; -1 for none.</summary>
    int32_t Id = -1;
    /// <summary>Members.</summary>
    int32_t NumMovers = 0;
    /// <summary>The members.</summary>
    MCMover* Movers[MAX_MOVERGROUP_COUNT] = {};
    /// <summary>The point.</summary>
    MCMover* Point = nullptr;
    /// <summary>Whether losing the point disbands the group.</summary>
    int DisbandOnNoPoint = 0;
};

/// <summary>
/// Picks <paramref name="numGoals"/> jump goals around <paramref name="goal"/>: the nearest open map cells of the 9x9
/// cells around it (skipping blocked cells, bridges' rails and live mechs other than <paramref name="dfaTarget"/>),
/// spiralling out from the goal's cell. A goal with no cell gets -99999 in every coordinate. Returns how many were
/// placed.
/// </summary>
int32_t CalcJumpGoals(MCVector3D goal, int32_t numGoals, MCVector3D* goalList, MCGameObject* dfaTarget);

/// <summary>The row and column steps (pairs) of the spiral CalcJumpGoals searches, 81 steps.</summary>
extern char CellSpiralIncrement[162];
