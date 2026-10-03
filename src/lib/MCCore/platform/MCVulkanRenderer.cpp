#include "stdafx.h"
#include "platform/MCVulkanRenderer.h"
#include "platform/MCSoftwareRenderer.h"
#include "platform/MCVulkanShaders.h"
#include "vfx/vfxint.h"

namespace
{
    std::string SdlError(std::string_view what)
    {
        return std::format("{}: {}", what, SDL_GetError());
    }

    // shaders/draw.pshader's kinds and flags.
    constexpr uint32_t KindFill = 0;
    constexpr uint32_t KindHash = 1;
    constexpr uint32_t KindTexture = 2;
    constexpr uint32_t KindGouraud = 3;
    constexpr uint32_t KindTexelWalk = 4;
    constexpr uint32_t OpaqueSource = 1u << 4;
    constexpr uint32_t FillSkipped = 1u << 5;
    constexpr uint32_t SkipRunKey = 1u << 6;
    constexpr uint32_t TableBefore = 1u << 7;
    constexpr uint32_t SkipKeyAfterTable = 1u << 8;
    constexpr uint32_t ColorKey = 1u << 9;
    constexpr uint32_t UseColor = 1u << 10;
    constexpr uint32_t ReadsDest = 1u << 11;
    constexpr uint32_t AlphaBlend = 1u << 12;
    constexpr uint32_t TableAfter = 1u << 13;
    constexpr uint32_t SeeThrough = 1u << 14;

    /// <summary>An atlas page's width and height.</summary>
    constexpr int32_t PageSize = 2048;
    /// <summary>More pages than this and the atlas starts again at the next frame.</summary>
    constexpr size_t MaxPages = 6;
    /// <summary>A picture not used for this many frames is dropped.</summary>
    constexpr uint64_t PictureFrames = 300;
    /// <summary>The bytes the pictures may take before the oldest go.</summary>
    constexpr size_t PictureBudget = 128 * 1024 * 1024;
    /// <summary>The width of the strip of written pixels.</summary>
    constexpr int32_t StripWidth = 4096;
    /// <summary>Upload offsets are kept on this boundary (the strictest any backend asks of a texture copy).</summary>
    constexpr size_t UploadAlignment = 512;

    /// <summary>The draw shader's vertex uniforms (shaders/instance.vshader, cbuffer Batch).</summary>
    struct BatchUniforms
    {
        float TargetSize[4];
        uint32_t Batch[4];
    };

    /// <summary>The draw shader's fragment uniforms (shaders/draw.pshader, cbuffer Frame).</summary>
    struct FrameUniforms
    {
        int32_t Underlay[4];
        int32_t World[4];
    };

    /// <summary>A 64-bit hash of <paramref name="size"/> bytes, continuing from <paramref name="seed"/>.</summary>
    uint64_t HashBytes(const void* data, size_t size, uint64_t seed)
    {
        const auto* p = static_cast<const uint8_t*>(data);
        uint64_t hash = seed ^ (static_cast<uint64_t>(size) * 0x9e3779b97f4a7c15ull);
        const auto step = [&hash](uint64_t value)
        {
            hash ^= value * 0xbf58476d1ce4e5b9ull;
            hash = std::rotl(hash, 27) * 0x94d049bb133111ebull;
        };

        for (; size >= 8; p += 8, size -= 8)
        {
            uint64_t value;
            std::memcpy(&value, p, 8);
            step(value);
        }

        uint64_t tail = 0;
        std::memcpy(&tail, p, size);
        step(tail ^ (static_cast<uint64_t>(size) << 56));
        hash ^= hash >> 31;
        hash *= 0xd6e8feb86659fd93ull;
        hash ^= hash >> 32;
        return hash;
    }

    /// <summary>Key spaces, so equal bytes of different formats don't share an atlas place.</summary>
    enum class ImageKind : uint64_t
    {
        Shape = 1,
        FastShape,
        Tile,
        Glyph,
        ShapeFill
    };

    uint64_t Seed(ImageKind kind, int32_t width, int32_t height)
    {
        return static_cast<uint64_t>(kind) << 56 ^ static_cast<uint64_t>(static_cast<uint32_t>(width)) << 28 ^
               static_cast<uint32_t>(height);
    }

    bool Empty(const MCRect& rect)
    {
        return rect.X1 < rect.X0 || rect.Y1 < rect.Y0;
    }

    bool Overlaps(const MCRect& a, const MCRect& b)
    {
        return !Empty(a) && !Empty(b) && a.X0 <= b.X1 && b.X0 <= a.X1 && a.Y0 <= b.Y1 && b.Y0 <= a.Y1;
    }

    bool Contains(const MCRect& outer, const MCRect& inner)
    {
        return !Empty(outer) && inner.X0 >= outer.X0 && inner.Y0 >= outer.Y0 && inner.X1 <= outer.X1 &&
               inner.Y1 <= outer.Y1;
    }

    MCRect Union(const MCRect& a, const MCRect& b)
    {
        if (Empty(a))
        {
            return b;
        }

        if (Empty(b))
        {
            return a;
        }

        return MCRect{std::min(a.X0, b.X0), std::min(a.Y0, b.Y0), std::max(a.X1, b.X1), std::max(a.Y1, b.Y1)};
    }

    // Run-length shapes (VFX's format, see MCSoftwareRendererShapes.cpp): each row is tokens; a token's high seven
    // bits count pixels (bit 0: a literal of that many bytes, else a run of the next byte), 0 with bit 0 set skips the
    // next byte's count of pixels, 0 ends the row.

    /// <summary>Walks <paramref name="rows"/> rows from <paramref name="data"/>: the widest row and where they end.</summary>
    const uint8_t* ShapeExtent(const uint8_t* data, int32_t rows, int32_t& width)
    {
        width = 0;

        for (int32_t row = 0; row < rows; ++row)
        {
            int32_t x = 0;

            for (;;)
            {
                const uint8_t token = *data++;
                const int32_t count = token >> 1;

                if (count == 0)
                {
                    if ((token & 1) == 0)
                    {
                        break;
                    }

                    x += *data++;
                    continue;
                }

                data += (token & 1) ? count : 1;
                x += count;
            }

            width = std::max(width, x);
        }

        return data;
    }

    /// <summary>Decodes shape rows into value + state pairs (1 a pixel, 2 a skipped pixel, 0 nothing).</summary>
    void DecodeShape(const uint8_t* data, int32_t rows, int32_t width, uint8_t* out)
    {
        std::memset(out, 0, static_cast<size_t>(width) * rows * 2);

        for (int32_t row = 0; row < rows; ++row)
        {
            uint8_t* line = out + static_cast<size_t>(row) * width * 2;
            int32_t x = 0;

            for (;;)
            {
                const uint8_t token = *data++;
                const int32_t count = token >> 1;

                if (count == 0)
                {
                    if ((token & 1) == 0)
                    {
                        break;
                    }

                    const int32_t skip = *data++;

                    for (int32_t i = 0; i < skip; ++i)
                    {
                        line[(x + i) * 2 + 1] = 2;
                    }

                    x += skip;
                    continue;
                }

                for (int32_t i = 0; i < count; ++i)
                {
                    line[(x + i) * 2] = (token & 1) ? data[i] : data[0];
                    line[(x + i) * 2 + 1] = 1;
                }

                data += (token & 1) ? count : 1;
                x += count;
            }
        }
    }

    /// <summary>
    /// The picture AG_shape_fill makes of a shape in a buffer of its bounds: <paramref name="rows"/> rows of
    /// <paramref name="width"/> bytes, drawn pixels as stored (through <paramref name="table"/> when there is one),
    /// everything else colour 0, each row cut at the buffer's width.
    /// </summary>
    void FillShape(const uint8_t* data, int32_t rows, int32_t width, const uint8_t* table, uint8_t* out)
    {
        std::memset(out, 0, static_cast<size_t>(width) * rows);

        for (int32_t row = 0; row < rows; ++row)
        {
            uint8_t* line = out + static_cast<size_t>(row) * width;
            int32_t x = 0;

            for (;;)
            {
                const uint8_t token = *data++;
                const int32_t count = token >> 1;

                if (count == 0)
                {
                    if ((token & 1) == 0)
                    {
                        break;
                    }

                    x += *data++;
                    continue;
                }

                for (int32_t i = 0; i < count && x + i < width; ++i)
                {
                    const uint8_t pixel = (token & 1) ? data[i] : data[0];
                    line[x + i] = table != nullptr ? table[pixel] : pixel;
                }

                data += (token & 1) ? count : 1;
                x += count;
            }
        }
    }

    uint16_t Read16(const uint8_t* p)
    {
        uint16_t value;
        std::memcpy(&value, p, 2);
        return value;
    }

    // Fast shapes (fastshp.cpp's format): rows of packets covering width + 1 pixels; c < 0x80 is a run of c pixels of
    // the next byte, c >= 0x80 is c - 0x80 literal bytes.

    /// <summary>Where fast-shape row <paramref name="row"/>'s packets end, covering <paramref name="width"/> pixels.</summary>
    const uint8_t* FastRowEnd(const uint8_t* data, int32_t width)
    {
        for (int32_t count = 0; count < width;)
        {
            const uint32_t c = *data++;

            if (c >= 0x80)
            {
                count += static_cast<int32_t>(c) - 0x80;
                data += c - 0x80;
            }
            else
            {
                count += static_cast<int32_t>(c);
                ++data;
            }
        }

        return data;
    }

    /// <summary>Decodes a fast-shape row into value + state pairs (1 a literal pixel, 3 a run's).</summary>
    void DecodeFastRow(const uint8_t* data, int32_t width, uint8_t* line)
    {
        for (int32_t x = 0; x < width;)
        {
            const uint32_t c = *data++;

            if (c >= 0x80)
            {
                const int32_t n = static_cast<int32_t>(c) - 0x80;

                for (int32_t i = 0; i < n && x < width; ++i, ++x)
                {
                    line[x * 2] = data[i];
                    line[x * 2 + 1] = 1;
                }

                data += n;
            }
            else
            {
                const uint8_t color = *data++;

                for (uint32_t i = 0; i < c && x < width; ++i, ++x)
                {
                    line[x * 2] = color;
                    line[x * 2 + 1] = 3;
                }
            }
        }
    }

    /// <summary>Where tile row <paramref name="row"/> starts, from the tile's start.</summary>
    uint32_t TileOffset(const uint8_t* tile, int32_t row)
    {
        return static_cast<uint32_t>(MCVfxRead32(tile + 4 + static_cast<intptr_t>(row) * 4));
    }

    /// <summary>
    /// The index shown at (<paramref name="x"/>, <paramref name="y"/>) of the GPU's screen (<paramref name="pixel"/>,
    /// RGBA) over an underlay shown in <paramref name="shown"/>: the index; at the key, the see-through value a draw
    /// left (blue set), else the world pixel under the pixel's centre (<paramref name="world"/>, RGBA,
    /// <paramref name="worldWidth"/> x <paramref name="worldHeight"/>). The composite shader's rule.
    /// </summary>
    uint8_t ShownIndex(const uint8_t* pixel, int32_t x, int32_t y, const MCRect& shown, const uint8_t* world,
                       int64_t worldWidth, int64_t worldHeight)
    {
        if (pixel[0] != MCRenderer::UnderlayKey)
        {
            return pixel[0];
        }

        if (pixel[2] >= 128)
        {
            return pixel[1];
        }

        const int64_t shownWidth = shown.X1 - shown.X0 + 1;
        const int64_t shownHeight = shown.Y1 - shown.Y0 + 1;
        const auto wx = static_cast<size_t>((2 * (x - shown.X0) + 1) * worldWidth / (2 * shownWidth));
        const auto wy = static_cast<size_t>((2 * (y - shown.Y0) + 1) * worldHeight / (2 * shownHeight));
        return world[(wy * static_cast<size_t>(worldWidth) + wx) * 4];
    }
}

std::expected<std::unique_ptr<MCVulkanRenderer>, std::string> MCVulkanRenderer::Create(SDL_GPUDevice* device)
{
    std::unique_ptr<MCVulkanRenderer> renderer(new MCVulkanRenderer());
    renderer->_Device = device;

    auto vertex = MCVulkanShaders::Load(device, MCVulkanShaders::DrawVertex);

    if (!vertex)
    {
        return std::unexpected(vertex.error());
    }

    renderer->_VertexShader = *vertex;
    auto fragment = MCVulkanShaders::Load(device, MCVulkanShaders::DrawFragment);

    if (!fragment)
    {
        return std::unexpected(fragment.error());
    }

    renderer->_FragmentShader = *fragment;

    for (const bool all : {false, true})
    {
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        target.blend_state.enable_color_write_mask = !all;
        target.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R;
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};
        pipeline.vertex_shader = renderer->_VertexShader;
        pipeline.fragment_shader = renderer->_FragmentShader;
        pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipeline.target_info.num_color_targets = 1;
        pipeline.target_info.color_target_descriptions = &target;
        SDL_GPUGraphicsPipeline* made = SDL_CreateGPUGraphicsPipeline(device, &pipeline);

        if (made == nullptr)
        {
            return std::unexpected(SdlError("SDL_CreateGPUGraphicsPipeline(draw)"));
        }

        (all ? renderer->_WriteAll : renderer->_WriteIndex) = made;
    }

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    renderer->_Nearest = SDL_CreateGPUSampler(device, &sampler);

    if (renderer->_Nearest == nullptr)
    {
        return std::unexpected(SdlError("SDL_CreateGPUSampler"));
    }

    for (auto made : {renderer->EnsureTexture(renderer->_Blank, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                              SDL_GPU_TEXTUREUSAGE_SAMPLER, 1, 1),
                      renderer->EnsureTexture(renderer->_AlphaTexture, SDL_GPU_TEXTUREFORMAT_R8_UNORM,
                                              SDL_GPU_TEXTUREUSAGE_SAMPLER, 256, ALPHA_COLORS)})
    {
        if (!made)
        {
            return std::unexpected(made.error());
        }
    }

    std::memset(renderer->QueueUpload(renderer->_Blank.Handle, 0, 0, 1, 1, 4), 0, 4);
    return renderer;
}

MCVulkanRenderer::~MCVulkanRenderer()
{
    if (_Device == nullptr)
    {
        return;
    }

    SDL_WaitForGPUIdle(_Device);

    for (Surface& surface : _Surfaces)
    {
        Release(surface.Target);
        Release(surface.Copy);
    }

    for (Page& page : _Pages)
    {
        Release(page.Texture);
    }

    for (auto& [key, picture] : _Pictures)
    {
        Release(picture.Texture);
    }

    Release(_Blank);
    Release(_TableTexture);
    Release(_AlphaTexture);
    Release(_Strip);

    if (_Transfer != nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, _Transfer);
    }

    if (_DrawBuffer != nullptr)
    {
        SDL_ReleaseGPUBuffer(_Device, _DrawBuffer);
    }

    if (_Nearest != nullptr)
    {
        SDL_ReleaseGPUSampler(_Device, _Nearest);
    }

    for (SDL_GPUGraphicsPipeline* pipeline : {_WriteIndex, _WriteAll})
    {
        if (pipeline != nullptr)
        {
            SDL_ReleaseGPUGraphicsPipeline(_Device, pipeline);
        }
    }

    for (SDL_GPUShader* shader : {_VertexShader, _FragmentShader})
    {
        if (shader != nullptr)
        {
            SDL_ReleaseGPUShader(_Device, shader);
        }
    }
}

void MCVulkanRenderer::Release(Texture& texture)
{
    if (texture.Handle != nullptr)
    {
        SDL_ReleaseGPUTexture(_Device, texture.Handle);
    }

    texture = Texture{};
}

std::expected<void, std::string> MCVulkanRenderer::EnsureTexture(Texture& texture, SDL_GPUTextureFormat format,
                                                                 SDL_GPUTextureUsageFlags usage, uint32_t width,
                                                                 uint32_t height)
{
    if (texture.Handle != nullptr && texture.Width == width && texture.Height == height)
    {
        return {};
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

uint8_t* MCVulkanRenderer::QueueUpload(SDL_GPUTexture* texture, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                       uint32_t bytesPerPixel)
{
    const size_t offset = (_Staging.size() + UploadAlignment - 1) & ~(UploadAlignment - 1);
    _Staging.resize(offset + static_cast<size_t>(width) * height * bytesPerPixel);
    _Uploads.push_back(Upload{texture, x, y, width, height, bytesPerPixel, offset});
    return _Staging.data() + offset;
}

void MCVulkanRenderer::NotSupported(const char* command)
{
    if (_Unsupported[command]++ == 0)
    {
        SDL_Log("MCVulkanRenderer: %s isn't drawn on the GPU yet", command);
    }
}

void MCVulkanRenderer::BeginRecording()
{
    if (_Recording)
    {
        return;
    }

    _Recording = true;

    // The atlas starts again when it has grown too big (nothing of the last frame is drawn from it any more).
    if (_Pages.size() > MaxPages)
    {
        for (Page& page : _Pages)
        {
            Release(page.Texture);
        }

        _Pages.clear();
        _Atlas.clear();
    }

    // Pictures go when unused for a while, or, oldest first, when they take more than their budget (pictures drawn
    // each frame, such as the shape transforms' work buffer, make a new one every frame).
    const auto drop = [this](auto it)
    {
        _PictureBytes -= static_cast<size_t>(it->second.Texture.Width) * it->second.Texture.Height;
        Release(it->second.Texture);
        return _Pictures.erase(it);
    };

    for (auto it = _Pictures.begin(); it != _Pictures.end();)
    {
        it = it->second.LastFrame + PictureFrames < _Frame ? drop(it) : std::next(it);
    }

    if (_PictureBytes > PictureBudget)
    {
        std::vector<std::pair<uint64_t, uint64_t>> byAge;

        for (const auto& [key, picture] : _Pictures)
        {
            byAge.emplace_back(picture.LastFrame, key);
        }

        std::ranges::sort(byAge);

        for (const auto& [lastFrame, key] : byAge)
        {
            if (_PictureBytes <= PictureBudget / 2)
            {
                break;
            }

            drop(_Pictures.find(key));
        }
    }

    _Tables.assign(256, 0);

    for (size_t i = 0; i < 256; ++i)
    {
        _Tables[i] = static_cast<uint8_t>(i);
    }

    _TableRows.clear();
}

uint16_t MCVulkanRenderer::SurfaceFor(const _window* window)
{
    BeginRecording();
    const _window* canonical = MCRenderer::FrameSurfaceOf(window);

    if (canonical == nullptr)
    {
        canonical = window;
    }

    size_t index = 0;

    while (index < _Surfaces.size() && _Surfaces[index].Window != canonical)
    {
        ++index;
    }

    if (index == _Surfaces.size())
    {
        _Surfaces.push_back(Surface{});
        _Surfaces.back().Window = canonical;
    }

    Surface& surface = _Surfaces[index];
    const auto width = static_cast<uint32_t>(canonical->x_max + 1);
    const auto height = static_cast<uint32_t>(canonical->y_max + 1);

    if (surface.RecordedWidth != width || surface.RecordedHeight != height)
    {
        surface.RecordedWidth = width;
        surface.RecordedHeight = height;
        Record resize;
        resize.Resize = true;
        resize.SurfaceIndex = static_cast<uint16_t>(index);
        _Records.push_back(resize);
    }

    return static_cast<uint16_t>(index);
}

bool MCVulkanRenderer::Add(uint16_t surfaceIndex, SourceKind source, uint32_t sourceIndex, bool writeAll,
                           const int32_t rect[4], const int32_t sourceAt[4], uint32_t op, uint32_t color,
                           uint32_t before, uint32_t after, bool join)
{
    const Surface& surface = _Surfaces[surfaceIndex];
    std::array<int32_t, 20> draw{};
    draw[0] = std::max(rect[0], 0);
    draw[1] = std::max(rect[1], 0);
    draw[2] = std::min(rect[2], static_cast<int32_t>(surface.RecordedWidth) - 1);
    draw[3] = std::min(rect[3], static_cast<int32_t>(surface.RecordedHeight) - 1);

    if (draw[2] < draw[0] || draw[3] < draw[1])
    {
        return false;
    }

    if (sourceAt != nullptr)
    {
        // The source pixel of the clipped corner.
        draw[4] = sourceAt[0] + (draw[0] - rect[0]) * sourceAt[2];
        draw[5] = sourceAt[1] + (draw[1] - rect[1]) * sourceAt[3];
        draw[6] = sourceAt[2];
        draw[7] = sourceAt[3];
    }

    if (MCRenderer::OpPlane(surface.Window) != nullptr && (op & ReadsDest) != 0)
    {
        op |= SeeThrough;
    }

    draw[8] = static_cast<int32_t>(op);
    draw[9] = static_cast<int32_t>(color);
    draw[10] = static_cast<int32_t>(before);
    draw[11] = static_cast<int32_t>(after);

    const MCRect written{draw[0], draw[1], draw[2], draw[3]};
    const bool readsDest = (op & ReadsDest) != 0;
    Record* record = _Records.empty() ? nullptr : &_Records.back();

    // A run of draws with the same target and bindings: those that read the target only when they are one command's
    // and apart (else each must see the ones before).
    const bool extends = record != nullptr && !record->Resize && record->SurfaceIndex == surfaceIndex &&
                         record->Source == source && record->SourceIndex == sourceIndex &&
                         record->WriteAll == (writeAll || readsDest) &&
                         (join || (!record->ReadsCopy && !readsDest && source != SourceKind::TargetCopy)) &&
                         record->First + record->Count == _Draws.size();

    if (!extends)
    {
        Record fresh;
        fresh.SurfaceIndex = surfaceIndex;
        fresh.Source = source;
        fresh.SourceIndex = sourceIndex;
        fresh.WriteAll = writeAll || readsDest;
        fresh.First = static_cast<uint32_t>(_Draws.size());
        _Records.push_back(fresh);
        record = &_Records.back();
    }

    if (readsDest)
    {
        record->ReadsCopy = true;
        record->Reads = Union(record->Reads, written);
    }

    if (source == SourceKind::TargetCopy)
    {
        // The source rectangle of the target, read from the copy.
        const int32_t width = draw[2] - draw[0];
        const int32_t height = draw[3] - draw[1];
        const MCRect read{std::min(draw[4], draw[4] + width * draw[6]), std::min(draw[5], draw[5] + height * draw[7]),
                          std::max(draw[4], draw[4] + width * draw[6]), std::max(draw[5], draw[5] + height * draw[7])};
        record->ReadsCopy = true;
        record->Reads = Union(record->Reads, read);
    }

    record->Writes = Union(record->Writes, written);
    ++record->Count;
    _Draws.push_back(draw);
    return true;
}

bool MCVulkanRenderer::AddSpan(uint16_t surface, const SourceRef& source, const int32_t at[4], const int32_t from[4],
                               uint32_t op, uint32_t color, uint32_t before, uint32_t after, const MCSpan& span,
                               bool join)
{
    if (!Add(surface, source.Kind, source.Index, false, at, nullptr, op, color, before, after, join))
    {
        return false;
    }

    // The span's first column (the values are counted from it, wherever the draw was clipped) and the texture's
    // size, then its values.
    std::array<int32_t, 20>& draw = _Draws.back();
    draw[4] = span.X0;
    draw[6] = from[2];
    draw[7] = from[3];

    if ((op & 15u) == KindGouraud)
    {
        draw[12] = static_cast<int32_t>(span.Value);
        draw[13] = span.Slope;
    }
    else if ((op & 15u) == KindTexelWalk)
    {
        draw[12] = static_cast<int32_t>(span.Texel);
        draw[13] = span.Step0;
        draw[14] = span.StepU;
        draw[15] = span.StepV;
        draw[16] = static_cast<int32_t>(span.UFraction);
        draw[17] = static_cast<int32_t>(span.UStep);
        draw[18] = static_cast<int32_t>(span.VFraction);
        draw[19] = static_cast<int32_t>(span.VStep);
    }

    if (source.Kind == SourceKind::TargetCopy)
    {
        // A walk through the target itself may read any of it.
        Record& record = _Records.back();
        record.ReadsCopy = true;
        record.Reads = MCRect{0, 0, from[2] - 1, from[3] - 1};
    }

    return true;
}

uint32_t MCVulkanRenderer::TableRow(const uint8_t* table)
{
    BeginRecording();
    const uint64_t key = HashBytes(table, 256, 0x7ab1e);

    if (const auto found = _TableRows.find(key); found != _TableRows.end())
    {
        return found->second;
    }

    const auto row = static_cast<uint32_t>(_Tables.size() / 256);
    _Tables.insert(_Tables.end(), table, table + 256);
    _TableRows.emplace(key, row);
    return row;
}

void MCVulkanRenderer::SyncAlphaTable()
{
    const uint64_t key = HashBytes(AlphaTable, sizeof(AlphaTable), 0xa1fa);

    if (key == _AlphaKey)
    {
        return;
    }

    _AlphaKey = key;
    std::memcpy(QueueUpload(_AlphaTexture.Handle, 0, 0, 256, ALPHA_COLORS, 1), AlphaTable, sizeof(AlphaTable));
}

auto MCVulkanRenderer::AtlasFor(uint64_t key, int32_t width, int32_t height,
                                const std::function<void(uint8_t*)>& decode) -> std::optional<AtlasPlace>
{
    if (const auto found = _Atlas.find(key); found != _Atlas.end())
    {
        return found->second;
    }

    if (width <= 0 || height <= 0 || width > PageSize || height > PageSize)
    {
        return std::nullopt;
    }

    Page* page = _Pages.empty() ? nullptr : &_Pages.back();

    if (page != nullptr && page->ShelfX + width > PageSize)
    {
        // A new shelf.
        page->ShelfY += page->ShelfHeight;
        page->ShelfX = 0;
        page->ShelfHeight = 0;
    }

    if (page == nullptr || page->ShelfY + height > PageSize)
    {
        _Pages.push_back(Page{});
        page = &_Pages.back();

        if (!EnsureTexture(page->Texture, SDL_GPU_TEXTUREFORMAT_R8G8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, PageSize,
                           PageSize))
        {
            _Pages.pop_back();
            return std::nullopt;
        }
    }

    const AtlasPlace place{static_cast<uint32_t>(_Pages.size() - 1), page->ShelfX, page->ShelfY};
    page->ShelfX += width;
    page->ShelfHeight = std::max(page->ShelfHeight, height);
    decode(QueueUpload(page->Texture.Handle, static_cast<uint32_t>(place.X), static_cast<uint32_t>(place.Y),
                       static_cast<uint32_t>(width), static_cast<uint32_t>(height), 2));
    _Atlas.emplace(key, place);
    ++_FrameUploads.AtlasImages;
    _FrameUploads.AtlasBytes += static_cast<int64_t>(width) * height * 2;
    return place;
}

std::optional<uint32_t> MCVulkanRenderer::PictureFor(const _window* window)
{
    BeginRecording();

    if (window->buffer == nullptr)
    {
        return std::nullopt;
    }

    if (const auto memo = _PictureMemo.find(window); memo != _PictureMemo.end())
    {
        const PictureMemo& seen = memo->second;

        if (seen.Buffer == window->buffer && seen.XMax == window->x_max && seen.YMax == window->y_max &&
            seen.Version == window->Version)
        {
            return seen.Index;
        }
    }

    const std::optional<uint32_t> index = PictureForBytes(window->buffer, static_cast<uint32_t>(window->x_max + 1),
                                                          static_cast<uint32_t>(window->y_max + 1), window->Movie);

    if (index)
    {
        _PictureMemo[window] = PictureMemo{window->buffer, window->x_max, window->y_max, window->Version, *index};
    }

    return index;
}

std::optional<uint32_t> MCVulkanRenderer::PictureForBytes(const uint8_t* pixels, uint32_t width, uint32_t height,
                                                          bool movie)
{
    const size_t size = static_cast<size_t>(width) * height;
    const uint64_t key = HashBytes(pixels, size, static_cast<uint64_t>(width) << 32 | height);
    return PictureForKey(key, width, height, [&](uint8_t* out) { std::memcpy(out, pixels, size); }, movie);
}

std::optional<uint32_t> MCVulkanRenderer::PictureForKey(uint64_t key, uint32_t width, uint32_t height,
                                                        const std::function<void(uint8_t*)>& fill, bool movie)
{
    BeginRecording();
    const size_t size = static_cast<size_t>(width) * height;
    auto found = _Pictures.find(key);

    if (found == _Pictures.end())
    {
        Picture picture;

        if (!EnsureTexture(picture.Texture, SDL_GPU_TEXTUREFORMAT_R8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, width,
                           height))
        {
            return std::nullopt;
        }

        fill(QueueUpload(picture.Texture.Handle, 0, 0, width, height, 1));
        found = _Pictures.emplace(key, picture).first;
        _PictureBytes += size;

        if (movie)
        {
            ++_FrameUploads.MovieFrames;
        }
        else
        {
            ++_FrameUploads.Pictures;
            _FrameUploads.PictureBytes += static_cast<int64_t>(size);
        }
    }

    found->second.LastFrame = _Frame;
    const auto index = static_cast<uint32_t>(_FramePictures.size());
    _FramePictures.push_back(found->second.Texture.Handle);
    return index;
}

auto MCVulkanRenderer::SourceFor(uint16_t target, const _window* window) -> std::optional<SourceRef>
{
    if (const _window* surface = MCRenderer::FrameSurfaceOf(window); surface != nullptr)
    {
        // A surface: read from the target's copy when it is the target (the pixels as they were before the draw).
        const uint16_t index = SurfaceFor(surface);
        return SourceRef{index == target ? SourceKind::TargetCopy : SourceKind::Surface, index};
    }

    const std::optional<uint32_t> picture = PictureFor(window);

    if (!picture)
    {
        return std::nullopt;
    }

    return SourceRef{SourceKind::Picture, *picture};
}

// Commands ---------------------------------------------------------------------------------------------------------

void MCVulkanRenderer::Clear(_window* target, const MCRect& rect, uint8_t color)
{
    const uint16_t surface = SurfaceFor(target);
    const int32_t at[4] = {rect.X0, rect.Y0, rect.X1, rect.Y1};
    Add(surface, SourceKind::None, 0, true, at, nullptr, KindFill, color, 0, 0);
}

void MCVulkanRenderer::Hash(_window* target, const MCRect& rect, uint8_t color)
{
    const uint16_t surface = SurfaceFor(target);
    const int32_t at[4] = {rect.X0, rect.Y0, rect.X1, rect.Y1};

    // The pattern counts from the rectangle's left column and bottom row, kept apart from the clipped rectangle.
    if (Add(surface, SourceKind::None, 0, false, at, nullptr, KindHash, color, 0, 0))
    {
        _Draws.back()[12] = rect.X0;
        _Draws.back()[13] = rect.Y1;
    }
}

void MCVulkanRenderer::Copy(_window* target, const MCCopyCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const MCRect& rect = command.SourceRect;
    const int32_t at[4] = {command.X, command.Y, command.X + rect.X1 - rect.X0, command.Y + rect.Y1 - rect.Y0};
    const int32_t from[4] = {rect.X0, rect.Y0, 1, 1};
    const uint32_t op = KindTexture | OpaqueSource | (command.ColorKey ? ColorKey : 0);
    const uint32_t color = static_cast<uint32_t>(command.Key) << 8;

    // A copy within a surface reads the target's copy: the pixels as they were, as the software renderer's ordered
    // copy reads them.
    const std::optional<SourceRef> source = SourceFor(surface, command.Source);

    if (!source)
    {
        NotSupported("Copy from a window without pixels");
        return;
    }

    Add(surface, source->Kind, source->Index, false, at, from, op, color, 0, 0);
}

void MCVulkanRenderer::AlphaBlit(_window* target, const MCAlphaBlitCommand& command)
{
    const uint16_t surface = SurfaceFor(target);

    // The bytes the blit reads, as a picture: every pixel, or every other pixel of every other row, mirrored or not.
    const int32_t rows =
        command.FullSize ? command.Rows : static_cast<int32_t>(static_cast<uint32_t>(command.Rows) >> 1);
    const int32_t columns =
        command.FullSize ? command.Columns : static_cast<int32_t>(static_cast<uint32_t>(command.Columns) >> 1);

    if (rows <= 0 || columns <= 0)
    {
        return;
    }

    const int32_t pitch = command.Pitch;
    const int32_t rowStep = command.FullSize ? pitch : pitch * 2;
    const int32_t step = (command.FullSize ? 1 : 2) * (command.Mirror ? -1 : 1);
    const uint8_t* start = command.Sprite + command.Offset + (command.Mirror ? rowStep - 1 : 0);
    std::vector<uint8_t> image(static_cast<size_t>(rows) * columns);

    for (int32_t row = 0; row < rows; ++row)
    {
        const uint8_t* s = start + static_cast<intptr_t>(row) * rowStep;

        for (int32_t column = 0; column < columns; ++column)
        {
            image[static_cast<size_t>(row) * columns + column] = s[static_cast<intptr_t>(column) * step];
        }
    }

    const std::optional<uint32_t> picture =
        PictureForBytes(image.data(), static_cast<uint32_t>(columns), static_cast<uint32_t>(rows));

    if (!picture)
    {
        return;
    }

    SyncAlphaTable();
    const int32_t at[4] = {command.Left, command.Top, command.Left + columns - 1, command.Top + rows - 1};
    const int32_t from[4] = {0, 0, 1, 1};
    Add(surface, SourceKind::Picture, *picture, false, at, from, KindTexture | OpaqueSource | ReadsDest | AlphaBlend, 0,
        0, 0);
}

void MCVulkanRenderer::ShapeBlit(_window* target, const MCShapeBlitCommand& command)
{
    const uint8_t* data = MCVfxShape(const_cast<void*>(command.ShapeTable), command.ShapeNum) + 0x18;
    const int32_t width = command.Width;
    const int32_t height = command.Height;
    int32_t widest = 0;
    const uint8_t* end = ShapeExtent(data, height, widest);
    const auto fill = [&](uint8_t* out) { FillShape(data, height, width, command.Table, out); };

    // Where the blit's first pixel lies in the shape's picture, and its steps: the bytes AlphaBlit reads, as
    // coordinates (each run stays within a row of the picture).
    const MCAlphaBlitCommand& blit = command.Blit;
    const int32_t rows = blit.FullSize ? blit.Rows : static_cast<int32_t>(static_cast<uint32_t>(blit.Rows) >> 1);
    const int32_t columns =
        blit.FullSize ? blit.Columns : static_cast<int32_t>(static_cast<uint32_t>(blit.Columns) >> 1);
    const int32_t stepX = (blit.FullSize ? 1 : 2) * (blit.Mirror ? -1 : 1);
    const int32_t stepY = blit.FullSize ? 1 : 2;
    const intptr_t first = blit.Offset + (blit.Mirror ? static_cast<intptr_t>(stepY) * width - 1 : 0);
    const auto x0 = static_cast<int32_t>(first >= 0 ? first % width : width - 1 - (-first - 1) % width);
    const auto y0 = static_cast<int32_t>((first - x0) / width);
    const int32_t lastX = x0 + (columns - 1) * stepX;
    const int32_t lastY = y0 + (rows - 1) * stepY;

    if (rows <= 0 || columns <= 0)
    {
        return;
    }

    // CopySprite's clipping keeps every pixel read inside the picture (the mirrored half size reads the odd rows).
    if (std::min(x0, lastX) < 0 || std::max(x0, lastX) >= width || y0 < 0 || lastY >= height)
    {
        NotSupported("ShapeBlit reading outside the shape's picture");
        return;
    }

    uint64_t key = HashBytes(data, static_cast<size_t>(end - data), Seed(ImageKind::ShapeFill, width, height));

    if (command.Table != nullptr)
    {
        key = HashBytes(command.Table, 256, key);
    }

    const std::optional<uint32_t> picture =
        PictureForKey(key, static_cast<uint32_t>(width), static_cast<uint32_t>(height), fill);

    if (!picture)
    {
        return;
    }

    const uint16_t surface = SurfaceFor(target);
    SyncAlphaTable();
    const int32_t at[4] = {blit.Left, blit.Top, blit.Left + columns - 1, blit.Top + rows - 1};
    const int32_t from[4] = {x0, y0, stepX, stepY};
    Add(surface, SourceKind::Picture, *picture, false, at, from, KindTexture | OpaqueSource | ReadsDest | AlphaBlend, 0,
        0, 0);
}

void MCVulkanRenderer::Write(_window* target, int32_t x, int32_t y, const uint8_t* pixels, int32_t count)
{
    const uint16_t surface = SurfaceFor(target);

    // Each piece of the row goes into a row of the frame's strip, drawn from there.
    for (int32_t done = 0; done < count; done += StripWidth)
    {
        const int32_t length = std::min(count - done, StripWidth);
        const uint32_t row = _StripRows++;
        _StripPixels.resize(static_cast<size_t>(_StripRows) * StripWidth);
        std::memcpy(&_StripPixels[static_cast<size_t>(row) * StripWidth], pixels + done, static_cast<size_t>(length));
        const int32_t at[4] = {x + done, y, x + done + length - 1, y};
        const int32_t from[4] = {0, static_cast<int32_t>(row), 1, 1};
        Add(surface, SourceKind::Strip, 0, false, at, from, KindTexture | OpaqueSource, 0, 0, 0);
    }
}

void MCVulkanRenderer::Pixel(_window* target, int32_t x, int32_t y, uint8_t color)
{
    const uint16_t surface = SurfaceFor(target);
    const int32_t at[4] = {x, y, x, y};
    Add(surface, SourceKind::None, 0, false, at, nullptr, KindFill, color, 0, 0);
}

void MCVulkanRenderer::Shape(_window* target, const MCShapeCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const uint8_t* header = MCVfxShape(const_cast<void*>(command.ShapeTable), command.ShapeNum);
    const uint8_t* data = header + 0x18;

    // The whole shape when its header covers the rows drawn (so a shape scrolling past an edge keeps one atlas place).
    const int32_t headerRows = MCVfxRead32(header + 0x14) - MCVfxRead32(header + 0x0c) + 1;
    const int32_t rows = std::max(command.SkipRows + command.Rows, headerRows);
    int32_t width = 0;
    const uint8_t* end = ShapeExtent(data, rows, width);

    if (width == 0)
    {
        return;
    }

    const uint64_t key = HashBytes(data, static_cast<size_t>(end - data), Seed(ImageKind::Shape, width, rows));
    const auto place = AtlasFor(key, width, rows, [&](uint8_t* out) { DecodeShape(data, rows, width, out); });

    if (!place)
    {
        NotSupported("Shape larger than an atlas page");
        return;
    }

    const int32_t at[4] = {std::max(command.Left, command.Lo), command.Top,
                           std::min(command.Left + width - 1, command.Hi), command.Top + command.Rows - 1};
    const int32_t from[4] = {place->X + at[0] - command.Left, place->Y + command.SkipRows, 1, 1};

    if (at[2] < at[0])
    {
        return;
    }

    uint32_t op = KindTexture;
    uint32_t before = 0;
    uint32_t after = 0;

    switch (command.Op)
    {
        case MCShapeOp::Draw:
            break;
        case MCShapeOp::Xlat:
        {
            op |= TableBefore;
            before = TableRow(command.Table);
            break;
        }
        case MCShapeOp::Alpha:
        {
            op |= ReadsDest | AlphaBlend;
            SyncAlphaTable();
            break;
        }
        case MCShapeOp::XlatAlpha:
        {
            op |= ReadsDest | AlphaBlend | TableAfter;
            after = TableRow(command.Table);
            SyncAlphaTable();
            break;
        }
        case MCShapeOp::Fill:
        {
            op |= FillSkipped;
            break;
        }
        case MCShapeOp::XlatFill:
        {
            op |= FillSkipped | TableBefore;
            before = TableRow(command.Table);
            break;
        }
    }

    Add(surface, SourceKind::Atlas, place->Page, false, at, from, op, 0, before, after);
}

void MCVulkanRenderer::FastShape(_window* target, const MCFastShapeCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const uint8_t* shape = command.Shape;
    const int32_t height = Read16(shape + 8);
    const int32_t width = Read16(shape + 10) + 1;

    // The rows' packets, as one key: the offsets and every row's bytes.
    uint64_t key = HashBytes(shape + 0xc, static_cast<size_t>(height) * 2, Seed(ImageKind::FastShape, width, height));

    for (int32_t row = 0; row < height; ++row)
    {
        const uint8_t* data = shape + Read16(shape + 0xc + static_cast<intptr_t>(row) * 2);
        key = HashBytes(data, static_cast<size_t>(FastRowEnd(data, width) - data), key);
    }

    const auto place = AtlasFor(key, width, height,
                                [&](uint8_t* out)
                                {
                                    std::memset(out, 0, static_cast<size_t>(width) * height * 2);

                                    for (int32_t row = 0; row < height; ++row)
                                    {
                                        DecodeFastRow(shape + Read16(shape + 0xc + static_cast<intptr_t>(row) * 2),
                                                      width, out + static_cast<size_t>(row) * width * 2);
                                    }
                                });

    if (!place)
    {
        NotSupported("FastShape larger than an atlas page");
        return;
    }

    // Shape columns LeftSkip.. are drawn from ClipX0 when the left is clipped, else from column 0 at StartX; Limit
    // of them a row.
    const int32_t left = command.LeftSkip > 0 ? command.ClipX0 : command.StartX;
    const int32_t column = command.LeftSkip > 0 ? command.LeftSkip : 0;
    const int32_t at[4] = {left, command.Top, left + command.Limit - 1,
                           command.Top + command.EndRow - command.FirstRow - 1};
    const int32_t from[4] = {place->X + column, place->Y + command.FirstRow, 1, 1};
    uint32_t op = KindTexture;
    uint32_t before = 0;

    if (command.Table != nullptr)
    {
        op |= TableBefore | SkipKeyAfterTable;
        before = TableRow(command.Table);
    }
    else
    {
        op |= SkipRunKey;
    }

    if (command.Alpha)
    {
        op |= ReadsDest | AlphaBlend;
        SyncAlphaTable();
    }

    Add(surface, SourceKind::Atlas, place->Page, false, at, from, op, 0, before, 0);
}

void MCVulkanRenderer::Tile(_window* target, const MCTileCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const uint8_t* tile = command.Tile;
    const int32_t height = tile[2];
    int32_t width = 0;

    for (int32_t row = 0; row < height; ++row)
    {
        const uint32_t offset = TileOffset(tile, row);
        const int32_t length = static_cast<int32_t>(TileOffset(tile, row + 1) - offset);

        if (length > 1)
        {
            width = std::max(width, tile[offset] + length - 1);
        }
    }

    if (width == 0)
    {
        return;
    }

    const uint64_t key = HashBytes(tile, TileOffset(tile, height), Seed(ImageKind::Tile, width, height));
    const auto place = AtlasFor(key, width, height,
                                [&](uint8_t* out)
                                {
                                    std::memset(out, 0, static_cast<size_t>(width) * height * 2);

                                    for (int32_t row = 0; row < height; ++row)
                                    {
                                        const uint32_t offset = TileOffset(tile, row);
                                        const int32_t length = static_cast<int32_t>(TileOffset(tile, row + 1) - offset);
                                        uint8_t* line = out + static_cast<size_t>(row) * width * 2;

                                        for (int32_t i = 0; i < length - 1; ++i)
                                        {
                                            line[(tile[offset] + i) * 2] = tile[offset + 1 + i];
                                            line[(tile[offset] + i) * 2 + 1] = 1;
                                        }
                                    }
                                });

    if (!place)
    {
        NotSupported("Tile larger than an atlas page");
        return;
    }

    const int32_t lo = command.Unclipped ? command.Left : std::max(command.Left, command.Lo);
    const int32_t hi = command.Unclipped ? command.Left + width - 1 : std::min(command.Left + width - 1, command.Hi);
    const int32_t at[4] = {lo, command.Top, hi, command.Top + command.EndRow - command.FirstRow - 1};
    const int32_t from[4] = {place->X + lo - command.Left, place->Y + command.FirstRow, 1, 1};

    if (hi < lo)
    {
        return;
    }

    uint32_t op = KindTexture;
    uint32_t color = 0;
    uint32_t before = 0;

    if (command.Table == VFX_TILE_FILL)
    {
        op |= UseColor;
        color = 0x10;
    }
    else if (command.Table != nullptr)
    {
        op |= TableBefore;
        before = TableRow(command.Table);
    }

    Add(surface, SourceKind::Atlas, place->Page, false, at, from, op, color, before, 0);
}

void MCVulkanRenderer::Polygon(_window* target, const MCPolygonCommand& command)
{
    if (command.Kind == MCPolygonKind::DitheredGouraud || command.Kind == MCPolygonKind::Illuminate)
    {
        NotSupported("Polygon (dithered)");
        return;
    }

    if (command.VertexCount <= 0)
    {
        return;
    }

    const uint16_t surface = SurfaceFor(target);
    SourceRef source;
    int32_t textureSize[2] = {0, 0};
    uint32_t op = KindFill;
    uint32_t color = 0;
    uint32_t before = 0;
    uint32_t after = 0;

    switch (command.Kind)
    {
        case MCPolygonKind::Flat:
        {
            color = ((static_cast<uint32_t>(command.Vertices[0].c) + 0x8000) >> 16) & 0xff;
            break;
        }
        case MCPolygonKind::Gouraud:
        {
            op = KindGouraud;
            break;
        }
        case MCPolygonKind::Translate:
        {
            op = KindFill | ReadsDest | TableAfter;
            after = TableRow(command.Table);
            break;
        }
        case MCPolygonKind::Map:
        {
            const std::optional<SourceRef> found = SourceFor(surface, command.Texture);

            if (!found)
            {
                NotSupported("Polygon mapped from a window without pixels");
                return;
            }

            source = *found;
            textureSize[0] = command.Texture->x_max + 1;
            textureSize[1] = command.Texture->y_max + 1;
            op = KindTexelWalk;

            if ((command.MapFlags & MP_XLAT) != 0)
            {
                op |= TableBefore;
                before = TableRow(command.Table);
            }

            if ((command.MapFlags & MP_XP) != 0)
            {
                op |= SkipKeyAfterTable;
            }
            break;
        }
        default:
            break;
    }

    // The spans never share a pixel: they all read the target's copy taken before the polygon.
    bool join = false;
    MCPolygonSpans(command, _Spans,
                   [&](const MCSpan& span)
                   {
                       const int32_t at[4] = {span.X0, span.Y, span.X1, span.Y};
                       const int32_t from[4] = {0, 0, textureSize[0], textureSize[1]};

                       if (AddSpan(surface, source, at, from, op, color, before, after, span, join))
                       {
                           join = true;
                       }
                   });
}

void MCVulkanRenderer::MapQuad(_window* target, const MCMapQuadCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const std::optional<SourceRef> source = SourceFor(surface, command.Texture);

    if (!source)
    {
        NotSupported("MapQuad from a window without pixels");
        return;
    }

    const int32_t from[4] = {0, 0, command.Texture->x_max + 1, command.Texture->y_max + 1};
    bool join = false;
    MCMapQuadSpans(command, _Spans,
                   [&](const MCSpan& span)
                   {
                       const int32_t at[4] = {span.X0, span.Y, span.X1, span.Y};

                       if (AddSpan(surface, *source, at, from, KindTexelWalk | SkipKeyAfterTable, 0, 0, 0, span, join))
                       {
                           join = true;
                       }
                   });
}

void MCVulkanRenderer::Line(_window* target, const MCLineCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    uint32_t op = KindFill;
    uint32_t after = 0;

    if (command.Table != nullptr)
    {
        op |= ReadsDest | TableAfter;
        after = TableRow(command.Table);
    }

    // The pixels in runs along a row (a line never comes back to a pixel).
    bool join = false;
    bool open = false;
    int32_t runY = 0;
    int32_t runX0 = 0;
    int32_t runX1 = 0;
    const auto flush = [&]
    {
        if (open)
        {
            const int32_t at[4] = {runX0, runY, runX1, runY};

            if (Add(surface, SourceKind::None, 0, false, at, nullptr, op, command.Color, 0, after, join))
            {
                join = true;
            }
        }
    };

    MCLinePixels(command,
                 [&](int32_t x, int32_t y)
                 {
                     if (open && y == runY && (x == runX1 + 1 || x == runX0 - 1))
                     {
                         runX0 = std::min(runX0, x);
                         runX1 = std::max(runX1, x);
                         return;
                     }

                     flush();
                     open = true;
                     runY = y;
                     runX0 = x;
                     runX1 = x;
                 });
    flush();
}

void MCVulkanRenderer::Ellipse(_window* target, const MCEllipseCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    uint32_t op = KindFill;

    if (command.Alpha)
    {
        op |= ReadsDest | AlphaBlend;
        SyncAlphaTable();
    }

    // Each pixel once (MCEllipseRuns), so the runs can share the target's copy.
    bool join = false;
    MCEllipseRuns(command,
                  [&](int32_t y, int32_t x0, int32_t x1)
                  {
                      const int32_t at[4] = {x0, y, x1, y};

                      if (Add(surface, SourceKind::None, 0, false, at, nullptr, op, command.Color, 0, 0, join))
                      {
                          join = true;
                      }
                  });
}

void MCVulkanRenderer::StatusBar(_window* target, const MCStatusBarCommand& command)
{
    // The AlphaTable row status-bar frames are darkened through (0x008011d0 in MCX.EXE).
    constexpr int32_t StatusFrameAlpha = 0x108;
    const uint16_t surface = SurfaceFor(target);
    const auto* alpha = reinterpret_cast<const uint8_t*>(AlphaTable);
    const uint32_t frame = TableRow(alpha + StatusFrameAlpha * 256);
    const uint32_t fill = TableRow(alpha + static_cast<intptr_t>(command.AlphaColor) * 256);
    const MCRect& box = command.Box;
    const int32_t width = box.X1 - box.X0;
    const uint32_t op = KindFill | ReadsDest | TableAfter;

    // The frame's top and bottom rows (the pixels between the corners), then the body rows: their left and right
    // pixels through the frame's table, then the bar through the fill's. A pixel the bar shares with the right
    // edge is mapped by both, in that order (each part reads the pixels the one before left).
    bool join = false;
    const auto add = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t table, bool joins)
    {
        const int32_t at[4] = {x0, y0, x1, y1};

        if (Add(surface, SourceKind::None, 0, false, at, nullptr, op, 0, 0, table, joins && join))
        {
            join = true;
        }
    };

    for (const int32_t row : {command.FrameTop, command.FrameBottom})
    {
        // A box of one row has the same frame row twice; it is mapped once.
        if (row >= box.Y0 && row <= box.Y1)
        {
            add(box.X0 + 1, row, box.X0 + width - 1, row, frame, true);
        }

        if (command.FrameTop == command.FrameBottom)
        {
            break;
        }
    }

    // The body rows: the box's rows less the frame rows, in up to three runs.
    std::vector<std::pair<int32_t, int32_t>> body;
    int32_t first = box.Y0;

    for (int32_t y = box.Y0; y <= box.Y1 + 1; ++y)
    {
        if (y > box.Y1 || y == command.FrameTop || y == command.FrameBottom)
        {
            if (y > first)
            {
                body.emplace_back(first, y - 1);
            }

            first = y + 1;
        }
    }

    for (const auto& [y0, y1] : body)
    {
        add(box.X0, y0, box.X0, y1, frame, true);
        // A box one pixel wide maps its one pixel twice.
        add(box.X0 + width, y0, box.X0 + width, y1, frame, width != 0);
    }

    if (command.BarLength != 0)
    {
        for (const auto& [y0, y1] : body)
        {
            add(box.X0 + 1, y0, box.X0 + command.BarLength + 1, y1, fill, false);
        }
    }
}

void MCVulkanRenderer::Glyph(_window* target, const MCGlyphCommand& command)
{
    const uint16_t surface = SurfaceFor(target);
    const uint8_t* font = static_cast<const uint8_t*>(command.Font);
    const uint8_t* glyph = font + MCVfxRead32(font + 0x10 + static_cast<intptr_t>(command.Character) * 4);
    const int32_t pitch = MCVfxRead32(glyph);
    const uint8_t* source = glyph + 4 + static_cast<intptr_t>(command.SourceY) * pitch + command.SourceX;
    const int32_t columns = command.Columns;
    const int32_t rows = command.Rows;

    if (columns <= 0 || rows <= 0)
    {
        return;
    }

    uint64_t key = Seed(ImageKind::Glyph, columns, rows);

    for (int32_t row = 0; row < rows; ++row)
    {
        key = HashBytes(source + static_cast<intptr_t>(row) * pitch, static_cast<size_t>(columns), key);
    }

    const auto place = AtlasFor(key, columns, rows,
                                [&](uint8_t* out)
                                {
                                    for (int32_t row = 0; row < rows; ++row)
                                    {
                                        for (int32_t i = 0; i < columns; ++i)
                                        {
                                            out[(row * columns + i) * 2] = source[row * pitch + i];
                                            out[(row * columns + i) * 2 + 1] = 1;
                                        }
                                    }
                                });

    if (!place)
    {
        NotSupported("Glyph larger than an atlas page");
        return;
    }

    const int32_t at[4] = {command.X, command.Y, command.X + columns - 1, command.Y + rows - 1};
    const int32_t from[4] = {place->X, place->Y, 1, 1};
    uint32_t op = KindTexture;
    uint32_t before = 0;

    if (command.Table != nullptr)
    {
        op |= TableBefore | SkipKeyAfterTable;
        before = TableRow(command.Table);
    }

    Add(surface, SourceKind::Atlas, place->Page, false, at, from, op, 0, before, 0);
}

// The frame ----------------------------------------------------------------------------------------------------------

void MCVulkanRenderer::TakeCopy(SDL_GPUCommandBuffer* commands, Surface& surface, const MCRect& rect)
{
    const int32_t x0 = std::max(rect.X0, 0);
    const int32_t y0 = std::max(rect.Y0, 0);
    const int32_t x1 = std::min(rect.X1, static_cast<int32_t>(surface.Target.Width) - 1);
    const int32_t y1 = std::min(rect.Y1, static_cast<int32_t>(surface.Target.Height) - 1);
    surface.Copied = rect;
    surface.DrawnSinceCopy.clear();

    if (x1 < x0 || y1 < y0)
    {
        return;
    }

    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureLocation from{};
    from.texture = surface.Target.Handle;
    from.x = static_cast<uint32_t>(x0);
    from.y = static_cast<uint32_t>(y0);
    SDL_GPUTextureLocation to = from;
    to.texture = surface.Copy.Handle;
    SDL_CopyGPUTextureToTexture(pass, &from, &to, static_cast<uint32_t>(x1 - x0 + 1),
                                static_cast<uint32_t>(y1 - y0 + 1), 1, false);
    SDL_EndGPUCopyPass(pass);
}

std::expected<void, std::string> MCVulkanRenderer::Execute(SDL_GPUCommandBuffer* commands,
                                                           std::span<const MCUnderlay> underlays)
{
    if (!_Recording && _Uploads.empty())
    {
        return {};
    }

    BeginRecording();

    // The frame's tables.
    const auto tableRows = static_cast<uint32_t>(_Tables.size() / 256);

    if (_TableTexture.Handle == nullptr || _TableTexture.Height < tableRows)
    {
        if (auto made = EnsureTexture(_TableTexture, SDL_GPU_TEXTUREFORMAT_R8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER, 256,
                                      std::bit_ceil(std::max(tableRows, 64u)));
            !made)
        {
            return made;
        }
    }

    std::memcpy(QueueUpload(_TableTexture.Handle, 0, 0, 256, tableRows, 1), _Tables.data(), _Tables.size());

    // The frame's written pixels.
    if (_StripRows > 0)
    {
        if (_Strip.Handle == nullptr || _Strip.Height < _StripRows)
        {
            if (auto made = EnsureTexture(_Strip, SDL_GPU_TEXTUREFORMAT_R8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                          StripWidth, std::bit_ceil(std::max(_StripRows, 64u)));
                !made)
            {
                return made;
            }
        }

        std::memcpy(QueueUpload(_Strip.Handle, 0, 0, StripWidth, _StripRows, 1), _StripPixels.data(),
                    _StripPixels.size());
    }

    // Everything up in one transfer: the uploads, then the draws.
    const size_t drawOffset = (_Staging.size() + UploadAlignment - 1) & ~(UploadAlignment - 1);
    const size_t drawBytes = _Draws.size() * sizeof(_Draws[0]);
    const size_t total = drawOffset + drawBytes;

    if (_Transfer == nullptr || _TransferSize < total)
    {
        if (_Transfer != nullptr)
        {
            SDL_ReleaseGPUTransferBuffer(_Device, _Transfer);
        }

        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = static_cast<uint32_t>(total + total / 4);
        _Transfer = SDL_CreateGPUTransferBuffer(_Device, &info);
        _TransferSize = _Transfer != nullptr ? info.size : 0;

        if (_Transfer == nullptr)
        {
            return std::unexpected(SdlError("SDL_CreateGPUTransferBuffer"));
        }
    }

    if (drawBytes > 0 && (_DrawBuffer == nullptr || _DrawBufferSize < drawBytes))
    {
        if (_DrawBuffer != nullptr)
        {
            SDL_ReleaseGPUBuffer(_Device, _DrawBuffer);
        }

        SDL_GPUBufferCreateInfo info{};
        info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        info.size = static_cast<uint32_t>(drawBytes + drawBytes / 2);
        _DrawBuffer = SDL_CreateGPUBuffer(_Device, &info);
        _DrawBufferSize = _DrawBuffer != nullptr ? info.size : 0;

        if (_DrawBuffer == nullptr)
        {
            return std::unexpected(SdlError("SDL_CreateGPUBuffer"));
        }
    }

    auto* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(_Device, _Transfer, true));

    if (mapped == nullptr)
    {
        return std::unexpected(SdlError("SDL_MapGPUTransferBuffer"));
    }

    std::memcpy(mapped, _Staging.data(), _Staging.size());

    if (drawBytes > 0)
    {
        std::memcpy(mapped + drawOffset, _Draws.data(), drawBytes);
    }

    SDL_UnmapGPUTransferBuffer(_Device, _Transfer);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commands);

    for (const Upload& upload : _Uploads)
    {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = _Transfer;
        source.offset = static_cast<uint32_t>(upload.Offset);
        source.pixels_per_row = upload.Width;
        source.rows_per_layer = upload.Height;
        SDL_GPUTextureRegion destination{};
        destination.texture = upload.Texture;
        destination.x = upload.X;
        destination.y = upload.Y;
        destination.w = upload.Width;
        destination.h = upload.Height;
        destination.d = 1;
        SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
    }

    if (drawBytes > 0)
    {
        SDL_GPUTransferBufferLocation source{_Transfer, static_cast<uint32_t>(drawOffset)};
        SDL_GPUBufferRegion destination{_DrawBuffer, 0, static_cast<uint32_t>(drawBytes)};
        SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
    }

    SDL_EndGPUCopyPass(copyPass);

    // The surfaces shown as underlays first: the screen's see-through draws read them finished.
    const auto isUnderlay = [&](uint16_t index)
    {
        const _window* window = _Surfaces[index].Window;
        return std::ranges::any_of(underlays, [window](const MCUnderlay& underlay)
                                   { return MCRenderer::FrameSurfaceOf(underlay.Source) == window; });
    };

    std::vector<uint32_t> order(_Records.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::stable_partition(order, [&](uint32_t i) { return isUnderlay(_Records[i].SurfaceIndex); });

    SDL_GPURenderPass* pass = nullptr;
    int32_t passSurface = -1;
    const auto endPass = [&]
    {
        if (pass != nullptr)
        {
            SDL_EndGPURenderPass(pass);
            pass = nullptr;
            passSurface = -1;
        }
    };

    for (size_t k = 0; k < order.size(); ++k)
    {
        const Record& record = _Records[order[k]];
        Surface& surface = _Surfaces[record.SurfaceIndex];

        if (record.Resize)
        {
            endPass();

            for (auto made :
                 {EnsureTexture(surface.Target, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER, surface.RecordedWidth,
                                surface.RecordedHeight),
                  EnsureTexture(surface.Copy, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                surface.RecordedWidth, surface.RecordedHeight)})
            {
                if (!made)
                {
                    return made;
                }
            }

            // A new surface starts as zeros, as the window's new pixels do.
            SDL_GPUColorTargetInfo clear{};
            clear.texture = surface.Target.Handle;
            clear.load_op = SDL_GPU_LOADOP_CLEAR;
            clear.store_op = SDL_GPU_STOREOP_STORE;
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(commands, &clear, 1, nullptr));
            surface.Copied = MCRect{0, 0, -1, -1};
            surface.DrawnSinceCopy.clear();
            continue;
        }

        if (surface.Target.Handle == nullptr)
        {
            continue;
        }

        if (record.ReadsCopy)
        {
            const bool covered = Contains(surface.Copied, record.Reads) &&
                                 std::ranges::none_of(surface.DrawnSinceCopy, [&](const MCRect& drawn)
                                                      { return Overlaps(drawn, record.Reads); });

            if (!covered)
            {
                // One copy for this and the draws reading the target that follow it, as long as none reads what
                // another of them draws.
                MCRect bounds = record.Reads;
                std::vector<MCRect> groupWrites{record.Writes};

                for (size_t j = k + 1; j < order.size(); ++j)
                {
                    const Record& next = _Records[order[j]];

                    if (next.Resize || next.SurfaceIndex != record.SurfaceIndex || !next.ReadsCopy ||
                        std::ranges::any_of(groupWrites,
                                            [&](const MCRect& drawn) { return Overlaps(drawn, next.Reads); }))
                    {
                        break;
                    }

                    bounds = Union(bounds, next.Reads);
                    groupWrites.push_back(next.Writes);
                }

                endPass();
                TakeCopy(commands, surface, bounds);
            }
        }

        if (passSurface != record.SurfaceIndex)
        {
            endPass();
            SDL_GPUColorTargetInfo target{};
            target.texture = surface.Target.Handle;
            target.load_op = SDL_GPU_LOADOP_LOAD;
            target.store_op = SDL_GPU_STOREOP_STORE;
            pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
            passSurface = record.SurfaceIndex;
            const SDL_GPUViewport viewport{
                0.0f, 0.0f, static_cast<float>(surface.RecordedWidth), static_cast<float>(surface.RecordedHeight),
                0.0f, 1.0f};
            SDL_SetGPUViewport(pass, &viewport);
            SDL_BindGPUVertexStorageBuffers(pass, 0, &_DrawBuffer, 1);

            // The underlay shown under this surface, for see-through draws.
            FrameUniforms frame{};
            SDL_GPUTexture* world = _Blank.Handle;

            for (const MCUnderlay& underlay : underlays)
            {
                if (surface.Window == nullptr || MCRenderer::FrameSurfaceOf(underlay.Target) != surface.Window)
                {
                    continue;
                }

                for (const Surface& candidate : _Surfaces)
                {
                    if (candidate.Window != nullptr && &candidate != &surface &&
                        candidate.Window == MCRenderer::FrameSurfaceOf(underlay.Source) &&
                        candidate.Target.Handle != nullptr)
                    {
                        world = candidate.Target.Handle;
                        frame.Underlay[0] = underlay.Rect.X0;
                        frame.Underlay[1] = underlay.Rect.Y0;
                        frame.Underlay[2] = underlay.Rect.X1 - underlay.Rect.X0 + 1;
                        frame.Underlay[3] = underlay.Rect.Y1 - underlay.Rect.Y0 + 1;
                        frame.World[0] = static_cast<int32_t>(candidate.Target.Width);
                        frame.World[1] = static_cast<int32_t>(candidate.Target.Height);
                        break;
                    }
                }

                break;
            }

            SDL_PushGPUFragmentUniformData(commands, 0, &frame, sizeof(frame));
            _PassWorld = world;
        }

        // Records that follow on with the same bindings, and need no new copy, go in the same draw call.
        uint32_t count = record.Count;
        size_t last = k;

        while (last + 1 < order.size())
        {
            const Record& next = _Records[order[last + 1]];
            const Record& previous = _Records[order[last]];

            if (next.Resize || next.SurfaceIndex != record.SurfaceIndex || next.Source != record.Source ||
                next.SourceIndex != record.SourceIndex || next.WriteAll != record.WriteAll || next.ReadsCopy ||
                record.ReadsCopy || next.First != previous.First + previous.Count)
            {
                break;
            }

            count += next.Count;
            ++last;
        }

        SDL_GPUTexture* source = _Blank.Handle;

        switch (record.Source)
        {
            case SourceKind::None:
                break;
            case SourceKind::Atlas:
            {
                source = _Pages[record.SourceIndex].Texture.Handle;
                break;
            }
            case SourceKind::Picture:
            {
                source = _FramePictures[record.SourceIndex];
                break;
            }
            case SourceKind::Surface:
            {
                if (_Surfaces[record.SourceIndex].Target.Handle != nullptr)
                {
                    source = _Surfaces[record.SourceIndex].Target.Handle;
                }
                break;
            }
            case SourceKind::TargetCopy:
            {
                source = surface.Copy.Handle;
                break;
            }
            case SourceKind::Strip:
            {
                source = _Strip.Handle;
                break;
            }
        }

        const SDL_GPUTextureSamplerBinding bindings[5] = {
            {source, _Nearest},
            {surface.Copy.Handle, _Nearest},
            {_TableTexture.Handle, _Nearest},
            {_AlphaTexture.Handle, _Nearest},
            {_PassWorld, _Nearest},
        };

        SDL_BindGPUGraphicsPipeline(pass, record.WriteAll ? _WriteAll : _WriteIndex);
        SDL_BindGPUFragmentSamplers(pass, 0, bindings, 5);
        const BatchUniforms batch{
            {static_cast<float>(surface.RecordedWidth), static_cast<float>(surface.RecordedHeight), 0.0f, 0.0f},
            {record.First, 0, 0, 0}};
        SDL_PushGPUVertexUniformData(commands, 0, &batch, sizeof(batch));
        SDL_DrawGPUPrimitives(pass, 6, count, 0, 0);

        for (size_t i = k; i <= last; ++i)
        {
            surface.DrawnSinceCopy.push_back(_Records[order[i]].Writes);
        }

        if (surface.DrawnSinceCopy.size() > 64)
        {
            MCRect all{0, 0, -1, -1};

            for (const MCRect& drawn : surface.DrawnSinceCopy)
            {
                all = Union(all, drawn);
            }

            surface.DrawnSinceCopy.assign(1, all);
        }

        k = last;
    }

    endPass();

    for (Surface& surface : _Surfaces)
    {
        if (surface.Window == nullptr)
        {
            Release(surface.Target);
            Release(surface.Copy);
        }
    }

    _Records.clear();
    _Draws.clear();
    _Uploads.clear();
    _Staging.clear();
    _FramePictures.clear();
    _StripPixels.clear();
    _StripRows = 0;
    _PictureMemo.clear();
    _LastFrameUploads = _FrameUploads;
    _FrameUploads = UploadTally{};
    _Recording = false;
    ++_Frame;
    return {};
}

void MCVulkanRenderer::OnFrameSurfaceRemoved(const _window* window)
{
    for (Surface& surface : _Surfaces)
    {
        if (surface.Window == window)
        {
            surface.Window = nullptr;
            surface.RecordedWidth = 0;
            surface.RecordedHeight = 0;
        }
    }
}

SDL_GPUTexture* MCVulkanRenderer::SurfaceTexture(const _window* window, uint32_t& width, uint32_t& height) const
{
    const _window* canonical = MCRenderer::FrameSurfaceOf(window);

    for (const Surface& surface : _Surfaces)
    {
        if (surface.Window == canonical && surface.Target.Handle != nullptr)
        {
            width = surface.Target.Width;
            height = surface.Target.Height;
            return surface.Target.Handle;
        }
    }

    width = 0;
    height = 0;
    return nullptr;
}

std::expected<void, std::string> MCVulkanRenderer::Flush(std::span<const MCUnderlay> underlays)
{
    if (!_Recording && _Uploads.empty())
    {
        return {};
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_Device);

    if (commands == nullptr)
    {
        return std::unexpected(SdlError("SDL_AcquireGPUCommandBuffer"));
    }

    if (auto executed = Execute(commands, underlays); !executed)
    {
        SDL_CancelGPUCommandBuffer(commands);
        return executed;
    }

    if (!SDL_SubmitGPUCommandBuffer(commands))
    {
        return std::unexpected(SdlError("SDL_SubmitGPUCommandBuffer"));
    }

    return {};
}

auto MCVulkanRenderer::Download(std::span<const Surface* const> surfaces)
    -> std::expected<std::vector<std::vector<uint8_t>>, std::string>
{
    std::vector<std::vector<uint8_t>> pixels(surfaces.size());

    if (surfaces.empty())
    {
        return pixels;
    }

    std::vector<uint32_t> offsets;
    uint32_t total = 0;

    for (const Surface* surface : surfaces)
    {
        offsets.push_back(total);
        total += surface->Target.Width * surface->Target.Height * 4;
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_Device);

    if (commands == nullptr)
    {
        return std::unexpected(SdlError("SDL_AcquireGPUCommandBuffer"));
    }

    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = total;
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(_Device, &info);

    if (buffer == nullptr)
    {
        SDL_CancelGPUCommandBuffer(commands);
        return std::unexpected(SdlError("SDL_CreateGPUTransferBuffer(download)"));
    }

    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(commands);

    for (size_t i = 0; i < surfaces.size(); ++i)
    {
        SDL_GPUTextureRegion source{};
        source.texture = surfaces[i]->Target.Handle;
        source.w = surfaces[i]->Target.Width;
        source.h = surfaces[i]->Target.Height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = buffer;
        destination.offset = offsets[i];
        destination.pixels_per_row = surfaces[i]->Target.Width;
        destination.rows_per_layer = surfaces[i]->Target.Height;
        SDL_DownloadFromGPUTexture(pass, &source, &destination);
    }

    SDL_EndGPUCopyPass(pass);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);

    if (fence == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, buffer);
        return std::unexpected(SdlError("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }

    SDL_WaitForGPUFences(_Device, true, &fence, 1);
    SDL_ReleaseGPUFence(_Device, fence);
    const auto* mapped = static_cast<const uint8_t*>(SDL_MapGPUTransferBuffer(_Device, buffer, false));

    if (mapped == nullptr)
    {
        SDL_ReleaseGPUTransferBuffer(_Device, buffer);
        return std::unexpected(SdlError("SDL_MapGPUTransferBuffer(download)"));
    }

    for (size_t i = 0; i < surfaces.size(); ++i)
    {
        const size_t size = static_cast<size_t>(surfaces[i]->Target.Width) * surfaces[i]->Target.Height * 4;
        pixels[i].assign(mapped + offsets[i], mapped + offsets[i] + size);
    }

    SDL_UnmapGPUTransferBuffer(_Device, buffer);
    SDL_ReleaseGPUTransferBuffer(_Device, buffer);
    return pixels;
}

auto MCVulkanRenderer::ReadShown(const _window* screen, std::span<const MCUnderlay> underlays)
    -> std::expected<std::vector<uint8_t>, std::string>
{
    if (auto flushed = Flush(underlays); !flushed)
    {
        return std::unexpected(flushed.error());
    }

    const auto surfaceOf = [this](const _window* window) -> const Surface*
    {
        const _window* canonical = MCRenderer::FrameSurfaceOf(window);

        for (const Surface& surface : _Surfaces)
        {
            if (canonical != nullptr && surface.Window == canonical && surface.Target.Handle != nullptr)
            {
                return &surface;
            }
        }

        return nullptr;
    };

    const Surface* screenSurface = surfaceOf(screen);

    if (screenSurface == nullptr)
    {
        return std::vector<uint8_t>{};
    }

    const auto width = static_cast<int32_t>(screenSurface->Target.Width);
    const auto height = static_cast<int32_t>(screenSurface->Target.Height);

    if (width != screen->x_max + 1 || height != screen->y_max + 1)
    {
        return std::unexpected(std::format("the screen's surface is {}x{}, the screen {}x{}", width, height,
                                           screen->x_max + 1, screen->y_max + 1));
    }

    // The screen and the world surfaces under it, in the order the composite draws them (a later one over an earlier).
    struct Shown
    {
        MCRect Rect;
        size_t World;
    };

    std::vector<const Surface*> wanted{screenSurface};
    std::vector<Shown> shown;

    for (const MCUnderlay& underlay : underlays)
    {
        const Surface* world = surfaceOf(underlay.Source);

        if (MCRenderer::FrameSurfaceOf(underlay.Target) != screenSurface->Window || world == nullptr ||
            world == screenSurface)
        {
            continue;
        }

        const auto found = std::ranges::find(wanted, world);
        shown.push_back(Shown{underlay.Rect, static_cast<size_t>(found - wanted.begin())});

        if (found == wanted.end())
        {
            wanted.push_back(world);
        }
    }

    auto downloaded = Download(wanted);

    if (!downloaded)
    {
        return std::unexpected(downloaded.error());
    }

    const std::vector<uint8_t>& gpu = (*downloaded)[0];
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height);

    for (size_t i = 0; i < pixels.size(); ++i)
    {
        pixels[i] = gpu[i * 4];
    }

    for (const Shown& over : shown)
    {
        const Surface* world = wanted[over.World];
        const uint8_t* worldPixels = (*downloaded)[over.World].data();

        for (int32_t y = std::max(over.Rect.Y0, 0); y <= std::min(over.Rect.Y1, height - 1); ++y)
        {
            for (int32_t x = std::max(over.Rect.X0, 0); x <= std::min(over.Rect.X1, width - 1); ++x)
            {
                const size_t at = static_cast<size_t>(y) * width + x;
                pixels[at] =
                    ShownIndex(&gpu[at * 4], x, y, over.Rect, worldPixels, world->Target.Width, world->Target.Height);
            }
        }
    }

    return pixels;
}

auto MCVulkanRenderer::Compare(std::span<const MCUnderlay> underlays, const SDL_Color* colors)
    -> std::expected<Comparison, std::string>
{
    // Every live surface, downloaded at once.
    std::vector<const Surface*> surfaces;

    for (const Surface& surface : _Surfaces)
    {
        if (surface.Target.Handle != nullptr && MCRenderer::FrameSurfaceOf(surface.Window) == surface.Window &&
            surface.Window->buffer != nullptr &&
            static_cast<uint32_t>(surface.Window->x_max + 1) == surface.Target.Width &&
            static_cast<uint32_t>(surface.Window->y_max + 1) == surface.Target.Height)
        {
            surfaces.push_back(&surface);
        }
    }

    Comparison result;

    if (surfaces.empty())
    {
        return result;
    }

    auto downloaded = Download(surfaces);

    if (!downloaded)
    {
        return std::unexpected(downloaded.error());
    }

    const auto gpuPixels = [&](const _window* window) -> const uint8_t*
    {
        for (size_t i = 0; i < surfaces.size(); ++i)
        {
            if (surfaces[i]->Window == MCRenderer::FrameSurfaceOf(window))
            {
                return (*downloaded)[i].data();
            }
        }

        return nullptr;
    };

    for (size_t surfaceIndex = 0; surfaceIndex < surfaces.size(); ++surfaceIndex)
    {
        const _window* window = surfaces[surfaceIndex]->Window;
        const uint8_t* gpu = (*downloaded)[surfaceIndex].data();
        const int32_t width = window->x_max + 1;
        const int32_t height = window->y_max + 1;
        const int64_t differentBefore = result.Different;

        for (int32_t y = 0; y < height; ++y)
        {
            for (int32_t x = 0; x < width; ++x)
            {
                const uint8_t cpu = window->buffer[static_cast<size_t>(y) * width + x];
                const uint8_t drawn = gpu[(static_cast<size_t>(y) * width + x) * 4];

                if (cpu != drawn)
                {
                    if (result.First.empty())
                    {
                        result.First = std::format("{}x{} surface at ({}, {}): software {} GPU {}", width, height, x, y,
                                                   cpu, drawn);
                    }

                    ++result.Different;
                }
            }
        }

        // -gpudump <folder>: the first differing frame of each surface, as both renderers drew it.
        const std::filesystem::path& dump = MCRenderer::MirrorDumpFolder();

        if (!dump.empty() && colors != nullptr && result.Different != differentBefore && _Dumped.insert(window).second)
        {
            std::vector<uint8_t> drawn(static_cast<size_t>(width) * height);

            for (size_t i = 0; i < drawn.size(); ++i)
            {
                drawn[i] = gpu[i * 4];
            }

            using Image = std::pair<const char*, const uint8_t*>;

            for (const auto& [name, pixels] : {Image{"software", window->buffer}, Image{"gpu", drawn.data()}})
            {
                SDL_Surface* image = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);

                if (image == nullptr)
                {
                    continue;
                }

                // The colours shown, or false colours while they are all black (a fade).
                std::array<SDL_Color, 256> shown{};
                const bool black = std::all_of(colors, colors + 256, [](const SDL_Color& color)
                                               { return color.r == 0 && color.g == 0 && color.b == 0; });

                for (size_t i = 0; i < 256; ++i)
                {
                    shown[i] = black ? SDL_Color{static_cast<uint8_t>(i), static_cast<uint8_t>(i * 7),
                                                 static_cast<uint8_t>(i * 13), 255}
                                     : colors[i];
                }

                SDL_SetPaletteColors(SDL_CreateSurfacePalette(image), shown.data(), 0, 256);

                for (int32_t y = 0; y < height; ++y)
                {
                    std::memcpy(static_cast<uint8_t*>(image->pixels) + static_cast<ptrdiff_t>(y) * image->pitch,
                                pixels + static_cast<size_t>(y) * width, static_cast<size_t>(width));
                }

                const std::filesystem::path path =
                    dump / std::format("mirror{}_{}x{}_{}.png", _Mirror.Frames, width, height, name);
                SDL_SavePNG(image, path.string().c_str());
                SDL_DestroySurface(image);
            }
        }

        // Over the world: the shown pixels (the CPU composite and the GPU's see-through pixels).
        if (MCRenderer::OpPlane(window) == nullptr)
        {
            continue;
        }

        std::vector<uint8_t> composed(window->buffer, window->buffer + static_cast<size_t>(width) * height);
        MCRenderer::ComposeUnderlays(window, composed.data(), MCRect{0, 0, width - 1, height - 1});

        for (const MCUnderlay& underlay : underlays)
        {
            const uint8_t* world = gpuPixels(underlay.Source);

            if (MCRenderer::FrameSurfaceOf(underlay.Target) != window || world == nullptr)
            {
                continue;
            }

            const MCRect& shown = underlay.Rect;

            for (int32_t y = std::max(shown.Y0, 0); y <= std::min(shown.Y1, height - 1); ++y)
            {
                for (int32_t x = std::max(shown.X0, 0); x <= std::min(shown.X1, width - 1); ++x)
                {
                    const uint8_t value = ShownIndex(gpu + (static_cast<size_t>(y) * width + x) * 4, x, y, shown, world,
                                                     underlay.Source->x_max + 1, underlay.Source->y_max + 1);
                    const uint8_t cpu = composed[static_cast<size_t>(y) * width + x];

                    if (cpu != value)
                    {
                        if (result.First.empty())
                        {
                            result.First = std::format("{}x{} screen as shown at ({}, {}): software {} GPU {}", width,
                                                       height, x, y, cpu, value);
                        }

                        ++result.ShownDifferent;
                    }
                }
            }

            break;
        }
    }

    if (result.Different != 0 || result.ShownDifferent != 0)
    {
        if (_Mirror.DifferentFrames < 5)
        {
            SDL_Log("MCVulkanRenderer: frame %lld differs in %lld pixels (%lld shown): %s",
                    static_cast<long long>(_Mirror.Frames), static_cast<long long>(result.Different),
                    static_cast<long long>(result.ShownDifferent), result.First.c_str());
        }

        if (_Mirror.FirstFrame < 0)
        {
            _Mirror.FirstFrame = _Mirror.Frames;
            _Mirror.First = result.First;
        }

        ++_Mirror.DifferentFrames;
    }

    ++_Mirror.Frames;
    _Mirror.Last = result;
    return result;
}
