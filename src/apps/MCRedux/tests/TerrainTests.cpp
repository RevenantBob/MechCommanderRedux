#include "stdafx.h"
#include "MCTest.h"
#include "camera/MCCamera.h"
#include "engine/MCByteFlag.h"
#include "fakes/MCMemoryFileSource.h"
#include "fixtures/MCTerrainGeometryScope.h"
#include "fixtures/MCTinyMap.h"
#include "lib/MCPacketFile.h"
#include "main/MCGameContext.h"
#include "main/main.h"
#include "terrain/MCMapBlockManager.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCVertex.h"
#include "terrain/MCTerrain.h"
#include "terrain/MCTerrainTiles.h"
#include "terrain/MCTerrainWindow.h"

namespace
{
    /// <summary>The tiny maps' metres between two vertices, and a height level.</summary>
    constexpr float MetersPerVertex = MCTinyMap::MetersPerTile;
    constexpr float MetersPerLevel = 16.0f;

    /// <summary>
    /// A tiny map of <paramref name="tiles"/> tiles a side with the terrain's whole geometry set for it (one block of
    /// that many vertices).
    /// </summary>
    struct TinyTerrain
    {
        explicit TinyTerrain(int32_t tiles) : map(tiles), geometry(tiles, 1, MetersPerVertex, MetersPerLevel) {}

        /// <summary>Sets the height level of map vertex (tile) (<paramref name="row"/>, <paramref name="col"/>).</summary>
        void SetLevel(int32_t row, int32_t col, uint32_t level)
        {
            MCMapTile& tile = GameMap->Map[row * GameMap->Width + col];
            tile.Cells = (tile.Cells & ~(0x3fu << 7)) | (level << 7);
        }

        /// <summary>The world point at <paramref name="x"/>, <paramref name="y"/> vertices from the map's top-left corner.</summary>
        static MCVector3D At(float x, float y)
        {
            return MCVector3D(MCTerrain::MapTopLeft3d100.X + x * MetersPerVertex,
                              MCTerrain::MapTopLeft3d100.Y - y * MetersPerVertex, 0.0f);
        }

        MCTinyMap map;
        MCTerrainGeometryScope geometry;
    };

    /// <summary>Writes the PAK <paramref name="name"/> of <paramref name="packets"/> to the memory source's folder.</summary>
    void WritePak(std::string_view name, const std::vector<std::vector<uint8_t>>& packets)
    {
        MCPacketFile pak;
        REQUIRE_EQ(pak.Create(name), NO_ERR);
        pak.Reserve(static_cast<int32_t>(packets.size()));

        for (size_t i = 0; i < packets.size(); ++i)
        {
            pak.WritePacket(static_cast<int32_t>(i), packets[i], MCPacketStorage::Raw);
        }
    }

    /// <summary>The tile path globals pointed at the memory source's root for the test's lifetime.</summary>
    struct TilePathScope
    {
        TilePathScope() : savedTilePath(std::exchange(TilePath, "")), savedTile90Path(std::exchange(Tile90Path, "")) {}

        ~TilePathScope()
        {
            TilePath = savedTilePath;
            Tile90Path = savedTile90Path;
        }

        std::string savedTilePath;
        std::string savedTile90Path;
    };
}

TEST_CASE("terrain: the geometry follows the terrain file's sizes")
{
    // A retail-sized map: 6 x 6 blocks of 20 vertices, 128 m apart.
    MCTerrainGeometryScope geometry(20, 6, 128.0f, 16.0f);
    CHECK_EQ(MCTerrain::TotalBlocks, 36);
    CHECK_EQ(VerticesPerBlock, 400);
    CHECK_EQ(MCTerrain::MetersBlockSide, 2560.0f);
    CHECK_EQ(WorldUnitsMapSide, 15360.0f);
    CHECK_EQ(MCTerrain::VerticesMapSide, 60);
    CHECK_EQ(MCTerrain::OneOvermetersPerVertex, 1.0f / 128.0f);
    CHECK_EQ(MCTerrain::MetersPerVertexDivMapcellDim, 128.0f / 3.0f);
    // The world's origin is the map's centre: the top-left corner is half a map left and up.
    CHECK_EQ(MCTerrain::MapTopLeft3d100.X, -7680.0f);
    CHECK_EQ(MCTerrain::MapTopLeft3d100.Y, 7680.0f);
    // The corner projects to the screen with the 30-degree view: x = (x + y) cos 30, y = (x - y) sin 30.
    CHECK_EQ(MCTerrain::MapTopLeft2d100.X, 0.0f);
    CHECK(std::fabs(MCTerrain::MapTopLeft2d100.Y - -7680.0f) < 0.01f);
}

TEST_CASE("terrain: a map vertex is found in its block, and off the map in the off-map block")
{
    MCTerrainGeometryScope geometry(4, 2, 128.0f, 16.0f);
    // Two by two blocks of 4 x 4 vertices; each vertex tagged with its block * 100 + place, the off-map block 900+.
    std::vector<MCPrecompVertex> vertices(5 * 16);

    for (size_t i = 0; i < vertices.size(); ++i)
    {
        vertices[i].TextureData = static_cast<int16_t>((i / 16 == 4 ? 900 : (i / 16) * 100) + i % 16);
    }

    MCMapBlockManager blocks(vertices, 4);
    CHECK_EQ(blocks.MapVertexAt(0, 0)->TextureData, int16_t{0});
    CHECK_EQ(blocks.MapVertexAt(1, 2)->TextureData, int16_t{6});
    CHECK_EQ(blocks.MapVertexAt(2, 5)->TextureData, int16_t{100 + 9});
    CHECK_EQ(blocks.MapVertexAt(5, 1)->TextureData, int16_t{200 + 5});
    CHECK_EQ(blocks.MapVertexAt(7, 7)->TextureData, int16_t{300 + 15});
    // Off the map: the off-map block, at the same place within a block.
    CHECK_EQ(blocks.MapVertexAt(-1, 0)->TextureData, int16_t{900 + 12});
    CHECK_EQ(blocks.MapVertexAt(8, 9)->TextureData, int16_t{900 + 1});
    CHECK_EQ(blocks.BlockPtr(-1), blocks.BlockPtr(4));
    CHECK_EQ(blocks.BlockPtr(37), blocks.BlockPtr(4));

    // Tiles placed by the map's objects add to the stored numbers.
    blocks.SetTile(3, 2, 5);
    blocks.SetOverlayTile(3, 2, -3);
    CHECK_EQ(blocks.GetTile(3, 2), 300 + 2 + 5);
    CHECK_EQ(blocks.GetOverlayTile(3, 2), -3);
}

TEST_CASE("terrain: on the first turn the off-map ground is raised a level above the map's base")
{
    TinyTerrain terrain(4);
    GameMap->BaseElevation = 3;
    std::vector<MCPrecompVertex> vertices(2 * 16);
    MCMapBlockManager blocks(vertices, 1);
    blocks.RaiseOffMapBlock();

    for (int32_t i = 0; i < 16; ++i)
    {
        CHECK_EQ(blocks.BlockPtr(1)[i].Elevation, uint8_t{4});
        CHECK_EQ(blocks.BlockPtr(0)[i].Elevation, uint8_t{0});
    }
}

TEST_CASE("terrain: the elevation at a point lies on the face of its map tile")
{
    TinyTerrain terrain(8);
    GameMap->BaseElevation = 1;

    // Flat ground: the base plus the tile's level, everywhere on the tile.
    for (int32_t row = 0; row < 8; ++row)
    {
        for (int32_t col = 0; col < 8; ++col)
        {
            terrain.SetLevel(row, col, 2);
        }
    }

    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(3.5f, 3.5f)), 3 * MetersPerLevel);
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(1.25f, 6.75f)), 3 * MetersPerLevel);

    // A slope from vertex (2, 2) up to (2, 3) (the next column): along the top edge the height rises evenly.
    terrain.SetLevel(2, 3, 6);
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(2.0f, 2.0f)), 3 * MetersPerLevel);
    CHECK(std::fabs(TerrainElevationAt(TinyTerrain::At(2.25f, 2.0f)) - 4 * MetersPerLevel) < 0.01f);
    CHECK(std::fabs(TerrainElevationAt(TinyTerrain::At(2.75f, 2.0f)) - 6 * MetersPerLevel) < 0.01f);
    // Along the left edge (towards row 3) it doesn't rise: that edge's corners are both level 2.
    CHECK(std::fabs(TerrainElevationAt(TinyTerrain::At(2.0f, 2.5f)) - 3 * MetersPerLevel) < 0.01f);
}

TEST_CASE("terrain: a bridge deck stands at its highest corner")
{
    TinyTerrain terrain(8);
    GameMap->BaseElevation = 0;

    for (int32_t row = 0; row < 8; ++row)
    {
        for (int32_t col = 0; col < 8; ++col)
        {
            terrain.SetLevel(row, col, 1);
        }
    }

    // Overlay types 0x25..0x28 are bridge decks: drawn flat at the highest of the tile's four corners...
    GameMap->Map[4 * 8 + 4].Overlay = 0x26;
    terrain.SetLevel(5, 5, 3);
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(4.5f, 4.5f)), 3 * MetersPerLevel);
    // ...and a level above the ground when the corners are level.
    GameMap->Map[1 * 8 + 1].Overlay = 0x25;
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(1.5f, 1.5f)), 2 * MetersPerLevel);
    // Other overlays are ground.
    GameMap->Map[1 * 8 + 1].Overlay = 0x24;
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(1.5f, 1.5f)), 1 * MetersPerLevel);
}

TEST_CASE("terrain: off the map the elevation is 0")
{
    TinyTerrain terrain(8);
    GameMap->BaseElevation = 4;
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(-0.5f, 3.0f)), 0.0f);
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(3.0f, 8.5f)), 0.0f);
    // The last row and column have no tile beyond them to make a face with.
    CHECK_EQ(TerrainElevationAt(TinyTerrain::At(7.5f, 3.0f)), 0.0f);
    CHECK(TerrainElevationAt(TinyTerrain::At(6.5f, 3.0f)) > 0.0f);
}

TEST_CASE("terrain: a looker marks the vertices around it as seen by its own side")
{
    TinyTerrain terrain(16);
    MCTestContextScope scope;
    auto land = std::make_unique<MCTerrain>();
    land->ISVisibleBits = std::make_unique<MCByteFlag>(16, 16, false);
    land->ClanVisibleBits = std::make_unique<MCByteFlag>(16, 16, false);
    MCTerrain* view = land.get();
    scope.Context().SetTerrain(std::move(land));

    // The radius in vertices is 3.34 times the range over the metres a vertex: 80 m makes 2 vertices.
    view->MarkRadiusSeen(TinyTerrain::At(8.5f, 6.5f), MCVector3D(), 360.0f, 80.0f, 1);
    CHECK(view->ISVisibleBits->GetFlag(6, 8));
    CHECK(view->ISVisibleBits->GetFlag(6, 9));
    CHECK(view->ISVisibleBits->GetFlag(5, 8));
    CHECK(!view->ISVisibleBits->GetFlag(6, 12));
    CHECK(!view->ISVisibleBits->GetFlag(10, 8));
    // Who 1 is the Inner Sphere: the Clans saw nothing.
    CHECK(!view->ClanVisibleBits->GetFlag(6, 8));

    // Anyone else marks the Clan bits; only full-circle looks count.
    view->MarkRadiusSeen(TinyTerrain::At(2.5f, 12.5f), MCVector3D(), 360.0f, 40.0f, 2);
    view->MarkRadiusSeen(TinyTerrain::At(12.5f, 2.5f), MCVector3D(), 90.0f, 40.0f, 2);
    CHECK(view->ClanVisibleBits->GetFlag(12, 2));
    CHECK(!view->ClanVisibleBits->GetFlag(2, 12));
    CHECK(!view->ISVisibleBits->GetFlag(12, 2));
}

TEST_CASE("terrain: the blocks drawn this frame have no limit, and no block (-1) always counts")
{
    MCTerrain terrain;
    // Block -1 (an object on no block) always counts, as the original's -1-ended list made it.
    CHECK(terrain.BlockUsed(-1));
    CHECK(!terrain.BlockUsed(3));

    // The original's list stopped at 324 blocks; the port's keeps them all.
    for (int32_t block = 0; block < 400; ++block)
    {
        terrain.MarkBlockUsed(block);
        terrain.MarkBlockUsed(block);
    }

    CHECK(terrain.BlockUsed(0));
    CHECK(terrain.BlockUsed(399));
    CHECK(!terrain.BlockUsed(400));
    terrain.ClearUsedBlocks();
    CHECK(!terrain.BlockUsed(399));
    CHECK(terrain.BlockUsed(-1));
}

TEST_CASE("terrain: a mine shows to its own side, the Clans' to the Clans or in god mode, and leaves a crater")
{
    // The overlay word: the Inner Sphere's mine state in bits 11..12, the Clans' in 13..14 (2 a mine laid, 3 one that
    // went off); bits 25..28 count the spread and don't show.
    constexpr auto innerSphere = [](uint32_t state) { return state << 11; };
    constexpr auto clan = [](uint32_t state) { return state << 13; };
    constexpr uint32_t spread = 0xfu << 25;

    MCMineView view = MCTerrainMineView(innerSphere(2) | spread, true, false);
    CHECK(view.InnerSphereMine);
    CHECK(!view.ClanMine);
    CHECK(!view.Crater);

    // Playing the Inner Sphere, the Clans' mines are hidden unless in god mode.
    view = MCTerrainMineView(clan(2), true, false);
    CHECK(!view.InnerSphereMine);
    CHECK(!view.ClanMine);
    view = MCTerrainMineView(clan(2), true, true);
    CHECK(view.ClanMine);
    // Playing the Clans, their own mines show and the Inner Sphere's don't.
    view = MCTerrainMineView(clan(2) | innerSphere(2), false, false);
    CHECK(view.ClanMine);
    CHECK(!view.InnerSphereMine);

    // A mine that went off leaves a crater, whoever laid it and whoever plays.
    CHECK(MCTerrainMineView(innerSphere(3), true, false).Crater);
    CHECK(MCTerrainMineView(clan(3), true, false).Crater);
    CHECK(MCTerrainMineView(innerSphere(3), false, false).Crater);
    CHECK(MCTerrainMineView(clan(2) | innerSphere(3), false, false).Crater);
    CHECK(!MCTerrainMineView(innerSphere(1) | clan(1), true, true).Crater);
    CHECK(!MCTerrainMineView(0, false, true).InnerSphereMine);
}

TEST_CASE("terrain: the mine tile follows the corners' heights above the lowest")
{
    // 81 mine tiles from 0xed1: the corners' levels above the lowest as base-3 digits, top-left first.
    CHECK_EQ(MCMineTileNumber(4, 4, 4, 4), 0xed1);
    CHECK_EQ(MCMineTileNumber(5, 4, 4, 4), 0xed1 + 27);
    CHECK_EQ(MCMineTileNumber(4, 4, 4, 5), 0xed1 + 1);
    CHECK_EQ(MCMineTileNumber(0, 1, 2, 1), 0xed1 + 9 + 2 * 3 + 1);
    CHECK_EQ(MCMineTileNumber(7, 9, 8, 7), 0xed1 + 2 * 9 + 1 * 3);
}

TEST_CASE("terrain: tiles are read from the tile PAKs once, and a missing one stays missing")
{
    MCTestContextScope scope;
    scope.Context().SetFiles(std::make_unique<MCMemoryFileSource>());
    TilePathScope paths;
    // Four tiles a set: the lower half of the table comes from the rotated PAK, the upper from the normal one.
    WritePak("ttest.pak", {{1}, {2}, {3, 3}, {}});
    WritePak("ttest90.pak", {{91}, {}, {93}, {94}});
    std::expected<std::unique_ptr<MCTerrainTiles>, std::string> made = MCTerrainTiles::Create("ttest");
    REQUIRE(made.has_value());
    MCTerrainTiles& tiles = **made;
    CHECK_EQ(tiles.NumTiles(), 4);
    CHECK(tiles.CustomTileSet());
    CHECK_EQ(tiles.TileSetOffset[1], 2);

    MCTerrainTile* first = tiles.Lookup(0);
    REQUIRE(first != nullptr);
    CHECK_EQ(first->TileData()[0], uint8_t{91});
    CHECK_EQ(tiles.Lookup(0), first);
    CHECK_EQ(tiles.Lookup(2)->Data.Size(), size_t{2});
    // The rotated view's numbers start half way.
    CHECK_EQ(tiles.Lookup(0, true)->TileData()[0], uint8_t{3});
    // Packet 1 of the rotated PAK is empty: missing, and not read again.
    CHECK(tiles.Lookup(1) == nullptr);
    CHECK(tiles.Lookup(1) == nullptr);
    CHECK(tiles.Lookup(-1) == nullptr);
    CHECK(tiles.Lookup(4) == nullptr);
    CHECK(tiles.Lookup(2, true) == nullptr);
}

TEST_CASE("tacmap: world points and map pixels round trip at every zoom and scroll")
{
    MCTacmapProjection map;
    map.MapWidth = 256;
    map.MapHeight = 256;
    // A 64-vertex map's diagonal over the map area's 260 pixels at 1x.
    const float diagonal = std::sqrt(2.0f * 64.0f * 64.0f) * MetersPerVertex;

    for (const int32_t zoom : {1, 2, 4, 8})
    {
        map.Zoom = zoom;
        map.MetersPerPixel = (diagonal / 260.0f) / static_cast<float>(zoom);

        for (const auto& [scrollX, scrollY] : {std::pair{0, 0}, std::pair{20, -12}})
        {
            MCTest::Scope where(std::format("zoom {} scroll ({}, {})", zoom, scrollX, scrollY));
            map.ScrollX = scrollX;
            map.ScrollY = scrollY;

            for (const MCVector3D& point : {MCVector3D(0.0f, 0.0f, 0.0f), MCVector3D(1200.0f, -340.0f, 0.0f),
                                            MCVector3D(-2500.0f, 2800.0f, 0.0f)})
            {
                for (const bool scrolled : {false, true})
                {
                    MCVector3D pixel = point;
                    map.WorldToMap(pixel, scrolled);
                    MCVector3D back = pixel;
                    map.MapToWorld(back, scrolled);
                    CHECK(std::fabs(back.X - point.X) < 0.05f);
                    CHECK(std::fabs(back.Y - point.Y) < 0.05f);
                    CHECK_EQ(back.Z, 0.0f);
                }
            }
        }
    }

    // Unscrolled at 1x, the world's centre is the 130-pixel map's centre; on the MFD it is the map area's (71, 99).
    map.Zoom = 1;
    map.MetersPerPixel = diagonal / 260.0f;
    map.ScrollX = 0;
    map.ScrollY = 0;
    MCVector3D centre(0.0f, 0.0f, 0.0f);
    map.WorldToMap(centre, false);
    CHECK_EQ(centre.X, 65.0f);
    CHECK_EQ(centre.Y, 65.0f);
    centre = MCVector3D(0.0f, 0.0f, 0.0f);
    map.WorldToMap(centre, true);
    CHECK_EQ(centre.X, 71.0f);
    CHECK_EQ(centre.Y, 99.0f);
    // North-east is straight up and east is up and right: the map is turned 45 degrees.
    MCVector3D northEast(1000.0f, 1000.0f, 0.0f);
    map.WorldToMap(northEast, false);
    CHECK(std::fabs(northEast.Y - 65.0f) < 0.001f);
    CHECK(northEast.X > 65.0f);
    // On the MFD, at 2x the same point lies twice as far from the centre; on the whole map it stays put.
    MCVector3D east(1000.0f, 0.0f, 0.0f);
    MCVector3D eastOnMap = east;
    map.WorldToMap(east, true);
    map.WorldToMap(eastOnMap, false);
    map.Zoom = 2;
    map.MetersPerPixel = (diagonal / 260.0f) / 2.0f;
    MCVector3D eastZoomed(1000.0f, 0.0f, 0.0f);
    MCVector3D eastZoomedOnMap = eastZoomed;
    map.WorldToMap(eastZoomed, true);
    map.WorldToMap(eastZoomedOnMap, false);
    CHECK(std::fabs((eastZoomed.X - 71.0f) - 2.0f * (east.X - 71.0f)) < 0.001f);
    CHECK(std::fabs(eastZoomedOnMap.X - eastOnMap.X) < 0.001f);
}

TEST_CASE("camera: a ground point projected to the view comes back through InverseProject at every zoom")
{
    // Two by two blocks of the retail maps' 20 vertices, all at height 0.
    MCTerrainGeometryScope geometry(20, 2, MetersPerVertex, MetersPerLevel);
    std::vector<MCPrecompVertex> vertices(5 * 400);
    MCMapBlockManager blocks(vertices, 4);

    // The view sizes of the zoom range: 480 lines closest, 1080, and 2160 furthest, at 4:3.
    for (const float height : {480.0f, 1080.0f, 2160.0f})
    {
        MCTest::Scope where(std::format("{} lines", height));
        MCCamera camera;
        camera.SetViewSize(height * 4.0f / 3.0f, height);
        camera.Position = MCVector3D(0.0f, 0.0f, 0.0f);
        camera.ScreenUL = MCTerrain::ProjectTerrain(camera.Position);
        MCTerrainWindow window(0, 30);
        blocks.Update(camera.Position, window);
        blocks.BuildWindow(window);

        // The grid as the terrain's render projects it (flat ground).
        for (int32_t i = 0; i < 30 * 30; ++i)
        {
            const MCVector3D world(window.TopLeftX + static_cast<float>(i % 30) * MetersPerVertex,
                                   window.TopLeftY - static_cast<float>(i / 30) * MetersPerVertex, 0.0f);
            const MCVector2D screen = camera.Project(world);
            window.Vertices[static_cast<size_t>(i)].Px = static_cast<int32_t>(std::floor(screen.X));
            window.Vertices[static_cast<size_t>(i)].Py = static_cast<int32_t>(std::floor(screen.Y));
        }

        camera.TerrainWindow = &window;

        for (const MCVector3D point : {MCVector3D(0.0f, 0.0f, 0.0f), MCVector3D(300.0f, 150.0f, 0.0f),
                                       MCVector3D(-400.0f, -230.0f, 0.0f), MCVector3D(510.0f, -620.0f, 0.0f)})
        {
            const MCVector2D screen = camera.Project(point);
            MCVector3D back;
            camera.InverseProject(screen, back);
            // Within the pixel the vertices were rounded to (about a metre and a half on the ground).
            CHECK(std::fabs(back.X - point.X) < 2.5f);
            CHECK(std::fabs(back.Y - point.Y) < 2.5f);
            CHECK_EQ(back.Z, 0.0f);
        }

        // The camera's position is the view's centre.
        const MCVector2D middle = camera.Project(camera.Position);
        CHECK_EQ(middle.X, camera.HalfWidth);
        CHECK_EQ(middle.Y, camera.HalfHeight);
    }
}

TEST_CASE("tacmap: the salvage list keeps every object, and drops one only once")
{
    std::array<int32_t, 150> dummies{};
    const auto object = [&dummies](size_t i) { return reinterpret_cast<MCGameObject*>(&dummies[i]); };
    MCSalvageList salvage;

    // The original's list held 100 and then hid the 100th (OB-052); the port's keeps all.
    for (size_t i = 0; i < dummies.size(); ++i)
    {
        CHECK(salvage.Add(object(i)));
    }

    CHECK(!salvage.Add(object(7)));
    CHECK_EQ(salvage.size(), size_t{150});
    CHECK_EQ(salvage[149], object(149));

    // Removing an object twice removes it once (OB-051: the original dropped its last entry instead).
    CHECK(salvage.Remove(object(149)));
    CHECK(!salvage.Remove(object(149)));
    CHECK(salvage.Remove(object(3)));
    CHECK(!salvage.Remove(object(3)));
    CHECK_EQ(salvage.size(), size_t{148});
    CHECK_EQ(salvage[3], object(4));
    CHECK_EQ(salvage[147], object(148));
}
