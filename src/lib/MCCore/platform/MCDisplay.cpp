#include "stdafx.h"
#include "platform/MCDisplay.h"
#include "platform/MCRenderer.h"
#include "platform/MCSdlPresenter.h"
#include "platform/MCVulkanPresenter.h"

namespace
{
    std::string SdlError(std::string_view what)
    {
        return std::format("{}: {}", what, SDL_GetError());
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

    /// <summary>The presenter for <paramref name="kind"/>; Vulkan falls back to software when it can't start.</summary>
    std::expected<std::unique_ptr<MCPresenter>, std::string> CreatePresenter(SDL_Window* window, MCRendererKind kind,
                                                                             bool vsync,
                                                                             const MCPresentation& presentation)
    {
        if (kind == MCRendererKind::Vulkan)
        {
            auto vulkan = MCVulkanPresenter::Create(window, vsync, presentation);

            if (vulkan)
            {
                return std::unique_ptr<MCPresenter>(std::move(*vulkan));
            }

            SDL_Log("MCDisplay: Vulkan can't start (%s); using the software renderer", vulkan.error().c_str());
        }

        auto software = MCSdlPresenter::Create(window, vsync, presentation);

        if (!software)
        {
            return std::unexpected(software.error());
        }

        return std::unique_ptr<MCPresenter>(std::move(*software));
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

    MCPresentation presentation;
    presentation.IntegerScale = options.IntegerScale;
    presentation.Stretch = options.Stretch;
    presentation.Linear = options.LinearFilter;
    auto presenter = CreatePresenter(display->_Window, options.Renderer, options.VSync, presentation);

    if (!presenter)
    {
        return std::unexpected(presenter.error());
    }

    display->_Presenter = std::move(*presenter);
    SDL_Log("MCDisplay: renderer %s", display->_Presenter->Name().c_str());
    display->SetTitle(options.Title.c_str());

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

    display->MakeScreen();
    return display;
}

MCDisplay::~MCDisplay()
{
    MCRenderer::SetOpPlane(&_Screen, nullptr);
    MCRenderer::RemoveFrameSurface(&_Screen);
    _Presenter.reset();

    if (_Window != nullptr)
    {
        SDL_DestroyWindow(_Window);
    }
}

void MCDisplay::SetTitle(const char* title)
{
    const char* renderer = _Presenter->Kind() == MCRendererKind::Vulkan ? "Vulkan" : "Software";
    SDL_SetWindowTitle(_Window, std::format("{} [{}]", title != nullptr ? title : "", renderer).c_str());
}

void MCDisplay::MakeScreen()
{
    _Pixels.assign(static_cast<size_t>(_Width) * _Height, 0);
    _Screen.buffer = _Pixels.data();
    _Screen.x_max = _Width - 1;
    _Screen.y_max = _Height - 1;
    _Ops.assign(_Pixels.size(), 0);
    MCRenderer::SetOpPlane(&_Screen, _Ops.data());
    MCRenderer::AddFrameSurface(&_Screen);
    std::lock_guard lock(_PaletteLock);
    _PaletteDirty = true;
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
    MakeScreen();
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

MCViewport MCDisplay::ShownViewport() const
{
    return _Presenter->Viewport(_View.w, _View.h);
}

void MCDisplay::PixelScale(float& scaleX, float& scaleY) const
{
    scaleX = 1.0f;
    scaleY = 1.0f;
    const MCViewport viewport = ShownViewport();

    if (viewport.W <= 0.0f || viewport.H <= 0.0f)
    {
        return;
    }

    scaleX = viewport.W / static_cast<float>(_View.w);
    scaleY = viewport.H / static_cast<float>(_View.h);
}

MCFrame MCDisplay::BuildFrame(bool allColors)
{
    MCFrame frame;
    frame.Screen = &_Screen;
    frame.Pixels = _Pixels.data();
    frame.Width = _Width;
    frame.Height = _Height;
    frame.View = _View;
    frame.Ops = _Ops.data();
    _FrameUnderlays.clear();

    for (const MCUnderlay& underlay : MCRenderer::Underlays())
    {
        if (underlay.Target != nullptr && underlay.Target->buffer == _Screen.buffer && underlay.Source != nullptr &&
            underlay.Source->buffer != nullptr)
        {
            _FrameUnderlays.push_back(underlay);
        }
    }

    frame.Underlays = _FrameUnderlays;

    {
        std::lock_guard lock(_PaletteLock);

        if (_PaletteDirty || allColors)
        {
            ApplyGamma(_Palette, _Gamma, _Brightness, _ShownColors.data());
            frame.ColorsChanged = true;
            _PaletteDirty = false;
        }
    }

    frame.Colors = _ShownColors.data();
    return frame;
}

std::expected<void, std::string> MCDisplay::Present()
{
    MCPort::ManualClockPresented();

    if (OnPresent)
    {
        OnPresent();
    }

    if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MINIMIZED) != 0)
    {
        return {};
    }

    return _Presenter->Present(BuildFrame(false));
}

std::expected<std::vector<SDL_Color>, std::string> MCDisplay::ReadFrame()
{
    return _Presenter->ReadFrame(BuildFrame(true));
}

std::vector<uint8_t> MCDisplay::ComposeScreen() const
{
    std::vector<uint8_t> pixels = _Pixels;
    MCRenderer::ComposeUnderlays(&_Screen, pixels.data(), MCRect{0, 0, _Width - 1, _Height - 1});
    return pixels;
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
    _Presenter->SetVSync(on);
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

    // The shown view only, with the underlays.
    const std::vector<uint8_t> shown = ComposeScreen();

    for (int y = 0; y < _View.h; ++y)
    {
        std::memcpy(static_cast<uint8_t*>(surface->pixels) + static_cast<ptrdiff_t>(y) * surface->pitch,
                    shown.data() + static_cast<size_t>(_View.y + y) * _Width + _View.x, static_cast<size_t>(_View.w));
    }

    const bool saved = SDL_SavePNG(surface, path.string().c_str());
    SDL_DestroySurface(surface);

    if (!saved)
    {
        return std::unexpected(SdlError("SDL_SavePNG"));
    }

    return {};
}

namespace
{
    /// <summary>Window pixels per window point (high-density displays have more than one).</summary>
    float PixelDensity(SDL_Window* window)
    {
        const float density = SDL_GetWindowPixelDensity(window);
        return density > 0.0f ? density : 1.0f;
    }
}

bool MCDisplay::WindowToLogical(float windowX, float windowY, float& logicalX, float& logicalY) const
{
    const float density = PixelDensity(_Window);
    const bool inside =
        MapToLogical(ShownViewport(), _View.w, _View.h, windowX * density, windowY * density, logicalX, logicalY);
    logicalX += static_cast<float>(_View.x);
    logicalY += static_cast<float>(_View.y);
    return inside;
}

void MCDisplay::LogicalToWindow(float logicalX, float logicalY, float& windowX, float& windowY) const
{
    logicalX -= static_cast<float>(_View.x);
    logicalY -= static_cast<float>(_View.y);
    const MCViewport viewport = ShownViewport();

    if (viewport.W <= 0.0f || viewport.H <= 0.0f)
    {
        windowX = logicalX;
        windowY = logicalY;
        return;
    }

    const float density = PixelDensity(_Window);
    windowX = (viewport.X + logicalX * viewport.W / static_cast<float>(_View.w)) / density;
    windowY = (viewport.Y + logicalY * viewport.H / static_cast<float>(_View.h)) / density;
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
