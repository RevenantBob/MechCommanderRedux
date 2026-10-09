#pragma once

class MCTrain;

/// <summary>
/// The mission's trains: it makes them, updates the ones running each frame, and keeps those that left the list (a
/// train whose last car left, or that fell into the water) until the mission ends, since their cars may still point
/// at them. A game system of <see cref="MCGameContext"/> (TrainManager()), made by the scenario when it has trains.
/// </summary>
/// <remarks>
/// Original source: <c>object\train.cpp</c>. The original held at most 64 trains (a full list made no train, and the
/// split that asked for one crashed); the port has no limit.
/// </remarks>
class MCTrainManager
{
public:
    MCTrainManager();
    ~MCTrainManager();
    MCTrainManager(const MCTrainManager&) = delete;
    MCTrainManager& operator=(const MCTrainManager&) = delete;

    /// <summary>Makes a new empty train at the end of the list.</summary>
    MCTrain* CreateTrain();
    /// <summary>
    /// Takes a train out of the list (it is kept, not updated). Faithful: when the train isn't in the list, the last
    /// one drops out instead (the original lowered its count either way).
    /// </summary>
    void RemoveTrain(MCTrain* train);
    /// <summary>Whether <paramref name="train"/> is in the list.</summary>
    bool Holds(const MCTrain* train) const;
    /// <summary>Updates every train in the list.</summary>
    void UpdateTrains();
    /// <summary>How many trains are in the list.</summary>
    size_t NumTrains() const { return _Trains.size(); }

private:
    /// <summary>The trains running, in the order they were made.</summary>
    std::vector<std::unique_ptr<MCTrain>> _Trains;
    /// <summary>The trains taken out of the list.</summary>
    std::vector<std::unique_ptr<MCTrain>> _Removed;
};

/// <summary>The mission's trains (null outside a mission, and in a mission without trains).</summary>
MCTrainManager* TrainManager();
