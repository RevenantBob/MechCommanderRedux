#include "stdafx.h"
#include "object/MCTrainManager.h"
#include "main/MCGameContext.h"
#include "object/MCTrain.h"

MCTrainManager::MCTrainManager() = default;

MCTrainManager::~MCTrainManager() = default;

auto MCTrainManager::CreateTrain() -> std::shared_ptr<MCTrain>
{
    return _Trains.emplace_back(std::make_shared<MCTrain>());
}

auto MCTrainManager::RemoveTrain(MCTrain& train) -> void
{
    if (_Trains.empty())
    {
        return;
    }

    auto position =
        std::ranges::find_if(_Trains, [&train](const std::shared_ptr<MCTrain>& held) { return held.get() == &train; });

    if (position == _Trains.end())
    {
        position = std::prev(_Trains.end());
    }

    _Removed.push_back(std::move(*position));
    _Trains.erase(position);
}

auto MCTrainManager::Holds(const MCTrain& train) const -> bool
{
    return std::ranges::any_of(_Trains,
                               [&train](const std::shared_ptr<MCTrain>& held) { return held.get() == &train; });
}

auto MCTrainManager::UpdateTrains() -> void
{
    for (size_t i = 0; i < _Trains.size(); i++)
    {
        _Trains[i]->Update();
    }
}

auto TrainManager() -> MCTrainManager&
{
    return MCGameContext::Current().TrainManager();
}
