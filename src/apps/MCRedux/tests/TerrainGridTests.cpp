#include "stdafx.h"
#include "MCTest.h"
#include "main/main.h"
#include "terrain/terrain.h"
#include "terrain/vertex.h"

namespace
{
    constexpr int32_t BlockSide = 20;
    constexpr int32_t MapBlocks = 6;
    constexpr int32_t MapVertices = BlockSide * MapBlocks;
    constexpr float MetersPerVertex = 128.0f;

    /// <summary>
    /// A 6x6-block map (the retail maps' 20 vertices per block, 128 m per vertex) held in memory, with the terrain
    /// statics, <c>land</c> and one window set up for <c>MapBlockManager::update</c> and <c>buildWindow</c>. Every
    /// map vertex is tagged with its own index in <c>textureData</c>; the off-map block with -1.
    /// </summary>
    class TerrainGridFixture
    {
    public:
        TerrainGridFixture()
        {
            _SavedBlockSide = MCTerrain::VerticesBlockSide;
            _SavedMapSide = MCTerrain::BlocksMapSide;
            _SavedMetersPerVertex = MCTerrain::MetersPerVertex;
            _SavedOneOver = MCTerrain::OneOvermetersPerVertex;
            _SavedMetersBlockSide = MCTerrain::MetersBlockSide;
            _SavedTopLeft = MCTerrain::MapTopLeft3d100;
            _SavedVisible = MCTerrain::VisibleVerticesPerSide;
            _SavedLand = Land;
            _SavedTurn = Turn;

            MCTerrain::VerticesBlockSide = BlockSide;
            MCTerrain::BlocksMapSide = MapBlocks;
            MCTerrain::MetersPerVertex = MetersPerVertex;
            MCTerrain::OneOvermetersPerVertex = 1.0f / MetersPerVertex;
            MCTerrain::MetersBlockSide = BlockSide * MetersPerVertex;
            MCTerrain::MapTopLeft3d100 =
                MCVector3D(MapVertices * MetersPerVertex * -0.5f, MapVertices * MetersPerVertex * 0.5f, 0.0f);
            Turn = 2;

            // The terrain is never destroyed: its destructor frees the real game's heaps.
            _Land = ::new (_LandStorage) MCTerrain;
            _Land->Windows = &_Window;
            _Land->NumWindows = 1;
            Land = _Land;

            _Blocks.assign(static_cast<size_t>(MapBlocks * MapBlocks + 1) * BlockSide * BlockSide, MCPrecompVertex{});

            for (int32_t i = 0; i <= MapBlocks * MapBlocks; i++)
            {
                _BlockTable[static_cast<size_t>(i)] = &_Blocks[static_cast<size_t>(i) * BlockSide * BlockSide];
            }

            for (int32_t y = 0; y < MapVertices; y++)
            {
                for (int32_t x = 0; x < MapVertices; x++)
                {
                    mapVertex(x, y)->TextureData = static_cast<int16_t>(y * MapVertices + x);
                }
            }

            for (int32_t i = 0; i < BlockSide * BlockSide; i++)
            {
                _BlockTable[MapBlocks * MapBlocks][i].TextureData = -1;
            }

            _Manager.Blocks.assign(_BlockTable.begin(), _BlockTable.end());
            // One window's tables.
            _Manager.LastBlock.assign(1, -1);
            _Manager.CurrentBlock.assign(1, 0);
            _Manager.BlockSteps.assign(1, 0);
            _Manager.VertexOffsets.assign(2, 0.0f);
        }

        ~TerrainGridFixture()
        {
            _Land->Windows = nullptr;
            MCTerrain::VerticesBlockSide = _SavedBlockSide;
            MCTerrain::BlocksMapSide = _SavedMapSide;
            MCTerrain::MetersPerVertex = _SavedMetersPerVertex;
            MCTerrain::OneOvermetersPerVertex = _SavedOneOver;
            MCTerrain::MetersBlockSide = _SavedMetersBlockSide;
            MCTerrain::MapTopLeft3d100 = _SavedTopLeft;
            MCTerrain::VisibleVerticesPerSide = _SavedVisible;
            Land = _SavedLand;
            Turn = _SavedTurn;
        }

        TerrainGridFixture(const TerrainGridFixture&) = delete;
        TerrainGridFixture& operator=(const TerrainGridFixture&) = delete;

        /// <summary>The stored vertex at map column <paramref name="x"/>, row <paramref name="y"/>.</summary>
        MCPrecompVertex* mapVertex(int32_t x, int32_t y)
        {
            return _BlockTable[static_cast<size_t>((y / BlockSide) * MapBlocks + x / BlockSide)] +
                   (y % BlockSide) * BlockSide + x % BlockSide;
        }

        /// <summary>
        /// Builds a <paramref name="side"/>-vertex grid for a camera over map vertex (<paramref name="cameraX"/>,
        /// <paramref name="cameraY"/>) and checks that every grid vertex is the map vertex it stands for, or an
        /// off-map one past the map's edge.
        /// </summary>
        void checkGrid(int32_t side, int32_t cameraX, int32_t cameraY)
        {
            MCTest::Scope scope(std::format("grid {} with the camera over vertex ({}, {})", side, cameraX, cameraY));
            MCTerrain::VisibleVerticesPerSide = side;
            std::vector<MCVertex> vertices(static_cast<size_t>(side) * side);
            std::vector<MCTerrainBlock> blocks(static_cast<size_t>(side) * side);
            MCVector3D camera(MCTerrain::MapTopLeft3d100.X + (static_cast<float>(cameraX) + 0.5f) * MetersPerVertex,
                              MCTerrain::MapTopLeft3d100.Y - (static_cast<float>(cameraY) + 0.5f) * MetersPerVertex,
                              0.0f);
            REQUIRE_EQ(_Manager.Update(camera, 0), 0);
            int32_t numVertices = 0;
            int32_t numBlocks = 0;
            _Manager.BuildWindow(vertices.data(), &numVertices, blocks.data(), &numBlocks, 0);
            REQUIRE_EQ(numVertices, side * side);

            // The grid's corner is half a grid before the camera's vertex, as the window's world corner says.
            const int32_t cornerX = cameraX - side / 2;
            const int32_t cornerY = cameraY - side / 2;
            CHECK_EQ(_Window.TopLeftX, MCTerrain::MapTopLeft3d100.X + static_cast<float>(cornerX) * MetersPerVertex);
            CHECK_EQ(_Window.TopLeftY, MCTerrain::MapTopLeft3d100.Y - static_cast<float>(cornerY) * MetersPerVertex);

            int32_t wrong = 0;

            for (int32_t row = 0; row < side; row++)
            {
                for (int32_t col = 0; col < side; col++)
                {
                    const int32_t x = cornerX + col;
                    const int32_t y = cornerY + row;
                    const bool onMap = x >= 0 && y >= 0 && x < MapVertices && y < MapVertices;
                    const int16_t expected = onMap ? static_cast<int16_t>(y * MapVertices + x) : int16_t{-1};

                    if (vertices[static_cast<size_t>(row) * side + col].PVertex->TextureData != expected)
                    {
                        wrong++;
                    }
                }
            }

            CHECK_EQ(wrong, 0);
        }

    private:
        MCMapBlockManager _Manager;
        MCTerrainWindow _Window;
        alignas(MCTerrain) std::byte _LandStorage[sizeof(MCTerrain)]{};
        MCTerrain* _Land = nullptr;
        std::vector<MCPrecompVertex> _Blocks;
        std::array<MCPrecompVertex*, MapBlocks * MapBlocks + 1> _BlockTable{};

        int32_t _SavedBlockSide = 0;
        int32_t _SavedMapSide = 0;
        float _SavedMetersPerVertex = 0.0f;
        float _SavedOneOver = 0.0f;
        float _SavedMetersBlockSide = 0.0f;
        MCVector3D _SavedTopLeft;
        int32_t _SavedVisible = 0;
        MCTerrain* _SavedLand = nullptr;
        int32_t _SavedTurn = 0;
    };

    /// <summary>
    /// Camera vertices across the map: the edges, each side of a block edge, and the middle. Not past column 114:
    /// there, in the last block column from its vertex 15 on, MCX.EXE's buildWindow steps the corner's block by one
    /// and wraps into the next block row, so the whole grid is off. The camera's edge margin (about ten vertices)
    /// keeps it from getting there.
    /// </summary>
    constexpr int32_t CameraVertices[] = {0, 3, 14, 15, 19, 20, 24, 35, 39, 40, 59, 60, 85, 99, 100, 105, 110, 114};
}

TEST_CASE("terrain: the data's 30-vertex grid holds the map vertices around the camera")
{
    TerrainGridFixture fixture;

    for (int32_t y : CameraVertices)
    {
        for (int32_t x : CameraVertices)
        {
            fixture.checkGrid(30, x, y);
        }
    }
}

TEST_CASE("terrain: a grid grown for a larger screen holds the map vertices around the camera")
{
    TerrainGridFixture fixture;

    for (int32_t side : {32, 42, 64, 80})
    {
        for (int32_t y : CameraVertices)
        {
            for (int32_t x : CameraVertices)
            {
                fixture.checkGrid(side, x, y);
            }
        }
    }
}
