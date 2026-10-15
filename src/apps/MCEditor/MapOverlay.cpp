#include "stdafx.h"
#include "MapOverlay.h"
#include "TileLibrary.h"
#include "vfx/vfxnshape.h"

MapOverlay::MapOverlay()
{
    std::fill(std::begin(kinds), std::end(kinds), static_cast<int32_t>(OVERLAY_NONE));
}

MapOverlay::MapOverlay(const MapOverlay& other) : tinted(other.tinted), tileId(other.tileId), damaged(other.damaged)
{
    std::copy(std::begin(other.kinds), std::end(other.kinds), kinds);
}

MapOverlay::MapOverlay(const int32_t* kinds, int32_t tileId, bool damaged) : tileId(tileId), damaged(damaged)
{
    std::copy(kinds, kinds + 4, this->kinds);
}

bool MapOverlay::set(const int32_t* kinds, const uint8_t* heights, bool damaged)
{
    const int32_t* tile = tileLibrary->findOverlayTile(kinds, heights, damaged);

    if (tile == nullptr)
    {
        return false;
    }

    tileId = *tile;
    std::fill(std::begin(zoomArt), std::end(zoomArt), nullptr);
    this->damaged = damaged;
    std::copy(kinds, kinds + 4, this->kinds);
    return true;
}

void MapOverlay::draw(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, const uint8_t* xlat,
                      int32_t zoom)
{
    if (zoomArt[zoom] == nullptr)
    {
        if (zoom == 0)
        {
            zoomArt[0] = tileLibrary->getTileData(tileId);
        }
        else if (zoom == 1)
        {
            zoomArt[1] = tileLibrary->getMediumTileData(tileId);
        }
        else if (zoom == 2)
        {
            zoomArt[2] = tileLibrary->getSmallTileData(tileId, true);
        }
    }

    // The original draws from the null pointer of an overlay without art.
    if (zoomArt[zoom] == nullptr)
    {
        return;
    }

    VFX_nShape_draw(buffer, width, height, zoomArt[zoom], 0, x, y, tinted != 0 ? xlat : nullptr);
}

bool MapOverlay::setCorner(int32_t corner, int32_t kind, const uint8_t* heights)
{
    kinds[corner] = kind;
    const int32_t* tile = tileLibrary->findOverlayTile(kinds, heights, damaged);

    if (tile == nullptr)
    {
        return false;
    }

    tileId = *tile;
    std::fill(std::begin(zoomArt), std::end(zoomArt), nullptr);
    return true;
}
