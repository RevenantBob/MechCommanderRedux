#pragma once

#include "platform/MCPresenter.h"

/// <summary>How <see cref="MCDisplay::Create"/> opens the window.</summary>
struct MCDisplayOptions
{
    /// <summary>The window title (the original's <c>appName</c>).</summary>
    std::string Title = "MechCommander Redux";
    /// <summary>The logical screen: the size of the 8-bit buffer the game draws into (PREFS "Resolution").</summary>
    int Width = 640;
    int Height = 480;
    /// <summary>Start in (desktop) fullscreen.</summary>
    bool Fullscreen = false;
    /// <summary>Let the user resize the window; the screen is scaled to fit.</summary>
    bool Resizable = true;
    /// <summary>The windowed size as a multiple of the logical size; 0 picks the largest that fits the desktop.</summary>
    int WindowScale = 0;
    /// <summary>Wait for the display's refresh in <see cref="MCDisplay::Present"/>.</summary>
    bool VSync = true;
    /// <summary>Scale only by whole multiples (sharper, with wider borders).</summary>
    bool IntegerScale = false;
    /// <summary>Fill the whole window, ignoring the aspect ratio (no bars); wins over <see cref="IntegerScale"/>.</summary>
    bool Stretch = false;
    /// <summary>Filter the scaled image linearly instead of taking the nearest pixel.</summary>
    bool LinearFilter = false;
    /// <summary>Create the window hidden (tests).</summary>
    bool Hidden = false;
    /// <summary>
    /// The logical screen follows the window: <see cref="Width"/> x <see cref="Height"/> is the smallest it gets (and
    /// the window's minimum size), the window opens at the scale of that size <see cref="WindowScale"/> picks, and the
    /// screen starts at the window's size in pixels. The window is never resized to fit the screen.
    /// </summary>
    bool FollowWindow = false;
    /// <summary>The renderer asked for; Vulkan falls back to software when it can't start.</summary>
    MCRendererKind Renderer = MCRendererKind::Vulkan;
};

/// <summary>
/// The window and the screen the game draws into: the port's replacement for DirectDraw's primary surface, its
/// palette and <c>Flip</c> (and the GDI DIB fallback of windowed mode).
/// </summary>
/// <remarks>
/// <para>The game draws every frame into one 8-bit buffer, the VFX <c>_window</c> <see cref="Screen"/> returns,
/// <see cref="Width"/> x <see cref="Height"/> palette indices with a pitch of <see cref="Width"/>. <see cref="Present"/>
/// hands it to the presenter of the renderer in use (<see cref="MCPresenter"/>), with its op plane, its underlays
/// (<see cref="MCRenderer::Underlays"/>: the world view, drawn at 1x into a surface of its own) and the palette.
/// Vulkan (<see cref="MCVulkanPresenter"/>) composites them in a shader and scales the shown view into the window;
/// software (<see cref="MCSdlPresenter"/>) composites on the CPU (<see cref="ComposeScreen"/>) and shows the result
/// with SDL's 2D renderer. Either way the view is letterboxed into the window, nearest unless
/// <see cref="MCDisplayOptions::LinearFilter"/>.</para>
/// <para>Threads: <see cref="SetPalette"/>, <see cref="Screen"/> and the getters may be called from any thread
/// (the palette is guarded); everything else, <see cref="Present"/> included, from the thread that created the
/// display (SDL's video thread).</para>
/// </remarks>
class MCDisplay
{
    /// <summary>Only <see cref="Create"/> makes one.</summary>
    struct Key
    {
        explicit Key() = default;
    };

public:
    /// <summary>A display with no window yet.</summary>
    explicit MCDisplay(Key) {}

    /// <summary>Opens the window and the renderer and makes the screen buffer (initialising SDL's video if needed).</summary>
    static std::expected<std::unique_ptr<MCDisplay>, std::string> Create(const MCDisplayOptions& options);

    ~MCDisplay();
    MCDisplay(const MCDisplay&) = delete;
    MCDisplay& operator=(const MCDisplay&) = delete;

    /// <summary>The SDL window.</summary>
    SDL_Window* Window() const { return _Window; }

    /// <summary>The renderer in use (Vulkan, or software when asked for or when Vulkan couldn't start).</summary>
    MCRendererKind RendererKind() const { return _Presenter->Kind(); }

    /// <summary>The renderer in use, for logs.</summary>
    std::string RendererName() const { return _Presenter->Name(); }

    /// <summary>
    /// Sets the window's title (the original's <c>SetWindowTextA</c>), with the renderer in use after it
    /// ("... [Vulkan]", "... [Software]").
    /// </summary>
    void SetTitle(const char* title);

    /// <summary>Whether the logical screen follows the window (<see cref="MCDisplayOptions::FollowWindow"/>).</summary>
    bool FollowsWindow() const { return _FollowWindow; }

    /// <summary>
    /// The size a window-following screen should have now: the window's size in pixels, no smaller than the minimum
    /// (<see cref="MCDisplayOptions::Width"/> x <see cref="MCDisplayOptions::Height"/>). A minimised window keeps the
    /// current size.
    /// </summary>
    void WindowScreenSize(int& width, int& height) const;

    /// <summary>
    /// The largest size <see cref="WindowScreenSize"/> can return on this machine: the biggest desktop of any display,
    /// in pixels (at least the current screen).
    /// </summary>
    void LargestScreenSize(int& width, int& height) const;

    /// <summary>The logical screen's width.</summary>
    int Width() const { return _Width; }

    /// <summary>The logical screen's height.</summary>
    int Height() const { return _Height; }

    /// <summary>
    /// The screen as a VFX window. The pointer stays the same for the display's life; after
    /// <see cref="SetLogicalSize"/> its buffer and extents are the new ones.
    /// </summary>
    MCWindow* Screen() { return &_Screen; }

    /// <summary>The screen's pixels, <see cref="Width"/> bytes per row.</summary>
    uint8_t* Pixels() { return _Pixels.data(); }

    /// <summary>
    /// Changes the logical screen (the game's mode switch, <c>aSystem::resetDirectDraw</c>). The buffer is
    /// reallocated and cleared to 0; a windowed window is resized to keep its scale, unless the screen follows the
    /// window.
    /// </summary>
    std::expected<void, std::string> SetLogicalSize(int width, int height);

    /// <summary>
    /// Shows only part of the screen: the <paramref name="width"/> x <paramref name="height"/> rectangle at
    /// (<paramref name="x"/>, <paramref name="y"/>), clamped to the screen, is what gets scaled into the window
    /// (letterboxed like the whole screen), and the mouse maps into it. The whole screen is the default and comes
    /// back after <see cref="SetLogicalSize"/>. Used to blow up the 640x480 menus the game draws in the top-left
    /// corner of a larger screen.
    /// </summary>
    /// <returns>Whether the view changed.</returns>
    bool SetView(int x, int y, int width, int height);

    /// <summary>The part of the screen that is shown (see <see cref="SetView"/>), in screen pixels.</summary>
    SDL_Rect View() const { return _View; }

    /// <summary>
    /// Sets <paramref name="count"/> palette entries from <paramref name="first"/> (DirectDraw's <c>SetEntries</c>
    /// and <c>AnimatePalette</c>, GDI's <c>SetDIBColorTable</c>). 8 bits per channel. Takes effect at the next
    /// <see cref="Present"/>; calling it every frame is cheap (palette animation).
    /// </summary>
    void SetPalette(int first, int count, const MCVfxRgb* entries);

    /// <summary>As the other overload, from packed r, g, b bytes.</summary>
    void SetPalette(int first, int count, const uint8_t* rgb);

    /// <summary>Reads <paramref name="count"/> palette entries from <paramref name="first"/> (as set, without gamma).</summary>
    void GetPalette(int first, int count, MCVfxRgb* out) const;

    /// <summary>
    /// A display gamma applied to the palette on its way to the GPU (1 = none; above 1 brightens the midtones), and a
    /// brightness factor (1 = none). The game's own gamma steps (<c>gammaCorrectCurrentPalette</c>) edit the palette
    /// itself; this is the port's extra hook for the options screen.
    /// </summary>
    void SetGamma(float gamma, float brightness = 1.0f);

    /// <summary>The gamma set last.</summary>
    float Gamma() const { return _Gamma; }

    /// <summary>
    /// Sets the colour cycle shown over the palette (the water's): takes effect at the next <see cref="Present"/>
    /// without changing the palette.
    /// </summary>
    void SetColorCycle(const MCColorCycle& cycle);

    /// <summary>
    /// Fades the whole picture toward black: <paramref name="levels"/> (0 none .. 255 black) are taken off every shown
    /// colour's components, by the presenter (the composite shader on the GPU), whatever the surfaces hold. Takes effect
    /// at the next <see cref="Present"/>; the palette isn't changed.
    /// </summary>
    void SetFade(int levels);

    /// <summary>The fade set last.</summary>
    int Fade() const { return _Fade; }

    /// <summary>The 256 colours as they are shown: the palette through the gamma and brightness, then the cycle.</summary>
    void GetShownColors(SDL_Color* out) const;

    /// <summary>
    /// How many output pixels one logical pixel covers across and down (window pixels, not points; the two differ
    /// on high-density displays). 1 before the first present.
    /// </summary>
    void PixelScale(float& scaleX, float& scaleY) const;

    /// <summary>
    /// Shows the screen: uploads the pixels and the palette and presents (waiting for the display's refresh when
    /// vsync is on). A minimised window is skipped without waiting.
    /// </summary>
    std::expected<void, std::string> Present();

    /// <summary>Switches between desktop fullscreen and the window (the original's InitFullScreen / InitWindowMode).</summary>
    bool SetFullscreen(bool fullscreen);

    /// <summary>Flips <see cref="IsFullscreen"/>.</summary>
    bool ToggleFullscreen() { return SetFullscreen(!IsFullscreen()); }

    /// <summary>Whether the window is fullscreen.</summary>
    bool IsFullscreen() const;

    /// <summary>Turns waiting for the display's refresh on or off.</summary>
    void SetVSync(bool on);

    /// <summary>
    /// Writes the shown view as it is now (the screen with its underlays, at the screen's size) as an 8-bit PNG with its
    /// palette (the game's screenshot key).
    /// </summary>
    std::expected<void, std::string> SaveScreenshot(const std::filesystem::path& path) const;

    /// <summary>
    /// The whole screen as the player sees it, at the screen's size: its pixels with the underlays' pixels in place of
    /// the key, as <see cref="Present"/> shows them (nearest). When the GPU draws the frame, read back from it (what was
    /// drawn so far is run first).
    /// </summary>
    std::vector<uint8_t> ComposeScreen() const;

    /// <summary>Whether the screen's underlays are composited by the shader (else by the CPU).</summary>
    bool CompositesOnGpu() const { return _Presenter->CompositesOnGpu(); }

    /// <summary>
    /// Draws the frame as <see cref="Present"/> would, at the screen's size, into an offscreen texture and reads it
    /// back as RGBA (tests compare it with <see cref="ComposeScreen"/>).
    /// </summary>
    std::expected<std::vector<SDL_Color>, std::string> ReadFrame();

    /// <summary>
    /// Maps a point in window coordinates (as SDL's mouse events give them) to the logical screen.
    /// </summary>
    /// <returns>Whether the point is on the shown view (not on a border). The mapped point is set either way.</returns>
    bool WindowToLogical(float windowX, float windowY, float& logicalX, float& logicalY) const;

    /// <summary>Maps a point of the logical screen to window coordinates (for warping and clipping the mouse).</summary>
    void LogicalToWindow(float logicalX, float logicalY, float& windowX, float& windowY) const;

    /// <summary>Maps an output point through a viewport to the logical screen (the pure form of <see cref="WindowToLogical"/>).</summary>
    static bool MapToLogical(const MCViewport& viewport, int logicalWidth, int logicalHeight, float x, float y,
                             float& logicalX, float& logicalY);

    /// <summary>
    /// Tests: called at the start of each <see cref="Present"/>, with the screen as it is about to be shown (frames
    /// drawn inside the game's own loops, such as the logistics screen wipes, are seen too).
    /// </summary>
    std::function<void()> OnPresent;

private:
    /// <summary>Resizes the window to the screen's size times its scale (windowed, not following the window).</summary>
    void ResizeWindowToScale();
    /// <summary>Makes the screen's buffer and op plane for its size.</summary>
    void MakeScreen();
    /// <summary>
    /// This frame for the presenter: the screen, its ops, its underlays and the colours as shown (marked changed when
    /// they changed since the last frame, or always with <paramref name="allColors"/>).
    /// </summary>
    MCFrame BuildFrame(bool allColors);
    /// <summary>The underlays shown on the screen now (those that lie under it and have pixels).</summary>
    std::vector<MCUnderlay> ScreenUnderlays() const;
    /// <summary>
    /// Ends the colour cycle when palette entries <paramref name="first"/>.. (<paramref name="count"/> of them) cover
    /// any it remaps: setting them replaced the animated colours. Called with the palette lock held.
    /// </summary>
    void EndCycleOver(int first, int count);
    /// <summary>Where the shown view lands in the window, in window pixels.</summary>
    MCViewport ShownViewport() const;

    SDL_Window* _Window = nullptr;
    std::unique_ptr<MCPresenter> _Presenter;
    /// <summary>The screen's op plane (see <see cref="MCRenderer::SetOpPlane"/>).</summary>
    std::vector<uint8_t> _Ops;
    /// <summary>This frame's underlays on the screen, and the colours as shown.</summary>
    std::vector<MCUnderlay> _FrameUnderlays;
    std::array<SDL_Color, 256> _ShownColors{};
    /// <summary>The palette's colours as shown, before the cycle.</summary>
    std::array<SDL_Color, 256> _PaletteColors{};

    int _Width = 0;
    int _Height = 0;
    /// <summary>The shown part of the screen.</summary>
    SDL_Rect _View{};
    int _WindowScale = 1;
    /// <summary>The smallest a window-following screen gets.</summary>
    int _MinWidth = 0;
    int _MinHeight = 0;
    bool _FollowWindow = false;
    std::vector<uint8_t> _Pixels;
    MCWindow _Screen{};

    mutable std::mutex _PaletteLock;
    std::array<MCVfxRgb, 256> _Palette{};
    bool _PaletteDirty = true;
    MCColorCycle _Cycle;
    bool _CycleDirty = false;
    float _Gamma = 1.0f;
    float _Brightness = 1.0f;
    /// <summary>The screen fade (<see cref="SetFade"/>).</summary>
    std::atomic<int> _Fade = 0;
};
