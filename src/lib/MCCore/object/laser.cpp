#include "stdafx.h"
#include "object/laser.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/cepoly.h"
#include "engine/crater.h"
#include "lib/cident.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "platform/MCRenderer.h"
#include "lib/inifile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "object/object.h"
#include "object/objque.h"
#include "sound/soundsys.h"
#include "terrain/terrain.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Radians to degrees, as MCX.EXE stores it.</summary>
    constexpr double RADIANS_TO_DEGREES = 0x1.ca5dc1a6402aap+5;

    /// <summary>
    /// Sets the beam's duration and colours from stage <paramref name="stage"/> of the friendly set, or of the enemy
    /// set (the second half of the arrays) when the shooter is missing or not on side 1. Past the last stage the
    /// duration is -1 and the colours 0.
    /// </summary>
    void setStage(Laser* laser, const LaserType* type, uint32_t stage)
    {
        auto* shooter = static_cast<GameObject*>(laser->source.object);
        const uint32_t numStages = type->numStages;
        const uint32_t base = (shooter == nullptr || shooter->getAlignment() != 1) ? numStages : 0;
        laser->stageTimeLeft = stage < numStages ? type->stageDuration[base + stage] : -1.0f;
        laser->hotColor = stage < numStages ? type->stageHot[base + stage] : 0;
        laser->coolColor = stage < numStages ? type->stageCool[base + stage] : 0;
    }

    /// <summary>Applies the laser's shot to its target (the server's job in multiplayer) and marks it applied.</summary>
    void applyShot(Laser* laser)
    {
        auto* victim = static_cast<GameObject*>(laser->target.object);

        if (MPlayer == nullptr)
        {
            victim->handleWeaponHit(&laser->shotInfo, 0);
        }
        else if (MPlayer->isServer != 0)
        {
            victim->handleWeaponHit(&laser->shotInfo, 1);
        }

        laser->damageApplied = 1;
    }

    /// <summary>
    /// Pulls the beam's end in from the target to <paramref name="fraction"/> of the way from
    /// <paramref name="start"/>, so it grows out over its first stage (or PPC frame).
    /// </summary>
    void extendBeam(vector_3d& end, const vector_3d& start, float fraction)
    {
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float dz = end.z - start.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (length != 0.0f)
        {
            dx /= length;
            dy /= length;
            dz /= length;
        }

        const float reach = fraction * length;
        end.x = dx * reach + start.x;
        end.y = start.y + dy * reach;
        end.z = start.z + dz * reach;
    }

    /// <summary>Creates the hit (or, without a target, the miss) effect at the beam's end; a miss leaves a crater.</summary>
    void createHitEffect(Laser* laser, const LaserType* type, GameObject* victim)
    {
        laser->hitEffectCreated = 1;
        GameObject* effect =
            createObject(static_cast<int32_t>(victim == nullptr ? type->laserMissEffect : type->laserHitEffect));

        if (effect == nullptr)
        {
            return;
        }

        if (laser->targetPosition != nullptr)
        {
            effect->setPosition(*laser->targetPosition);
        }

        if (objectList->head != nullptr)
        {
            objectList->head->addNode(effect);
        }

        if (victim == nullptr && laser->targetPosition != nullptr)
        {
            craterManager->addCrater(7, *laser->targetPosition, 1);
        }
    }

    /// <summary>A screen vertex (colour, u and v as given; w zero).</summary>
    SCRNVERTEX screenVertex(int32_t x, int32_t y, FIXED16 c, FIXED16 u, FIXED16 v)
    {
        SCRNVERTEX vertex;
        vertex.x = x;
        vertex.y = y;
        vertex.c = c;
        vertex.u = u;
        vertex.v = v;
        vertex.w = 0;
        return vertex;
    }
} // namespace

uint8_t* laserEffectBuffer = nullptr;
_pane* laserPane = nullptr;
_window* laserWindow = nullptr;

namespace
{
    /// <summary>
    /// The texture the PPC beam's polygons map: laserEffectBuffer as they read it (laserWindow's x_max bytes a row,
    /// one fewer than the window draws it with).
    /// </summary>
    MCTexture* laserTexture = nullptr;
}

//---------------------------------------------------------------------------
// LaserType
//---------------------------------------------------------------------------

auto LaserType::init() -> void
{
    ObjectType::init();
    stageDuration = nullptr;
    stageHot = nullptr;
    stageCool = nullptr;
    dmgLevel = 0;
    numStages = 0;
    pixelWidth = 0;
    laserHitEffect = 0xffffffff;
    laserMissEffect = 0xffffffff;
    laserEffectShape = nullptr;
    hitPPC = 0;
    numPPCFrames = 0;
    bPPC = 0;
    rPPC = 0;
    tPPC = 0;
    lPPC = 0;
    lengthPPC = 0.0f;
    animPPC = 0.0f;
}

auto LaserType::createInstance() -> BaseObject*
{
    auto* newLaser = new Laser;

    if (newLaser == nullptr)
    {
        return nullptr;
    }

    if (newLaser->init(this) != 0)
    {
        return nullptr;
    }

    newLaser->idNumber = NextIdNumber++;
    return newLaser;
}

auto LaserType::destroy() -> void
{
    UserHeap* cache = ObjectTypeManager::objectTypeCache;

    if (cache == nullptr || cache->heapSize == 0)
    {
        return;
    }

    cache->free(stageDuration);
    stageDuration = nullptr;
    cache->free(stageCool);
    stageCool = nullptr;
    cache->free(stageHot);
    stageHot = nullptr;
}

auto LaserType::init(File* objFile, uint32_t fileSize) -> int32_t
{
    FitIniFile laserFile;
    int32_t result = laserFile.open(objFile, fileSize, 50);

    if (result != 0)
    {
        return result;
    }

    if ((result = laserFile.seekBlock("LaserData")) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdUChar("PixelWidth", pixelWidth)) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdUChar("NumStages", numStages)) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdULong("DmgLevel", dmgLevel)) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdULong("SoundEffectId", soundEffectId)) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdULong("LaserHitEffect", laserHitEffect)) != 0)
    {
        return result;
    }

    if ((result = laserFile.readIdULong("LaserMissEffect", laserMissEffect)) != 0)
    {
        return result;
    }

    // A PPC: the effect shape and its frame data.
    char shapeName[80];

    if (laserFile.readIdString("LaserEffectShape", shapeName, 79) == 0)
    {
        FullPathFileName shapePath;
        shapePath.init(spritePath, shapeName, ".shp");
        File shapeFile;

        if ((result = shapeFile.open(shapePath, READ, 50)) != 0)
        {
            return result;
        }

        const uint32_t size = shapeFile.fileSize();
        laserEffectShape = static_cast<uint8_t*>(ObjectTypeManager::objectTypeCache->malloc(size));

        if (laserEffectShape == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0001);
        }

        shapeFile.read(laserEffectShape, static_cast<int32_t>(size));
        MCRenderer::RegisterData(laserEffectShape, size, MCDataKind::Shapes);
        shapeFile.close();

        if ((result = laserFile.seekBlock("PPCData")) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("numPPCFrames", numPPCFrames)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("tPPC", tPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("lPPC", lPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("bPPC", bPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("rPPC", rPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdULong("hitPPC", hitPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("lengthPPC", lengthPPC)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("animPPC", animPPC)) != 0)
        {
            return result;
        }
    }
    else
    {
        laserEffectShape = nullptr;
    }

    // The stage arrays: numStages friendly stages, then numStages enemy ones.
    UserHeap* cache = ObjectTypeManager::objectTypeCache;

    if (cache != nullptr && cache->heapSize != 0)
    {
        const uint32_t count = numStages;
        stageDuration = static_cast<float*>(cache->malloc(count * 2 * sizeof(float)));

        if (stageDuration == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0001);
        }

        stageCool = static_cast<uint8_t*>(cache->malloc(count * 2 * sizeof(uint8_t)));

        if (stageCool == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0001);
        }

        stageHot = static_cast<uint8_t*>(cache->malloc(count * 2 * sizeof(uint8_t)));

        if (stageHot == nullptr)
        {
            return static_cast<int32_t>(0xdcdc0001);
        }
    }

    const int32_t count = numStages;
    char blockName[20];

    for (int32_t i = 0; i < count; i++)
    {
        std::sprintf(blockName, "FLaser%d", i);

        if ((result = laserFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("StageDuration", stageDuration[i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("StageCool", stageCool[i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("StageHot", stageHot[i])) != 0)
        {
            return result;
        }
    }

    for (int32_t i = 0; i < count; i++)
    {
        std::sprintf(blockName, "ELaser%d", i);

        if ((result = laserFile.seekBlock(blockName)) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdFloat("StageDuration", stageDuration[count + i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("StageCool", stageCool[count + i])) != 0)
        {
            return result;
        }

        if ((result = laserFile.readIdUChar("StageHot", stageHot[count + i])) != 0)
        {
            return result;
        }
    }

    result = ObjectType::init(&laserFile);
    objectTypeManager->load(static_cast<int32_t>(laserHitEffect), 1);
    objectTypeManager->load(static_cast<int32_t>(laserMissEffect), 1);
    return result;
}

auto LaserType::handleCollision(GameObject*, GameObject*) -> int
{
    return 0;
}

auto LaserType::handleDestruction(GameObject*, GameObject*) -> int
{
    return 0;
}

//---------------------------------------------------------------------------
// Laser
//---------------------------------------------------------------------------

auto Laser::init() -> void
{
    GameObject::init();
    currentStage = 0xff;
    stageTimeLeft = 0.0f;
    coolColor = 0;
    hotColor = 0;
    source.setWatcher(nullptr);
    target.setWatcher(nullptr);
    shotInfo.damage = 0.0f;
    targetHotSpot = 0;
    sourceHotSpot = 0;
    targetPosition = nullptr;
    hitEffectCreated = 0;
    damageApplied = 0;
    shotInfo.masterId = -1;
    shotInfo.hitLocation = -1;
    justCreated = 1;
}

auto Laser::init(ObjectType* objType) -> int32_t
{
    init();
    GameObject::init(objType);
    currentStage = 0xff;
    objectClass = LASER;
    justCreated = 1;
    return 0;
}

auto Laser::destroy() -> void
{
    if (targetPosition != nullptr)
    {
        delete targetPosition;
        targetPosition = nullptr;
    }
}

auto Laser::update() -> int32_t
{
    const auto* type = static_cast<LaserType*>(objType);

    // A PPC runs through the frames of its effect shape and hits on frame hitPPC.
    if (type->laserEffectShape != nullptr)
    {
        if (justCreated == 0)
        {
            ppcAnimTimeLeft -= frameLength;

            if (ppcAnimTimeLeft < 0.0f)
            {
                ppcAnimTimeLeft = type->animPPC;
            }

            ppcFrameTimeLeft -= frameLength;

            if (ppcFrameTimeLeft < 0.0f)
            {
                ppcFrameTimeLeft = type->lengthPPC;
                ppcFrame++;

                if (ppcFrame == static_cast<int32_t>(type->hitPPC) && target.object != nullptr && damageApplied == 0)
                {
                    applyShot(this);
                }

                if (ppcFrame == static_cast<int32_t>(type->numPPCFrames))
                {
                    return 0;
                }
            }
        }
        else
        {
            ppcFrame = 0;
            damageApplied = 0;
            ppcFrameTimeLeft = type->lengthPPC;
            ppcAnimTimeLeft = type->animPPC;
            soundSystem->playDigitalSample(type->soundEffectId, 1, this, 0, 0);
        }

        justCreated = 0;
        return 1;
    }

    // A beam steps through its colour stages, then hits.
    if (currentStage == 0xff)
    {
        currentStage = 0;
        setStage(this, type, 0);
        justCreated = 0;
        return 1;
    }

    if (0.0f <= stageTimeLeft)
    {
        stageTimeLeft -= frameLength;
        justCreated = 0;
        return 1;
    }

    currentStage++;

    if (currentStage < type->numStages)
    {
        setStage(this, type, currentStage);
        justCreated = 0;
        return 1;
    }

    if (target.object == nullptr || damageApplied != 0)
    {
        return 0;
    }

    applyShot(this);
    return 0;
}

auto Laser::render() -> void
{
    if (justCreated != 0)
    {
        return;
    }

    auto* shooter = static_cast<GameObject*>(source.object);

    if (shooter == nullptr)
    {
        return;
    }

    const vector_3d start = shooter->getPositionFromHS(static_cast<uint32_t>(sourceHotSpot));
    auto* victim = static_cast<GameObject*>(target.object);

    if (victim != nullptr)
    {
        // Port fix (OB-017): end at the hot spot that was hit (the original used sourceHotSpot).
        const uint32_t hotSpot = victim->objectClass == TURRET ? 0xffffffff : static_cast<uint32_t>(targetHotSpot);
        setTargetPosition(victim->getPositionFromHS(hotSpot));
    }

    // Grow the beam out to the target over the first stage (or PPC frame).
    const auto* type = static_cast<LaserType*>(objType);

    if (type->laserEffectShape == nullptr)
    {
        if (currentStage == 0 && victim != nullptr && targetPosition != nullptr)
        {
            // Faithful: the first stage's length is always read from the enemy set (index numStages).
            const float firstDuration = type->numStages != 0 ? type->stageDuration[type->numStages] : -1.0f;
            extendBeam(*targetPosition, start, firstDuration - stageTimeLeft);
        }
    }
    else if (ppcFrame == 0 && victim != nullptr && targetPosition != nullptr)
    {
        extendBeam(*targetPosition, start, type->lengthPPC - ppcFrameTimeLeft);
    }

    // The hit (or miss) effect, once: when the beam reaches its second stage, or the PPC its hit frame.
    if (hitEffectCreated == 0)
    {
        if (type->laserEffectShape == nullptr)
        {
            if (currentStage != 0)
            {
                createHitEffect(this, type, victim);
            }
        }
        else if (ppcFrame == static_cast<int32_t>(type->hitPPC))
        {
            createHitEffect(this, type, victim);
        }
    }

    // Port fix: the original goes on to draw from an unset end point when there is no target position.
    if (targetPosition == nullptr)
    {
        return;
    }

    const vector_3d end = *targetPosition;
    vector_2d start100;
    vector_2d start50;
    vector_2d end100;
    vector_2d end50;

    if (land != nullptr)
    {
        vector_3d startPos = start;
        vector_3d endPos = end;
        land->projectTerrain(startPos, start100, start50);
        land->projectTerrain(endPos, end100, end50);
    }

    float startX;
    float startY;
    float endX;
    float endY;

    if (eye->cameraScale == 1)
    {
        startX = (start50.x - eye->screenUL50.x) + eye->halfWidth;
        startY = (start50.y - eye->screenUL50.y) + eye->halfHeight;
        endX = (end50.x - eye->screenUL50.x) + eye->halfWidth;
        endY = (end50.y - eye->screenUL50.y) + eye->halfHeight;
    }
    else
    {
        startX = (start100.x - eye->screenUL.x) + eye->halfWidth;
        startY = (start100.y - eye->screenUL.y) + eye->halfHeight;
        endX = (end100.x - eye->screenUL.x) + eye->halfWidth;
        endY = (end100.y - eye->screenUL.y) + eye->halfHeight;
    }

    // (The original then tests the leftmost end against the screen's left edge, but draws either way.)

    PolyElementData data;

    if (type->laserEffectShape == nullptr)
    {
        // A beam: a quad pixelWidth wide across the facing, in the stage's outer colour.
        const float width = static_cast<float>(type->pixelWidth);
        // Original behaviour (OB-018): the facing is in degrees, and gets multiplied by 57.3 again before cos/sin.
        float crossX = static_cast<float>(std::cos(shooter->relViewFacingTo(end) * RADIANS_TO_DEGREES) * width);
        float crossY = static_cast<float>(std::sin(shooter->relViewFacingTo(end) * RADIANS_TO_DEGREES) * width);
        ElementList->openGroup(static_cast<int32_t>(startY), 1);
        const auto depth = static_cast<int32_t>((endY + startY) * 0.5f);
        const FIXED16 color = static_cast<FIXED16>(coolColor << 16);
        data.numVertices = 4;
        data.vertices[0] =
            screenVertex(static_cast<int32_t>(crossX + startX), static_cast<int32_t>(startY + crossY), color, 0, 0);
        data.vertices[3] =
            screenVertex(static_cast<int32_t>(startX - crossX), static_cast<int32_t>(startY - crossY), color, 0, 0);
        data.vertices[2] =
            screenVertex(static_cast<int32_t>(endX - crossX), static_cast<int32_t>(endY - crossY), color, 0, 0);
        data.vertices[1] =
            screenVertex(static_cast<int32_t>(crossX + endX), static_cast<int32_t>(endY + crossY), color, 0, 0);
        ElementList->add(ElementPool::Make<PolygonElement>(&data, depth));

        if (hotColor == coolColor)
        {
            return;
        }

        // The core, half as wide.
        // Original behaviour (OB-019): the core is drawn in the outer (cool) colour too; the hot colour is never used.
        crossX = static_cast<float>(crossX * 0.5);
        crossY = static_cast<float>(crossY * 0.5);
        data.vertices[0] =
            screenVertex(static_cast<int32_t>(crossX + startX), static_cast<int32_t>(startY + crossY), color, 0, 0);
        data.vertices[3] =
            screenVertex(static_cast<int32_t>(startX - crossX), static_cast<int32_t>(startY - crossY), color, 0, 0);
        data.vertices[2] =
            screenVertex(static_cast<int32_t>(endX - crossX), static_cast<int32_t>(endY - crossY), color, 0, 0);
        data.vertices[1] =
            screenVertex(static_cast<int32_t>(crossX + endX), static_cast<int32_t>(endY + crossY), color, 0, 0);
        ElementList->add(ElementPool::Make<PolygonElement>(&data, depth));
        return;
    }

    // A PPC: the current frame of the effect shape, drawn into its own buffer and stretched along the beam.
    if (laserEffectBuffer == nullptr)
    {
        laserEffectBuffer = static_cast<uint8_t*>(systemHeap->malloc(0x10000));
        laserPane = static_cast<_pane*>(systemHeap->malloc(sizeof(_pane)));
        laserWindow = static_cast<_window*>(systemHeap->malloc(sizeof(_window)));
        const auto shapeWidth = static_cast<int32_t>(type->rPPC - type->lPPC);
        const auto shapeHeight = static_cast<int32_t>(type->bPPC - type->tPPC);
        laserPane->x0 = 0;
        laserPane->y0 = 0;
        laserPane->x1 = shapeWidth;
        laserPane->y1 = shapeHeight;
        laserWindow->buffer = laserEffectBuffer;
        laserWindow->x_max = shapeWidth + 1;
        laserWindow->y_max = shapeHeight + 1;
        laserPane->window = laserWindow;
        laserTexture =
            MCRenderer::CreateTexture(laserEffectBuffer, laserWindow->x_max, laserWindow->y_max, MCTextureUse::Dynamic);
    }

    AG_shape_draw(laserPane, type->laserEffectShape, ppcFrame, 0, 0);
    MCRenderer::UnlockTexture(laserTexture);
    const float width = static_cast<float>(type->pixelWidth);
    const auto top = static_cast<int32_t>(startY);
    ElementList->openGroup(top, 1);
    const auto left = static_cast<int32_t>(startX);
    const auto right = static_cast<int32_t>(endX);
    data.numVertices = 4;
    data.vertices[0] = screenVertex(left, top, 0, laserPane->x0 << 16, laserPane->y0 << 16);
    data.vertices[3] =
        screenVertex(left, static_cast<int32_t>(startY - width), 0, laserPane->x0 << 16, laserPane->y1 << 16);
    data.vertices[2] =
        screenVertex(right, static_cast<int32_t>(endY - width), 0, laserPane->x1 << 16, laserPane->y1 << 16);
    data.vertices[1] = screenVertex(right, static_cast<int32_t>(endY), 0, laserPane->x1 << 16, laserPane->y0 << 16);
    data.texture = laserEffectBuffer;
    data.textureWidth = laserWindow->x_max;
    data.textureHeight = laserWindow->y_max;
    data.textureHandle = laserTexture;
    data.fadeTable = nullptr;
    ElementList->add(ElementPool::Make<PolygonElement>(&data, static_cast<int32_t>((endY + startY) * 0.5f)));
}

auto Laser::setTargetPosition(vector_3d position) -> void
{
    if (targetPosition == nullptr)
    {
        targetPosition = new vector_3d;
    }

    *targetPosition = position;
}

auto Laser::connect(GameObject* source, vector_3d targetPos, _WeaponShotInfo* shotInfo, int32_t sourceHotSpot) -> void
{
    this->source.setWatcher(source);
    this->sourceHotSpot = sourceHotSpot;
    setTargetPosition(targetPos);

    if (shotInfo != nullptr)
    {
        const _WeaponShotInfo shot = *shotInfo;
        this->shotInfo.init(shot.attacker, shot.masterId, shot.damage, shot.hitLocation, shot.entryAngle);
    }
}
