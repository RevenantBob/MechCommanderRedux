#pragma once

// The frame half of the renderer: what turns the screen the game drew (8-bit, with its op plane and underlays) into
// the picture in the window. MCDisplay owns the window, the screen buffer and the palette, and hands each frame to a
// presenter. MCVulkanPresenter composites and scales on the GPU (SDL GPU, Vulkan, HLSL shaders); MCSdlPresenter, the
// software renderer's, composites on the CPU and shows the result with SDL's 2D renderer.

#include "platform/MCRenderer.h"

/// <summary>Which renderer draws the game (PREFS "Renderer", <c>-renderer</c>).</summary>
enum class MCRendererKind
{
    /// <summary>The GPU, through SDL GPU on Vulkan (the default; falls back to software when it can't start).</summary>
    Vulkan,
    /// <summary>The CPU, shown with SDL's 2D renderer.</summary>
    Software
};

/// <summary>The renderer kind a name stands for ("vulkan", "software", any case); empty for another name.</summary>
std::optional<MCRendererKind> MCRendererKindFromName(std::string_view name);

/// <summary>The name of a renderer kind, as <see cref="MCRendererKindFromName"/> reads it.</summary>
const char* MCRendererKindName(MCRendererKind kind);

/// <summary>A rectangle in window pixels where the logical screen is shown.</summary>
struct MCViewport
{
    float X = 0.0f;
    float Y = 0.0f;
    float W = 0.0f;
    float H = 0.0f;
};

/// <summary>How the shown part of the screen is fitted into the window.</summary>
struct MCPresentation
{
    /// <summary>Scale only by whole multiples (sharper, with wider borders).</summary>
    bool IntegerScale = false;
    /// <summary>Fill the whole window, ignoring the aspect ratio; wins over <see cref="IntegerScale"/>.</summary>
    bool Stretch = false;
    /// <summary>Filter the scaled image linearly instead of taking the nearest pixel.</summary>
    bool Linear = false;
};

/// <summary>One frame as the game left it, handed to <see cref="MCPresenter::Present"/>.</summary>
struct MCFrame
{
    /// <summary>The screen as a VFX window (its pixels, <c>Width</c> bytes a row).</summary>
    const _window* Screen = nullptr;
    const uint8_t* Pixels = nullptr;
    int Width = 0;
    int Height = 0;
    /// <summary>The part of the screen that is shown in the window.</summary>
    SDL_Rect View{};
    /// <summary>The screen's op plane (one byte a pixel, laid out as the pixels).</summary>
    const uint8_t* Ops = nullptr;
    /// <summary>The screen's underlays this frame (only those that lie under the screen and have pixels).</summary>
    std::span<const MCUnderlay> Underlays;
    /// <summary>The 256 colours as shown (gamma applied), and whether they changed since the last frame.</summary>
    const SDL_Color* Colors = nullptr;
    bool ColorsChanged = false;
};

/// <summary>
/// Shows frames in the display's window. Every call is made from the thread that created the display.
/// </summary>
class MCPresenter
{
public:
    virtual ~MCPresenter() = default;

    /// <summary>The kind of renderer this presenter belongs to.</summary>
    virtual MCRendererKind Kind() const = 0;

    /// <summary>A name for logs ("vulkan", "software (direct3d11)", ...).</summary>
    virtual std::string Name() const = 0;

    /// <summary>Whether the screen's underlays are composited on the GPU (else on the CPU).</summary>
    virtual bool CompositesOnGpu() const = 0;

    /// <summary>Changes how the shown part is fitted into the window.</summary>
    virtual void SetPresentation(const MCPresentation& presentation) = 0;

    /// <summary>Turns waiting for the display's refresh on or off.</summary>
    virtual void SetVSync(bool on) = 0;

    /// <summary>Shows a frame (waiting for the display's refresh when vsync is on).</summary>
    virtual std::expected<void, std::string> Present(const MCFrame& frame) = 0;

    /// <summary>
    /// Draws the frame as <see cref="Present"/> would, at the screen's size, and reads it back as RGBA (tests compare
    /// it with the CPU's composite).
    /// </summary>
    virtual std::expected<std::vector<SDL_Color>, std::string> ReadFrame(const MCFrame& frame) = 0;

    /// <summary>
    /// Where a view of <paramref name="viewWidth"/> x <paramref name="viewHeight"/> lands in the window now, in
    /// window pixels (not points).
    /// </summary>
    virtual MCViewport Viewport(int viewWidth, int viewHeight) const = 0;
};

/// <summary>
/// Where a view of <paramref name="viewWidth"/> x <paramref name="viewHeight"/> lands in an output of
/// <paramref name="outputWidth"/> x <paramref name="outputHeight"/> pixels: stretched over it, or letterboxed (by
/// whole multiples with <see cref="MCPresentation::IntegerScale"/>), as SDL's logical presentation places it.
/// </summary>
MCViewport MCPresentViewport(int outputWidth, int outputHeight, int viewWidth, int viewHeight,
                             const MCPresentation& presentation);
