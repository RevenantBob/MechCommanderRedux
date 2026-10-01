#pragma once

/// <summary>
/// An 8-bit cursor picture: palette indices, which of them show, and the hot spot (the pixel the mouse position is
/// on). The hot spot is always inside the picture.
/// </summary>
struct MCCursorImage
{
    int Width = 0;
    int Height = 0;
    int HotX = 0;
    int HotY = 0;
    /// <summary>Palette indices, <see cref="Width"/> per row.</summary>
    std::vector<uint8_t> Pixels;
    /// <summary>1 where the pixel shows, 0 where it is transparent; as <see cref="Pixels"/>.</summary>
    std::vector<uint8_t> Opaque;

    bool operator==(const MCCursorImage&) const = default;

    /// <summary>A transparent picture of <paramref name="width"/> x <paramref name="height"/>.</summary>
    static MCCursorImage Blank(int width, int height, int hotX, int hotY);

    /// <summary>
    /// <paramref name="over"/> drawn on <paramref name="under"/> with their hot spots on the same point; the result
    /// is as large as both need.
    /// </summary>
    static MCCursorImage Overlay(const MCCursorImage& under, const MCCursorImage& over);

    /// <summary>The picture grown with transparent pixels until it holds its hot spot.</summary>
    static MCCursorImage WithHotSpotInside(const MCCursorImage& image);
};

/// <summary>A cursor picture in colour at the size it is shown: RGBA bytes, <see cref="Width"/> * 4 per row.</summary>
struct MCCursorBitmap
{
    int Width = 0;
    int Height = 0;
    int HotX = 0;
    int HotY = 0;
    std::vector<uint8_t> Rgba;
};

/// <summary>
/// The game's cursor shown as the system (hardware) cursor, so it follows the mouse at the desktop's rate instead
/// of being drawn into the frame. The picture is scaled as the screen is (nearest pixel) and coloured with the
/// display's palette.
/// </summary>
/// <remarks>Threads: the thread that created the display (as <see cref="MCDisplay::Present"/>).</remarks>
namespace MCCursor
{
    /// <summary>
    /// Makes <paramref name="image"/> the cursor over the attached display's window. Cheap to call every frame: a new
    /// SDL cursor is made only when the picture, its colours or the screen's scale changed.
    /// </summary>
    void Show(const MCCursorImage& image);

    /// <summary>Shows no game cursor (the game hid its own).</summary>
    void Hide();

    /// <summary>Frees the SDL cursor (before the display goes).</summary>
    void Shutdown();

    /// <summary>
    /// <paramref name="image"/> in <paramref name="colors"/> scaled by <paramref name="scaleX"/> x
    /// <paramref name="scaleY"/> (nearest pixel), transparent pixels alpha 0. The hot spot lands on the top-left of
    /// its scaled pixel.
    /// </summary>
    MCCursorBitmap Rasterize(const MCCursorImage& image, const SDL_Color* colors, float scaleX, float scaleY);
}
