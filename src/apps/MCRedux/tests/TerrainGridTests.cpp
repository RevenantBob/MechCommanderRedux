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
            _SavedBlockSide = Terrain::verticesBlockSide;
            _SavedMapSide = Terrain::blocksMapSide;
            _SavedMetersPerVertex = Terrain::metersPerVertex;
            _SavedOneOver = Terrain::OneOvermetersPerVertex;
            _SavedMetersBlockSide = Terrain::metersBlockSide;
            _SavedTopLeft = Terrain::mapTopLeft3d100;
            _SavedVisible = Terrain::visibleVerticesPerSide;
            _SavedLand = land;
            _SavedTurn = turn;

            Terrain::verticesBlockSide = BlockSide;
            Terrain::blocksMapSide = MapBlocks;
            Terrain::metersPerVertex = MetersPerVertex;
            Terrain::OneOvermetersPerVertex = 1.0f / MetersPerVertex;
            Terrain::metersBlockSide = BlockSide * MetersPerVertex;
            Terrain::mapTopLeft3d100 =
                vector_3d(MapVertices * MetersPerVertex * -0.5f, MapVertices * MetersPerVertex * 0.5f, 0.0f);
            turn = 2;

            // The terrain is never destroyed: its destructor frees the real game's heaps.
            _Land = ::new (_LandStorage) Terrain;
            _Land->windows = &_Window;
            _Land->numWindows = 1;
            land = _Land;

            _Blocks.assign(static_cast<size_t>(MapBlocks * MapBlocks + 1) * BlockSide * BlockSide, PrecompVertex{});

            for (int32_t i = 0; i <= MapBlocks * MapBlocks; i++)
            {
                _BlockTable[static_cast<size_t>(i)] = &_Blocks[static_cast<size_t>(i) * BlockSide * BlockSide];
            }

            for (int32_t y = 0; y < MapVertices; y++)
            {
                for (int32_t x = 0; x < MapVertices; x++)
                {
                    mapVertex(x, y)->textureData = static_cast<int16_t>(y * MapVertices + x);
                }
            }

            for (int32_t i = 0; i < BlockSide * BlockSide; i++)
            {
                _BlockTable[MapBlocks * MapBlocks][i].textureData = -1;
            }

            _Manager.blocks.assign(_BlockTable.begin(), _BlockTable.end());
            // One window's tables.
            _Manager.lastBlock.assign(1, -1);
            _Manager.currentBlock.assign(1, 0);
            _Manager.blockSteps.assign(1, 0);
            _Manager.vertexOffsets.assign(2, 0.0f);
        }

        ~TerrainGridFixture()
        {
            _Land->windows = nullptr;
            Terrain::verticesBlockSide = _SavedBlockSide;
            Terrain::blocksMapSide = _SavedMapSide;
            Terrain::metersPerVertex = _SavedMetersPerVertex;
            Terrain::OneOvermetersPerVertex = _SavedOneOver;
            Terrain::metersBlockSide = _SavedMetersBlockSide;
            Terrain::mapTopLeft3d100 = _SavedTopLeft;
            Terrain::visibleVerticesPerSide = _SavedVisible;
            land = _SavedLand;
            turn = _SavedTurn;
        }

        TerrainGridFixture(const TerrainGridFixture&) = delete;
        TerrainGridFixture& operator=(const TerrainGridFixture&) = delete;

        /// <summary>The stored vertex at map column <paramref name="x"/>, row <paramref name="y"/>.</summary>
        PrecompVertex* mapVertex(int32_t x, int32_t y)
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
            Terrain::visibleVerticesPerSide = side;
            std::vector<Vertex> vertices(static_cast<size_t>(side) * side);
            std::vector<TerrainBlock> blocks(static_cast<size_t>(side) * side);
            vector_3d camera(Terrain::mapTopLeft3d100.x + (static_cast<float>(cameraX) + 0.5f) * MetersPerVertex,
                             Terrain::mapTopLeft3d100.y - (static_cast<float>(cameraY) + 0.5f) * MetersPerVertex, 0.0f);
            REQUIRE_EQ(_Manager.update(camera, 0), 0);
            int32_t numVertices = 0;
            int32_t numBlocks = 0;
            _Manager.buildWindow(vertices.data(), &numVertices, blocks.data(), &numBlocks, 0);
            REQUIRE_EQ(numVertices, side * side);

            // The grid's corner is half a grid before the camera's vertex, as the window's world corner says.
            const int32_t cornerX = cameraX - side / 2;
            const int32_t cornerY = cameraY - side / 2;
            CHECK_EQ(_Window.topLeftX, Terrain::mapTopLeft3d100.x + static_cast<float>(cornerX) * MetersPerVertex);
            CHECK_EQ(_Window.topLeftY, Terrain::mapTopLeft3d100.y - static_cast<float>(cornerY) * MetersPerVertex);

            int32_t wrong = 0;

            for (int32_t row = 0; row < side; row++)
            {
                for (int32_t col = 0; col < side; col++)
                {
                    const int32_t x = cornerX + col;
                    const int32_t y = cornerY + row;
                    const bool onMap = x >= 0 && y >= 0 && x < MapVertices && y < MapVertices;
                    const int16_t expected = onMap ? static_cast<int16_t>(y * MapVertices + x) : int16_t{-1};

                    if (vertices[static_cast<size_t>(row) * side + col].pVertex->textureData != expected)
                    {
                        wrong++;
                    }
                }
            }

            CHECK_EQ(wrong, 0);
        }

    private:
        MapBlockManager _Manager;
        TerrainWindow _Window;
        alignas(Terrain) std::byte _LandStorage[sizeof(Terrain)]{};
        Terrain* _Land = nullptr;
        std::vector<PrecompVertex> _Blocks;
        std::array<PrecompVertex*, MapBlocks * MapBlocks + 1> _BlockTable{};

        int32_t _SavedBlockSide = 0;
        int32_t _SavedMapSide = 0;
        float _SavedMetersPerVertex = 0.0f;
        float _SavedOneOver = 0.0f;
        float _SavedMetersBlockSide = 0.0f;
        vector_3d _SavedTopLeft;
        int32_t _SavedVisible = 0;
        Terrain* _SavedLand = nullptr;
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
