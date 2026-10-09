#include "stdafx.h"
#include "gui/MCGuiTitleWindow.h"
#include "camera/MCCamera.h"
#include "gui/MCGuiSystem.h"
#include "platform/MCInput.h"
#include "terrain/MCTerrain.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>
    /// Makes a plain frame part: an object drawn by <paramref name="paint"/> (or its background), added to
    /// <paramref name="parent"/> at (<paramref name="xPos"/>, <paramref name="yPos"/>).
    /// </summary>
    /// <returns>The part, or its init's error.</returns>
    std::expected<MCGuiOwned<MCGuiObject>, int32_t> MakePart(MCGuiObject* parent, int32_t width, int32_t height,
                                                             int32_t xPos, int32_t yPos, void (*paint)(MCGuiObject*))
    {
        auto part = MCMakeGui<MCGuiObject>();
        // Port: a part only shows its paint routine or background, so it draws itself each frame.
        part->SetDrawsLive();

        if (const int32_t result = part->Init(0, 0, width, height, nullptr); result != 0)
        {
            return std::unexpected(result);
        }

        if (paint != nullptr)
        {
            part->SetPaintRoutine(paint);
        }

        parent->AddChild(part.get());
        part->MoveTo(xPos, yPos);
        return part;
    }

    /// <summary>Makes the 8x8 resize handle (art packet 0x29), driven by <see cref="HandleResizeButtonEvent"/>.</summary>
    std::expected<MCGuiOwned<MCGuiObject>, int32_t> MakeResizeHandle()
    {
        auto handle = MCMakeGui<MCGuiObject>();
        handle->SetDrawsLive();

        if (const int32_t result = handle->Init(0, 0, 8, 8, nullptr); result != 0)
        {
            return std::unexpected(result);
        }

        handle->SetBackground(0x29);
        handle->Draw();
        return handle;
    }

    /// <summary>Snaps a size to the 40-pixel grid of a grid-aligned window, rounding to the nearest step.</summary>
    void SnapToGrid(int32_t& newWidth, int32_t& newHeight)
    {
        if (newWidth % 0x28 > 0x13)
        {
            newWidth += 0x28;
        }

        newWidth -= newWidth % 0x28;

        if (newWidth == 0)
        {
            newWidth = 0x28;
        }

        if (newHeight % 0x28 > 0x13)
        {
            newHeight += 0x28;
        }

        newHeight -= newHeight % 0x28;
    }

    /// <summary>Destroys a window's frame parts (in the original's order: the bar, the left bar, then the rest).</summary>
    void DestroyFrame(MCGuiWindowFrame& frame, bool handleBeforeRightBar)
    {
        frame.TitleBar.reset();
        frame.LeftBar.reset();

        if (handleBeforeRightBar)
        {
            frame.ResizeButton.reset();
            frame.RightBar.reset();
            frame.BottomBar.reset();
        }
        else
        {
            frame.RightBar.reset();
            frame.BottomBar.reset();
            frame.ResizeButton.reset();
        }
    }
}

// Frame paint routines.

auto CameraBottomBarPaint(MCGuiObject* object) -> void
{
    VfxPaneWipe(object->Port()->Frame(), 0x10);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, 1, 0x15);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, 1, 0x10);
}

auto CameraLeftBarPaint(MCGuiObject* object) -> void
{
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height(), 0x15);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height(), 0x10);
}

auto CameraRightBarPaint(MCGuiObject* object) -> void
{
    VfxPaneWipe(object->Port()->Frame(), 0x10);
}

auto BottomBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(0, 0, 0xe);
    object->SetBit(object->Width(), 0, 0xe);
    VfxLineDraw(object->Port()->Frame(), 1, 0, object->Width(), 0, 3);
    VfxLineDraw(object->Port()->Frame(), 0, 1, object->Width(), 1, 0xe);
}

auto LeftBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(object->Width(), object->Height(), 0xe);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height(), 0xe);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height() - 1, 3);
}

auto RightBarPaint(MCGuiObject* object) -> void
{
    object->SetBit(0, object->Height(), 0xe);
    VfxPaneWipe(object->Port()->Frame(), 8);
    VfxLineDraw(object->Port()->Frame(), 0, 0, 0, object->Height() - 2, 3);
    VfxLineDraw(object->Port()->Frame(), 1, 0, 1, object->Height() - 1, 0xe);
}

auto HandleResizeButtonEvent(MCGuiObject* object, MCGuiEvent* event) -> void
{
    if (object->Parent == nullptr)
    {
        return;
    }

    // The window is the handle's parent's parent (the handle sits in the right frame bar).
    MCGuiObject* window = object->Parent->Parent;

    if (window == nullptr)
    {
        return;
    }

    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        {
            MCInput::SetCapture();
            GuiSystem()->Grab(object);
            break;
        }
        case MCGuiEventType::LeftButtonUp:
        {
            GuiSystem()->Release();
            MCInput::ReleaseCapture();
            window->Draw();
            break;
        }
        case MCGuiEventType::MouseMove:
        {
            if (GuiSystem()->GrabbedObject() == object)
            {
                window->Resize(event->X - window->GlobalX(), event->Y - window->GlobalY());
                window->Draw();
            }
            break;
        }
    }
}

// MCGuiTitleWindow

MCGuiTitleWindow::~MCGuiTitleWindow()
{
    MCGuiTitleWindow::Destroy();
}

auto MCGuiTitleWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    if (const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name); result != 0)
    {
        return result;
    }

    SetTransparent(false);
    TitleBar = MCMakeGui<MCGuiTitleBar>();

    if (const int32_t result = TitleBar->Init(0, 0, width + 0xc, 0xd, name); result != 0)
    {
        return result;
    }

    AddChild(TitleBar.get());
    TitleBar->ShowCloseButton(true);
    TitleBar->MoveTo(-2, -0xd);
    TitleBar->SetZoomCallbacks();

    // The left bar's init result is not checked.
    auto left = MakePart(this, 2, height, -2, 0, LeftBarPaint);
    LeftBar = left ? std::move(*left) : nullptr;
    auto bottom = MakePart(this, width + 4, 2, -2, height, BottomBarPaint);

    if (!bottom)
    {
        return bottom.error();
    }

    BottomBar = std::move(*bottom);
    auto right = MakePart(this, 10, height + 2, width, 0, RightBarPaint);

    if (!right)
    {
        return right.error();
    }

    RightBar = std::move(*right);
    auto handle = MakeResizeHandle();

    if (!handle)
    {
        return handle.error();
    }

    ResizeButton = std::move(*handle);
    RightBar->AddChild(ResizeButton.get());
    ResizeButton->MoveTo(2, RightBar->Height() - ResizeButton->Height());
    ResizeButton->SetEventRoutine(HandleResizeButtonEvent);

    MoveTo(xPos, yPos + 0xd);
    return 0;
}

auto MCGuiTitleWindow::Destroy() -> void
{
    DestroyFrame(*this, true);
    MCGuiObject::Destroy();
}

auto MCGuiTitleWindow::Draw() -> void
{
    VfxPaneWipe(DisplayPort->Frame(), BackColor());

    if (!Dragging() || WinState == MCGuiWindowState::Iconized)
    {
        MCGuiObject::Draw();
        return;
    }

    // While dragged, only the frame is drawn (the right bar twice, as the original).
    TitleBar->Draw();
    LeftBar->Draw();
    RightBar->Draw();
    RightBar->Draw();
    BottomBar->Draw();

    if (ResizeButton != nullptr)
    {
        ResizeButton->Draw();
    }
}

auto MCGuiTitleWindow::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || !TitleBar->ResizeOK(newWidth))
    {
        return;
    }

    if (GridAligned)
    {
        SnapToGrid(newWidth, newHeight);
    }

    MCGuiObject::Resize(newWidth, newHeight);
    TitleBar->Resize(newWidth + 0xc, TitleBar->Height());
    LeftBar->Resize(LeftBar->Width(), newHeight);
    RightBar->Resize(RightBar->Width(), newHeight);
    RightBar->MoveTo(newWidth, 0);
    BottomBar->Resize(newWidth + 4, BottomBar->Height());
    BottomBar->MoveTo(-2, newHeight);
    ResizeButton->MoveTo(2, RightBar->Height() - ResizeButton->Height());
}

auto MCGuiTitleWindow::SetTitle(std::string_view newTitle) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetTitle(newTitle);
    }
}

// MCGuiEmptyTitleWindow

MCGuiEmptyTitleWindow::~MCGuiEmptyTitleWindow()
{
    MCGuiEmptyTitleWindow::Destroy();
}

auto MCGuiEmptyTitleWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    if (const int32_t result = MCGuiHolderObject::Init(xPos, yPos, width, height, name); result != 0)
    {
        return result;
    }

    TitleBar = MCMakeGui<MCGuiTitleBar>();

    if (const int32_t result = TitleBar->Init(0, 0, width + 4, 8, name); result != 0)
    {
        return result;
    }

    AddChild(TitleBar.get());
    TitleBar->SetDepth(1);
    TitleBar->ShowCloseButton(false);
    TitleBar->MoveTo(-2, -8);
    TitleBar->SetZoomCallbacks();

    // The left bar's init result is not checked.
    auto left = MakePart(this, 2, height, -2, 0, CameraLeftBarPaint);
    LeftBar = left ? std::move(*left) : nullptr;
    auto bottom = MakePart(this, width + 4, 2, -2, height, CameraBottomBarPaint);

    if (!bottom)
    {
        return bottom.error();
    }

    BottomBar = std::move(*bottom);
    auto right = MakePart(this, 2, height, width, 0, CameraRightBarPaint);

    if (!right)
    {
        return right.error();
    }

    RightBar = std::move(*right);
    auto handle = MakeResizeHandle();

    if (!handle)
    {
        return handle.error();
    }

    ResizeButton = std::move(*handle);
    // Original bug (OB-066): the handle is the window's own child here, so HandleResizeButtonEvent resizes the
    // window's parent (the screen window), not the window.
    AddChild(ResizeButton.get());
    ResizeButton->MoveTo(2 - ResizeButton->Width() + Width(), 2 - ResizeButton->Height() + Height());
    ResizeButton->SetEventRoutine(HandleResizeButtonEvent);
    ResizeButton->SetDepth(1);

    MoveTo(xPos, yPos + 8);
    return 0;
}

auto MCGuiEmptyTitleWindow::Destroy() -> void
{
    DestroyFrame(*this, false);
    MCGuiHolderObject::Destroy();
}

auto MCGuiEmptyTitleWindow::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth < 0 || newHeight < 0 || !TitleBar->ResizeOK(newWidth))
    {
        return;
    }

    if (GridAligned)
    {
        SnapToGrid(newWidth, newHeight);
    }

    MCGuiHolderObject::Resize(newWidth, newHeight);
    TitleBar->Resize(newWidth + 4, TitleBar->Height());
    LeftBar->Resize(LeftBar->Width(), newHeight);
    RightBar->Resize(RightBar->Width(), newHeight);
    RightBar->MoveTo(newWidth, 0);
    BottomBar->Resize(newWidth + 4, BottomBar->Height());
    BottomBar->MoveTo(-2, newHeight);
    ResizeButton->MoveTo(2 - ResizeButton->Width() + Width(), 2 - ResizeButton->Height() + Height());
}

auto MCGuiEmptyTitleWindow::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject* pane = Panes[0];

    if (pane != nullptr)
    {
        if (event->Type == MCGuiEventType::Close)
        {
            // The camera goes off and the window leaves the screen.
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->Deactivate();
            }

            ScreenWindow()->RemoveChild(this);
            return;
        }

        if (event->Type == MCGuiEventType::ZoomIn)
        {
            // Zoom. Port: the view goes between the closest and the furthest zoom, not while paused or asked; the camera
            // stays at full scale (the original flipped it between 100 and 1, only 1 while paused or asked, or when
            // only the 45-pixel art is loaded, and locked multiplayer to 1).
            MCCamera* camera = pane->GetCamera();

            if (camera != nullptr)
            {
                if (GamePaused == 0 && GameAsked == 0 && camera->View() != nullptr)
                {
                    camera->View()->ToggleZoom();
                }

                camera->ForceUpdate = 1;
                MCTerrain::ForceRedraw = 1;
            }
        }
        else if (event->Type == MCGuiEventType::ClearTarget)
        {
            if (pane->GetCamera() != nullptr)
            {
                pane->GetCamera()->ChangeTarget(nullptr, 0);
            }
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiEmptyTitleWindow::SetTitle(std::string_view newTitle) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetTitle(newTitle);
    }
}

auto MCGuiEmptyTitleWindow::SetBackColor(int32_t color) -> void
{
    if (TitleBar != nullptr)
    {
        TitleBar->SetBackColor(color);
    }
}
