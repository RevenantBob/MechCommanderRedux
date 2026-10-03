#include "stdafx.h"
#include "gui/aport.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/packet.h"
#include "logistics/logbri.h"
#include "ai/move.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>
    /// Points <paramref name="window"/> and <paramref name="pane"/> at a <paramref name="width"/> x
    /// <paramref name="height"/> bitmap (the pane covers all of it).
    /// </summary>
    void setExtent(_window* window, _pane* pane, int32_t width, int32_t height)
    {
        window->x_max = width - 1;
        window->y_max = height - 1;
        pane->window = window;
        pane->x1 = width - 1;
        pane->y1 = height - 1;
    }

    /// <summary>
    /// The body of <c>aPort::init(long, long)</c> and <c>aScrollPort::init</c>: they differ only in where the pixels
    /// come from (<paramref name="allocPixels"/>, <paramref name="freePixels"/>).
    /// </summary>
    template <typename Alloc, typename Free>
    int32_t initPort(aPort* port, int32_t width, int32_t height, Alloc allocPixels, Free freePixels)
    {
        _window* window = port->portWindow;

        if (window != nullptr)
        {
            if (window->buffer != nullptr)
            {
                freePixels(window->buffer);
            }

            guiHeap->free(window);
        }

        window = static_cast<_window*>(guiHeap->malloc(sizeof(_window)));
        port->portWindow = window;

        if (window == nullptr)
        {
            return 3;
        }

        window->x_max = width - 1;
        window->y_max = height - 1;

        // The original zeroes two more dwords past its 12-byte window (a 0x14-byte block).
        if (port != screenPort)
        {
            window->buffer = static_cast<uint8_t*>(allocPixels(static_cast<uint32_t>(width * height)));

            if (window->buffer == nullptr)
            {
                return 3;
            }
        }
        else
        {
            // Port fix: the original leaves the screen port's buffer uninitialised here (asystem sets it).
            window->buffer = nullptr;
        }

        if (port->portPane != nullptr)
        {
            guiHeap->free(port->portPane);
        }

        _pane* pane = static_cast<_pane*>(guiHeap->malloc(sizeof(_pane)));
        port->portPane = pane;

        if (pane == nullptr)
        {
            return 3;
        }

        pane->x0 = 0;
        pane->y0 = 0;
        setExtent(window, pane, width, height);
        port->portWidth = width;
        port->portHeight = height;
        return 0;
    }

    /// <summary>The body of <c>aPort::resize</c> and <c>aScrollPort::resize</c>.</summary>
    template <typename Alloc, typename Free>
    int32_t resizePort(aPort* port, int32_t width, int32_t height, Alloc allocPixels, Free freePixels)
    {
        // Port: a view has no pixels to reallocate.
        if (port != screenPort && !port->isView())
        {
            _window* window = port->portWindow;

            if (window->buffer != nullptr)
            {
                freePixels(window->buffer);
            }

            window->buffer = static_cast<uint8_t*>(allocPixels(static_cast<uint32_t>(width * height)));
        }

        port->portHeight = height;
        setExtent(port->portWindow, port->portPane, width, height);
        port->portWidth = width;
        return 0;
    }

    /// <summary>The body of <c>aPort::destroy</c> and <c>aScrollPort::destroy</c>.</summary>
    template <typename Free> void destroyPort(aPort* port, Free freePixels)
    {
        if (port->portWindow != nullptr)
        {
            if (port->portWindow->buffer != nullptr)
            {
                freePixels(port->portWindow->buffer);
            }

            guiHeap->free(port->portWindow);
            port->portWindow = nullptr;
        }

        if (port->portPane != nullptr)
        {
            guiHeap->free(port->portPane);
            port->portPane = nullptr;
        }
    }

    void* guiAlloc(uint32_t size)
    {
        return guiHeap->malloc(size);
    }

    void guiFree(void* block)
    {
        guiHeap->free(block);
    }

    void* crtAlloc(uint32_t size)
    {
        return std::malloc(size);
    }

    void crtFree(void* block)
    {
        std::free(block);
    }
}

// aPort

auto aPort::operator new(size_t size) noexcept -> void*
{
    return guiHeap->malloc(static_cast<uint32_t>(size));
}

auto aPort::operator delete(void* ptr) -> void
{
    guiHeap->free(ptr);
}

aPort::aPort()
{
}

aPort::~aPort()
{
    destroy();
}

auto aPort::init(int32_t width, int32_t height) -> int32_t
{
    if (width == portWidth && height == portHeight)
    {
        return 0;
    }

    return initPort(this, width, height, guiAlloc, guiFree);
}

auto aPort::init(int32_t artPacket) -> int32_t
{
    if (artFile->seekPacket(artPacket) != 0)
    {
        Fatal(0, "Art packet not found");
        return -1;
    }

    File file;

    if (file.open(artFile, static_cast<uint32_t>(artFile->getPacketSize()), 0x32) != 0)
    {
        Fatal(0, "Cant open child file.");
    }

    const uint32_t size = file.fileSize();

    if (size == 0)
    {
        Fatal(-2, "Bad child file in art.pak");
    }

    auto* gif = static_cast<uint8_t*>(guiHeap->malloc(size));

    if (gif == nullptr)
    {
        Fatal(3, "Not enough memory to read art file");
    }

    file.read(gif, static_cast<int32_t>(size));
    file.close();

    const uint32_t resolution = static_cast<uint32_t>(VFX_GIF_resolution(gif));
    void* decodeBuffer = std::malloc(0x502e);

    if (decodeBuffer == nullptr)
    {
        Fatal(3, "Not enough memory to decode gif file");
    }

    const int32_t result = init(static_cast<int32_t>(resolution >> 16), static_cast<int32_t>(resolution & 0xffff));

    if (result != 0)
    {
        return result; // The original leaks both buffers here too.
    }

    VFX_GIF_draw(portPane, gif, decodeBuffer);
    guiHeap->free(gif);
    std::free(decodeBuffer);
    return 0;
}

auto aPort::init(char* fileName) -> int32_t
{
    char path[256];
    File file;
    bool opened = false;

    if (CurPlanet == 1)
    {
        std::snprintf(path, sizeof(path), "%sx%s", artPath, fileName);
        opened = file.open(path, READ, 0x32) == 0;
    }

    if (!opened)
    {
        std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

        if (file.open(path, READ, 0x32) != 0)
        {
            std::snprintf(path, sizeof(path), "%s", fileName);

            if (file.open(path, READ, 0x32) != 0)
            {
                char message[256];
                std::snprintf(message, sizeof(message), "Error reading '%s'", path);
                GeneralMsg(message);
                return -2;
            }
        }
    }

    const uint32_t size = file.fileSize();

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

    file.read(data, static_cast<int32_t>(size));
    file.close();

    // A TGA header: the image width and height are the 16-bit values at +0x0c and +0x0e, and the 8-bit pixels follow
    // the 18-byte header and a 256-entry palette.
    int16_t tgaWidth = 0;
    int16_t tgaHeight = 0;
    std::memcpy(&tgaWidth, data + 0x0c, sizeof(tgaWidth));
    std::memcpy(&tgaHeight, data + 0x0e, sizeof(tgaHeight));
    const int32_t width = tgaWidth;
    const int32_t height = tgaHeight;
    const int32_t result = init(width, height);

    if (result != 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Failed trying to create a %d by %d aPort with dataBuffer", width,
                      height);
        Fatal(result, message);
    }

    std::memcpy(portPane->window->buffer, data + 0x312, static_cast<size_t>(height * width));
    MCRenderer::PixelsChanged(portPane->window);
    std::free(data);
    return 0;
}

auto aPort::destroy() -> void
{
    portHeight = -1;
    portWidth = -1;
    destroyPort(this, guiFree);
}

auto aPort::resize(int32_t width, int32_t height) -> int32_t
{
    return resizePort(this, width, height, guiAlloc, guiFree);
}

auto aPort::initView(int32_t width, int32_t height) -> int32_t
{
    if (isView() && width == portWidth && height == portHeight)
    {
        return 0;
    }

    destroyPort(this, guiFree);
    auto* window = static_cast<_window*>(guiHeap->malloc(sizeof(_window)));
    auto* pane = static_cast<_pane*>(guiHeap->malloc(sizeof(_pane)));

    if (window == nullptr || pane == nullptr)
    {
        return 3;
    }

    portWindow = window;
    portPane = pane;
    window->buffer = nullptr;
    window->View = &view;
    view = MCView{};
    pane->x0 = 0;
    pane->y0 = 0;
    setExtent(window, pane, width, height);
    portWidth = width;
    portHeight = height;
    return 0;
}

auto aPort::openView(_window* target, int32_t x, int32_t y, const MCRect& scissor, bool keyTransparent) -> void
{
    view.Target = target;
    view.OriginX = x;
    view.OriginY = y;
    view.Scissor = scissor;
    view.KeyTransparent = keyTransparent;
}

auto aPort::copyTo(_pane* dest, int32_t xPos, int32_t yPos, int transparent) -> void
{
    // Port: a view has no picture to copy (its owner draws itself in the frame pass).
    if (isView())
    {
        return;
    }

    if (transparent != 0)
    {
        _window* source = portPane->window;
        DrawTransparent(dest, source, xPos, yPos, source->x_max + 1, source->y_max + 1);
        return;
    }

    VFX_pane_copy(portPane, 0, 0, dest, xPos, yPos, -1);
}

auto aPort::width() -> int32_t
{
    return portWidth;
}

auto aPort::height() -> int32_t
{
    return portHeight;
}

auto aPort::bitmap() -> _window*
{
    return portWindow;
}

auto aPort::frame() -> _pane*
{
    return portPane;
}

// aScrollPort

auto aScrollPort::init(int32_t width, int32_t height) -> int32_t
{
    // Unlike aPort::init, this always reallocates, even at the same size.
    return initPort(this, width, height, crtAlloc, crtFree);
}

auto aScrollPort::destroy() -> void
{
    // Unlike aPort::destroy, the size is left as it was.
    destroyPort(this, crtFree);
}

auto aScrollPort::resize(int32_t width, int32_t height) -> int32_t
{
    return resizePort(this, width, height, crtAlloc, crtFree);
}
