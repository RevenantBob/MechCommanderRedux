#include "stdafx.h"
#include "EditorTool.h"
#include "EditorPalette.h"

namespace
{
    /// <summary>The bytes the editor copies of a BITMAPINFO with 256 colours.</summary>
    inline constexpr size_t BITMAPINFO_256_SIZE = 0x428;
    /// <summary>A cursor pixel this close to magenta is transparent.</summary>
    inline constexpr BYTE MAGENTA_MIN = 0xfb;
    inline constexpr BYTE MAGENTA_GREEN_MAX = 4;
    /// <summary>The colour index of a transparent cursor pixel.</summary>
    inline constexpr uint8_t TRANSPARENT_PIXEL = 0xff;
}

bool EditorTool::canApply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                          [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool EditorTool::canApplyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                               [[maybe_unused]] ToolKeys keys)
{
    return true;
}

bool EditorTool::hasTarget([[maybe_unused]] EditorMap& map)
{
    return true;
}

const uint8_t* EditorTool::cursorImage([[maybe_unused]] EditorMap& map, [[maybe_unused]] const POINT& position,
                                       [[maybe_unused]] POINT* topLeft, [[maybe_unused]] SIZE* size,
                                       [[maybe_unused]] int32_t zoom)
{
    return nullptr;
}

const uint8_t* EditorTool::notAllowedImage([[maybe_unused]] EditorMap& map, const POINT& position, POINT* topLeft,
                                           SIZE* size, [[maybe_unused]] int32_t zoom)
{
    *topLeft = position;
    CSize signSize;
    const uint8_t* bits = editorPalette->getNotAllowedBits(signSize);
    *size = signSize;
    return bits;
}

void EditorTool::track([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                       [[maybe_unused]] ToolKeys keys)
{
}

CursorTool::CursorTool(UINT bitmapId, int32_t hotX, int32_t hotY) : hotX(hotX), hotY(hotY)
{
    const HDC dc = GetDC(nullptr);
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    auto* info = reinterpret_cast<BITMAPINFO*>(new uint8_t[BITMAPINFO_256_SIZE]);
    std::memcpy(info, editorPalette->bitmapInfo, BITMAPINFO_256_SIZE);
    info->bmiHeader.biWidth = CURSOR_SIZE;
    info->bmiHeader.biHeight = -CURSOR_SIZE;
    bitmap = static_cast<HBITMAP>(
        LoadImageA(instance, MAKEINTRESOURCEA(bitmapId & 0xffff), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    // The bitmap's own colour indices come back, and its colour table into the header.
    GetDIBits(dc, bitmap, 0, CURSOR_SIZE, image, info, DIB_RGB_COLORS);
    std::memcpy(redImage, image, sizeof(image));
    const uint8_t* red = editorPalette->redTint;

    for (size_t i = 0; i < sizeof(image); i++)
    {
        const RGBQUAD& color = info->bmiColors[image[i]];

        if (color.rgbRed < MAGENTA_MIN || color.rgbBlue < MAGENTA_MIN || color.rgbGreen > MAGENTA_GREEN_MAX)
        {
            redImage[i] = red[image[i]];
        }
        else
        {
            image[i] = TRANSPARENT_PIXEL;
            redImage[i] = TRANSPARENT_PIXEL;
        }
    }

    ReleaseDC(nullptr, dc);
    delete[] reinterpret_cast<uint8_t*>(info);
}

CursorTool::~CursorTool()
{
    DeleteObject(bitmap);
}

bool CursorTool::begin([[maybe_unused]] EditorMap& map)
{
    return false;
}

EditorAction* CursorTool::end()
{
    return nullptr;
}

bool CursorTool::apply([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                       [[maybe_unused]] ToolKeys keys)
{
    return false;
}

bool CursorTool::applyAgain([[maybe_unused]] EditorMap& map, [[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                            [[maybe_unused]] ToolKeys keys)
{
    return false;
}

const uint8_t* CursorTool::cursorImage([[maybe_unused]] EditorMap& map, const POINT& position, POINT* topLeft,
                                       SIZE* size, [[maybe_unused]] int32_t zoom)
{
    size->cx = CURSOR_SIZE;
    size->cy = CURSOR_SIZE;
    topLeft->x = position.x - hotX + CURSOR_SIZE / 2;
    topLeft->y = position.y - hotY + CURSOR_SIZE / 2;
    return image;
}

const uint8_t* CursorTool::notAllowedImage([[maybe_unused]] EditorMap& map, const POINT& position, POINT* topLeft,
                                           SIZE* size, [[maybe_unused]] int32_t zoom)
{
    size->cx = CURSOR_SIZE;
    size->cy = CURSOR_SIZE;
    topLeft->x = position.x - hotX + CURSOR_SIZE / 2;
    topLeft->y = position.y - hotY + CURSOR_SIZE / 2;
    return redImage;
}
