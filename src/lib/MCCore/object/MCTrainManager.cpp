#include "stdafx.h"
#include "object/MCTrainManager.h"
#include "main/MCGameContext.h"
#include "object/MCTrain.h"

MCTrainManager::MCTrainManager() = default;

MCTrainManager::~MCTrainManager() = default;

auto MCTrainManager::CreateTrain() -> MCTrain*
{
    return _Trains.emplace_back(std::make_unique<MCTrain>()).get();
}

auto MCTrainManager::RemoveTrain(MCTrain* train) -> void
{
    if (_Trains.empty())
    {
        return;
    }

    auto position = std::ranges::find(_Trains, train, &std::unique_ptr<MCTrain>::get);

    if (position == _Trains.end())
    {
        position = std::prev(_Trains.end());
    }

    _Removed.push_back(std::move(*position));
    _Trains.erase(position);
}

auto MCTrainManager::Holds(const MCTrain* train) const -> bool
{
    return std::ranges::find(_Trains, train, &std::unique_ptr<MCTrain>::get) != _Trains.end();
}

auto MCTrainManager::UpdateTrains() -> void
{
    for (size_t i = 0; i < _Trains.size(); i++)
    {
        _Trains[i]->Update();
    }
}

auto TrainManager() -> MCTrainManager*
{
    return MCGameContext::Current().TrainManager();
}
