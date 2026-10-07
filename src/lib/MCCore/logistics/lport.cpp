#include "stdafx.h"
#include "logistics/lport.h"
#include "gui/aanim.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "platform/MCInput.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Allocates in a logistics block.</summary>
    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    /// <summary>Frees a logistics block.</summary>
    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    /// <summary>The art <see cref="LogArt"/> loaded, by file name (null for a file that couldn't be read).</summary>
    std::unordered_map<std::string, MCLogPort*> LoadedArt;
}

auto LogArt(const char* fileName) -> MCLogPort*
{
    const auto found = LoadedArt.find(fileName);

    if (found != LoadedArt.end())
    {
        return found->second;
    }

    auto* art = new MCLogPort;

    if (art->Init(const_cast<char*>(fileName)) != 0)
    {
        delete art;
        art = nullptr;
    }

    LoadedArt.emplace(fileName, art);
    return art;
}

auto LogArtf(const char* format, ...) -> MCLogPort*
{
    char fileName[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(fileName, sizeof(fileName), format, args);
    va_end(args);
    return LogArt(fileName);
}

auto ClearLogArt() -> void
{
    for (auto& [name, art] : LoadedArt)
    {
        delete art;
    }

    LoadedArt.clear();
}

// lPort

auto MCLogPort::Init(int32_t width, int32_t height, int allocBitmap) -> int32_t
{
    if (PortWindow != nullptr)
    {
        MCRenderer::DestroyTexture(PortWindow);

        if (PortWindow->Buffer != nullptr)
        {
            LogFree(PortWindow->Buffer);
        }

        LogFree(PortWindow);
    }

    auto* window = static_cast<MCWindow*>(LogAlloc(sizeof(MCWindow)));
    PortWindow = window;

    if (window == nullptr)
    {
        return 3;
    }

    window->View = nullptr;
    window->Texture = nullptr;
    window->XMax = width - 1;
    window->YMax = height - 1;
    // Port fix: the original left the buffer uninitialised without a bitmap (and destroy then freed it).
    window->Buffer = nullptr;

    if (allocBitmap != 0 && this != ScreenPort)
    {
        window->Buffer = static_cast<uint8_t*>(LogAlloc(static_cast<uint32_t>(width * height)));

        if (window->Buffer == nullptr)
        {
            return 3;
        }
    }

    if (PortPane != nullptr)
    {
        LogFree(PortPane);
    }

    auto* pane = static_cast<MCPane*>(LogAlloc(sizeof(MCPane)));
    PortPane = pane;

    if (pane == nullptr)
    {
        return 3;
    }

    pane->Window = window;
    pane->X0 = 0;
    pane->Y0 = 0;
    PortWidth = width;
    pane->X1 = width - 1;
    PortHeight = height;
    pane->Y1 = height - 1;

    if (window->Buffer != nullptr)
    {
        MCRenderer::CreateTexture(window, MCTextureUse::Dynamic);
    }

    return 0;
}

auto MCLogPort::Init(char* fileName) -> int32_t
{
    char message[256];
    char path[256];
    MCFile file;
    std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

    if (file.Open(path) != 0)
    {
        std::snprintf(path, sizeof(path), "%s", fileName);

        if (file.Open(path) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
            GeneralMsg(message);
            return -2;
        }
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", fileName);
        GeneralMsg(message);
        return -2;
    }

    auto* data = static_cast<uint8_t*>(LogAlloc(size));

    if (data == nullptr)
    {
        return 3;
    }

    file.Read(data, static_cast<int32_t>(size));
    file.Close();
    // A TGA header: the 16-bit width and height at +0x0c/+0x0e; the 8-bit pixels follow the 18-byte header and a
    // 256-entry palette.
    int16_t tgaWidth = 0;
    int16_t tgaHeight = 0;
    std::memcpy(&tgaWidth, data + 0x0c, sizeof(tgaWidth));
    std::memcpy(&tgaHeight, data + 0x0e, sizeof(tgaHeight));
    const int32_t result = Init(tgaWidth, tgaHeight, -1);

    if (result != 0)
    {
        return result; // The original leaks the file data here.
    }

    MCTexture* texture = PortPane->Window->Texture;
    std::memcpy(MCRenderer::LockTexture(texture), data + 0x312, static_cast<size_t>(tgaHeight * tgaWidth));
    MCRenderer::UnlockTexture(texture);
    LogFree(data);
    return 0;
}

auto MCLogPort::InitView(int32_t width, int32_t height) -> int32_t
{
    if (IsView() && width == PortWidth && height == PortHeight)
    {
        return 0;
    }

    Destroy();
    auto* window = static_cast<MCWindow*>(LogAlloc(sizeof(MCWindow)));
    auto* pane = static_cast<MCPane*>(LogAlloc(sizeof(MCPane)));

    if (window == nullptr || pane == nullptr)
    {
        return 3;
    }

    PortWindow = window;
    PortPane = pane;
    window->Buffer = nullptr;
    window->Texture = nullptr;
    window->View = &View;
    window->XMax = width - 1;
    window->YMax = height - 1;
    View = MCView{};
    pane->Window = window;
    pane->X0 = 0;
    pane->Y0 = 0;
    pane->X1 = width - 1;
    pane->Y1 = height - 1;
    PortWidth = width;
    PortHeight = height;
    return 0;
}

auto MCLogPort::Resize(int32_t width, int32_t height) -> int32_t
{
    // Port: a view has no pixels to reallocate.
    if (this != ScreenPort && !IsView())
    {
        if (PortWindow->Buffer != nullptr)
        {
            LogFree(PortWindow->Buffer);
        }

        PortWindow->Buffer = static_cast<uint8_t*>(LogAlloc(static_cast<uint32_t>(width * height)));
    }

    PortHeight = height;
    PortPane->Window = PortWindow;
    PortPane->X1 = width - 1;
    PortPane->Y1 = height - 1;
    PortWindow->XMax = width - 1;
    PortWindow->YMax = height - 1;
    PortWidth = width;
    MCRenderer::ResizeTexture(PortWindow);
    return 0;
}

auto MCLogPort::Destroy() -> void
{
    if (PortWindow != nullptr)
    {
        MCRenderer::DestroyTexture(PortWindow);

        if (PortWindow->Buffer != nullptr)
        {
            LogFree(PortWindow->Buffer);
        }

        LogFree(PortWindow);
        PortWindow = nullptr;
    }

    if (PortPane != nullptr)
    {
        LogFree(PortPane);
        PortPane = nullptr;
    }
}

// lObject

MCLogObject::~MCLogObject()
{
    MCLogObject::Destroy();
}

auto MCLogObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name, MCLogPort* port)
    -> int32_t
{
    (void)name;
    WinX = xPos;
    MaxX = xPos;
    NormalX = xPos;
    IconX = xPos;
    HomeX = xPos;
    WinWidth = width;
    _OwnPort = nullptr;
    _SharedPort = nullptr;
    WinHeight = height;
    WinY = yPos;
    MaxWidth = width;
    MaxHeight = height;
    MaxY = yPos;
    NormalWidth = width;
    NormalHeight = height;
    NormalY = yPos;
    IconWidth = width;
    IconHeight = height;
    IconY = yPos;
    HideOffset = 0;
    HomeY = yPos;
    WinState = aSTATE_NORMAL;
    ShowWindow = -1;
    DragOn = 0;
    Transparent = 0;
    BackgroundColor = 0xff;

    if (port == nullptr)
    {
        _OwnPort = new MCLogPort;
        const int32_t result = DrawsLive() ? _OwnPort->InitView(width, height) : _OwnPort->Init(width, height, -1);

        if (result != 0)
        {
            return result;
        }
    }
    else
    {
        _SharedPort = port;
    }

    if (FramePane != nullptr)
    {
        // Port fix: the original freed it with the CRT's delete although it came in a logistics block.
        LogFree(FramePane);
        FramePane = nullptr;
    }

    FramePane = static_cast<MCPane*>(LogAlloc(sizeof(MCPane)));

    if (FramePane == nullptr)
    {
        return 3;
    }

    FramePane->Window = ScreenPort->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    Hidden = 0;
    FramePane->Y1 = yPos + height;
    HideDirection = DIRECTION_DOWN;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    NumChildren = 0;
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation = nullptr;
    Animating = 0;
    IconAnimation = nullptr;
    MCGuiObject::BackgroundPort = nullptr;
    _BackgroundPort = nullptr;
    ObjectType = -1;
    return 0;
}

auto MCLogObject::Destroy() -> void
{
    Application->RemoveTimers(this);

    if (_OwnPort != nullptr)
    {
        _OwnPort->Destroy();
        delete _OwnPort;
        _OwnPort = nullptr;
    }

    if (FramePane != nullptr)
    {
        LogFree(FramePane);
        FramePane = nullptr;
    }

    if (MCGuiObject::BackgroundPort != nullptr)
    {
        MCGuiObject::BackgroundPort->Destroy();
        delete MCGuiObject::BackgroundPort;
        MCGuiObject::BackgroundPort = nullptr;
    }

    if (_BackgroundPort != nullptr)
    {
        _BackgroundPort->Destroy();
        delete _BackgroundPort;
        _BackgroundPort = nullptr;
    }

    if (IconAnimation != nullptr)
    {
        IconAnimation->Destroy();
        delete IconAnimation;
        IconAnimation = nullptr;
    }

    if (WindowAnimation != nullptr)
    {
        WindowAnimation->Destroy();
        delete WindowAnimation;
        WindowAnimation = nullptr;
    }

    if (NumChildren > 0)
    {
        // Original behaviour (OB-071): removes the first child, then deletes whichever child is first after that
        // (its destroy removes it). The first child is therefore only unlinked, and each later pass "removes" the
        // child just deleted (no longer listed, so nothing happens).
        MCGuiObject* child = ChildList[0];

        do
        {
            RemoveChild(child);
            child = ChildList[0];

            if (child != nullptr)
            {
                delete child;
            }
        } while (NumChildren > 0);
    }

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    Parent = nullptr;
    Animating = 0;

    if (Application->GrabbedObject() == this)
    {
        Application->Release();
    }

    if (Application->TextObject() == this)
    {
        Application->ReleaseText();
    }

    if (Application->ModalObject() == this)
    {
        Application->ClearModal();
    }

    if (Application->CurrentObject() == this)
    {
        const MCPoint cursor = MCInput::GetCursorPos();
        Application->SetCurrentObject(ScreenWindow->FindObject(cursor.x, cursor.y));
    }
}

auto MCLogObject::Lport() -> MCLogPort*
{
    return _OwnPort;
}

auto MCLogObject::Draw() -> void
{
    const int32_t state = WinState;

    if (state == aSTATE_ICONIZED)
    {
        IconAnimation->Draw(_OwnPort->Frame(), 0, 0);
    }
    else
    {
        if (_BackgroundPort != nullptr)
        {
            _BackgroundPort->CopyTo(_OwnPort->Frame(), 0, 0, -1);
        }

        if (WindowAnimation != nullptr && Animating != 0)
        {
            WindowAnimation->Draw(_OwnPort->Frame(), 0, 0);
        }
    }

    if (state != aSTATE_ICONIZED)
    {
        Paint();

        for (int32_t i = 0; i < NumChildren; i++)
        {
            DrawChild(ChildList[i]);
        }
    }
}

auto MCLogObject::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // An object that draws itself does so after the slide has moved it.
    if (!DrawsLive())
    {
        if (WinState == aSTATE_ICONIZED)
        {
            if (IconAnimation != nullptr)
            {
                Draw();
            }
        }
        else if (WindowAnimation != nullptr)
        {
            WindowAnimation->Draw(_OwnPort->Frame(), 0, 0);
            Draw();
        }
    }

    if (HideOffset != 0)
    {
        // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
        if (HideDirection == DIRECTION_LEFT || HideDirection == DIRECTION_RIGHT)
        {
            MoveTo(X() + HideOffset, Y(), -1);
        }
        else
        {
            MoveTo(X(), Y() + HideOffset, -1);
        }

        if (Hidden != 0)
        {
            if (RectIntersect(0, 0, Application->Width(), Application->Height()) == 0)
            {
                HideOffset = 0;
            }
        }
        else
        {
            bool home = false;

            if (HideOffset < 0)
            {
                home = HomeX >= GlobalX() && HomeY >= GlobalY();
            }
            else if (HideOffset > 0)
            {
                home = HomeX <= GlobalX() && HomeY <= GlobalY();
            }

            if (home)
            {
                const int32_t homeYOffset = HomeY - Parent->GlobalY();
                MoveTo(HomeX - Parent->GlobalX(), homeYOffset, -1);
                HideOffset = 0;
            }
        }
    }

    if (DrawsLive() && _OwnPort != nullptr)
    {
        DrawInFramePass(_OwnPort);
        return;
    }

    if (_OwnPort != nullptr)
    {
        _OwnPort->CopyTo(FramePane, 0, 0, Transparent);
    }

    if (WinState != aSTATE_ICONIZED)
    {
        for (int32_t i = 0; i < NumChildren; i++)
        {
            ChildList[i]->Display();
        }
    }
}

auto MCLogObject::Resize(int32_t width, int32_t height) -> void
{
    if (width > 0 && height > 0 && (width != WinWidth || height != WinHeight))
    {
        if (_OwnPort != nullptr)
        {
            _OwnPort->Resize(width, height);
        }

        WinWidth = width;
        WinHeight = height;
        FramePane->X1 = FramePane->X0 - 1 + width;
        FramePane->Y1 = FramePane->Y0 - 1 + height;
    }
}

auto MCLogObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    // The rectangle is in the port's bitmap coordinates (the pane's own origin is not added).
    MCPane box = *_OwnPort->Frame();
    box.X0 = left;
    box.Y0 = top;
    box.X1 = right;
    box.Y1 = bottom;
    VfxPaneWipe(&box, color);
}

auto MCLogObject::SetBackground(char* fileName) -> int32_t
{
    if (_BackgroundPort != nullptr)
    {
        _BackgroundPort->Destroy();
        delete _BackgroundPort;
        _BackgroundPort = nullptr;
    }

    _BackgroundPort = new MCLogPort;

    if (_BackgroundPort == nullptr)
    {
        Fatal(0, "Not enough memory to create background port");
    }

    return _BackgroundPort->Init(fileName);
}
