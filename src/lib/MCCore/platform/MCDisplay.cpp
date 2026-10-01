#include "stdafx.h"
#include "platform/MCDisplay.h"

namespace
{
    std::string SdlError(std::string_view what)
    {
        return std::format("{}: {}", what, SDL_GetError());
    }

    /// <summary>The GPU renderer (Vulkan where available), created as OgreBattleCPP creates it.</summary>
    SDL_Renderer* CreateGpuRenderer(SDL_Window* window)
    {
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
        SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, "gpu");
        SDL_Renderer* renderer = SDL_CreateRendererWithProperties(props);
        SDL_DestroyProperties(props);
        return renderer;
    }

    /// <summary>The palette through a gamma and brightness, as the GPU gets it.</summary>
    void ApplyGamma(const std::array<VFX_RGB, 256>& palette, float gamma, float brightness, SDL_Color* out)
    {
        std::array<uint8_t, 256> curve{};

        for (int i = 0; i < 256; ++i)
        {
            float value = static_cast<float>(i) / 255.0f;

            if (gamma != 1.0f)
            {
                value = std::pow(value, 1.0f / gamma);
            }

            curve[static_cast<size_t>(i)] =
                static_cast<uint8_t>(std::clamp(value * brightness * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        for (size_t i = 0; i < 256; ++i)
        {
            out[i] = {curve[palette[i].r], curve[palette[i].g], curve[palette[i].b], 255};
        }
    }
}

std::expected<std::unique_ptr<MCDisplay>, std::string> MCDisplay::Create(const MCDisplayOptions& options)
{
    if (options.Width <= 0 || options.Height <= 0)
    {
        return std::unexpected(std::format("bad screen size {}x{}", options.Width, options.Height));
    }

    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0 && !SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        return std::unexpected(SdlError("SDL_InitSubSystem(video)"));
    }

    std::unique_ptr<MCDisplay> display(new MCDisplay());
    display->_IntegerScale = options.IntegerScale;
    display->_Stretch = options.Stretch;
    display->_Linear = options.LinearFilter;
    display->_Width = options.Width;
    display->_Height = options.Height;
    display->_View = SDL_Rect{0, 0, options.Width, options.Height};

    // The windowed size: the logical size times the largest whole scale that fits the usable desktop.
    int scale = options.WindowScale;

    if (scale <= 0)
    {
        scale = 1;
        SDL_Rect usable{};

        if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable))
        {
            while ((scale + 1) * options.Width <= usable.w * 9 / 10 &&
                   (scale + 1) * options.Height <= usable.h * 9 / 10)
            {
                ++scale;
            }
        }
    }

    display->_WindowScale = scale;

    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (options.Resizable)
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    if (options.Fullscreen)
    {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (options.Hidden)
    {
        flags |= SDL_WINDOW_HIDDEN;
    }

    display->_Window = SDL_CreateWindow(options.Title.c_str(), options.Width * scale, options.Height * scale, flags);

    if (display->_Window == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateWindow"));
    }

    display->_Renderer = CreateGpuRenderer(display->_Window);

    if (display->_Renderer == nullptr)
    {
        SDL_Log("MCDisplay: GPU renderer unavailable (%s); using the default renderer", SDL_GetError());
        display->_Renderer = SDL_CreateRenderer(display->_Window, nullptr);
    }

    if (display->_Renderer == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateRenderer"));
    }

    SDL_SetRenderVSync(display->_Renderer, options.VSync ? 1 : 0);

    display->_SdlPalette = SDL_CreatePalette(256);

    if (display->_SdlPalette == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreatePalette"));
    }

    if (options.FollowWindow)
    {
        display->_FollowWindow = true;
        display->_MinWidth = options.Width;
        display->_MinHeight = options.Height;
        SDL_SetWindowMinimumSize(display->_Window, options.Width, options.Height);
        SDL_SyncWindow(display->_Window);
        display->WindowScreenSize(display->_Width, display->_Height);
        display->_View = SDL_Rect{0, 0, display->_Width, display->_Height};
    }

    display->_Pixels.assign(static_cast<size_t>(display->_Width) * display->_Height, 0);
    display->_Screen.buffer = display->_Pixels.data();
    display->_Screen.x_max = display->_Width - 1;
    display->_Screen.y_max = display->_Height - 1;

    if (auto created = display->CreateTexture(); !created)
    {
        return std::unexpected(created.error());
    }

    display->ApplyPresentation();
    return display;
}

MCDisplay::~MCDisplay()
{
    if (_Target != nullptr)
    {
        SDL_DestroyTexture(_Target);
    }

    if (_Texture != nullptr)
    {
        SDL_DestroyTexture(_Texture);
    }

    if (_SdlPalette != nullptr)
    {
        SDL_DestroyPalette(_SdlPalette);
    }

    if (_Renderer != nullptr)
    {
        SDL_DestroyRenderer(_Renderer);
    }

    if (_Window != nullptr)
    {
        SDL_DestroyWindow(_Window);
    }
}

std::expected<void, std::string> MCDisplay::CreateTexture()
{
    if (_Target != nullptr)
    {
        SDL_DestroyTexture(_Target);
    }

    if (_Texture != nullptr)
    {
        SDL_DestroyTexture(_Texture);
    }

    _Target = nullptr;
    _Texture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, _Width, _Height);

    if (_Texture == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateTexture(INDEX8)"));
    }

    if (!SDL_SetTexturePalette(_Texture, _SdlPalette))
    {
        return std::unexpected(SdlError("SDL_SetTexturePalette"));
    }

    SDL_SetTextureBlendMode(_Texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(_Texture, SDL_SCALEMODE_NEAREST);

    if (_Linear)
    {
        // A palette lookup can't be filtered, so the screen is expanded to RGBA at its own size first.
        _Target = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, _Width, _Height);

        if (_Target != nullptr)
        {
            SDL_SetTextureBlendMode(_Target, SDL_BLENDMODE_NONE);
            SDL_SetTextureScaleMode(_Target, SDL_SCALEMODE_LINEAR);
        }
    }

    std::lock_guard lock(_PaletteLock);
    _PaletteDirty = true;
    return {};
}

void MCDisplay::ApplyPresentation()
{
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;

    if (_Stretch)
    {
        mode = SDL_LOGICAL_PRESENTATION_STRETCH;
    }
    else if (_IntegerScale)
    {
        mode = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
    }

    SDL_SetRenderLogicalPresentation(_Renderer, _View.w, _View.h, mode);
}

void MCDisplay::WindowScreenSize(int& width, int& height) const
{
    width = _Width;
    height = _Height;

    if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MINIMIZED) != 0)
    {
        return;
    }

    int pixelWidth = 0;
    int pixelHeight = 0;

    if (!SDL_GetWindowSizeInPixels(_Window, &pixelWidth, &pixelHeight) || pixelWidth <= 0 || pixelHeight <= 0)
    {
        return;
    }

    width = std::max(pixelWidth, _MinWidth);
    height = std::max(pixelHeight, _MinHeight);
}

void MCDisplay::LargestScreenSize(int& width, int& height) const
{
    width = _Width;
    height = _Height;
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);

    if (displays == nullptr)
    {
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(displays[i]);

        if (mode == nullptr)
        {
            continue;
        }

        const float density = mode->pixel_density > 0.0f ? mode->pixel_density : 1.0f;
        width = std::max(width, static_cast<int>(std::ceil(static_cast<float>(mode->w) * density)));
        height = std::max(height, static_cast<int>(std::ceil(static_cast<float>(mode->h) * density)));
    }

    SDL_free(displays);
}

void MCDisplay::ResizeWindowToScale()
{
    if (IsFullscreen() || _FollowWindow)
    {
        return;
    }

    SDL_SetWindowSize(_Window, _Width * _WindowScale, _Height * _WindowScale);
    SDL_SetWindowPosition(_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

std::expected<void, std::string> MCDisplay::SetLogicalSize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return std::unexpected(std::format("bad screen size {}x{}", width, height));
    }

    if (width == _Width && height == _Height)
    {
        return {};
    }

    _Width = width;
    _Height = height;
    _View = SDL_Rect{0, 0, width, height};
    _Pixels.assign(static_cast<size_t>(width) * height, 0);
    _Screen.buffer = _Pixels.data();
    _Screen.x_max = width - 1;
    _Screen.y_max = height - 1;

    if (auto created = CreateTexture(); !created)
    {
        return created;
    }

    ApplyPresentation();
    ResizeWindowToScale();
    return {};
}

bool MCDisplay::SetView(int x, int y, int width, int height)
{
    SDL_Rect view;
    view.w = std::clamp(width, 1, _Width);
    view.h = std::clamp(height, 1, _Height);
    view.x = std::clamp(x, 0, _Width - view.w);
    view.y = std::clamp(y, 0, _Height - view.h);

    if (view.x == _View.x && view.y == _View.y && view.w == _View.w && view.h == _View.h)
    {
        return false;
    }

    _View = view;
    ApplyPresentation();
    return true;
}

void MCDisplay::SetPalette(int first, int count, const VFX_RGB* entries)
{
    if (entries == nullptr || first < 0 || first >= 256)
    {
        return;
    }

    count = std::min(count, 256 - first);
    std::lock_guard lock(_PaletteLock);

    for (int i = 0; i < count; ++i)
    {
        _Palette[static_cast<size_t>(first + i)] = entries[i];
    }

    _PaletteDirty = true;
}

void MCDisplay::SetPalette(int first, int count, const uint8_t* rgb)
{
    if (rgb == nullptr || first < 0 || first >= 256)
    {
        return;
    }

    count = std::min(count, 256 - first);
    std::lock_guard lock(_PaletteLock);

    for (int i = 0; i < count; ++i)
    {
        _Palette[static_cast<size_t>(first + i)] = {rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]};
    }

    _PaletteDirty = true;
}

void MCDisplay::GetPalette(int first, int count, VFX_RGB* out) const
{
    if (out == nullptr || first < 0 || first >= 256)
    {
        return;
    }

    count = std::min(count, 256 - first);
    std::lock_guard lock(_PaletteLock);

    for (int i = 0; i < count; ++i)
    {
        out[i] = _Palette[static_cast<size_t>(first + i)];
    }
}

void MCDisplay::SetGamma(float gamma, float brightness)
{
    std::lock_guard lock(_PaletteLock);
    _Gamma = std::clamp(gamma, 0.1f, 10.0f);
    _Brightness = std::clamp(brightness, 0.0f, 4.0f);
    _PaletteDirty = true;
}

void MCDisplay::GetShownColors(SDL_Color* out) const
{
    if (out == nullptr)
    {
        return;
    }

    std::lock_guard lock(_PaletteLock);
    ApplyGamma(_Palette, _Gamma, _Brightness, out);
}

void MCDisplay::PixelScale(float& scaleX, float& scaleY) const
{
    scaleX = 1.0f;
    scaleY = 1.0f;
    SDL_FRect rect{};

    if (!SDL_GetRenderLogicalPresentationRect(_Renderer, &rect) || rect.w <= 0.0f || rect.h <= 0.0f)
    {
        return;
    }

    scaleX = rect.w / static_cast<float>(_View.w);
    scaleY = rect.h / static_cast<float>(_View.h);
}

std::expected<void, std::string> MCDisplay::Present()
{
    if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MINIMIZED) != 0)
    {
        return {};
    }

    {
        std::lock_guard lock(_PaletteLock);

        if (_PaletteDirty)
        {
            SDL_Color colors[256];
            ApplyGamma(_Palette, _Gamma, _Brightness, colors);
            SDL_SetPaletteColors(_SdlPalette, colors, 0, 256);
            _PaletteDirty = false;
        }
    }

    if (!SDL_UpdateTexture(_Texture, nullptr, _Pixels.data(), _Width))
    {
        return std::unexpected(SdlError("SDL_UpdateTexture"));
    }

    SDL_Texture* source = _Texture;

    if (_Target != nullptr)
    {
        SDL_SetRenderLogicalPresentation(_Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderTarget(_Renderer, _Target);
        SDL_RenderTexture(_Renderer, _Texture, nullptr, nullptr);
        SDL_SetRenderTarget(_Renderer, nullptr);
        ApplyPresentation();
        source = _Target;
    }

    SDL_SetRenderDrawColor(_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(_Renderer);
    const SDL_FRect view{static_cast<float>(_View.x), static_cast<float>(_View.y), static_cast<float>(_View.w),
                         static_cast<float>(_View.h)};
    SDL_RenderTexture(_Renderer, source, &view, nullptr);

    if (!SDL_RenderPresent(_Renderer))
    {
        return std::unexpected(SdlError("SDL_RenderPresent"));
    }

    return {};
}

bool MCDisplay::SetFullscreen(bool fullscreen)
{
    if (!SDL_SetWindowFullscreen(_Window, fullscreen))
    {
        return false;
    }

    SDL_SyncWindow(_Window);

    if (!fullscreen)
    {
        ResizeWindowToScale();
    }

    return true;
}

bool MCDisplay::IsFullscreen() const
{
    return (SDL_GetWindowFlags(_Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void MCDisplay::SetVSync(bool on)
{
    SDL_SetRenderVSync(_Renderer, on ? 1 : 0);
}

std::expected<void, std::string> MCDisplay::SaveScreenshot(const std::filesystem::path& path) const
{
    SDL_Surface* surface = SDL_CreateSurface(_View.w, _View.h, SDL_PIXELFORMAT_INDEX8);

    if (surface == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateSurface"));
    }

    SDL_Palette* palette = SDL_CreateSurfacePalette(surface);

    if (palette != nullptr)
    {
        SDL_Color colors[256];
        {
            std::lock_guard lock(_PaletteLock);

            for (size_t i = 0; i < 256; ++i)
            {
                colors[i] = {_Palette[i].r, _Palette[i].g, _Palette[i].b, 255};
            }
        }

        SDL_SetPaletteColors(palette, colors, 0, 256);
    }

    // The shown view only.
    for (int y = 0; y < _View.h; ++y)
    {
        std::memcpy(static_cast<uint8_t*>(surface->pixels) + static_cast<ptrdiff_t>(y) * surface->pitch,
                    _Pixels.data() + static_cast<size_t>(_View.y + y) * _Width + _View.x, static_cast<size_t>(_View.w));
    }

    const bool saved = SDL_SavePNG(surface, path.string().c_str());
    SDL_DestroySurface(surface);

    if (!saved)
    {
        return std::unexpected(SdlError("SDL_SavePNG"));
    }

    return {};
}

bool MCDisplay::WindowToLogical(float windowX, float windowY, float& logicalX, float& logicalY) const
{
    bool inside;

    if (!SDL_RenderCoordinatesFromWindow(_Renderer, windowX, windowY, &logicalX, &logicalY))
    {
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(_Window, &w, &h);
        const MCViewport view = _Stretch ? MCViewport{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)}
                                         : ComputeLetterbox(w, h, _View.w, _View.h, _IntegerScale);
        inside = MapToLogical(view, _View.w, _View.h, windowX, windowY, logicalX, logicalY);
    }
    else
    {
        inside = logicalX >= 0.0f && logicalY >= 0.0f && logicalX < static_cast<float>(_View.w) &&
                 logicalY < static_cast<float>(_View.h);
    }

    logicalX += static_cast<float>(_View.x);
    logicalY += static_cast<float>(_View.y);
    return inside;
}

void MCDisplay::LogicalToWindow(float logicalX, float logicalY, float& windowX, float& windowY) const
{
    logicalX -= static_cast<float>(_View.x);
    logicalY -= static_cast<float>(_View.y);

    if (!SDL_RenderCoordinatesToWindow(_Renderer, logicalX, logicalY, &windowX, &windowY))
    {
        windowX = logicalX;
        windowY = logicalY;
    }
}

MCViewport MCDisplay::ComputeLetterbox(int outputWidth, int outputHeight, int logicalWidth, int logicalHeight,
                                       bool integerScale)
{
    MCViewport view;

    if (outputWidth <= 0 || outputHeight <= 0 || logicalWidth <= 0 || logicalHeight <= 0)
    {
        return view;
    }

    const float outW = static_cast<float>(outputWidth);
    const float outH = static_cast<float>(outputHeight);
    const float logW = static_cast<float>(logicalWidth);
    const float logH = static_cast<float>(logicalHeight);
    const float wantAspect = logW / logH;
    const float realAspect = outW / outH;

    if (integerScale)
    {
        float scale = wantAspect > realAspect ? static_cast<float>(outputWidth / logicalWidth)
                                              : static_cast<float>(outputHeight / logicalHeight);

        if (scale < 1.0f)
        {
            scale = 1.0f;
        }

        view.W = std::floor(logW * scale);
        view.H = std::floor(logH * scale);
        view.X = (outW - view.W) / 2.0f;
        view.Y = (outH - view.H) / 2.0f;
    }
    else if (std::fabs(wantAspect - realAspect) < 0.0001f)
    {
        view = {0.0f, 0.0f, outW, outH};
    }
    else if (wantAspect > realAspect)
    {
        const float scale = outW / logW;
        view.X = 0.0f;
        view.W = outW;
        view.H = std::floor(logH * scale);
        view.Y = (outH - view.H) / 2.0f;
    }
    else
    {
        const float scale = outH / logH;
        view.Y = 0.0f;
        view.H = outH;
        view.W = std::floor(logW * scale);
        view.X = (outW - view.W) / 2.0f;
    }

    return view;
}

bool MCDisplay::MapToLogical(const MCViewport& viewport, int logicalWidth, int logicalHeight, float x, float y,
                             float& logicalX, float& logicalY)
{
    if (viewport.W <= 0.0f || viewport.H <= 0.0f)
    {
        logicalX = x;
        logicalY = y;
        return false;
    }

    logicalX = (x - viewport.X) * static_cast<float>(logicalWidth) / viewport.W;
    logicalY = (y - viewport.Y) * static_cast<float>(logicalHeight) / viewport.H;
    return logicalX >= 0.0f && logicalY >= 0.0f && logicalX < static_cast<float>(logicalWidth) &&
           logicalY < static_cast<float>(logicalHeight);
}
