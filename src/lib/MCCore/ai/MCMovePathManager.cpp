#include "stdafx.h"
#include "ai/MCMovePathManager.h"
#include "object/warrior.h"

auto MCMovePathManager::Remove(const MCPathQueueRec* rec) -> void
{
    _Queue.remove_if([rec](const MCPathQueueRec& queued) { return &queued == rec; });
}

auto MCMovePathManager::Remove(MCMechWarrior* pilot) -> void
{
    if (pilot->MovePathRequest == nullptr)
    {
        return;
    }

    Remove(pilot->MovePathRequest);
    pilot->MovePathRequest = nullptr;
}

auto MCMovePathManager::Request(MCMechWarrior* pilot, int32_t selectionIndex, uint32_t moveParams, float priority,
                                int32_t initPath) -> void
{
    Remove(pilot);

    // Behind the last request of a higher priority (so ahead of those of the same priority).
    const auto before = std::find_if(_Queue.begin(), _Queue.end(), [priority](const MCPathQueueRec& queued)
                                     { return !(priority < queued.Priority); });
    const auto rec = _Queue.insert(before, MCPathQueueRec{priority, pilot, selectionIndex, moveParams, initPath});
    pilot->MovePathRequest = &*rec;
}

auto MCMovePathManager::CalcPath() -> void
{
    if (_Queue.empty())
    {
        return;
    }

    const MCPathQueueRec rec = _Queue.front();
    _Queue.pop_front();
    MCMechWarrior* pilot = rec.Pilot;
    pilot->MovePathRequest = nullptr;

    if (pilot->Vehicle != nullptr)
    {
        pilot->CalcMovePath(rec.SelectionIndex, rec.MoveParams, rec.InitPath);
    }
}

auto MCMovePathManager::Update() -> void
{
    for (int32_t i = 0; i < RequestsPerUpdate && !_Queue.empty(); i++)
    {
        CalcPath();
    }
}
