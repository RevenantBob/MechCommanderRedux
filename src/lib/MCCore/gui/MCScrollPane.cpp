#include "stdafx.h"
#include "gui/MCScrollPane.h"
#include "gui/MCGuiFont.h"
#include "lib/MCFatal.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The arrows' repeat timer (200 ms).</summary>
    constexpr int32_t ArrowTimer = 6;
}

MCScrollPane::~MCScrollPane()
{
    MCScrollPane::Destroy();
}

auto MCScrollPane::Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, const char* name) -> int32_t
{
    if (name == nullptr)
    {
        Init(width, height, xPos, yPos, static_cast<MCLogPort*>(nullptr));
        return 0;
    }

    MCLogPort background;
    background.Init(name);
    Init(width, height, xPos, yPos, &background);
    return 0;
}

auto MCScrollPane::Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, MCLogPort* background) -> void
{
    _OwnPort = nullptr;
    FramePane = nullptr;

    OwnedContent = std::make_unique<MCLogPort>();
    ContentPort = OwnedContent.get();
    ContentPort->Init(width - SliderWidth, height, -1);
    VfxPaneWipe(ContentPort->Frame(), 0x10);

    if (background != nullptr)
    {
        BackgroundCopy = std::make_unique<MCLogPort>();
        BackgroundCopy->Init(background->Width(), background->Height(), -1);
        background->CopyTo(BackgroundCopy->Frame(), 0, 0, true);
    }

    const int32_t result = MCLogObject::Init(xPos, yPos, width, height, nullptr, ContentPort);
    Assert(result == 0, 0, " could not initialize ScrollPane ");
    _OwnPort = ContentPort;

    SliderPort = std::make_unique<MCLogPort>();
    SliderPort->Init(SliderWidth, height, -1);

    // The track tile repeats down the column, below the first row; the arrows go at the ends.
    MCLogPort art;
    art.Init(std::format("{}logart\\scroll.tga", ArtPath).c_str());
    const int32_t numTiles = height / art.Height() - 1;

    for (int32_t i = 0; i < numTiles; i++)
    {
        art.CopyTo(SliderPort->Frame(), 0, art.Height() * i + 1, true);
    }

    art.Init(std::format("{}logart\\supbup.tga", ArtPath).c_str());
    art.CopyTo(SliderPort->Frame(), 0, 0, true);
    art.Destroy();
    art.Init(std::format("{}logart\\sdnbup.tga", ArtPath).c_str());
    art.CopyTo(SliderPort->Frame(), 0, height - 15, true);

    if (PanePort == nullptr)
    {
        PanePort = std::make_unique<MCLogPort>();
    }

    PanePort->InitView(width, height);
    SetUpSlider();
    SetScrollPos(0.0f);
    ShowGuiWindow(false);
    SetDepth(100);
}

auto MCScrollPane::Destroy() -> void
{
    _OwnPort = nullptr;
    MCLogObject::Destroy();
    ContentPort = nullptr;
    OwnedContent.reset();
    SliderPort.reset();
    FreeSlider();
    BackgroundCopy.reset();
    PanePort.reset();
}

auto MCScrollPane::SetScrollPos(float position) -> void
{
    ScrollPos = position;

    if (MaxScroll < position)
    {
        ScrollPos = MaxScroll;
    }

    if (ScrollPos < 0.0f)
    {
        ScrollPos = 0.0f;
    }

    if (SliderHeight != 0)
    {
        const auto newSliderPos =
            static_cast<int32_t>((WinHeight - 2 * ArrowHeight) * static_cast<double>(0.01f) * ScrollPos + ArrowHeight);
        SetChildren();
        SliderPos = newSliderPos;
    }
}

auto MCScrollPane::SetSliderPos(int32_t position) -> void
{
    if (SliderHeight == 0)
    {
        return;
    }

    position = std::max(std::min(position, SliderMax), ArrowHeight);
    SliderPos = position;
    const auto newScrollPos = static_cast<float>(static_cast<double>(position - ArrowHeight) /
                                                 static_cast<double>(WinHeight - 2 * ArrowHeight) * 100.0);
    ScrollPos = std::min(newScrollPos, MaxScroll);
    SetChildren();
}

auto MCScrollPane::SetChildren() -> void
{
    for (int32_t i = 0; i < NumberOfChildren(); i++)
    {
        MCGuiObject* child = Child(i);
        child->MoveTo(child->X(), LastScrollOffset - GetScrollOffset() + child->Y());
    }

    LastScrollOffset = GetScrollOffset();
}

auto MCScrollPane::Draw() -> void
{
    MCPane* view = PanePort->Frame();

    if (BackgroundCopy != nullptr)
    {
        BackgroundCopy->CopyTo(view, 0, 0, true);
    }

    if (ContentPort != nullptr)
    {
        const auto offset = static_cast<int32_t>(-(static_cast<double>(ScrollPos) * ScrollUnit));

        if (ContentPort->IsView())
        {
            const MCView& pane = PanePort->View;
            ContentPort->OpenView(pane.Target, pane.OriginX, pane.OriginY + offset, pane.Scissor, true);
            DrawContent();
            ContentPort->CloseView();
        }
        else
        {
            ContentPort->CopyTo(view, 0, offset, true);
        }
    }

    DrawSliderColumn(view, WinWidth - SliderWidth, 0, true);
}

auto MCScrollPane::DrawContent() -> void
{
    if (ContentPort->DrawContent)
    {
        ContentPort->DrawContent(ContentPort);
    }
}

auto MCScrollPane::DrawContentTo(MCPane* target, int32_t xPos, int32_t yPos) -> void
{
    if (ContentPort == nullptr)
    {
        return;
    }

    if (!ContentPort->IsView())
    {
        VfxPaneCopy(ContentPort->Frame(), 0, GetScrollOffset(), target, xPos, yPos, -1);
        return;
    }

    const auto offset = static_cast<int32_t>(-(static_cast<double>(ScrollPos) * ScrollUnit));
    const MCRect scissor{target->X0 + xPos, target->Y0 + yPos, target->X1, target->Y1};
    ContentPort->OpenView(target->Window, target->X0 + xPos, target->Y0 + yPos + offset, scissor, false);
    DrawContent();
    ContentPort->CloseView();
}

auto MCScrollPane::Display() -> void
{
    if (!IsShowing())
    {
        return;
    }

    // The original copied its pieces over what the parent had shown there, and showed no children.
    DrawInFramePass(PanePort.get(), 0, false, false);
}

auto MCScrollPane::HeldArrow() const -> Arrow
{
    // An arrow shows held only while the content scrolls (the original put its art in the column when it erased
    // the slider, and there is none to erase when the content fits).
    if (SliderHeight == 0 || GuiSystem()->GrabbedObject() != this)
    {
        return Arrow::None;
    }

    return _ArrowPressed;
}

auto MCScrollPane::PressedArrowArt(bool down) -> MCLogPort*
{
    return LogArtf("%slogart\\%s", ArtPath, down ? "lscsb04.tga" : "lscsb03.tga");
}

auto MCScrollPane::DrawSliderColumn(MCPane* target, int32_t xPos, int32_t yPos, bool keyed) -> void
{
    const int key = keyed ? -1 : 0;
    SliderPort->CopyTo(target, xPos, yPos, key);
    const Arrow held = HeldArrow();

    if (held == Arrow::Up)
    {
        if (MCLogPort* art = PressedArrowArt(false); art != nullptr)
        {
            art->CopyTo(target, xPos, yPos, key);
        }
    }
    else if (held == Arrow::Down)
    {
        if (MCLogPort* art = PressedArrowArt(true); art != nullptr)
        {
            art->CopyTo(target, xPos, yPos + Height() - art->Height(), key);
        }
    }

    if (SliderHeight > 0 && SliderTexture != nullptr)
    {
        // The slider image is a 13-wide picture of whole rows.
        MCWindow image{};
        image.Buffer = SliderImage.data();
        image.XMax = SliderWidth - 1;
        image.YMax = SliderTexture->Height - 1;
        image.Texture = SliderTexture;
        MCPane imagePane{&image, 0, 0, SliderWidth - 1, SliderTexture->Height - 1};
        VfxPaneCopy(&imagePane, 0, 0, target, xPos, yPos + SliderPos, -1);
    }
}

auto MCScrollPane::MakeSliderTexture() -> void
{
    MCRenderer::DestroyTexture(SliderTexture);
    SliderTexture = MCRenderer::CreateTexture(SliderImage.data(), SliderWidth, SliderHeight, MCTextureUse::Static);
}

auto MCScrollPane::FreeSlider() -> void
{
    MCRenderer::DestroyTexture(SliderTexture);
    SliderImage = {};
}

auto MCScrollPane::SetUpSlider() -> void
{
    const int32_t paneHeight = WinHeight;

    if (ContentPort->Height() <= paneHeight)
    {
        SliderHeight = 0;
        return;
    }

    // The slider's share of the track (the column less its two arrows) is the pane's share of the content.
    const auto paneHeightF = static_cast<float>(paneHeight);
    SliderHeight = std::max(
        static_cast<int32_t>(static_cast<double>(paneHeightF) / ContentPort->Height() * (paneHeight - 2 * ArrowHeight)),
        3);
    SliderPos = ArrowHeight;

    // Every row: dark edges, a light left bevel, a mid fill and a shadowed right bevel; the top row has a highlight
    // and the bottom row a shadow.
    const uint8_t edge = SliderEdgeColor();
    const std::array<uint8_t, SliderWidth> sliderRow = {edge, 0x10, 0x1c, 0x1a, 0x1a, 0x1a, 0x1a,
                                                        0x1a, 0x1a, 0x1a, 0x17, 0x10, edge};
    FreeSlider();
    SliderImage.resize(static_cast<size_t>(SliderHeight * SliderWidth));

    for (int32_t row = 0; row < SliderHeight; row++)
    {
        std::ranges::copy(sliderRow, SliderImage.begin() + row * SliderWidth);
    }

    std::fill_n(SliderImage.begin() + 3, 8, uint8_t{0x1c});
    std::fill_n(SliderImage.end() - 11, 9, uint8_t{0x17});
    MakeSliderTexture();
}

auto MCScrollPane::SetDisplayPort(std::unique_ptr<MCLogPort> port, bool resetPosition) -> void
{
    MCLogPort* content = port.get();
    OwnedContent = std::move(port);
    ShowContent(content, resetPosition);
}

auto MCScrollPane::SetDisplayPort(MCLogPort* port, bool resetPosition) -> void
{
    // The pane keeps owning its content when it is shown again (the logistics store keeps its views and resizes them).
    if (port != OwnedContent.get())
    {
        OwnedContent.reset();
    }

    ShowContent(port, resetPosition);
}

auto MCScrollPane::ClearDisplayPort() -> void
{
    OwnedContent.reset();
    ContentPort = nullptr;
    _OwnPort = nullptr;
}

auto MCScrollPane::ShowContent(MCLogPort* port, bool resetPosition) -> void
{
    if (port == nullptr)
    {
        ClearDisplayPort();
        return;
    }

    ContentPort = port;
    _OwnPort = port;
    const auto unit = static_cast<float>(port->Height() * 0.01);
    ScrollUnit = unit;
    MaxScroll = static_cast<float>(port->Height() - WinHeight) / unit;
    float newScrollPos = 0.0f;

    if (!resetPosition && Height() <= port->Height())
    {
        newScrollPos = static_cast<float>(LastScrollOffset) / static_cast<float>(port->Height()) * 100.0f;
    }

    LastScrollOffset = 0;
    SetUpSlider();
    SetScrollPos(0.0f);
    SliderMax = WinHeight - SliderHeight - ArrowHeight;

    if (newScrollPos != 0.0f)
    {
        SetScrollPos(newScrollPos);
    }
}

auto MCScrollPane::Lport() -> MCLogPort*
{
    return ContentPort;
}

auto MCScrollPane::ForwardToChildren(MCGuiEvent* event) -> void
{
    if (GlobalX() - 14 + WinWidth <= event->X)
    {
        return;
    }

    for (int32_t i = 0; i < NumberOfChildren(); i++)
    {
        MCGuiObject* child = Child(i);

        if (child->GlobalX() <= event->X && event->X <= child->GlobalX() + child->Width() &&
            child->GlobalY() <= event->Y && event->Y <= child->Height() + child->GlobalY())
        {
            child->HandleEvent(event);
            return;
        }
    }
}

auto MCScrollPane::ScrollToRow(int32_t row) -> void
{
    const auto rowTop = static_cast<float>(Child(0)->Height() * row);
    SetScrollPos(rowTop / static_cast<float>(ContentPort->Height()) * 100.0f);
}

auto MCScrollPane::NextRow() -> int32_t
{
    const int32_t rowHeight = Child(0)->Height();
    const int32_t offset = GetScrollOffset();
    return offset % rowHeight == 0 ? offset / rowHeight + 1 : offset / rowHeight + 2;
}

auto MCScrollPane::HandleEvent(MCGuiEvent* event) -> void
{
    if (ContentPort == nullptr)
    {
        return;
    }

    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        {
            if (event->X <= GlobalX() - SliderWidth + WinWidth || GlobalX() + WinWidth <= event->X)
            {
                // Outside the slider column: the first child whose rows hold the mouse.
                for (int32_t i = 0; i < NumberOfChildren(); i++)
                {
                    MCGuiObject* child = Child(i);

                    if (child->GlobalY() <= event->Y && event->Y <= child->GlobalY() + child->Height())
                    {
                        child->HandleEvent(event);
                        return;
                    }
                }

                return;
            }

            GuiSystem()->Grab(this);
            const int32_t mouseY = event->Y;

            if (mouseY < GlobalY() + ArrowHeight)
            {
                // The up arrow.
                _ArrowPressed = Arrow::Up;
                GuiSystem()->AddTimer(this, ArrowTimer, 200, 0, 0, 0);

                if (Child(0) == nullptr)
                {
                    SetSliderPos(SliderPos - BlackFont->Height());
                    return;
                }

                const int32_t rowHeight = Child(0)->Height();
                const int32_t offset = GetScrollOffset();
                ScrollToRow(offset % rowHeight == 0 ? offset / rowHeight - 1 : offset / rowHeight);
                return;
            }

            if (mouseY > GlobalY() - ArrowHeight + WinHeight)
            {
                // The down arrow.
                GuiSystem()->AddTimer(this, ArrowTimer, 200, 0, 0, 0);

                if (Child(0) == nullptr)
                {
                    SetSliderPos(BlackFont->Height() + SliderPos);
                }
                else
                {
                    ScrollToRow(NextRow());
                }

                _ArrowPressed = Arrow::Down;
                return;
            }

            // The track: grab the slider, or page toward the click.
            if (GlobalY() + SliderPos < mouseY && mouseY < GlobalY() + SliderHeight + SliderPos)
            {
                _DragY = mouseY;
                _DraggingSlider = true;
                return;
            }

            if (GlobalY() + ArrowHeight + SliderPos <= mouseY)
            {
                SetSliderPos(SliderHeight + SliderPos);
            }
            else
            {
                SetSliderPos(SliderPos - SliderHeight);
            }

            return;
        }

        case MCGuiEventType::LeftButtonUp:
        {
            GuiSystem()->RemoveTimer(this, ArrowTimer);

            if (GuiSystem()->GrabbedObject() != nullptr)
            {
                GuiSystem()->Release();
                _DraggingSlider = false;
                _DragY = -1;
            }

            // OB-131 (fixed): the original let go of the arrow only on a release over the column. Let go elsewhere, it
            // stayed pressed on screen and the slider could not be dragged until an arrow was released on the column.
            const Arrow released = _ArrowPressed;
            _ArrowPressed = Arrow::None;

            if (GlobalX() - 14 + WinWidth <= event->X)
            {
                if (released != Arrow::None)
                {
                    SetScrollPos(ScrollPos);
                }

                return;
            }

            ForwardToChildren(event);
            return;
        }

        case MCGuiEventType::MouseMove:
        {
            if (GuiSystem()->GrabbedObject() != nullptr)
            {
                if (_ArrowPressed != Arrow::None || !_DraggingSlider)
                {
                    return;
                }

                SetSliderPos(SliderPos - _DragY + event->Y);
                _DragY = event->Y;
                return;
            }

            ForwardToChildren(event);
            return;
        }

        case MCGuiEventType::KeyUp:
        case MCGuiEventType::KeyDown:
        {
            Parent->HandleEvent(event);
            return;
        }

        case MCGuiEventType::Timer:
        {
            // The arrow timer repeats the step.
            if (event->Y < GlobalY() + ArrowHeight)
            {
                if (Child(0) == nullptr)
                {
                    SetSliderPos(SliderPos - BlackFont->Height());
                    return;
                }

                const int32_t rowHeight = Child(0)->Height();
                const int32_t offset = GetScrollOffset();
                ScrollToRow(std::max(offset / rowHeight + (offset % rowHeight == 0 ? -1 : -2), 0));
                return;
            }

            if (Child(0) == nullptr)
            {
                SetSliderPos(BlackFont->Height() + SliderPos);
                return;
            }

            ScrollToRow(NextRow());
            return;
        }

        default:
        {
            ForwardToChildren(event);
            return;
        }
    }
}

auto MCScrollPane::MouseWheel(int32_t steps, int32_t, int32_t) -> bool
{
    if (ContentPort == nullptr || SliderHeight == 0)
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        if (Child(0) == nullptr)
        {
            SetSliderPos(SliderPos + (steps < 0 ? -BlackFont->Height() : BlackFont->Height()));
            continue;
        }

        // To the row boundary above or below the top of the view. The percent position can leave the offset a pixel
        // short of a boundary, so going down counts that pixel as the boundary.
        const int32_t rowHeight = Child(0)->Height();
        const int32_t offset = GetScrollOffset();
        int32_t row;

        if (steps < 0)
        {
            row = offset % rowHeight == 0 ? offset / rowHeight - 1 : offset / rowHeight;
        }
        else
        {
            row = (offset + 1) / rowHeight + 1;
        }

        ScrollToRow(row);
    }

    return true;
}

auto MCScrollPane::GetScrollOffset() -> int32_t
{
    return static_cast<int32_t>(static_cast<double>(ScrollPos) * ScrollUnit);
}

auto MCScrollPane::GetScrollBottom() -> int32_t
{
    return static_cast<int32_t>(Height() + static_cast<double>(ScrollPos) * ScrollUnit);
}
