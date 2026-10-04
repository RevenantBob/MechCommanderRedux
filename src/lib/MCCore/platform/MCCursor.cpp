#include "stdafx.h"
#include "platform/MCCursor.h"
#include "platform/MCDisplay.h"
#include "platform/MCFrameLog.h"
#include "platform/MCInput.h"

MCCursorImage MCCursorImage::Blank(int width, int height, int hotX, int hotY)
{
    MCCursorImage image;
    image.Width = std::max(width, 0);
    image.Height = std::max(height, 0);
    image.HotX = hotX;
    image.HotY = hotY;
    image.Pixels.assign(static_cast<size_t>(image.Width) * image.Height, 0);
    image.Opaque.assign(image.Pixels.size(), 0);
    return image;
}

MCCursorImage MCCursorImage::Overlay(const MCCursorImage& under, const MCCursorImage& over)
{
    // Both pictures relative to the shared hot spot.
    const int left = std::min(-under.HotX, -over.HotX);
    const int top = std::min(-under.HotY, -over.HotY);
    const int right = std::max(under.Width - under.HotX, over.Width - over.HotX);
    const int bottom = std::max(under.Height - under.HotY, over.Height - over.HotY);
    MCCursorImage result = Blank(right - left, bottom - top, -left, -top);

    for (const MCCursorImage* layer : {&under, &over})
    {
        const int originX = result.HotX - layer->HotX;
        const int originY = result.HotY - layer->HotY;

        for (int y = 0; y < layer->Height; ++y)
        {
            for (int x = 0; x < layer->Width; ++x)
            {
                const size_t from = static_cast<size_t>(y) * layer->Width + x;

                if (layer->Opaque[from] == 0)
                {
                    continue;
                }

                const size_t to = static_cast<size_t>(originY + y) * result.Width + originX + x;
                result.Pixels[to] = layer->Pixels[from];
                result.Opaque[to] = 1;
            }
        }
    }

    return result;
}

MCCursorImage MCCursorImage::WithHotSpotInside(const MCCursorImage& image)
{
    if (image.HotX >= 0 && image.HotY >= 0 && image.HotX < image.Width && image.HotY < image.Height)
    {
        return image;
    }

    return Overlay(image, Blank(1, 1, 0, 0));
}

namespace
{
    /// <summary>An SDL cursor made for a picture, in the colours it uses.</summary>
    struct MCMadeCursor
    {
        SDL_Cursor* Cursor = nullptr;
        /// <summary>The colours of the indices the picture shows; the others are zero.</summary>
        std::array<SDL_Color, 256> Colors{};
    };

    struct MCCursorState
    {
        /// <summary>The game's cursor shapes (<see cref="MCCursor::Preload"/>).</summary>
        std::vector<MCCursorImage> Shapes;
        /// <summary>Each shape's cursor, at the current scale; null until made.</summary>
        std::vector<MCMadeCursor> ShapeCursors;
        /// <summary>The last other picture shown (a dragged item), remade when it changes.</summary>
        MCCursorImage OtherImage;
        MCMadeCursor Other;
        SDL_Cursor* Shown = nullptr;
        /// <summary>Cursors no longer wanted, freed once another is shown (SDL would show its arrow in between).</summary>
        std::vector<SDL_Cursor*> Retired;
        /// <summary>
        /// Made but not yet shown over the window, where Windows builds the cursor (see warm). Recoloured cursors of
        /// shapes already shown aren't added (see ensure).
        /// </summary>
        std::vector<SDL_Cursor*> Cold;
        float ScaleX = 0.0f;
        float ScaleY = 0.0f;
        float Density = 0.0f;
        /// <summary>The shown colours of the previous call, to tell a settled palette from a fade.</summary>
        std::array<SDL_Color, 256> LastColors{};
        /// <summary>The colours every shape was last made in; false when a shape may be missing.</summary>
        std::array<SDL_Color, 256> ReadyColors{};
        bool Ready = false;
        uint64_t MadeCount = 0;
    };

    MCCursorState& state()
    {
        static MCCursorState instance;
        return instance;
    }

    /// <summary>A bitmap as an RGBA surface, or null.</summary>
    SDL_Surface* toSurface(const MCCursorBitmap& bitmap)
    {
        SDL_Surface* surface = SDL_CreateSurface(bitmap.Width, bitmap.Height, SDL_PIXELFORMAT_RGBA32);

        if (surface == nullptr)
        {
            return nullptr;
        }

        for (int y = 0; y < bitmap.Height; ++y)
        {
            std::memcpy(static_cast<uint8_t*>(surface->pixels) + static_cast<ptrdiff_t>(y) * surface->pitch,
                        bitmap.Rgba.data() + static_cast<size_t>(y) * bitmap.Width * 4,
                        static_cast<size_t>(bitmap.Width) * 4);
        }

        return surface;
    }

    bool sameColors(const std::array<SDL_Color, 256>& a, const std::array<SDL_Color, 256>& b)
    {
        return std::memcmp(a.data(), b.data(), sizeof(SDL_Color) * 256) == 0;
    }

    /// <summary>
    /// <paramref name="colors"/> kept only at the indices <paramref name="image"/> shows, so a palette change it
    /// doesn't use (the water cycle) doesn't remake its cursor.
    /// </summary>
    std::array<SDL_Color, 256> usedColors(const MCCursorImage& image, const std::array<SDL_Color, 256>& colors)
    {
        std::array<SDL_Color, 256> used{};

        for (size_t i = 0; i < image.Pixels.size(); ++i)
        {
            if (image.Opaque[i] != 0)
            {
                used[image.Pixels[i]] = colors[image.Pixels[i]];
            }
        }

        return used;
    }

    /// <summary>A new SDL cursor of <paramref name="picture"/>, or null.</summary>
    SDL_Cursor* makeCursor(const MCCursorImage& picture, const SDL_Color* colors, float scaleX, float scaleY,
                           float density)
    {
        // The surface is the picture at 100% display scale (window points); on a high-density display the pixel
        // sized one goes with it as an alternate image, which SDL picks there.
        const MCCursorImage image = MCCursorImage::WithHotSpotInside(picture);
        const MCCursorBitmap base = MCCursor::Rasterize(image, colors, scaleX / density, scaleY / density);
        SDL_Surface* surface = toSurface(base);

        if (surface == nullptr)
        {
            return nullptr;
        }

        if (density != 1.0f)
        {
            SDL_Surface* dense = toSurface(MCCursor::Rasterize(image, colors, scaleX, scaleY));

            if (dense != nullptr)
            {
                SDL_AddSurfaceAlternateImage(surface, dense);
                SDL_DestroySurface(dense);
            }
        }

        SDL_Cursor* made = SDL_CreateColorCursor(surface, base.HotX, base.HotY);
        SDL_DestroySurface(surface);

        if (made == nullptr)
        {
            SDL_Log("MCCursor: SDL_CreateColorCursor failed: %s", SDL_GetError());
        }

        return made;
    }

    /// <summary>Retires <paramref name="made"/>'s cursor, if any, and empties it.</summary>
    void retire(MCCursorState& cursor, MCMadeCursor& made)
    {
        if (made.Cursor != nullptr)
        {
            cursor.Retired.push_back(made.Cursor);
            std::erase(cursor.Cold, made.Cursor);
        }

        made = {};
    }

    /// <summary>
    /// Makes <paramref name="made"/> show <paramref name="image"/> in <paramref name="colors"/> unless it already
    /// does; false if SDL couldn't.
    /// </summary>
    bool ensure(MCCursorState& cursor, MCMadeCursor& made, const MCCursorImage& image,
                const std::array<SDL_Color, 256>& colors)
    {
        const std::array<SDL_Color, 256> used = usedColors(image, colors);

        if (made.Cursor != nullptr && sameColors(made.Colors, used))
        {
            return true;
        }

        MCFrameLog::Scope part("cursor.make");
        SDL_Cursor* fresh = makeCursor(image, colors.data(), cursor.ScaleX, cursor.ScaleY, cursor.Density);

        if (MCFrameLog::Enabled())
        {
            MCFrameLog::Note(std::format("cursor made, {}x{} picture{}", image.Width, image.Height,
                                         made.Cursor != nullptr ? " (remade: new colours)" : ""));
        }

        if (fresh == nullptr)
        {
            return false;
        }

        // Only a first cursor for the picture (or one replacing a cursor never shown) is warmed. Warming shows each
        // cold cursor over the window, so warming every shape recoloured by a palette change (the brightness slider
        // changes it with every mouse move) would flash the other shapes on screen; a recoloured shape builds its
        // icon when it is next shown.
        const bool wasCold = made.Cursor == nullptr || std::ranges::find(cursor.Cold, made.Cursor) != cursor.Cold.end();
        retire(cursor, made);
        made = {fresh, used};

        if (wasCold)
        {
            cursor.Cold.push_back(fresh);
        }

        ++cursor.MadeCount;
        return true;
    }

    /// <summary>
    /// Takes the display's scale and colours. A new scale drops every cursor. Once the palette has settled (the
    /// same two calls running; not mid-fade) every shape gets its cursor in the colours now shown, so switching
    /// shapes in play only selects one.
    /// </summary>
    void prepare(MCCursorState& cursor, MCDisplay& display)
    {
        std::array<SDL_Color, 256> colors{};
        display.GetShownColors(colors.data());
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        display.PixelScale(scaleX, scaleY);
        float density = SDL_GetWindowPixelDensity(display.Window());

        if (density <= 0.0f)
        {
            density = 1.0f;
        }

        if (cursor.ScaleX != scaleX || cursor.ScaleY != scaleY || cursor.Density != density)
        {
            if (MCFrameLog::Enabled())
            {
                MCFrameLog::Note(std::format("cursor scale {}x{} density {} -> {}x{} density {}: all remade",
                                             cursor.ScaleX, cursor.ScaleY, cursor.Density, scaleX, scaleY, density));
            }

            for (MCMadeCursor& made : cursor.ShapeCursors)
            {
                retire(cursor, made);
            }

            retire(cursor, cursor.Other);
            cursor.ScaleX = scaleX;
            cursor.ScaleY = scaleY;
            cursor.Density = density;
            cursor.Ready = false;
        }

        const bool settled = sameColors(colors, cursor.LastColors);
        cursor.LastColors = colors;

        if (!settled || (cursor.Ready && sameColors(colors, cursor.ReadyColors)))
        {
            return;
        }

        bool all = true;

        for (size_t i = 0; i < cursor.Shapes.size(); ++i)
        {
            if (cursor.Shapes[i].Width > 0)
            {
                all = ensure(cursor, cursor.ShapeCursors[i], cursor.Shapes[i], colors) && all;
            }
        }

        cursor.Ready = all;
        cursor.ReadyColors = colors;
    }

    /// <summary>
    /// Shows each cold cursor once, then puts back the one on screen. Windows builds an SDL cursor's icon (an .ANI
    /// resource, slow) the first time it is shown over the window, so this moves that work off the switch. Only
    /// while the game's cursor is up over the window: elsewhere SDL shows its arrow and builds nothing.
    /// </summary>
    void warm(MCCursorState& cursor, SDL_Window* window)
    {
        if (cursor.Cold.empty() || SDL_GetMouseFocus() != window || !SDL_CursorVisible() ||
            SDL_GetCursor() != cursor.Shown)
        {
            return;
        }

        MCFrameLog::Scope part("cursor.warm");

        if (MCFrameLog::Enabled())
        {
            MCFrameLog::Note(std::format("cursor: warmed {}", cursor.Cold.size()));
        }

        for (SDL_Cursor* cold : cursor.Cold)
        {
            SDL_SetCursor(cold);
        }

        cursor.Cold.clear();
        SDL_SetCursor(cursor.Shown);
    }

    /// <summary>Puts <paramref name="shown"/> on screen, then frees the retired cursors and warms the cold ones.</summary>
    void showCursor(MCCursorState& cursor, SDL_Cursor* shown, SDL_Window* window)
    {
        if (MCFrameLog::Enabled() && cursor.Shown != shown)
        {
            const bool cold = std::ranges::find(cursor.Cold, shown) != cursor.Cold.end();
            MCFrameLog::Note(std::format("cursor switched{} (mouse focus {}, visible {})", cold ? " to a cold one" : "",
                                         SDL_GetMouseFocus() == window, SDL_CursorVisible()));
        }

        cursor.Shown = shown;

        if (MCFrameLog::Scope part("cursor.set"); true)
        {
            MCInput::SetGameCursor(shown);
        }

        std::erase_if(cursor.Retired,
                      [&](SDL_Cursor* retired)
                      {
                          if (retired == cursor.Shown)
                          {
                              return false;
                          }

                          SDL_DestroyCursor(retired);
                          return true;
                      });

        warm(cursor, window);
    }

    /// <summary>The attached display, or null if there is no window to show a cursor over.</summary>
    MCDisplay* shownDisplay()
    {
        MCDisplay* display = MCInput::Display();
        return display != nullptr && display->Window() != nullptr ? display : nullptr;
    }
}

namespace MCCursor
{
    MCCursorBitmap Rasterize(const MCCursorImage& image, const SDL_Color* colors, float scaleX, float scaleY)
    {
        MCCursorBitmap bitmap;

        if (image.Width <= 0 || image.Height <= 0 || colors == nullptr)
        {
            return bitmap;
        }

        scaleX = std::max(scaleX, 0.01f);
        scaleY = std::max(scaleY, 0.01f);
        bitmap.Width = std::max(1, static_cast<int>(std::lround(image.Width * scaleX)));
        bitmap.Height = std::max(1, static_cast<int>(std::lround(image.Height * scaleY)));
        bitmap.HotX = std::clamp(static_cast<int>(std::floor(image.HotX * scaleX)), 0, bitmap.Width - 1);
        bitmap.HotY = std::clamp(static_cast<int>(std::floor(image.HotY * scaleY)), 0, bitmap.Height - 1);
        bitmap.Rgba.assign(static_cast<size_t>(bitmap.Width) * bitmap.Height * 4, 0);

        for (int y = 0; y < bitmap.Height; ++y)
        {
            const int sourceY = std::min(image.Height - 1, static_cast<int>(static_cast<float>(y) / scaleY));

            for (int x = 0; x < bitmap.Width; ++x)
            {
                const int sourceX = std::min(image.Width - 1, static_cast<int>(static_cast<float>(x) / scaleX));
                const size_t from = static_cast<size_t>(sourceY) * image.Width + sourceX;

                if (image.Opaque[from] == 0)
                {
                    continue;
                }

                const SDL_Color color = colors[image.Pixels[from]];
                uint8_t* to = bitmap.Rgba.data() + (static_cast<size_t>(y) * bitmap.Width + x) * 4;
                to[0] = color.r;
                to[1] = color.g;
                to[2] = color.b;
                to[3] = 255;
            }
        }

        return bitmap;
    }

    void Preload(std::vector<MCCursorImage> shapes)
    {
        MCCursorState& cursor = state();

        for (MCMadeCursor& made : cursor.ShapeCursors)
        {
            retire(cursor, made);
        }

        cursor.Shapes = std::move(shapes);
        cursor.ShapeCursors.assign(cursor.Shapes.size(), {});
        cursor.Ready = false;
        // Made now in the colours shown now, not waiting for a settled palette: the game may show one at once.
        cursor.LastColors = {};

        if (MCDisplay* display = shownDisplay())
        {
            display->GetShownColors(cursor.LastColors.data());
            prepare(cursor, *display);
        }
    }

    void ShowShape(size_t shape)
    {
        MCCursorState& cursor = state();
        MCDisplay* display = shownDisplay();

        if (display == nullptr || shape >= cursor.Shapes.size() || cursor.Shapes[shape].Width <= 0)
        {
            Hide();
            return;
        }

        prepare(cursor, *display);
        MCMadeCursor& made = cursor.ShapeCursors[shape];

        // Mid-fade only the shape on screen follows the colours; all of them are remade once the palette settles.
        if (!ensure(cursor, made, cursor.Shapes[shape], cursor.LastColors))
        {
            return;
        }

        retire(cursor, cursor.Other);
        cursor.OtherImage = {};
        showCursor(cursor, made.Cursor, display->Window());
    }

    void Show(const MCCursorImage& picture)
    {
        MCCursorState& cursor = state();
        MCDisplay* display = shownDisplay();

        if (display == nullptr || picture.Width <= 0 || picture.Height <= 0)
        {
            Hide();
            return;
        }

        prepare(cursor, *display);

        if (!(cursor.OtherImage == picture))
        {
            retire(cursor, cursor.Other);
            cursor.OtherImage = picture;
        }

        if (!ensure(cursor, cursor.Other, picture, cursor.LastColors))
        {
            return;
        }

        showCursor(cursor, cursor.Other.Cursor, display->Window());
    }

    void Hide()
    {
        MCInput::SetGameCursor(nullptr);
    }

    void Shutdown()
    {
        MCCursorState& cursor = state();
        MCInput::SetGameCursor(nullptr);

        for (MCMadeCursor& made : cursor.ShapeCursors)
        {
            retire(cursor, made);
        }

        retire(cursor, cursor.Other);

        for (SDL_Cursor* retired : cursor.Retired)
        {
            SDL_DestroyCursor(retired);
        }

        const uint64_t madeCount = cursor.MadeCount;
        cursor = MCCursorState{};
        cursor.MadeCount = madeCount;
    }

    uint64_t CursorsMade()
    {
        return state().MadeCount;
    }

    size_t ColdCursors()
    {
        return state().Cold.size();
    }
}
