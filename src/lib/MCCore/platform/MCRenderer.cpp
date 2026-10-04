#include "stdafx.h"
#include "platform/MCRenderer.h"
#include "platform/MCSoftwareRenderer.h"

namespace
{
    /// <summary>The hardware renderer and who draws the frame surfaces.</summary>
    MCRenderer* HardwareRenderer = nullptr;
    MCGpuDrawing Drawing = MCGpuDrawing::Off;
    std::optional<MCGpuDrawing> Requested;

    /// <summary>Every renderer there is.</summary>
    std::vector<MCRenderer*> AllRenderers()
    {
        std::vector<MCRenderer*> renderers{&MCSoftwareRenderer::Instance()};

        if (HardwareRenderer != nullptr)
        {
            renderers.push_back(HardwareRenderer);
        }

        return renderers;
    }

    // The registries are never destroyed: the display and the view windows (globals, and objects on the game's heaps)
    // remove themselves from them as they go, which can be during the exit's static destruction.

    /// <summary>A frame surface, and whether its memory is kept drawn too.</summary>
    struct FrameSurface
    {
        const _window* Window;
        bool Kept;
    };

    /// <summary>The frame surfaces.</summary>
    std::vector<FrameSurface>& FrameSurfaces()
    {
        static auto* surfaces = new std::vector<FrameSurface>();
        return *surfaces;
    }

    /// <summary>The frame surface <paramref name="window"/> is (or lies over the pixels of), or null.</summary>
    const FrameSurface* FrameSurfaceEntry(const _window* window)
    {
        if (window == nullptr)
        {
            return nullptr;
        }

        for (const FrameSurface& surface : FrameSurfaces())
        {
            if (surface.Window == window || (window->buffer != nullptr && window->buffer == surface.Window->buffer))
            {
                return &surface;
            }
        }

        return nullptr;
    }

    /// <summary>The underlays set (see <see cref="MCRenderer::SetUnderlay"/>).</summary>
    std::vector<MCUnderlay>& UnderlayList()
    {
        static auto* underlays = new std::vector<MCUnderlay>();
        return *underlays;
    }

    /// <summary>The windows with an op plane, and their planes.</summary>
    std::vector<std::pair<const _window*, uint8_t*>>& OpPlanes()
    {
        static auto* planes = new std::vector<std::pair<const _window*, uint8_t*>>();
        return *planes;
    }

    /// <summary>The frame's op tables, and their bookkeeping.</summary>
    struct OpTableSet
    {
        OpTableSet() { Reset(); }

        void Reset()
        {
            for (int32_t i = 0; i < 256; ++i)
            {
                Rows[i] = static_cast<uint8_t>(i);
            }

            Count = 1;
            ByTable.clear();
            Composed.clear();
        }

        std::array<uint8_t, 256 * 256> Rows{};
        int32_t Count = 1;
        /// <summary>The rows by the address of the table they were taken from.</summary>
        std::map<const uint8_t*, uint8_t> ByTable;
        /// <summary>first << 8 | second → their composition.</summary>
        std::unordered_map<uint16_t, uint8_t> Composed;
    };

    OpTableSet& Tables()
    {
        static auto* tables = new OpTableSet();
        return *tables;
    }
}

namespace
{
    /// <summary>
    /// The renderer of a view: each command, already clipped to the view's window and scissor by the front end, is
    /// moved by the view's origin and drawn on its target by the target's renderer.
    /// </summary>
    class MCViewRenderer final : public MCRenderer
    {
    public:
        /// <summary>Draws into <paramref name="view"/> until the next call.</summary>
        MCViewRenderer& Bind(const MCView* view)
        {
            SDL_assert(view->Target != nullptr && view->Target->View == nullptr);
            _View = view;
            _X = view->OriginX;
            _Y = view->OriginY;
            return *this;
        }

        void Clear(_window*, const MCRect& rect, uint8_t color) override
        {
            if (!Drops(color))
            {
                Target().Clear(_View->Target, Move(rect), color);
            }
        }

        void Hash(_window*, const MCRect& rect, uint8_t color) override
        {
            if (!Drops(color))
            {
                Target().Hash(_View->Target, Move(rect), color);
            }
        }

        void Copy(_window*, const MCCopyCommand& command) override
        {
            // A view has no pixels to copy from.
            SDL_assert(command.Source->View == nullptr);

            if (command.Source->View != nullptr)
            {
                return;
            }

            MCCopyCommand moved = command;
            moved.X += _X;
            moved.Y += _Y;

            if (_View->KeyTransparent && !moved.ColorKey)
            {
                moved.ColorKey = true;
                moved.Key = 0xff;
            }

            Target().Copy(_View->Target, moved);
        }

        void AlphaBlit(_window*, const MCAlphaBlitCommand& command) override
        {
            MCAlphaBlitCommand moved = command;
            moved.Left += _X;
            moved.Top += _Y;
            Target().AlphaBlit(_View->Target, moved);
        }

        void ShapeBlit(_window*, const MCShapeBlitCommand& command) override
        {
            MCShapeBlitCommand moved = command;
            moved.Blit.Left += _X;
            moved.Blit.Top += _Y;
            Target().ShapeBlit(_View->Target, moved);
        }

        void Write(_window*, int32_t x, int32_t y, const uint8_t* pixels, int32_t count) override
        {
            if (!_View->KeyTransparent)
            {
                Target().Write(_View->Target, x + _X, y + _Y, pixels, count);
                return;
            }

            // The runs between the key pixels.
            int32_t start = 0;

            while (start < count)
            {
                while (start < count && pixels[start] == 0xff)
                {
                    ++start;
                }

                int32_t end = start;

                while (end < count && pixels[end] != 0xff)
                {
                    ++end;
                }

                if (end > start)
                {
                    Target().Write(_View->Target, x + start + _X, y + _Y, pixels + start, end - start);
                }

                start = end;
            }
        }

        void Pixel(_window*, int32_t x, int32_t y, uint8_t color) override
        {
            if (!Drops(color))
            {
                Target().Pixel(_View->Target, x + _X, y + _Y, color);
            }
        }

        void Shape(_window*, const MCShapeCommand& command) override
        {
            MCShapeCommand moved = command;
            moved.Top += _Y;
            moved.Left += _X;
            moved.Lo += _X;
            moved.Hi += _X;
            Target().Shape(_View->Target, moved);
        }

        void FastShape(_window*, const MCFastShapeCommand& command) override
        {
            MCFastShapeCommand moved = command;
            moved.Top += _Y;
            moved.StartX += _X;
            moved.ClipX0 += _X;
            Target().FastShape(_View->Target, moved);
        }

        void Tile(_window*, const MCTileCommand& command) override
        {
            MCTileCommand moved = command;
            moved.Left += _X;
            moved.Top += _Y;
            moved.Lo += _X;
            moved.Hi += _X;
            Target().Tile(_View->Target, moved);
        }

        void Polygon(_window*, const MCPolygonCommand& command) override
        {
            MCPolygonCommand moved = command;
            moved.OriginX += _X;
            moved.OriginY += _Y;
            Target().Polygon(_View->Target, moved);
        }

        void MapQuad(_window*, const MCMapQuadCommand& command) override
        {
            MCMapQuadCommand moved = command;

            for (MCMapQuadVertex& corner : moved.Corners)
            {
                corner.X += _X;
                corner.Y += _Y;
            }

            moved.Clip = Move(command.Clip);
            Target().MapQuad(_View->Target, moved);
        }

        void Line(_window*, const MCLineCommand& command) override
        {
            if (command.Table == nullptr && Drops(command.Color))
            {
                return;
            }

            MCLineCommand moved = command;
            moved.X += _X;
            moved.Y += _Y;
            Target().Line(_View->Target, moved);
        }

        void Ellipse(_window*, const MCEllipseCommand& command) override
        {
            if (!command.Alpha && Drops(command.Color))
            {
                return;
            }

            MCEllipseCommand moved = command;
            moved.CenterX += _X;
            moved.CenterY += _Y;
            moved.Clip = Move(command.Clip);
            Target().Ellipse(_View->Target, moved);
        }

        void StatusBar(_window*, const MCStatusBarCommand& command) override
        {
            MCStatusBarCommand moved = command;
            moved.Box = Move(command.Box);
            moved.FrameTop += _Y;
            moved.FrameBottom += _Y;
            Target().StatusBar(_View->Target, moved);
        }

        void Glyph(_window*, const MCGlyphCommand& command) override
        {
            MCGlyphCommand moved = command;
            moved.X += _X;
            moved.Y += _Y;
            Target().Glyph(_View->Target, moved);
        }

    protected:
        void OnAlphaTableChanged() override {}

    private:
        MCRenderer& Target() const { return MCRenderer::For(_View->Target); }

        MCRect Move(const MCRect& rect) const { return MCRect{rect.X0 + _X, rect.Y0 + _Y, rect.X1 + _X, rect.Y1 + _Y}; }

        /// <summary>Whether a write of <paramref name="color"/> draws nothing (0xff in a colour-keyed view).</summary>
        bool Drops(uint8_t color) const { return _View->KeyTransparent && color == 0xff; }

        const MCView* _View = nullptr;
        int32_t _X = 0;
        int32_t _Y = 0;
    };

    /// <summary>Mirror mode's renderer of the frame surfaces: each command goes to the hardware renderer, then the
    /// software one.</summary>
    class MCMirrorRenderer final : public MCRenderer
    {
    public:
        void Clear(_window* target, const MCRect& rect, uint8_t color) override
        {
            Gpu().Clear(target, rect, color);
            Cpu().Clear(target, rect, color);
        }

        void Hash(_window* target, const MCRect& rect, uint8_t color) override
        {
            Gpu().Hash(target, rect, color);
            Cpu().Hash(target, rect, color);
        }

        void Copy(_window* target, const MCCopyCommand& command) override
        {
            Gpu().Copy(target, command);
            Cpu().Copy(target, command);
        }

        void AlphaBlit(_window* target, const MCAlphaBlitCommand& command) override
        {
            Gpu().AlphaBlit(target, command);
            Cpu().AlphaBlit(target, command);
        }

        void ShapeBlit(_window* target, const MCShapeBlitCommand& command) override
        {
            Gpu().ShapeBlit(target, command);
            Cpu().ShapeBlit(target, command);
        }

        void Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count) override
        {
            Gpu().Write(target, x, y, pixels, count);
            Cpu().Write(target, x, y, pixels, count);
        }

        void Pixel(_window* target, int32_t x, int32_t y, uint8_t color) override
        {
            Gpu().Pixel(target, x, y, color);
            Cpu().Pixel(target, x, y, color);
        }

        void Shape(_window* target, const MCShapeCommand& command) override
        {
            Gpu().Shape(target, command);
            Cpu().Shape(target, command);
        }

        void FastShape(_window* target, const MCFastShapeCommand& command) override
        {
            Gpu().FastShape(target, command);
            Cpu().FastShape(target, command);
        }

        void Tile(_window* target, const MCTileCommand& command) override
        {
            Gpu().Tile(target, command);
            Cpu().Tile(target, command);
        }

        void Polygon(_window* target, const MCPolygonCommand& command) override
        {
            Gpu().Polygon(target, command);
            Cpu().Polygon(target, command);
        }

        void MapQuad(_window* target, const MCMapQuadCommand& command) override
        {
            Gpu().MapQuad(target, command);
            Cpu().MapQuad(target, command);
        }

        void Line(_window* target, const MCLineCommand& command) override
        {
            Gpu().Line(target, command);
            Cpu().Line(target, command);
        }

        void Ellipse(_window* target, const MCEllipseCommand& command) override
        {
            Gpu().Ellipse(target, command);
            Cpu().Ellipse(target, command);
        }

        void StatusBar(_window* target, const MCStatusBarCommand& command) override
        {
            Gpu().StatusBar(target, command);
            Cpu().StatusBar(target, command);
        }

        void Glyph(_window* target, const MCGlyphCommand& command) override
        {
            Gpu().Glyph(target, command);
            Cpu().Glyph(target, command);
        }

        // The GPU draws the layer from its mesh; the software renderer draws the pass's tiles, which still come.
        std::expected<void, std::string> TerrainLayer(_window* target, const MCTerrainFrame& frame) override
        {
            return Gpu().TerrainLayer(target, frame);
        }

        void EndTerrainLayer(_window* target) override { Gpu().EndTerrainLayer(target); }

    protected:
        void OnAlphaTableChanged() override {}

    private:
        static MCRenderer& Gpu() { return *HardwareRenderer; }
        static MCRenderer& Cpu() { return MCSoftwareRenderer::Instance(); }
    };
}

std::expected<void, std::string> MCRenderer::TerrainLayer(_window* /*target*/, const MCTerrainFrame& /*frame*/)
{
    return std::unexpected("this renderer draws no terrain layer");
}

void MCRenderer::EndTerrainLayer(_window* /*target*/)
{
}

MCRenderer& MCRenderer::For(const _window* window)
{
    if (window != nullptr && window->View != nullptr)
    {
        static MCViewRenderer viewRenderer;
        return viewRenderer.Bind(window->View);
    }

    if (Drawing != MCGpuDrawing::Off)
    {
        if (const FrameSurface* surface = FrameSurfaceEntry(window); surface != nullptr)
        {
            // A kept surface's memory is read by the game, so the software renderer draws it as well.
            static MCMirrorRenderer mirror;
            return Drawing == MCGpuDrawing::Mirror || surface->Kept ? static_cast<MCRenderer&>(mirror)
                                                                    : *HardwareRenderer;
        }
    }

    // The software renderer draws into the picture's memory: its texture is no longer what the GPU holds.
    if (window != nullptr && window->Texture != nullptr)
    {
        window->Texture->Dirty = true;

        if (window->Texture->CpuStale)
        {
            NoteStale("a draw into a stream texture whose pixels went to the GPU alone");
        }
    }

    return MCSoftwareRenderer::Instance();
}

namespace
{
    /// <summary>The textures made and not yet destroyed (each knows its slot).</summary>
    std::vector<MCTexture*>& TextureList()
    {
        static auto* textures = new std::vector<MCTexture*>();
        return *textures;
    }

    int64_t Unregistered = 0;

    /// <summary>How many <see cref="MCRenderer::ExpectUnregistered"/> scopes are alive.</summary>
    int32_t UnregisteredExpected = 0;
}

MCTexture* MCRenderer::CreateTexture(uint8_t* pixels, int32_t width, int32_t height, MCTextureUse use)
{
    auto* texture = new MCTexture{};
    texture->Pixels = pixels;
    texture->Width = width;
    texture->Height = height;
    texture->Use = use;
    texture->Slot = TextureList().size();
    TextureList().push_back(texture);
    return texture;
}

MCTexture* MCRenderer::CreateTexture(_window* window, MCTextureUse use)
{
    DestroyTexture(window);
    window->Texture = CreateTexture(window->buffer, window->x_max + 1, window->y_max + 1, use);
    return window->Texture;
}

void MCRenderer::ResizeTexture(MCTexture* texture, uint8_t* pixels, int32_t width, int32_t height)
{
    if (texture->Width != width || texture->Height != height)
    {
        if (HardwareRenderer != nullptr)
        {
            HardwareRenderer->OnTextureReleased(texture);
        }

        texture->Width = width;
        texture->Height = height;
    }

    texture->Pixels = pixels;
    texture->Dirty = true;
}

void MCRenderer::ResizeTexture(_window* window)
{
    if (window->Texture != nullptr)
    {
        ResizeTexture(window->Texture, window->buffer, window->x_max + 1, window->y_max + 1);
    }
}

void MCRenderer::DestroyTexture(MCTexture*& texture)
{
    if (texture == nullptr)
    {
        return;
    }

    if (HardwareRenderer != nullptr)
    {
        HardwareRenderer->OnTextureReleased(texture);
    }

    auto& textures = TextureList();
    textures[texture->Slot] = textures.back();
    textures[texture->Slot]->Slot = texture->Slot;
    textures.pop_back();
    delete texture;
    texture = nullptr;
}

void MCRenderer::DestroyTexture(_window* window)
{
    if (window != nullptr)
    {
        DestroyTexture(window->Texture);
    }
}

uint8_t* MCRenderer::LockTexture(MCTexture* texture)
{
    return LockTexture(texture, MCRect{0, 0, texture->Width - 1, texture->Height - 1});
}

uint8_t* MCRenderer::LockTexture(MCTexture* texture, const MCRect& rect)
{
    SDL_assert(rect.X0 >= 0 && rect.Y0 >= 0 && rect.X1 < texture->Width && rect.Y1 < texture->Height &&
               rect.X0 <= rect.X1 && rect.Y0 <= rect.Y1);
    texture->Locked = rect;
    texture->LockedOnHardware = false;

    if (texture->Use == MCTextureUse::Stream && HardwareRenderer != nullptr && Drawing == MCGpuDrawing::On)
    {
        if (uint8_t* mapped = HardwareRenderer->OnLockStream(texture); mapped != nullptr)
        {
            texture->LockedOnHardware = true;
            return mapped;
        }
    }

    return texture->Pixels + static_cast<ptrdiff_t>(rect.Y0) * texture->Width + rect.X0;
}

void MCRenderer::UnlockTexture(MCTexture* texture)
{
    if (texture->LockedOnHardware)
    {
        HardwareRenderer->OnUnlockStream(texture, nullptr);
        texture->LockedOnHardware = false;
        texture->CpuStale = true;
        return;
    }

    // Mirror mode sends a stream's pixels up the same way, from its memory.
    if (texture->Use == MCTextureUse::Stream && HardwareRenderer != nullptr && Drawing == MCGpuDrawing::Mirror &&
        !texture->Dirty)
    {
        const MCRect& rect = texture->Locked;
        HardwareRenderer->OnUnlockStream(texture,
                                         texture->Pixels + static_cast<ptrdiff_t>(rect.Y0) * texture->Width + rect.X0);
        return;
    }

    texture->Dirty = true;
}

std::span<MCTexture* const> MCRenderer::Textures()
{
    return TextureList();
}

void MCRenderer::NoteUnregistered(const char* what)
{
    static std::set<std::string> logged;

    if (logged.insert(what).second)
    {
        SDL_Log("MCRenderer: unregistered: %s (its owner never registered it)", what);
    }

    ++Unregistered;
    SDL_assert(UnregisteredExpected > 0 && "a draw read data its owner never registered (logged above)");
}

int64_t MCRenderer::UnregisteredDraws()
{
    return Unregistered;
}

MCRenderer::ExpectUnregistered::ExpectUnregistered()
{
    ++UnregisteredExpected;
}

MCRenderer::ExpectUnregistered::~ExpectUnregistered()
{
    --UnregisteredExpected;
}

void MCRenderer::AddFrameSurface(const _window* window, bool kept)
{
    auto& surfaces = FrameSurfaces();

    if (std::ranges::any_of(surfaces, [window](const FrameSurface& surface) { return surface.Window == window; }))
    {
        return;
    }

    surfaces.push_back(FrameSurface{window, kept});

    // A kept surface may hold pixels already: the GPU's copy starts from them.
    if (kept && Drawing != MCGpuDrawing::Off && window->buffer != nullptr)
    {
        auto* target = const_cast<_window*>(window);
        const int32_t width = window->x_max + 1;

        for (int32_t y = 0; y <= window->y_max; ++y)
        {
            HardwareRenderer->Write(target, 0, y, window->buffer + static_cast<size_t>(y) * width, width);
        }
    }
}

void MCRenderer::RemoveFrameSurface(const _window* window)
{
    if (std::erase_if(FrameSurfaces(), [window](const FrameSurface& surface) { return surface.Window == window; }) !=
            0 &&
        HardwareRenderer != nullptr)
    {
        HardwareRenderer->OnFrameSurfaceRemoved(window);
    }
}

const _window* MCRenderer::FrameSurfaceOf(const _window* window)
{
    const FrameSurface* surface = FrameSurfaceEntry(window);
    return surface != nullptr ? surface->Window : nullptr;
}

bool MCRenderer::KeptSurface(const _window* window)
{
    const FrameSurface* surface = FrameSurfaceEntry(window);
    return surface != nullptr && surface->Kept;
}

void MCRenderer::SetHardware(MCRenderer* hardware, MCGpuDrawing drawing)
{
    HardwareRenderer = hardware;
    Drawing = hardware != nullptr ? drawing : MCGpuDrawing::Off;
}

MCRenderer* MCRenderer::Hardware()
{
    return HardwareRenderer;
}

MCGpuDrawing MCRenderer::GpuDrawing()
{
    return Drawing;
}

MCGpuDrawing MCRenderer::RequestedGpuDrawing()
{
    return Requested.value_or(MCGpuDrawing::On);
}

void MCRenderer::RequestGpuDrawing(MCGpuDrawing drawing)
{
    Requested = drawing;
}

namespace
{
    std::filesystem::path& DumpFolder()
    {
        static auto* folder = new std::filesystem::path();
        return *folder;
    }
}

const std::filesystem::path& MCRenderer::MirrorDumpFolder()
{
    return DumpFolder();
}

void MCRenderer::SetMirrorDumpFolder(const std::filesystem::path& folder)
{
    DumpFolder() = folder;
}

std::optional<MCGpuDrawing> MCGpuDrawingFromName(std::string_view name)
{
    for (const auto& [text, drawing] : {std::pair{"off", MCGpuDrawing::Off}, std::pair{"on", MCGpuDrawing::On},
                                        std::pair{"mirror", MCGpuDrawing::Mirror}})
    {
        if (name.size() == std::strlen(text) && SDL_strncasecmp(name.data(), text, name.size()) == 0)
        {
            return drawing;
        }
    }

    return std::nullopt;
}

namespace
{
    int64_t StaleReads = 0;
}

void MCRenderer::NoteStale(const char* what)
{
    static std::set<std::string> logged;

    if (logged.insert(what).second)
    {
        SDL_Log("MCRenderer: %s; its memory is stale", what);
    }

    ++StaleReads;
}

void MCRenderer::NoteCpuRead(const _window* source, const char* command)
{
    if (source != nullptr && source->Texture != nullptr && source->Texture->CpuStale)
    {
        NoteStale(std::format("{} reads a stream texture whose pixels went to the GPU alone", command).c_str());
        return;
    }

    const FrameSurface* surface = FrameSurfaceEntry(source);

    if (Drawing != MCGpuDrawing::On || surface == nullptr || surface->Kept)
    {
        return;
    }

    NoteStale(std::format("{} reads a frame surface the GPU draws", command).c_str());
}

int64_t MCRenderer::StaleCpuReads()
{
    return StaleReads;
}

void MCRenderer::AlphaTableChanged()
{
    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnAlphaTableChanged();
    }
}

namespace
{
    /// <summary>The registered data blocks, by their first byte.</summary>
    std::map<const uint8_t*, MCDataBlock>& DataBlocks()
    {
        static auto* blocks = new std::map<const uint8_t*, MCDataBlock>();
        return *blocks;
    }

    /// <summary>The block <see cref="MCRenderer::DataBlockOf"/> found last (most draws read the block before's).</summary>
    const MCDataBlock* LastDataBlock = nullptr;

    /// <summary>Drops a registered block: every renderer forgets what it made of it.</summary>
    std::map<const uint8_t*, MCDataBlock>::iterator DropDataBlock(std::map<const uint8_t*, MCDataBlock>::iterator it)
    {
        const MCDataBlock block = it->second;
        LastDataBlock = nullptr;
        it = DataBlocks().erase(it);
        MCRenderer::DataChanged(block.Begin, static_cast<size_t>(block.End - block.Begin));
        return it;
    }
}

void MCRenderer::RegisterData(const void* begin, size_t size, MCDataKind kind)
{
    const auto* first = static_cast<const uint8_t*>(begin);
    const uint8_t* end = first + size;

    if (first == nullptr || size == 0)
    {
        return;
    }

    auto& blocks = DataBlocks();
    auto it = blocks.upper_bound(first);

    // A block over any of these bytes is gone: the memory holds the new data now.
    if (it != blocks.begin() && std::prev(it)->second.End > first)
    {
        DropDataBlock(std::prev(it));
    }

    for (it = blocks.lower_bound(first); it != blocks.end() && it->first < end;)
    {
        it = DropDataBlock(it);
    }

    blocks.emplace(first, MCDataBlock{first, end, kind});
}

void MCRenderer::DataChanged(const void* begin, size_t size)
{
    // The frame's op rows taken from the bytes keep the old ones (draws made before used them); a new use takes the
    // new bytes.
    auto& byTable = Tables().ByTable;
    const auto* first = static_cast<const uint8_t*>(begin);
    byTable.erase(byTable.lower_bound(MCFirstTableReaching(first)), byTable.lower_bound(first + size));

    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnDataChanged(begin, size);
    }
}

void MCRenderer::UnregisterData(const void* begin, size_t size)
{
    const auto* first = static_cast<const uint8_t*>(begin);
    auto& blocks = DataBlocks();

    for (auto it = blocks.lower_bound(first); it != blocks.end() && static_cast<size_t>(it->first - first) < size;)
    {
        it = DropDataBlock(it);
    }
}

const MCDataBlock* MCRenderer::DataBlockOf(const void* at)
{
    const auto* p = static_cast<const uint8_t*>(at);

    if (LastDataBlock != nullptr && p >= LastDataBlock->Begin && p < LastDataBlock->End)
    {
        return LastDataBlock;
    }

    auto& blocks = DataBlocks();
    auto it = blocks.upper_bound(p);

    if (it == blocks.begin() || p >= std::prev(it)->second.End)
    {
        return nullptr;
    }

    LastDataBlock = &std::prev(it)->second;
    return LastDataBlock;
}

size_t MCRenderer::DataBlockCount()
{
    return DataBlocks().size();
}

void MCRenderer::SetUnderlay(const MCUnderlay& underlay)
{
    for (MCUnderlay& existing : UnderlayList())
    {
        if (existing.Owner == underlay.Owner)
        {
            existing = underlay;
            return;
        }
    }

    UnderlayList().push_back(underlay);
}

void MCRenderer::RemoveUnderlay(const void* owner)
{
    std::erase_if(UnderlayList(), [owner](const MCUnderlay& underlay) { return underlay.Owner == owner; });
}

std::span<const MCUnderlay> MCRenderer::Underlays()
{
    return UnderlayList();
}

void MCRenderer::SetOpPlane(const _window* target, uint8_t* ops)
{
    auto& planes = OpPlanes();
    std::erase_if(planes, [target](const auto& plane) { return plane.first == target; });

    if (ops != nullptr)
    {
        planes.emplace_back(target, ops);
    }
}

uint8_t* MCRenderer::OpPlane(const _window* target)
{
    for (const auto& [window, ops] : OpPlanes())
    {
        if (window == target || (window->buffer != nullptr && window->buffer == target->buffer))
        {
            return ops;
        }
    }

    return nullptr;
}

const uint8_t* MCRenderer::OpTables()
{
    return Tables().Rows.data();
}

int32_t MCRenderer::OpTableCount()
{
    return Tables().Count;
}

uint8_t MCRenderer::OpFor(const uint8_t* table)
{
    OpTableSet& tables = Tables();

    if (const auto found = tables.ByTable.find(table); found != tables.ByTable.end())
    {
        return found->second;
    }

    const MCDataBlock* block = DataBlockOf(table);

    if (block == nullptr || block->Kind != MCDataKind::Tables || block->End - table < 256)
    {
        NoteUnregistered("a colour table mapping an underlay");
    }

    if (tables.Count == 256)
    {
        return 0;
    }

    const auto op = static_cast<uint8_t>(tables.Count++);
    std::memcpy(&tables.Rows[static_cast<size_t>(op) * 256], table, 256);
    tables.ByTable.emplace(table, op);
    return op;
}

uint8_t MCRenderer::ComposeOps(uint8_t first, uint8_t second)
{
    if (first == 0)
    {
        return second;
    }

    if (second == 0)
    {
        return first;
    }

    OpTableSet& tables = Tables();
    const auto key = static_cast<uint16_t>(first << 8 | second);

    if (const auto found = tables.Composed.find(key); found != tables.Composed.end())
    {
        return found->second;
    }

    if (tables.Count == 256)
    {
        return 0;
    }

    const auto op = static_cast<uint8_t>(tables.Count++);
    uint8_t* composed = &tables.Rows[static_cast<size_t>(op) * 256];
    const uint8_t* a = &tables.Rows[static_cast<size_t>(first) * 256];
    const uint8_t* b = &tables.Rows[static_cast<size_t>(second) * 256];

    for (size_t i = 0; i < 256; ++i)
    {
        composed[i] = b[a[i]];
    }

    tables.Composed.emplace(key, op);
    return op;
}

void MCRenderer::ResetOpTables()
{
    Tables().Reset();
}

void MCRenderer::ComposeUnderlays(const _window* target, uint8_t* pixels, const MCRect& rect)
{
    const int32_t stride = target->x_max + 1;
    const uint8_t* ops = OpPlane(target);
    const uint8_t* tables = OpTables();

    for (const MCUnderlay& underlay : UnderlayList())
    {
        if (underlay.Target == nullptr || underlay.Target->buffer != target->buffer || underlay.Source == nullptr ||
            underlay.Source->buffer == nullptr)
        {
            continue;
        }

        const MCRect& shown = underlay.Rect;
        const int32_t x0 = std::max({rect.X0, shown.X0, 0});
        const int32_t y0 = std::max({rect.Y0, shown.Y0, 0});
        const int32_t x1 = std::min({rect.X1, shown.X1, target->x_max});
        const int32_t y1 = std::min({rect.Y1, shown.Y1, target->y_max});

        if (x1 < x0 || y1 < y0)
        {
            continue;
        }

        // The source pixel under each target pixel's centre: (2 * offset + 1) * source / (2 * shown), as a GPU's
        // nearest sampling picks it.
        const int64_t shownWidth = shown.X1 - shown.X0 + 1;
        const int64_t shownHeight = shown.Y1 - shown.Y0 + 1;
        const int64_t sourceWidth = underlay.Source->x_max + 1;
        const int64_t sourceHeight = underlay.Source->y_max + 1;
        const int32_t sourceStride = underlay.Source->x_max + 1;

        for (int32_t y = y0; y <= y1; ++y)
        {
            uint8_t* row = pixels + static_cast<intptr_t>(y) * stride;
            const auto sourceY = static_cast<int32_t>((2 * (y - shown.Y0) + 1) * sourceHeight / (2 * shownHeight));
            const uint8_t* sourceRow = underlay.Source->buffer + static_cast<intptr_t>(sourceY) * sourceStride;

            const uint8_t* opRow = ops != nullptr ? ops + static_cast<intptr_t>(y) * stride : nullptr;

            for (int32_t x = x0; x <= x1; ++x)
            {
                if (row[x] == UnderlayKey)
                {
                    const uint8_t world = sourceRow[(2 * (x - shown.X0) + 1) * sourceWidth / (2 * shownWidth)];
                    const uint8_t op = opRow != nullptr ? opRow[x] : 0;
                    row[x] = tables[static_cast<size_t>(op) * 256 + world];
                }
            }
        }
    }
}
