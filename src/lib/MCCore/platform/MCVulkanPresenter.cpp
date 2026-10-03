#include "stdafx.h"
#include "platform/MCVulkanPresenter.h"
#include "platform/MCVulkanShaders.h"

namespace
{
    std::string SdlError(std::string_view what)
    {
        return std::format("{}: {}", what, SDL_GetError());
    }

    /// <summary>The quad shader's uniforms (shaders/quad.vshader, cbuffer Quad).</summary>
    struct QuadUniforms
    {
        float Rect[4];
        float TargetSize[4];
    };

    /// <summary>The composite shader's uniforms (shaders/composite.pshader, cbuffer Composite).</summary>
    struct CompositeUniforms
    {
        float ScreenSize[4];
        float Shown[4];
        float WorldSize[4];
        float Mode[4];
    };

    /// <summary>Upload offsets are kept on this boundary (the strictest any backend asks of a texture copy).</summary>
    constexpr uint32_t UploadAlignment = 512;

    uint32_t AlignUpload(uint32_t offset)
    {
        return (offset + UploadAlignment - 1) & ~(UploadAlignment - 1);
    }
}

std::expected<std::unique_ptr<MCVulkanPresenter>, std::string> MCVulkanPresenter::Create(
    SDL_Window* window, bool vsync, const MCPresentation& presentation)
{
    std::unique_ptr<MCVulkanPresenter> presenter(new MCVulkanPresenter());
    presenter->_Window = window;
    presenter->_Presentation = presentation;
    presenter->_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");

    if (presenter->_Device == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateGPUDevice(vulkan)"));
    }

    if (!SDL_ClaimWindowForGPUDevice(presenter->_Device, window))
    {
        return std::unexpected(SdlError("SDL_ClaimWindowForGPUDevice"));
    }

    presenter->_Claimed = true;
    presenter->SetVSync(vsync);

    auto quad = MCVulkanShaders::Load(presenter->_Device, MCVulkanShaders::Quad);

    if (!quad)
    {
        return std::unexpected(quad.error());
    }

    presenter->_QuadShader = *quad;
    auto composite = MCVulkanShaders::Load(presenter->_Device, MCVulkanShaders::Composite);

    if (!composite)
    {
        return std::unexpected(composite.error());
    }

    presenter->_CompositeShader = *composite;

    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo pipeline{};
    pipeline.vertex_shader = presenter->_QuadShader;
    pipeline.fragment_shader = presenter->_CompositeShader;
    pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline.target_info.num_color_targets = 1;
    pipeline.target_info.color_target_descriptions = &target;
    presenter->_CompositePipeline = SDL_CreateGPUGraphicsPipeline(presenter->_Device, &pipeline);

    if (presenter->_CompositePipeline == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateGPUGraphicsPipeline(composite)"));
    }

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    presenter->_Nearest = SDL_CreateGPUSampler(presenter->_Device, &sampler);

    if (presenter->_Nearest == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateGPUSampler"));
    }

    // The renderer of the frame surfaces, drawing when asked to.
    const MCGpuDrawing drawing = MCRenderer::RequestedGpuDrawing();

    if (drawing != MCGpuDrawing::Off)
    {
        auto renderer = MCVulkanRenderer::Create(presenter->_Device);

        if (renderer)
        {
            presenter->_Renderer = std::move(*renderer);
            MCRenderer::SetHardware(presenter->_Renderer.get(), drawing);
        }
        else
        {
            SDL_Log("MCVulkanPresenter: the GPU renderer can't start (%s); drawing in software",
                    renderer.error().c_str());
        }
    }

    return presenter;
}

MCVulkanPresenter::~MCVulkanPresenter()
{
    if (_Device == nullptr)
    {
        return;
    }

    SDL_WaitForGPUIdle(_Device);

    if (_Renderer != nullptr && MCRenderer::Hardware() == _Renderer.get())
    {
        MCRenderer::SetHardware(nullptr, MCGpuDrawing::Off);
    }

    _Renderer.reset();

    for (Texture* texture : {&_Screen, &_Ops, &_Tables, &_Palette, &_Frame})
    {
        Release(*texture);
    }

    for (Texture& texture : _Underlays)
    {
        Release(texture);
    }

    if (_Upload != nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, _Upload);
    }

    if (_Nearest != nullptr)
    {
        SDL_ReleaseGPUSampler(_Device, _Nearest);
    }

    if (_CompositePipeline != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(_Device, _CompositePipeline);
    }

    for (SDL_GPUShader* shader : {_QuadShader, _CompositeShader})
    {
        if (shader != nullptr)
        {
            SDL_ReleaseGPUShader(_Device, shader);
        }
    }

    if (_Claimed)
    {
        SDL_ReleaseWindowFromGPUDevice(_Device, _Window);
    }

    SDL_DestroyGPUDevice(_Device);
}

std::string MCVulkanPresenter::Name() const
{
    const char* device =
        SDL_GetStringProperty(SDL_GetGPUDeviceProperties(_Device), SDL_PROP_GPU_DEVICE_NAME_STRING, "");
    return *device != '\0' ? std::format("vulkan ({})", device) : std::string("vulkan");
}

void MCVulkanPresenter::SetVSync(bool on)
{
    _VSync = on;
    SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;

    if (!on)
    {
        if (SDL_WindowSupportsGPUPresentMode(_Device, _Window, SDL_GPU_PRESENTMODE_IMMEDIATE))
        {
            mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
        }
        else if (SDL_WindowSupportsGPUPresentMode(_Device, _Window, SDL_GPU_PRESENTMODE_MAILBOX))
        {
            mode = SDL_GPU_PRESENTMODE_MAILBOX;
        }
    }

    SDL_SetGPUSwapchainParameters(_Device, _Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
}

void MCVulkanPresenter::Release(Texture& texture)
{
    if (texture.Handle != nullptr)
    {
        SDL_ReleaseGPUTexture(_Device, texture.Handle);
    }

    texture = Texture{};
}

std::expected<void, std::string> MCVulkanPresenter::Ensure(Texture& texture, SDL_GPUTextureFormat format,
                                                           SDL_GPUTextureUsageFlags usage, uint32_t width,
                                                           uint32_t height, bool exact)
{
    if (texture.Handle != nullptr && (exact ? texture.Width == width && texture.Height == height
                                            : texture.Width >= width && texture.Height >= height))
    {
        return {};
    }

    if (!exact)
    {
        // Grown with some room, so a zoom easing the surface's size a pixel at a time doesn't remake it every frame.
        width = std::max(texture.Width, width + width / 4);
        height = std::max(texture.Height, height + height / 4);
    }

    Release(texture);
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    texture.Handle = SDL_CreateGPUTexture(_Device, &info);

    if (texture.Handle == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateGPUTexture"));
    }

    texture.Width = width;
    texture.Height = height;
    return {};
}

bool MCVulkanPresenter::ShowsGpuSurfaces(const MCFrame& frame) const
{
    uint32_t width = 0;
    uint32_t height = 0;
    return _Renderer != nullptr && MCRenderer::GpuDrawing() == MCGpuDrawing::On &&
           _Renderer->SurfaceTexture(frame.Screen, width, height) != nullptr;
}

std::expected<void, std::string> MCVulkanPresenter::Prepare(SDL_GPUCommandBuffer* commands, const MCFrame& frame,
                                                            bool& gpuSurfaces)
{
    if (_Renderer != nullptr && MCRenderer::GpuDrawing() != MCGpuDrawing::Off)
    {
        if (auto executed = _Renderer->Execute(commands, frame.Underlays); !executed)
        {
            return executed;
        }
    }

    // Until the GPU has drawn the screen once, the screen's own (blank) pixels are shown.
    gpuSurfaces = ShowsGpuSurfaces(frame);
    return Upload(commands, frame, gpuSurfaces);
}

std::expected<void, std::string> MCVulkanPresenter::Upload(SDL_GPUCommandBuffer* commands, const MCFrame& frame,
                                                           bool gpuSurfaces)
{
    const uint32_t width = static_cast<uint32_t>(frame.Width);
    const uint32_t height = static_cast<uint32_t>(frame.Height);
    constexpr SDL_GPUTextureFormat R8 = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    constexpr SDL_GPUTextureFormat Rgba = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    bool paletteNew = _Palette.Handle == nullptr;

    for (auto made :
         {Ensure(_Palette, Rgba, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, 1, true),
          Ensure(_Frame, Rgba, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height, true)})
    {
        if (!made)
        {
            return made;
        }
    }

    // What goes up this frame: the palette when it changed; unless the GPU's surfaces are shown, the screen, and with
    // underlays their surfaces, the ops and the op tables in use.
    struct Part
    {
        const void* Data;
        SDL_GPUTexture* Texture;
        uint32_t Width;
        uint32_t Height;
        uint32_t BytesPerPixel;
        uint32_t Offset;
    };

    std::vector<Part> parts;

    if (!gpuSurfaces)
    {
        for (auto made : {Ensure(_Screen, R8, SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height, true),
                          Ensure(_Ops, R8, SDL_GPU_TEXTUREUSAGE_SAMPLER, width, height, true),
                          Ensure(_Tables, R8, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, 256, true)})
        {
            if (!made)
            {
                return made;
            }
        }

        if (_Underlays.size() < frame.Underlays.size())
        {
            _Underlays.resize(frame.Underlays.size());
        }

        for (size_t i = 0; i < frame.Underlays.size(); ++i)
        {
            const _window* source = frame.Underlays[i].Source;

            if (auto made =
                    Ensure(_Underlays[i], R8, SDL_GPU_TEXTUREUSAGE_SAMPLER, static_cast<uint32_t>(source->x_max + 1),
                           static_cast<uint32_t>(source->y_max + 1), false);
                !made)
            {
                return made;
            }
        }

        parts.push_back({frame.Pixels, _Screen.Handle, width, height, 1, 0});
    }

    if (!gpuSurfaces && !frame.Underlays.empty())
    {
        parts.push_back({frame.Ops, _Ops.Handle, width, height, 1, 0});
        parts.push_back(
            {MCRenderer::OpTables(), _Tables.Handle, 256, static_cast<uint32_t>(MCRenderer::OpTableCount()), 1, 0});

        for (size_t i = 0; i < frame.Underlays.size(); ++i)
        {
            const _window* source = frame.Underlays[i].Source;
            parts.push_back({source->buffer, _Underlays[i].Handle, static_cast<uint32_t>(source->x_max + 1),
                             static_cast<uint32_t>(source->y_max + 1), 1, 0});
        }
    }

    if (frame.ColorsChanged || paletteNew)
    {
        // SDL_Color is r, g, b, a bytes: RGBA's layout. The alpha isn't used.
        parts.push_back({frame.Colors, _Palette.Handle, 256, 1, 4, 0});
    }

    if (parts.empty())
    {
        return {};
    }

    uint32_t total = 0;

    for (Part& part : parts)
    {
        part.Offset = AlignUpload(total);
        total = part.Offset + part.Width * part.Height * part.BytesPerPixel;
    }

    if (_Upload == nullptr || _UploadSize < total)
    {
        if (_Upload != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(_Device, _Upload);
        }

        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = total + total / 4;
        _Upload = SDL_CreateGPUTransferBuffer(_Device, &info);
        _UploadSize = _Upload != nullptr ? info.size : 0;

        if (_Upload == nullptr)
        {
            return std::unexpected(SdlError("SDL_CreateGPUTransferBuffer"));
        }
    }

    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(_Device, _Upload, true));

    if (mapped == nullptr)
    {
        return std::unexpected(SdlError("SDL_MapGPUTransferBuffer"));
    }

    for (const Part& part : parts)
    {
        std::memcpy(mapped + part.Offset, part.Data,
                    static_cast<size_t>(part.Width) * part.Height * part.BytesPerPixel);
    }

    SDL_UnmapGPUTransferBuffer(_Device, _Upload);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);

    for (const Part& part : parts)
    {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = _Upload;
        source.offset = part.Offset;
        source.pixels_per_row = part.Width;
        source.rows_per_layer = part.Height;
        SDL_GPUTextureRegion destination{};
        destination.texture = part.Texture;
        destination.w = part.Width;
        destination.h = part.Height;
        destination.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &destination, false);
    }

    SDL_EndGPUCopyPass(pass);
    return {};
}

void MCVulkanPresenter::Composite(SDL_GPUCommandBuffer* commands, const MCFrame& frame, bool gpuSurfaces)
{
    const float width = static_cast<float>(frame.Width);
    const float height = static_cast<float>(frame.Height);

    // The screen's texture and its allocated size; the GPU's surfaces have no op plane or op tables (the screen is
    // bound in their place, unread).
    SDL_GPUTexture* screen = _Screen.Handle;
    uint32_t screenWidth = static_cast<uint32_t>(frame.Width);
    uint32_t screenHeight = static_cast<uint32_t>(frame.Height);

    if (gpuSurfaces)
    {
        screen = _Renderer->SurfaceTexture(frame.Screen, screenWidth, screenHeight);
    }

    SDL_GPUTexture* ops = gpuSurfaces ? screen : _Ops.Handle;
    SDL_GPUTexture* tables = gpuSurfaces ? screen : _Tables.Handle;
    const float source = gpuSurfaces ? 1.0f : 0.0f;
    const float screenSize[4] = {width, height, static_cast<float>(screenWidth), static_cast<float>(screenHeight)};
    SDL_GPUColorTargetInfo target{};
    target.texture = _Frame.Handle;
    // The first quad covers every pixel.
    target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(pass, _CompositePipeline);
    const SDL_GPUViewport viewport{0.0f, 0.0f, width, height, 0.0f, 1.0f};
    SDL_SetGPUViewport(pass, &viewport);

    auto draw = [&](float x, float y, float w, float h, SDL_GPUTexture* world, const CompositeUniforms& uniforms)
    {
        const SDL_GPUTextureSamplerBinding bindings[5] = {
            {screen, _Nearest}, {ops, _Nearest}, {world, _Nearest}, {tables, _Nearest}, {_Palette.Handle, _Nearest},
        };

        const QuadUniforms quad{{x, y, w, h}, {width, height, 0.0f, 0.0f}};
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 5);
        SDL_PushGPUVertexUniformData(commands, 0, &quad, sizeof(quad));
        SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    };

    // The screen alone, then each underlay's rectangle with its surface.
    draw(0.0f, 0.0f, width, height, screen,
         CompositeUniforms{{screenSize[0], screenSize[1], screenSize[2], screenSize[3]},
                           {0.0f, 0.0f, 1.0f, 1.0f},
                           {1.0f, 1.0f, 1.0f, 1.0f},
                           {0.0f, source, 0.0f, 0.0f}});

    for (size_t i = 0; i < frame.Underlays.size(); ++i)
    {
        const MCUnderlay& underlay = frame.Underlays[i];
        const int x0 = std::max(underlay.Rect.X0, 0);
        const int y0 = std::max(underlay.Rect.Y0, 0);
        const int x1 = std::min(underlay.Rect.X1, frame.Width - 1);
        const int y1 = std::min(underlay.Rect.Y1, frame.Height - 1);

        if (x1 < x0 || y1 < y0)
        {
            continue;
        }

        // The world's texture: its size in use and allocated (the GPU's surface is exactly its size, as its draws
        // saw it).
        SDL_GPUTexture* world = nullptr;
        float worldSize[4] = {};

        if (!gpuSurfaces)
        {
            world = _Underlays[i].Handle;
            worldSize[0] = static_cast<float>(underlay.Source->x_max + 1);
            worldSize[1] = static_cast<float>(underlay.Source->y_max + 1);
            worldSize[2] = static_cast<float>(_Underlays[i].Width);
            worldSize[3] = static_cast<float>(_Underlays[i].Height);
        }
        else
        {
            uint32_t worldWidth = 0;
            uint32_t worldHeight = 0;
            world = _Renderer->SurfaceTexture(underlay.Source, worldWidth, worldHeight);

            if (world == nullptr)
            {
                continue;
            }

            worldSize[0] = worldSize[2] = static_cast<float>(worldWidth);
            worldSize[1] = worldSize[3] = static_cast<float>(worldHeight);
        }

        const CompositeUniforms uniforms{
            {screenSize[0], screenSize[1], screenSize[2], screenSize[3]},
            {static_cast<float>(underlay.Rect.X0), static_cast<float>(underlay.Rect.Y0),
             static_cast<float>(underlay.Rect.X1 - underlay.Rect.X0 + 1),
             static_cast<float>(underlay.Rect.Y1 - underlay.Rect.Y0 + 1)},
            {worldSize[0], worldSize[1], worldSize[2], worldSize[3]},
            {1.0f, source, 0.0f, 0.0f},
        };

        draw(static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1 - x0 + 1),
             static_cast<float>(y1 - y0 + 1), world, uniforms);
    }

    SDL_EndGPURenderPass(pass);
}

std::expected<void, std::string> MCVulkanPresenter::Present(const MCFrame& frame)
{
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_Device);

    if (commands == nullptr)
    {
        return std::unexpected(SdlError("SDL_AcquireGPUCommandBuffer"));
    }

    bool gpuSurfaces = false;

    if (auto prepared = Prepare(commands, frame, gpuSurfaces); !prepared)
    {
        SDL_CancelGPUCommandBuffer(commands);
        return prepared;
    }

    const bool drawing = _Renderer != nullptr && MCRenderer::GpuDrawing() != MCGpuDrawing::Off;
    Composite(commands, frame, gpuSurfaces);
    SDL_GPUTexture* swapchain = nullptr;
    uint32_t swapchainWidth = 0;
    uint32_t swapchainHeight = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, _Window, &swapchain, &swapchainWidth, &swapchainHeight))
    {
        SDL_SubmitGPUCommandBuffer(commands);
        return std::unexpected(SdlError("SDL_WaitAndAcquireGPUSwapchainTexture"));
    }

    // No swapchain image (a minimised or hidden window): the frame is composited but not shown.
    if (swapchain != nullptr)
    {
        const MCViewport shown = MCPresentViewport(static_cast<int>(swapchainWidth), static_cast<int>(swapchainHeight),
                                                   frame.View.w, frame.View.h, _Presentation);
        SDL_GPUBlitInfo blit{};
        blit.source.texture = _Frame.Handle;
        blit.source.x = static_cast<uint32_t>(frame.View.x);
        blit.source.y = static_cast<uint32_t>(frame.View.y);
        blit.source.w = static_cast<uint32_t>(frame.View.w);
        blit.source.h = static_cast<uint32_t>(frame.View.h);
        blit.destination.texture = swapchain;
        blit.destination.x = static_cast<uint32_t>(std::lround(std::max(shown.X, 0.0f)));
        blit.destination.y = static_cast<uint32_t>(std::lround(std::max(shown.Y, 0.0f)));
        blit.destination.w = static_cast<uint32_t>(std::lround(shown.W));
        blit.destination.h = static_cast<uint32_t>(std::lround(shown.H));
        blit.load_op = SDL_GPU_LOADOP_CLEAR;
        blit.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        blit.filter = _Presentation.Linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;

        if (blit.destination.w != 0 && blit.destination.h != 0)
        {
            SDL_BlitGPUTexture(commands, &blit);
        }
    }

    if (!SDL_SubmitGPUCommandBuffer(commands))
    {
        return std::unexpected(SdlError("SDL_SubmitGPUCommandBuffer"));
    }

    if (drawing && MCRenderer::GpuDrawing() == MCGpuDrawing::Mirror)
    {
        if (auto compared = _Renderer->Compare(frame.Underlays, frame.Colors); !compared)
        {
            return std::unexpected(compared.error());
        }
    }

    return {};
}

std::expected<std::vector<SDL_Color>, std::string> MCVulkanPresenter::ReadFrame(const MCFrame& frame)
{
    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_Device);

    if (commands == nullptr)
    {
        return std::unexpected(SdlError("SDL_AcquireGPUCommandBuffer"));
    }

    bool gpuSurfaces = false;

    if (auto prepared = Prepare(commands, frame, gpuSurfaces); !prepared)
    {
        SDL_CancelGPUCommandBuffer(commands);
        return std::unexpected(prepared.error());
    }

    Composite(commands, frame, gpuSurfaces);
    const uint32_t size = static_cast<uint32_t>(frame.Width) * static_cast<uint32_t>(frame.Height) * 4;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = size;
    SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(_Device, &info);

    if (download == nullptr)
    {
        SDL_CancelGPUCommandBuffer(commands);
        return std::unexpected(SdlError("SDL_CreateGPUTransferBuffer(download)"));
    }

    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureRegion source{};
    source.texture = _Frame.Handle;
    source.w = static_cast<uint32_t>(frame.Width);
    source.h = static_cast<uint32_t>(frame.Height);
    source.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download;
    destination.pixels_per_row = static_cast<uint32_t>(frame.Width);
    destination.rows_per_layer = static_cast<uint32_t>(frame.Height);
    SDL_DownloadFromGPUTexture(pass, &source, &destination);
    SDL_EndGPUCopyPass(pass);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);

    if (fence == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, download);
        return std::unexpected(SdlError("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }

    SDL_WaitForGPUFences(_Device, true, &fence, 1);
    SDL_ReleaseGPUFence(_Device, fence);
    std::vector<SDL_Color> pixels(static_cast<size_t>(frame.Width) * frame.Height);
    const void* mapped = SDL_MapGPUTransferBuffer(_Device, download, false);

    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, download);
        return std::unexpected(SdlError("SDL_MapGPUTransferBuffer(download)"));
    }

    // R8G8B8A8 is SDL_Color's byte order.
    std::memcpy(pixels.data(), mapped, size);
    SDL_UnmapGPUTransferBuffer(_Device, download);
    SDL_ReleaseGPUTransferBuffer(_Device, download);
    return pixels;
}

std::expected<std::vector<uint8_t>, std::string> MCVulkanPresenter::ReadScreen(const MCFrame& frame)
{
    if (_Renderer != nullptr && MCRenderer::GpuDrawing() == MCGpuDrawing::On)
    {
        auto shown = _Renderer->ReadShown(frame.Screen, frame.Underlays);

        if (!shown || !shown->empty())
        {
            return shown;
        }
    }

    // The software renderer's pixels (also before the GPU has drawn the screen once, as the composite shows them).
    return MCPresenter::ReadScreen(frame);
}

std::expected<void, std::string> MCVulkanPresenter::Discard(const MCFrame& frame)
{
    if (_Renderer == nullptr || MCRenderer::GpuDrawing() == MCGpuDrawing::Off)
    {
        return {};
    }

    return _Renderer->Flush(frame.Underlays);
}

MCViewport MCVulkanPresenter::Viewport(int viewWidth, int viewHeight) const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(_Window, &width, &height);
    return MCPresentViewport(width, height, viewWidth, viewHeight, _Presentation);
}
