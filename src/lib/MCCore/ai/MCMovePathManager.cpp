#include "stdafx.h"
#include "ai/MCMovePathManager.h"
#include "object/MCMechWarrior.h"
#include "object/MCMoverGameSystem.h"

auto MCMovePathManager::Remove(const MCPathQueueRec& rec) -> void
{
    _Queue.remove_if([&rec](const std::shared_ptr<MCPathQueueRec>& queued) { return queued.get() == &rec; });
}

auto MCMovePathManager::Remove(MCMechWarrior& pilot) -> void
{
    const std::shared_ptr<MCPathQueueRec> request = pilot.MovePathRequest.lock();

    if (request == nullptr)
    {
        return;
    }

    Remove(*request);
    pilot.MovePathRequest.reset();
}

auto MCMovePathManager::Request(MCMechWarrior& pilot, int32_t selectionIndex, uint32_t moveParams, float priority,
                                int32_t initPath) -> void
{
    Remove(pilot);

    // Behind the last request of a higher priority (so ahead of those of the same priority).
    const auto before = std::ranges::find_if(_Queue, [priority](const std::shared_ptr<MCPathQueueRec>& queued)
                                             { return !(priority < queued->Priority); });
    const auto rec = _Queue.insert(before, std::make_shared<MCPathQueueRec>(priority, pilot.shared_from_this(),
                                                                            selectionIndex, moveParams, initPath));
    pilot.MovePathRequest = *rec;
}

auto MCMovePathManager::CalcPath() -> void
{
    if (_Queue.empty())
    {
        return;
    }

    const std::shared_ptr<MCPathQueueRec> rec = _Queue.front();
    _Queue.pop_front();
    std::shared_ptr<MCMechWarrior> pilot = rec->Pilot.lock();
    pilot->MovePathRequest.reset();

    if (pilot->GetVehicle() != nullptr)
    {
        pilot->CalcMovePath(rec->SelectionIndex, rec->MoveParams, rec->InitPath);
    }
}

auto MCMovePathManager::Update() -> void
{
    for (int32_t i = 0; i < RequestsPerUpdate && !_Queue.empty(); i++)
    {
        CalcPath();
    }
}
