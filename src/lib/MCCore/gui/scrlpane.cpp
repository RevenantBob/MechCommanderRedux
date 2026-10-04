#include "stdafx.h"
#include "gui/scrlpane.h"
#include "gui/afont.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "main/logistics.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>The slider column's width.</summary>
    constexpr int32_t SliderWidth = 13;

    /// <summary>The mouse y the slider drag last moved to; -1 when not dragging.</summary>
    /// <remarks>MCX.EXE @ 0x007a1560 (DAT_007a1560)</remarks>
    int32_t dragY = -1;
    /// <summary>Nonzero while the slider is being dragged.</summary>
    /// <remarks>MCX.EXE @ 0x00808744 (DAT_00808744)</remarks>
    int32_t draggingSlider = 0;
    /// <summary>The arrow held down: 0 none, 1 up, 2 down.</summary>
    /// <remarks>MCX.EXE @ 0x00808748 (DAT_00808748)</remarks>
    int32_t arrowPressed = 0;

    /// <summary>
    /// Passes <paramref name="event"/> to the first child whose box holds the mouse, unless the mouse is over the
    /// slider column (or right of it).
    /// </summary>
    void forwardToChildren(ScrollPane* pane, aEvent* event)
    {
        int32_t mouseX = event->x;

        if (pane->globalX() - 14 + pane->winWidth <= mouseX)
        {
            return;
        }

        for (int32_t i = 0; i < pane->numberOfChildren(); i++)
        {
            aObject* child = pane->child(i);

            if (child->globalX() <= mouseX && mouseX <= child->globalX() + child->width() &&
                child->globalY() <= event->y && event->y <= child->height() + child->globalY())
            {
                pane->child(i)->handleEvent(event);
                return;
            }
        }
    }
}

ScrollPane::~ScrollPane()
{
    destroy();
}

auto ScrollPane::operator new(size_t size) noexcept -> void*
{
    return globalLogPtr->logisticsHeap->malloc(static_cast<uint32_t>(size));
}

auto ScrollPane::operator delete(void* ptr) -> void
{
    globalLogPtr->logisticsHeap->free(ptr);
}

auto ScrollPane::init() -> void
{
    trackImage = nullptr;
    backgroundCopy = nullptr;
    contentPort = nullptr;
    sliderPort = nullptr;
    sliderHeight = -1;
    sliderPos = 0;
    sliderImage = nullptr;
    sliderImageSize = 0;
    lastScrollOffset = 0;
    scrollUnit = 0.0f;
    scrollPos = 0.0f;
    maxScroll = 0.0f;
    sliderMax = 0;
    unknown4E8 = -1;
}

auto ScrollPane::init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, char* name) -> int32_t
{
    lPort* background = nullptr;

    if (name != nullptr)
    {
        background = new lPort;
        background->init(name);
    }

    init(width, height, xPos, yPos, background);

    if (background != nullptr)
    {
        delete background;
    }

    return 0;
}

auto ScrollPane::init(int32_t width, int32_t height, int32_t xPos, int32_t yPos, lPort* background) -> void
{
    ownPort = nullptr;
    framePane = nullptr;

    lPort* content = new lPort;
    contentPort = content;
    Assert(content != nullptr, 0, " not enough memory for fullPane ", nullptr);
    content->init(width - SliderWidth, height, -1);
    VFX_pane_wipe(content->frame(), 0x10);

    lPort* art = new lPort;

    if (background != nullptr)
    {
        if (backgroundCopy != nullptr)
        {
            delete backgroundCopy;
        }

        backgroundCopy = new lPort;
        backgroundCopy->init(background->width(), background->height(), -1);
        background->copyTo(backgroundCopy->frame(), 0, 0, -1);
    }

    int32_t result = lObject::init(xPos, yPos, width, height, nullptr, content);
    Assert(result == 0, 0, " could not initialize ScrollPane ", nullptr);
    ownPort = contentPort;

    lPort* slider = new lPort;
    sliderPort = slider;
    Assert(slider != nullptr, 0, " not enought memory to allocate ", nullptr);
    slider->init(SliderWidth, height, -1);

    char fileName[256];
    sprintf(fileName, "%slogart\\scroll.tga", artPath);

    if (trackImage != nullptr)
    {
        globalLogPtr->logisticsHeap->free(trackImage);
    }

    uint32_t trackSize = static_cast<uint32_t>(height * SliderWidth);
    trackImage = static_cast<uint8_t*>(globalLogPtr->logisticsHeap->malloc(trackSize));

    // The track tile repeats down the column, below the first row.
    art->init(fileName);
    int32_t numTiles = height / art->height() - 1;

    for (int32_t i = 0; i < numTiles; i++)
    {
        art->copyTo(slider->frame(), 0, art->height() * i + 1, -1);
    }

    sprintf(fileName, "%slogart\\supbup.tga", artPath);
    art->init(fileName);
    art->copyTo(slider->frame(), 0, 0, -1);
    art->destroy();
    sprintf(fileName, "%slogart\\sdnbup.tga", artPath);
    art->init(fileName);
    art->copyTo(slider->frame(), 0, height - 15, -1);

    if (art != nullptr)
    {
        delete art;
    }

    memcpy(trackImage, slider->frame()->window->buffer, trackSize);

    if (panePort == nullptr)
    {
        panePort = new lPort;
    }

    panePort->initView(width, height);
    setUpSlider();
    setScrollPos(0.0f);
    ShowGUIWindow(0);
    setDepth(100);
}

auto ScrollPane::destroy() -> void
{
    ownPort = nullptr;
    lObject::destroy();

    if (trackImage != nullptr)
    {
        globalLogPtr->logisticsHeap->free(trackImage);
        trackImage = nullptr;
    }

    if (contentPort != nullptr)
    {
        contentPort->destroy();
        delete contentPort;
        contentPort = nullptr;
    }

    if (sliderPort != nullptr)
    {
        sliderPort->destroy();
        delete sliderPort;
        sliderPort = nullptr;
    }

    if (sliderImage != nullptr)
    {
        MCRenderer::DestroyTexture(sliderTexture);
        globalLogPtr->logisticsHeap->free(sliderImage);
        sliderImage = nullptr;
    }

    if (backgroundCopy != nullptr)
    {
        delete backgroundCopy;
        backgroundCopy = nullptr;
    }

    if (panePort != nullptr)
    {
        delete panePort;
        panePort = nullptr;
    }
}

auto ScrollPane::setScrollPos(float position) -> void
{
    scrollPos = position;

    if (maxScroll < position)
    {
        scrollPos = maxScroll;
    }

    if (scrollPos < 0.0f)
    {
        scrollPos = 0.0f;
    }

    if (sliderHeight != 0)
    {
        eraseSlider();
        int32_t newSliderPos = static_cast<int32_t>((winHeight - 32) * static_cast<double>(0.01f) * scrollPos + 16.0);
        setChildren();
        sliderPos = newSliderPos;
    }
}

auto ScrollPane::setSliderPos(int32_t position) -> void
{
    if (sliderHeight == 0)
    {
        return;
    }

    if (sliderMax < position)
    {
        position = sliderMax;
    }

    if (position < 0x10)
    {
        position = 0x10;
    }

    eraseSlider();
    sliderPos = position;
    float newScrollPos =
        static_cast<float>(static_cast<double>(position - 0x10) / static_cast<double>(winHeight - 0x20) * 100.0);
    scrollPos = newScrollPos;

    if (maxScroll < newScrollPos)
    {
        scrollPos = maxScroll;
    }

    setChildren();
}

auto ScrollPane::setChildren() -> void
{
    unknown4E8 = -1;

    for (int32_t i = 0; i < numberOfChildren(); i++)
    {
        aObject* child = this->child(i);
        int32_t newY = lastScrollOffset - getScrollOffset() + child->y();
        child->moveTo(child->x(), newY, 0);
    }

    lastScrollOffset = getScrollOffset();
}

auto ScrollPane::draw() -> void
{
    _pane* view = panePort->frame();

    if (backgroundCopy != nullptr)
    {
        backgroundCopy->copyTo(view, 0, 0, -1);
    }

    if (contentPort != nullptr)
    {
        const auto offset = static_cast<int32_t>(-(static_cast<double>(scrollPos) * scrollUnit));

        if (contentPort->isView())
        {
            const MCView& pane = panePort->view;
            contentPort->openView(pane.Target, pane.OriginX, pane.OriginY + offset, pane.Scissor, true);
            drawContent();
            contentPort->closeView();
        }
        else
        {
            contentPort->copyTo(view, 0, offset, -1);
        }
    }

    DrawSliderColumn(view, winWidth - SliderWidth, 0, true);
}

auto ScrollPane::drawContent() -> void
{
    if (contentPort->DrawContent)
    {
        contentPort->DrawContent(contentPort);
    }
}

auto ScrollPane::DrawContentTo(_pane* target, int32_t xPos, int32_t yPos) -> void
{
    if (contentPort == nullptr)
    {
        return;
    }

    if (!contentPort->isView())
    {
        VFX_pane_copy(contentPort->frame(), 0, getScrollOffset(), target, xPos, yPos, -1);
        return;
    }

    const auto offset = static_cast<int32_t>(-(static_cast<double>(scrollPos) * scrollUnit));
    const MCRect scissor{target->x0 + xPos, target->y0 + yPos, target->x1, target->y1};
    contentPort->openView(target->window, target->x0 + xPos, target->y0 + yPos + offset, scissor, false);
    drawContent();
    contentPort->closeView();
}

auto ScrollPane::display() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    // The original copied its pieces over what the parent had shown there, and showed no children.
    DrawInFramePass(panePort, 0, false, false);
}

auto ScrollPane::HeldArrow() const -> int32_t
{
    // An arrow shows held only while the content scrolls (the original put its art in the column when it erased
    // the slider, and there is none to erase when the content fits).
    if (sliderHeight == 0 || application->grabbedObject() != this)
    {
        return 0;
    }

    return arrowPressed;
}

auto ScrollPane::PressedArrowArt(bool down) -> lPort*
{
    return logArtf("%slogart\\%s", artPath, down ? "lscsb04.tga" : "lscsb03.tga");
}

auto ScrollPane::DrawSliderColumn(_pane* target, int32_t xPos, int32_t yPos, bool keyed) -> void
{
    const int key = keyed ? -1 : 0;
    sliderPort->copyTo(target, xPos, yPos, key);
    const int32_t held = HeldArrow();

    if (held == 1)
    {
        if (lPort* art = PressedArrowArt(false); art != nullptr)
        {
            art->copyTo(target, xPos, yPos, key);
        }
    }
    else if (held == 2)
    {
        if (lPort* art = PressedArrowArt(true); art != nullptr)
        {
            art->copyTo(target, xPos, yPos + height() - art->height(), key);
        }
    }

    if (sliderHeight > 0 && sliderTexture != nullptr)
    {
        // The slider image is a 13-wide picture of whole rows.
        _window image{};
        image.buffer = sliderImage;
        image.x_max = SliderWidth - 1;
        image.y_max = sliderTexture->Height - 1;
        image.Texture = sliderTexture;
        _pane imagePane{&image, 0, 0, SliderWidth - 1, sliderTexture->Height - 1};
        VFX_pane_copy(&imagePane, 0, 0, target, xPos, yPos + sliderPos, -1);
    }
}

auto ScrollPane::MakeSliderTexture() -> void
{
    MCRenderer::DestroyTexture(sliderTexture);
    sliderTexture = MCRenderer::CreateTexture(sliderImage, SliderWidth, sliderHeight, MCTextureUse::Static);
}

auto ScrollPane::setUpSlider() -> void
{
    int32_t paneHeight = winHeight;

    if (contentPort->height() <= paneHeight)
    {
        sliderHeight = 0;
        return;
    }

    if (sliderImage != nullptr)
    {
        MCRenderer::DestroyTexture(sliderTexture);
        globalLogPtr->logisticsHeap->free(sliderImage);
    }

    // The slider's share of the track (the column less its two 16-pixel arrows) is the pane's share of the content.
    float paneHeightF = static_cast<float>(paneHeight);
    sliderHeight = static_cast<int32_t>(static_cast<double>(paneHeightF) / contentPort->height() * (paneHeight - 0x20));
    sliderPos = 0x10;

    if (sliderHeight < 3)
    {
        sliderHeight = 3;
    }

    uint32_t size = static_cast<uint32_t>(sliderHeight * SliderWidth);
    sliderImageSize = size;
    uint8_t* image = static_cast<uint8_t*>(globalLogPtr->logisticsHeap->malloc(size));
    sliderImage = image;

    // Every row: dark edges, a light left bevel, a mid fill and a shadowed right bevel.
    static constexpr uint8_t sliderRow[SliderWidth] = {0x35, 0x10, 0x1c, 0x1a, 0x1a, 0x1a, 0x1a,
                                                       0x1a, 0x1a, 0x1a, 0x17, 0x10, 0x35};

    for (int32_t row = 0; row < sliderHeight; row++)
    {
        memcpy(image + row * SliderWidth, sliderRow, SliderWidth);
    }

    // The top row's highlight and the bottom row's shadow.
    memset(image + 3, 0x1c, 8);
    memset(image + size - 11, 0x17, 9);
    MakeSliderTexture();
}

auto ScrollPane::setDisplayPort(lPort* port, int deleteOld, int resetPosition) -> void
{
    // Port fix: not when the new content is the old one (the logistics store keeps its views and resizes them).
    if (deleteOld != 0 && contentPort != nullptr && contentPort != port)
    {
        delete contentPort;
    }

    if (port == nullptr)
    {
        contentPort = nullptr;
        ownPort = nullptr;
        return;
    }

    contentPort = port;
    ownPort = port;
    float unit = static_cast<float>(port->height() * 0.01);
    scrollUnit = unit;
    maxScroll = static_cast<float>(port->height() - winHeight) / unit;

    float newScrollPos = 0.0f;

    if (resetPosition == 0 && height() <= port->height())
    {
        newScrollPos = static_cast<float>(lastScrollOffset) / static_cast<float>(port->height()) * 100.0f;
    }

    lastScrollOffset = 0;
    eraseSlider();
    setUpSlider();
    setScrollPos(0.0f);
    sliderMax = winHeight - sliderHeight - 0x10;

    if (newScrollPos != 0.0f)
    {
        setScrollPos(newScrollPos);
    }
}

auto ScrollPane::eraseSlider() -> void
{
}

auto ScrollPane::lport() -> lPort*
{
    return contentPort;
}

auto ScrollPane::getDisplayPort(lPort*& port) -> void
{
    port = contentPort;
}

auto ScrollPane::handleEvent(aEvent* event) -> void
{
    if (contentPort == nullptr)
    {
        return;
    }

    float newScrollPos;

    switch (event->type)
    {
        case 1:
        {
            if (event->x <= globalX() - SliderWidth + winWidth || globalX() + winWidth <= event->x)
            {
                // Outside the slider column: the first child whose rows hold the mouse.
                for (int32_t i = 0; i < numberOfChildren(); i++)
                {
                    aObject* child = this->child(i);

                    if (child->globalY() <= event->y && event->y <= child->globalY() + child->height())
                    {
                        child->handleEvent(event);
                        return;
                    }
                }

                return;
            }

            application->grab(this);
            int32_t mouseY = event->y;

            if (globalY() + 0x10 <= mouseY)
            {
                if (mouseY <= globalY() - 0x10 + winHeight)
                {
                    // The track: grab the slider, or page toward the click.
                    if (globalY() + sliderPos < mouseY && mouseY < globalY() + sliderHeight + sliderPos)
                    {
                        dragY = mouseY;
                        draggingSlider = 1;
                        return;
                    }

                    int32_t oldSliderPos = sliderPos;

                    if (globalY() + 0x10 + oldSliderPos <= mouseY)
                    {
                        setSliderPos(sliderHeight + oldSliderPos);
                    }
                    else
                    {
                        setSliderPos(oldSliderPos - sliderHeight);
                    }

                    return;
                }

                // The down arrow.
                application->AddTimer(this, 6, 200, 0, 0, 0);

                if (this->child(0) == nullptr)
                {
                    setSliderPos(blackFont->height() + sliderPos);
                    arrowPressed = 2;
                    return;
                }

                int32_t rowHeight = this->child(0)->height();
                int32_t offset = getScrollOffset();
                int32_t row = (getScrollOffset() % this->child(0)->height() == 0) ? offset / rowHeight + 1
                                                                                  : offset / rowHeight + 2;
                float rowTop = static_cast<float>(this->child(0)->height() * row);
                setScrollPos(rowTop / static_cast<float>(contentPort->height()) * 100.0f);
                arrowPressed = 2;
                return;
            }

            // The up arrow.
            arrowPressed = 1;
            application->AddTimer(this, 6, 200, 0, 0, 0);

            if (this->child(0) == nullptr)
            {
                setSliderPos(sliderPos - blackFont->height());
                return;
            }

            int32_t offset = getScrollOffset();
            int32_t row = offset / this->child(0)->height();

            if (getScrollOffset() % this->child(0)->height() == 0)
            {
                row--;
            }

            float rowTop = static_cast<float>(this->child(0)->height() * row);
            newScrollPos = rowTop / static_cast<float>(contentPort->height());
            break;
        }

        case 4:
        {
            application->RemoveTimer(this, 6);

            if (application->grabbedObject() != nullptr)
            {
                application->release();
                draggingSlider = 0;
                dragY = -1;
            }

            // OB-131 (fixed): the original let go of the arrow only on a release over the column. Let go elsewhere, it
            // stayed pressed on screen and the slider could not be dragged until an arrow was released on the column.
            const int32_t released = arrowPressed;
            arrowPressed = 0;

            if (globalX() - 14 + winWidth <= event->x)
            {
                if (released != 0)
                {
                    setScrollPos(scrollPos);
                }

                return;
            }

            forwardToChildren(this, event);
            return;
        }

        case 7:
        {
            if (application->grabbedObject() != nullptr)
            {
                if (arrowPressed != 0 || draggingSlider == 0)
                {
                    return;
                }

                int32_t mouseY = event->y;
                setSliderPos(sliderPos - dragY + mouseY);
                dragY = mouseY;
                return;
            }

            forwardToChildren(this, event);
            return;
        }

        case 8:
        case 9:
        {
            parent->handleEvent(event);
            return;
        }

        case 0x13:
        {
            // The arrow timer repeats the step.
            if (event->y < globalY() + 0x10)
            {
                if (this->child(0) == nullptr)
                {
                    setSliderPos(sliderPos - blackFont->height());
                    return;
                }

                int32_t rowHeight = this->child(0)->height();
                int32_t offset = getScrollOffset();
                int32_t step = (getScrollOffset() % this->child(0)->height() == 0) ? -1 : -2;
                int32_t row = offset / rowHeight + step;

                if (row < 0)
                {
                    row = 0;
                }

                float rowTop = static_cast<float>(this->child(0)->height() * row);
                newScrollPos = rowTop / static_cast<float>(contentPort->height());
            }
            else
            {
                if (this->child(0) == nullptr)
                {
                    setSliderPos(blackFont->height() + sliderPos);
                    return;
                }

                int32_t rowHeight = this->child(0)->height();
                int32_t offset = getScrollOffset();
                int32_t row = (getScrollOffset() % this->child(0)->height() == 0) ? offset / rowHeight + 1
                                                                                  : offset / rowHeight + 2;
                float rowTop = static_cast<float>(this->child(0)->height() * row);
                newScrollPos = rowTop / static_cast<float>(contentPort->height());
            }
            break;
        }

        default:
        {
            forwardToChildren(this, event);
            return;
        }
    }

    setScrollPos(newScrollPos * 100.0f);
}

auto ScrollPane::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (contentPort == nullptr || sliderHeight == 0)
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        if (child(0) == nullptr)
        {
            setSliderPos(sliderPos + (steps < 0 ? -blackFont->height() : blackFont->height()));
            continue;
        }

        // To the row boundary above or below the top of the view. The percent position can leave the offset a pixel
        // short of a boundary, so going down counts that pixel as the boundary.
        const int32_t rowHeight = child(0)->height();
        const int32_t offset = getScrollOffset();
        int32_t row;

        if (steps < 0)
        {
            row = offset % rowHeight == 0 ? offset / rowHeight - 1 : offset / rowHeight;
        }
        else
        {
            row = (offset + 1) / rowHeight + 1;
        }

        setScrollPos(static_cast<float>(rowHeight * row) / static_cast<float>(contentPort->height()) * 100.0f);
    }

    return true;
}

auto ScrollPane::getScrollOffset() -> int32_t
{
    return static_cast<int32_t>(static_cast<double>(scrollPos) * scrollUnit);
}

auto ScrollPane::getScrollBottom() -> int32_t
{
    return static_cast<int32_t>(height() + static_cast<double>(scrollPos) * scrollUnit);
}
