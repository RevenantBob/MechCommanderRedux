#include "stdafx.h"
#include "object/explode.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "camera/camlist.h"
#include "gui/asystem.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/collsn.h"
#include "object/gate.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/turret.h"
#include "sound/soundsys.h"
#include "sprite/actor.h"
#include "terrain/terrain.h"

namespace
{
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

    /// <summary>Runs a collision check between the explosion and every object of the list.</summary>
    void collideWithList(Explosion* explosion, ObjectQueueNode* list)
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

                collisionSystem->detectStaticCollision(explosion, other);
            }

            // Port fix (OB-015): the original only steps to the next object after one with a type, so an object
            // without one hangs the game here.
            object = object->next;
        }
    }

    /// <summary>
    /// For turrets and gates: false when the explosion's radius doesn't reach the collider's extent (measured
    /// centre to centre).
    /// </summary>
    bool reachesExtent(GameObject* explosion, GameObject* collider, float extent)
    {
        const vector_3d colliderPos = collider->getPosition();
        const vector_3d explosionPos = explosion->getPosition();
        const double dx = static_cast<double>(colliderPos.x) - explosionPos.x;
        const double dy = static_cast<double>(colliderPos.y) - explosionPos.y;
        const float dz = colliderPos.z - explosionPos.z;
        const auto distance = static_cast<float>(std::sqrt((dx * dx + dy * dy) + static_cast<double>(dz) * dz));
        return !(extent < distance &&
                 static_cast<double>(explosion->getExtentRadius()) < static_cast<double>(distance) - extent);
    }
} // namespace

//---------------------------------------------------------------------------
// ExplosionType
//---------------------------------------------------------------------------

ExplosionType::ExplosionType()
{
    dmgLevel = 0;
    soundEffectId = 0xffffffff;
    explosionRadius = 0;
    damageChunkSize = 0.0f;
}

auto ExplosionType::createInstance() -> BaseObject*
{
    auto* newExplosion = new Explosion;

    if (newExplosion == nullptr)
    {
        return nullptr;
    }

    if (newExplosion->init(this) != 0)
    {
        return nullptr;
    }

    newExplosion->idNumber = NextIdNumber++;
    return newExplosion;
}

auto ExplosionType::destroy() -> void
{
}

auto ExplosionType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile explFile;
    int32_t result = explFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = explFile.seekBlock("ExplosionData")) != 0)
    {
        return result;
    }

    if ((result = explFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    if ((result = explFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
    {
        return result;
    }

    if (explFile.readIdLong("ExplosionRadius", explosionRadius) != 0)
    {
        explosionRadius = 0;
    }

    if (explFile.readIdULong("LightObjectId", lightObjectId) != 0)
    {
        lightObjectId = 0xffffffff;
    }

    if (explFile.readIdFloat("DamageChunkSize", damageChunkSize) != 0)
    {
        damageChunkSize = 5.0f;
    }

    return ObjectType::init(&explFile);
}

auto ExplosionType::handleCollision(GameObject* collidee, GameObject* collider) -> int
{
    // Only the server deals explosion damage in multiplayer.
    if (MPlayer != nullptr && MPlayer->isServer == 0)
    {
        return 0;
    }

    const float damage = collidee->getExplDmg();

    if (damage == 0.0f)
    {
        return 0;
    }

    const float chunk = damageChunkSize < damage ? damageChunkSize : damage;
    const int multiplayer = MPlayer != nullptr ? 1 : 0;
    _WeaponShotInfo shot;

    switch (collider->objectClass)
    {
        case BATTLEMECH:
        case GROUNDVEHICLE:
        case ELEMENTAL:
        case MOVER:
        {
            // Movers take the damage in chunks, each on a location of its own.
            shot.init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = damage; 0.0f < remaining; remaining -= damageChunkSize)
            {
                shot.hitLocation = collider->calcHitLocation(collidee, -1, 4, 0);
                collider->handleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        case TURRET:
        {
            if (!reachesExtent(collidee, collider, static_cast<TurretType*>(collider->objType)->littleExtent))
            {
                return 0;
            }

            float remaining = collidee->getExplDmg();
            shot.init(nullptr, -1, chunk, 0, 0.0f);

            for (; 0.0f < remaining; remaining -= damageChunkSize)
            {
                shot.hitLocation = 0;
                collider->handleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        case GATE:
        {
            if (!reachesExtent(collidee, collider, static_cast<GateType*>(collider->objType)->littleExtent))
            {
                return 0;
            }

            shot.init(nullptr, -1, chunk, 0, 0.0f);

            for (float remaining = damage; 0.0f < remaining; remaining -= damageChunkSize)
            {
                shot.hitLocation = 0;
                collider->handleWeaponHit(&shot, multiplayer);
            }

            return 0;
        }

        default:
        {
            // Anything else takes it all at once.
            shot.init(nullptr, -1, collidee->getExplDmg(), 0, 0.0f);
            collider->handleWeaponHit(&shot, multiplayer);
            return 0;
        }
    }
}

auto ExplosionType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Explosion
//---------------------------------------------------------------------------

auto Explosion::init() -> void
{
    justCreated = 1;
    appearance = nullptr;
    collisionChecked = 0;
    timeAlive = 0.0f;
    damageChunkSize = 0.0f;
    light = nullptr;
}

auto Explosion::getExtentRadius() -> float
{
    return explRadius;
}

auto Explosion::setExtentRadius(float newRadius) -> void
{
    explRadius = newRadius;
}

auto Explosion::onScreen() -> int
{
    Camera* camera = cameraList->findCameraFromIDNumber(1);

    if (camera == nullptr || camera->active == 0)
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

    if (appearance != nullptr && appearance->recalcBounds(camera) != 0)
    {
        windowsVisible = turn;
        return 1;
    }

    return 0;
}

auto Explosion::handleStaticCollision() -> void
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

auto Explosion::update() -> int32_t
{
    const int visibleNow = onScreen();

    if (justCreated != 0)
    {
        justCreated = 0;
        collisionsOn = 0;
        const uint32_t soundId = static_cast<ExplosionType*>(objType)->soundEffectId;

        if (soundId != 0xffffffff)
        {
            soundSystem->playDigitalSample(soundId, 1, this, 0, 0);
        }
    }

    // Collisions are on for the one frame after the explosion is half a second old.
    if (collisionChecked != 0)
    {
        collisionsOn = 0;
    }

    const double aliveSum = static_cast<double>(frameLength) + timeAlive;
    timeAlive = static_cast<float>(aliveSum);

    if (0.5 < aliveSum && collisionChecked == 0)
    {
        collisionChecked = 1;
        collisionsOn = 1;
    }

    if (light != nullptr)
    {
        vector_3d lightPos = position;
        light->setPosition(lightPos);
        light->update();
    }

    appearance->visible = visibleNow;
    return appearance->update();
}

auto Explosion::render() -> void
{
    if (gamePaused != 0)
    {
        onScreen();
    }

    if (justCreated == 0 && windowsVisible == turn)
    {
        appearance->render(-150);
    }

    if (light != nullptr)
    {
        light->render();
    }
}

auto Explosion::destroy() -> void
{
    delete appearance;
    appearance = nullptr;
    delete light;
    light = nullptr;
}

auto Explosion::init(ObjectType* objType) -> int32_t
{
    int32_t result = GameObject::init(objType);

    if (result != 0)
    {
        return result;
    }

    justCreated = 1;
    collisionsOn = 0;
    collisionChecked = 0;
    timeAlive = 0.0f;
    AppearanceType* apprType = appearanceTypeList->getAppearance(objType->appearName, 0);

    if (apprType == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0003);
    }

    if ((apprType->appearanceNum & 0xff000000) != 0x2000000)
    {
        return static_cast<int32_t>(0xdcdc0005);
    }

    auto* vfxAppearance = new VFXAppearance;
    appearance = vfxAppearance;

    if (vfxAppearance == nullptr)
    {
        return static_cast<int32_t>(0xdcdc0004);
    }

    vfxAppearance->init(nullptr, nullptr);

    if ((result = vfxAppearance->init(apprType, this)) != 0)
    {
        return result;
    }

    objectClass = EXPLOSION;
    const auto* explType = static_cast<ExplosionType*>(objType);

    if (explType->explosionRadius != 0)
    {
        setExtentRadius(static_cast<float>(explType->explosionRadius));
        setExplDmg(static_cast<float>(explType->dmgLevel));
    }

    if (static_cast<int32_t>(explType->lightObjectId) != -1)
    {
        light = createObject(static_cast<int32_t>(explType->lightObjectId));
    }

    damageChunkSize = explType->damageChunkSize;
    return 0;
}

void CreateExplosion(int32_t objectTypeId, vector_3d& position, float damage, float radius)
{
    if (objectTypeId == -1)
    {
        return;
    }

    GameObject* explosion = createObject(objectTypeId);

    if (explosion == nullptr)
    {
        return;
    }

    explosion->setPosition(position);

    if (radius != 0.0f)
    {
        explosion->setExtentRadius(radius);
        explosion->setExplDmg(damage);
    }

    if (objectList->head != nullptr)
    {
        objectList->head->addNode(explosion);
    }
}
