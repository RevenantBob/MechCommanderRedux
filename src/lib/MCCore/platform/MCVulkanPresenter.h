#pragma once

#include "platform/MCPresenter.h"
#include "platform/MCVulkanRenderer.h"

/// <summary>
/// The Vulkan renderer's presenter: an SDL GPU device (SPIR-V, Vulkan) that owns the window's swapchain. Each frame
/// it uploads the screen, its op plane, the op tables, the underlays' surfaces and (when it changed) the palette,
/// composites them in palette indices into the frame texture at the screen's size (quad.vshader +
/// composite.pshader), and blits the shown view into the swapchain, letterboxed, nearest or linear.
/// </summary>
/// <remarks>
/// Textures: the screen, its ops and each underlay are R8 (palette indices); the op tables R8 256 x 256 (row 0 the
/// identity); the palette RGBA 256 x 1. The frame texture is RGBA: the palette is resolved in the composite, so
/// palette fades and cycling recolour everything, as in the original.
/// </remarks>
class MCVulkanPresenter final : public MCPresenter
{
public:
    /// <summary>Makes the device, claims <paramref name="window"/> and loads the shaders.</summary>
    static std::expected<std::unique_ptr<MCVulkanPresenter>, std::string> Create(SDL_Window* window, bool vsync,
                                                                                 const MCPresentation& presentation);

    ~MCVulkanPresenter() override;

    MCRendererKind Kind() const override { return MCRendererKind::Vulkan; }
    std::string Name() const override;
    bool CompositesOnGpu() const override { return true; }
    void SetPresentation(const MCPresentation& presentation) override { _Presentation = presentation; }
    void SetVSync(bool on) override;
    std::expected<void, std::string> Present(const MCFrame& frame) override;
    std::expected<std::vector<SDL_Color>, std::string> ReadFrame(const MCFrame& frame) override;
    MCViewport Viewport(int viewWidth, int viewHeight) const override;

    /// <summary>The device.</summary>
    SDL_GPUDevice* Device() const { return _Device; }

    /// <summary>The renderer that draws the frame surfaces on this device (null when it couldn't start).</summary>
    MCVulkanRenderer* Renderer() const { return _Renderer.get(); }

private:
    MCVulkanPresenter() = default;

    /// <summary>An R8 or RGBA texture, grown as needed (only its top-left Width x Height is in use).</summary>
    struct Texture
    {
        SDL_GPUTexture* Handle = nullptr;
        /// <summary>The allocated size.</summary>
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    /// <summary>Makes sure <paramref name="texture"/> holds at least <paramref name="width"/> x
    /// <paramref name="height"/> (exactly that with <paramref name="exact"/>).</summary>
    std::expected<void, std::string> Ensure(Texture& texture, SDL_GPUTextureFormat format,
                                            SDL_GPUTextureUsageFlags usage, uint32_t width, uint32_t height,
                                            bool exact);
    /// <summary>Uploads the frame's textures in a copy pass on <paramref name="commands"/>.</summary>
    std::expected<void, std::string> Upload(SDL_GPUCommandBuffer* commands, const MCFrame& frame);
    /// <summary>Composites the uploaded frame into the frame texture.</summary>
    void Composite(SDL_GPUCommandBuffer* commands, const MCFrame& frame);
    void Release(Texture& texture);

    SDL_Window* _Window = nullptr;
    SDL_GPUDevice* _Device = nullptr;
    bool _Claimed = false;
    bool _VSync = true;
    MCPresentation _Presentation;
    SDL_GPUShader* _QuadShader = nullptr;
    SDL_GPUShader* _CompositeShader = nullptr;
    SDL_GPUGraphicsPipeline* _CompositePipeline = nullptr;
    SDL_GPUSampler* _Nearest = nullptr;
    Texture _Screen;
    Texture _Ops;
    Texture _Tables;
    Texture _Palette;
    /// <summary>The composited frame, RGBA at the screen's size.</summary>
    Texture _Frame;
    /// <summary>The underlays' surfaces, in the order of the frame's underlays.</summary>
    std::vector<Texture> _Underlays;
    /// <summary>The upload buffer, and its size.</summary>
    SDL_GPUTransferBuffer* _Upload = nullptr;
    uint32_t _UploadSize = 0;
    std::unique_ptr<MCVulkanRenderer> _Renderer;
};
