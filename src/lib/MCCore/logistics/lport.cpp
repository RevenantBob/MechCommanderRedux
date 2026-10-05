#include "stdafx.h"
#include "logistics/lport.h"
#include "gui/aanim.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "platform/MCInput.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Allocates in a logistics block.</summary>
    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsBlocks->Allocate(size);
    }

    /// <summary>Frees a logistics block.</summary>
    void logFree(void* block)
    {
        globalLogPtr->logisticsBlocks->Free(block);
    }

    /// <summary>The art <see cref="logArt"/> loaded, by file name (null for a file that couldn't be read).</summary>
    std::unordered_map<std::string, lPort*> loadedArt;
}

auto logArt(const char* fileName) -> lPort*
{
    const auto found = loadedArt.find(fileName);

    if (found != loadedArt.end())
    {
        return found->second;
    }

    auto* art = new lPort;

    if (art->init(const_cast<char*>(fileName)) != 0)
    {
        delete art;
        art = nullptr;
    }

    loadedArt.emplace(fileName, art);
    return art;
}

auto logArtf(const char* format, ...) -> lPort*
{
    char fileName[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(fileName, sizeof(fileName), format, args);
    va_end(args);
    return logArt(fileName);
}

auto ClearLogArt() -> void
{
    for (auto& [name, art] : loadedArt)
    {
        delete art;
    }

    loadedArt.clear();
}

// lPort

auto lPort::init(int32_t width, int32_t height, int allocBitmap) -> int32_t
{
    if (portWindow != nullptr)
    {
        MCRenderer::DestroyTexture(portWindow);

        if (portWindow->buffer != nullptr)
        {
            logFree(portWindow->buffer);
        }

        logFree(portWindow);
    }

    auto* window = static_cast<_window*>(logAlloc(sizeof(_window)));
    portWindow = window;

    if (window == nullptr)
    {
        return 3;
    }

    window->View = nullptr;
    window->Texture = nullptr;
    window->x_max = width - 1;
    window->y_max = height - 1;
    // Port fix: the original left the buffer uninitialised without a bitmap (and destroy then freed it).
    window->buffer = nullptr;

    if (allocBitmap != 0 && this != screenPort)
    {
        window->buffer = static_cast<uint8_t*>(logAlloc(static_cast<uint32_t>(width * height)));

        if (window->buffer == nullptr)
        {
            return 3;
        }
    }

    if (portPane != nullptr)
    {
        logFree(portPane);
    }

    auto* pane = static_cast<_pane*>(logAlloc(sizeof(_pane)));
    portPane = pane;

    if (pane == nullptr)
    {
        return 3;
    }

    pane->window = window;
    pane->x0 = 0;
    pane->y0 = 0;
    portWidth = width;
    pane->x1 = width - 1;
    portHeight = height;
    pane->y1 = height - 1;

    if (window->buffer != nullptr)
    {
        MCRenderer::CreateTexture(window, MCTextureUse::Dynamic);
    }

    return 0;
}

auto lPort::init(char* fileName) -> int32_t
{
    char message[256];
    char path[256];
    File file;
    std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

    if (file.open(path, READ, 0x32) != 0)
    {
        std::snprintf(path, sizeof(path), "%s", fileName);

        if (file.open(path, READ, 0x32) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
            GeneralMsg(message);
            return -2;
        }
    }

    const uint32_t size = file.fileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
        GeneralMsg(message);
        return -2;
    }

    auto* data = static_cast<uint8_t*>(logAlloc(size));

    if (data == nullptr)
    {
        return 3;
    }

    file.read(data, static_cast<int32_t>(size));
    file.close();
    // A TGA header: the 16-bit width and height at +0x0c/+0x0e; the 8-bit pixels follow the 18-byte header and a
    // 256-entry palette.
    int16_t tgaWidth = 0;
    int16_t tgaHeight = 0;
    std::memcpy(&tgaWidth, data + 0x0c, sizeof(tgaWidth));
    std::memcpy(&tgaHeight, data + 0x0e, sizeof(tgaHeight));
    const int32_t result = init(tgaWidth, tgaHeight, -1);

    if (result != 0)
    {
        return result; // The original leaks the file data here.
    }

    MCTexture* texture = portPane->window->Texture;
    std::memcpy(MCRenderer::LockTexture(texture), data + 0x312, static_cast<size_t>(tgaHeight * tgaWidth));
    MCRenderer::UnlockTexture(texture);
    logFree(data);
    return 0;
}

auto lPort::initView(int32_t width, int32_t height) -> int32_t
{
    if (isView() && width == portWidth && height == portHeight)
    {
        return 0;
    }

    destroy();
    auto* window = static_cast<_window*>(logAlloc(sizeof(_window)));
    auto* pane = static_cast<_pane*>(logAlloc(sizeof(_pane)));

    if (window == nullptr || pane == nullptr)
    {
        return 3;
    }

    portWindow = window;
    portPane = pane;
    window->buffer = nullptr;
    window->Texture = nullptr;
    window->View = &view;
    window->x_max = width - 1;
    window->y_max = height - 1;
    view = MCView{};
    pane->window = window;
    pane->x0 = 0;
    pane->y0 = 0;
    pane->x1 = width - 1;
    pane->y1 = height - 1;
    portWidth = width;
    portHeight = height;
    return 0;
}

auto lPort::resize(int32_t width, int32_t height) -> int32_t
{
    // Port: a view has no pixels to reallocate.
    if (this != screenPort && !isView())
    {
        if (portWindow->buffer != nullptr)
        {
            logFree(portWindow->buffer);
        }

        portWindow->buffer = static_cast<uint8_t*>(logAlloc(static_cast<uint32_t>(width * height)));
    }

    portHeight = height;
    portPane->window = portWindow;
    portPane->x1 = width - 1;
    portPane->y1 = height - 1;
    portWindow->x_max = width - 1;
    portWindow->y_max = height - 1;
    portWidth = width;
    MCRenderer::ResizeTexture(portWindow);
    return 0;
}

auto lPort::destroy() -> void
{
    if (portWindow != nullptr)
    {
        MCRenderer::DestroyTexture(portWindow);

        if (portWindow->buffer != nullptr)
        {
            logFree(portWindow->buffer);
        }

        logFree(portWindow);
        portWindow = nullptr;
    }

    if (portPane != nullptr)
    {
        logFree(portPane);
        portPane = nullptr;
    }
}

// lObject

lObject::~lObject()
{
    lObject::destroy();
}

auto lObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name, lPort* port) -> int32_t
{
    (void)name;
    winX = xPos;
    maxX = xPos;
    normalX = xPos;
    iconX = xPos;
    homeX = xPos;
    winWidth = width;
    ownPort = nullptr;
    sharedPort = nullptr;
    winHeight = height;
    winY = yPos;
    maxWidth = width;
    maxHeight = height;
    maxY = yPos;
    normalWidth = width;
    normalHeight = height;
    normalY = yPos;
    iconWidth = width;
    iconHeight = height;
    iconY = yPos;
    hideOffset = 0;
    homeY = yPos;
    winState = aSTATE_NORMAL;
    showWindow = -1;
    dragOn = 0;
    transparent = 0;
    backgroundColor = 0xff;
    unknown4B8 = 0;

    if (port == nullptr)
    {
        ownPort = new lPort;
        const int32_t result = DrawsLive() ? ownPort->initView(width, height) : ownPort->init(width, height, -1);

        if (result != 0)
        {
            return result;
        }
    }
    else
    {
        sharedPort = port;
    }

    if (framePane != nullptr)
    {
        // Port fix: the original freed it with the CRT's delete although it came in a logistics block.
        logFree(framePane);
        framePane = nullptr;
    }

    framePane = static_cast<_pane*>(logAlloc(sizeof(_pane)));

    if (framePane == nullptr)
    {
        return 3;
    }

    framePane->window = screenPort->bitmap();
    framePane->x0 = xPos;
    framePane->y0 = yPos;
    framePane->x1 = xPos + width;
    hidden = 0;
    framePane->y1 = yPos + height;
    hideDirection = DIRECTION_DOWN;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    winDepth = 0;
    windowAnimation = nullptr;
    animating = 0;
    iconAnimation = nullptr;
    aObject::backgroundPort = nullptr;
    backgroundPort = nullptr;
    objectType = -1;
    return 0;
}

auto lObject::destroy() -> void
{
    application->RemoveTimers(this);

    if (ownPort != nullptr)
    {
        ownPort->destroy();
        delete ownPort;
        ownPort = nullptr;
    }

    if (framePane != nullptr)
    {
        logFree(framePane);
        framePane = nullptr;
    }

    if (aObject::backgroundPort != nullptr)
    {
        aObject::backgroundPort->destroy();
        delete aObject::backgroundPort;
        aObject::backgroundPort = nullptr;
    }

    if (backgroundPort != nullptr)
    {
        backgroundPort->destroy();
        delete backgroundPort;
        backgroundPort = nullptr;
    }

    if (iconAnimation != nullptr)
    {
        iconAnimation->destroy();
        delete iconAnimation;
        iconAnimation = nullptr;
    }

    if (windowAnimation != nullptr)
    {
        windowAnimation->destroy();
        delete windowAnimation;
        windowAnimation = nullptr;
    }

    if (numChildren > 0)
    {
        // Original behaviour (OB-071): removes the first child, then deletes whichever child is first after that
        // (its destroy removes it). The first child is therefore only unlinked, and each later pass "removes" the
        // child just deleted (no longer listed, so nothing happens).
        aObject* child = childList[0];

        do
        {
            removeChild(child);
            child = childList[0];

            if (child != nullptr)
            {
                delete child;
            }
        } while (numChildren > 0);
    }

    if (parent != nullptr)
    {
        parent->removeChild(this);
    }

    parent = nullptr;
    animating = 0;

    if (application->grabbedObject() == this)
    {
        application->release();
    }

    if (application->textObject() == this)
    {
        application->releaseText();
    }

    if (application->modalObject() == this)
    {
        application->clearModal();
    }

    if (application->currentObject() == this)
    {
        const MCPoint cursor = MCInput::GetCursorPos();
        application->setCurrentObject(screenWindow->findObject(cursor.x, cursor.y));
    }
}

auto lObject::lport() -> lPort*
{
    return ownPort;
}

auto lObject::draw() -> void
{
    const int32_t state = winState;

    if (state == aSTATE_ICONIZED)
    {
        iconAnimation->draw(ownPort->frame(), 0, 0);
    }
    else
    {
        if (backgroundPort != nullptr)
        {
            backgroundPort->copyTo(ownPort->frame(), 0, 0, -1);
        }

        if (windowAnimation != nullptr && animating != 0)
        {
            windowAnimation->draw(ownPort->frame(), 0, 0);
        }
    }

    if (state != aSTATE_ICONIZED)
    {
        paint();

        for (int32_t i = 0; i < numChildren; i++)
        {
            DrawChild(childList[i]);
        }
    }
}

auto lObject::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    // An object that draws itself does so after the slide has moved it.
    if (!DrawsLive())
    {
        if (winState == aSTATE_ICONIZED)
        {
            if (iconAnimation != nullptr)
            {
                draw();
            }
        }
        else if (windowAnimation != nullptr)
        {
            windowAnimation->draw(ownPort->frame(), 0, 0);
            draw();
        }
    }

    if (hideOffset != 0)
    {
        // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
        if (hideDirection == DIRECTION_LEFT || hideDirection == DIRECTION_RIGHT)
        {
            moveTo(x() + hideOffset, y(), -1);
        }
        else
        {
            moveTo(x(), y() + hideOffset, -1);
        }

        if (hidden != 0)
        {
            if (rectIntersect(0, 0, application->width(), application->height()) == 0)
            {
                hideOffset = 0;
            }
        }
        else
        {
            bool home = false;

            if (hideOffset < 0)
            {
                home = homeX >= globalX() && homeY >= globalY();
            }
            else if (hideOffset > 0)
            {
                home = homeX <= globalX() && homeY <= globalY();
            }

            if (home)
            {
                const int32_t homeYOffset = homeY - parent->globalY();
                moveTo(homeX - parent->globalX(), homeYOffset, -1);
                hideOffset = 0;
            }
        }
    }

    if (DrawsLive() && ownPort != nullptr)
    {
        DrawInFramePass(ownPort);
        return;
    }

    if (ownPort != nullptr)
    {
        ownPort->copyTo(framePane, 0, 0, transparent);
    }

    if (winState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < numChildren; i++)
        {
            childList[i]->display();
        }
    }
}

auto lObject::resize(int32_t width, int32_t height) -> void
{
    if (width > 0 && height > 0 && (width != winWidth || height != winHeight))
    {
        if (ownPort != nullptr)
        {
            ownPort->resize(width, height);
        }

        winWidth = width;
        winHeight = height;
        framePane->x1 = framePane->x0 - 1 + width;
        framePane->y1 = framePane->y0 - 1 + height;
    }
}

auto lObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    // The rectangle is in the port's bitmap coordinates (the pane's own origin is not added).
    _pane box = *ownPort->frame();
    box.x0 = left;
    box.y0 = top;
    box.x1 = right;
    box.y1 = bottom;
    VFX_pane_wipe(&box, color);
}

auto lObject::setBackground(char* fileName) -> int32_t
{
    if (backgroundPort != nullptr)
    {
        backgroundPort->destroy();
        delete backgroundPort;
        backgroundPort = nullptr;
    }

    backgroundPort = new lPort;

    if (backgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return backgroundPort->init(fileName);
}
