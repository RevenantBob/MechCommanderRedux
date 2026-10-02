#include "stdafx.h"
#include "engine/crater.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/cident.h"
#include "lib/heap.h"
#include "lib/packet.h"
#include "logistics/logmain.h"
#include "object/team.h"
#include "sprite/bactor.h"
#include "terrain/terrain.h"

CraterManager* craterManager = nullptr;

namespace
{
    /// <summary>Room for the shape pointers the original allocated (0x2c bytes).</summary>
    constexpr int32_t ORIGINAL_SHAPE_SLOTS = 11;
}

auto CraterManager::init(int32_t numCraters, uint32_t unused, char* craterFileName) -> int32_t
{
    (void)unused;
    craterPosHeap = nullptr;
    craterShpHeap = nullptr;
    craterShpHeapSize = 0;
    currentCrater = 0;
    craterList = nullptr;
    numCraterShapes = 0;
    numCraterTypes = 0;
    craterShapes = nullptr;
    craterFile = nullptr;
    maxCraters = numCraters;
    craterPosHeapSize = static_cast<uint32_t>(numCraters * 0x1c);
    // Port fix: at least one CraterData per slot (0x14 in the original, which sized the heap at 0x1c a slot).
    const uint32_t posHeapSize =
        std::max<uint32_t>(craterPosHeapSize, static_cast<uint32_t>(numCraters * sizeof(CraterData)));
    craterPosHeap = new HeapManager();

    if (craterPosHeap == nullptr)
    {
        return -0x3520ffff;
    }

    int32_t result = craterPosHeap->createHeap(posHeapSize);

    if (result != 0)
    {
        return result;
    }

    result = craterPosHeap->commitHeap(posHeapSize);

    if (result != 0)
    {
        return result;
    }

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
        result = packetFile->open(cdPath, READ, 0x32);

        if (result != 0)
        {
            return result;
        }
    }

    numCraterShapes = packetFile->getNumPackets();
    numCraterTypes = numCraterShapes >> 1;
    craterShpHeapSize = 0;

    for (int32_t i = 0; i < numCraterShapes; i++)
    {
        packetFile->seekPacket(i);
        craterShpHeapSize += static_cast<uint32_t>(packetFile->getPacketSize() + 100);
    }

    craterShpHeap = new UserHeap();

    if (craterShpHeap == nullptr)
    {
        return -0x3520ffff;
    }

    result = craterShpHeap->init(craterShpHeapSize, nullptr);

    if (result != 0)
    {
        return result;
    }

    // Port fix: one pointer per shape. The original allocated 11 whatever the PAK held.
    const int32_t shapeSlots = std::max(numCraterShapes, ORIGINAL_SHAPE_SLOTS);
    craterShapes = static_cast<uint8_t**>(craterShpHeap->malloc(static_cast<uint32_t>(shapeSlots * sizeof(uint8_t*))));
    std::memset(craterShapes, 0, shapeSlots * sizeof(uint8_t*));

    for (int32_t i = 0; i < numCraterShapes; i++)
    {
        if (craterFile->seekPacket(i) == 0)
        {
            craterShapes[i] =
                static_cast<uint8_t*>(craterShpHeap->malloc(static_cast<uint32_t>(craterFile->getPacketSize())));

            if (craterShapes[i] != nullptr)
            {
                craterFile->readPacket(i, craterShapes[i]);
            }
        }
    }

    craterFile->close();
    craterList = reinterpret_cast<CraterData*>(craterPosHeap->getHeapPtr());
    std::memset(craterPosHeap->getHeapPtr(), 0xff, craterPosHeapSize);
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
    delete craterShpHeap;
    craterShpHeap = nullptr;
    delete craterPosHeap;
    craterPosHeap = nullptr;
    craterShpHeapSize = 0;
    craterPosHeapSize = 0;
    craterList = nullptr;
    currentCrater = 0;
}

auto CraterManager::getCrater(int32_t craterId) -> uint8_t*
{
    if (craterId == -1)
    {
        return nullptr;
    }

    uint8_t*& shape = craterShapes[craterId];

    if (shape == nullptr && craterFile->seekPacket(craterId) == 0)
    {
        shape = static_cast<uint8_t*>(craterShpHeap->malloc(static_cast<uint32_t>(craterFile->getPacketSize())));

        if (shape != nullptr)
        {
            dynamicFrameTiming = 0;
            craterFile->readPacket(craterId, shape);
        }
    }

    return shape;
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
    CraterData* crater = craterList;

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
            ElementList->add(
                new VFXElement(craterShapes[shapeId], screenX, screenY, crater->rotation, 0, nullptr, 1, 0));
        }
    }
}
