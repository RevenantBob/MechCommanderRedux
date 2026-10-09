#pragma once

#include "lib/MCVector3D.h"

class MCTrainCar;

/// <summary>
/// A train: a list of cars moved together along the track, with a speed limited by its slowest car. The mission's
/// <see cref="MCTrainManager"/> owns its trains.
/// </summary>
/// <remarks>Original source: <c>object\train.cpp</c>.</remarks>
class MCTrain
{
public:
    /// <summary>
    /// Accelerates or brakes toward the desired speed (stopping if any car derailed), then moves every car along
    /// the track, moving the cars' path locks on the move map with them.
    /// </summary>
    void Update();
    /// <summary>Appends a car (placing it <see cref="CarOffset"/> behind the last one).</summary>
    /// <returns>The number of cars, or an error when the object is not a train car.</returns>
    int32_t AddCar(MCTrainCar* car);
    /// <summary>
    /// Removes a car. Without <paramref name="justUnlink"/> the car and those behind it become new trains; a train
    /// left with no cars leaves the train manager's list.
    /// </summary>
    /// <returns>The number of cars left, or -1 when the car isn't in the train.</returns>
    int32_t RemoveCar(MCTrainCar* car, bool justUnlink);
    /// <summary>Recomputes the limits from the cars and the lead car's position, and clamps the desired speed.</summary>
    void RecalcInfo();
    /// <summary>The sum of the cars' tonnage.</summary>
    float GetTotalTonnage();
    /// <summary>
    /// Empties the car list, as the original's destroy left it (it walked the list's entries without freeing them);
    /// the car count stays.
    /// </summary>
    void ForgetCars() { Cars.clear(); }

    /// <summary>The cars, the lead car first.</summary>
    std::vector<MCTrainCar*> Cars;
    /// <summary>How many cars the train has (what the original counted, which can outlive the list).</summary>
    int32_t NumCars = 0;
    /// <summary>The current speed (negative when backing).</summary>
    float Speed = 0;
    /// <summary>The best acceleration of the cars.</summary>
    float MaxAccel = 0;
    /// <summary>The best deceleration of the cars.</summary>
    float MaxDecel = 0;
    /// <summary>The lowest top speed of the cars.</summary>
    float MaxSpeed = 0;
    /// <summary>The speed the train drives toward (set by the scenario; clamped to +/- maxSpeed).</summary>
    float DesiredSpeed = 0;
    /// <summary>
    /// The track direction in degrees, set by the scenario; -45 and 135 mark cells along one axis, anything else the
    /// other, when the cars' path locks are moved.
    /// </summary>
    int32_t TrackDirection = 0;
    /// <summary>The lead car's position at the last RecalcInfo.</summary>
    MCVector3D LeadPosition;
};

/// <summary>The distance between two cars of a train (added along the track axis by <see cref="MCTrain::AddCar"/>).</summary>
inline constexpr float CarOffset = 84.0f;

/// <summary>
/// Sets (<paramref name="locked"/> 1) or clears the path locks of the three map cells a train car covers, from the cell
/// under it: down a column (one right of it) for the -45/135 tracks, along a row (one below it) for the others.
/// </summary>
void LockTrackCells(MCTrainCar* car, int32_t trackDirection, uint32_t locked);
