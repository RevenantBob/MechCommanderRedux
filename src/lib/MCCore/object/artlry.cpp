#include "stdafx.h"
#include "object/artlry.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "engine/cfont.h"
#include "gui/asystem.h"
#include "iface/iface.h"
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
#include "object/collsn.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/explode.h"
#include "object/gate.h"
#include "object/mech.h"
#include "object/object.h"
#include "object/objevnt.h"
#include "object/objque.h"
#include "object/team.h"
#include "object/turret.h"
#include "sound/soundsys.h"
#include "sprite/gvactor.h"
#include "sprite/sprtmgr.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Half pi, as MCX.EXE stores it (a hair under the true value).</summary>
    constexpr double HALF_PI = 0x1.921fb5443e88cp+0;

    /// <summary>Incoming-shell sample of the multiplayer strikes (types 507-509), from 4 seconds out.</summary>
    constexpr uint32_t SAMPLE_INCOMING_MP = 0x3f;
    /// <summary>Incoming-shell sample of the other strikes, from 2 seconds out.</summary>
    constexpr uint32_t SAMPLE_INCOMING = 0x3e;
    /// <summary>Object type of the camera drone a sensor probe launches.</summary>
    constexpr int32_t CAMERA_DRONE_TYPE = 0x205;
    /// <summary>Part id of the first camera drone.</summary>
    constexpr int32_t FIRST_CAMERA_DRONE_PART_ID = 0x802c8;
    /// <summary>How many camera drones a game can launch.</summary>
    constexpr int32_t MAX_CAMERA_DRONES = 1000;

    /// <summary>Turns a frame about its k axis (MC2's inline frame_of_ref::rotate_about_k).</summary>
    void rotateAboutK(frame_of_ref& frame, float s, float c)
    {
        const vector_3d oldI = frame.i;
        frame.i = frame.i * c + frame.j * s;
        frame.j = frame.j * c - oldI * s;
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

    /// <summary>Runs a collision check between the strike and every object of the list.</summary>
    void collideWithList(Artillery* strike, ObjectQueueNode* list)
    {
        if (list == nullptr)
        {
            return;
        }

        BaseObject* object = list->head;

        while (object != nullptr)
        {
            auto* other = static_cast<GameObject*>(object);

            if (other->getObjectType() != nullptr)
            {
                // The block and vertex are fetched but never used.
                int32_t otherBlock = -1;
                int32_t otherVertex = -1;

                switch (other->objectClass)
                {
                    case BUILDING:
                    case TREE:
                    case TERRAINOBJECT:
                    case MISCTERRAINOBJECT:
                    case TREEBUILDING:
                    case CAMERADRONE:
                        other->getBlockAndVertexNumber(otherBlock, otherVertex);
                        break;
                    default:
                        break;
                }

                collisionSystem->detectStaticCollision(strike, other);
            }

            // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
            // without one hangs the game here.
            object = object->next;
        }
    }

    /// <summary>
    /// Sets off every entry of the strike's explosion pattern whose delay has passed since impact and that hasn't
    /// gone off: <c>explosionsPerExplosion</c> explosions scattered around the entry's offset.
    /// </summary>
    /// <returns>True when the pattern's last entry just went off (the strike is over).</returns>
    bool setOffExplosions(Artillery* strike)
    {
        auto* type = static_cast<ArtilleryType*>(strike->objType);

        for (int32_t i = 0; i < type->numExplosions; i++)
        {
            if (type->explosionDelay[i] < std::fabs(strike->timeToImpact) && strike->explosionsDone[i] == 0)
            {
                const float centerX = strike->position.x + type->explosionOffsetX[i];
                const float centerY = strike->position.y + type->explosionOffsetY[i];
                const float centerZ = strike->position.z;

                for (int32_t n = 0; n < type->explosionsPerExplosion; n++)
                {
                    float offsetX = static_cast<float>(
                        RandomNumber(static_cast<ArtilleryType*>(strike->getObjectType())->explosionRandomOffsetX));
                    float offsetY = static_cast<float>(
                        RandomNumber(static_cast<ArtilleryType*>(strike->getObjectType())->explosionRandomOffsetY));

                    if (RollDice(50) != 0)
                    {
                        offsetX = -offsetX;
                    }

                    if (RollDice(50) != 0)
                    {
                        offsetY = -offsetY;
                    }

                    vector_3d spot(offsetX + centerX, offsetY + centerY, centerZ);
                    type->createExplosion(spot, 0.0f, 0.0f);
                }

                type = static_cast<ArtilleryType*>(strike->objType);
                strike->explosionsDone[i] = 1;

                if (i + 1 == type->numExplosions)
                {
                    return true;
                }
            }
        }

        return false;
    }

    /// <summary>
    /// Projects the object to the screen through the terrain (the 100% or 50% projection, by the camera's scale)
    /// into <c>screenPos</c>.
    /// </summary>
    void projectToScreen(BigGameObject* object, Camera* camera)
    {
        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            land->projectTerrain(object->position, screen100, screen50);
        }

        float screenY;

        if (camera->cameraScale == 1)
        {
            object->screenPos.x = (screen50.x - camera->screenUL50.x) + camera->halfWidth;
            screenY = screen50.y - camera->screenUL50.y;
        }
        else
        {
            object->screenPos.x = (screen100.x - camera->screenUL.x) + camera->halfWidth;
            screenY = screen100.y - camera->screenUL.y;
        }

        object->screenPos.y = screenY + camera->halfHeight;
    }
} // namespace

int32_t artilleryTypeTable[8] = {249, 248, 250, 516, 508, 507, 509, 516};
int32_t numCameraDrones = 0;

//---------------------------------------------------------------------------
// CallArtillery / ArtilleryChunk
//---------------------------------------------------------------------------

void CallArtillery(int32_t commanderId, int32_t strikeType, vector_3d location, int32_t seconds, int randomOffset)
{
    Commander* commander = CommanderTable[commanderId];

    switch (strikeType)
    {
        case 0:
        case 4:
        {
            if (commander->numSmallStrikes < 1)
            {
                return;
            }

            commander->numSmallStrikes--;
            break;
        }
        case 1:
        case 5:
        {
            if (commander->numLargeStrikes < 1)
            {
                return;
            }

            commander->numLargeStrikes--;
            break;
        }
        case 2:
        case 6:
        {
            if (commander->numSensorStrikes < 1)
            {
                return;
            }

            commander->numSensorStrikes--;
            break;
        }
        case 3:
        case 7:
        {
            if (commander->numCameraDrones < 1)
            {
                return;
            }

            commander->numCameraDrones--;
            break;
        }
        default:
            Fatal(0, " ArtilleryStrike: Bad StrikeType ");
    }

    if (MPlayer != nullptr)
    {
        if (strikeType == 4)
        {
            strikeType = 0;
        }
        else if (strikeType == 5)
        {
            strikeType = 1;
        }
        else if (strikeType == 6)
        {
            strikeType = 2;
        }
    }

    auto* strike = static_cast<Artillery*>(createObject(artilleryTypeTable[strikeType]));
    strike->unknownCC = randomOffset;
    strike->setAlignment(CommanderTable[commanderId]->getTeam()->alignment);

    if (objectList->head != nullptr && strike != nullptr)
    {
        objectList->head->addNode(strike);
    }

    strike->setPosition(location);

    if (CommanderTable[commanderId] == HomeCommander)
    {
        for (ArtilleryButton* button : theInterface->tacticalMap->artilleryButtons)
        {
            button->draw();
        }
    }

    if (seconds != -1)
    {
        strike->timeToImpact = static_cast<float>(seconds);
    }

    if (seconds < 3)
    {
        strike->timeToImpact = -1.0f;
    }

    if (MPlayer != nullptr && MPlayer->isServer != 0)
    {
        MPlayer->addArtilleryChunk(commanderId, strikeType, location, seconds);
    }
}

void* ArtilleryChunk::operator new(size_t size) noexcept
{
    if (systemHeap != nullptr)
    {
        return systemHeap->malloc(static_cast<uint32_t>(size));
    }

    return std::malloc(size);
}

void ArtilleryChunk::operator delete(void* ptr)
{
    if (systemHeap != nullptr)
    {
        systemHeap->free(ptr);
        return;
    }

    std::free(ptr);
}

auto ArtilleryChunk::build(int32_t newCommanderId, int32_t newStrikeType, vector_3d location, int32_t newSeconds)
    -> void
{
    commanderId = static_cast<int8_t>(newCommanderId);
    strikeType = static_cast<int8_t>(newStrikeType);
    worldCoordToMapCell(location, cellRow, cellCol);
    seconds = static_cast<int8_t>(newSeconds);
    data = 0;
}

auto ArtilleryChunk::pack() -> void
{
    // The signed fields are sign-extended, as in the original.
    data = static_cast<uint32_t>(((((cellRow << 10) | cellCol) << 3 | static_cast<int32_t>(strikeType)) << 3) |
                                 ((static_cast<int32_t>(seconds) + 1) * 0x4000000) | static_cast<int32_t>(commanderId));
}

auto ArtilleryChunk::unpack() -> void
{
    commanderId = static_cast<int8_t>(data & 7);
    strikeType = static_cast<int8_t>((data >> 3) & 7);
    cellCol = static_cast<int32_t>((data >> 6) & 0x3ff);
    cellRow = static_cast<int32_t>((data >> 16) & 0x3ff);
    seconds = static_cast<int8_t>(static_cast<uint8_t>(data >> 26) - 1);
}

auto ArtilleryChunk::equalTo(ArtilleryChunk* chunk) -> int
{
    if (commanderId != chunk->commanderId)
    {
        return 0;
    }

    if (strikeType != chunk->strikeType)
    {
        return 0;
    }

    if (cellRow != chunk->cellRow)
    {
        return 0;
    }

    if (cellCol != chunk->cellCol)
    {
        return 0;
    }

    return seconds == chunk->seconds ? 1 : 0;
}

//---------------------------------------------------------------------------
// ArtilleryType
//---------------------------------------------------------------------------

auto ArtilleryType::createInstance() -> BaseObject*
{
    auto* newStrike = new Artillery;

    if (newStrike == nullptr)
    {
        return nullptr;
    }

    if (newStrike->init(this) != 0)
    {
        return nullptr;
    }

    newStrike->idNumber = NextIdNumber++;
    return newStrike;
}

auto ArtilleryType::destroy() -> void
{
    spriteManager->freeShapeRAM(shapeData);
    shapeData = nullptr;
    systemHeap->free(explosionOffsetX);
    explosionOffsetX = nullptr;
    systemHeap->free(explosionOffsetY);
    explosionOffsetY = nullptr;
    systemHeap->free(explosionDelay);
    explosionDelay = nullptr;
}

auto ArtilleryType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile artFile;
    int32_t result = artFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = artFile.seekBlock("Artillery")) != 0)
    {
        return result;
    }

    char spriteName[80];

    if ((result = artFile.readIdString("ArtillerySpriteName", spriteName, 79)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdULong("FrameCount", frameCount)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdULong("StartFrame", startFrame)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("FrameRate", frameRate)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalTimeToImpact", nominalTimeToImpact)) != 0)
    {
        return result;
    }

    if (artFile.readIdFloat("NominalTimeToLaunch", nominalTimeToLaunch) != 0)
    {
        nominalTimeToLaunch = nominalTimeToImpact - 10.0f;
    }

    if ((result = artFile.readIdFloat("NominalDamage", nominalDamage)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalMajorRange", nominalMajorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalMajorHits", nominalMajorHits)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalMinorRange", nominalMinorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalMinorHits", nominalMinorHits)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalSensorTime", nominalSensorTime)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("NominalSensorRange", nominalSensorRange)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("fontScale", fontScale)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("fontXOffset", fontXOffset)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdFloat("fontYOffset", fontYOffset)) != 0)
    {
        return result;
    }

    if ((result = artFile.readIdULong("fontColor", fontColor)) != 0)
    {
        return result;
    }

    if (nominalDamage == 0.0f)
    {
        explosionDelay = nullptr;
        explosionOffsetY = nullptr;
        explosionOffsetX = nullptr;
    }
    else
    {
        if ((result = artFile.readIdLong("NumExplosions", numExplosions)) != 0)
        {
            return result;
        }

        const int32_t count = numExplosions;
        const uint32_t tableSize = static_cast<uint32_t>(count) * sizeof(float);
        explosionOffsetX = static_cast<float*>(systemHeap->malloc(tableSize));
        explosionOffsetY = static_cast<float*>(systemHeap->malloc(tableSize));
        explosionDelay = static_cast<float*>(systemHeap->malloc(tableSize));
        char keyName[52];

        for (int32_t i = 0; i < count; i++)
        {
            std::sprintf(keyName, "ExplosionDelay%d", i);

            if ((result = artFile.readIdFloat(keyName, explosionDelay[i])) != 0)
            {
                return result;
            }

            std::sprintf(keyName, "ExplosionOffsetX%d", i);

            if ((result = artFile.readIdFloat(keyName, explosionOffsetX[i])) != 0)
            {
                return result;
            }

            std::sprintf(keyName, "ExplosionOffsetY%d", i);

            if ((result = artFile.readIdFloat(keyName, explosionOffsetY[i])) != 0)
            {
                return result;
            }
        }

        if ((result = artFile.readIdLong("ExplosionsPerExplosion", explosionsPerExplosion)) != 0)
        {
            return result;
        }

        if ((result = artFile.readIdLong("ExplosionRandomOffsetX", explosionRandomOffsetX)) != 0)
        {
            return result;
        }

        if ((result = artFile.readIdLong("ExplosionRandomOffsetY", explosionRandomOffsetY)) != 0)
        {
            return result;
        }

        if (artFile.readIdLong("MinArtilleryHeadRange", minArtilleryHeadRange) != 0)
        {
            minArtilleryHeadRange = 5;
        }
    }

    FullPathFileName spritePath;
    spritePath.init(shapesPath, spriteName, ".shp");
    File spriteFile;

    if ((result = spriteFile.open(spritePath, READ, 50)) != 0)
    {
        return result;
    }

    const uint32_t spriteSize = spriteFile.fileSize();
    shapeData = static_cast<uint8_t*>(spriteManager->mallocShapeRAM(spriteSize));

    if (shapeData == nullptr)
    {
        spriteManager->dumpLRU(static_cast<int32_t>(spriteSize));
        shapeData = static_cast<uint8_t*>(spriteManager->mallocShapeRAM(spriteSize));

        if (shapeData == nullptr)
        {
            return -0x2102ffff;
        }
    }

    spriteFile.read(shapeData, static_cast<int32_t>(spriteSize));
    spriteFile.close();
    return ObjectType::init(&artFile);
}

auto ArtilleryType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    auto* strike = static_cast<Artillery*>(collidee);

    if ((MPlayer != nullptr && MPlayer->isServer == 0) || strike->hasImpacted == 0)
    {
        return 0;
    }

    const vector_3d colliderPos = collider->getPosition();
    const vector_3d strikePos = collidee->getPosition();
    const double dx = static_cast<double>(colliderPos.x) - strikePos.x;
    const double dy = static_cast<double>(colliderPos.y) - strikePos.y;
    const auto distance = static_cast<float>(std::sqrt(dx * dx + dy * dy) * metersPerWorldUnit);

    // A turret or gate counts as hit from anywhere within its little extent of the major range.
    if (collider->objectClass == TURRET || collider->objectClass == GATE)
    {
        // TurretType and GateType both keep littleExtent at +0x58.
        const double extent =
            collider->objectClass == TURRET
                ? static_cast<double>(static_cast<TurretType*>(collider->objType)->littleExtent) * metersPerWorldUnit
                : static_cast<double>(static_cast<GateType*>(collider->objType)->littleExtent) * metersPerWorldUnit;

        if (extent < distance && static_cast<ArtilleryType*>(collidee->getObjectType())->nominalMajorRange <
                                     static_cast<float>(distance - extent))
        {
            return 0;
        }
    }

    // Beyond the major range the minor hit count lands, else the major one.
    const bool minor = static_cast<ArtilleryType*>(collidee->getObjectType())->nominalMajorRange < distance;
    auto hitCount = [&]()
    {
        auto* type = static_cast<ArtilleryType*>(collidee->getObjectType());
        return minor ? type->nominalMinorHits : type->nominalMajorHits;
    };

    if (0.0f < hitCount())
    {
        int32_t hit = 0;

        do
        {
            _WeaponShotInfo shot;
            shot.init(nullptr, -3, static_cast<ArtilleryType*>(collidee->getObjectType())->nominalDamage, 0, 0.0f);
            const int32_t colliderClass = collider->objectClass;

            if (colliderClass == BATTLEMECH || colliderClass == GROUNDVEHICLE || colliderClass == ELEMENTAL ||
                colliderClass == MOVER)
            {
                const int32_t hitTable = static_cast<float>(minArtilleryHeadRange) < distance ? 4 : 2;
                shot.hitLocation = collider->calcHitLocation(collidee, -1, hitTable, 0);
                shot.setEntryAngle(collider->relFacingTo(collidee->getPosition(), -1));
            }

            collider->handleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
            hit++;
        } while (static_cast<float>(hit) < hitCount());
    }

    return 0;
}

auto ArtilleryType::handleDestruction(GameObject* /*collidee*/, GameObject* /*collider*/) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Artillery
//---------------------------------------------------------------------------

Artillery::Artillery()
{
    init();
    timeToImpact = -1.0f;
    timeToLaunch = -1.0f;
    justCreated = 1;
    unknownCC = 1;
    currentFrame = 0;
    frameTime = 0.0f;
    frameCount = 0;
    startTime = 0.0f;
    sensorSystem = nullptr;
    sensorRange = 0.0f;
    sensorTime = 0.0f;
    hasImpacted = 0;
    impactSoundPlayed = 0;
}

auto Artillery::init() -> void
{
    startTime = scenarioTime;
}

auto Artillery::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    setExists(1);
    justCreated = 1;
    objectClass = ARTILLERY;
    hasImpacted = 0;
    timeToImpact = -1.0f;
    auto* type = static_cast<ArtilleryType*>(objType);

    if (type->nominalDamage != 0.0f)
    {
        const uint32_t count = static_cast<uint32_t>(type->numExplosions);
        explosionsDone = static_cast<int32_t*>(systemHeap->malloc(count * sizeof(int32_t)));
        std::memset(explosionsDone, 0, count * sizeof(int32_t));
        return 0;
    }

    explosionsDone = nullptr;
    return 0;
}

auto Artillery::destroy() -> void
{
    if (sensorSystem != nullptr)
    {
        sensorSystem->setTeam(nullptr);
        sensorSystemManager->freeSensor(sensorSystem);
        sensorSystem = nullptr;
    }

    systemHeap->free(explosionsDone);
}

auto Artillery::update() -> int32_t
{
    if (justCreated != 0)
    {
        setJustCreated();
    }

    auto* type = static_cast<ArtilleryType*>(objType);

    if (type != nullptr && type->shapeData != nullptr)
    {
        frameTime += frameLength;
        const double frames = static_cast<double>(frameTime * type->frameRate);

        if (frameCount < static_cast<int32_t>(std::floor(frames)))
        {
            const int32_t newCount = static_cast<int32_t>(std::floor(frames));
            const int32_t oldCount = frameCount;
            frameCount = static_cast<int32_t>(std::floor(frames));

            if (newCount - oldCount != 0)
            {
                currentFrame += static_cast<uint32_t>(newCount - oldCount);

                if (type->frameCount <= currentFrame)
                {
                    currentFrame %= type->frameCount;
                }
            }
        }
    }

    timeToImpact -= frameLength;
    timeToLaunch -= frameLength;

    // After impact the rest of the pattern goes off on its delays.
    if (hasImpacted != 0 && 0.0 < type->nominalDamage)
    {
        collisionsOn = 0;

        if (setOffExplosions(this))
        {
            return 0;
        }
    }

    if (timeToImpact <= 5.0 && impactSoundPlayed == 0 && soundSystem != nullptr &&
        0.0 < static_cast<ArtilleryType*>(objType)->nominalDamage)
    {
        const int32_t typeNum = getObjectType()->objTypeNum;

        if (typeNum >= 507 && typeNum <= 509 && timeToImpact < 4.0)
        {
            impactSoundPlayed = 1;
            soundSystem->playDigitalSample(SAMPLE_INCOMING_MP, 1, this, 0, 0);
        }
        else if (timeToImpact < 2.0)
        {
            impactSoundPlayed = 1;
            soundSystem->playDigitalSample(SAMPLE_INCOMING, 1, this, 0, 0);
        }
    }

    // Impact: the first explosions go off, and from now on the strike collides.
    if (hasImpacted == 0 && timeToImpact <= 0.0 && 0.0 < static_cast<ArtilleryType*>(objType)->nominalDamage)
    {
        if (setOffExplosions(this))
        {
            return 0;
        }

        if (unknownCC != 0)
        {
            RandomNumber(500);
            RandomNumber(500);
        }

        hasImpacted = 1;
        collisionsOn = 1;
    }

    // The sensor probe's sensor shrinks as its time runs out; the strike ends with it.
    if (0.0 < sensorTime && sensorActive != 0)
    {
        sensorTime -= frameLength;
        auto* sensorType = static_cast<ArtilleryType*>(objType);
        sensorRange = sensorTime / sensorType->nominalSensorTime * sensorType->nominalSensorRange * worldUnitsPerMeter;
        sensorSystem->setRange(sensorRange * metersPerWorldUnit);
    }
    else if (sensorTime <= 0.0 && sensorActive != 0)
    {
        return 0;
    }

    type = static_cast<ArtilleryType*>(objType);

    if (type->nominalDamage == 0.0f && timeToImpact <= 0.0 && sensorActive == 0)
    {
        if (0.0f < type->nominalSensorTime)
        {
            sensorActive = 1;
            sensorTime = type->nominalSensorTime;
            return 1;
        }

        // No sensor time: the strike launches a camera drone instead.
        if (numCameraDrones == MAX_CAMERA_DRONES)
        {
            Fatal(0, " Artillery.update: Too many camera drones ");
        }

        auto* drone = static_cast<CameraDrone*>(createObject(CAMERA_DRONE_TYPE));
        const int32_t partId = numCameraDrones + FIRST_CAMERA_DRONE_PART_ID;
        numCameraDrones++;
        drone->launchTime = scenarioTime;
        drone->setPartId(partId);
        vector_3d here = getPosition();
        drone->setPosition(here);
        drone->spiralDirection = -1;
        drone->setAlignment(alignment);
        GameObjectMap->addObject(drone);

        if (objectList->head != nullptr && drone != nullptr)
        {
            objectList->head->addNode(drone);
        }

        drone->findNextTargetTile();
        return 0;
    }

    return 1;
}

auto Artillery::render() -> void
{
    if (onScreen() == 0 || objType == nullptr)
    {
        return;
    }

    auto* type = static_cast<ArtilleryType*>(objType);
    uint8_t* shape = type->shapeData;

    if (justCreated != 0)
    {
        setJustCreated();
    }

    // The home side sees its own strikes count down; everyone sees one in its last four seconds.
    const int32_t homeAlignment = homeTeam->alignment;

    if (getAlignment() != homeAlignment && !(timeToImpact < 4.0))
    {
        return;
    }

    if (shape == nullptr)
    {
        return;
    }

    if (!(0.0 < timeToImpact) && static_cast<ArtilleryType*>(objType)->nominalSensorTime == 0.0)
    {
        return;
    }

    int32_t frame = static_cast<int32_t>(currentFrame);

    if (selected != 0)
    {
        recalcBounds(eye);
        drawSelectBox(0xfd);
    }

    // The 50% frames follow the 100% ones.
    if (eye->cameraScale == 1)
    {
        frame += static_cast<int32_t>(static_cast<ArtilleryType*>(objType)->frameCount);
    }

    ElementList->openGroup(-40000, 1);
    ElementList->add(ElementPool::Make<VFXElement>(shape, screenPos.x, screenPos.y, frame, 0, nullptr, 1, 0));

    const int32_t seconds = std::abs(static_cast<int32_t>(std::floor(static_cast<double>(timeToImpact))));
    std::sprintf(timeString, "%01d:%02d", seconds / 60, seconds % 60);
    type = static_cast<ArtilleryType*>(objType);
    // Port: the countdown is an overlay, on the screen over the view at its scale; the marker stays on the ground.
    const vector_2d textPos = MCOverlayPoint(screenPos);
    screenPos.x = type->fontXOffset + textPos.x + 6.0f;
    screenPos.y = type->fontYOffset + textPos.y;
    // Blue after impact, yellow before (the original has the same code for both camera scales).
    aFont* font = timeToImpact <= 0.0f ? blueDropFont : yellowDropFont;
    ElementList->add(ElementPool::Make<FontElement>(font, screenPos, timeString, -40000));
}

auto Artillery::handleEvent(ObjectEvent* event) -> int32_t
{
    if (event->type == 0)
    {
        switch (event->id)
        {
            case 0x1c:
                selected = 1;
                break;
            case 0x1d:
                selected = 0;
                break;
            case 0x1e:
                unknown2C = 1;
                break;
            case 0x1f:
                unknown2C = 0;
                break;
            default:
                break;
        }
    }

    return 0;
}

auto Artillery::handleStaticCollision() -> void
{
    if (collisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);

    // Mines in the 3x3 map tiles around the strike go off.
    int32_t centerR = 0;
    int32_t centerC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(getPosition(), centerR, centerC, cellR, cellC);

    for (int32_t tileR = centerR - 1; tileR < centerR + 2; tileR++)
    {
        for (int32_t tileC = centerC - 1; tileC < centerC + 2; tileC++)
        {
            if (tileR * 3 <= -3 || tileR >= GameMap->height || tileC * 3 <= -3 || tileC >= GameMap->width)
            {
                continue;
            }

            const uint32_t overlay = GameMap->map[GameMap->width * tileR + tileC].overlay;

            if ((overlay & 0x6000) == 0x4000)
            {
                vector_3d minePos;
                mapTileCellToWorldPos(tileR, tileC, 1, 1, minePos);
                GameMap->map[GameMap->width * tileR + tileC].overlay |= 0x6000;

                if (MPlayer != nullptr)
                {
                    MPlayer->addMineChunk(tileR * 3, tileC * 3, 1, 3, 2);
                }

                CreateExplosion(MineExplosion, minePos, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
            }

            if (((overlay >> 11) & 3) == 2)
            {
                vector_3d minePos;
                mapTileCellToWorldPos(tileR, tileC, 1, 1, minePos);
                GameMap->map[GameMap->width * tileR + tileC].overlay |= 0x1800;

                if (MPlayer != nullptr)
                {
                    MPlayer->addMineChunk(tileR * 3, tileC * 3, 0, 3, 2);
                }

                CreateExplosion(MineExplosion, minePos, MineSplashDamage, worldUnitsPerMeter * MineSplashRange);
            }
        }
    }

    // Then the terrain objects of the 3x3 terrain blocks around it.
    const int32_t firstBlock = blockNumber - Terrain::blocksMapSide - 1;

    for (int32_t row = 0; row < 3; row++)
    {
        int32_t block = row * Terrain::blocksMapSide + firstBlock;

        for (int32_t col = 0; col < 3; col++, block++)
        {
            char listName[12];
            std::sprintf(listName, "TBlk%d", block);
            collideWithList(this, findObjectList(listName));
            std::sprintf(listName, "RBlk%d", block);
            collideWithList(this, findObjectList(listName));
        }
    }
}

auto Artillery::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    projectToScreen(this, camera);

    if (recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Artillery::setJustCreated() -> void
{
    if (justCreated == 0)
    {
        return;
    }

    justCreated = 0;
    auto* type = static_cast<ArtilleryType*>(objType);

    if (timeToImpact == -1.0f)
    {
        timeToImpact = type->nominalTimeToImpact;
    }

    sensorTime = 0.0f;
    collisionsOn = 0;
    sensorActive = 0;
    sensorRange = type->nominalSensorRange;
    currentFrame = type->startFrame;
    timeToLaunch = type->nominalTimeToLaunch;

    if (sensorRange != 0.0f)
    {
        sensorSystem = sensorSystemManager->newSensor();

        if (alignment == -1)
        {
            setSensorData(clanTeam, -1.0f, -1.0f);
        }
        else if (alignment == 1)
        {
            setSensorData(innerSphereTeam, -1.0f, -1.0f);
        }
    }
}

auto Artillery::recalcBounds(Camera* camera) -> int
{
    const float left = screenPos.x;
    const float top = screenPos.y;
    boundsLeft = left;
    boundsTop = top;
    boundsRight = left;
    boundsBottom = top;
    uint8_t* shape = static_cast<ArtilleryType*>(objType)->shapeData;

    if (shape != nullptr)
    {
        const int32_t minXY = VFX_shape_minxy(shape, static_cast<int32_t>(currentFrame));
        boundsLeft = static_cast<float>(minXY >> 16) + left;
        boundsTop = static_cast<float>(static_cast<int16_t>(minXY)) + top;
        const int32_t size = VFX_shape_resolution(shape, static_cast<int32_t>(currentFrame));
        boundsRight = static_cast<float>(size >> 16) + boundsLeft;
        boundsBottom = static_cast<float>(static_cast<int16_t>(size)) + boundsTop;
    }

    if (0.0f <= boundsRight && 0.0f <= boundsBottom)
    {
        const auto viewRight = static_cast<int16_t>(std::floor(static_cast<double>(camera->viewWidth)));

        if (boundsLeft <= static_cast<float>(viewRight))
        {
            const auto viewBottom = static_cast<int16_t>(std::floor(static_cast<double>(camera->viewHeight)));

            if (boundsTop <= static_cast<float>(viewBottom))
            {
                return 1;
            }
        }
    }

    return 0;
}

auto Artillery::setSensorData(Team* team, float newSensorTime, float newSensorRange) -> void
{
    if (newSensorTime != -1.0f)
    {
        sensorTime = newSensorTime;
    }

    if (newSensorRange != -1.0f)
    {
        sensorRange = newSensorRange;
    }

    sensorSystem->owner = this;
    sensorSystem->setTeam(team);
    // The range passed, even -1, not the one kept.
    sensorSystem->setRange(newSensorRange);
    sensorSystem->scanFrequency = 0.5f;
}

auto Artillery::drawSelectBox(uint8_t /*color*/) -> void
{
}

//---------------------------------------------------------------------------
// CameraDroneType
//---------------------------------------------------------------------------

auto CameraDroneType::createInstance() -> BaseObject*
{
    auto* newDrone = new CameraDrone;

    if (newDrone == nullptr)
    {
        return nullptr;
    }

    if (newDrone->init(this) != 0)
    {
        return nullptr;
    }

    newDrone->idNumber = NextIdNumber++;
    return newDrone;
}

auto CameraDroneType::destroy() -> void
{
}

auto CameraDroneType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile droneFile;
    int32_t result = droneFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = droneFile.seekBlock("General")) != 0)
    {
        return result;
    }

    if ((result = droneFile.readIdFloat("maxVelocity", maxVelocity)) != 0)
    {
        return result;
    }

    if ((result = droneFile.readIdLong("maxDamage", maxDamage)) != 0)
    {
        return result;
    }

    if (droneFile.readIdLong("BRValue", brValue) != 0)
    {
        brValue = 0;
    }

    result = ObjectType::init(&droneFile);
    extentRadius = -1.0f;
    return result;
}

auto CameraDroneType::handleDestruction(GameObject* collidee, GameObject* /*collider*/) -> int
{
    collidee->status = 2;
    return 1;
}

//---------------------------------------------------------------------------
// CameraDrone
//---------------------------------------------------------------------------

CameraDrone::CameraDrone()
{
    frame.reset_to_world_frame();
    init();
}

auto CameraDrone::init() -> void
{
    appearance = nullptr;
    spiralDirection = -1;
    spiralLength = 1;
    targetTileCol = -1;
    targetTileRow = -1;
    launchTime = -1.0f;
    maxVelocity = 0.0f;
    hitPoints = 0;
}

auto CameraDrone::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return -0x2102fffd;
    }

    auto* droneAppearance = new GVAppearance;
    appearance = droneAppearance;

    if (droneAppearance == nullptr)
    {
        return -0x2102fffc;
    }

    droneAppearance->init(nullptr, nullptr);

    if ((apprType->appearanceNum & 0xff000000) != 0x5000000)
    {
        return -0x2323fff7;
    }

    if ((result = droneAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = CAMERADRONE;
    auto* type = static_cast<CameraDroneType*>(objType);
    maxVelocity = type->maxVelocity;
    hitPoints = type->maxDamage;
    curCV = type->brValue;
    maxCV = type->brValue;

    if (0 < type->brValue)
    {
        setPotentialContact(1);
    }

    // The drone starts turned 45 degrees from the world frame.
    frame.reset_to_world_frame();
    rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI / 2.0)), static_cast<float>(std::cos(HALF_PI / 2.0)));
    collisionsOn = 1;
    launchTime = scenarioTime;
    return 0;
}

auto CameraDrone::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
}

auto CameraDrone::update() -> int32_t
{
    const float speed = maxVelocity;

    if (isDestroyed() != 0)
    {
        return 1;
    }

    // Fly straight at the target tile's corner.
    const float step = frameLength * speed * worldUnitsPerMeter;
    const float targetX = static_cast<float>(targetTileCol) * Terrain::metersPerVertex - worldUnitsMapSide * 0.5f;
    const float targetY = worldUnitsMapSide * 0.5f - static_cast<float>(targetTileRow) * Terrain::metersPerVertex;
    float dirX = targetX - position.x;
    float dirY = targetY - position.y;
    float dirZ = 0.0f;
    const float distance = std::sqrt(dirX * dirX + dirY * dirY);

    if (distance != 0.0)
    {
        dirX = dirX / distance;
        dirY = dirY / distance;
        dirZ = 0.0f / distance;
    }

    position.x = dirX * step + position.x;
    position.y = dirY * step + position.y;
    position.z = dirZ * step + position.z;

    // Leaving the map ends the drone (clamped to the edge on its way out).
    const float halfSide = worldUnitsMapSide * 0.5f;
    const float movedX = position.x;

    if (movedX < -halfSide)
    {
        position.x = -halfSide;
    }

    const float clampedX = position.x;

    if (halfSide < clampedX)
    {
        position.x = halfSide;
    }

    const float movedY = position.y;

    if (-halfSide > movedY)
    {
        position.y = -halfSide;
    }

    if (position.y > halfSide)
    {
        position.y = halfSide;
        return 0;
    }

    if (!(-halfSide <= movedY && halfSide >= clampedX && -halfSide <= movedX))
    {
        return 0;
    }

    int32_t tileR = 0;
    int32_t tileC = 0;
    GameMap->worldToMapTilePos(position, tileR, tileC);

    if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
    {
        return 0;
    }

    GameObjectMap->updateObject(this, 0);
    const uint8_t seenBy = alignment == 1 ? 1 : 2;
    frame_of_ref lookFrame = getFrame();
    land->markRadiusSeen(position, lookFrame.j, 360.0f, scenario->maxVisualRange * 0.5f, seenBy);

    // Within a tile of the target: on to the next one.
    if (std::abs(targetTileCol - tileC) > 1)
    {
        return 1;
    }

    if (std::abs(targetTileRow - tileR) > 1)
    {
        return 1;
    }

    findNextTargetTile();
    return 1;
}

auto CameraDrone::render() -> void
{
    const int visibleNow = onScreen();
    auto* droneAppearance = static_cast<GVAppearance*>(appearance);
    droneAppearance->visible = visibleNow;
    droneAppearance->update();

    if (visibleNow != 0)
    {
        windowsVisible = turn;
        droneAppearance->hazePalette = nullptr;
        droneAppearance->render(-150);
    }
}

auto CameraDrone::handleEvent(ObjectEvent* event) -> int32_t
{
    if (event->type == 0)
    {
        switch (event->id)
        {
            case 0x1c:
                selected = 1;
                break;
            case 0x1d:
                selected = 0;
                break;
            case 0x1e:
                unknown2C = 1;
                break;
            case 0x1f:
                unknown2C = 0;
                break;
            default:
                break;
        }
    }

    return 0;
}

auto CameraDrone::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
    {
        return 0;
    }

    projectToScreen(this, camera);

    if (appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto CameraDrone::handleWeaponHit(_WeaponShotInfo* shotInfo, int addMultiplayChunk) -> int32_t
{
    if (shotInfo == nullptr)
    {
        return 0;
    }

    if (addMultiplayChunk != 0)
    {
        MPlayer->addWeaponHitChunk(this, shotInfo, 0);
    }

    if (isDestroyed() == 0 && 0.0f < shotInfo->damage)
    {
        BadGuy = shotInfo->attacker;
        hitPoints = static_cast<int32_t>(static_cast<float>(hitPoints) - shotInfo->damage);

        if (hitPoints < 1)
        {
            objType->handleDestruction(this, nullptr);
            objType->createExplosion(position, 0.0f, 0.0f);
            static_cast<GVAppearance*>(appearance)->setTypeId(GV_ACTOR_STATE_DESTROYED);
        }
    }

    return 0;
}

auto CameraDrone::findNextTargetTile() -> void
{
    rotateAboutK(frame, static_cast<float>(std::sin(HALF_PI)), static_cast<float>(std::cos(HALF_PI)));

    // An outward square spiral: each leg turns a quarter, and every other leg is one tile longer.
    spiralDirection++;

    if (spiralDirection > 3)
    {
        spiralDirection = 0;
    }

    if (spiralDirection % 2 != 0)
    {
        spiralLength++;
    }

    switch (spiralDirection)
    {
        case 0:
        {
            targetTileCol = getObjPosition()->tileC;
            targetTileRow = getObjPosition()->tileR - spiralLength;
            break;
        }
        case 1:
        {
            targetTileCol = getObjPosition()->tileC + spiralLength;
            targetTileRow = getObjPosition()->tileR;
            break;
        }
        case 2:
        {
            targetTileCol = getObjPosition()->tileC;
            targetTileRow = getObjPosition()->tileR + spiralLength;
            break;
        }
        case 3:
        {
            targetTileCol = getObjPosition()->tileC - spiralLength;
            targetTileRow = getObjPosition()->tileR;
            break;
        }
        default:
            break;
    }
}
