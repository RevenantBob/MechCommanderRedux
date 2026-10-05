#include "stdafx.h"
#include "object/train.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cellip.h"
#include "engine/cevfx.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
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
#include "sprite/gvactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

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
    frame_of_ref TurnedFrame(const frame_of_ref& frame)
    {
        const float s = static_cast<float>(std::sin(EIGHTH_TURN));
        const float c = static_cast<float>(std::cos(EIGHTH_TURN));
        frame_of_ref turned = frame;
        turned.i = frame.i * c + frame.j * s;
        turned.j = frame.j * c - frame.i * s;
        return turned;
    }

    /// <summary>
    /// Sets or clears the path locks of the three map cells a train car covers, from the cell under it: down a
    /// column (one right of it) for the -45/135 tracks, along a row (one below it) for the others. The cell index is
    /// not wrapped at the tile's edge (a car in the last column locks index row * 3 + 3).
    /// </summary>
    void lockTrackCells(TrainCar* car, int32_t trackDirection, uint32_t locked)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(car->getPosition(), tileR, tileC, cellR, cellC);
        const auto lock = [&]
        {
            MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];
            const auto shift = static_cast<uint32_t>(cellC + cellR * 3);
            tile.overlay = (locked << ((shift + 0xf) & 0x1f)) | (~(0x8000u << (shift & 0x1f)) & tile.overlay);
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
    ObjectQueueNode* findObjectList(const char* listName)
    {
        for (ObjectQueueNode* list = objectList->head; list != nullptr; list = list->next)
        {
            if (list->operator==(listName) != 0)
            {
                return list;
            }
        }

        return nullptr;
    }
} // namespace

float carOffset = 84.0f;

//---------------------------------------------------------------------------
// Train
//---------------------------------------------------------------------------

auto Train::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto Train::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

Train::Train()
{
    init();
}

Train::~Train()
{
    // Faithful: the list entries are walked but not freed.
    while (cars != nullptr)
    {
        cars = cars->next;
    }
}

auto Train::init() -> void
{
    speed = 0.0f;
    leadPosition.y = 0.0f;
    leadPosition.x = 0.0f;
    maxAccel = 0.0f;
    maxDecel = 0.0f;
    maxSpeed = 0.0f;
    desiredSpeed = 0.0f;
    leadPosition.z = 0.0f;
    numCars = 0;
    cars = nullptr;
}

auto Train::destroy() -> void
{
    // Faithful: the list entries are walked but not freed.
    while (cars != nullptr)
    {
        cars = cars->next;
    }
}

auto Train::Update() -> void
{
    TrainListEntry* entry = cars;

    if (entry == nullptr)
    {
        return;
    }

    // A derailed car brakes the whole train; otherwise it speeds toward desiredSpeed (maxAccel, and maxDecel to
    // slow or to come back through zero) within maxSpeed.
    int32_t anyDerailed = 0;

    for (TrainListEntry* check = entry; check != nullptr; check = check->next)
    {
        if (check->car->derailed != 0)
        {
            anyDerailed = check->car->derailed;
            break;
        }
    }

    bool stop = false;

    if (anyDerailed == 0)
    {
        if (maxAccel <= 0.0f)
        {
            if (0.0f < speed)
            {
                speed = static_cast<float>(static_cast<double>(frameLength) * maxAccel + speed);

                if (speed < 0.0f)
                {
                    speed = 0.0f;
                }
            }

            if (speed < 0.0f)
            {
                speed = static_cast<float>(speed - static_cast<double>(frameLength) * maxAccel);
                stop = 0.0f < speed;
            }
        }
        else if (desiredSpeed <= speed)
        {
            if (desiredSpeed < speed)
            {
                if (speed <= 0.0f)
                {
                    speed = static_cast<float>(speed - static_cast<double>(frameLength) * maxAccel);

                    if (speed < desiredSpeed)
                    {
                        speed = desiredSpeed;
                    }

                    if (maxSpeed < -speed)
                    {
                        speed = -maxSpeed;
                    }
                }
                else
                {
                    speed = static_cast<float>(speed - static_cast<double>(frameLength) * maxDecel);
                    stop = speed < 0.0f;
                }
            }
        }
        else if (0.0f <= speed)
        {
            speed = static_cast<float>(static_cast<double>(frameLength) * maxAccel + speed);

            if (desiredSpeed < speed)
            {
                speed = desiredSpeed;
            }

            if (maxSpeed < speed)
            {
                speed = maxSpeed;
            }
        }
        else
        {
            speed = static_cast<float>(static_cast<double>(frameLength) * maxDecel + speed);
            stop = 0.0f < speed;
        }
    }
    else
    {
        if (0.0f < speed)
        {
            speed = static_cast<float>(speed - static_cast<double>(frameLength) * maxDecel);

            if (speed < 0.0f)
            {
                speed = 0.0f;
            }
        }

        if (speed < 0.0f)
        {
            speed = static_cast<float>(static_cast<double>(frameLength) * maxDecel + speed);
            stop = 0.0f < speed;
        }
    }

    if (stop)
    {
        speed = 0.0f;
    }

    // The step this frame, along the lead car's (turned) facing.
    vector_3d move;
    move.x = 0.0f;
    move.y = 0.0f;
    move.z = 0.0f;

    if (speed != 0.0f)
    {
        const float reach = -(worldUnitsPerMeter * speed);
        const frame_of_ref turned = TurnedFrame(entry->car->getFrame());
        move.x = static_cast<float>(static_cast<double>(turned.j.x) * reach * frameLength);
        move.y = turned.j.y * reach * frameLength;
        move.z = turned.j.z * reach * frameLength;
    }

    // Each car on the rails: unlock its cells, move, lock the new ones.
    for (; entry != nullptr; entry = entry->next)
    {
        TrainCar* car = entry->car;

        if (car->derailed == 1)
        {
            continue;
        }

        lockTrackCells(car, trackDirection, 0);
        car->speed = speed;
        const vector_3d carPos = car->getPosition();
        vector_3d newPos;
        newPos.x = carPos.x + move.x;
        newPos.y = carPos.y + move.y;
        newPos.z = carPos.z + move.z;
        car->setPosition(newPos);
        lockTrackCells(car, trackDirection, 1);
    }
}

auto Train::AddCar(TrainCar* car) -> int32_t
{
    if (car->objectClass != TRAINCAR)
    {
        return static_cast<int32_t>(0xdefc0005);
    }

    auto* newEntry = new TrainListEntry;
    Assert(newEntry != nullptr ? 1u : 0u, 0, "Not enough memory to allocate new TrainListEntry");
    newEntry->car = car;
    TrainListEntry* tail = cars;

    while (tail != nullptr && tail->next != nullptr)
    {
        tail = tail->next;
    }

    if (tail == nullptr)
    {
        cars = newEntry;
    }
    else
    {
        // Hitch it carOffset behind the last car, along the (turned) lead car's main axis.
        const frame_of_ref turned = TurnedFrame(cars->car->getFrame());
        vector_3d carPos = tail->car->getPosition();

        if (std::abs(turned.j.x) <= std::abs(turned.j.y))
        {
            if (turned.j.y <= 0.0f)
            {
                carPos.y = carPos.y - carOffset;
            }
            else
            {
                carPos.y = carOffset + carPos.y;
            }
        }
        else if (turned.j.x <= 0.0f)
        {
            carPos.x = carPos.x - carOffset;
        }
        else
        {
            carPos.x = carOffset + carPos.x;
        }

        car->setPosition(carPos);
        tail->next = newEntry;
        newEntry->prev = tail;
    }

    car->train = this;
    numCars++;
    RecalcInfo();
    return numCars;
}

auto Train::RemoveCar(TrainCar* car, int justUnlink) -> int32_t
{
    TrainListEntry* entry = cars;

    while (entry != nullptr && entry->car != car)
    {
        entry = entry->next;
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
        float lastSpeed = speed;
        float lastDesiredSpeed = desiredSpeed;
        int32_t carsLeft = numCars;
        const auto moveCar = [&](Train* to, TrainCar* moving)
        {
            to->AddCar(moving);
            lastSpeed = speed;
            lastDesiredSpeed = desiredSpeed;
            carsLeft = RemoveCar(moving, 1);
        };

        Train* alone = trainManager->CreateTrain();
        TrainListEntry* behind = entry->next;
        moveCar(alone, entry->car);

        if (behind != nullptr)
        {
            Train* rest = trainManager->CreateTrain();

            do
            {
                TrainListEntry* nextEntry = behind->next;
                moveCar(rest, behind->car);
                behind = nextEntry;
            } while (behind != nullptr);

            if (carsLeft != 0)
            {
                lastSpeed = speed;
                lastDesiredSpeed = desiredSpeed;
            }

            rest->speed = lastSpeed;
            rest->desiredSpeed = lastDesiredSpeed;
            rest->RecalcInfo();
        }

        return carsLeft != 0 ? numCars : 0;
    }

    // Unlink it; a train left with no cars is removed and freed.
    TrainListEntry* before = entry->prev;
    TrainListEntry* after = entry->next;

    if (before == nullptr)
    {
        cars = after;
    }
    else
    {
        before->next = after;
    }

    if (after != nullptr)
    {
        after->prev = before;
    }

    delete entry;
    numCars--;

    if (numCars == 0)
    {
        trainManager->RemoveTrain(this);
        destroy();
        delete this;
        return 0;
    }

    RecalcInfo();
    return numCars;
}

auto Train::RecalcInfo() -> void
{
    // The train goes at the pace of its weakest car.
    maxDecel = -9999999.0f;
    maxAccel = -9999999.0f;
    maxSpeed = 9999999.0f;

    if (cars != nullptr)
    {
        leadPosition = cars->car->getPosition();

        for (TrainListEntry* entry = cars; entry != nullptr; entry = entry->next)
        {
            TrainCar* car = entry->car;

            if (maxAccel < car->GetMaxAccel())
            {
                maxAccel = car->GetMaxAccel();
            }

            if (maxDecel < car->GetMaxDecel())
            {
                maxDecel = car->GetMaxDecel();
            }

            if (car->GetMaxSpeed() < maxSpeed)
            {
                maxSpeed = car->GetMaxSpeed();
            }
        }
    }

    if (maxSpeed < std::abs(desiredSpeed))
    {
        if (0.0f < desiredSpeed)
        {
            desiredSpeed = maxSpeed;
        }
        else
        {
            desiredSpeed = -maxSpeed;
        }
    }
}

auto Train::GetTotalTonnage() -> float
{
    float tonnage = 0.0f;

    for (TrainListEntry* entry = cars; entry != nullptr; entry = entry->next)
    {
        tonnage = entry->car->getTonnage() + tonnage;
    }

    return tonnage;
}

//---------------------------------------------------------------------------
// TrainCarType
//---------------------------------------------------------------------------

TrainCarType::TrainCarType()
{
    damage = 0;
    explosionChance = 0;
    explosionDamage = 0;
    velocityMultiplier = 0;
    topSpeed = 0.0f;
    acceleration = 0.0f;
    deceleration = 0.0f;
    tonnageClass = -1.0f;
    nameId = 0;
    unknown48 = -1;
}

auto TrainCarType::createInstance() -> BaseObject*
{
    auto* newCar = new TrainCar;

    if (newCar == nullptr)
    {
        return nullptr;
    }

    if (newCar->init(this) != 0)
    {
        return nullptr;
    }

    newCar->idNumber = NextIdNumber++;
    return newCar;
}

auto TrainCarType::destroy() -> void
{
}

auto TrainCarType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile trainFile;
    int32_t result = trainFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = trainFile.seekBlock("Train")) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdLong("Name", nameId)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdUChar("Explosion Chance", explosionChance)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdUChar("Explosion Damage", explosionDamage)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdUChar("Velocity Multiplier", velocityMultiplier)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdFloat("Acceleration", acceleration)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdFloat("Deceleration", deceleration)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdFloat("TopSpeed", topSpeed)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdLong("Damage", damage)) != 0)
    {
        return result;
    }

    if ((result = trainFile.readIdFloat("TonnageClass", tonnageClass)) != 0)
    {
        return result;
    }

    if ((result = ObjectType::init(&trainFile)) != 0)
    {
        return result;
    }

    return 0;
}

auto TrainCarType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // The server's job in multiplayer.
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    auto* car = static_cast<TrainCar*>(collidee);
    Train* train = car->train;
    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    // The car takes (collider tonnage + 1) / 2, from the collider's side.
    const auto hitCar = [&](int32_t hitLocation)
    {
        const auto angle = static_cast<float>(car->relFacingTo(collider->getPosition(), -1));
        _WeaponShotInfo shot;
        shot.init(collider, -1, static_cast<float>((collider->getTonnage() + 1.0) * 0.5), hitLocation, angle);
        car->handleWeaponHit(&shot, multiplayer);
    };

    if (car->derailed == 1)
    {
        // A derailed car is only hurt by mechs, vehicles and elementals.
        if (collider->objectClass < BATTLEMECH || ELEMENTAL < collider->objectClass)
        {
            return 0;
        }

        hitCar(-1);
        return 0;
    }

    // Something heavy stops the train; movers and buildings in the way take the train's weight.
    if (20.0f <= collider->getTonnage())
    {
        train->speed = 0.0f;
    }

    switch (collider->objectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        case ELEMENTAL:
        {
            const int32_t hitLocation = collider->calcHitLocation(car, -1, 1, 0);
            const auto angle = static_cast<float>(collider->relFacingTo(car->getPosition(), -1));
            _WeaponShotInfo shot;
            shot.init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, hitLocation, angle);
            collider->handleWeaponHit(&shot, multiplayer);
            hitCar(hitLocation);
            return 0;
        }

        case BUILDING:
        case TREEBUILDING:
        {
            train->speed = 0.0f;
            _WeaponShotInfo shot;
            shot.init(car, -1, train->GetTotalTonnage() * 0.2f + 0.5f, -1, -1.0f);
            collider->handleWeaponHit(&shot, multiplayer);
            hitCar(-1);
            return 0;
        }

        default:
            return 0;
    }
}

auto TrainCarType::handleDestruction(GameObject* collidee, GameObject*) -> int
{
    const auto blast = static_cast<float>(explosionDamage);
    vector_3d where = collidee->getPosition();
    createExplosion(where, blast, blast);
    return 0;
}

//---------------------------------------------------------------------------
// TrainCar
//---------------------------------------------------------------------------

auto TrainCar::init() -> void
{
    appearance = nullptr;
    train = nullptr;
    name = nullptr;
    speed = 0.0f;
    wrecked = 0;
    onMap = 1;
    damageTaken = 0.0f;
    soundHandle = 0xffffffff;
    justCreated = 1;
}

auto TrainCar::getFrame() -> frame_of_ref
{
    return frame;
}

auto TrainCar::setFrame(frame_of_ref& newFrame) -> void
{
    frame = newFrame;
}

auto TrainCar::handleStaticCollision() -> void
{
    if (collisionsOn == 0 || onMap == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);
    char listName[12];
    std::sprintf(listName, "TBlk%d", blockNumber);
    ObjectQueueNode* list = findObjectList(listName);

    // Port fix: the original reads the list's objects without checking that the block has a list.
    if (list == nullptr)
    {
        return;
    }

    // The terrain objects of its own block, on its own vertex.
    BaseObject* object = list->head;

    while (object != nullptr)
    {
        auto* other = static_cast<GameObject*>(object);

        if (other->getObjectType() != nullptr)
        {
            int32_t otherBlock = -1;
            int32_t otherVertex = -1;

            switch (other->objectClass)
            {
                case BUILDING:
                case TREE:
                case TERRAINOBJECT:
                case TREEBUILDING:
                    other->getBlockAndVertexNumber(otherBlock, otherVertex);
                    break;
                case MISCTERRAINOBJECT:
                    // Original behaviour (OB-022): walls, bridges and forests read the train car's own vertex, so they
                    // always match.
                    getBlockAndVertexNumber(otherBlock, otherVertex);
                    break;
                default:
                    break;
            }

            if (vertexNumber == otherVertex)
            {
                collisionSystem->detectStaticCollision(this, other);
            }
        }

        // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
        // without one hangs the game here.
        object = object->next;
    }
}

auto TrainCar::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    objectClass = TRAINCAR;

    if (objType != nullptr)
    {
        auto* carType = static_cast<TrainCarType*>(objType);
        char nameBuffer[256];
        cLoadString(thisInstance, static_cast<uint32_t>(carType->nameId), nameBuffer, 0xfe);
        name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(nameBuffer) + 1)));
        std::strcpy(name, nameBuffer);
        damage = static_cast<float>(carType->damage);
        setTonnage(carType->tonnageClass);
        collisionsOn = 1;
    }

    derailed = 0;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdefc0003);
    }

    auto* vehicleAppearance = new GVAppearance;
    appearance = vehicleAppearance;

    if (vehicleAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdefc0001);
    }

    vehicleAppearance->init(nullptr, nullptr);

    if ((apprType->appearanceNum & 0xff000000) != 0x5000000)
    {
        return -0x2fff6;
    }

    if ((result = vehicleAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    setPotentialContact(1);
    justCreated = 1;
    return 0;
}

auto TrainCar::destroy() -> void
{
    if (name != nullptr)
    {
        systemHeap->free(name);
        name = nullptr;
    }
}

auto TrainCar::setPartId(int32_t trainNumber, int32_t carNumber) -> void
{
    partId = carNumber + (trainNumber * 5 + 0x6400) * 0x14;
}

auto TrainCar::isRevealed() -> int
{
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);
    const auto col = static_cast<uint32_t>((blockNumber % Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber % Terrain::verticesBlockSide);
    const auto row = static_cast<uint32_t>((blockNumber / Terrain::blocksMapSide) * Terrain::verticesBlockSide +
                                           vertexNumber / Terrain::verticesBlockSide);

    if (visibleBits->getFlag(row, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        return 1;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        return 1;
    }

    return visibleBits->getFlag(row, col + 1) != 0 ? 1 : 0;
}

auto TrainCar::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (onMap == 0 || camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    vector_2d screen100;
    vector_2d screen50;

    if (land != nullptr)
    {
        land->projectTerrain(position, screen100, screen50);
    }

    float screenY;

    if (camera->cameraScale == 1)
    {
        screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
        screenY = screen50.y - camera->screenUL50.y;
    }
    else
    {
        screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
        screenY = screen100.y - camera->screenUL.y;
    }

    screenPos.y = screenY + camera->halfHeight;

    if (appearance->recalcBounds(camera) == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto TrainCar::update() -> int32_t
{
    if (justCreated != 0)
    {
        justCreated = 0;
    }

    int visibleNow = 0;

    if (wrecked == 0)
    {
        // Damaged past half (or, from 10 points up, by chance) the car blows up and jumps the rails.
        const auto* carType = static_cast<TrainCarType*>(objType);
        const int32_t maxDamage = carType->damage;
        bool blowUp = static_cast<float>(maxDamage / 2) <= damageTaken;

        if (!blowUp && 10.0f <= damageTaken)
        {
            // Original behaviour (OB-024): the roll derails the car when its damage percentage is BELOW the roll, so
            // lightly damaged cars go more often.
            const int32_t roll = RandomNumber(100);
            blowUp = damageTaken * 100.0f / static_cast<float>(maxDamage) < static_cast<float>(roll);
        }

        if (blowUp && derailed != 1)
        {
            damage = 0.0f;
            status = 2;
            objType->handleDestruction(this, nullptr);
            derail(lastHitAngle);
        }

        // Off the map it stops drawing; on a broken bridge it falls in.
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        GameMap->worldToMapPos(getPosition(), tileR, tileC, cellR, cellC);
        onMap = tileR < 0 || GameMap->height <= tileR || tileC < 0 || GameMap->width <= tileC ? 0 : 1;

        if (onMap != 0)
        {
            const uint32_t overlayType = GameMap->map[GameMap->width * tileR + tileC].overlay & 0x7f;

            if (overlayType == 0x38 || overlayType == 0x3a)
            {
                wrecked = 1;
                derailed = 1;
                collisionsOn = 0;
                vector_3d where = getPosition();
                CreateExplosion(MineExplosion, where, 0.0f, 0.0f);
                status = 2;
                train->RemoveCar(this, 0);
                speed = 0.0f;
                train->RecalcInfo();
                objType->handleDestruction(this, nullptr);
            }
        }

        if (onMap != 0 && onScreen() != 0)
        {
            visibleNow = 1;
        }
    }

    mineCheck();

    if (appearance != nullptr)
    {
        appearance->visible = visibleNow;

        if (isDestroyed() != 0)
        {
            static_cast<GVAppearance*>(appearance)->setTypeId(GV_ACTOR_STATE_DESTROYED);
        }
    }

    appearance->update();
    return 1;
}

auto TrainCar::render() -> void
{
    if (wrecked != 0)
    {
        return;
    }

    if (justCreated == 0)
    {
        if (appearance != nullptr)
        {
            appearance->visible = onScreen() != 0 && onMap != 0 ? 1 : 0;
            appearance->update();
        }

        const int32_t contactType = getContactType(homeTeam->id);

        if (contactType == 2)
        {
            // A sensor contact: a blip sized by tonnage.
            uint8_t* shape;

            if (50.0f < getTonnage())
            {
                shape = scenario->sensorContactShapes[0];
            }
            else if (35.0f < getTonnage())
            {
                shape = scenario->sensorContactShapes[2];
            }
            else
            {
                shape = scenario->sensorContactShapes[4];
            }

            if (shape != nullptr)
            {
                if (VFX_shape_count(shape) < blipFrame)
                {
                    if (soundSystem != nullptr)
                    {
                        soundSystem->playDigitalSample(0x14, 1, this, 0, 1);
                    }

                    blipFrame = 0;
                }

                ElementList->openGroup(-100000, 1);
                ElementList->add(
                    ElementPool::Make<VFXElement>(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0));
                blipFrame++;
            }
        }
        else if (contactType == 1)
        {
            // Seen: drawn, with its looping sound.
            if (windowsVisible == turn)
            {
                auto* carAppearance = static_cast<GVAppearance*>(appearance);
                carAppearance->hazePalette = nullptr;
                carAppearance->render(0);

                if (soundHandle == 0xffffffff)
                {
                    const auto soundId = static_cast<uint32_t>(static_cast<TrainCarType*>(objType)->unknown48);

                    if (soundId != 0xffffffff)
                    {
                        soundHandle = static_cast<uint32_t>(soundSystem->playDigitalSample(soundId, 0, this, 1, 0));
                    }
                }
                else
                {
                    // Original behaviour (OB-023): a visible car stops its own looping sound when it is already
                    // playing, so the sound restarts every other frame.
                    soundSystem->stopDigitalSample(soundHandle);
                    soundHandle = 0xffffffff;
                }
            }
            else if (soundHandle != 0xffffffff)
            {
                soundSystem->stopDigitalSample(soundHandle);
                soundHandle = 0xffffffff;
            }
        }
    }

    if (drawExtents != 0)
    {
        // Debug: the extent radius as an ellipse.
        float radius = objType->extentRadius;

        if (eye->cameraScale == 1)
        {
            radius *= 0.5f;
        }

        const float scale = eye->cameraScale != 1 ? 1.0f : 0.5f;
        const float sx = (position.x - eye->position.x) * scale;
        const float sy = (position.y - eye->position.y) * scale;
        vector_2d center;
        center.x = sx * eye->cosAngle + sy * eye->cosAngle + eye->halfWidth;
        center.y =
            ((sx * eye->sinAngle + eye->halfHeight) - sy * eye->sinAngle) - scale * (position.z - eye->position.z);
        vector_2d size(radius, radius);
        ElementList->openGroup(-50000, 1);
        // Port: an overlay, on the screen over the view: it follows the object through the zoom.
        center = MCOverlayPoint(center);
        size.x *= MCOverlay.ScaleX;
        size.y *= MCOverlay.ScaleY;
        ElementList->add(ElementPool::Make<EllipseElement>(center, size, 0xfe, -50000));
    }
}

auto TrainCar::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    if (0.0f < shotInfo->damage && isDestroyed() == 0)
    {
        lastHitAngle = shotInfo->entryAngle;
        damageTaken = shotInfo->damage + damageTaken;
        const float remaining = damage - shotInfo->damage;
        damage = remaining;

        if (remaining <= 0.0f)
        {
            // Destroyed: it jumps the rails and leaves its train.
            status = 2;
            derail(static_cast<float>(RandomNumber(10) - 20));
            train->RemoveCar(this, 0);
            speed = 0.0f;
            train->RecalcInfo();
            objType->handleDestruction(this, nullptr);
            collisionsOn = 0;
        }
    }

    return 0;
}

auto TrainCar::derail(float angle) -> void
{
    if (derailed == 1)
    {
        return;
    }

    // Off the rails: free its cells, then (by the train's speed and a roll) take the car ahead or behind with it.
    lockTrackCells(this, train->trackDirection, 0);
    Assert(train != nullptr ? 1u : 0u, 0, "Car must have a train to derail");
    TrainListEntry* entry = train->cars;

    while (entry != nullptr && entry->car != this)
    {
        entry = entry->next;
    }

    Assert(entry != nullptr ? 1u : 0u, 0, "Can't find carEntry for this car");
    derailed = 1;
    const auto roll = static_cast<float>(RandomNumber(100));
    Train* oldTrain = train;
    const bool slowEnough = static_cast<double>(std::abs(oldTrain->speed)) * 5.0 <= roll;

    if ((speed <= 0.0f && slowEnough) || entry->next == nullptr)
    {
        if ((speed < 0.0f || !slowEnough) && entry->prev != nullptr)
        {
            entry->prev->car->derail(-angle);
        }
    }
    else
    {
        entry->next->car->derail(-angle);
    }

    // Port fix: the neighbour's derail can split and free the old train; the original then calls RemoveCar on the
    // freed train. Only a train the manager still holds is used.
    bool oldTrainAlive = false;

    for (int32_t i = 0; i < trainManager->numTrains && i < TrainManager::MAX_TRAINS; i++)
    {
        if (trainManager->trains[i] == oldTrain)
        {
            oldTrainAlive = true;
        }
    }

    if (oldTrainAlive)
    {
        oldTrain->RemoveCar(this, 0);
    }

    // Slew it round: a random swing (Faithful: -50..0 whichever side it was hit, 1..100 when head on).
    frame_of_ref turned = getFrame();
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
    const vector_3d oldI = turned.i;
    turned.i = turned.i * c + turned.j * s;
    turned.j = turned.j * c - oldI * s;
    setFrame(turned);

    // Into water: the car (and its one-car train) is gone.
    _ObjectPosition* objectPosition = getObjPosition();

    // Port fix: the original reads the object position without checking it for null.
    if (objectPosition != nullptr)
    {
        const int32_t tileR = objectPosition->tileR;
        const int32_t tileC = objectPosition->tileC;
        const MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

        if ((tile.cells & 0x7f) == 0x2b)
        {
            Train* lostTrain = train;
            trainManager->RemoveTrain(lostTrain);
            lostTrain->destroy();
            wrecked = 1;
            collisionsOn = 0;
            return;
        }

        // Onto a wall or bridge: it takes the train's weight.
        if ((tile.overlay & 0x7f) == 0x3e)
        {
            const int32_t vbs = Terrain::verticesBlockSide;
            const int32_t partId = ((Terrain::blocksMapSide * (tileR / vbs) + tileC / vbs) * 400 + tileC +
                                    ((tileR - vbs * (tileR / vbs)) - tileC / vbs) * vbs) *
                                       8 +
                                   0x1000;
            auto* hit = static_cast<GameObject*>(objectList->findObjectFromPart(partId));

            // Port fix: the original reads the object's class without checking that one was found.
            if (hit != nullptr && hit->objectClass == MISCTERRAINOBJECT)
            {
                _WeaponShotInfo shot;
                shot.init(nullptr, 0, train->GetTotalTonnage() * 0.1f + 0.5f, 0, 0.0f);

                if (MPlayer == nullptr)
                {
                    hit->handleWeaponHit(&shot, 0);
                }
                else if (MPlayer->isServer != 0)
                {
                    hit->handleWeaponHit(&shot, 1);
                }
            }
        }
    }

    vector_3d where = getPosition();
    CreateExplosion(MineExplosion, where, 0.0f, 0.0f);
}

auto TrainCar::mineCheck() -> void
{
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return;
    }

    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(getPosition(), tileR, tileC, cellR, cellC);
    // Each side's mines only go off under the other side.
    const uint32_t mine = alignment == -1 || alignment == 0 ? GameMap->getInnerSphereMine(tileR, tileC, cellR, cellC)
                                                            : GameMap->getClanMine(tileR, tileC, cellR, cellC);

    if (mine == 0)
    {
        return;
    }

    vector_3d where = getPosition();
    CreateExplosion(MineExplosion, where, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
    const int32_t hitLocation = calcHitLocation(nullptr, -1, 3, 0);
    _WeaponShotInfo shot;
    shot.init(nullptr, -2, MineBaseDamage, hitLocation, 0.0f);
    handleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
    MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];

    if (getAlignment() == -1 || getAlignment() == 0)
    {
        tile.overlay |= 0x1800;
    }
    else
    {
        tile.overlay |= 0x6000;
    }

    if (MPlayer != nullptr)
    {
        MPlayer->addMineChunk(cellR + tileR * 3, cellC + tileC * 3, alignment == -1 || alignment != 0 ? 0 : 1, 3, 2);
    }
}

auto TrainCar::relFacingTo(vector_3d goal, int32_t) -> float
{
    const float x = position.x;
    const float y = position.y;
    const frame_of_ref turned = TurnedFrame(frame);
    vector_3d facing;
    facing.x = -turned.j.x;
    facing.y = -turned.j.y;
    facing.z = -turned.j.z;

    vector_3d toGoal;
    toGoal.x = goal.x - x;
    toGoal.y = goal.y - y;
    toGoal.z = 0.0f;
    const double length =
        std::sqrt((static_cast<double>(toGoal.x) * toGoal.x + static_cast<double>(toGoal.y) * toGoal.y) +
                  static_cast<double>(toGoal.z) * toGoal.z);

    if (length != 0.0)
    {
        toGoal.x = static_cast<float>(toGoal.x / length);
        toGoal.y = static_cast<float>(toGoal.y / length);
        toGoal.z = static_cast<float>(toGoal.z / length);
    }

    const double cosine = static_cast<double>(toGoal.z) * facing.z + static_cast<double>(toGoal.y) * facing.y +
                          static_cast<double>(toGoal.x) * facing.x;
    const float angle = static_cast<float>(acosMatherr(cosine) * RADIANS_TO_DEGREES_F);

    // Negative to the left.
    if ((facing & toGoal).z >= 0.0f)
    {
        return -angle;
    }

    return angle;
}

auto TrainCar::relativePosition(float angle, float distance, uint32_t flags) -> vector_3d
{
    // Off the map: nowhere.
    if (onMap == 0)
    {
        vector_3d nowhere;
        nowhere.x = -999999.0f;
        nowhere.y = -999999.0f;
        nowhere.z = -999999.0f;
        return nowhere;
    }

    // The point distance meters away at angle: flag 1, an absolute angle in radians; else degrees from the car's
    // facing. The x87 keeps some of the sums below at extended precision, done here in double.
    const float reach = -(worldUnitsPerMeter * distance);
    const float x = position.x;
    const float y = position.y;
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
        frame_of_ref turned = frame;
        const double radians = (static_cast<double>(angle) + 45.0) * DEGREES_TO_RADIANS;
        const float s = static_cast<float>(std::sin(radians));
        const float c = static_cast<float>(std::cos(radians));
        const vector_3d oldI = turned.i;
        turned.i = turned.i * c + turned.j * s;
        turned.j = turned.j * c - oldI * s;
        const vector_3d offset = turned.j * reach;
        offsetX = offset.x;
        offsetY = offset.y;
    }

    const double targetX = offsetX + x;
    const float targetY = static_cast<float>(static_cast<double>(offsetY) + y);

    // Flag 2 walks from the car out to the point; otherwise from the point back to the car.
    vector_2d start;
    vector_2d end;

    if ((flags & 2) != 0)
    {
        end.x = static_cast<float>(targetX);
        start.x = x;
        start.y = y;
        end.y = targetY;
    }
    else
    {
        start.y = targetY;
        start.x = static_cast<float>(targetX);
        end.x = x;
        end.y = y;
    }

    // Half a map cell per step.
    const double deltaX = static_cast<double>(end.x) - start.x;
    const float deltaXf = static_cast<float>(deltaX);
    const float deltaY = end.y - start.y;
    const float length =
        static_cast<float>(std::sqrt(static_cast<double>(deltaY) * deltaY + static_cast<double>(deltaXf) * deltaXf));
    double directionX = deltaX;
    float directionY = deltaY;

    if (length != 0.0)
    {
        directionX = static_cast<double>(deltaXf) / length;
        directionY = static_cast<float>(static_cast<double>(deltaY) / length);
    }

    const float stepLength = static_cast<float>(static_cast<double>(Terrain::metersPerVertex) * 0.33333334f * 0.5);
    const float stepX = static_cast<float>(directionX * stepLength);
    const double stepYExact = static_cast<double>(directionY) * stepLength;
    const float stepY = static_cast<float>(stepYExact);

    if (std::sqrt(stepYExact * stepY + static_cast<double>(stepX) * stepX) == 0.0)
    {
        vector_3d result;
        result.x = x;
        result.y = y;
        result.z = 0.0f;
        return result;
    }

    const vector_2d span = start - end;
    const float maxDistance =
        static_cast<float>(std::sqrt(static_cast<double>(span.x) * span.x + static_cast<double>(span.y) * span.y));
    float traveled = 0.0f;
    vector_2d current = start;

    // Whether the cell under current is passable.
    auto cellPassable = [&]()
    {
        vector_3d point;
        point.x = current.x;
        point.y = current.y;
        point.z = 0.0f;
        int32_t tileR;
        int32_t tileC;
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(point, tileR, tileC, cellR, cellC);

        // Port fix: the walk can leave the map, where the original reads outside it. Off the map is impassable.
        if (!GameMap->onMap(tileR, tileC))
        {
            return 0u;
        }

        return GameMap->map[GameMap->width * tileR + tileC].getCellPassable(cellR, cellC);
    };

    uint32_t passable = cellPassable();
    vector_2d previous = start;
    // Walk until the cell changes kind (or the distance runs out); the answer is the step before.
    const uint32_t keepGoingWhile = (flags & 2) != 0 ? 1u : 0u;

    if ((passable != 0) == (keepGoingWhile != 0))
    {
        while (traveled < maxDistance)
        {
            previous = current;
            current.x = stepX + current.x;
            current.y = stepY + current.y;
            const double dx = static_cast<double>(current.x) - start.x;
            const double dy = static_cast<double>(current.y) - start.y;
            traveled = static_cast<float>(std::sqrt(dx * dx + dy * dy));
            passable = cellPassable();

            if ((passable != 0) != (keepGoingWhile != 0))
            {
                break;
            }
        }
    }

    vector_3d ground;
    ground.x = previous.x;
    ground.y = previous.y;
    ground.z = 0.0f;
    vector_3d result;
    result.x = previous.x;
    result.y = previous.y;
    result.z = GameMap->getTerrainElevation(ground);
    return result;
}

auto TrainCar::GetMaxAccel() -> float
{
    return static_cast<TrainCarType*>(objType)->acceleration;
}

auto TrainCar::GetMaxDecel() -> float
{
    return static_cast<TrainCarType*>(objType)->deceleration;
}

auto TrainCar::GetMaxSpeed() -> float
{
    return static_cast<TrainCarType*>(objType)->topSpeed;
}

//---------------------------------------------------------------------------
// TrainListEntry
//---------------------------------------------------------------------------

auto TrainListEntry::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto TrainListEntry::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

TrainListEntry::TrainListEntry()
{
    init();
}

auto TrainListEntry::init() -> void
{
    car = nullptr;
    prev = nullptr;
    next = nullptr;
}

//---------------------------------------------------------------------------
// TrainManager
//---------------------------------------------------------------------------

auto TrainManager::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto TrainManager::operator delete(void* ptr) -> void
{
    systemHeap->free(ptr);
}

auto TrainManager::init() -> void
{
    for (int32_t i = 0; i < MAX_TRAINS; i++)
    {
        trains[i] = nullptr;
    }

    numTrains = 0;
}

auto TrainManager::destroy() -> void
{
    for (int32_t i = 0; i < MAX_TRAINS; i++)
    {
        if (trains[i] != nullptr)
        {
            // Faithful: freed without running the destructor.
            trains[i]->destroy();
            systemHeap->free(trains[i]);
            trains[i] = nullptr;
        }
    }
}

auto TrainManager::CreateTrain() -> Train*
{
    Train* newTrain = nullptr;
    const int32_t index = numTrains;

    if (index < MAX_TRAINS)
    {
        newTrain = new Train;
        trains[index] = newTrain;

        if (newTrain != nullptr)
        {
            numTrains++;
        }
    }

    return newTrain;
}

auto TrainManager::RemoveTrain(Train* train) -> void
{
    int32_t index = 0;

    while (index < numTrains && trains[index] != train)
    {
        index++;
    }

    // Port fix: the original shifts in (and then clears) the slot after the last, which with 64 trains is the
    // count itself.
    for (; index < numTrains; index++)
    {
        trains[index] = index + 1 < MAX_TRAINS ? trains[index + 1] : nullptr;
    }

    if (numTrains < MAX_TRAINS)
    {
        trains[numTrains] = nullptr;
    }

    // Faithful: the count drops even when the train wasn't in the list.
    numTrains--;
}

auto TrainManager::UpdateTrains() -> void
{
    for (int32_t i = 0; i < numTrains; i++)
    {
        trains[i]->Update();
    }
}
