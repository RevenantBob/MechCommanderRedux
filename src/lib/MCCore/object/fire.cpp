#include "stdafx.h"
#include "object/fire.h"
#include "ai/move.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/collsn.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/tree.h"
#include "sound/soundsys.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Terrain objects of class 6 in a MiscTerrainObject are forests: a fire there damages them.</summary>
    constexpr int32_t FOREST_TERRAIN_OBJECT = 6;

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

    /// <summary>Runs a collision check between the fire and every object of the list.</summary>
    void collideWithList(Fire* fire, ObjectQueueNode* list)
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

                collisionSystem->detectStaticCollision(fire, other);
            }

            // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
            // without one hangs the game here.
            object = object->next;
        }
    }

    /// <summary>A random offset of up to <paramref name="range"/>, added or (on a coin flip) taken away.</summary>
    float scatter(float base, int32_t range)
    {
        const auto offset = static_cast<float>(RandomNumber(range));

        if (RollDice(50) == 0)
        {
            return base - offset;
        }

        return offset + base;
    }
} // namespace

Fire** Fire::maxFiresList = nullptr;
float maxFireBurnTime = 5.0f;
int32_t maxFiresBurning = 0;
int32_t currentFireIndex = 0;

//---------------------------------------------------------------------------
// FireType
//---------------------------------------------------------------------------

auto FireType::init() -> void
{
    ObjectType::init();
    dmgLevel = 0;
    soundEffectId = 0xffffffff;
    timeToMaxExtent = 0.0f;
    maxExtentRadius = 0.0f;
    totalFireShapes = 1;
    fireOffsetX = nullptr;
    fireOffsetY = nullptr;
    fireDelay = nullptr;
    fireRandomOffsetX = nullptr;
    fireRandomOffsetY = nullptr;
    fireRandomDelay = nullptr;
}

auto FireType::createInstance() -> BaseObject*
{
    auto* newFire = new Fire;

    if (newFire == nullptr)
    {
        return nullptr;
    }

    if (newFire->init(this) != 0)
    {
        return nullptr;
    }

    newFire->idNumber = NextIdNumber++;
    return newFire;
}

auto FireType::destroy() -> void
{
    systemHeap->free(fireOffsetX);
    systemHeap->free(fireOffsetY);
    systemHeap->free(fireDelay);
    fireOffsetX = nullptr;
    fireOffsetY = nullptr;
    fireDelay = nullptr;
    systemHeap->free(fireRandomOffsetX);
    systemHeap->free(fireRandomOffsetY);
    systemHeap->free(fireRandomDelay);
    fireRandomOffsetX = nullptr;
    fireRandomOffsetY = nullptr;
    fireRandomDelay = nullptr;
}

auto FireType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile fireFile;
    int32_t result = fireFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = fireFile.seekBlock("FireData")) != 0)
    {
        return result;
    }

    if ((result = fireFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    if ((result = fireFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
    {
        return result;
    }

    if (fireFile.readIdULong("LightObjectId", lightObjectId) != 0)
    {
        lightObjectId = 0xffffffff;
    }

    if ((result = fireFile.readIdULong("startLoopFrame", startLoopFrame)) != 0)
    {
        return result;
    }

    if ((result = fireFile.readIdULong("numLoops", numLoops)) != 0)
    {
        return result;
    }

    if ((result = fireFile.readIdULong("endLoopFrame", endLoopFrame)) != 0)
    {
        return result;
    }

    if (fireFile.readIdFloat("maxExtentRadius", maxExtentRadius) != 0)
    {
        maxExtentRadius = 0.0f;
    }

    if (fireFile.readIdFloat("TimeToMaxExtent", timeToMaxExtent) != 0)
    {
        timeToMaxExtent = 0.0f;
    }

    if (fireFile.readIdLong("TotalFireShapes", totalFireShapes) != 0)
    {
        totalFireShapes = 1;
    }

    const int32_t numShapes = totalFireShapes;
    const uint32_t size = static_cast<uint32_t>(numShapes) * 4;
    fireOffsetX = static_cast<float*>(systemHeap->malloc(size));
    fireOffsetY = static_cast<float*>(systemHeap->malloc(size));
    fireDelay = static_cast<float*>(systemHeap->malloc(size));
    fireRandomOffsetX = static_cast<int32_t*>(systemHeap->malloc(size));
    fireRandomOffsetY = static_cast<int32_t*>(systemHeap->malloc(size));
    fireRandomDelay = static_cast<int32_t*>(systemHeap->malloc(size));

    for (int32_t i = 0; i < numShapes; i++)
    {
        char offsetXName[50];
        char offsetYName[50];
        char delayName[50];
        char randomOffsetXName[50];
        char randomOffsetYName[50];
        char randomDelayName[50];
        std::sprintf(offsetXName, "FireOffsetX%d", i);
        std::sprintf(offsetYName, "FireOffsetY%d", i);
        std::sprintf(delayName, "FireDelay%d", i);
        std::sprintf(randomOffsetXName, "FireRandomOffsetX%d", i);
        std::sprintf(randomOffsetYName, "FireRandomOffsetY%d", i);
        std::sprintf(randomDelayName, "FireRandomDelay%d", i);

        if (fireFile.readIdFloat(offsetXName, fireOffsetX[i]) != 0)
        {
            fireOffsetX[i] = 0.0f;
        }

        if (fireFile.readIdFloat(offsetYName, fireOffsetY[i]) != 0)
        {
            fireOffsetY[i] = 0.0f;
        }

        if (fireFile.readIdFloat(delayName, fireDelay[i]) != 0)
        {
            fireDelay[i] = 0.0f;
        }

        if (fireFile.readIdLong(randomOffsetXName, fireRandomOffsetX[i]) != 0)
        {
            fireRandomOffsetX[i] = 0;
        }

        if (fireFile.readIdLong(randomOffsetYName, fireRandomOffsetY[i]) != 0)
        {
            fireRandomOffsetY[i] = 0;
        }

        if (fireFile.readIdLong(randomDelayName, fireRandomDelay[i]) != 0)
        {
            fireRandomDelay[i] = 0;
        }
    }

    return ObjectType::init(&fireFile);
}

auto FireType::handleCollision(GameObject*, GameObject* collider) -> int
{
    // The fire spreads (one chance in ten per collision) to what it touches; the server's job in multiplayer.
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    if (collider->isDestroyed() != 0)
    {
        return 0;
    }

    switch (collider->objectClass)
    {
        case BUILDING:
        {
            if (RollDice(10) != 0)
            {
                const float timeToBurn =
                    10.0f / static_cast<BuildingType*>(collider->getObjectType())->timeToBurnDamage;
                static_cast<Building*>(collider)->lightOnFire(timeToBurn);

                if (MPlayer != nullptr)
                {
                    MPlayer->addLightOnFireChunk(collider, static_cast<int32_t>(timeToBurn));
                }
            }
            break;
        }
        case TREE:
        {
            if (RollDice(10) != 0)
            {
                static_cast<Tree*>(collider)->lightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->addLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        case MISCTERRAINOBJECT:
        {
            if (RollDice(10) != 0)
            {
                static_cast<MiscTerrainObject*>(collider)->lightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->addLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        case TREEBUILDING:
        {
            if (RollDice(10) != 0)
            {
                static_cast<TreeBuilding*>(collider)->lightOnFire(15.0f);

                if (MPlayer != nullptr)
                {
                    MPlayer->addLightOnFireChunk(collider, 15);
                }
            }
            break;
        }
        default:
            break;
    }

    return 0;
}

auto FireType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Fire
//---------------------------------------------------------------------------

auto Fire::init() -> void
{
    justCreated = 0;
    appearances = nullptr;
    extentRadius = 0.0f;
    burningOut = 0;
    burningObject = nullptr;
    shapeOffsets = nullptr;
    startDelays = nullptr;
    loopsLeft = nullptr;
    timeLeftToBurn = nullptr;
    light = nullptr;
    lastVisibleTurn = 0;
}

auto Fire::handleStaticCollision() -> void
{
    if (collisionsOn == 0)
    {
        return;
    }

    int32_t blockNumber = 0;
    int32_t vertexNumber = 0;
    getBlockAndVertexNumber(blockNumber, vertexNumber);

    // The terrain objects of the 3x3 terrain blocks around it.
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

auto Fire::isVisible(int32_t shapeIndex) -> int
{
    int onScreenNow = 0;
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera != nullptr && camera->active != 0)
    {
        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            vector_3d shapePos;
            shapePos.x = position.x + shapeOffsets[shapeIndex].x;
            shapePos.y = position.y + shapeOffsets[shapeIndex].y;
            shapePos.z = position.z + shapeOffsets[shapeIndex].z;
            land->projectTerrain(shapePos, screen100, screen50);
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
        Appearance* shapeAppearance = appearances[shapeIndex];

        if (shapeAppearance != nullptr)
        {
            onScreenNow = shapeAppearance->recalcBounds(camera);

            if (onScreenNow != 0)
            {
                lastVisibleTurn = turn;
            }
        }
    }

    // In multiplayer fires always count as visible.
    if (MPlayer == nullptr && onScreenNow == 0)
    {
        return 0;
    }

    windowsVisible = turn;
    return 1;
}

auto Fire::finishFireNow() -> void
{
    const int32_t numShapes = static_cast<FireType*>(objType)->totalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        loopsLeft[i] = 2;
    }

    burningOut = 1;

    for (int32_t i = 0; i < numShapes; i++)
    {
        startDelays[i] = 0.0f;
    }

    for (int32_t i = 0; i < numShapes; i++)
    {
        timeLeftToBurn[i] = 0.0f;
    }
}

auto Fire::addTimeLeftToBurn(float extraTime) -> void
{
    const int32_t numShapes = static_cast<FireType*>(objType)->totalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        if (extraTime + timeLeftToBurn[i] < maxFireBurnTime)
        {
            timeLeftToBurn[i] = extraTime + timeLeftToBurn[i];
        }
    }
}

auto Fire::isRevealed() -> int
{
    int32_t tileR = 0;
    int32_t tileC = 0;
    int32_t cellR = 0;
    int32_t cellC = 0;
    GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);
    ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
    // Faithful: tile coordinates are looked up in the vertex-resolution visibility bits.
    const auto row = static_cast<uint32_t>(tileR);
    const auto col = static_cast<uint32_t>(tileC);
    int revealed = visibleBits->getFlag(row, col) != 0 ? 1 : 0;

    if (visibleBits->getFlag(row + 1, col) != 0)
    {
        revealed = 1;
    }

    if (visibleBits->getFlag(row + 1, col + 1) != 0)
    {
        revealed = 1;
    }

    if (visibleBits->getFlag(row, col + 1) != 0)
    {
        revealed = 1;
    }

    return revealed;
}

auto Fire::update() -> int32_t
{
    if (justCreated != 0)
    {
        // Make sure the fire is in the object lists: if no list holds it, append it to the first.
        justCreated = 0;
        setPotentialContact(3);
        BaseObject* current = nullptr;

        do
        {
            objectList->traverse(current);

            if (current == nullptr)
            {
                if (objectList->head != nullptr)
                {
                    objectList->head->addNode(this);
                }
                break;
            }
        } while (current != this);
    }

    if (burningOut != 0)
    {
        collisionsOn = 0;
    }

    // Each shape waits out its start delay, then burns until its time runs out; a shape burnt out while still
    // looping sets the fire burning out, and collisions on for a frame at the full extent.
    const auto* fireType = static_cast<FireType*>(objType);

    for (int32_t i = 0; i < fireType->totalFireShapes; i++)
    {
        if (startDelays[i] <= 0.0f)
        {
            if (loopsLeft[i] != 0)
            {
                const float timeLeft = timeLeftToBurn[i] - frameLength;
                timeLeftToBurn[i] = timeLeft;

                if (0.0f < timeLeft || static_cast<uint32_t>(loopsLeft[i]) < 3 ||
                    static_cast<VFXAppearance*>(appearances[i])->currentState == ACTOR_STATE_DAMAGED)
                {
                    if (0.0f < timeLeft)
                    {
                        loopsLeft[i] = 999;
                    }
                }
                else
                {
                    collisionsOn = 1;
                    extentRadius = fireType->maxExtentRadius;
                    burningOut = 1;
                    loopsLeft[i] = 2;
                }
            }
        }
        else
        {
            startDelays[i] -= frameLength;
        }
    }

    // Done once burning out and (if anyone can see it) every shape has finished.
    bool done = false;

    if (burningOut != 0)
    {
        done = true;

        if (isRevealed() != 0 || MPlayer != nullptr)
        {
            for (int32_t i = 0; i < fireType->totalFireShapes; i++)
            {
                if (loopsLeft[i] != 0)
                {
                    done = false;
                }
            }
        }
    }

    if (light != nullptr)
    {
        vector_3d lightPos = position;
        light->setPosition(lightPos);
        light->update();
    }

    if (!done)
    {
        return 1;
    }

    // Put the burning object out; a burnt forest takes its damage.
    if (burningObject != nullptr)
    {
        if (burningObject->objectClass == MISCTERRAINOBJECT &&
            static_cast<MiscTerrainObject*>(burningObject)->terrainObjectKind == FOREST_TERRAIN_OBJECT)
        {
            const auto* forestType = static_cast<MiscTerrainObjectType*>(burningObject->getObjectType());
            _WeaponShotInfo shot;
            shot.init(nullptr, -3, static_cast<float>(static_cast<int32_t>(forestType->forestDmgLevel)), 0, 0.0f);
            burningObject->handleWeaponHit(&shot, MPlayer != nullptr ? 1 : 0);
        }

        burningObject->killFireObject();
    }

    for (int32_t i = 0; i < maxFiresBurning; i++)
    {
        if (maxFiresList[i] == this)
        {
            maxFiresList[i] = nullptr;
        }
    }

    return 0;
}

auto Fire::render() -> void
{
    int tagged = 0;
    const int32_t contactType = getContactType(homeTeam->id, tagged);
    const int revealed = burningObject != nullptr ? burningObject->isRevealed() : isRevealed();
    const auto* fireType = static_cast<FireType*>(objType);

    if (revealed != 0 || MPlayer != nullptr)
    {
        // Each burning shape plays its start (0), loop (1) and end (2) animations in turn.
        for (int32_t i = 0; i < fireType->totalFireShapes; i++)
        {
            int finished = 0;

            if (loopsLeft[i] == 0)
            {
                continue;
            }

            const int visibleNow = isVisible(i);
            auto* shapeAppearance = static_cast<VFXAppearance*>(appearances[i]);
            shapeAppearance->visible = visibleNow;

            if (0.0f < startDelays[i])
            {
                continue;
            }

            if (shapeAppearance->update() == 0)
            {
                if (shapeAppearance->currentState == ACTOR_STATE_DAMAGED)
                {
                    finished = 1;
                    loopsLeft[i] = 0;
                }
                else
                {
                    if (shapeAppearance->currentState == ACTOR_STATE_NORMAL)
                    {
                        shapeAppearance->setTypeId(ACTOR_STATE_BLOWING_UP1, 0xff);
                        shapeAppearance->update();
                    }

                    loopsLeft[i]--;

                    if (loopsLeft[i] == 1)
                    {
                        auto* endAppearance = static_cast<VFXAppearance*>(appearances[i]);
                        endAppearance->setTypeId(ACTOR_STATE_DAMAGED, 0xff);
                        endAppearance->update();
                    }
                }
            }

            if (justCreated == 0 && finished == 0 && isRevealed() != 0 && lastVisibleTurn == turn)
            {
                somethingOnFire = 1;
                appearances[i]->render(-150);
            }
        }

        if (light != nullptr)
        {
            light->render();
        }

        return;
    }

    // Unseen: a sensor contact shows a blip sized by tonnage.
    if (contactType != 2)
    {
        return;
    }

    if (isVisible(0) == 0)
    {
        return;
    }

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

    if (shape == nullptr)
    {
        return;
    }

    if (VFX_shape_count(shape) <= blipFrame)
    {
        if (soundSystem != nullptr && useSound != 0)
        {
            soundSystem->playDigitalSample(0x14, 1, this, 0, 1);
        }

        blipFrame = 0;
    }

    ElementList->openGroup(-100000, 1);
    ElementList->add(new VFXElement(shape, screenPos.x, screenPos.y, blipFrame, 0, nullptr, 0, 0));
    blipTime = frameLength + blipTime;

    if (0.067 < blipTime)
    {
        blipFrame = static_cast<int32_t>(blipTime * (1.0 / 0.067) + blipFrame + 0.5);
        blipTime = 0.0f;
    }
}

auto Fire::destroy() -> void
{
    setPotentialContact(0);
    const int32_t numShapes = static_cast<FireType*>(objType)->totalFireShapes;

    for (int32_t i = 0; i < numShapes; i++)
    {
        delete appearances[i];
        appearances[i] = nullptr;
    }

    systemHeap->free(appearances);
    appearances = nullptr;
    systemHeap->free(shapeOffsets);
    shapeOffsets = nullptr;
    systemHeap->free(startDelays);
    startDelays = nullptr;
    systemHeap->free(loopsLeft);
    loopsLeft = nullptr;
    systemHeap->free(timeLeftToBurn);
    timeLeftToBurn = nullptr;
    delete light;
    light = nullptr;
}

auto Fire::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    const auto* fireType = static_cast<FireType*>(objType);
    const int32_t numShapes = fireType->totalFireShapes;
    justCreated = 1;
    appearances = static_cast<Appearance**>(systemHeap->malloc(numShapes * sizeof(Appearance*)));
    shapeOffsets = static_cast<vector_3d*>(systemHeap->malloc(numShapes * sizeof(vector_3d)));
    startDelays = static_cast<float*>(systemHeap->malloc(numShapes * sizeof(float)));
    loopsLeft = static_cast<int32_t*>(systemHeap->malloc(numShapes * sizeof(int32_t)));
    timeLeftToBurn = static_cast<float*>(systemHeap->malloc(numShapes * sizeof(float)));
    const uint32_t appearId = objType->appearName;

    for (int32_t i = 0; i < numShapes; i++)
    {
        vector_3d& offset = shapeOffsets[i];
        offset.z = 0.0f;
        offset.y = 0.0f;
        offset.x = 0.0f;
        startDelays[i] = 0.0f;
        loopsLeft[i] = static_cast<int32_t>(static_cast<FireType*>(this->objType)->numLoops);
        timeLeftToBurn[i] = maxFireBurnTime;
        AppearanceType* apprType = appearanceTypeList->getAppearance(appearId, 0);

        if (apprType == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0003);
        }

        appearanceClass = apprType->appearanceNum >> 24;

        if (appearanceClass != 2)
        {
            return static_cast<int32_t>(0xdcdc0005);
        }

        auto* vfxAppearance = new VFXAppearance;
        appearances[i] = vfxAppearance;

        if (vfxAppearance == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0004);
        }

        vfxAppearance->init(nullptr, nullptr);

        if ((result = vfxAppearance->init(apprType, this)) != 0)
        {
            return result;
        }

        static_cast<VFXAppearance*>(appearances[i])->setTypeId(ACTOR_STATE_NORMAL, 0xff);

        // Place the shape at its offset, scattered, and delay its start (tenths of a second).
        offset.x = fireType->fireOffsetX[i] + offset.x;
        offset.y = fireType->fireOffsetY[i] + offset.y;
        offset.x = scatter(offset.x, fireType->fireRandomOffsetX[i]);
        offset.y = scatter(offset.y, fireType->fireRandomOffsetY[i]);
        const float delay = fireType->fireDelay[i] + startDelays[i];
        startDelays[i] = delay;
        startDelays[i] = (static_cast<float>(RandomNumber(fireType->fireRandomDelay[i])) + delay) * 0.1f;
    }

    objectClass = FIRE;
    collisionsOn = 0;
    burningOut = 0;
    blipFrame = 0;

    // Fires share a ring of maxFiresBurning slots; taking a slot finishes the fire that held it.
    if (maxFiresList == nullptr)
    {
        maxFiresList = static_cast<Fire**>(systemHeap->malloc(maxFiresBurning * sizeof(Fire*)));

        for (int32_t i = 0; i < maxFiresBurning; i++)
        {
            maxFiresList[i] = nullptr;
        }
    }

    currentFireIndex++;

    if (currentFireIndex == maxFiresBurning)
    {
        currentFireIndex = 0;
    }

    if (maxFiresList[currentFireIndex] != nullptr)
    {
        maxFiresList[currentFireIndex]->finishFireNow();
    }

    maxFiresList[currentFireIndex] = this;

    if (static_cast<int32_t>(fireType->lightObjectId) != -1)
    {
        light = createObject(static_cast<int32_t>(fireType->lightObjectId));
    }

    return 0;
}
