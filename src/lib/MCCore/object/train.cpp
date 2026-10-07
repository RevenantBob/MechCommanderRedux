#include "stdafx.h"
#include "object/train.h"
#include "ai/move.h"
#include "appear/MCAppearanceType.h"
#include "appear/MCAppearanceTypeList.h"
#include "camera/MCCamera.h"
#include "camera/MCCameraList.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCEllipseElement.h"
#include "engine/MCVfxElement.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/collsn.h"
#include "object/explode.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/team.h"
#include "sound/soundsys.h"
#include "sprite/MCGVAppearance.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Degrees to radians, as MCX.EXE stores it (a hair under pi / 180).</summary>
    constexpr double DEGREES_TO_RADIANS = 0x1.1df46a2526c7ap-6;
    /// <summary>An eighth of a turn, as MCX.EXE stores it (a hair under pi / 4).</summary>
    constexpr double EIGHTH_TURN = 0x1.921fb5443e88cp-1;
    /// <summary>Radians to degrees at float precision, as TrainCar::relFacingTo uses it.</summary>
    constexpr double RADIANS_TO_DEGREES_F = 0x1.ca5dc2p+5;
    /// <summary>The track directions that run down the map's rows (the others run along its columns).</summary>
    constexpr int32_t TRACK_DIAGONAL_A = -45;
    constexpr int32_t TRACK_DIAGONAL_B = 135;

    /// <summary>The frame turned an eighth of a turn about its up axis (the facing the art is drawn at).</summary>
    MCFrameOfRef TurnedFrame(const MCFrameOfRef& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        MCFrameOfRef turned = frame;
        turned.I = frame.I * c + frame.J * s;
        turned.J = frame.J * c - frame.I * s;
        return turned;
    }

    /// <summary>
    /// Sets or clears the path locks of the three map cells a train car covers, from the cell under it: down a
    /// column (one right of it) for the -45/135 tracks, along a row (one below it) for the others. The cell index is
    /// not wrapped at the tile's edge (a car in the last column locks index row * 3 + 3).
    /// </summary>
    void LockTrackCells(MCTrainCar* car, int32_t trackDirection, uint32_t locked)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(car->GetPosition(), tileR, tileC, cellR, cellC);
        const auto lock = [&]
        {
            MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];
            const auto shift = static_cast<uint32_t>(cellC + cellR * 3);
            tile.Overlay = (locked << ((shift + 0xf) & 0x1f)) | (~(0x8000u << (shift & 0x1f)) & tile.Overlay);
        };

        if (trackDirection == TRACK_DIAGONAL_A || trackDirection == TRACK_DIAGONAL_B)
        {
            cellC++;

            for (int32_t i = 0; i < 3; i++)
            {
                if (i != 0)
                {
                    if (cellR == 2)
                    {
                        cellR = 0;
                        tileR++;
                    }
                    else
                    {
                        cellR++;
                    }
                }

                lock();
            }
        }
        else
        {
            cellR++;

            for (int32_t i = 0; i < 3; i++)
            {
                if (i != 0)
                {
                    if (cellC == 2)
                    {
                        cellC = 0;
                        tileC++;
                    }
                    else
                    {
                        cellC++;
                    }
                }

                lock();
            }
        }
    }

    /// <summary>The object list named <paramref name="listName"/>, or null.</summary>
    MCObjectQueueNode* FindObjectList(const char* listName)
    {
        for (MCObjectQueueNode* list = ObjectList->Head; list != nullptr; list = list->Next)
        {
            if (list->operator==(listName) != 0)
            {
                return list;
            }
        }

        return nullptr;
    }
} // namespace

float CarOffset = 84.0f;

//---------------------------------------------------------------------------
// Train
//---------------------------------------------------------------------------

MCTrain::MCTrain()
{
    Init();
}

MCTrain::~MCTrain()
{
    // Faithful: the list entries are walked but not freed.
    while (Cars != nullptr)
    {
        Cars = Cars->Next;
    }
}

auto MCTrain::Init() -> void
{
    Speed = 0.0f;
    LeadPosition.Y = 0.0f;
    LeadPosition.X = 0.0f;
    MaxAccel = 0.0f;
    MaxDecel = 0.0f;
    MaxSpeed = 0.0f;
    DesiredSpeed = 0.0f;
    LeadPosition.Z = 0.0f;
    NumCars = 0;
    Cars = nullptr;
}

auto MCTrain::Destroy() -> void
{
    // Faithful: the list entries are walked but not freed.
    while (Cars != nullptr)
    {
        Cars = Cars->Next;
    }
}

auto MCTrain::Update() -> void
{
    MCTrainListEntry* entry = Cars;

    if (entry == nullptr)
    {
        return;
    }

    // A derailed car brakes the whole train; otherwise it speeds toward desiredSpeed (maxAccel, and maxDecel to
    // slow or to come back through zero) within maxSpeed.
    int32_t anyDerailed = 0;

    for (MCTrainListEntry* check = entry; check != nullptr; check = check->Next)
    {
        if (check->Car->Derailed != 0)
        {
            anyDerailed = check->Car->Derailed;
            break;
        }
    }

    bool stop = false;

    if (anyDerailed == 0)
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
    MCVector3D move;
    move.X = 0.0f;
    move.Y = 0.0f;
    move.Z = 0.0f;

    if (Speed != 0.0f)
    {
        const float reach = -(WorldUnitsPerMeter * Speed);
        const MCFrameOfRef turned = TurnedFrame(entry->Car->GetFrame());
        move.X = static_cast<float>(static_cast<double>(turned.J.X) * reach * FrameLength);
        move.Y = turned.J.Y * reach * FrameLength;
        move.Z = turned.J.Z * reach * FrameLength;
    }

    // Each car on the rails: unlock its cells, move, lock the new ones.
    for (; entry != nullptr; entry = entry->Next)
    {
        MCTrainCar* car = entry->Car;

        if (car->Derailed == 1)
        {
            continue;
        }

        LockTrackCells(car, TrackDirection, 0);
        car->Speed = Speed;
        const MCVector3D carPos = car->GetPosition();
        MCVector3D newPos;
        newPos.X = carPos.X + move.X;
        newPos.Y = carPos.Y + move.Y;
        newPos.Z = carPos.Z + move.Z;
        car->SetPosition(newPos);
        LockTrackCells(car, TrackDirection, 1);
    }
}

auto MCTrain::AddCar(MCTrainCar* car) -> int32_t
{
    if (car->ObjectClass != TRAINCAR)
    {
        return static_cast<int32_t>(0xdefc0005);
    }

    auto* newEntry = new MCTrainListEntry;
    Assert(newEntry != nullptr ? 1u : 0u, 0, "Not enough memory to allocate new TrainListEntry");
    newEntry->Car = car;
    MCTrainListEntry* tail = Cars;

    while (tail != nullptr && tail->Next != nullptr)
    {
        tail = tail->Next;
    }

    if (tail == nullptr)
    {
        Cars = newEntry;
    }
    else
    {
        // Hitch it carOffset behind the last car, along the (turned) lead car's main axis.
        const MCFrameOfRef turned = TurnedFrame(Cars->Car->GetFrame());
        MCVector3D carPos = tail->Car->GetPosition();

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

        car->SetPosition(carPos);
        tail->Next = newEntry;
        newEntry->Prev = tail;
    }

    car->Train = this;
    NumCars++;
    RecalcInfo();
    return NumCars;
}

auto MCTrain::RemoveCar(MCTrainCar* car, int justUnlink) -> int32_t
{
    MCTrainListEntry* entry = Cars;

    while (entry != nullptr && entry->Car != car)
    {
        entry = entry->Next;
    }

    if (entry == nullptr)
    {
        return -1;
    }

    if (justUnlink == 0)
    {
        // Split the train: the car becomes a train of its own, and the cars behind it another.
        // Port fix: the original reads this train's speeds (and car count) after the last car has left and it has
        // been freed; the values it would have read are kept from before each move.
        float lastSpeed = Speed;
        float lastDesiredSpeed = DesiredSpeed;
        int32_t carsLeft = NumCars;
        const auto moveCar = [&](MCTrain* to, MCTrainCar* moving)
        {
            to->AddCar(moving);
            lastSpeed = Speed;
            lastDesiredSpeed = DesiredSpeed;
            carsLeft = RemoveCar(moving, 1);
        };

        MCTrain* alone = TrainManager->CreateTrain();
        MCTrainListEntry* behind = entry->Next;
        moveCar(alone, entry->Car);

        if (behind != nullptr)
        {
            MCTrain* rest = TrainManager->CreateTrain();

            do
            {
                MCTrainListEntry* nextEntry = behind->Next;
                moveCar(rest, behind->Car);
                behind = nextEntry;
            } while (behind != nullptr);

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

    // Unlink it; a train left with no cars is removed and freed.
    MCTrainListEntry* before = entry->Prev;
    MCTrainListEntry* after = entry->Next;

    if (before == nullptr)
    {
        Cars = after;
    }
    else
    {
        before->Next = after;
    }

    if (after != nullptr)
    {
        after->Prev = before;
    }

    delete entry;
    NumCars--;

    if (NumCars == 0)
    {
        TrainManager->RemoveTrain(this);
        Destroy();
        delete this;
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

    if (Cars != nullptr)
    {
        LeadPosition = Cars->Car->GetPosition();

        for (MCTrainListEntry* entry = Cars; entry != nullptr; entry = entry->Next)
        {
            MCTrainCar* car = entry->Car;

            if (MaxAccel < car->GetMaxAccel())
            {
                MaxAccel = car->GetMaxAccel();
            }

            if (MaxDecel < car->GetMaxDecel())
            {
                MaxDecel = car->GetMaxDecel();
            }

            if (car->GetMaxSpeed() < MaxSpeed)
            {
                MaxSpeed = car->GetMaxSpeed();
            }
        }
    }

    if (MaxSpeed < std::abs(DesiredSpeed))
    {
        if (0.0f < DesiredSpeed)
        {
            DesiredSpeed = MaxSpeed;
        }
        else
        {
            DesiredSpeed = -MaxSpeed;
        }
    }
}

auto MCTrain::GetTotalTonnage() -> float
{
    float tonnage = 0.0f;

    for (MCTrainListEntry* entry = Cars; entry != nullptr; entry = entry->Next)
    {
        tonnage = entry->Car->GetTonnage() + tonnage;
    }

    return tonnage;
}

//---------------------------------------------------------------------------
// TrainCarType
//---------------------------------------------------------------------------

MCTrainCarType::MCTrainCarType()
{
    Damage = 0;
    ExplosionChance = 0;
    ExplosionDamage = 0;
    VelocityMultiplier = 0;
    TopSpeed = 0.0f;
    Acceleration = 0.0f;
    Deceleration = 0.0f;
    TonnageClass = -1.0f;
    NameId = 0;
}

auto MCTrainCarType::CreateInstance() -> MCBaseObject*
{
    auto* newCar = new MCTrainCar;

    if (newCar == nullptr)
    {
        return nullptr;
    }

    if (newCar->Init(this) != 0)
    {
        return nullptr;
    }

    newCar->IdNumber = NextIdNumber++;
    return newCar;
}

auto MCTrainCarType::Destroy() -> void
{
}

auto MCTrainCarType::Init(MCFile* objFile, uint32_t fileSize) -> int32_t
{
    MCFitIniFile trainFile;
    int32_t result = trainFile.Open(objFile, fileSize);

    if (result != 0)
    {
        return result;
    }

    if ((result = trainFile.SeekBlock("Train")) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdLong("Name", NameId)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdUChar("Explosion Chance", ExplosionChance)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdUChar("Explosion Damage", ExplosionDamage)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdUChar("Velocity Multiplier", VelocityMultiplier)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdFloat("Acceleration", Acceleration)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdFloat("Deceleration", Deceleration)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdFloat("TopSpeed", TopSpeed)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdLong("Damage", Damage)) != 0)
    {
        return result;
    }

    if ((result = trainFile.ReadIdFloat("TonnageClass", TonnageClass)) != 0)
    {
        return result;
    }

    if ((result = MCObjectType::Init(&trainFile)) != 0)
    {
        return result;
    }

    return 0;
}

auto MCTrainCarType::HandleCollision(MCGameObject* collidee, MCGameObject* collider) -> int
{
    // The server's job in multiplayer.
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return 0;
    }

    auto* car = static_cast<MCTrainCar*>(collidee);
    MCTrain* train = car->Train;
    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    // The car takes (collider tonnage + 1) / 2, from the collider's side.
    const auto hitCar = [&](int32_t hitLocation)
    {
        const auto angle = static_cast<float>(car->RelFacingTo(collider->GetPosition(), -1));
        MCWeaponShotInfo shot;
        shot.Init(collider, -1, static_cast<float>((collider->GetTonnage() + 1.0) * 0.5), hitLocation, angle);
        car->HandleWeaponHit(&shot, multiplayer);
    };

    if (car->Derailed == 1)
    {
        // A derailed car is only hurt by mechs, vehicles and elementals.
        if (collider->ObjectClass < BATTLEMECH || ELEMENTAL < collider->ObjectClass)
        {
            return 0;
        }

        hitCar(-1);
        return 0;
    }

    // Something heavy stops the train; movers and buildings in the way take the train's weight.
    if (20.0f <= collider->GetTonnage())
    {
        train->Speed = 0.0f;
    }

    switch (collider->ObjectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        case ELEMENTAL:
        {
            const int32_t hitLocation = collider->CalcHitLocation(car, -1, 1, 0);
            const auto angle = static_cast<float>(collider->RelFacingTo(car->GetPosition(), -1));
            MCWeaponShotInfo shot;
            shot.Init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, hitLocation, angle);
            collider->HandleWeaponHit(&shot, multiplayer);
            hitCar(hitLocation);
            return 0;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            train->Speed = 0.0f;
            MCWeaponShotInfo shot;
            shot.Init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, -1, -1.0f);
            collider->HandleWeaponHit(&shot, multiplayer);
            hitCar(-1);
            return 0;
        }

        default:
            return 0;
    }
}

auto MCTrainCarType::HandleDestruction(MCGameObject* collidee, MCGameObject*) -> int
{
    const auto blast = static_cast<float>(ExplosionDamage);
    MCVector3D where = collidee->GetPosition();
    CreateExplosion(where, blast, blast);
    return 0;
}

//---------------------------------------------------------------------------
// TrainCar
//---------------------------------------------------------------------------

auto MCTrainCar::Init() -> void
{
    Appearance = nullptr;
    Train = nullptr;
    Name.clear();
    Speed = 0.0f;
    Wrecked = 0;
    OnMap = 1;
    DamageTaken = 0.0f;
    JustCreated = 1;
}

auto MCTrainCar::GetFrame() -> MCFrameOfRef
{
    return Frame;
}

auto MCTrainCar::SetFrame(MCFrameOfRef& newFrame) -> void
{
    Frame = newFrame;
}

auto MCTrainCar::HandleStaticCollision() -> void
{
    if (CollisionsOn == 0 || OnMap == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    MCObjectQueueNode* list = FindObjectList(listName);

    // Port fix: the original reads the list's objects without checking that the block has a list.
    if (list == nullptr)
    {
        return;
    }

    // The terrain objects of its own block, on its own vertex.
    MCBaseObject* object = list->Head;

    while (object != nullptr)
    {
        auto* other = static_cast<MCGameObject*>(object);

        if (other->GetObjectType() != nullptr)
        {
            int32_t otherBlock = -1;
            int32_t otherVertex = -1;

            switch (other->ObjectClass)
            {
                case BUILDING:
                case TREE:
                case TERRAINOBJECT:
                case TREEBUILDING:
                    other->GetBlockAndVertexNumber(otherBlock, otherVertex);
                    break;
                case MISCTERRAINOBJECT:
                    // Original behaviour (OB-022): walls, bridges and forests read the train car's own vertex, so they
                    // always match.
                    GetBlockAndVertexNumber(otherBlock, otherVertex);
                    break;
                default:
                    break;
            }

            if (vertexNumber == otherVertex)
            {
                CollisionSystem->DetectStaticCollision(this, other);
            }
        }

        // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
        // without one hangs the game here.
        object = object->Next;
    }
}

auto MCTrainCar::Init(MCObjectType* objType) -> int32_t
{
    int32_t result = MCGameObject::Init(objType);

    if (result != 0)
    {
        return result;
    }

    ObjectClass = TRAINCAR;

    if (objType != nullptr)
    {
        auto* carType = static_cast<MCTrainCarType*>(objType);
        char nameBuffer[256];
        CLoadString(ThisInstance, static_cast<uint32_t>(carType->NameId), nameBuffer, 0xfe);
        Name = nameBuffer;
        Damage = static_cast<float>(carType->Damage);
        SetTonnage(carType->TonnageClass);
        CollisionsOn = 1;
    }

    Derailed = 0;
    MCAppearanceType* apprType = AppearanceTypeList()->GetAppearance(objType->AppearName);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdefc0003);
    }

    auto* vehicleAppearance = new MCGVAppearance;
    Appearance = vehicleAppearance;

    if (vehicleAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdefc0001);
    }

    vehicleAppearance->Init(nullptr, nullptr);

    if ((apprType->AppearanceNum & 0xff000000) != 0x5000000)
    {
        return -0x2fff6;
    }

    if ((result = vehicleAppearance->Init(apprType, this)) != 0)
    {
        return result;
    }

    SetPotentialContact(1);
    JustCreated = 1;
    return 0;
}

auto MCTrainCar::Destroy() -> void
{
    Name.clear();
}

auto MCTrainCar::SetPartId(int32_t trainNumber, int32_t carNumber) -> void
{
    PartId = carNumber + (trainNumber * 5 + 0x6400) * 0x14;
}

auto MCTrainCar::IsRevealed() -> int
{
    MCByteFlag* visibleBits = Terrain()->HomeVisibleBits();
    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    GetBlockAndVertexNumber(blockNumber, vertexNumber);
    const auto col = static_cast<uint32_t>((blockNumber % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           vertexNumber % MCTerrain::VerticesBlockSide);
    const auto row = static_cast<uint32_t>((blockNumber / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                                           vertexNumber / MCTerrain::VerticesBlockSide);

    if (visibleBits->GetFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col) != 0)
    {
        return 1;
    }

    if (visibleBits->GetFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->GetFlag(row, col + 1) != 0 ? 1 : 0;
}

auto MCTrainCar::OnScreen() -> int
{
    MCCamera* camera = CameraList()->FindCameraFromIDNumber(1);

    if (OnMap == 0 || camera == nullptr || camera->Active == 0)
    {
        return 0;
    }

    MCVector2D screen100;
    MCVector2D screen50;

    if (Terrain() != nullptr)
    {
        Terrain()->ProjectTerrain(Position, screen100, screen50);
    }

    float screenY;

    if (camera->CameraScale == 1)
    {
        ScreenPos.X = (screen50.X - camera->ScreenUL50.X) + camera->HalfWidth;
        screenY = screen50.Y - camera->ScreenUL50.Y;
    }
    else
    {
        ScreenPos.X = (screen100.X - camera->ScreenUL.X) + camera->HalfWidth;
        screenY = screen100.Y - camera->ScreenUL.Y;
    }

    ScreenPos.Y = screenY + camera->HalfHeight;

    if (Appearance->RecalcBounds(camera) == 0)
    {
        return 0;
    }

    WindowsVisible = Turn;
    return 1;
}

auto MCTrainCar::Update() -> int32_t
{
    if (JustCreated != 0)
    {
        JustCreated = 0;
    }

    int visibleNow = 0;

    if (Wrecked == 0)
    {
        // Damaged past half (or, from 10 points up, by chance) the car blows up and jumps the rails.
        const auto* carType = static_cast<MCTrainCarType*>(ObjType);
        const int32_t maxDamage = carType->Damage;
        bool blowUp = static_cast<float>(maxDamage / 2) <= DamageTaken;

        if (!blowUp && 10.0f <= DamageTaken)
        {
            // Original behaviour (OB-024): the roll derails the car when its damage percentage is BELOW the roll, so
            // lightly damaged cars go more often.
            const int32_t roll = RandomNumber(100);
            blowUp = DamageTaken * 100.0f / static_cast<float>(maxDamage) < static_cast<float>(roll);
        }

        if (blowUp && Derailed != 1)
        {
            Damage = 0.0f;
            Status = 2;
            ObjType->HandleDestruction(this, nullptr);
            Derail(LastHitAngle);
        }

        // Off the map it stops drawing; on a broken bridge it falls in.
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->WorldToMapPos(GetPosition(), tileR, tileC, cellR, cellC);
        OnMap = tileR < 0 || GameMap->Height <= tileR || tileC < 0 || GameMap->Width <= tileC ? 0 : 1;

        if (OnMap != 0)
        {
            const uint32_t overlayType = GameMap->Map[GameMap->Width * tileR + tileC].Overlay & 0x7f;

            if (overlayType == 0x38 || overlayType == 0x3a)
            {
                Wrecked = 1;
                Derailed = 1;
                CollisionsOn = 0;
                MCVector3D where = GetPosition();
                CreateExplosion(MineExplosion, where, 0.0f, 0.0f);
                Status = 2;
                Train->RemoveCar(this, 0);
                Speed = 0.0f;
                Train->RecalcInfo();
                ObjType->HandleDestruction(this, nullptr);
            }
        }

        if (OnMap != 0 && OnScreen() != 0)
        {
            visibleNow = 1;
        }
    }

    MineCheck();

    if (Appearance != nullptr)
    {
        Appearance->Visible = visibleNow;

        if (IsDestroyed() != 0)
        {
            static_cast<MCGVAppearance*>(Appearance)->SetTypeId(MCGVActorState::Destroyed);
        }
    }

    Appearance->Update();
    return 1;
}

auto MCTrainCar::Render() -> void
{
    if (Wrecked != 0)
    {
        return;
    }

    if (JustCreated == 0)
    {
        if (Appearance != nullptr)
        {
            Appearance->Visible = OnScreen() != 0 && OnMap != 0 ? 1 : 0;
            Appearance->Update();
        }

        const int32_t contactType = GetContactType(HomeTeam->Id);

        if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage.
            uint8_t* shape;

            if (50.0f < GetTonnage())
            {
                shape = Scenario->SensorContactShapes[0];
            }
            else if (35.0f < GetTonnage())
            {
                shape = Scenario->SensorContactShapes[2];
            }
            else
            {
                shape = Scenario->SensorContactShapes[4];
            }

            if (shape != nullptr)
            {
                if (VfxShapeCount(shape) < BlipFrame)
                {
                    if (SoundSystem != nullptr)
                    {
                        SoundSystem->PlayDigitalSample(0x14, 1, this, 0, 1);
                    }

                    BlipFrame = 0;
                }

                ElementList()->OpenGroup(-100000, 1);
                ElementList()->Add(
                    ElementList()->Make<MCVfxElement>(shape, ScreenPos.X, ScreenPos.Y, BlipFrame, 0, nullptr, 0));
                BlipFrame++;
            }
        }
        else if (contactType == 1)
        {
            // Seen: drawn. The original also started and stopped a looping sound here, from a type field nothing
            // ever set (always -1), so a train car never makes one (OB-023).
            if (WindowsVisible == Turn)
            {
                auto* carAppearance = static_cast<MCGVAppearance*>(Appearance);
                carAppearance->HazePalette = nullptr;
                carAppearance->Render(0);
            }
        }
    }

    if (DrawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = ObjType->ExtentRadius;

        if (Eye->CameraScale == 1)
        {
            radius *= 0.5f;
        }

        const float scale = Eye->CameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (Position.X - Eye->Position.X) * scale;
        const float sy = (Position.Y - Eye->Position.Y) * scale;
        MCVector2D center;
        center.X = sx * Eye->CosAngle + sy * Eye->CosAngle + Eye->HalfWidth;
        center.Y =
            ((sx * Eye->SinAngle + Eye->HalfHeight) - sy * Eye->SinAngle) - scale * (Position.Z - Eye->Position.Z);
        MCVector2D size(radius, radius);
        ElementList()->OpenGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.X *= MCOverlay.ScaleX;
        size.Y *= MCOverlay.ScaleY;
        ElementList()->Add(ElementList()->Make<MCEllipseElement>(center, size, 0xfe, -50000));
    }
}

auto MCTrainCar::HandleWeaponHit(MCWeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->AddWeaponHitChunk(this, shotInfo, 0);
    }

    if (0.0f < shotInfo->Damage && IsDestroyed() == 0)
    {
        LastHitAngle = shotInfo->EntryAngle;
        DamageTaken = shotInfo->Damage + DamageTaken;
        const float remaining = Damage - shotInfo->Damage;
        Damage = remaining;

        if (remaining <= 0.0f)
        {
            // Destroyed: it jumps the rails and leaves its train.
            Status = 2;
            Derail(static_cast<float>(RandomNumber(10) - 20));
            Train->RemoveCar(this, 0);
            Speed = 0.0f;
            Train->RecalcInfo();
            ObjType->HandleDestruction(this, nullptr);
            CollisionsOn = 0;
        }
    }

    return 0;
}

auto MCTrainCar::Derail(float angle) -> void
{
    if (Derailed == 1)
    {
        return;
    }

    // Off the rails: free its cells, then (by the train's speed and a roll) take the car ahead or behind with it.
    LockTrackCells(this, Train->TrackDirection, 0);
    Assert(Train != nullptr ? 1u : 0u, 0, "Car must have a train to derail");
    MCTrainListEntry* entry = Train->Cars;

    while (entry != nullptr && entry->Car != this)
    {
        entry = entry->Next;
    }

    Assert(entry != nullptr ? 1u : 0u, 0, "Can't find carEntry for this car");
    Derailed = 1;
    const auto roll = static_cast<float>(RandomNumber(100));
    MCTrain* oldTrain = Train;
    const bool slowEnough = static_cast<double>(std::abs(oldTrain->Speed)) * 5.0 <= roll;

    if ((Speed <= 0.0f && slowEnough) || entry->Next == nullptr)
    {
        if ((Speed < 0.0f || !slowEnough) && entry->Prev != nullptr)
        {
            entry->Prev->Car->Derail(-angle);
        }
    }
    else
    {
        entry->Next->Car->Derail(-angle);
    }

    // Port fix: the neighbour's derail can split and free the old train; the original then calls RemoveCar on the
    // freed train. Only a train the manager still holds is used.
    bool oldTrainAlive = false;

    for (int32_t i = 0; i < TrainManager->NumTrains && i < MCTrainManager::MAX_TRAINS; i++)
    {
        if (TrainManager->Trains[i] == oldTrain)
        {
            oldTrainAlive = true;
        }
    }

    if (oldTrainAlive)
    {
        oldTrain->RemoveCar(this, 0);
    }

    // Slew it round: a random swing (Faithful: -50..0 whichever side it was hit, 1..100 when head on).
    MCFrameOfRef turned = GetFrame();
    double swing;

    if (angle <= 0.0f && 0.0f <= angle)
    {
        swing = 50.0 - RandomNumber(100);
    }
    else
    {
        swing = -static_cast<double>(RandomNumber(50));
    }

    const auto s = static_cast<float>(std::sin(swing * DEGREES_TO_RADIANS));
    const auto c = static_cast<float>(std::cos(swing * DEGREES_TO_RADIANS));
    const MCVector3D oldI = turned.I;
    turned.I = turned.I * c + turned.J * s;
    turned.J = turned.J * c - oldI * s;
    SetFrame(turned);

    // Into water: the car (and its one-car train) is gone.
    MCObjectPosition* objectPosition = GetObjPosition();

    // Port fix: the original reads the object position without checking it for null.
    if (objectPosition != nullptr)
    {
        const int32_t tileR = objectPosition->TileR;
        const int32_t tileC = objectPosition->TileC;
        const MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];

        if ((tile.Cells & 0x7f) == 0x2b)
        {
            MCTrain* lostTrain = Train;
            TrainManager->RemoveTrain(lostTrain);
            lostTrain->Destroy();
            Wrecked = 1;
            CollisionsOn = 0;
            return;
        }

        // Onto a wall or bridge: it takes the train's weight.
        if ((tile.Overlay & 0x7f) == 0x3e)
        {
            const int32_t vbs = MCTerrain::VerticesBlockSide;
            const int32_t partId = ((MCTerrain::BlocksMapSide * (tileR / vbs) + tileC / vbs) * 400 + tileC +
                                    ((tileR - vbs * (tileR / vbs)) - tileC / vbs) * vbs) *
                                       8 +
                                   0x1000;
            auto* hit = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(partId));

            // Port fix: the original reads the object's class without checking that one was found.
            if (hit != nullptr && hit->ObjectClass == MISCTERRAINOBJECT)
            {
                MCWeaponShotInfo shot;
                shot.Init(nullptr, 0, Train->GetTotalTonnage() * 0.1f + 0.5f, 0, 0.0f);

                if (MPlayer == nullptr)
                {
                    hit->HandleWeaponHit(&shot, 0);
                }
                else if (MPlayer->IsServer != 0)
                {
                    hit->HandleWeaponHit(&shot, 1);
                }
            }
        }
    }

    MCVector3D where = GetPosition();
    CreateExplosion(MineExplosion, where, 0.0f, 0.0f);
}

auto MCTrainCar::MineCheck() -> void
{
    if (MPlayer != nullptr && MPlayer->IsServer == 0)
    {
        return;
    }

    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->WorldToMapPos(GetPosition(), tileR, tileC, cellR, cellC);
    // Each side's mines only go off under the other side.
    const uint32_t mine = Alignment == -1 || Alignment == 0 ? GameMap->GetInnerSphereMine(tileR, tileC, cellR, cellC)
                                                            : GameMap->GetClanMine(tileR, tileC, cellR, cellC);

    if (mine == 0)
    {
        return;
    }

    MCVector3D where = GetPosition();
    CreateExplosion(MineExplosion, where, MineSplashDamage, WorldUnitsPerMeter * MineSplashRange);
    const int32_t hitLocation = CalcHitLocation(nullptr, -1, 3, 0);
    MCWeaponShotInfo shot;
    shot.Init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
    HandleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
    MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];

    if (GetAlignment() == -1 || GetAlignment() == 0)
    {
        tile.Overlay |= 0x1800;
    }
    else
    {
        tile.Overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->AddMineChunk(cellR + tileR * 3, cellC + tileC * 3, Alignment == -1 || Alignment != 0 ? 0 : 1, 3, 2);
    }
}

auto MCTrainCar::RelFacingTo(MCVector3D goal, int32_t) -> float
{
    const float x = Position.X;
    const float y = Position.Y;
    const MCFrameOfRef turned = TurnedFrame(Frame);
    MCVector3D facing;
    facing.X = -turned.J.X;
    facing.Y = -turned.J.Y;
    facing.Z = -turned.J.Z;

    MCVector3D toGoal;
    toGoal.X = goal.X - x;
    toGoal.Y = goal.Y - y;
    toGoal.Z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.X) * toGoal.X + static_cast<double>(toGoal.Y) * toGoal.Y) +
                  static_cast<double>(toGoal.Z) * toGoal.Z);

    if (length != 0.0)
    {
        toGoal.X = static_cast<float>(toGoal.X / length);
        toGoal.Y = static_cast<float>(toGoal.Y / length);
        toGoal.Z = static_cast<float>(toGoal.Z / length);
    }

    const double cosine = static_cast<double>(toGoal.Z) * facing.Z + static_cast<double>(toGoal.Y) * facing.Y +
                          static_cast<double>(toGoal.X) * facing.X;
    const float angle = static_cast<float>(AcosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).Z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto MCTrainCar::RelativePosition(float angle, float distance, uint32_t flags) -> MCVector3D
{
    // Off the map: nowhere.
    if (OnMap == 0)
    {
        MCVector3D nowhere;
        nowhere.X = -999999.0f;
        nowhere.Y = -999999.0f;
        nowhere.Z = -999999.0f;
        return nowhere;
    }

    // The point distance meters away at angle: flag 1, an absolute angle in radians; else degrees from the car's
    // facing. The x87 keeps some of the sums below at extended precision, done here in double.
    const float reach = -(WorldUnitsPerMeter * distance);
    const float x = Position.X;
    const float y = Position.Y;
    double offsetX;
    float offsetY;

    if ((flags & 1) != 0)
    {
        const double sine = std::sin(static_cast<double>(angle));
        const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
        offsetX = (sine + 0.0) * reach;
        offsetY = cosine * reach;
    }
    else
    {
        MCFrameOfRef turned = Frame;
        const double radians = (static_cast<double>(angle) + 45.0) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const MCVector3D oldI = turned.I;
        turned.I = turned.I * c + turned.J * s;
        turned.J = turned.J * c - oldI * s;
        const MCVector3D offset = turned.J * reach;
        offsetX = offset.X;
        offsetY = offset.Y;
    }

    const double targetX = offsetX + x;
    const float targetY = static_cast<float>(static_cast<double>(offsetY) + y);

    // Flag 2 walks from the car out to the point; otherwise from the point back to the car.
    MCVector2D start;
    MCVector2D end;

    if ((flags & 2) != 0)
    {
        end.X = static_cast<float>(targetX);
        start.X = x;
        start.Y = y;
        end.Y = targetY;
    }
    else
    {
        start.Y = targetY;
        start.X = static_cast<float>(targetX);
        end.X = x;
        end.Y = y;
    }

    // Half a map cell per step.
    const double deltaX = static_cast<double>(end.X) - start.X;
    const float deltaXf = static_cast<float>(deltaX);
    const float deltaY = end.Y - start.Y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaXf) * deltaXf));
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaXf) / length;
        directionY = static_cast<float>(static_cast<double>(deltaY) / length);
    }

    const float stepLength = static_cast<float>(static_cast<double>(MCTerrain::MetersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const double stepYExact = static_cast<double>(directionY) * stepLength;
    const float stepY = static_cast<float>(stepYExact);

    if (std::sqrt(stepYExact * stepY + static_cast<double>(stepX) * stepX) == 0.0)
    {
        MCVector3D result;
        result.X = x;
        result.Y = y;
        result.Z = 0.0f;
        return result;
    }

    const MCVector2D span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.X) * span.X + static_cast<double>(span.Y) * span.Y));
    float traveled = 0.0f;
    MCVector2D current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        MCVector3D point;
        point.X = current.X;
        point.Y = current.Y;
        point.Z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->OnMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->Map[GameMap->Width * tileR + tileC].GetCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    MCVector2D previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.X = stepX + current.X;
            current.Y = stepY + current.Y;
            const double dx = static_cast<double>(current.X) - start.X;
            const double dy = static_cast<double>(current.Y) - start.Y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    MCVector3D ground;
    ground.X = previous.X;
    ground.Y = previous.Y;
    ground.Z = 0.0f;
    MCVector3D result;
    result.X = previous.X;
    result.Y = previous.Y;
    result.Z = GameMap->GetTerrainElevation(ground);
    return result;
}

auto MCTrainCar::GetMaxAccel() -> float
{
    return static_cast<MCTrainCarType*>(ObjType)->Acceleration;
}

auto MCTrainCar::GetMaxDecel() -> float
{
    return static_cast<MCTrainCarType*>(ObjType)->Deceleration;
}

auto MCTrainCar::GetMaxSpeed() -> float
{
    return static_cast<MCTrainCarType*>(ObjType)->TopSpeed;
}

//---------------------------------------------------------------------------
// TrainListEntry
//---------------------------------------------------------------------------

MCTrainListEntry::MCTrainListEntry()
{
    Init();
}

auto MCTrainListEntry::Init() -> void
{
    Car = nullptr;
    Prev = nullptr;
    Next = nullptr;
}

//---------------------------------------------------------------------------
// TrainManager
//---------------------------------------------------------------------------

auto MCTrainManager::Init() -> void
{
    for (int32_t i = 0; i < MAX_TRAINS; i++)
    {
        Trains[i] = nullptr;
    }

    NumTrains = 0;
}

auto MCTrainManager::Destroy() -> void
{
    for (int32_t i = 0; i < MAX_TRAINS; i++)
    {
        if (Trains[i] != nullptr)
        {
            // The original freed the train without its destructor, which only repeats destroy.
            Trains[i]->Destroy();
            delete Trains[i];
            Trains[i] = nullptr;
        }
    }
}

auto MCTrainManager::CreateTrain() -> MCTrain*
{
    MCTrain* newTrain = nullptr;
    const int32_t index = NumTrains;

    if (index < MAX_TRAINS)
    {
        newTrain = new MCTrain;
        Trains[index] = newTrain;

        if (newTrain != nullptr)
        {
            NumTrains++;
        }
    }

    return newTrain;
}

auto MCTrainManager::RemoveTrain(MCTrain* train) -> void
{
    int32_t index = 0;

    while (index < NumTrains && Trains[index] != train)
    {
        index++;
    }

    // Port fix: the original shifts in (and then clears) the slot after the last, which with 64 trains is the
    // count itself.
    for (; index < NumTrains; index++)
    {
        Trains[index] = index + 1 < MAX_TRAINS ? Trains[index + 1] : nullptr;
    }

    if (NumTrains < MAX_TRAINS)
    {
        Trains[NumTrains] = nullptr;
    }

    // Faithful: the count drops even when the train wasn't in the list.
    NumTrains--;
}

auto MCTrainManager::UpdateTrains() -> void
{
    for (int32_t i = 0; i < NumTrains; i++)
    {
        Trains[i]->Update();
    }
}
