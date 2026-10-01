#include "stdafx.h"
#include "platform/MCCursor.h"
#include "platform/MCDisplay.h"
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
    /// <summary>What the SDL cursor now made shows, to skip remaking it.</summary>
    struct MCCursorState
    {
        SDL_Cursor* Cursor = nullptr;
        MCCursorImage Image;
        std::array<SDL_Color, 256> Colors{};
        float ScaleX = 0.0f;
        float ScaleY = 0.0f;
        float Density = 0.0f;
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

    void Show(const MCCursorImage& picture)
    {
        MCDisplay* display = MCInput::Display();

        if (display == nullptr || display->Window() == nullptr || picture.Width <= 0 || picture.Height <= 0)
        {
            Hide();
            return;
        }

        MCCursorState& cursor = state();
        std::array<SDL_Color, 256> colors{};
        display->GetShownColors(colors.data());
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        display->PixelScale(scaleX, scaleY);
        float density = SDL_GetWindowPixelDensity(display->Window());

        if (density <= 0.0f)
        {
            density = 1.0f;
        }

        if (cursor.Cursor != nullptr && cursor.Image == picture && sameColors(cursor.Colors, colors) &&
            cursor.ScaleX == scaleX && cursor.ScaleY == scaleY && cursor.Density == density)
        {
            MCInput::SetGameCursor(cursor.Cursor);
            return;
        }

        // The surface is the picture at 100% display scale (window points); on a high-density display the pixel
        // sized one goes with it as an alternate image, which SDL picks there.
        const MCCursorImage image = MCCursorImage::WithHotSpotInside(picture);
        const MCCursorBitmap base = Rasterize(image, colors.data(), scaleX / density, scaleY / density);
        SDL_Surface* surface = toSurface(base);

        if (surface == nullptr)
        {
            return;
        }

        if (density != 1.0f)
        {
            SDL_Surface* dense = toSurface(Rasterize(image, colors.data(), scaleX, scaleY));

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
            return;
        }

        // Switch before freeing the old one, so SDL never falls back to its arrow in between.
        MCInput::SetGameCursor(made);

        if (cursor.Cursor != nullptr)
        {
            SDL_DestroyCursor(cursor.Cursor);
        }

        cursor.Cursor = made;
        cursor.Image = picture;
        cursor.Colors = colors;
        cursor.ScaleX = scaleX;
        cursor.ScaleY = scaleY;
        cursor.Density = density;
    }

    void Hide()
    {
        MCInput::SetGameCursor(nullptr);
    }

    void Shutdown()
    {
        MCCursorState& cursor = state();
        MCInput::SetGameCursor(nullptr);

        if (cursor.Cursor != nullptr)
        {
            SDL_DestroyCursor(cursor.Cursor);
        }

        cursor = MCCursorState{};
    }
}
