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

    /// <summary>The frame surfaces.</summary>
    std::vector<const _window*>& FrameSurfaces()
    {
        static auto* surfaces = new std::vector<const _window*>();
        return *surfaces;
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
            ByHash.clear();
            Composed.clear();
        }

        std::array<uint8_t, 256 * 256> Rows{};
        int32_t Count = 1;
        /// <summary>The rows by an FNV-1a hash of their bytes.</summary>
        std::unordered_multimap<uint32_t, uint8_t> ByHash;
        /// <summary>first << 8 | second → their composition.</summary>
        std::unordered_map<uint16_t, uint8_t> Composed;
    };

    OpTableSet& Tables()
    {
        static auto* tables = new OpTableSet();
        return *tables;
    }

    uint32_t HashTable(const uint8_t* table)
    {
        uint32_t hash = 0x811c9dc5;

        for (int32_t i = 0; i < 256; ++i)
        {
            hash = (hash ^ table[i]) * 0x01000193;
        }

        return hash;
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
        void OnShapesForgotten(const void*, size_t) override {}

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

    protected:
        void OnAlphaTableChanged() override {}
        void OnShapesForgotten(const void*, size_t) override {}

    private:
        static MCRenderer& Gpu() { return *HardwareRenderer; }
        static MCRenderer& Cpu() { return MCSoftwareRenderer::Instance(); }
    };
}

MCRenderer& MCRenderer::For(const _window* window)
{
    if (window != nullptr && window->View != nullptr)
    {
        static MCViewRenderer viewRenderer;
        return viewRenderer.Bind(window->View);
    }

    if (Drawing != MCGpuDrawing::Off && FrameSurfaceOf(window) != nullptr)
    {
        static MCMirrorRenderer mirror;
        return Drawing == MCGpuDrawing::Mirror ? static_cast<MCRenderer&>(mirror) : *HardwareRenderer;
    }

    if (window != nullptr)
    {
        ++window->Version;
    }

    return MCSoftwareRenderer::Instance();
}

void MCRenderer::AddFrameSurface(const _window* window)
{
    auto& surfaces = FrameSurfaces();

    if (std::ranges::find(surfaces, window) == surfaces.end())
    {
        surfaces.push_back(window);
    }
}

void MCRenderer::RemoveFrameSurface(const _window* window)
{
    if (std::erase(FrameSurfaces(), window) != 0 && HardwareRenderer != nullptr)
    {
        HardwareRenderer->OnFrameSurfaceRemoved(window);
    }
}

const _window* MCRenderer::FrameSurfaceOf(const _window* window)
{
    if (window == nullptr)
    {
        return nullptr;
    }

    for (const _window* surface : FrameSurfaces())
    {
        if (surface == window || (window->buffer != nullptr && window->buffer == surface->buffer))
        {
            return surface;
        }
    }

    return nullptr;
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
    if (Requested)
    {
        return *Requested;
    }

    const char* value = SDL_getenv("MC_GPU_DRAW");

    if (value != nullptr && SDL_strcasecmp(value, "on") == 0)
    {
        return MCGpuDrawing::On;
    }

    if (value != nullptr && SDL_strcasecmp(value, "mirror") == 0)
    {
        return MCGpuDrawing::Mirror;
    }

    return MCGpuDrawing::Off;
}

void MCRenderer::RequestGpuDrawing(MCGpuDrawing drawing)
{
    Requested = drawing;
}

void MCRenderer::AlphaTableChanged()
{
    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnAlphaTableChanged();
    }
}

void MCRenderer::ForgetShapes(const void* begin, size_t size)
{
    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnShapesForgotten(begin, size);
    }
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
    const uint32_t hash = HashTable(table);
    const auto [first, last] = tables.ByHash.equal_range(hash);

    for (auto it = first; it != last; ++it)
    {
        if (std::memcmp(&tables.Rows[static_cast<size_t>(it->second) * 256], table, 256) == 0)
        {
            return it->second;
        }
    }

    if (std::memcmp(tables.Rows.data(), table, 256) == 0)
    {
        return 0;
    }

    if (tables.Count == 256)
    {
        return 0;
    }

    const auto op = static_cast<uint8_t>(tables.Count++);
    std::memcpy(&tables.Rows[static_cast<size_t>(op) * 256], table, 256);
    tables.ByHash.emplace(hash, op);
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

    std::array<uint8_t, 256> composed{};
    const uint8_t* a = &tables.Rows[static_cast<size_t>(first) * 256];
    const uint8_t* b = &tables.Rows[static_cast<size_t>(second) * 256];

    for (size_t i = 0; i < 256; ++i)
    {
        composed[i] = b[a[i]];
    }

    const uint8_t op = OpFor(composed.data());

    // A full table set gives 0, which stands for the identity only when the composition is the identity.
    if (op != 0 || std::memcmp(composed.data(), tables.Rows.data(), 256) == 0)
    {
        tables.Composed.emplace(key, op);
    }

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
