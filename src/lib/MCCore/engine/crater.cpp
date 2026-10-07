#include "stdafx.h"
#include "engine/crater.h"
#include "ai/move.h"
#include "camera/camera.h"
#include "engine/bitflag.h"
#include "engine/ceglist.h"
#include "engine/cevfx.h"
#include "lib/MCIDString.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "object/team.h"
#include "sprite/bactor.h"
#include "terrain/terrain.h"
#include "platform/MCRenderer.h"

MCCraterManager* CraterManager = nullptr;

namespace
{
    /// <summary>Room for the shape pointers the original allocated (0x2c bytes).</summary>
    constexpr int32_t ORIGINAL_SHAPE_SLOTS = 11;
}

auto MCCraterManager::Init(int32_t numCraters, uint32_t unused, char* craterFileName) -> int32_t
{
    (void)unused;
    CurrentCrater = 0;
    CraterList.clear();
    NumCraterShapes = 0;
    NumCraterTypes = 0;
    CraterShapes.clear();
    CraterFile = nullptr;
    MaxCraters = numCraters;

    std::string craterPath;
    craterPath = GamePath(SpritePath, craterFileName, ".pak");
    MCPacketFile* packetFile = new MCPacketFile();
    CraterFile = packetFile;

    if (packetFile == nullptr)
    {
        return -0x3520fffd;
    }

    if (packetFile->Open(craterPath) != 0)
    {
        std::string cdPath;
        cdPath = GamePath(CDspritePath, craterFileName, ".pak");
        const int32_t result = packetFile->Open(cdPath);

        if (result != 0)
        {
            return result;
        }
    }

    NumCraterShapes = packetFile->GetNumPackets();
    NumCraterTypes = NumCraterShapes >> 1;

    // Port fix: one pointer per shape. The original allocated 11 whatever the PAK held.
    CraterShapes.resize(static_cast<size_t>(std::max(NumCraterShapes, ORIGINAL_SHAPE_SLOTS)));

    for (int32_t i = 0; i < NumCraterShapes; i++)
    {
        if (CraterFile->SeekPacket(i) == 0)
        {
            LoadShape(i);
        }
    }

    CraterFile->Close();
    // Every slot starts as 0xFF bytes: a shape id of -1 (free) and NaN positions.
    CraterList.resize(static_cast<size_t>(std::max(numCraters, 0)));
    std::memset(static_cast<void*>(CraterList.data()), 0xff, CraterList.size() * sizeof(MCCraterData));
    return 0;
}

auto MCCraterManager::Destroy() -> void
{
    if (CraterFile != nullptr)
    {
        CraterFile->Close();
        delete CraterFile;
    }

    CraterFile = nullptr;

    for (std::unique_ptr<uint8_t[]>& shape : CraterShapes)
    {
        if (shape != nullptr)
        {
            MCRenderer::UnregisterData(shape.get());
        }
    }

    CraterShapes.clear();
    CraterList = {};
    CurrentCrater = 0;
}

auto MCCraterManager::GetCrater(int32_t craterId) -> uint8_t*
{
    if (craterId == -1)
    {
        return nullptr;
    }

    if (CraterShapes[craterId] == nullptr && CraterFile->SeekPacket(craterId) == 0)
    {
        DynamicFrameTiming = 0;
        LoadShape(craterId);
    }

    return CraterShapes[craterId].get();
}

auto MCCraterManager::LoadShape(int32_t craterId) -> void
{
    const int32_t size = CraterFile->GetPacketSize();
    CraterShapes[craterId] = std::make_unique<uint8_t[]>(static_cast<size_t>(size));
    CraterFile->ReadPacket(craterId, CraterShapes[craterId].get());
    MCRenderer::RegisterData(CraterShapes[craterId].get(), static_cast<size_t>(size), MCDataKind::Shapes);
}

auto MCCraterManager::AddCrater(int32_t craterType, MCVector3D& position, int32_t rotation) -> int32_t
{
    int32_t tileR;
    int32_t tileC;
    GameMap->WorldToMapTilePos(position, tileR, tileC);

    if (tileR > -1 && tileR < GameMap->Height && tileC > -1 && tileC < GameMap->Width)
    {
        int32_t cellR;
        int32_t cellC;
        GameMap->WorldToMapPos(position, tileR, tileC, cellR, cellC);
        const MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];
        const uint32_t terrainType = tile.Cells & 0x7f;
        const uint32_t overlayType = tile.Overlay & 0x7f;
        MCByteFlag* visibleBits =
            HomeTeam->Alignment == -1 ? MCTerrain::ClanVisibleBits : MCTerrain::TerrainVisibleBits;
        const auto row = static_cast<uint32_t>(tileR);
        const auto col = static_cast<uint32_t>(tileC);
        const uint8_t corner0 = visibleBits->GetFlag(row, col);
        const uint8_t corner1 = visibleBits->GetFlag(row + 1, col);
        const uint8_t corner2 = visibleBits->GetFlag(row + 1, col + 1);
        const uint8_t corner3 = visibleBits->GetFlag(row, col + 1);
        const bool visible = corner0 != 0 || corner1 != 0 || corner2 != 0 || corner3 != 0;

        if (terrainType < 0x2b && overlayType != 0x3e && visible)
        {
            const int32_t tileId = Land->GetTile((tileR / MCTerrain::VerticesBlockSide) * MCTerrain::BlocksMapSide +
                                                     tileC / MCTerrain::VerticesBlockSide,
                                                 (tileR % MCTerrain::VerticesBlockSide) * MCTerrain::VerticesBlockSide +
                                                     tileC % MCTerrain::VerticesBlockSide);

            if (tileId != 0xd64 && tileId != 0xd65)
            {
                MCCraterData& crater = CraterList[CurrentCrater];
                crater.CraterShapeId = craterType;
                crater.Position = position;
                crater.Rotation = rotation;
                CurrentCrater++;

                if (CurrentCrater == MaxCraters)
                {
                    CurrentCrater = 0;
                }
            }
        }
    }

    return 0;
}

auto MCCraterManager::Update() -> int32_t
{
    return 1;
}

auto MCCraterManager::Render() -> void
{
    ElementList->OpenGroup(30000000, 0);
    const int32_t paneWidth = GlobalPane->X1 - GlobalPane->X0;
    const int32_t paneHeight = GlobalPane->Y1 - GlobalPane->Y0;
    MCCraterData* crater = CraterList.data();

    for (int32_t count = MaxCraters; count > 0; count--, crater++)
    {
        if (crater->CraterShapeId == -1)
        {
            continue;
        }

        MCVector2D screen100;
        MCVector2D screen50;

        if (Land != nullptr)
        {
            Land->ProjectTerrain(crater->Position, screen100, screen50);
        }

        int32_t shapeId = crater->CraterShapeId;
        float screenX;
        float screenY;

        if (Eye->CameraScale == 1)
        {
            shapeId += NumCraterTypes;
            screenX = (screen50.X - Eye->ScreenUL50.X) + Eye->HalfWidth;
            screenY = (screen50.Y - Eye->ScreenUL50.Y) + Eye->HalfHeight;
        }
        else
        {
            screenX = (screen100.X - Eye->ScreenUL.X) + Eye->HalfWidth;
            screenY = (screen100.Y - Eye->ScreenUL.Y) + Eye->HalfHeight;
        }

        if (0.0f < screenX && 0.0f < screenY && screenX < static_cast<float>(paneWidth) &&
            screenY < static_cast<float>(paneHeight))
        {
            ElementList->Add(MCElementPool::Make<MCVfxElement>(CraterShapes[shapeId].get(), screenX, screenY,
                                                               crater->Rotation, 0, nullptr, 1, 0));
        }
    }
}
