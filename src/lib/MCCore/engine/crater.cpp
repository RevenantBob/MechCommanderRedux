#include "stdafx.h"
#include "engine/crater.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/cident.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "object/team.h"
#include "sprite/bactor.h"
#include "terrain/terrain.h"
#include "platform/MCRenderer.h"

CraterManager* craterManager = nullptr;

namespace
{
    /// <summary>Room for the shape pointers the original allocated (0x2c bytes).</summary>
    constexpr int32_t ORIGINAL_SHAPE_SLOTS = 11;
}

auto CraterManager::init(int32_t numCraters, uint32_t unused, char* craterFileName) -> int32_t
{
    (void)unused;
    currentCrater = 0;
    craterList.clear();
    numCraterShapes = 0;
    numCraterTypes = 0;
    craterShapes.clear();
    craterFile = nullptr;
    maxCraters = numCraters;

    FullPathFileName craterPath;
    craterPath.init(spritePath, craterFileName, ".pak");
    PacketFile* packetFile = new PacketFile();
    craterFile = packetFile;

    if (packetFile == nullptr)
    {
        return -0x3520fffd;
    }

    if (packetFile->open(craterPath, READ, 0x32) != 0)
    {
        FullPathFileName cdPath;
        cdPath.init(CDspritePath, craterFileName, ".pak");
        const int32_t result = packetFile->open(cdPath, READ, 0x32);

        if (result != 0)
        {
            return result;
        }
    }

    numCraterShapes = packetFile->getNumPackets();
    numCraterTypes = numCraterShapes >> 1;

    // Port fix: one pointer per shape. The original allocated 11 whatever the PAK held.
    craterShapes.resize(static_cast<size_t>(std::max(numCraterShapes, ORIGINAL_SHAPE_SLOTS)));

    for (int32_t i = 0; i < numCraterShapes; i++)
    {
        if (craterFile->seekPacket(i) == 0)
        {
            loadShape(i);
        }
    }

    craterFile->close();
    // Every slot starts as 0xFF bytes: a shape id of -1 (free) and NaN positions.
    craterList.resize(static_cast<size_t>(std::max(numCraters, 0)));
    std::memset(static_cast<void*>(craterList.data()), 0xff, craterList.size() * sizeof(CraterData));
    return 0;
}

auto CraterManager::destroy() -> void
{
    if (craterFile != nullptr)
    {
        craterFile->close();
        delete craterFile;
    }

    craterFile = nullptr;

    for (std::unique_ptr<uint8_t[]>& shape : craterShapes)
    {
        if (shape != nullptr)
        {
            MCRenderer::UnregisterData(shape.get());
        }
    }

    craterShapes.clear();
    craterList = {};
    currentCrater = 0;
}

auto CraterManager::getCrater(int32_t craterId) -> uint8_t*
{
    if (craterId == -1)
    {
        return nullptr;
    }

    if (craterShapes[craterId] == nullptr && craterFile->seekPacket(craterId) == 0)
    {
        dynamicFrameTiming = 0;
        loadShape(craterId);
    }

    return craterShapes[craterId].get();
}

auto CraterManager::loadShape(int32_t craterId) -> void
{
    const int32_t size = craterFile->getPacketSize();
    craterShapes[craterId] = std::make_unique<uint8_t[]>(static_cast<size_t>(size));
    craterFile->readPacket(craterId, craterShapes[craterId].get());
    MCRenderer::RegisterData(craterShapes[craterId].get(), static_cast<size_t>(size), MCDataKind::Shapes);
}

auto CraterManager::addCrater(int32_t craterType, vector_3d& position, int32_t rotation) -> int32_t
{
    int32_t tileR;
    int32_t tileC;
    GameMap->worldToMapTilePos(position, tileR, tileC);

    if (tileR > -1 && tileR < GameMap->height && tileC > -1 && tileC < GameMap->width)
    {
        int32_t cellR;
        int32_t cellC;
        GameMap->worldToMapPos(position, tileR, tileC, cellR, cellC);
        const MapTile& tile = GameMap->map[GameMap->width * tileR + tileC];
        const uint32_t terrainType = tile.cells & 0x7f;
        const uint32_t overlayType = tile.overlay & 0x7f;
        ByteFlag* visibleBits = homeTeam->alignment == -1 ? Terrain::ClanVisibleBits : Terrain::terrainVisibleBits;
        const auto row = static_cast<uint32_t>(tileR);
        const auto col = static_cast<uint32_t>(tileC);
        const uint8_t corner0 = visibleBits->getFlag(row, col);
        const uint8_t corner1 = visibleBits->getFlag(row + 1, col);
        const uint8_t corner2 = visibleBits->getFlag(row + 1, col + 1);
        const uint8_t corner3 = visibleBits->getFlag(row, col + 1);
        const bool visible = corner0 != 0 || corner1 != 0 || corner2 != 0 || corner3 != 0;

        if (terrainType < 0x2b && overlayType != 0x3e && visible)
        {
            const int32_t tileId = land->getTile(
                (tileR / Terrain::verticesBlockSide) * Terrain::blocksMapSide + tileC / Terrain::verticesBlockSide,
                (tileR % Terrain::verticesBlockSide) * Terrain::verticesBlockSide + tileC % Terrain::verticesBlockSide);

            if (tileId != 0xd64 && tileId != 0xd65)
            {
                CraterData& crater = craterList[currentCrater];
                crater.craterShapeId = craterType;
                crater.position = position;
                crater.rotation = rotation;
                currentCrater++;

                if (currentCrater == maxCraters)
                {
                    currentCrater = 0;
                }
            }
        }
    }

    return 0;
}

auto CraterManager::update() -> int32_t
{
    return 1;
}

auto CraterManager::render() -> void
{
    ElementList->openGroup(30000000, 0);
    const int32_t paneWidth = globalPane->x1 - globalPane->x0;
    const int32_t paneHeight = globalPane->y1 - globalPane->y0;
    CraterData* crater = craterList.data();

    for (int32_t count = maxCraters; count > 0; count--, crater++)
    {
        if (crater->craterShapeId == -1)
        {
            continue;
        }

        vector_2d screen100;
        vector_2d screen50;

        if (land != nullptr)
        {
            land->projectTerrain(crater->position, screen100, screen50);
        }

        int32_t shapeId = crater->craterShapeId;
        float screenX;
        float screenY;

        if (eye->cameraScale == 1)
        {
            shapeId += numCraterTypes;
            screenX = (screen50.x - eye->screenUL50.x) + eye->halfWidth;
            screenY = (screen50.y - eye->screenUL50.y) + eye->halfHeight;
        }
        else
        {
            screenX = (screen100.x - eye->screenUL.x) + eye->halfWidth;
            screenY = (screen100.y - eye->screenUL.y) + eye->halfHeight;
        }

        if (0.0f < screenX && 0.0f < screenY && screenX < static_cast<float>(paneWidth) &&
            screenY < static_cast<float>(paneHeight))
        {
            ElementList->add(ElementPool::Make<VFXElement>(craterShapes[shapeId].get(), screenX, screenY,
                                                           crater->rotation, 0, nullptr, 1, 0));
        }
    }
}
