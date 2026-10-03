#pragma once

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
};

/// <summary>A rectangle in window pixels where the logical screen is shown.</summary>
struct MCViewport
{
    float X = 0.0f;
    float Y = 0.0f;
    float W = 0.0f;
    float H = 0.0f;
};

/// <summary>
/// The window and the screen the game draws into: the port's replacement for DirectDraw's primary surface, its
/// palette and <c>Flip</c> (and the GDI DIB fallback of windowed mode).
/// </summary>
/// <remarks>
/// <para>The game draws every frame into one 8-bit buffer, the VFX <c>_window</c> <see cref="Screen"/> returns,
/// <see cref="Width"/> x <see cref="Height"/> palette indices with a pitch of <see cref="Width"/>. <see cref="Present"/>
/// uploads it to an <c>SDL_PIXELFORMAT_INDEX8</c> streaming texture and draws it with SDL's GPU renderer (Vulkan):
/// the palette lookup happens on the GPU, and <c>SDL_SetRenderLogicalPresentation</c> scales the screen to the
/// window with borders (letterbox), sampling the nearest pixel unless <see cref="MCDisplayOptions::LinearFilter"/>.</para>
/// <para>The screen's underlays (<see cref="MCRenderer::Underlays"/>: the world view, drawn at 1x into a surface of its
/// own) are composited by a fragment shader (shaders/composite.pshader) over each underlay's rectangle: the screen's
/// key pixels show the underlay's pixel (nearest), mapped through their op (the screen's op plane and the frame's op
/// tables). Without the shader (no GPU renderer) the CPU composites the screen (<see cref="ComposeScreen"/>) and that
/// is shown instead.</para>
/// <para>Threads: <see cref="SetPalette"/>, <see cref="Screen"/> and the getters may be called from any thread
/// (the palette is guarded); everything else, <see cref="Present"/> included, from the thread that created the
/// display (SDL's video thread).</para>
/// </remarks>
class MCDisplay
{
public:
    /// <summary>Opens the window and the renderer and makes the screen buffer (initialising SDL's video if needed).</summary>
    static std::expected<std::unique_ptr<MCDisplay>, std::string> Create(const MCDisplayOptions& options);

    ~MCDisplay();
    MCDisplay(const MCDisplay&) = delete;
    MCDisplay& operator=(const MCDisplay&) = delete;

    /// <summary>The SDL window.</summary>
    SDL_Window* Window() const { return _Window; }

    /// <summary>The SDL renderer.</summary>
    SDL_Renderer* Renderer() const { return _Renderer; }

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
    _window* Screen() { return &_Screen; }

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
    void SetPalette(int first, int count, const VFX_RGB* entries);

    /// <summary>As the other overload, from packed r, g, b bytes.</summary>
    void SetPalette(int first, int count, const uint8_t* rgb);

    /// <summary>Reads <paramref name="count"/> palette entries from <paramref name="first"/> (as set, without gamma).</summary>
    void GetPalette(int first, int count, VFX_RGB* out) const;

    /// <summary>
    /// A display gamma applied to the palette on its way to the GPU (1 = none; above 1 brightens the midtones), and a
    /// brightness factor (1 = none). The game's own gamma steps (<c>gammaCorrectCurrentPalette</c>) edit the palette
    /// itself; this is the port's extra hook for the options screen.
    /// </summary>
    void SetGamma(float gamma, float brightness = 1.0f);

    /// <summary>The gamma set last.</summary>
    float Gamma() const { return _Gamma; }

    /// <summary>The 256 colours as they are shown: the palette through the gamma and brightness.</summary>
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
    /// the key, as <see cref="Present"/> shows them (nearest).
    /// </summary>
    std::vector<uint8_t> ComposeScreen() const;

    /// <summary>Whether the screen's underlays are composited by the shader (else by the CPU).</summary>
    bool CompositesOnGpu() const { return _Composite != nullptr; }

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

    /// <summary>
    /// Where a letterboxed logical screen lands in an output of <paramref name="outputWidth"/> x
    /// <paramref name="outputHeight"/>: the largest scale that fits (a whole one with <paramref name="integerScale"/>),
    /// centred. The same rule SDL's <c>SDL_LOGICAL_PRESENTATION_LETTERBOX</c> follows.
    /// </summary>
    static MCViewport ComputeLetterbox(int outputWidth, int outputHeight, int logicalWidth, int logicalHeight,
                                       bool integerScale);

    /// <summary>Maps an output point through a viewport to the logical screen (the pure form of <see cref="WindowToLogical"/>).</summary>
    static bool MapToLogical(const MCViewport& viewport, int logicalWidth, int logicalHeight, float x, float y,
                             float& logicalX, float& logicalY);

    /// <summary>
    /// Tests: called at the start of each <see cref="Present"/>, with the screen as it is about to be shown (frames
    /// drawn inside the game's own loops, such as the logistics screen wipes, are seen too).
    /// </summary>
    std::function<void()> OnPresent;

private:
    MCDisplay() = default;

    std::expected<void, std::string> CreateTexture();
    void ApplyPresentation();
    void ResizeWindowToScale();
    /// <summary>Makes the composite shader and its fixed textures (leaves <see cref="_Composite"/> null without a GPU
    /// renderer that takes SPIR-V).</summary>
    void CreateComposite();
    /// <summary>Uploads the screen (composited by the CPU when the shader can't), its ops, the op tables and the
    /// underlays.</summary>
    std::expected<void, std::string> UploadFrame();
    /// <summary>
    /// Draws the uploaded frame: into <paramref name="target"/> at the screen's size, or with null to the window
    /// through the logical presentation (the shown view).
    /// </summary>
    void DrawFrame(SDL_Texture* target);

    /// <summary>An underlay's texture, grown as needed (only its top-left Width x Height is in use), and the render
    /// state that composites it.</summary>
    struct UnderlayTexture
    {
        SDL_Texture* Texture = nullptr;
        int Width = 0;
        int Height = 0;
        /// <summary>Its composite state (made for this texture and the screen's op texture).</summary>
        SDL_GPURenderState* State = nullptr;
        SDL_Texture* StateOps = nullptr;
        /// <summary>The rectangle of the screen it covers this frame (inclusive), and the surface's size.</summary>
        int X0 = 0;
        int Y0 = 0;
        int X1 = -1;
        int Y1 = -1;
        int SourceWidth = 0;
        int SourceHeight = 0;
    };

    SDL_Window* _Window = nullptr;
    SDL_Renderer* _Renderer = nullptr;
    SDL_Texture* _Texture = nullptr;
    /// <summary>With linear filtering: the screen converted to RGBA at logical size, which is then scaled.</summary>
    SDL_Texture* _Target = nullptr;
    SDL_Palette* _SdlPalette = nullptr;
    /// <summary>The underlays' textures, in the order of <see cref="MCRenderer::Underlays"/>; the first
    /// <see cref="_UnderlaysUsed"/> are this frame's.</summary>
    std::vector<UnderlayTexture> _UnderlayTextures;
    size_t _UnderlaysUsed = 0;
    /// <summary>The composite shader (null: the CPU composites), its sampler, and its textures: the screen's ops
    /// (INDEX8 as R8), the op tables (256 x 256) and the palette (256 x 1 RGBA).</summary>
    SDL_GPUShader* _Composite = nullptr;
    SDL_GPUSampler* _CompositeSampler = nullptr;
    SDL_Texture* _OpTexture = nullptr;
    SDL_Texture* _TableTexture = nullptr;
    SDL_Texture* _PaletteTexture = nullptr;
    /// <summary>The screen's op plane (see <see cref="MCRenderer::SetOpPlane"/>).</summary>
    std::vector<uint8_t> _Ops;
    /// <summary>The CPU's composite of the screen, shown without the shader.</summary>
    std::vector<uint8_t> _Composed;

    int _Width = 0;
    int _Height = 0;
    /// <summary>The shown part of the screen.</summary>
    SDL_Rect _View{};
    int _WindowScale = 1;
    /// <summary>The smallest a window-following screen gets.</summary>
    int _MinWidth = 0;
    int _MinHeight = 0;
    bool _FollowWindow = false;
    bool _IntegerScale = false;
    bool _Stretch = false;
    bool _Linear = false;
    std::vector<uint8_t> _Pixels;
    _window _Screen{};

    mutable std::mutex _PaletteLock;
    std::array<VFX_RGB, 256> _Palette{};
    bool _PaletteDirty = true;
    float _Gamma = 1.0f;
    float _Brightness = 1.0f;
};
