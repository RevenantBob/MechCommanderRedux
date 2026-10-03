#include "stdafx.h"
#include "platform/MCSdlPresenter.h"

namespace
{
    std::string SdlError(std::string_view what)
    {
        return std::format("{}: {}", what, SDL_GetError());
    }
}

std::expected<std::unique_ptr<MCSdlPresenter>, std::string> MCSdlPresenter::Create(SDL_Window* window, bool vsync,
                                                                                   const MCPresentation& presentation)
{
    std::unique_ptr<MCSdlPresenter> presenter(new MCSdlPresenter());
    presenter->_Window = window;
    presenter->_Presentation = presentation;
    presenter->_Renderer = SDL_CreateRenderer(window, nullptr);

    if (presenter->_Renderer == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateRenderer"));
    }

    SDL_SetRenderVSync(presenter->_Renderer, vsync ? 1 : 0);
    presenter->_Palette = SDL_CreatePalette(256);

    if (presenter->_Palette == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreatePalette"));
    }

    return presenter;
}

MCSdlPresenter::~MCSdlPresenter()
{
    for (SDL_Texture* texture : {_Target, _Texture})
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
        }
    }

    if (_Palette != nullptr)
    {
        SDL_DestroyPalette(_Palette);
    }

    if (_Renderer != nullptr)
    {
        SDL_DestroyRenderer(_Renderer);
    }
}

std::string MCSdlPresenter::Name() const
{
    return std::format("software ({})", SDL_GetRendererName(_Renderer));
}

void MCSdlPresenter::SetPresentation(const MCPresentation& presentation)
{
    const bool linearChanged = presentation.Linear != _Presentation.Linear;
    _Presentation = presentation;

    if (linearChanged)
    {
        // The RGBA step is made (or dropped) with the textures.
        _Width = 0;
        _Height = 0;
    }

    if (_View.w > 0 && _View.h > 0)
    {
        ApplyPresentation(_View);
    }
}

void MCSdlPresenter::SetVSync(bool on)
{
    SDL_SetRenderVSync(_Renderer, on ? 1 : 0);
}

std::expected<void, std::string> MCSdlPresenter::EnsureTextures(int width, int height)
{
    if (width == _Width && height == _Height && _Texture != nullptr)
    {
        return {};
    }

    for (SDL_Texture** texture : {&_Target, &_Texture})
    {
        if (*texture != nullptr)
        {
            SDL_DestroyTexture(*texture);
            *texture = nullptr;
        }
    }

    _Width = 0;
    _Height = 0;
    _Texture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, width, height);

    if (_Texture == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateTexture(INDEX8)"));
    }

    if (!SDL_SetTexturePalette(_Texture, _Palette))
    {
        return std::unexpected(SdlError("SDL_SetTexturePalette"));
    }

    SDL_SetTextureBlendMode(_Texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(_Texture, SDL_SCALEMODE_NEAREST);

    if (_Presentation.Linear)
    {
        // A palette lookup can't be filtered, so the screen is expanded to RGBA at its own size first.
        _Target = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);

        if (_Target != nullptr)
        {
            SDL_SetTextureBlendMode(_Target, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(_Target, SDL_SCALEMODE_LINEAR);
        }
    }

    _Width = width;
    _Height = height;
    return {};
}

void MCSdlPresenter::ApplyPresentation(const SDL_Rect& view)
{
    _View = view;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;

    if (_Presentation.Stretch)
    {
        mode = SDL_LOGICAL_PRESENTATION_STRETCH;
    }
    else if (_Presentation.IntegerScale)
    {
        mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    }

    SDL_SetRenderLogicalPresentation(_Renderer, view.w, view.h, mode);
}

std::expected<void, std::string> MCSdlPresenter::Upload(const MCFrame& frame)
{
    if (auto made = EnsureTextures(frame.Width, frame.Height); !made)
    {
        return made;
    }

    if (frame.ColorsChanged)
    {
        SDL_SetPaletteColors(_Palette, frame.Colors, 0, 256);
    }

    const uint8_t* pixels = frame.Pixels;

    if (!frame.Underlays.empty())
    {
        _Composed.assign(frame.Pixels, frame.Pixels + static_cast<size_t>(frame.Width) * frame.Height);
        MCRenderer::ComposeUnderlays(frame.Screen, _Composed.data(), MCRect{0, 0, frame.Width - 1, frame.Height - 1});
        pixels = _Composed.data();
    }

    if (!SDL_UpdateTexture(_Texture, nullptr, pixels, frame.Width))
    {
        return std::unexpected(SdlError("SDL_UpdateTexture"));
    }

    if (frame.View.x != _View.x || frame.View.y != _View.y || frame.View.w != _View.w || frame.View.h != _View.h)
    {
        ApplyPresentation(frame.View);
    }

    return {};
}

std::expected<void, std::string> MCSdlPresenter::Present(const MCFrame& frame)
{
    if (auto uploaded = Upload(frame); !uploaded)
    {
        return uploaded;
    }

    SDL_SetRenderDrawColor(_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(_Renderer);
    const SDL_FRect view{static_cast<float>(frame.View.x), static_cast<float>(frame.View.y),
                         static_cast<float>(frame.View.w), static_cast<float>(frame.View.h)};

    if (_Target != nullptr)
    {
        // Converted at the screen's size, then filtered up.
        SDL_SetRenderLogicalPresentation(_Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderTarget(_Renderer, _Target);
        SDL_RenderTexture(_Renderer, _Texture, nullptr, nullptr);
        SDL_SetRenderTarget(_Renderer, nullptr);
        ApplyPresentation(frame.View);
        SDL_RenderClear(_Renderer);
        SDL_RenderTexture(_Renderer, _Target, &view, nullptr);
    }
    else
    {
        SDL_RenderTexture(_Renderer, _Texture, &view, nullptr);
    }

    if (!SDL_RenderPresent(_Renderer))
    {
        return std::unexpected(SdlError("SDL_RenderPresent"));
    }

    return {};
}

std::expected<std::vector<SDL_Color>, std::string> MCSdlPresenter::ReadFrame(const MCFrame& frame)
{
    if (auto uploaded = Upload(frame); !uploaded)
    {
        return std::unexpected(uploaded.error());
    }

    SDL_Texture* target =
        SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, frame.Width, frame.Height);

    if (target == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateTexture(read frame)"));
    }

    SDL_SetRenderLogicalPresentation(_Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderTarget(_Renderer, target);
    SDL_RenderTexture(_Renderer, _Texture, nullptr, nullptr);
    SDL_Surface* read = SDL_RenderReadPixels(_Renderer, nullptr);
    SDL_SetRenderTarget(_Renderer, nullptr);
    SDL_DestroyTexture(target);
    ApplyPresentation(frame.View);

    if (read == nullptr)
    {
        return std::unexpected(SdlError("SDL_RenderReadPixels"));
    }

    SDL_Surface* rgba = SDL_ConvertSurface(read, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(read);

    if (rgba == nullptr)
    {
        return std::unexpected(SdlError("SDL_ConvertSurface"));
    }

    std::vector<SDL_Color> pixels(static_cast<size_t>(frame.Width) * frame.Height);

    for (int y = 0; y < frame.Height && y < rgba->h; ++y)
    {
        std::memcpy(pixels.data() + static_cast<size_t>(y) * frame.Width,
                    static_cast<const uint8_t*>(rgba->pixels) + static_cast<ptrdiff_t>(y) * rgba->pitch,
                    static_cast<size_t>(std::min(frame.Width, rgba->w)) * sizeof(SDL_Color));
    }

    SDL_DestroySurface(rgba);
    return pixels;
}

MCViewport MCSdlPresenter::Viewport(int viewWidth, int viewHeight) const
{
    SDL_FRect rect{};

    if (viewWidth == _View.w && viewHeight == _View.h && SDL_GetRenderLogicalPresentationRect(_Renderer, &rect) &&
        rect.w > 0.0f && rect.h > 0.0f)
    {
        return MCViewport{rect.x, rect.y, rect.w, rect.h};
    }

    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(_Renderer, &width, &height);
    return MCPresentViewport(width, height, viewWidth, viewHeight, _Presentation);
}
