#pragma once

#include "platform/MCSoftwareRenderer.h"

/// <summary>
/// The hardware renderer: draws the frame surfaces (the screen and the world view's surface, see
/// <see cref="MCRenderer::AddFrameSurface"/>) on the GPU, through SDL GPU on Vulkan, on the presenter's device.
/// </summary>
/// <remarks>
/// <para>Commands are recorded as they come (resolved into draws: shaders/instance.vshader's <c>Draw</c>) and run when the
/// frame is presented (<see cref="Execute"/>), in order, so the presenter composites what they drew. What a command
/// reads is taken at the time of the call, as the software renderer reads it: shapes, tiles, fast shapes and glyphs are
/// decoded into an atlas (value + state), pictures in memory become textures, tables are copied into the frame's table
/// texture. All of them are keyed by their bytes, so changed data is never drawn stale and unchanged data goes to the GPU
/// once.</para>
/// <para>Surfaces are RGBA: red the palette index, green and blue the screen's see-through pixels (the software op
/// plane's GPU form, see shaders/draw.pshader). A draw that reads the target (translucent shapes, table-mapped draws, a
/// copy within the surface) reads a copy taken before it; consecutive such draws that don't touch each other's pixels
/// share one copy.</para>
/// <para>Integer arithmetic throughout: the GPU writes the software renderer's pixels exactly. Mirror mode
/// (<see cref="MCGpuDrawing::Mirror"/>) draws both and compares them every frame (<see cref="Compare"/>).</para>
/// </remarks>
class MCVulkanRenderer final : public MCRenderer
{
public:
    /// <summary>Makes the renderer's pipelines and fixed textures on <paramref name="device"/>.</summary>
    static std::expected<std::unique_ptr<MCVulkanRenderer>, std::string> Create(SDL_GPUDevice* device);

    ~MCVulkanRenderer() override;

    void Clear(_window* target, const MCRect& rect, uint8_t color) override;
    void Hash(_window* target, const MCRect& rect, uint8_t color) override;
    void Copy(_window* target, const MCCopyCommand& command) override;
    void AlphaBlit(_window* target, const MCAlphaBlitCommand& command) override;
    void ShapeBlit(_window* target, const MCShapeBlitCommand& command) override;
    void Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count) override;
    void Pixel(_window* target, int32_t x, int32_t y, uint8_t color) override;
    void Shape(_window* target, const MCShapeCommand& command) override;
    void FastShape(_window* target, const MCFastShapeCommand& command) override;
    void Tile(_window* target, const MCTileCommand& command) override;
    void Polygon(_window* target, const MCPolygonCommand& command) override;
    void MapQuad(_window* target, const MCMapQuadCommand& command) override;
    void Line(_window* target, const MCLineCommand& command) override;
    void Ellipse(_window* target, const MCEllipseCommand& command) override;
    void StatusBar(_window* target, const MCStatusBarCommand& command) override;
    void Glyph(_window* target, const MCGlyphCommand& command) override;

    /// <summary>
    /// Runs the commands recorded since the last call on <paramref name="commands"/>: uploads what they read, then
    /// draws into the surfaces. The surfaces that are underlays (<paramref name="underlays"/>) are drawn first, so the
    /// screen's see-through draws read the finished world.
    /// </summary>
    std::expected<void, std::string> Execute(SDL_GPUCommandBuffer* commands, std::span<const MCUnderlay> underlays);

    /// <summary>
    /// Runs the commands recorded so far (<see cref="Execute"/>) in a command buffer of its own: before a read of the
    /// surfaces ahead of the frame's present, or for a frame that isn't shown.
    /// </summary>
    std::expected<void, std::string> Flush(std::span<const MCUnderlay> underlays);

    /// <summary>
    /// The texture of <paramref name="window"/>'s surface (RGBA, as <see cref="Execute"/> left it) and its size in use;
    /// null when the window has no surface yet.
    /// </summary>
    SDL_GPUTexture* SurfaceTexture(const _window* window, uint32_t& width, uint32_t& height) const;

    /// <summary>
    /// <paramref name="screen"/> as shown, in palette indices with its layout, as the composite shows the GPU's
    /// surfaces: flushes, reads the screen's surface and its underlays' back (waiting for the GPU), and resolves each
    /// key pixel over an underlay (the see-through value, else the world pixel under its centre). Empty when the screen
    /// has no surface yet.
    /// </summary>
    std::expected<std::vector<uint8_t>, std::string> ReadShown(const _window* screen,
                                                               std::span<const MCUnderlay> underlays);

    /// <summary>The result of <see cref="Compare"/>.</summary>
    struct Comparison
    {
        /// <summary>Pixels whose index differs, over every surface.</summary>
        int64_t Different = 0;
        /// <summary>Screen pixels over the world whose shown index differs (the see-through pixels resolved).</summary>
        int64_t ShownDifferent = 0;
        /// <summary>What differed first, for the log ("" when nothing did).</summary>
        std::string First;
    };

    /// <summary>
    /// Mirror mode: downloads every surface (waiting for the GPU) and compares it with the window's own pixels, which
    /// the software renderer drew from the same commands; screen pixels over the world are compared as shown.
    /// </summary>
    std::expected<Comparison, std::string> Compare(std::span<const MCUnderlay> underlays, const SDL_Color* colors);

    /// <summary>The comparisons so far (<see cref="Compare"/> keeps them, and logs the first frames that differ).</summary>
    struct MirrorTally
    {
        int64_t Frames = 0;
        int64_t DifferentFrames = 0;
        /// <summary>The first frame that differed (its number among those compared) and what differed first in it.</summary>
        int64_t FirstFrame = -1;
        std::string First;
        Comparison Last;
    };

    const MirrorTally& Mirror() const { return _Mirror; }

    /// <summary>Starts the tally again.</summary>
    void ResetMirror() { _Mirror = MirrorTally{}; }

    /// <summary>
    /// What a frame sent to the GPU besides its draws and tables: new pictures, new movie frames (decoded on the CPU,
    /// the one upload expected every frame) and new atlas images.
    /// </summary>
    struct UploadTally
    {
        int64_t MovieFrames = 0;
        int64_t Pictures = 0;
        int64_t PictureBytes = 0;
        int64_t AtlasImages = 0;
        int64_t AtlasBytes = 0;
    };

    /// <summary>The uploads of the last frame <see cref="Execute"/> ran.</summary>
    const UploadTally& LastFrameUploads() const { return _LastFrameUploads; }

    /// <summary>The commands the GPU can't draw yet that were asked of it (each logged once).</summary>
    const std::map<std::string, int64_t>& Unsupported() const { return _Unsupported; }

protected:
    void OnAlphaTableChanged() override {}
    void OnShapesForgotten(const void*, size_t) override {}

    /// <summary>The surface is let go: its commands still run, then its textures go (after the next execution).</summary>
    void OnFrameSurfaceRemoved(const _window* window) override;

private:
    MCVulkanRenderer() = default;

    /// <summary>A texture and its allocated size.</summary>
    struct Texture
    {
        SDL_GPUTexture* Handle = nullptr;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    /// <summary>A frame surface on the GPU.</summary>
    struct Surface
    {
        const _window* Window = nullptr;
        /// <summary>The surface (RGBA, colour target) and the copy draws that read it read.</summary>
        Texture Target;
        Texture Copy;
        /// <summary>The size the commands recorded last saw (the window's).</summary>
        uint32_t RecordedWidth = 0;
        uint32_t RecordedHeight = 0;
        /// <summary>Execution: the part of the surface the copy holds, and what was drawn since it was taken.</summary>
        MCRect Copied{0, 0, -1, -1};
        std::vector<MCRect> DrawnSinceCopy;
    };

    /// <summary>What a draw reads its pixels from.</summary>
    enum class SourceKind : uint8_t
    {
        None,
        /// <summary>An atlas page (<c>SourceIndex</c>).</summary>
        Atlas,
        /// <summary>A picture's texture (<c>SourceIndex</c> into the frame's picture list).</summary>
        Picture,
        /// <summary>Another surface (<c>SourceIndex</c>).</summary>
        Surface,
        /// <summary>The target's own copy (a copy within the surface).</summary>
        TargetCopy,
        /// <summary>The frame's strip of written pixels (<see cref="Write"/>).</summary>
        Strip
    };

    /// <summary>One recorded command: a run of draws with the same target and bindings, or a surface resize.</summary>
    struct Record
    {
        bool Resize = false;
        uint16_t SurfaceIndex = 0;
        SourceKind Source = SourceKind::None;
        uint32_t SourceIndex = 0;
        /// <summary>Whether all four channels are written (a clear), or only the index.</summary>
        bool WriteAll = false;
        /// <summary>Whether the draws read the target's copy, and the part of the target they read.</summary>
        bool ReadsCopy = false;
        MCRect Reads{0, 0, -1, -1};
        /// <summary>The part of the target drawn.</summary>
        MCRect Writes{0, 0, -1, -1};
        uint32_t First = 0;
        uint32_t Count = 0;
    };

    /// <summary>The surface for <paramref name="window"/> (made on first use); records a resize when its size changed.</summary>
    uint16_t SurfaceFor(const _window* window);
    /// <summary>Starts a frame's recording, if this is its first command.</summary>
    void BeginRecording();
    /// <summary>
    /// Adds a draw to the frame; returns false (and draws nothing) when its rectangle is empty. With
    /// <paramref name="join"/>, the draw belongs to the same command as the one before and touches none of its pixels,
    /// so it can read the same copy of the target.
    /// </summary>
    bool Add(uint16_t surface, SourceKind source, uint32_t sourceIndex, bool writeAll, const int32_t rect[4],
             const int32_t sourceAt[4], uint32_t op, uint32_t color, uint32_t before, uint32_t after,
             bool join = false);
    /// <summary>What a draw reads from a window: its surface, the target's copy, or a picture of its pixels.</summary>
    struct SourceRef
    {
        SourceKind Kind = SourceKind::None;
        uint32_t Index = 0;
    };

    std::optional<SourceRef> SourceFor(uint16_t target, const _window* window);
    /// <summary>
    /// Adds a span of a polygon or quadrilateral (<paramref name="from"/>: the texture's width and height for a texel
    /// walk), with its values.
    /// </summary>
    bool AddSpan(uint16_t surface, const SourceRef& source, const int32_t at[4], const int32_t from[4], uint32_t op,
                 uint32_t color, uint32_t before, uint32_t after, const MCSpan& span, bool join);
    /// <summary>The frame's picture index of <paramref name="height"/> rows of <paramref name="width"/> bytes (uploaded
    /// when new).</summary>
    std::optional<uint32_t> PictureForBytes(const uint8_t* pixels, uint32_t width, uint32_t height, bool movie = false);
    /// <summary>The frame's picture index of the picture keyed by <paramref name="key"/>; a new one is
    /// <paramref name="width"/> x <paramref name="height"/> bytes that <paramref name="fill"/> writes (counted as a movie
    /// frame with <paramref name="movie"/>).</summary>
    std::optional<uint32_t> PictureForKey(uint64_t key, uint32_t width, uint32_t height,
                                          const std::function<void(uint8_t*)>& fill, bool movie = false);
    /// <summary>The row of the frame's table texture holding <paramref name="table"/> (interned by content).</summary>
    uint32_t TableRow(const uint8_t* table);
    /// <summary>Makes sure the alpha table on the GPU is the current AlphaTable.</summary>
    void SyncAlphaTable();
    /// <summary>
    /// The atlas place of an image keyed by <paramref name="key"/>: an existing one, or a new one that
    /// <paramref name="decode"/> fills (width x height pixels of value + state).
    /// </summary>
    struct AtlasPlace
    {
        uint32_t Page = 0;
        int32_t X = 0;
        int32_t Y = 0;
    };

    std::optional<AtlasPlace> AtlasFor(uint64_t key, int32_t width, int32_t height,
                                       const std::function<void(uint8_t*)>& decode);
    /// <summary>The frame's picture index of <paramref name="window"/>'s pixels (uploaded when new).</summary>
    std::optional<uint32_t> PictureFor(const _window* window);
    /// <summary>Queues <paramref name="size"/> bytes for an upload into a rectangle of a texture.</summary>
    uint8_t* QueueUpload(SDL_GPUTexture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                         uint32_t bytesPerPixel);
    /// <summary>Notes a command the GPU can't draw yet.</summary>
    void NotSupported(const char* command);

    std::expected<void, std::string> EnsureTexture(Texture& texture, SDL_GPUTextureFormat format,
                                                   SDL_GPUTextureUsageFlags usage, uint32_t width, uint32_t height);
    void Release(Texture& texture);
    /// <summary>Takes the copy of a surface's <paramref name="rect"/> (between render passes).</summary>
    void TakeCopy(SDL_GPUCommandBuffer* commands, Surface& surface, const MCRect& rect);
    /// <summary>Reads <paramref name="surfaces"/>' textures back (RGBA, each its texture's size), waiting for the GPU.</summary>
    std::expected<std::vector<std::vector<uint8_t>>, std::string> Download(std::span<const Surface* const> surfaces);

    SDL_GPUDevice* _Device = nullptr;
    SDL_GPUShader* _VertexShader = nullptr;
    SDL_GPUShader* _FragmentShader = nullptr;
    /// <summary>Writing the index only, and writing all four channels.</summary>
    SDL_GPUGraphicsPipeline* _WriteIndex = nullptr;
    SDL_GPUGraphicsPipeline* _WriteAll = nullptr;
    SDL_GPUSampler* _Nearest = nullptr;
    /// <summary>A 1x1 texture bound where a draw has no source or no world.</summary>
    Texture _Blank;

    std::vector<Surface> _Surfaces;

    // The frame being recorded.
    bool _Recording = false;
    uint64_t _Frame = 0;
    std::vector<Record> _Records;
    std::vector<std::array<int32_t, 20>> _Draws;
    /// <summary>The frame's tables (256 bytes a row, row 0 the identity), by content.</summary>
    std::vector<uint8_t> _Tables;
    std::unordered_map<uint64_t, uint32_t> _TableRows;
    Texture _TableTexture;
    Texture _AlphaTexture;
    /// <summary>The pixels <see cref="Write"/> was given this frame: a row each (StripWidth wide), and the texture.</summary>
    std::vector<uint8_t> _StripPixels;
    uint32_t _StripRows = 0;
    Texture _Strip;
    uint64_t _AlphaKey = 0;

    /// <summary>Uploads waiting for the next <see cref="Execute"/>.</summary>
    struct Upload
    {
        SDL_GPUTexture* Texture;
        uint32_t X;
        uint32_t Y;
        uint32_t Width;
        uint32_t Height;
        uint32_t BytesPerPixel;
        size_t Offset;
    };

    std::vector<Upload> _Uploads;
    std::vector<uint8_t> _Staging;
    SDL_GPUTransferBuffer* _Transfer = nullptr;
    uint32_t _TransferSize = 0;
    SDL_GPUBuffer* _DrawBuffer = nullptr;
    uint32_t _DrawBufferSize = 0;

    /// <summary>The atlas: pages of value + state (R8G8), filled in shelves.</summary>
    struct Page
    {
        Texture Texture;
        int32_t ShelfX = 0;
        int32_t ShelfY = 0;
        int32_t ShelfHeight = 0;
    };

    std::vector<Page> _Pages;
    std::unordered_map<uint64_t, AtlasPlace> _Atlas;
    /// <summary>Execution: the world texture bound for the render pass under way.</summary>
    SDL_GPUTexture* _PassWorld = nullptr;

    /// <summary>Pictures (windows in memory) by content, and when each was used last.</summary>
    struct Picture
    {
        Texture Texture;
        uint64_t LastFrame = 0;
    };

    std::unordered_map<uint64_t, Picture> _Pictures;
    size_t _PictureBytes = 0;
    /// <summary>The frame's pictures in use (their textures), and a memo of the windows hashed this frame.</summary>
    std::vector<SDL_GPUTexture*> _FramePictures;
    struct PictureMemo
    {
        const uint8_t* Buffer;
        int32_t XMax;
        int32_t YMax;
        uint32_t Version;
        uint32_t Index;
    };

    std::unordered_map<const _window*, PictureMemo> _PictureMemo;

    std::map<std::string, int64_t> _Unsupported;
    /// <summary>The uploads of the frame being recorded, and of the last one run.</summary>
    UploadTally _FrameUploads;
    UploadTally _LastFrameUploads;
    /// <summary>The span walks' state (as the software renderer keeps its own).</summary>
    MCSpanState _Spans;
    MirrorTally _Mirror;
    /// <summary>The surfaces dumped (-gpudump).</summary>
    std::set<const _window*> _Dumped;
};
