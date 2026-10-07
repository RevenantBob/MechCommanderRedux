#include "stdafx.h"
#include "engine/MCCraterManager.h"
#include "ai/move.h"
#include "camera/MCCamera.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCVfxElement.h"
#include "lib/MCPacketFile.h"
#include "logistics/logmain.h"
#include "object/team.h"
#include "platform/MCRenderer.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>
    /// The shape slots the original allocated whatever the PAK held (0x2c bytes of pointers): crater types run up to
    /// 10, so a smaller PAK still has a (null) slot for each.
    /// </summary>
    constexpr size_t MinShapeSlots = 11;

    /// <summary>The terrain types from this one on are water: no craters.</summary>
    constexpr uint32_t FirstWaterTerrain = 0x2b;
    /// <summary>The overlay that takes no craters (a road-like overlay).</summary>
    constexpr uint32_t NoCraterOverlay = 0x3e;
    /// <summary>The bridge tiles: no craters.</summary>
    constexpr int32_t BridgeTiles[] = {0xd64, 0xd65};
    /// <summary>The draw-list depth of the crater group: under everything else.</summary>
    constexpr int32_t CraterDepth = 30000000;
}

MCCraterManager::MCCraterManager(int32_t numCraters, std::vector<std::vector<uint8_t>> shapes)
    : _Shapes(std::move(shapes)), _ShapeCount(static_cast<int32_t>(_Shapes.size())), _TypeCount(_ShapeCount >> 1)
{
    _Shapes.resize(std::max(_Shapes.size(), MinShapeSlots));

    for (const std::vector<uint8_t>& shape : _Shapes)
    {
        if (!shape.empty())
        {
            MCRenderer::RegisterData(shape.data(), shape.size(), MCDataKind::Shapes);
        }
    }

    // A free slot: shape -1, and a position the original's 0xFF fill left as NaNs.
    MCCraterData free;
    const float unset = std::bit_cast<float>(0xffffffffu);
    free.Position = MCVector3D(unset, unset, unset);
    _Craters.assign(static_cast<size_t>(std::max(numCraters, 0)), free);
}

MCCraterManager::~MCCraterManager()
{
    for (const std::vector<uint8_t>& shape : _Shapes)
    {
        if (!shape.empty())
        {
            MCRenderer::UnregisterData(shape.data());
        }
    }
}

auto MCCraterManager::Create(int32_t numCraters, std::string_view craterFileName)
    -> std::expected<std::unique_ptr<MCCraterManager>, std::string>
{
    MCPacketFile packetFile;

    if (packetFile.Open(GamePath(SpritePath, craterFileName, ".pak")) != 0)
    {
        const std::string cdPath = GamePath(CDspritePath, craterFileName, ".pak");

        if (const int32_t result = packetFile.Open(cdPath); result != 0)
        {
            return std::unexpected(
                std::format("Could not open crater file {} ({:#x})", cdPath, static_cast<uint32_t>(result)));
        }
    }

    std::vector<std::vector<uint8_t>> shapes(static_cast<size_t>(packetFile.GetNumPackets()));

    for (int32_t i = 0; i < packetFile.GetNumPackets(); i++)
    {
        if (packetFile.SeekPacket(i) == 0)
        {
            std::vector<uint8_t>& shape = shapes[static_cast<size_t>(i)];
            shape.resize(static_cast<size_t>(packetFile.GetPacketSize()));
            packetFile.ReadPacket(i, shape);
        }
    }

    return std::make_unique<MCCraterManager>(numCraters, std::move(shapes));
}

auto MCCraterManager::AddCrater(int32_t craterType, const MCVector3D& position, int32_t rotation) -> void
{
    int32_t tileR;
    int32_t tileC;
    GameMap->WorldToMapTilePos(position, tileR, tileC);

    if (tileR < 0 || tileR >= GameMap->Height || tileC < 0 || tileC >= GameMap->Width)
    {
        return;
    }

    const MCMapTile& tile = GameMap->Map[GameMap->Width * tileR + tileC];
    const uint32_t terrainType = tile.Cells & 0x7f;
    const uint32_t overlayType = tile.Overlay & 0x7f;
    // Seen by the player's side when any corner of the tile is.
    const MCByteFlag* visibleBits = Terrain()->HomeVisibleBits();
    const auto row = static_cast<uint32_t>(tileR);
    const auto col = static_cast<uint32_t>(tileC);
    const bool visible = visibleBits->GetFlag(row, col) || visibleBits->GetFlag(row + 1, col) ||
                         visibleBits->GetFlag(row + 1, col + 1) || visibleBits->GetFlag(row, col + 1);

    if (terrainType >= FirstWaterTerrain || overlayType == NoCraterOverlay || !visible)
    {
        return;
    }

    const int32_t side = MCTerrain::VerticesBlockSide;
    const int32_t tileId = Terrain()->GetTile((tileR / side) * MCTerrain::BlocksMapSide + tileC / side,
                                              (tileR % side) * side + tileC % side);

    if (std::ranges::find(BridgeTiles, tileId) == std::end(BridgeTiles))
    {
        Place(craterType, position, rotation);
    }
}

auto MCCraterManager::Place(int32_t craterType, const MCVector3D& position, int32_t rotation) -> void
{
    if (_Craters.empty())
    {
        return;
    }

    MCCraterData& crater = _Craters[static_cast<size_t>(_NextSlot)];
    crater.CraterShapeId = craterType;
    crater.Position = position;
    crater.Rotation = rotation;
    _NextSlot = (_NextSlot + 1) % static_cast<int32_t>(_Craters.size());
}

auto MCCraterManager::Render() -> void
{
    MCElementBuffer* elements = ElementList();
    elements->OpenGroup(CraterDepth, false);
    const int32_t paneWidth = GlobalPane->X1 - GlobalPane->X0;
    const int32_t paneHeight = GlobalPane->Y1 - GlobalPane->Y0;

    for (MCCraterData& crater : _Craters)
    {
        if (crater.CraterShapeId == -1)
        {
            continue;
        }

        MCVector2D screen100;
        MCVector2D screen50;

        if (Terrain() != nullptr)
        {
            Terrain()->ProjectTerrain(crater.Position, screen100, screen50);
        }

        int32_t shapeId = crater.CraterShapeId;
        float screenX;
        float screenY;

        if (Eye->CameraScale == 1)
        {
            shapeId += _TypeCount;
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
            std::vector<uint8_t>& shape = _Shapes[static_cast<size_t>(shapeId)];
            elements->Add(elements->Make<MCVfxElement>(shape.empty() ? nullptr : shape.data(), screenX, screenY,
                                                       crater.Rotation, 0, nullptr, true));
        }
    }
}
