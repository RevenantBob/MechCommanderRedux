#include "stdafx.h"
#include "object/MCTrain.h"
#include "ai/MCMoveSystem.h"
#include "ai/MCScenarioMap.h"
#include "lib/MCFatal.h"
#include "main/MCMissionGlobals.h"
#include "object/MCTrainCar.h"
#include "object/MCTrainManager.h"

namespace
{
    /// <summary>The track directions that run down the map's rows (the others run along its columns).</summary>
    constexpr int32_t TrackDiagonalA = -45;
    constexpr int32_t TrackDiagonalB = 135;
} // namespace

void LockTrackCells(MCTrainCar& car, int32_t trackDirection, uint32_t locked)
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    MCScenarioMap::WorldToMapPos(car.GetPosition(), tileR, tileC, cellR, cellC);
    // The cell index is not wrapped at the tile's edge (a car in the last column locks index row * 3 + 3).
    const auto lock = [&]
    {
        MCMapTile& tile = GameMap().Map[GameMap().Width * tileR + tileC];
        const auto shift = static_cast<uint32_t>(cellC + cellR * 3);
        tile.Overlay = (locked << ((shift + 0xf) & 0x1f)) | (~(0x8000u << (shift & 0x1f)) & tile.Overlay);
    };

    const bool downColumn = trackDirection == TrackDiagonalA || trackDirection == TrackDiagonalB;
    int32_t& along = downColumn ? cellR : cellC;
    int32_t& alongTile = downColumn ? tileR : tileC;

    if (downColumn)
    {
        cellC++;
    }
    else
    {
        cellR++;
    }

    for (int32_t i = 0; i < 3; i++)
    {
        if (i != 0)
        {
            if (along == 2)
            {
                along = 0;
                alongTile++;
            }
            else
            {
                along++;
            }
        }

        lock();
    }
}

auto MCTrain::PinCars() const -> std::vector<std::shared_ptr<MCTrainCar>>
{
    std::vector<std::shared_ptr<MCTrainCar>> cars;

    for (const std::weak_ptr<MCTrainCar>& link : Cars)
    {
        if (std::shared_ptr<MCTrainCar> car = LockObject(link))
        {
            cars.push_back(std::move(car));
        }
    }

    return cars;
}

auto MCTrain::Update() -> void
{
    const std::vector<std::shared_ptr<MCTrainCar>> cars = PinCars();

    if (cars.empty())
    {
        return;
    }

    // A derailed car brakes the whole train; otherwise it speeds toward desiredSpeed (maxAccel, and maxDecel to
    // slow or to come back through zero) within maxSpeed.
    const bool anyDerailed =
        std::ranges::any_of(cars, [](const std::shared_ptr<MCTrainCar>& car) { return car->Derailed; });
    bool stop = false;

    if (!anyDerailed)
    {
        if (MaxAccel <= 0.0f)
        {
            if (0.0f < Speed)
            {
                Speed = static_cast<float>(static_cast<double>(FrameLength) * MaxAccel + Speed);

                if (Speed < 0.0f)
                {
                    Speed = 0.0f;
                }
            }

            if (Speed < 0.0f)
            {
                Speed = static_cast<float>(Speed - static_cast<double>(FrameLength) * MaxAccel);
                stop = 0.0f < Speed;
            }
        }
        else if (DesiredSpeed <= Speed)
        {
            if (DesiredSpeed < Speed)
            {
                if (Speed <= 0.0f)
                {
                    Speed = static_cast<float>(Speed - static_cast<double>(FrameLength) * MaxAccel);

                    if (Speed < DesiredSpeed)
                    {
                        Speed = DesiredSpeed;
                    }

                    if (MaxSpeed < -Speed)
                    {
                        Speed = -MaxSpeed;
                    }
                }
                else
                {
                    Speed = static_cast<float>(Speed - static_cast<double>(FrameLength) * MaxDecel);
                    stop = Speed < 0.0f;
                }
            }
        }
        else if (0.0f <= Speed)
        {
            Speed = static_cast<float>(static_cast<double>(FrameLength) * MaxAccel + Speed);

            if (DesiredSpeed < Speed)
            {
                Speed = DesiredSpeed;
            }

            if (MaxSpeed < Speed)
            {
                Speed = MaxSpeed;
            }
        }
        else
        {
            Speed = static_cast<float>(static_cast<double>(FrameLength) * MaxDecel + Speed);
            stop = 0.0f < Speed;
        }
    }
    else
    {
        if (0.0f < Speed)
        {
            Speed = static_cast<float>(Speed - static_cast<double>(FrameLength) * MaxDecel);

            if (Speed < 0.0f)
            {
                Speed = 0.0f;
            }
        }

        if (Speed < 0.0f)
        {
            Speed = static_cast<float>(static_cast<double>(FrameLength) * MaxDecel + Speed);
            stop = 0.0f < Speed;
        }
    }

    if (stop)
    {
        Speed = 0.0f;
    }

    // The step this frame, along the lead car's (turned) facing.
    MCVector3D move(0.0f, 0.0f, 0.0f);

    if (Speed != 0.0f)
    {
        const float reach = -(WorldUnitsPerMeter * Speed);
        const MCFrameOfRef turned = MCTrainCar::TurnedFrame(cars.front()->GetFrame());
        move.X = static_cast<float>(static_cast<double>(turned.J.X) * reach * FrameLength);
        move.Y = turned.J.Y * reach * FrameLength;
        move.Z = turned.J.Z * reach * FrameLength;
    }

    // Each car on the rails: unlock its cells, move, lock the new ones.
    for (const std::shared_ptr<MCTrainCar>& car : cars)
    {
        if (car->Derailed)
        {
            continue;
        }

        LockTrackCells(*car, TrackDirection, 0);
        car->Speed = Speed;
        const MCVector3D carPos = car->GetPosition();
        MCVector3D newPos;
        newPos.X = carPos.X + move.X;
        newPos.Y = carPos.Y + move.Y;
        newPos.Z = carPos.Z + move.Z;
        car->SetPosition(newPos);
        LockTrackCells(*car, TrackDirection, 1);
    }
}

auto MCTrain::AddCar(MCTrainCar& car) -> int32_t
{
    if (car.ObjectClass != MCObjectClass::TrainCar)
    {
        return static_cast<int32_t>(0xdefc0005);
    }

    if (const std::vector<std::shared_ptr<MCTrainCar>> cars = PinCars(); !cars.empty())
    {
        // Hitch it carOffset behind the last car, along the (turned) lead car's main axis.
        const MCFrameOfRef turned = MCTrainCar::TurnedFrame(cars.front()->GetFrame());
        MCVector3D carPos = cars.back()->GetPosition();

        if (std::abs(turned.J.X) <= std::abs(turned.J.Y))
        {
            if (turned.J.Y <= 0.0f)
            {
                carPos.Y = carPos.Y - CarOffset;
            }
            else
            {
                carPos.Y = CarOffset + carPos.Y;
            }
        }
        else if (turned.J.X <= 0.0f)
        {
            carPos.X = carPos.X - CarOffset;
        }
        else
        {
            carPos.X = CarOffset + carPos.X;
        }

        car.SetPosition(carPos);
    }

    Cars.push_back(car.SharedAs<MCTrainCar>());
    car.Train = shared_from_this();
    NumCars++;
    RecalcInfo();
    return NumCars;
}

auto MCTrain::RemoveCar(MCTrainCar& car, bool justUnlink) -> int32_t
{
    const auto position =
        std::ranges::find_if(Cars, [&car](const std::weak_ptr<MCTrainCar>& link) { return link.lock().get() == &car; });

    if (position == Cars.end())
    {
        return -1;
    }

    if (!justUnlink)
    {
        // Split the train: the car becomes a train of its own, and the cars behind it another.
        // Port fix: the original reads this train's speeds (and car count) after the last car has left and it has
        // been freed; the values it would have read are kept from before each move.
        float lastSpeed = Speed;
        float lastDesiredSpeed = DesiredSpeed;
        int32_t carsLeft = NumCars;
        const auto moveCar = [&](MCTrain& to, MCTrainCar& moving)
        {
            to.AddCar(moving);
            lastSpeed = Speed;
            lastDesiredSpeed = DesiredSpeed;
            carsLeft = RemoveCar(moving, true);
        };

        std::vector<std::shared_ptr<MCTrainCar>> behind;

        for (auto link = std::next(position); link != Cars.end(); ++link)
        {
            if (std::shared_ptr<MCTrainCar> moving = LockObject(*link))
            {
                behind.push_back(std::move(moving));
            }
        }

        const std::shared_ptr<MCTrain> alone = TrainManager().CreateTrain();
        moveCar(*alone, car);

        if (!behind.empty())
        {
            const std::shared_ptr<MCTrain> rest = TrainManager().CreateTrain();

            for (const std::shared_ptr<MCTrainCar>& moving : behind)
            {
                moveCar(*rest, *moving);
            }

            if (carsLeft != 0)
            {
                lastSpeed = Speed;
                lastDesiredSpeed = DesiredSpeed;
            }

            rest->Speed = lastSpeed;
            rest->DesiredSpeed = lastDesiredSpeed;
            rest->RecalcInfo();
        }

        return carsLeft != 0 ? NumCars : 0;
    }

    // Unlink it; a train left with no cars leaves the manager's list (the original freed it).
    Cars.erase(position);
    NumCars--;

    if (NumCars == 0)
    {
        TrainManager().RemoveTrain(*this);
        ForgetCars();
        return 0;
    }

    RecalcInfo();
    return NumCars;
}

auto MCTrain::RecalcInfo() -> void
{
    // The train goes at the pace of its weakest car.
    MaxDecel = -9999999.0f;
    MaxAccel = -9999999.0f;
    MaxSpeed = 9999999.0f;

    if (const std::vector<std::shared_ptr<MCTrainCar>> cars = PinCars(); !cars.empty())
    {
        LeadPosition = cars.front()->GetPosition();

        for (const std::shared_ptr<MCTrainCar>& car : cars)
        {
            MaxAccel = std::max(MaxAccel, car->GetMaxAccel());
            MaxDecel = std::max(MaxDecel, car->GetMaxDecel());
            MaxSpeed = std::min(car->GetMaxSpeed(), MaxSpeed);
        }
    }

    if (MaxSpeed < std::abs(DesiredSpeed))
    {
        DesiredSpeed = 0.0f < DesiredSpeed ? MaxSpeed : -MaxSpeed;
    }
}

auto MCTrain::GetTotalTonnage() -> float
{
    float tonnage = 0.0f;

    for (const std::shared_ptr<MCTrainCar>& car : PinCars())
    {
        tonnage = car->GetTonnage() + tonnage;
    }

    return tonnage;
}
