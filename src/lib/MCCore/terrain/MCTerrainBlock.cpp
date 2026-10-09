#include "stdafx.h"
#include "terrain/MCVertex.h"
#include "ai/MCMoveSystem.h"
#include "camera/MCCamera.h"
#include "color/MCPalette.h"
#include "engine/MCByteFlag.h"
#include "engine/MCElementBuffer.h"
#include "engine/MCLineElement.h"
#include "lib/MCFatal.h"
#include "mission/MCScenario.h"
#include "object/MCForces.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTerrainTiles.h"
#include "vfx/MCVfx.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The tag at the start of a fast-shape table ("DNAH" read as a little-endian int).</summary>
    constexpr int32_t FastShapeTag = 'D' | ('N' << 8) | ('A' << 16) | ('H' << 24);

    /// <summary>Tile number of the first mine tile (by the corners' relative elevations).</summary>
    constexpr int32_t MineTileBase = 0xed1;
    /// <summary>Tile number of the exploded-mine crater.</summary>
    constexpr int32_t MineExplodedTile = 0xf21;

    /// <summary>The cached terrain tile <paramref name="tileNum"/> of the main camera's tile set.</summary>
    MCTerrainTile* LookupTile(int32_t tileNum)
    {
        return Terrain()->Tiles->Lookup(tileNum);
    }

    /// <summary>How many of the block's corners the home team sees this frame.</summary>
    uint32_t CountVisibleCorners(const MCTerrainBlock& block)
    {
        const MCByteFlag* visibleBits = Terrain()->HomeVisibleBits();
        uint32_t visibleCount = 0;

        for (const MCVertex* vertex : block.Vertices)
        {
            const auto row = static_cast<uint32_t>(static_cast<int32_t>(vertex->PosTile) >> 16);
            const uint32_t col = vertex->PosTile & 0xffff;

            if (visibleBits->GetFlag(row, col))
            {
                visibleCount++;
            }

            // The original also reads the home team's seen bits here, into a table nothing reads.
        }

        return visibleCount;
    }

    /// <summary>The number of the block's corners that project outside the pane.</summary>
    uint32_t CountClipped(const MCTerrainBlock& block)
    {
        uint32_t clippedCount = 0;

        for (const MCVertex* vertex : block.Vertices)
        {
            clippedCount += vertex->Clipped ? 1 : 0;
        }

        return clippedCount;
    }

    /// <summary>
    /// Draws a fast-shape overlay tile at the block's top-left vertex.
    /// </summary>
    /// <returns>False when the tile's data isn't a fast-shape table (the caller then stops drawing).</returns>
    bool DrawOverlayShape(const MCTerrainTile* tile, const MCVertex* topLeft, uint8_t* hazePalette)
    {
        if (tile == nullptr || tile->TileData() == nullptr)
        {
            return true;
        }

        int32_t tag = 0;
        std::memcpy(&tag, tile->TileData(), sizeof(tag));

        if (tag != FastShapeTag)
        {
            return false;
        }

        FastShapeDraw(GlobalPane, tile->TileData(), 0, topLeft->Px, topLeft->Py, hazePalette);
        return true;
    }

    /// <summary>Draws the mine tile matching the block's corner elevations.</summary>
    /// <returns>False when a tile's data isn't a fast-shape table.</returns>
    bool DrawMineTile(const MCTerrainBlock& block, uint8_t* hazePalette)
    {
        const int32_t tileNum =
            MCMineTileNumber(block.Vertices[0]->PVertex->Elevation, block.Vertices[1]->PVertex->Elevation,
                             block.Vertices[2]->PVertex->Elevation, block.Vertices[3]->PVertex->Elevation);
        return DrawOverlayShape(LookupTile(tileNum), block.Vertices[0], hazePalette);
    }
}

auto MCTerrainMineView(uint32_t overlay, bool playingInnerSphere, bool godMode) -> MCMineView
{
    const uint32_t clanMine = (overlay >> 13) & 3;
    const uint32_t innerSphereMine = (overlay >> 11) & 3;
    MCMineView view;
    view.InnerSphereMine = playingInnerSphere && innerSphereMine == 2;
    view.ClanMine = (!playingInnerSphere || godMode) && clanMine == 2;
    // Under a shown Clan mine only the Inner Sphere's state can make a crater (the Clans' is "laid" there).
    view.Crater = view.ClanMine ? innerSphereMine == 3 : (clanMine == 3 || innerSphereMine == 3);
    return view;
}

auto MCMineTileNumber(uint32_t e0, uint32_t e1, uint32_t e2, uint32_t e3) -> int32_t
{
    const uint32_t lowest = std::min({e0, e1, e2, e3});
    return static_cast<int32_t>(((e0 * 3 + e1) * 3 + e2) * 3 + e3) - static_cast<int32_t>(lowest) * 0x28 + MineTileBase;
}

uint8_t* MCTerrainHazePalette(int32_t hazeFactor, uint32_t visibleCount)
{
    const int32_t hazed = Eye->HazeInc * static_cast<int32_t>(visibleCount) + hazeFactor;
    const int32_t hazeLevel = (hazeFactor < 0 && hazed > 0) ? 0 : hazed;
    return GamePalette()->GetHazePalette(hazeLevel);
}

auto MCTerrainBlock::Draw(int32_t hazeFactor) -> void
{
    const MCVertex* topLeft = Vertices[0];
    uint32_t redrawCount = 0;

    for (const MCVertex* vertex : Vertices)
    {
        redrawCount += vertex->Redraw ? 1 : 0;
    }

    if (CountClipped(*this) == 4)
    {
        return;
    }

    uint8_t* hazePalette = nullptr;
    uint32_t visibleCount = 0;

    if (hazeFactor != 0x7fff)
    {
        visibleCount = CountVisibleCorners(*this);

        if (visibleCount != LastVisibleCount)
        {
            redrawCount++;

            for (MCVertex* vertex : Vertices)
            {
                vertex->EdgeRedraw = true;
            }
        }

        if (visibleCount != 4)
        {
            hazePalette = MCTerrainHazePalette(hazeFactor, visibleCount);
        }
    }

    // Unseen blocks (and the 0x7fff "all black" factor) are filled black.
    if (hazeFactor == 0x7fff || visibleCount == 0)
    {
        hazePalette = VfxTileFill;
    }

    const int32_t textureData = topLeft->PVertex->TextureData;
    const MCTerrainTile* terrainTile = textureData < 0 ? nullptr : LookupTile(textureData);

    if (terrainTile != nullptr && terrainTile->TileData() != nullptr && redrawCount != 0)
    {
        VfxNTileDraw(GlobalPane, terrainTile->TileData(), topLeft->Px, topLeft->Py, hazePalette);
    }
}

auto MCTerrainBlock::DrawOverlay(int32_t hazeFactor) -> void
{
    const MCVertex* topLeft = Vertices[0];

    if (CountClipped(*this) == 4 || hazeFactor == 0x7fff)
    {
        return;
    }

    const uint32_t visibleCount = CountVisibleCorners(*this);
    LastVisibleCount = static_cast<uint8_t>(visibleCount);
    uint8_t* hazePalette = nullptr;

    if (visibleCount != 4)
    {
        hazePalette = MCTerrainHazePalette(hazeFactor, visibleCount);
    }

    if (visibleCount == 0)
    {
        return;
    }

    const int32_t overlayData = topLeft->PVertex->OverlayData;
    const MCTerrainTile* overlay = overlayData < 0 ? nullptr : LookupTile(overlayData);

    if (!DrawOverlayShape(overlay, topLeft, hazePalette))
    {
        return;
    }

    // Mines: the map tile's overlay word holds the Inner Sphere's (bits 11-12) and the Clans' (bits 13-14) mine
    // state; 2 = a known mine, 3 = an exploded one.
    const int32_t col = (topLeft->BlockNum % MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                        topLeft->VertexNum % MCTerrain::VerticesBlockSide;
    const int32_t row = (topLeft->BlockNum / MCTerrain::BlocksMapSide) * MCTerrain::VerticesBlockSide +
                        topLeft->VertexNum / MCTerrain::VerticesBlockSide;
    const int ok = (row >= 0 && row < GameMap()->Height && col >= 0 && col < GameMap()->Width) ? 1 : 0;
    Assert(ok, 0, " bldng MapTile Out of Bounds ");
    const MCMineView mines = MCTerrainMineView(GameMap()->Map[GameMap()->Width * row + col].Overlay,
                                               HomeTeam() == InnerSphereTeam(), Scenario()->GodMode != 0);

    if (mines.InnerSphereMine && !DrawMineTile(*this, hazePalette))
    {
        return;
    }

    if (mines.ClanMine && !DrawMineTile(*this, hazePalette))
    {
        return;
    }

    if (mines.Crater)
    {
        DrawOverlayShape(LookupTile(MineExplodedTile), topLeft, hazePalette);
    }
}

auto MCTerrainBlock::DrawLine(int32_t color) -> void
{
    if (CountClipped(*this) == 4)
    {
        return;
    }

    // Every edge is drawn at the top-left vertex's depth.
    const int32_t depth = Vertices[0]->Py;

    for (size_t edge = 0; edge < 4; edge++)
    {
        const MCVertex* from = Vertices[edge];
        const MCVertex* to = Vertices[(edge + 1) & 3];
        MCVector2D start(static_cast<float>(from->Px), static_cast<float>(from->Py));
        MCVector2D end(static_cast<float>(to->Px), static_cast<float>(to->Py));
        ElementList()->Add(ElementList()->Make<MCLineElement>(start, end, color, nullptr, depth, -1));
    }
}
