#pragma once

#include "ai/MCTacticalOrder.h"
#include "object/MCPilotAlarm.h"

class MCGameObject;
class MCMechWarrior;
class MCMover;

/// <summary>
/// A lance: up to twelve movers under one id, one of them the point. Orders given to the group go to every member's
/// pilot.
/// </summary>
/// <remarks>Original source: <c>object\group.cpp</c>, <c>object\group.h</c>. A commander owns its groups.</remarks>
class MCMoverGroup : public std::enable_shared_from_this<MCMoverGroup>
{
public:
    /// <summary>Movers a group holds (kept: the interface's group display and the ABL group arrays are sized for
    /// it).</summary>
    static constexpr int32_t MaxMovers = 12;

    /// <summary>Group <paramref name="id"/>, with no members and no point.</summary>
    explicit MCMoverGroup(int32_t id = -1) : Id(id) {}
    MCMoverGroup(const MCMoverGroup&) = delete;
    MCMoverGroup& operator=(const MCMoverGroup&) = delete;

    int32_t GetId() const { return Id; }
    void SetId(int32_t newId) { Id = newId; }
    /// <summary>Adds <paramref name="mover"/> and tells it its group; fatal when full.</summary>
    int Add(std::shared_ptr<MCMover> mover);
    /// <summary>
    /// Removes <paramref name="mover"/> (the last member fills its slot). Removing the point disbands the group.
    /// </summary>
    int Remove(MCMover& mover);
    int IsMember(const MCMover& mover) const;
    /// <summary>Tells every member it has no group, clears the interface's point mark, and empties the group (the
    /// original's name is lost; MechCommander 2's is <c>disband</c>).</summary>
    void Disband();
    /// <summary>Makes member <paramref name="mover"/> the point (and the interface's point mark).</summary>
    int32_t SetPoint(std::shared_ptr<MCMover> mover);
    std::shared_ptr<MCMover> GetPoint() const;
    void SetDisbandOnNoPoint(int setting) { DisbandOnNoPoint = setting; }
    /// <summary>Copies the members to <paramref name="moverList"/>; returns how many.</summary>
    std::vector<std::shared_ptr<MCMover>> GetMovers() const;
    /// <summary>Gives <paramref name="tacOrder"/> to every member (or, jumping, per-member goals).</summary>
    int32_t HandleTacticalOrder(MCTacticalOrder tacOrder, int32_t priority, std::span<const MCVector3D> destinations,
                                int queueGroupOrder);
    /// <summary>
    /// Makes the first member (other than the point, when <paramref name="excludePoint"/>) whose pilot has fewer
    /// than 6 wounds the point; none when no member qualifies.
    /// </summary>
    std::shared_ptr<MCMover> SelectPoint(int excludePoint);
    std::shared_ptr<MCMechWarrior> GetPointPilot() const;
    /// <summary>
    /// Adds the members to <paramref name="counts"/>: [status 0..5] by status, [6] pilot ejected, [7] asleep, [8]
    /// gone (as <c>Team::statusCount</c>, without its checks). The original's name is lost.
    /// </summary>
    void StatusCount(std::span<int32_t> counts);
    /// <summary>Adds every member to the interface's mech list.</summary>
    void AddToGui(int visible);
    /// <summary>The members' jump goals around <paramref name="goal"/> (CalcJumpGoals).</summary>
    int32_t CalcMemberJumpGoals(MCVector3D goal, std::span<MCVector3D> goalList,
                                std::shared_ptr<MCGameObject> dfaTarget) const;
    int32_t OrderMoveToPoint(int setTacOrder, MCOrderOrigin origin, MCVector3D location, uint32_t params);
    int32_t OrderMoveToObject(int setTacOrder, MCOrderOrigin origin, std::shared_ptr<MCGameObject> target,
                              uint32_t params);
    int32_t OrderPowerDown(MCOrderOrigin origin);
    int32_t OrderPowerUp(MCOrderOrigin origin);
    int32_t OrderAttackObject(MCOrderOrigin origin, std::shared_ptr<MCGameObject> target, int32_t attackType,
                              int32_t attackMethod, int32_t attackRange, int32_t aimLocation, uint32_t params);
    int32_t OrderWithdraw(MCOrderOrigin origin, MCVector3D location);
    /// <summary>Triggers alarm <paramref name="alarmCode"/> in every member's pilot.</summary>
    void TriggerAlarm(MCPilotAlarmType alarm, uint32_t triggerId);
    int32_t HandleMateDestroyed(uint32_t mateId);
    static int32_t HandleMateEjected(uint32_t mateId);
    void HandleMateFiredWeapon(uint32_t mateId);
    /// <summary>Member <paramref name="index"/>.</summary>
    std::shared_ptr<MCMover> GetMover(int32_t index) const;
    /// <summary>Members.</summary>
    int32_t NumMovers() const { return static_cast<int32_t>(Movers.size()); }

    /// <summary>The group's id; -1 for none.</summary>
    int32_t Id = -1;
    /// <summary>The members, in the order they joined (a removed member's place goes to the last).</summary>
    std::vector<std::weak_ptr<MCMover>> Movers;
    /// <summary>The point.</summary>
    std::weak_ptr<MCMover> Point;
    /// <summary>Whether losing the point disbands the group.</summary>
    int DisbandOnNoPoint = 0;
};

/// <summary>
/// Picks <paramref name="numGoals"/> jump goals around <paramref name="goal"/>: the nearest open map cells of the 9x9
/// cells around it (skipping blocked cells, bridges' rails and live mechs other than <paramref name="dfaTarget"/>),
/// spiralling out from the goal's cell. A goal with no cell gets -99999 in every coordinate. Returns how many were
/// placed.
/// </summary>
int32_t CalcJumpGoals(MCVector3D goal, int32_t numGoals, std::span<MCVector3D> goalList,
                      std::shared_ptr<MCGameObject> dfaTarget);

/// <summary>The row and column steps (pairs) of the spiral CalcJumpGoals searches, 81 steps.</summary>
extern const std::array<int8_t, 162> CellSpiralIncrement;
