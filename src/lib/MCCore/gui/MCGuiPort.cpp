#include "stdafx.h"
#include "gui/MCGuiPort.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
#include "main/MCGamePaths.h"
#include "ai/MCMoveGeometry.h"
#include "vfx/MCVfxFunctions.h"
#include "platform/MCBlockStore.h"

namespace
{
    /// <summary>
    /// Points <paramref name="window"/> and <paramref name="pane"/> at a <paramref name="width"/> x
    /// <paramref name="height"/> bitmap (the pane covers all of it).
    /// </summary>
    void SetExtent(MCWindow* window, MCPane* pane, int32_t width, int32_t height)
    {
        window->XMax = width - 1;
        window->YMax = height - 1;
        pane->Window = window;
        pane->X1 = width - 1;
        pane->Y1 = height - 1;
    }

    /// <summary>
    /// The bitmaps of the ports (the GUI heap's in the original). A port's bitmap isn't always its own (the screen
    /// port's is the screen, the fog port's the fog flags), and freeing one that isn't from here is ignored, as the
    /// heap did.
    /// </summary>
    MCBlockStore PixelBlocks;

    void* GuiAlloc(uint32_t size)
    {
        return PixelBlocks.Allocate(size);
    }

    void GuiFree(void* block)
    {
        PixelBlocks.Free(block);
    }

    /// <summary>The bitmaps of the scroll ports (the C runtime heap's in the original).</summary>
    MCBlockStore ScrollPixelBlocks;

    void* ScrollAlloc(uint32_t size)
    {
        return ScrollPixelBlocks.Allocate(size);
    }

    void ScrollFree(void* block)
    {
        ScrollPixelBlocks.Free(block);
    }
}

const MCGuiPort::PixelSource MCGuiPort::GuiPixels{GuiAlloc, GuiFree};
const MCGuiPort::PixelSource MCGuiScrollPort::ScrollPixels{ScrollAlloc, ScrollFree};

// aPort

MCGuiPort::~MCGuiPort()
{
    FreeBitmap(GuiPixels);
}

auto MCGuiPort::MakeBitmap(int32_t width, int32_t height, const PixelSource& pixels) -> int32_t
{
    if (_Window != nullptr)
    {
        MCRenderer::DestroyTexture(_Window.get());

        if (_Window->Buffer != nullptr)
        {
            pixels.Free(_Window->Buffer);
        }
    }

    _Window = std::make_unique<MCWindow>();
    MCWindow* window = _Window.get();
    window->XMax = width - 1;
    window->YMax = height - 1;

    if (!_Screen)
    {
        window->Buffer = static_cast<uint8_t*>(pixels.Allocate(static_cast<uint32_t>(width * height)));

        if (window->Buffer == nullptr)
        {
            return 3;
        }
    }

    _Pane = std::make_unique<MCPane>();
    SetExtent(window, _Pane.get(), width, height);
    PortWidth = width;
    PortHeight = height;

    if (window->Buffer != nullptr)
    {
        MCRenderer::CreateTexture(window, MCTextureUse::Dynamic);
    }

    return 0;
}

auto MCGuiPort::ResizeBitmap(int32_t width, int32_t height, const PixelSource& pixels) -> int32_t
{
    // A view has no pixels to reallocate, and the screen port's are the display's.
    if (!_Screen && !IsView())
    {
        if (_Window->Buffer != nullptr)
        {
            pixels.Free(_Window->Buffer);
        }

        _Window->Buffer = static_cast<uint8_t*>(pixels.Allocate(static_cast<uint32_t>(width * height)));
    }

    SetExtent(_Window.get(), _Pane.get(), width, height);
    PortWidth = width;
    PortHeight = height;
    MCRenderer::ResizeTexture(_Window.get());
    return 0;
}

auto MCGuiPort::FreeBitmap(const PixelSource& pixels) -> void
{
    if (_Window != nullptr)
    {
        MCRenderer::DestroyTexture(_Window.get());

        if (_Window->Buffer != nullptr && !_Screen)
        {
            pixels.Free(_Window->Buffer);
        }
    }

    _Window.reset();
    _Pane.reset();
}

auto MCGuiPort::Init(int32_t width, int32_t height) -> int32_t
{
    if (width == PortWidth && height == PortHeight)
    {
        return 0;
    }

    return MakeBitmap(width, height, GuiPixels);
}

auto MCGuiPort::InitScreen(int32_t width, int32_t height) -> void
{
    _Screen = true;
    MakeBitmap(width, height, GuiPixels);
}

auto MCGuiPort::Init(int32_t artPacket) -> int32_t
{
    MCPacketFile* artFile = GuiSystem()->ArtFile.get();

    if (artFile->SeekPacket(artPacket) != 0)
    {
        Fatal(0, "Art packet not found");
    }

    MCFile file;

    if (file.Open(artFile, static_cast<uint32_t>(artFile->GetPacketSize())) != 0)
    {
        Fatal(0, "Cant open child file.");
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        Fatal(-2, "Bad child file in art.pak");
    }

    std::vector<uint8_t> gif(size);
    file.Read(gif);
    file.Close();

    const uint32_t resolution = static_cast<uint32_t>(VfxGifResolution(gif.data()));
    std::vector<uint8_t> decodeBuffer(0x502e);
    const int32_t result = Init(static_cast<int32_t>(resolution >> 16), static_cast<int32_t>(resolution & 0xffff));

    if (result != 0)
    {
        return result;
    }

    VfxGifDraw(_Pane.get(), gif.data(), decodeBuffer.data());
    return 0;
}

auto MCGuiPort::Init(std::string_view fileName) -> int32_t
{
    std::string path;
    MCFile file;
    bool opened = false;

    if (CurPlanet == 1)
    {
        path = std::format("{}x{}", std::string_view(ArtPath), fileName);
        opened = file.Open(path) == 0;
    }

    if (!opened)
    {
        path = std::format("{}{}", std::string_view(ArtPath), fileName);

        if (file.Open(path) != 0)
        {
            path = fileName;

            if (file.Open(path) != 0)
            {
                GeneralMsg(std::format("Error reading '{}'", path));
            }
        }
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        GeneralMsg(std::format("Error reading '{}'", path));
    }

    std::vector<uint8_t> data(size);
    file.Read(data);
    file.Close();

    // A TGA header: the image width and height are the 16-bit values at +0x0c and +0x0e, and the 8-bit pixels follow
    // the 18-byte header and a 256-entry palette.
    int16_t tgaWidth = 0;
    int16_t tgaHeight = 0;
    std::memcpy(&tgaWidth, data.data() + 0x0c, sizeof(tgaWidth));
    std::memcpy(&tgaHeight, data.data() + 0x0e, sizeof(tgaHeight));
    const int32_t width = tgaWidth;
    const int32_t height = tgaHeight;
    const int32_t result = Init(width, height);

    if (result != 0)
    {
        Fatal(result, std::format("Failed trying to create a {} by {} aPort with dataBuffer", width, height));
    }

    MCTexture* texture = _Pane->Window->Texture;
    std::memcpy(MCRenderer::LockTexture(texture), data.data() + 0x312, static_cast<size_t>(height * width));
    MCRenderer::UnlockTexture(texture);
    return 0;
}

auto MCGuiPort::FreePixels(uint8_t* pixels) -> void
{
    GuiFree(pixels);
}

auto MCGuiPort::Destroy() -> void
{
    PortHeight = -1;
    PortWidth = -1;
    FreeBitmap(GuiPixels);
}

auto MCGuiPort::Resize(int32_t width, int32_t height) -> int32_t
{
    return ResizeBitmap(width, height, GuiPixels);
}

auto MCGuiPort::InitView(int32_t width, int32_t height) -> int32_t
{
    if (IsView() && width == PortWidth && height == PortHeight)
    {
        return 0;
    }

    FreeBitmap(GuiPixels);
    _Window = std::make_unique<MCWindow>();
    _Pane = std::make_unique<MCPane>();
    _Window->View = &View;
    View = MCView{};
    SetExtent(_Window.get(), _Pane.get(), width, height);
    PortWidth = width;
    PortHeight = height;
    return 0;
}

auto MCGuiPort::OpenView(MCWindow* target, int32_t x, int32_t y, const MCRect& scissor, bool keyTransparent) -> void
{
    View.Target = target;
    View.OriginX = x;
    View.OriginY = y;
    View.Scissor = scissor;
    View.KeyTransparent = keyTransparent;

    // A view as the target (a block drawn in place, see lBlockPort): the view lands on that view's target, moved by
    // its origin and cut to its scissor; shut when it is.
    if (const MCView* outer = target != nullptr ? target->View : nullptr; outer != nullptr)
    {
        if (outer->Target == nullptr || !outer->Open())
        {
            View.Target = nullptr;
            CloseView();
            return;
        }

        View.Target = outer->Target;
        View.OriginX += outer->OriginX;
        View.OriginY += outer->OriginY;
        View.Scissor.X0 = std::max({scissor.X0, 0}) + outer->OriginX;
        View.Scissor.Y0 = std::max({scissor.Y0, 0}) + outer->OriginY;
        View.Scissor.X1 = std::min(scissor.X1, target->XMax) + outer->OriginX;
        View.Scissor.Y1 = std::min(scissor.Y1, target->YMax) + outer->OriginY;
        View.Scissor.X0 = std::max(View.Scissor.X0, outer->Scissor.X0);
        View.Scissor.Y0 = std::max(View.Scissor.Y0, outer->Scissor.Y0);
        View.Scissor.X1 = std::min(View.Scissor.X1, outer->Scissor.X1);
        View.Scissor.Y1 = std::min(View.Scissor.Y1, outer->Scissor.Y1);
        View.KeyTransparent = keyTransparent || outer->KeyTransparent;
    }
}

auto MCGuiPort::OpenViewOn(MCPane* dest, int32_t xPos, int32_t yPos, bool keyTransparent) -> void
{
    // The block in the destination window's coordinates: the pane cut to its window, and to the view's size.
    MCWindow* target = dest->Window;
    const int32_t originX = dest->X0 + xPos;
    const int32_t originY = dest->Y0 + yPos;
    const MCRect scissor{std::max({dest->X0, 0, originX}), std::max({dest->Y0, 0, originY}),
                         std::min({dest->X1, target->XMax, originX + PortWidth - 1}),
                         std::min({dest->Y1, target->YMax, originY + PortHeight - 1})};
    OpenView(target, originX, originY, scissor, keyTransparent);
}

auto MCGuiPort::CopyTo(MCPane* dest, int32_t xPos, int32_t yPos, bool transparent) -> void
{
    // A view has no picture to copy (its owner draws itself in the frame pass).
    if (IsView())
    {
        return;
    }

    if (transparent)
    {
        MCWindow* source = _Pane->Window;
        DrawTransparent(dest, source, xPos, yPos, source->XMax + 1, source->YMax + 1);
        return;
    }

    VfxPaneCopy(_Pane.get(), 0, 0, dest, xPos, yPos, -1);
}

// aScrollPort

MCGuiScrollPort::~MCGuiScrollPort()
{
    FreeBitmap(ScrollPixels);
}

auto MCGuiScrollPort::Init(int32_t width, int32_t height) -> int32_t
{
    return MakeBitmap(width, height, ScrollPixels);
}

auto MCGuiScrollPort::Destroy() -> void
{
    FreeBitmap(ScrollPixels);
}

auto MCGuiScrollPort::Resize(int32_t width, int32_t height) -> int32_t
{
    return ResizeBitmap(width, height, ScrollPixels);
}
