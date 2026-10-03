#include "stdafx.h"
#include "platform/MCDisplay.h"
#include "platform/MCRenderer.h"
#include "shaders/composite.spv.h"

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
        // The composite shader is SPIR-V: a device that takes it (Vulkan).
        SDL_SetBooleanProperty(props, SDL_PROP_RENDERER_CREATE_GPU_SHADERS_SPIRV_BOOLEAN, true);
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

    display->CreateComposite();

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
    MCRenderer::SetOpPlane(&_Screen, nullptr);

    for (const UnderlayTexture& underlay : _UnderlayTextures)
    {
        if (underlay.State != nullptr)
        {
            SDL_DestroyGPURenderState(underlay.State);
        }

        if (underlay.Texture != nullptr)
        {
            SDL_DestroyTexture(underlay.Texture);
        }
    }

    for (SDL_Texture* texture : {_Target, _Texture, _OpTexture, _TableTexture, _PaletteTexture})
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture(texture);
        }
    }

    if (_Composite != nullptr || _CompositeSampler != nullptr)
    {
        SDL_GPUDevice* device = SDL_GetGPURendererDevice(_Renderer);

        if (_CompositeSampler != nullptr)
        {
            SDL_ReleaseGPUSampler(device, _CompositeSampler);
        }

        if (_Composite != nullptr)
        {
            SDL_ReleaseGPUShader(device, _Composite);
        }
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

    if (_OpTexture != nullptr)
    {
        SDL_DestroyTexture(_OpTexture);
    }

    _Target = nullptr;
    _OpTexture = nullptr;
    _Texture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, _Width, _Height);

    if (_Texture == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateTexture(INDEX8)"));
    }

    if (!SDL_SetTexturePalette(_Texture, _SdlPalette))
    {
        return std::unexpected(SdlError("SDL_SetTexturePalette"));
    }

    // The screen's op plane, and its texture for the composite.
    _Ops.assign(_Pixels.size(), 0);
    MCRenderer::SetOpPlane(&_Screen, _Ops.data());

    if (_Composite != nullptr)
    {
        _OpTexture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, _Width, _Height);

        if (_OpTexture == nullptr)
        {
            return std::unexpected(SdlError("SDL_CreateTexture(ops)"));
        }
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
    MCPort::ManualClockPresented();

    if (OnPresent)
    {
        OnPresent();
    }

    if ((SDL_GetWindowFlags(_Window) & SDL_WINDOW_MINIMIZED) != 0)
    {
        return {};
    }

    if (auto uploaded = UploadFrame(); !uploaded)
    {
        return uploaded;
    }

    if (_Target != nullptr)
    {
        // Composited at the screen's size, then filtered up.
        DrawFrame(_Target);
        SDL_SetRenderDrawColor(_Renderer, 0, 0, 0, 255);
        SDL_RenderClear(_Renderer);
        const SDL_FRect view{static_cast<float>(_View.x), static_cast<float>(_View.y), static_cast<float>(_View.w),
                             static_cast<float>(_View.h)};
        SDL_RenderTexture(_Renderer, _Target, &view, nullptr);
    }
    else
    {
        DrawFrame(nullptr);
    }

    if (!SDL_RenderPresent(_Renderer))
    {
        return std::unexpected(SdlError("SDL_RenderPresent"));
    }

    return {};
}

void MCDisplay::CreateComposite()
{
    SDL_GPUDevice* device = SDL_GetGPURendererDevice(_Renderer);

    if (device == nullptr || (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV) == 0)
    {
        SDL_Log("MCDisplay: no GPU renderer with SPIR-V (renderer %s); the CPU composites the world view",
                SDL_GetRendererName(_Renderer));
        return;
    }

    SDL_GPUShaderCreateInfo shader{};
    shader.code = MC_composite_spirv;
    shader.code_size = sizeof(MC_composite_spirv);
    shader.entrypoint = "main";
    shader.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shader.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shader.num_samplers = 5;
    shader.num_uniform_buffers = 1;
    _Composite = SDL_CreateGPUShader(device, &shader);

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    _CompositeSampler = SDL_CreateGPUSampler(device, &sampler);
    _TableTexture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, 256, 256);
    _PaletteTexture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 1);

    if (_Composite == nullptr || _CompositeSampler == nullptr || _TableTexture == nullptr || _PaletteTexture == nullptr)
    {
        SDL_Log("MCDisplay: no composite shader (%s); the CPU composites the world view", SDL_GetError());

        if (_Composite != nullptr)
        {
            SDL_ReleaseGPUShader(device, _Composite);
            _Composite = nullptr;
        }
    }
}

namespace
{
    /// <summary>The GPU texture of an SDL texture of the GPU renderer.</summary>
    SDL_GPUTexture* GpuTexture(SDL_Texture* texture)
    {
        return static_cast<SDL_GPUTexture*>(
            SDL_GetPointerProperty(SDL_GetTextureProperties(texture), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
    }

    /// <summary>The composite shader's uniforms (shaders/composite.pshader, cbuffer Composite).</summary>
    struct CompositeUniforms
    {
        float ScreenSize[4];
        float Shown[4];
        float WorldSize[4];
    };
}

std::expected<void, std::string> MCDisplay::UploadFrame()
{
    SDL_Color colors[256];
    bool paletteChanged = false;

    {
        std::lock_guard lock(_PaletteLock);

        if (_PaletteDirty)
        {
            ApplyGamma(_Palette, _Gamma, _Brightness, colors);
            SDL_SetPaletteColors(_SdlPalette, colors, 0, 256);
            _PaletteDirty = false;
            paletteChanged = true;
        }
    }

    // This frame's underlays on the screen.
    _UnderlaysUsed = 0;

    for (const MCUnderlay& underlay : MCRenderer::Underlays())
    {
        const _window* source = underlay.Source;

        if (underlay.Target == nullptr || underlay.Target->buffer != _Screen.buffer || source == nullptr ||
            source->buffer == nullptr)
        {
            continue;
        }

        if (_UnderlaysUsed == _UnderlayTextures.size())
        {
            _UnderlayTextures.emplace_back();
        }

        UnderlayTexture& texture = _UnderlayTextures[_UnderlaysUsed++];
        texture.X0 = underlay.Rect.X0;
        texture.Y0 = underlay.Rect.Y0;
        texture.X1 = underlay.Rect.X1;
        texture.Y1 = underlay.Rect.Y1;
        texture.SourceWidth = source->x_max + 1;
        texture.SourceHeight = source->y_max + 1;

        if (_Composite == nullptr)
        {
            continue;
        }

        if (texture.Width < texture.SourceWidth || texture.Height < texture.SourceHeight)
        {
            if (texture.State != nullptr)
            {
                SDL_DestroyGPURenderState(texture.State);
                texture.State = nullptr;
            }

            if (texture.Texture != nullptr)
            {
                SDL_DestroyTexture(texture.Texture);
            }

            // Grown with some room, so a zoom easing the surface's size a pixel at a time doesn't remake it every frame.
            texture.Width = std::max(texture.Width, texture.SourceWidth + texture.SourceWidth / 4);
            texture.Height = std::max(texture.Height, texture.SourceHeight + texture.SourceHeight / 4);
            texture.Texture = SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING,
                                                texture.Width, texture.Height);

            if (texture.Texture == nullptr)
            {
                texture.Width = 0;
                texture.Height = 0;
                return std::unexpected(SdlError("SDL_CreateTexture(underlay)"));
            }
        }

        const SDL_Rect area{0, 0, texture.SourceWidth, texture.SourceHeight};

        if (!SDL_UpdateTexture(texture.Texture, &area, source->buffer, texture.SourceWidth))
        {
            return std::unexpected(SdlError("SDL_UpdateTexture(underlay)"));
        }

        if (texture.State == nullptr || texture.StateOps != _OpTexture)
        {
            if (texture.State != nullptr)
            {
                SDL_DestroyGPURenderState(texture.State);
            }

            const SDL_GPUTextureSamplerBinding bindings[4] = {
                {GpuTexture(_OpTexture), _CompositeSampler},
                {GpuTexture(texture.Texture), _CompositeSampler},
                {GpuTexture(_TableTexture), _CompositeSampler},
                {GpuTexture(_PaletteTexture), _CompositeSampler},
            };

            SDL_GPURenderStateCreateInfo state{};
            state.fragment_shader = _Composite;
            state.num_sampler_bindings = 4;
            state.sampler_bindings = bindings;
            texture.State = SDL_CreateGPURenderState(_Renderer, &state);
            texture.StateOps = _OpTexture;

            if (texture.State == nullptr)
            {
                return std::unexpected(SdlError("SDL_CreateGPURenderState"));
            }
        }

        const CompositeUniforms uniforms{
            {static_cast<float>(_Width), static_cast<float>(_Height), 0.0f, 0.0f},
            {static_cast<float>(texture.X0), static_cast<float>(texture.Y0),
             static_cast<float>(texture.X1 - texture.X0 + 1), static_cast<float>(texture.Y1 - texture.Y0 + 1)},
            {static_cast<float>(texture.SourceWidth), static_cast<float>(texture.SourceHeight),
             static_cast<float>(texture.Width), static_cast<float>(texture.Height)},
        };

        SDL_SetGPURenderStateFragmentUniforms(texture.State, 0, &uniforms, sizeof(uniforms));
    }

    // The screen: as it is, or composited by the CPU when the shader can't.
    const uint8_t* screen = _Pixels.data();

    if (_UnderlaysUsed != 0 && _Composite == nullptr)
    {
        _Composed = ComposeScreen();
        screen = _Composed.data();
    }

    if (!SDL_UpdateTexture(_Texture, nullptr, screen, _Width))
    {
        return std::unexpected(SdlError("SDL_UpdateTexture"));
    }

    if (_UnderlaysUsed == 0 || _Composite == nullptr)
    {
        return {};
    }

    if (!SDL_UpdateTexture(_OpTexture, nullptr, _Ops.data(), _Width))
    {
        return std::unexpected(SdlError("SDL_UpdateTexture(ops)"));
    }

    const SDL_Rect tableRows{0, 0, 256, MCRenderer::OpTableCount()};

    if (!SDL_UpdateTexture(_TableTexture, &tableRows, MCRenderer::OpTables(), 256))
    {
        return std::unexpected(SdlError("SDL_UpdateTexture(op tables)"));
    }

    if (paletteChanged)
    {
        for (SDL_Color& color : colors)
        {
            color.a = 255;
        }

        // SDL_Color is r, g, b, a bytes: RGBA32's layout.
        if (!SDL_UpdateTexture(_PaletteTexture, nullptr, colors, sizeof(colors)))
        {
            return std::unexpected(SdlError("SDL_UpdateTexture(palette)"));
        }
    }

    return {};
}

void MCDisplay::DrawFrame(SDL_Texture* target)
{
    // Into a target: the whole screen at its size. To the window: the shown view, through the logical presentation.
    const int originX = target != nullptr ? 0 : _View.x;
    const int originY = target != nullptr ? 0 : _View.y;

    if (target != nullptr)
    {
        SDL_SetRenderLogicalPresentation(_Renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderTarget(_Renderer, target);
    }

    SDL_SetRenderDrawColor(_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(_Renderer);
    const SDL_FRect whole = target != nullptr
                                ? SDL_FRect{0.0f, 0.0f, static_cast<float>(_Width), static_cast<float>(_Height)}
                                : SDL_FRect{static_cast<float>(_View.x), static_cast<float>(_View.y),
                                            static_cast<float>(_View.w), static_cast<float>(_View.h)};
    SDL_RenderTexture(_Renderer, _Texture, &whole, nullptr);

    if (_Composite != nullptr)
    {
        for (size_t i = 0; i < _UnderlaysUsed; ++i)
        {
            const UnderlayTexture& texture = _UnderlayTextures[i];

            // The underlay's rectangle within the screen.
            const int x0 = std::max(texture.X0, 0);
            const int y0 = std::max(texture.Y0, 0);
            const int x1 = std::min(texture.X1, _Width - 1);
            const int y1 = std::min(texture.Y1, _Height - 1);

            if (texture.State == nullptr || x1 < x0 || y1 < y0)
            {
                continue;
            }

            const SDL_FRect from{static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 - x0 + 1),
                                 static_cast<float>(y1 - y0 + 1)};
            const SDL_FRect to{static_cast<float>(x0 - originX), static_cast<float>(y0 - originY), from.w, from.h};
            SDL_SetGPURenderState(_Renderer, texture.State);
            SDL_RenderTexture(_Renderer, _Texture, &from, &to);
            SDL_SetGPURenderState(_Renderer, nullptr);
        }
    }

    if (target != nullptr)
    {
        SDL_SetRenderTarget(_Renderer, nullptr);
        ApplyPresentation();
    }
}

std::expected<std::vector<SDL_Color>, std::string> MCDisplay::ReadFrame()
{
    if (auto uploaded = UploadFrame(); !uploaded)
    {
        return std::unexpected(uploaded.error());
    }

    SDL_Texture* target =
        SDL_CreateTexture(_Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, _Width, _Height);

    if (target == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateTexture(read frame)"));
    }

    DrawFrame(target);
    SDL_SetRenderTarget(_Renderer, target);
    SDL_Surface* read = SDL_RenderReadPixels(_Renderer, nullptr);
    SDL_SetRenderTarget(_Renderer, nullptr);
    SDL_DestroyTexture(target);

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

    std::vector<SDL_Color> pixels(static_cast<size_t>(_Width) * _Height);

    for (int y = 0; y < _Height && y < rgba->h; ++y)
    {
        std::memcpy(pixels.data() + static_cast<size_t>(y) * _Width,
                    static_cast<const uint8_t*>(rgba->pixels) + static_cast<ptrdiff_t>(y) * rgba->pitch,
                    static_cast<size_t>(std::min(_Width, rgba->w)) * sizeof(SDL_Color));
    }

    SDL_DestroySurface(rgba);
    return pixels;
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
