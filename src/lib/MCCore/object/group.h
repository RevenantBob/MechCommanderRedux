#pragma once

#include "ai/tacordr.h"
#include "lib/cvmath.h"

class GameObject;
class MechWarrior;
class Mover;

/// <summary>Maximum movers in a group.</summary>
constexpr int32_t MAX_MOVERGROUP_COUNT = 12;

/// <summary>
/// A lance: up to twelve movers under one id, one of them the point. Orders given to the group go to every member's
/// pilot.
/// </summary>
/// <remarks>Original source: <c>object\group.cpp</c>, <c>object\group.h</c>; 0x44 bytes.</remarks>
class MoverGroup
{
public:
    /// <summary>No id, no members, no point.</summary>
    /// <remarks>MCX.EXE @ 0x006583c0 (inline in <c>object\group.h</c>)</remarks>
    virtual void init();
    /// <remarks>MCX.EXE @ 0x00667710</remarks>
    virtual void destroy();
    /// <remarks>MCX.EXE @ 0x006583e0 (inline in <c>object\group.h</c>)</remarks>
    virtual int32_t getId() { return id; }
    /// <remarks>MCX.EXE @ 0x006583f0 (inline in <c>object\group.h</c>)</remarks>
    virtual void setId(int32_t newId) { id = newId; }
    /// <summary>Adds <paramref name="mover"/> and tells it its group; fatal when full.</summary>
    /// <remarks>MCX.EXE @ 0x00667720</remarks>
    virtual int add(Mover* mover);
    /// <summary>
    /// Removes <paramref name="mover"/> (the last member fills its slot). Removing the point disbands the group.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00667770</remarks>
    virtual int remove(Mover* mover);
    /// <remarks>MCX.EXE @ 0x006677f0</remarks>
    virtual int isMember(Mover* mover);
    /// <summary>Tells every member it has no group, clears the interface's point mark, and empties the group (the
    /// original's name is lost; MechCommander 2's is <c>disband</c>).</summary>
    /// <remarks>MCX.EXE @ 0x00667820</remarks>
    virtual void disband();
    /// <summary>Makes member <paramref name="mover"/> the point (and the interface's point mark).</summary>
    /// <remarks>MCX.EXE @ 0x00667870</remarks>
    virtual int32_t setPoint(Mover* mover);
    /// <remarks>MCX.EXE @ 0x00658400 (inline in <c>object\group.h</c>)</remarks>
    virtual Mover* getPoint() { return point; }
    /// <remarks>MCX.EXE @ 0x00658410 (inline in <c>object\group.h</c>)</remarks>
    virtual void setDisbandOnNoPoint(int setting) { disbandOnNoPoint = setting; }
    /// <remarks>MCX.EXE @ 0x00658420 (inline in <c>object\group.h</c>)</remarks>
    virtual int getDisbandOnNoPoint() { return disbandOnNoPoint; }
    /// <summary>Copies the members to <paramref name="moverList"/>; returns how many.</summary>
    /// <remarks>MCX.EXE @ 0x00667940</remarks>
    virtual int32_t getMovers(Mover** moverList);
    /// <summary>Gives <paramref name="tacOrder"/> to every member (or, jumping, per-member goals).</summary>
    /// <remarks>MCX.EXE @ 0x00667f40</remarks>
    virtual int32_t handleTacticalOrder(TacticalOrder tacOrder, int32_t priority, vector_3d* destinations,
                                        int queueGroupOrder);

    /// <summary>
    /// Makes the first member (other than the point, when <paramref name="excludePoint"/>) whose pilot has fewer
    /// than 6 wounds the point; none when no member qualifies.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x006678c0</remarks>
    Mover* selectPoint(int excludePoint);
    /// <remarks>MCX.EXE @ 0x00667960</remarks>
    MechWarrior* getPointPilot();
    /// <summary>
    /// Adds the members to <paramref name="counts"/>: [status 0..5] by status, [6] pilot ejected, [7] asleep, [8]
    /// gone (as <c>Team::statusCount</c>, without its checks). The original's name is lost.
    /// </summary>
    /// <remarks>MCX.EXE @ 0x00667980</remarks>
    void statusCount(int32_t* counts);
    /// <summary>Adds every member to the interface's mech list.</summary>
    /// <remarks>MCX.EXE @ 0x00667a10</remarks>
    void addToGUI(int visible);
    /// <summary>The members' jump goals around <paramref name="goal"/> (CalcJumpGoals).</summary>
    /// <remarks>MCX.EXE @ 0x00667f10</remarks>
    int32_t calcJumpGoals(vector_3d goal, vector_3d* goalList, GameObject* dfaTarget);
    /// <remarks>MCX.EXE @ 0x006686f0</remarks>
    int32_t orderMoveToPoint(int setTacOrder, int32_t origin, vector_3d location, uint32_t params);
    /// <remarks>MCX.EXE @ 0x00668770</remarks>
    int32_t orderMoveToObject(int setTacOrder, int32_t origin, GameObject* target, uint32_t params);
    /// <remarks>MCX.EXE @ 0x006687f0</remarks>
    int32_t orderTraversePath(int32_t origin, _WayPath* wayPath, uint32_t params);
    /// <summary>The original's name is lost (its assert says <c>orderPatrolPath</c>).</summary>
    /// <remarks>MCX.EXE @ 0x00668870</remarks>
    int32_t orderPatrolPath(int32_t origin, _WayPath* wayPath);
    /// <remarks>MCX.EXE @ 0x006688f0</remarks>
    int32_t orderPowerDown(int32_t origin);
    /// <remarks>MCX.EXE @ 0x00668960</remarks>
    int32_t orderPowerUp(int32_t origin);
    /// <remarks>MCX.EXE @ 0x006689d0</remarks>
    int32_t orderAttackObject(int32_t origin, GameObject* target, int32_t attackType, int32_t attackMethod,
                              int32_t attackRange, int32_t aimLocation, uint32_t params);
    /// <remarks>MCX.EXE @ 0x00668a70</remarks>
    int32_t orderWithdraw(int32_t origin, vector_3d location);
    /// <remarks>MCX.EXE @ 0x00668af0</remarks>
    int32_t orderEject(int32_t origin);
    /// <summary>Triggers alarm <paramref name="alarmCode"/> in every member's pilot.</summary>
    /// <remarks>MCX.EXE @ 0x00668b60</remarks>
    void triggerAlarm(int32_t alarmCode, uint32_t triggerId);
    /// <summary>Alarm 4 for every member.</summary>
    /// <remarks>MCX.EXE @ 0x00668ba0</remarks>
    int32_t handleMateCrippled(uint32_t mateId);
    /// <remarks>MCX.EXE @ 0x00668bc0</remarks>
    int32_t handleMateDisabled(uint32_t mateId);
    /// <remarks>MCX.EXE @ 0x00668bd0</remarks>
    int32_t handleMateDestroyed(uint32_t mateId);
    /// <remarks>MCX.EXE @ 0x00668bf0</remarks>
    int32_t handleMateEjected(uint32_t mateId);
    /// <remarks>MCX.EXE @ 0x00668c00</remarks>
    void handleMateFiredWeapon(uint32_t mateId);
    /// <summary>Member <paramref name="index"/>.</summary>
    /// <remarks>MCX.EXE @ 0x006d3ff0 (inline in <c>object\group.h</c>)</remarks>
    Mover* getMover(int32_t index) { return movers[index]; }

    /// <summary>The group's id; -1 for none.</summary>
    int32_t id = -1; // +0x04
    /// <summary>Members.</summary>
    int32_t numMovers = 0; // +0x08
    /// <summary>The members.</summary>
    Mover* movers[MAX_MOVERGROUP_COUNT] = {}; // +0x0c
    /// <summary>The point.</summary>
    Mover* point = nullptr; // +0x3c
    /// <summary>Whether losing the point disbands the group.</summary>
    int disbandOnNoPoint = 0; // +0x40
};

/// <summary>
/// Picks <paramref name="numGoals"/> jump goals around <paramref name="goal"/>: the nearest open map cells of the 9x9
/// cells around it (skipping blocked cells, bridges' rails and live mechs other than <paramref name="dfaTarget"/>),
/// spiralling out from the goal's cell. A goal with no cell gets -99999 in every coordinate. Returns how many were
/// placed.
/// </summary>
/// <remarks>MCX.EXE @ 0x00667a60</remarks>
int32_t CalcJumpGoals(vector_3d goal, int32_t numGoals, vector_3d* goalList, GameObject* dfaTarget);

/// <summary>The row and column steps (pairs) of the spiral CalcJumpGoals searches, 81 steps.</summary>
extern char CellSpiralIncrement[162];
