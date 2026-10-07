#include "stdafx.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCPacketFile.h"
#include "logistics/logbri.h"
#include "ai/move.h"
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
    /// The body of <c>aPort::init(long, long)</c> and <c>aScrollPort::init</c>: they differ only in where the pixels
    /// come from (<paramref name="allocPixels"/>, <paramref name="freePixels"/>).
    /// </summary>
    template <typename Alloc, typename Free>
    int32_t InitPort(MCGuiPort* port, int32_t width, int32_t height, Alloc allocPixels, Free freePixels)
    {
        MCWindow* window = port->PortWindow;

        if (window != nullptr)
        {
            MCRenderer::DestroyTexture(window);

            if (window->Buffer != nullptr)
            {
                freePixels(window->Buffer);
            }

            delete window;
        }

        window = new MCWindow{};
        port->PortWindow = window;
        window->View = nullptr;
        window->Texture = nullptr;
        window->XMax = width - 1;
        window->YMax = height - 1;

        // The original zeroes two more dwords past its 12-byte window (a 0x14-byte block).
        if (port != ScreenPort)
        {
            window->Buffer = static_cast<uint8_t*>(allocPixels(static_cast<uint32_t>(width * height)));

            if (window->Buffer == nullptr)
            {
                return 3;
            }
        }
        else
        {
            // Port fix: the original leaves the screen port's buffer uninitialised here (asystem sets it).
            window->Buffer = nullptr;
        }

        delete port->PortPane;
        MCPane* pane = new MCPane{};
        port->PortPane = pane;
        pane->X0 = 0;
        pane->Y0 = 0;
        SetExtent(window, pane, width, height);
        port->PortWidth = width;
        port->PortHeight = height;

        if (window->Buffer != nullptr)
        {
            MCRenderer::CreateTexture(window, MCTextureUse::Dynamic);
        }

        return 0;
    }

    /// <summary>The body of <c>aPort::resize</c> and <c>aScrollPort::resize</c>.</summary>
    template <typename Alloc, typename Free>
    int32_t ResizePort(MCGuiPort* port, int32_t width, int32_t height, Alloc allocPixels, Free freePixels)
    {
        // Port: a view has no pixels to reallocate.
        if (port != ScreenPort && !port->IsView())
        {
            MCWindow* window = port->PortWindow;

            if (window->Buffer != nullptr)
            {
                freePixels(window->Buffer);
            }

            window->Buffer = static_cast<uint8_t*>(allocPixels(static_cast<uint32_t>(width * height)));
        }

        port->PortHeight = height;
        SetExtent(port->PortWindow, port->PortPane, width, height);
        port->PortWidth = width;
        MCRenderer::ResizeTexture(port->PortWindow);
        return 0;
    }

    /// <summary>The body of <c>aPort::destroy</c> and <c>aScrollPort::destroy</c>.</summary>
    template <typename Free> void DestroyPort(MCGuiPort* port, Free freePixels)
    {
        if (port->PortWindow != nullptr)
        {
            MCRenderer::DestroyTexture(port->PortWindow);

            if (port->PortWindow->Buffer != nullptr)
            {
                freePixels(port->PortWindow->Buffer);
            }

            delete port->PortWindow;
            port->PortWindow = nullptr;
        }

        delete port->PortPane;
        port->PortPane = nullptr;
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

    void* CrtAlloc(uint32_t size)
    {
        return std::malloc(size);
    }

    void CrtFree(void* block)
    {
        std::free(block);
    }
}

// aPort

MCGuiPort::MCGuiPort()
{
}

MCGuiPort::~MCGuiPort()
{
    Destroy();
}

auto MCGuiPort::Init(int32_t width, int32_t height) -> int32_t
{
    if (width == PortWidth && height == PortHeight)
    {
        return 0;
    }

    return InitPort(this, width, height, GuiAlloc, GuiFree);
}

auto MCGuiPort::Init(int32_t artPacket) -> int32_t
{
    if (ArtFile->SeekPacket(artPacket) != 0)
    {
        Fatal(0, "Art packet not found");
        return -1;
    }

    MCFile file;

    if (file.Open(ArtFile, static_cast<uint32_t>(ArtFile->GetPacketSize())) != 0)
    {
        Fatal(0, "Cant open child file.");
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        Fatal(-2, "Bad child file in art.pak");
    }

    std::vector<uint8_t> gif(size);
    file.Read(gif.data(), static_cast<int32_t>(size));
    file.Close();

    const uint32_t resolution = static_cast<uint32_t>(VfxGifResolution(gif.data()));
    std::vector<uint8_t> decodeBuffer(0x502e);
    const int32_t result = Init(static_cast<int32_t>(resolution >> 16), static_cast<int32_t>(resolution & 0xffff));

    if (result != 0)
    {
        return result;
    }

    VfxGifDraw(PortPane, gif.data(), decodeBuffer.data());
    return 0;
}

auto MCGuiPort::Init(char* fileName) -> int32_t
{
    char path[256];
    MCFile file;
    bool opened = false;

    if (CurPlanet == 1)
    {
        std::snprintf(path, sizeof(path), "%sx%s", ArtPath, fileName);
        opened = file.Open(path) == 0;
    }

    if (!opened)
    {
        std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

        if (file.Open(path) != 0)
        {
            std::snprintf(path, sizeof(path), "%s", fileName);

            if (file.Open(path) != 0)
            {
                char message[256];
                std::snprintf(message, sizeof(message), "Error reading '%s'", path);
                GeneralMsg(message);
                return -2;
            }
        }
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
        return -2;
    }

    // Port: the original falls back to VirtualAlloc when malloc fails ("dataBufferVA"); one allocation does here.
    auto* data = static_cast<uint8_t*>(std::malloc(size));

    if (data == nullptr)
    {
        char message[256];
        std::snprintf(message, sizeof(message), " Could Not Commit %d bytes in Heap of size %d ", size, size);
        Fatal(0, message);
    }

    file.Read(data, static_cast<int32_t>(size));
    file.Close();

    // A TGA header: the image width and height are the 16-bit values at +0x0c and +0x0e, and the 8-bit pixels follow
    // the 18-byte header and a 256-entry palette.
    int16_t tgaWidth = 0;
    int16_t tgaHeight = 0;
    std::memcpy(&tgaWidth, data + 0x0c, sizeof(tgaWidth));
    std::memcpy(&tgaHeight, data + 0x0e, sizeof(tgaHeight));
    const int32_t width = tgaWidth;
    const int32_t height = tgaHeight;
    const int32_t result = Init(width, height);

    if (result != 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Failed trying to create a %d by %d aPort with dataBuffer", width,
                      height);
        Fatal(result, message);
    }

    MCTexture* texture = PortPane->Window->Texture;
    std::memcpy(MCRenderer::LockTexture(texture), data + 0x312, static_cast<size_t>(height * width));
    MCRenderer::UnlockTexture(texture);
    std::free(data);
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
    DestroyPort(this, GuiFree);
}

auto MCGuiPort::Resize(int32_t width, int32_t height) -> int32_t
{
    return ResizePort(this, width, height, GuiAlloc, GuiFree);
}

auto MCGuiPort::InitView(int32_t width, int32_t height) -> int32_t
{
    if (IsView() && width == PortWidth && height == PortHeight)
    {
        return 0;
    }

    DestroyPort(this, GuiFree);
    auto* window = new MCWindow{};
    auto* pane = new MCPane{};
    PortWindow = window;
    PortPane = pane;
    window->Buffer = nullptr;
    window->Texture = nullptr;
    window->View = &View;
    View = MCView{};
    pane->X0 = 0;
    pane->Y0 = 0;
    SetExtent(window, pane, width, height);
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

auto MCGuiPort::CopyTo(MCPane* dest, int32_t xPos, int32_t yPos, int transparent) -> void
{
    // Port: a view has no picture to copy (its owner draws itself in the frame pass).
    if (IsView())
    {
        return;
    }

    if (transparent != 0)
    {
        MCWindow* source = PortPane->Window;
        DrawTransparent(dest, source, xPos, yPos, source->XMax + 1, source->YMax + 1);
        return;
    }

    VfxPaneCopy(PortPane, 0, 0, dest, xPos, yPos, -1);
}

auto MCGuiPort::Width() -> int32_t
{
    return PortWidth;
}

auto MCGuiPort::Height() -> int32_t
{
    return PortHeight;
}

auto MCGuiPort::Bitmap() -> MCWindow*
{
    return PortWindow;
}

auto MCGuiPort::Frame() -> MCPane*
{
    return PortPane;
}

// aScrollPort

auto MCGuiScrollPort::Init(int32_t width, int32_t height) -> int32_t
{
    // Unlike aPort::init, this always reallocates, even at the same size.
    return InitPort(this, width, height, CrtAlloc, CrtFree);
}

auto MCGuiScrollPort::Destroy() -> void
{
    // Unlike aPort::destroy, the size is left as it was.
    DestroyPort(this, CrtFree);
}

auto MCGuiScrollPort::Resize(int32_t width, int32_t height) -> int32_t
{
    return ResizePort(this, width, height, CrtAlloc, CrtFree);
}
