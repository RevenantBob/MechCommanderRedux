#pragma once

#include "platform/MCPresenter.h"
#include "platform/MCVulkanRenderer.h"

/// <summary>
/// The Vulkan renderer's presenter: an SDL GPU device (SPIR-V, Vulkan) that owns the window's swapchain. Each frame
/// it composites the screen over its underlays in palette indices into the frame texture at the screen's size
/// (quad.vshader + composite.pshader), and blits the shown view into the swapchain, letterboxed, nearest or linear.
/// </summary>
/// <remarks>
/// <para>What it composites: when its renderer draws the frame surfaces (<see cref="MCGpuDrawing::On"/>), their
/// textures as the renderer left them, and only the palette goes up (when it changed). Otherwise (the software
/// renderer draws them, or mirror mode) the screen, its op plane, the op tables in use and the underlays' surfaces are
/// uploaded each frame: R8 (palette indices), the op tables R8 256 x 256 (row 0 the identity).</para>
/// <para>The palette is RGBA 256 x 1 and the frame texture RGBA: the palette is resolved in the composite, so palette
/// fades and cycling recolour everything, as in the original.</para>
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
    std::expected<std::vector<uint8_t>, std::string> ReadScreen(const MCFrame& frame) override;
    std::expected<void, std::string> Discard(const MCFrame& frame) override;
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
    /// <summary>Whether the frame surfaces' GPU textures are composited (the renderer draws them and the screen has
    /// one).</summary>
    bool ShowsGpuSurfaces(const MCFrame& frame) const;
    /// <summary>
    /// Runs the renderer's recorded commands on <paramref name="commands"/> when it draws, then uploads the frame's
    /// textures in a copy pass (with <paramref name="gpuSurfaces"/>, the palette only); sets
    /// <paramref name="gpuSurfaces"/> to what the composite should read.
    /// </summary>
    std::expected<void, std::string> Prepare(SDL_GPUCommandBuffer* commands, const MCFrame& frame, bool& gpuSurfaces);
    /// <summary>Uploads the frame's textures in a copy pass on <paramref name="commands"/>.</summary>
    std::expected<void, std::string> Upload(SDL_GPUCommandBuffer* commands, const MCFrame& frame, bool gpuSurfaces);
    /// <summary>Composites the frame into the frame texture, from the uploaded textures or the GPU's surfaces.</summary>
    void Composite(SDL_GPUCommandBuffer* commands, const MCFrame& frame, bool gpuSurfaces);
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
