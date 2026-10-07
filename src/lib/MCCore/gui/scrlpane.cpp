#include "stdafx.h"
#include "gui/scrlpane.h"
#include "gui/afont.h"
#include "lib/MCFatal.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The slider column's width.</summary>
    constexpr int32_t SliderWidth = 13;

    /// <summary>The mouse y the slider drag last moved to; -1 when not dragging.</summary>
    int32_t DragY = -1;
    /// <summary>Nonzero while the slider is being dragged.</summary>
    int32_t DraggingSlider = 0;
    /// <summary>The arrow held down: 0 none, 1 up, 2 down.</summary>
    int32_t ArrowPressed = 0;

    /// <summary>
    /// Passes <paramref name="event"/> to the first child whose box holds the mouse, unless the mouse is over the
    /// slider column (or right of it).
    /// </summary>
    void ForwardToChildren(MCScrollPane* pane, MCGuiEvent* event)
    {
        int32_t mouseX = event->X;

        if (pane->GlobalX() - 14 + pane->WinWidth <= mouseX)
        {
            return;
        }

        for (int32_t i = 0; i < pane->NumberOfChildren(); i++)
        {
            MCGuiObject* child = pane->Child(i);

            if (child->GlobalX() <= mouseX && mouseX <= child->GlobalX() + child->Width() &&
                child->GlobalY() <= event->Y && event->Y <= child->Height() + child->GlobalY())
            {
                pane->Child(i)->HandleEvent(event);
                return;
            }
        }
    }
}

MCScrollPane::~MCScrollPane()
{
    Destroy();
}

auto MCScrollPane::Init() -> void
{
    TrackImage = nullptr;
    BackgroundCopy = nullptr;
    ContentPort = nullptr;
    SliderPort = nullptr;
    SliderHeight = -1;
    SliderPos = 0;
    SliderImage = nullptr;
    SliderImageSize = 0;
    LastScrollOffset = 0;
    ScrollUnit = 0.0f;
    ScrollPos = 0.0f;
    MaxScroll = 0.0f;
    SliderMax = 0;
}

auto MCScrollPane::Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, char* name) -> int32_t
{
    MCLogPort* background = nullptr;

    if (name != nullptr)
    {
        background = new MCLogPort;
        background->Init(name);
    }

    Init(width, height, xPos, yPos, background);

    if (background != nullptr)
    {
        delete background;
    }

    return 0;
}

auto MCScrollPane::Init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, MCLogPort* background) -> void
{
    _OwnPort = nullptr;
    FramePane = nullptr;

    MCLogPort* content = new MCLogPort;
    ContentPort = content;
    Assert(content != nullptr, 0, " not enough memory for fullPane ");
    content->Init(width - SliderWidth, height, -1);
    VfxPaneWipe(content->Frame(), 0x10);

    MCLogPort* art = new MCLogPort;

    if (background != nullptr)
    {
        if (BackgroundCopy != nullptr)
        {
            delete BackgroundCopy;
        }

        BackgroundCopy = new MCLogPort;
        BackgroundCopy->Init(background->Width(), background->Height(), -1);
        background->CopyTo(BackgroundCopy->Frame(), 0, 0, -1);
    }

    int32_t result = MCLogObject::Init(xPos, yPos, width, height, nullptr, content);
    Assert(result == 0, 0, " could not initialize ScrollPane ");
    _OwnPort = ContentPort;

    MCLogPort* slider = new MCLogPort;
    SliderPort = slider;
    Assert(slider != nullptr, 0, " not enought memory to allocate ");
    slider->Init(SliderWidth, height, -1);

    char fileName[256];
    sprintf(fileName, "%slogart\\scroll.tga", ArtPath);

    if (TrackImage != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(TrackImage);
    }

    uint32_t trackSize = static_cast<uint32_t>(height * SliderWidth);
    TrackImage = static_cast<uint8_t*>(GlobalLogPtr->LogisticsBlocks->Allocate(trackSize));

    // The track tile repeats down the column, below the first row.
    art->Init(fileName);
    int32_t numTiles = height / art->Height() - 1;

    for (int32_t i = 0; i < numTiles; i++)
    {
        art->CopyTo(slider->Frame(), 0, art->Height() * i + 1, -1);
    }

    sprintf(fileName, "%slogart\\supbup.tga", ArtPath);
    art->Init(fileName);
    art->CopyTo(slider->Frame(), 0, 0, -1);
    art->Destroy();
    sprintf(fileName, "%slogart\\sdnbup.tga", ArtPath);
    art->Init(fileName);
    art->CopyTo(slider->Frame(), 0, height - 15, -1);

    if (art != nullptr)
    {
        delete art;
    }

    memcpy(TrackImage, slider->Frame()->Window->Buffer, trackSize);

    if (PanePort == nullptr)
    {
        PanePort = new MCLogPort;
    }

    PanePort->InitView(width, height);
    SetUpSlider();
    SetScrollPos(0.0f);
    ShowGuiWindow(0);
    SetDepth(100);
}

auto MCScrollPane::Destroy() -> void
{
    _OwnPort = nullptr;
    MCLogObject::Destroy();

    if (TrackImage != nullptr)
    {
        GlobalLogPtr->LogisticsBlocks->Free(TrackImage);
        TrackImage = nullptr;
    }

    if (ContentPort != nullptr)
    {
        ContentPort->Destroy();
        delete ContentPort;
        ContentPort = nullptr;
    }

    if (SliderPort != nullptr)
    {
        SliderPort->Destroy();
        delete SliderPort;
        SliderPort = nullptr;
    }

    if (SliderImage != nullptr)
    {
        MCRenderer::DestroyTexture(SliderTexture);
        GlobalLogPtr->LogisticsBlocks->Free(SliderImage);
        SliderImage = nullptr;
    }

    if (BackgroundCopy != nullptr)
    {
        delete BackgroundCopy;
        BackgroundCopy = nullptr;
    }

    if (PanePort != nullptr)
    {
        delete PanePort;
        PanePort = nullptr;
    }
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
        EraseSlider();
        int32_t newSliderPos = static_cast<int32_t>((WinHeight - 32) * static_cast<double>(0.01f) * ScrollPos + 16.0);
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

    if (SliderMax < position)
    {
        position = SliderMax;
    }

    if (position < 0x10)
    {
        position = 0x10;
    }

    EraseSlider();
    SliderPos = position;
    float newScrollPos =
        static_cast<float>(static_cast<double>(position - 0x10) / static_cast<double>(WinHeight - 0x20) * 100.0);
    ScrollPos = newScrollPos;

    if (MaxScroll < newScrollPos)
    {
        ScrollPos = MaxScroll;
    }

    SetChildren();
}

auto MCScrollPane::SetChildren() -> void
{
    for (int32_t i = 0; i < NumberOfChildren(); i++)
    {
        MCGuiObject* child = this->Child(i);
        int32_t newY = LastScrollOffset - GetScrollOffset() + child->Y();
        child->MoveTo(child->X(), newY, 0);
    }

    LastScrollOffset = GetScrollOffset();
}

auto MCScrollPane::Draw() -> void
{
    MCPane* view = PanePort->Frame();

    if (BackgroundCopy != nullptr)
    {
        BackgroundCopy->CopyTo(view, 0, 0, -1);
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
            ContentPort->CopyTo(view, 0, offset, -1);
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
    if (IsShowing() == 0)
    {
        return;
    }

    // The original copied its pieces over what the parent had shown there, and showed no children.
    DrawInFramePass(PanePort, 0, false, false);
}

auto MCScrollPane::HeldArrow() const -> int32_t
{
    // An arrow shows held only while the content scrolls (the original put its art in the column when it erased
    // the slider, and there is none to erase when the content fits).
    if (SliderHeight == 0 || Application->GrabbedObject() != this)
    {
        return 0;
    }

    return ArrowPressed;
}

auto MCScrollPane::PressedArrowArt(bool down) -> MCLogPort*
{
    return LogArtf("%slogart\\%s", ArtPath, down ? "lscsb04.tga" : "lscsb03.tga");
}

auto MCScrollPane::DrawSliderColumn(MCPane* target, int32_t xPos, int32_t yPos, bool keyed) -> void
{
    const int key = keyed ? -1 : 0;
    SliderPort->CopyTo(target, xPos, yPos, key);
    const int32_t held = HeldArrow();

    if (held == 1)
    {
        if (MCLogPort* art = PressedArrowArt(false); art != nullptr)
        {
            art->CopyTo(target, xPos, yPos, key);
        }
    }
    else if (held == 2)
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
        image.Buffer = SliderImage;
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
    SliderTexture = MCRenderer::CreateTexture(SliderImage, SliderWidth, SliderHeight, MCTextureUse::Static);
}

auto MCScrollPane::SetUpSlider() -> void
{
    int32_t paneHeight = WinHeight;

    if (ContentPort->Height() <= paneHeight)
    {
        SliderHeight = 0;
        return;
    }

    if (SliderImage != nullptr)
    {
        MCRenderer::DestroyTexture(SliderTexture);
        GlobalLogPtr->LogisticsBlocks->Free(SliderImage);
    }

    // The slider's share of the track (the column less its two 16-pixel arrows) is the pane's share of the content.
    float paneHeightF = static_cast<float>(paneHeight);
    SliderHeight = static_cast<int32_t>(static_cast<double>(paneHeightF) / ContentPort->Height() * (paneHeight - 0x20));
    SliderPos = 0x10;

    if (SliderHeight < 3)
    {
        SliderHeight = 3;
    }

    uint32_t size = static_cast<uint32_t>(SliderHeight * SliderWidth);
    SliderImageSize = size;
    uint8_t* image = static_cast<uint8_t*>(GlobalLogPtr->LogisticsBlocks->Allocate(size));
    SliderImage = image;

    // Every row: dark edges, a light left bevel, a mid fill and a shadowed right bevel.
    static constexpr uint8_t sliderRow[SliderWidth] = {0x35, 0x10, 0x1c, 0x1a, 0x1a, 0x1a, 0x1a,
                                                       0x1a, 0x1a, 0x1a, 0x17, 0x10, 0x35};

    for (int32_t row = 0; row < SliderHeight; row++)
    {
        memcpy(image + row * SliderWidth, sliderRow, SliderWidth);
    }

    // The top row's highlight and the bottom row's shadow.
    memset(image + 3, 0x1c, 8);
    memset(image + size - 11, 0x17, 9);
    MakeSliderTexture();
}

auto MCScrollPane::SetDisplayPort(MCLogPort* port, int deleteOld, int resetPosition) -> void
{
    // Port fix: not when the new content is the old one (the logistics store keeps its views and resizes them).
    if (deleteOld != 0 && ContentPort != nullptr && ContentPort != port)
    {
        delete ContentPort;
    }

    if (port == nullptr)
    {
        ContentPort = nullptr;
        _OwnPort = nullptr;
        return;
    }

    ContentPort = port;
    _OwnPort = port;
    float unit = static_cast<float>(port->Height() * 0.01);
    ScrollUnit = unit;
    MaxScroll = static_cast<float>(port->Height() - WinHeight) / unit;

    float newScrollPos = 0.0f;

    if (resetPosition == 0 && Height() <= port->Height())
    {
        newScrollPos = static_cast<float>(LastScrollOffset) / static_cast<float>(port->Height()) * 100.0f;
    }

    LastScrollOffset = 0;
    EraseSlider();
    SetUpSlider();
    SetScrollPos(0.0f);
    SliderMax = WinHeight - SliderHeight - 0x10;

    if (newScrollPos != 0.0f)
    {
        SetScrollPos(newScrollPos);
    }
}

auto MCScrollPane::EraseSlider() -> void
{
}

auto MCScrollPane::Lport() -> MCLogPort*
{
    return ContentPort;
}

auto MCScrollPane::GetDisplayPort(MCLogPort*& port) -> void
{
    port = ContentPort;
}

auto MCScrollPane::HandleEvent(MCGuiEvent* event) -> void
{
    if (ContentPort == nullptr)
    {
        return;
    }

    float newScrollPos;

    switch (event->Type)
    {
        case 1:
        {
            if (event->X <= GlobalX() - SliderWidth + WinWidth || GlobalX() + WinWidth <= event->X)
            {
                // Outside the slider column: the first child whose rows hold the mouse.
                for (int32_t i = 0; i < NumberOfChildren(); i++)
                {
                    MCGuiObject* child = this->Child(i);

                    if (child->GlobalY() <= event->Y && event->Y <= child->GlobalY() + child->Height())
                    {
                        child->HandleEvent(event);
                        return;
                    }
                }

                return;
            }

            Application->Grab(this);
            int32_t mouseY = event->Y;

            if (GlobalY() + 0x10 <= mouseY)
            {
                if (mouseY <= GlobalY() - 0x10 + WinHeight)
                {
                    // The track: grab the slider, or page toward the click.
                    if (GlobalY() + SliderPos < mouseY && mouseY < GlobalY() + SliderHeight + SliderPos)
                    {
                        DragY = mouseY;
                        DraggingSlider = 1;
                        return;
                    }

                    int32_t oldSliderPos = SliderPos;

                    if (GlobalY() + 0x10 + oldSliderPos <= mouseY)
                    {
                        SetSliderPos(SliderHeight + oldSliderPos);
                    }
                    else
                    {
                        SetSliderPos(oldSliderPos - SliderHeight);
                    }

                    return;
                }

                // The down arrow.
                Application->AddTimer(this, 6, 200, 0, 0, 0);

                if (this->Child(0) == nullptr)
                {
                    SetSliderPos(BlackFont->Height() + SliderPos);
                    ArrowPressed = 2;
                    return;
                }

                int32_t rowHeight = this->Child(0)->Height();
                int32_t offset = GetScrollOffset();
                int32_t row = (GetScrollOffset() % this->Child(0)->Height() == 0) ? offset / rowHeight + 1
                                                                                  : offset / rowHeight + 2;
                float rowTop = static_cast<float>(this->Child(0)->Height() * row);
                SetScrollPos(rowTop / static_cast<float>(ContentPort->Height()) * 100.0f);
                ArrowPressed = 2;
                return;
            }

            // The up arrow.
            ArrowPressed = 1;
            Application->AddTimer(this, 6, 200, 0, 0, 0);

            if (this->Child(0) == nullptr)
            {
                SetSliderPos(SliderPos - BlackFont->Height());
                return;
            }

            int32_t offset = GetScrollOffset();
            int32_t row = offset / this->Child(0)->Height();

            if (GetScrollOffset() % this->Child(0)->Height() == 0)
            {
                row--;
            }

            float rowTop = static_cast<float>(this->Child(0)->Height() * row);
            newScrollPos = rowTop / static_cast<float>(ContentPort->Height());
            break;
        }

        case 4:
        {
            Application->RemoveTimer(this, 6);

            if (Application->GrabbedObject() != nullptr)
            {
                Application->Release();
                DraggingSlider = 0;
                DragY = -1;
            }

            // OB-131 (fixed): the original let go of the arrow only on a release over the column. Let go elsewhere, it
            // stayed pressed on screen and the slider could not be dragged until an arrow was released on the column.
            const int32_t released = ArrowPressed;
            ArrowPressed = 0;

            if (GlobalX() - 14 + WinWidth <= event->X)
            {
                if (released != 0)
                {
                    SetScrollPos(ScrollPos);
                }

                return;
            }

            ForwardToChildren(this, event);
            return;
        }

        case 7:
        {
            if (Application->GrabbedObject() != nullptr)
            {
                if (ArrowPressed != 0 || DraggingSlider == 0)
                {
                    return;
                }

                int32_t mouseY = event->Y;
                SetSliderPos(SliderPos - DragY + mouseY);
                DragY = mouseY;
                return;
            }

            ForwardToChildren(this, event);
            return;
        }

        case 8:
        case 9:
        {
            Parent->HandleEvent(event);
            return;
        }

        case 0x13:
        {
            // The arrow timer repeats the step.
            if (event->Y < GlobalY() + 0x10)
            {
                if (this->Child(0) == nullptr)
                {
                    SetSliderPos(SliderPos - BlackFont->Height());
                    return;
                }

                int32_t rowHeight = this->Child(0)->Height();
                int32_t offset = GetScrollOffset();
                int32_t step = (GetScrollOffset() % this->Child(0)->Height() == 0) ? -1 : -2;
                int32_t row = offset / rowHeight + step;

                if (row < 0)
                {
                    row = 0;
                }

                float rowTop = static_cast<float>(this->Child(0)->Height() * row);
                newScrollPos = rowTop / static_cast<float>(ContentPort->Height());
            }
            else
            {
                if (this->Child(0) == nullptr)
                {
                    SetSliderPos(BlackFont->Height() + SliderPos);
                    return;
                }

                int32_t rowHeight = this->Child(0)->Height();
                int32_t offset = GetScrollOffset();
                int32_t row = (GetScrollOffset() % this->Child(0)->Height() == 0) ? offset / rowHeight + 1
                                                                                  : offset / rowHeight + 2;
                float rowTop = static_cast<float>(this->Child(0)->Height() * row);
                newScrollPos = rowTop / static_cast<float>(ContentPort->Height());
            }
            break;
        }

        default:
        {
            ForwardToChildren(this, event);
            return;
        }
    }

    SetScrollPos(newScrollPos * 100.0f);
}

auto MCScrollPane::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
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

        SetScrollPos(static_cast<float>(rowHeight * row) / static_cast<float>(ContentPort->Height()) * 100.0f);
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
