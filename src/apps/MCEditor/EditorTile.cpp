#include "stdafx.h"
#include "EditorTile.h"
#include "MapOverlay.h"
#include "TileLibrary.h"
#include "vfx/vfxtile.h"

int32_t tileWorldWidth = 79;
// Original behaviour: a static initializer (0x00472970) copies the width.
int32_t tileWorldHeight = tileWorldWidth;
int32_t elevationScale = 14;
int32_t defaultElevation = 4;

EditorTile::EditorTile()
{
    for (int32_t i = 0; i < 4; i++)
    {
        elevations[i] = defaultElevation;
        textures[i] = defaultElevation == 0 ? TEXTURE_WATER : TileLibrary::getDefaultTexture();
    }

    uint8_t heights[4];
    getHeights(heights);
    tileId = tileLibrary->pickTile(textures, heights, -1);
    // The original releases its temporary copy of the reference too.
    tileLibrary->releaseTile(tileId);
}

EditorTile::~EditorTile()
{
    delete overlay;

    if (tileLibrary != nullptr)
    {
        tileLibrary->releaseTile(tileId);
    }
}

EditorTile::EditorTile(const EditorTile& other)
{
    assign(other);
}

EditorTile& EditorTile::operator=(const EditorTile& other)
{
    if (&other != this)
    {
        assign(other);
    }

    return *this;
}

void EditorTile::assign(const EditorTile& other)
{
    for (int32_t i = 0; i < 4; i++)
    {
        elevations[i] = other.elevations[i];
        textures[i] = other.textures[i];
    }

    tileId = other.tileId;
    std::fill(std::begin(zoomArt), std::end(zoomArt), nullptr);
    delete overlay;
    overlay = other.overlay != nullptr ? new MapOverlay(*other.overlay) : nullptr;
    concIndex = other.concIndex;
    cropIndex = other.cropIndex;
}

uint32_t EditorTile::drawArt(uint8_t* buffer, int32_t width, int32_t height, int32_t x, int32_t y, const uint8_t* xlat,
                             int32_t outline, int32_t zoom)
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
            zoomArt[2] = tileLibrary->getSmallTileData(tileId, false);
        }
    }

    // The original draws from the null pointer of a tile without art.
    if (zoomArt[zoom] == nullptr)
    {
        return VFX_TILE_OUTSIDE;
    }

    return VFX_nTile_draw(buffer, width, height, zoomArt[zoom], x, y, xlat, outline);
}

namespace
{
    /// <summary>The corners' heights above the lowest one, without the check getHeights makes.</summary>
    void cornerHeights(const int32_t* elevations, uint8_t* heights)
    {
        int32_t lowest = elevations[3];

        for (int32_t i = 0; i < 3; i++)
        {
            lowest = (std::min)(lowest, elevations[i]);
        }

        for (int32_t i = 0; i < 4; i++)
        {
            heights[i] = static_cast<uint8_t>(elevations[i] - lowest);
        }
    }

    /// <summary>The variant <see cref="TileLibrary::pickTile"/> takes for a random one.</summary>
    inline constexpr int32_t RANDOM_VARIANT = -1;
}

bool EditorTile::setTextures(const uint8_t* newTextures)
{
    uint8_t picked[4];
    int32_t roughCorners = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        picked[i] = newTextures[i] == KEEP_TEXTURE ? textures[i] : newTextures[i];

        if (picked[i] == TEXTURE_RUFF)
        {
            picked[i] = TEXTURE_GRASS;
            roughCorners++;
        }

        if (elevations[i] == 0)
        {
            picked[i] = TEXTURE_WATER;
        }
    }

    if (roughCorners == 4)
    {
        std::fill(std::begin(picked), std::end(picked), static_cast<uint8_t>(TEXTURE_RUFF));
    }

    std::copy(std::begin(picked), std::end(picked), textures);
    uint8_t heights[4];
    cornerHeights(elevations, heights);

    if (concIndex != -1 || cropIndex != -1)
    {
        cropIndex = -1;
        concIndex = -1;
    }

    const int32_t id = tileLibrary->pickTile(textures, heights, RANDOM_VARIANT);

    if (id == NO_TILE)
    {
        return false;
    }

    tileId = id;
    std::fill(std::begin(zoomArt), std::end(zoomArt), nullptr);
    // The original's temporary reference to the tile goes.
    tileLibrary->releaseTile(id);
    return true;
}

bool EditorTile::setTextures(const uint8_t* newTextures, bool* changed)
{
    *changed = false;

    for (int32_t i = 0; i < 4; i++)
    {
        if (newTextures[i] != KEEP_TEXTURE && newTextures[i] != textures[i])
        {
            *changed = true;
        }
    }

    if (concIndex != -1 || cropIndex != -1)
    {
        cropIndex = -1;
        concIndex = -1;
        *changed = true;
    }

    return setTextures(newTextures);
}

bool EditorTile::setElevations(const int32_t* newElevations)
{
    uint8_t picked[4];

    for (int32_t i = 0; i < 4; i++)
    {
        if (newElevations[i] != KEEP_ELEVATION)
        {
            elevations[i] = newElevations[i];
        }

        picked[i] = elevations[i] == 0 ? static_cast<uint8_t>(TEXTURE_WATER) : textures[i];
    }

    uint8_t heights[4];
    bool fits = getHeights(heights);

    if (fits)
    {
        const int32_t id = tileLibrary->pickTile(picked, heights, RANDOM_VARIANT);

        if (id == NO_TILE)
        {
            fits = false;
        }
        else
        {
            tileId = id;
            std::fill(std::begin(zoomArt), std::end(zoomArt), nullptr);
            tileLibrary->releaseTile(id);
        }
    }

    concIndex = -1;
    cropIndex = -1;
    return fits;
}

bool EditorTile::setTerrain(const uint8_t* newTextures, const int32_t* newElevations, bool* changed)
{
    bool textureChanged = false;

    for (int32_t i = 0; i < 4; i++)
    {
        if (newTextures[i] != KEEP_TEXTURE && newTextures[i] != textures[i])
        {
            textureChanged = true;
        }
    }

    if (concIndex != -1 || cropIndex != -1)
    {
        cropIndex = -1;
        concIndex = -1;
        textureChanged = true;
    }

    const bool texturesFit = setTextures(newTextures);
    *changed = textureChanged;

    if (!textureChanged)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            if (newElevations[i] != KEEP_ELEVATION && newElevations[i] != elevations[i])
            {
                *changed = true;
                break;
            }
        }
    }

    const bool elevationsFit = setElevations(newElevations);
    return elevationsFit && texturesFit;
}

bool EditorTile::setOverlay(const int32_t* kinds, bool damaged)
{
    const bool any = std::any_of(kinds, kinds + 4, [](int32_t kind) { return kind != OVERLAY_NONE; });

    if (!any)
    {
        delete overlay;
        overlay = nullptr;
        return false;
    }

    if (overlay == nullptr)
    {
        overlay = new MapOverlay();
    }

    uint8_t heights[4];
    cornerHeights(elevations, heights);
    return overlay->set(kinds, heights, damaged);
}

void EditorTile::setOverlayCorner(int32_t corner, int32_t kind)
{
    if (overlay == nullptr)
    {
        overlay = new MapOverlay();
    }

    uint8_t heights[4];
    cornerHeights(elevations, heights);
    overlay->setCorner(corner, kind, heights);
}

bool EditorTile::canSetOverlayCorner(int32_t corner, int32_t kind) const
{
    int32_t kinds[4] = {OVERLAY_NONE, OVERLAY_NONE, OVERLAY_NONE, OVERLAY_NONE};
    bool damaged = false;

    if (overlay != nullptr)
    {
        std::copy(std::begin(overlay->kinds), std::end(overlay->kinds), kinds);
        damaged = overlay->damaged;
    }

    uint8_t heights[4];
    cornerHeights(elevations, heights);

    for (int32_t i = 0; i < 4; i++)
    {
        if (heights[i] > 1)
        {
            return false;
        }
    }

    kinds[corner] = kind;
    return tileLibrary->findOverlayTile(kinds, heights, damaged) != nullptr;
}

bool EditorTile::hasOverlayFor(const int32_t* kinds, bool damaged) const
{
    uint8_t heights[4];
    cornerHeights(elevations, heights);
    return tileLibrary->findOverlayTile(kinds, heights, damaged) != nullptr;
}

bool EditorTile::hasTexture(uint8_t texture) const
{
    if (concIndex != -1)
    {
        return texture == TEXTURE_CONC;
    }

    if (cropIndex != -1)
    {
        return texture == TEXTURE_CROP;
    }

    for (int32_t i = 0; i < 4; i++)
    {
        if (textures[i] == texture || (elevations[i] == 0 && texture == TEXTURE_WATER))
        {
            return true;
        }
    }

    return false;
}

bool EditorTile::getHeights(uint8_t* heights) const
{
    int32_t lowest = elevations[3];

    for (int32_t i = 0; i < 3; i++)
    {
        lowest = (std::min)(lowest, elevations[i]);
    }

    bool fits = true;

    for (int32_t i = 0; i < 4; i++)
    {
        heights[i] = static_cast<uint8_t>(elevations[i] - lowest);

        if (heights[i] > 2)
        {
            fits = false;
        }
    }

    return fits;
}
