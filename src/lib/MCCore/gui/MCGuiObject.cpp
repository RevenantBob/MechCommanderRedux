#include "stdafx.h"
#include "gui/MCGuiObject.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "platform/MCInput.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>
    /// The scissors of the objects drawing in the frame pass around the one displaying now (innermost last), with the
    /// window each is on: a child that draws itself is cut to its nearest such ancestor's.
    /// </summary>
    std::vector<std::pair<const MCWindow*, MCRect>> ViewClips;

    /// <summary>The object drawing in the frame pass right now (its view open), or null.</summary>
    MCGuiObject* DrawingLive = nullptr;

    /// <summary>A size snapped to the 40-pixel grid: to the nearer multiple (19 and less round down).</summary>
    int32_t SnapToGrid(int32_t size)
    {
        if (size % 40 > 19)
        {
            size += 40;
        }

        return size - size % 40;
    }
}

MCGuiObject::~MCGuiObject()
{
    DestroyObject();
}

auto MCGuiObject::Place(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    WinWidth = width;
    WinHeight = height;
    WinX = xPos;
    WinY = yPos;
    MaxWidth = width;
    MaxHeight = height;
    MaxX = xPos;
    MaxY = yPos;
    NormalWidth = width;
    NormalHeight = height;
    NormalX = xPos;
    NormalY = yPos;
    IconWidth = width;
    IconHeight = height;
    IconX = xPos;
    IconY = yPos;
    HideOffset = 0;
    HomeX = xPos;
    HomeY = yPos;
    WinState = MCGuiWindowState::Normal;
    ShowWindow = true;
    DragOn = false;
    BackgroundColor = 0xff;

    // The pane starts one pixel wider and taller than the object (the original's); the first move puts it right.
    FramePane = std::make_unique<MCPane>();
    FramePane->Window = ScreenPort()->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    FramePane->Y1 = yPos + height;
    Hidden = false;
    HideDirection = MCDirection::Down;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    // The original forgot its children without telling them.
    ChildList.clear();
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation.reset();
    Animating = false;
    IconAnimation.reset();
    ObjectType = -1;
}

auto MCGuiObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, [[maybe_unused]] const char* name)
    -> int32_t
{
    // Asked before the state is reset, as the original made the port first.
    const bool live = DrawsLive();
    Transparent = false;
    DropTargets.clear();
    Place(xPos, yPos, width, height);
    DisplayPort = std::make_unique<MCGuiPort>();
    return live ? DisplayPort->InitView(width, height) : DisplayPort->Init(width, height);
}

auto MCGuiObject::Destroy() -> void
{
    DestroyObject();
}

auto MCGuiObject::DestroyObject() -> void
{
    MCGuiSystem* gui = GuiSystem();

    if (gui != nullptr)
    {
        gui->RemoveTimers(this);
    }

    DisplayPort.reset();
    FramePane.reset();
    BackgroundPort.reset();
    IconAnimation.reset();
    WindowAnimation.reset();
    DropTargets.clear();
    Assert(ChildList.empty(), 0, " Number of Children NOT Zero ");

    if (Parent != nullptr)
    {
        Parent->RemoveChild(this);
    }

    Parent = nullptr;
    Animating = false;

    if (gui == nullptr)
    {
        return;
    }

    if (gui->GrabbedObject() == this)
    {
        gui->Release();
    }

    if (gui->TextObject() == this)
    {
        gui->ReleaseText();
    }

    if (gui->ModalObject() == this)
    {
        gui->ClearModal();
    }

    if (gui->CurrentObject() == this)
    {
        MCGuiObject* screen = ScreenWindow();
        const MCPoint cursor = MCInput::GetCursorPos();
        gui->SetCurrentObject(screen != nullptr && screen != this ? screen->FindObject(cursor.x, cursor.y) : nullptr);
    }
}

auto MCGuiObject::SetDisplayPort(MCGuiPort* newPort) -> void
{
    FramePane->Window = newPort->Bitmap();

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        ChildList[i]->SetDisplayPort(newPort);
    }
}

auto MCGuiObject::PointInside(int32_t xPos, int32_t yPos) -> bool
{
    return FramePane->X0 <= xPos && xPos <= FramePane->X1 && FramePane->Y0 <= yPos && yPos <= FramePane->Y1;
}

auto MCGuiObject::RectIntersect(int32_t left, int32_t top, int32_t right, int32_t bottom) -> bool
{
    return FramePane->X0 < right && left < FramePane->X1 && FramePane->Y0 < bottom && top < FramePane->Y1;
}

auto MCGuiObject::RectIntersect(tagRECT area) -> bool
{
    return RectIntersect(area.left, area.top, area.right, area.bottom);
}

auto MCGuiObject::SetPaintRoutine(std::function<void(MCGuiObject*)> routine) -> void
{
    PaintRoutine = std::move(routine);
}

auto MCGuiObject::SetEventRoutine(std::function<void(MCGuiObject*, MCGuiEvent*)> routine) -> void
{
    EventRoutine = std::move(routine);
}

auto MCGuiObject::Paint() -> void
{
    if (PaintRoutine)
    {
        PaintRoutine(this);
    }
}

auto MCGuiObject::FindObject(int32_t xPos, int32_t yPos) -> MCGuiObject*
{
    if (WinState != MCGuiWindowState::Iconized)
    {
        if (!ShowWindow)
        {
            return nullptr;
        }

        for (size_t i = ChildList.size(); i > 0; i--)
        {
            MCGuiObject* found = ChildList[i - 1]->FindObject(xPos, yPos);

            if (found != nullptr)
            {
                return found;
            }
        }
    }

    if (ShowWindow && FramePane != nullptr && PointInside(xPos, yPos))
    {
        return this;
    }

    return nullptr;
}

auto MCGuiObject::SetParent(MCGuiObject* newParent) -> void
{
    Parent = newParent;
}

auto MCGuiObject::SetDepth(int32_t newDepth) -> void
{
    MCGuiObject* owner = Parent;

    if (owner != nullptr)
    {
        owner->RemoveChild(this);
    }

    WinDepth = newDepth;

    if (owner != nullptr)
    {
        owner->AddChild(this);
    }
}

auto MCGuiObject::Depth() -> int32_t
{
    return WinDepth;
}

auto MCGuiObject::StartAnimation() -> void
{
    Animating = true;
}

auto MCGuiObject::StopAnimation() -> void
{
    Animating = false;
}

auto MCGuiObject::StartModal() -> void
{
    GuiSystem()->SetModalObject(this);
}

auto MCGuiObject::StopModal() -> void
{
    GuiSystem()->ClearModal();
}

auto MCGuiObject::SetBackColor(int32_t color) -> void
{
    BackgroundColor = color;
}

auto MCGuiObject::BackColor() -> int32_t
{
    return BackgroundColor;
}

auto MCGuiObject::DrawBox(uint8_t color, tagRECT area) -> void
{
    DrawBox(color, area.left, area.top, area.right, area.bottom);
}

auto MCGuiObject::DrawBox(uint8_t color, int32_t left, int32_t top, int32_t right, int32_t bottom) -> void
{
    if (left == -1)
    {
        left = 0;
    }

    if (top == -1)
    {
        top = 0;
    }

    if (right == -1)
    {
        right = Width() - 1;
    }

    if (bottom == -1)
    {
        bottom = Height() - 1;
    }

    MCPane* pane = DisplayPort->Frame();
    VfxLineDraw(pane, left, top, right, top, color);
    VfxLineDraw(pane, left, top, left, bottom, color);
    VfxLineDraw(pane, left, bottom, right, bottom, color);
    VfxLineDraw(pane, right, top, right, bottom, color);
}

auto MCGuiObject::DrawFramed(bool pushed, bool fill) -> void
{
    // Raised: light top and left, dark bottom and right; sunken the other way round.
    const int32_t innerTopLeft = pushed ? 4 : 0xc;
    const int32_t outerTopLeft = pushed ? 6 : 10;
    const int32_t innerBottomRight = pushed ? 0xc : 4;
    const int32_t outerBottomRight = pushed ? 10 : 6;
    MCPane* pane = DisplayPort->Frame();

    if (fill && BackgroundColor != 0xff)
    {
        VfxPaneWipe(pane, BackColor());
    }

    VfxLineDraw(pane, 0, Height() - 1, Width(), Height() - 1, 0x10);
    VfxLineDraw(pane, 0, 0, Width(), 0, 0x10);
    VfxLineDraw(pane, 0, 0, 0, Height() - 1, 0x10);
    VfxLineDraw(pane, Width() - 1, 0, Width() - 1, Height() - 1, 0x10);
    VfxLineDraw(pane, 1, 1, Width() - 2, 1, innerTopLeft);
    VfxLineDraw(pane, 2, 2, Width() - 3, 2, outerTopLeft);
    VfxLineDraw(pane, 1, 1, 1, Height() - 2, innerTopLeft);
    VfxLineDraw(pane, 2, 2, 2, Height() - 3, outerTopLeft);
    VfxLineDraw(pane, Width() - 2, 1, Width() - 2, Height() - 2, innerBottomRight);
    VfxLineDraw(pane, Width() - 3, 2, Width() - 3, Height() - 3, outerBottomRight);
    VfxLineDraw(pane, 1, Height() - 2, Width() - 2, Height() - 2, innerBottomRight);
    VfxLineDraw(pane, 2, Height() - 3, Width() - 3, Height() - 3, outerBottomRight);
}

auto MCGuiObject::FillBox(int16_t left, int16_t top, int16_t right, int16_t bottom, uint8_t color) -> void
{
    MCPane box = *DisplayPort->Frame();
    box.X0 = left;
    box.Y0 = top;
    box.X1 = right;
    box.Y1 = bottom;
    VfxPaneWipe(&box, color);
}

auto MCGuiObject::BringToFront(bool noShuffle) -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    Parent->BringToFront(noShuffle);
    std::vector<MCGuiObject*>& siblings = Parent->ChildList;

    if (siblings.size() <= 1)
    {
        return;
    }

    // The children are kept sorted by depth: the ones behind this object's depth stay, then this depth's others, with
    // this object last (in front), then the rest.
    const std::vector<MCGuiObject*> sorted = siblings;
    size_t next = 0;
    size_t placed = 0;

    while (next < sorted.size() && sorted[next]->Depth() < WinDepth)
    {
        siblings[placed++] = sorted[next++];
    }

    while (next < sorted.size() && sorted[next]->Depth() == WinDepth)
    {
        if (sorted[next] != this)
        {
            siblings[placed++] = sorted[next];
        }

        next++;
    }

    siblings[placed] = this;

    for (size_t slot = placed + 1; slot < siblings.size(); slot++)
    {
        siblings[slot] = sorted[next++];
    }

    if (GridAligned && !noShuffle)
    {
        for (int32_t i = Parent->NumberOfChildren() - 1; i >= 0; i--)
        {
            MCGuiObject* sibling = Parent->Child(i);

            if (sibling->IsShowing() && sibling != this && sibling->GridAligned && sibling->X() / 40 == X() / 40 &&
                sibling->Y() / 40 == Y() / 40)
            {
                sibling->GridAligned = false;
                const int32_t newY = 5 - sibling->Y() % 40 + sibling->Y();
                const int32_t newX = 5 - sibling->X() % 40 + sibling->X();
                sibling->MoveTo(newX, newY);
                sibling->GridAligned = true;
            }
        }
    }
}

auto MCGuiObject::NumberOfChildren() -> int32_t
{
    return static_cast<int32_t>(ChildList.size());
}

auto MCGuiObject::AddChild(MCGuiObject* child) -> void
{
    if (child == nullptr)
    {
        return;
    }

    Assert(child->Parent == nullptr || child->Parent == this, 0, " Adding child that's someone else's ");
    RemoveChild(child);
    child->SetParent(this);
    ChildList.push_back(child);
    child->BringToFront(true);
    child->MoveTo(child->X(), child->Y());
}

auto MCGuiObject::RemoveChild(MCGuiObject* child) -> void
{
    const auto found = std::ranges::find(ChildList, child);

    if (child == nullptr || found == ChildList.end())
    {
        return;
    }

    ChildList.erase(found);
    child->SetParent(nullptr);
}

auto MCGuiObject::Dragging() -> bool
{
    return DragOn;
}

auto MCGuiObject::StartDrag(int32_t xPos, int32_t yPos) -> void
{
    DragOn = true;
    DragX = xPos;
    DragY = yPos;
}

auto MCGuiObject::StopDrag() -> void
{
    DragOn = false;
}

auto MCGuiObject::DragStartX() -> int32_t
{
    return DragX;
}

auto MCGuiObject::DragStartY() -> int32_t
{
    return DragY;
}

auto MCGuiObject::ForemostChild(int32_t atDepth) -> MCGuiObject*
{
    for (size_t i = ChildList.size(); i > 0; i--)
    {
        if (ChildList[i - 1]->WinDepth == atDepth)
        {
            return ChildList[i - 1];
        }
    }

    return nullptr;
}

auto MCGuiObject::Child(int32_t index) -> MCGuiObject*
{
    if (index < 0 || static_cast<size_t>(index) >= ChildList.size())
    {
        return nullptr;
    }

    return ChildList[static_cast<size_t>(index)];
}

auto MCGuiObject::Width() -> int32_t
{
    return WinWidth;
}

auto MCGuiObject::Height() -> int32_t
{
    return WinHeight;
}

auto MCGuiObject::Port() -> MCGuiPort*
{
    return DisplayPort.get();
}

auto MCGuiObject::X() -> int32_t
{
    return WinX;
}

auto MCGuiObject::Y() -> int32_t
{
    return WinY;
}

auto MCGuiObject::GlobalX() -> int32_t
{
    int32_t result = WinX;

    for (MCGuiObject* owner = Parent; owner != nullptr; owner = owner->Parent)
    {
        result += owner->X();
    }

    return result;
}

auto MCGuiObject::GlobalY() -> int32_t
{
    int32_t result = WinY;

    for (MCGuiObject* owner = Parent; owner != nullptr; owner = owner->Parent)
    {
        result += owner->Y();
    }

    return result;
}

auto MCGuiObject::Frame() -> MCPane*
{
    return FramePane.get();
}

auto MCGuiObject::MoveTo(int32_t xPos, int32_t yPos, bool temporary) -> void
{
    WinY = yPos;
    WinX = xPos;
    const int32_t parentX = Parent != nullptr ? Parent->GlobalX() : 0;
    const int32_t parentY = Parent != nullptr ? Parent->GlobalY() : 0;
    FramePane->X0 = parentX + xPos;
    FramePane->Y0 = parentY + yPos;
    FramePane->X1 = WinWidth - 1 + FramePane->X0;
    FramePane->Y1 = WinHeight - 1 + FramePane->Y0;

    if (!temporary)
    {
        HomeX = xPos;
        HomeY = yPos;
    }

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        MCGuiObject* child = ChildList[i];
        child->MoveTo(child->X(), child->Y());
    }
}

auto MCGuiObject::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    if (newWidth <= 0 || newHeight <= 0 || (newWidth == WinWidth && newHeight == WinHeight))
    {
        return;
    }

    if (GridAligned)
    {
        newWidth = SnapToGrid(newWidth);

        if (newWidth == 0)
        {
            newWidth = 40;
        }

        newHeight = SnapToGrid(newHeight);
    }

    if (DisplayPort != nullptr)
    {
        DisplayPort->Resize(newWidth, newHeight);
    }

    WinWidth = newWidth;
    WinHeight = newHeight;
    FramePane->X1 = FramePane->X0 - 1 + newWidth;
    FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
}

auto MCGuiObject::Draw() -> void
{
    if (WinState == MCGuiWindowState::Iconized)
    {
        IconAnimation->Draw(DisplayPort->Frame(), 0, 0);
        return;
    }

    if (BackgroundPort != nullptr)
    {
        BackgroundPort->CopyTo(DisplayPort->Frame(), 0, 0, true);
    }

    if (WindowAnimation != nullptr && Animating)
    {
        WindowAnimation->Draw(DisplayPort->Frame(), 0, 0);
    }

    Paint();

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        DrawChild(ChildList[i]);
    }
}

auto MCGuiObject::DisplayChildren() -> void
{
    for (size_t i = 0; i < ChildList.size(); i++)
    {
        ChildList[i]->Display();
    }
}

auto MCGuiObject::Display() -> void
{
    if (!ShowWindow || (IsHidden() && HideOffset == 0))
    {
        return;
    }

    if (DrawsLive())
    {
        SlideStep();
        DrawInFramePass(DisplayPort.get());
        return;
    }

    if (WinState == MCGuiWindowState::Iconized)
    {
        if (IconAnimation != nullptr)
        {
            Draw();
        }
    }
    else if (WindowAnimation != nullptr)
    {
        WindowAnimation->Draw(DisplayPort->Frame(), 0, 0);
        Draw();
    }

    SlideStep();

    if (DisplayPort != nullptr)
    {
        DisplayPort->CopyTo(FramePane.get(), 0, 0, Transparent);
    }

    if (WinState != MCGuiWindowState::Iconized)
    {
        DisplayChildren();
    }
}

auto MCGuiObject::SetDrawsLive() -> void
{
    LiveDraw = true;

    if (DisplayPort != nullptr && !DisplayPort->IsView())
    {
        DisplayPort->InitView(Width(), Height());
    }
}

auto MCGuiObject::DrawsChild(MCGuiObject* child) -> bool
{
    return !child->DrawsLive() && DrawingLive != this;
}

auto MCGuiObject::DrawChild(MCGuiObject* child) -> void
{
    if (DrawsChild(child))
    {
        child->Draw();
    }
}

auto MCGuiObject::DrawInFramePass(MCGuiPort* port, int32_t scrollY, bool wipe, bool displayChildren) -> void
{
    // The view lies over the pane, on the window the pane is on (the screen, or a scroll pane's content), cut to the
    // window and to the scissor of the nearest clipping ancestor on the same window.
    MCWindow* target = FramePane->Window;
    MCRect scissor{std::max(FramePane->X0, 0), std::max(FramePane->Y0, 0), std::min(FramePane->X1, target->XMax),
                   std::min(FramePane->Y1, target->YMax)};

    if (!ViewClips.empty() && ViewClips.back().first == target)
    {
        const MCRect& outer = ViewClips.back().second;
        scissor.X0 = std::max(scissor.X0, outer.X0);
        scissor.Y0 = std::max(scissor.Y0, outer.Y0);
        scissor.X1 = std::min(scissor.X1, outer.X1);
        scissor.Y1 = std::min(scissor.Y1, outer.Y1);
    }

    if (scissor.X1 < scissor.X0 || scissor.Y1 < scissor.Y0)
    {
        // An empty scissor: the shut one draws nothing either way, and the children are cut away by it.
        scissor = MCRect{0, 0, -1, -1};
    }

    port->OpenView(target, FramePane->X0, FramePane->Y0 - scrollY, scissor, Transparent);
    MCGuiObject* const outerDrawing = DrawingLive;
    DrawingLive = this;

    // A picture that was never painted held zeros (a port's bitmap starts zeroed), and an opaque object copied them to
    // the screen; a transparent one let what was under it show.
    if (!Transparent && wipe)
    {
        VfxPaneWipe(port->Frame(), 0);
    }

    if (WinState != MCGuiWindowState::Iconized || IconAnimation != nullptr)
    {
        Draw();
    }

    DrawingLive = outerDrawing;
    port->CloseView();

    if (WinState != MCGuiWindowState::Iconized && displayChildren)
    {
        const bool clips = ClipsChildren();

        if (clips)
        {
            ViewClips.emplace_back(target, scissor);
        }

        DisplayChildren();

        if (clips)
        {
            ViewClips.pop_back();
        }
    }
}

auto MCGuiObject::SlideStep() -> void
{
    if (HideOffset == 0)
    {
        return;
    }

    // A slide (HideMe) moves the whole offset each frame until the object is off the screen, or back home.
    if (HideDirection == MCDirection::Left || HideDirection == MCDirection::Right)
    {
        MoveTo(X() + HideOffset, Y(), true);
    }
    else
    {
        MoveTo(X(), Y() + HideOffset, true);
    }

    if (Hidden)
    {
        const tagRECT screen = {0, 0, GuiSystem()->Width(), GuiSystem()->Height()};

        if (!RectIntersect(screen))
        {
            HideOffset = 0;
        }

        return;
    }

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
        MoveTo(HomeX - Parent->GlobalX(), homeYOffset, true);
        HideOffset = 0;
    }
}

auto MCGuiObject::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiSystem* gui = GuiSystem();

    if (event->Type == MCGuiEventType::LeftButtonDown)
    {
        if (Parent != nullptr)
        {
            BringToFront(false);
            ARedrawScreen();
        }
    }
    else if (event->Type == MCGuiEventType::ScreenResized)
    {
        for (size_t i = 0; i < ChildList.size(); i++)
        {
            ChildList[i]->HandleEvent(event);
        }
    }

    if (WinState == MCGuiWindowState::Iconized)
    {
        // An icon is dragged by the mouse, and opened by a double click.
        switch (event->Type)
        {
            case MCGuiEventType::LeftButtonDown:
            {
                gui->Grab(this);
                LastX = event->X;
                LastY = event->Y;
                break;
            }
            case MCGuiEventType::LeftButtonUp:
            {
                if (gui->GrabbedObject() == this)
                {
                    gui->Release();
                    MCGuiObject* target = ScreenWindow()->FindObject(event->X, event->Y);

                    if (target != nullptr)
                    {
                        target->Enter();
                    }
                }
                break;
            }
            case MCGuiEventType::MouseMove:
            {
                if (gui->GrabbedObject() == this)
                {
                    const int32_t eventY = event->Y;
                    MCGuiObject* target = ScreenWindow()->FindObject(event->X, eventY);

                    if (target != nullptr && target->Depth() == 100)
                    {
                        return;
                    }

                    const int32_t newY = Y() + (eventY - LastY);
                    MoveTo(X() + (event->X - LastX), newY);
                    LastX = event->X;
                    LastY = eventY;
                }
                break;
            }
            case MCGuiEventType::LeftDoubleClick:
            {
                if (gui->GrabbedObject() == this)
                {
                    gui->Release();
                }

                Normalize();
                break;
            }
        }
    }
    else if (event->Type == MCGuiEventType::Close)
    {
        Destroy();
    }

    if (EventRoutine)
    {
        EventRoutine(this, event);
    }
}

auto MCGuiObject::SetBackground(std::string_view fileName) -> int32_t
{
    BackgroundPort = std::make_unique<MCGuiPort>();
    return BackgroundPort->Init(fileName);
}

auto MCGuiObject::SetBackground(int32_t artPacket) -> int32_t
{
    BackgroundPort = std::make_unique<MCGuiPort>();
    return BackgroundPort->Init(artPacket);
}

auto MCGuiObject::Maximize() -> void
{
    SetState(std::to_underlying(MCGuiWindowState::Maximized));
}

auto MCGuiObject::Normalize() -> void
{
    SetState(std::to_underlying(MCGuiWindowState::Normal));
}

auto MCGuiObject::Iconize() -> void
{
    SetState(std::to_underlying(MCGuiWindowState::Iconized));
}

auto MCGuiObject::State() -> MCGuiWindowState
{
    return WinState;
}

auto MCGuiObject::SetState(int32_t state) -> void
{
    const auto newState = static_cast<MCGuiWindowState>(state);

    // Saves the placement of the state left, then takes the new state's. From the iconized state the new state is
    // stored first, so a state the switch doesn't list sticks (with the icon's placement).
    const auto take = [this](MCGuiWindowState taken, int32_t width, int32_t height, int32_t xPos, int32_t yPos)
    {
        WinX = xPos;
        WinState = taken;
        WinY = yPos;
        WinWidth = width;
        WinHeight = height;
    };

    switch (WinState)
    {
        case MCGuiWindowState::Normal:
        {
            NormalY = WinY;
            NormalX = WinX;
            NormalWidth = WinWidth;
            NormalHeight = WinHeight;

            if (newState == MCGuiWindowState::Maximized)
            {
                take(newState, MaxWidth, MaxHeight, MaxX, MaxY);
            }
            else if (newState == MCGuiWindowState::Iconized)
            {
                take(newState, IconWidth, IconHeight, IconX, IconY);
            }
            break;
        }
        case MCGuiWindowState::Maximized:
        {
            MaxY = WinY;
            MaxX = WinX;
            MaxWidth = WinWidth;
            MaxHeight = WinHeight;

            if (newState == MCGuiWindowState::Normal)
            {
                take(newState, NormalWidth, NormalHeight, NormalX, NormalY);
            }
            else if (newState == MCGuiWindowState::Iconized)
            {
                take(newState, IconWidth, IconHeight, IconX, IconY);
            }
            break;
        }
        case MCGuiWindowState::Iconized:
        {
            IconX = WinX;
            IconY = WinY;
            WinState = newState;
            IconWidth = WinWidth;
            IconHeight = WinHeight;

            if (newState == MCGuiWindowState::Normal)
            {
                take(newState, NormalWidth, NormalHeight, NormalX, NormalY);
            }
            else if (newState == MCGuiWindowState::Maximized)
            {
                take(newState, MaxWidth, MaxHeight, MaxX, MaxY);
            }
            break;
        }
    }

    Resize(WinWidth, WinHeight);
    MoveTo(WinX, WinY);
    ARedrawScreen();
}

auto MCGuiObject::SetAnimation(std::string_view fileName) -> int32_t
{
    WindowAnimation = std::make_unique<MCGuiAnimation>();
    return WindowAnimation->Load(fileName);
}

auto MCGuiObject::SetIcon(std::string_view fileName) -> int32_t
{
    IconAnimation = std::make_unique<MCGuiAnimation>();
    const int32_t result = IconAnimation->Load(fileName);

    if (result == 0)
    {
        IconWidth = IconAnimation->Width();
        IconHeight = IconAnimation->Height();
    }

    return result;
}

auto MCGuiObject::Animation() -> MCGuiAnimation*
{
    return WindowAnimation.get();
}

auto MCGuiObject::Icon() -> MCGuiAnimation*
{
    return IconAnimation.get();
}

auto MCGuiObject::Background() -> MCGuiPort*
{
    return BackgroundPort.get();
}

auto MCGuiObject::SetBit(int32_t xPos, int32_t yPos, uint8_t color) -> void
{
    if (xPos < 0 || xPos >= Width() || yPos < 0 || yPos >= Height())
    {
        return;
    }

    if (DisplayPort->Buffer() != nullptr)
    {
        DisplayPort->Buffer()[Width() * yPos + xPos] = color;
    }
    else if (DisplayPort->IsView())
    {
        // A view has no pixels; the pixel is drawn through it.
        VfxPixelWrite(DisplayPort->Frame(), xPos, yPos, color);
    }
}

auto MCGuiObject::HideMe(bool hide) -> void
{
    if (Hidden == hide || HideOffset != 0)
    {
        return;
    }

    if (hide)
    {
        HomeX = GlobalX();
        HomeY = GlobalY();
        Hidden = true;

        switch (HideDirection)
        {
            case MCDirection::Left:
                HideOffset = -(GlobalX() + Width());
                break;
            case MCDirection::Up:
                HideOffset = -(GlobalY() + Height());
                break;
            case MCDirection::Right:
                HideOffset = GuiSystem()->Width() - GlobalX();
                break;
            case MCDirection::Down:
                HideOffset = GuiSystem()->Height() - GlobalY();
                break;
        }

        return;
    }

    Hidden = false;
    HideOffset = HomeX != GlobalX() ? HomeX - GlobalX() : HomeY - GlobalY();
}
