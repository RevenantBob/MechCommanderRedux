#include "stdafx.h"
#include "gui/atextbox.h"
#include "camera/camera.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/vfx.h"
#include "vfx/vfxfuncs.h"

/// <summary>The gap aScrollTextObject's section highlight leaves at the right (3).</summary>
int16_t rOffset = 3;
int32_t playerColor[6] = {1, 3, 4, 2, 6, 5};
/// <summary>Nonzero to swallow the next Enter in the chat line (set by the interface when Enter opens the chat).</summary>
int FirstReturn = 0;

namespace
{
    /// <summary>The size of aScrollTextObject's text buffer (one more byte is allocated for a terminator).</summary>
    constexpr int32_t TextBufferSize = 0x1000;

    /// <summary>Destroys and deletes a child made with <c>new</c>, and clears the pointer.</summary>
    template <typename T> void releaseChild(T*& child)
    {
        if (child != nullptr)
        {
            child->destroy();
            delete child;
            child = nullptr;
        }
    }

    /// <summary>The font row (the <c>fonts</c> table's first index) aScrollTextObject draws colour <paramref name="color"/> in.</summary>
    int32_t fontRowForColor(uint8_t color)
    {
        switch (color)
        {
            case 0x10:
                return 0;
            case 0xef:
                return 1;
            case 0xf2:
                return 2;
            case 0x0b:
                return 3;
            case 0x0c:
                return 4;
            case 0x19:
                return 5;
            case 0x1f:
                return 6;
            default:
                return 7;
        }
    }
}

/// <remarks>MCX.EXE @ 0x00616860</remarks>
auto aTextObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    const int32_t result = aObject::init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    readOnly = 0;
    textFont = greyFont;
    textTerminator = 0;

    if (newText == nullptr)
    {
        text[0] = '\0';
        textLength = 0;
    }
    else
    {
        std::strncpy(text, newText, 0xfe);
        textLength = static_cast<int16_t>(std::strlen(newText));

        if (textLength > 0xfe)
        {
            textLength = 0xfe;
        }
    }

    backgroundColor = 0;
    VFX_pane_wipe(displayPort->frame(), 0);
    return 0;
}

/// <remarks>MCX.EXE @ 0x00616920</remarks>
auto aTextObject::handleEvent(aEvent* event) -> void
{
    // The event routine gets a copy taken before any editing.
    aEvent eventCopy = *event;

    if (readOnly == 0)
    {
        if (event->type == 1)
        {
            application->setText(this);

            if (parent != nullptr)
            {
                bringToFront(0);
                aRedrawScreen();
            }
        }
        else if (event->type == 10 && application->textObject() == this)
        {
            const uint8_t key = event->key;

            if (key == 8)
            {
                if (textLength > 0)
                {
                    // Original behaviour (OB-067): clears the terminator, not the last character, so the character
                    // stays on show until the next one typed replaces it.
                    text[textLength] = '\0';
                    textLength--;
                }
            }
            else
            {
                if (key == 0xd && parent != nullptr)
                {
                    aPostMessage(parent, 0x17);
                }

                if (textLength < 0xfe && (std::isprint(key) != 0 || key == ' '))
                {
                    text[textLength] = static_cast<char>(key);
                    textLength++;
                    text[textLength] = '\0';
                }
            }
        }
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, &eventCopy);
    }
}

/// <remarks>MCX.EXE @ 0x00616a70</remarks>
auto aTextObject::draw() -> void
{
    // textTerminator ends a full 254 characters.
    char shown[256];
    const size_t length = strnlen(text, sizeof(text));
    std::memcpy(shown, text, length);
    shown[length] = '\0';
    VFX_pane_wipe(displayPort->frame(), backgroundColor);
    drawFramed(0, 0);

    if (application->textObject() == this)
    {
        // The caret.
        shown[textLength] = 0x7f;
        shown[textLength + 1] = '\0';
    }

    // Drop leading characters until the rest fits.
    uint8_t* start = reinterpret_cast<uint8_t*>(shown);

    while (*start != 0 && textFont->width(start) > width() - 6)
    {
        start++;
    }

    if (*start != 0)
    {
        textFont->writeString(displayPort->frame(), 3, 3, start, -1);
    }
}

/// <remarks>MCX.EXE @ 0x00616b80</remarks>
auto aTextObject::setText(char* newText) -> void
{
    if (newText == nullptr)
    {
        text[0] = '\0';
        textLength = 0;
        return;
    }

    std::strncpy(text, newText, 0xfe);
    // textTerminator ends a full 254 characters.
    textLength = static_cast<int16_t>(strnlen(text, sizeof(text)));
}

/// <remarks>MCX.EXE @ 0x00616bd0</remarks>
auto PaintScrollTab(aObject* obj) -> void
{
    const int32_t width = obj->width();
    const int32_t height = obj->height();
    PANE* pane = obj->port()->frame();
    VFX_pane_wipe(pane, 0x1a);
    VFX_line_draw(pane, 0, 0, width - 2, 0, LD_DRAW, 0x1f);
    VFX_line_draw(pane, 0, 0, 0, height - 2, LD_DRAW, 0x1f);
    VFX_line_draw(pane, width - 1, 0, width - 1, height - 1, LD_DRAW, 0x16);
    VFX_line_draw(pane, 0, height - 1, width - 1, height - 1, LD_DRAW, 0x16);
}

/// <remarks>MCX.EXE @ 0x00616c60</remarks>
auto ScrollTabEventHandler(aObject* obj, aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            application->grab(obj);
            obj->startDrag(0, event->y - obj->globalY());
            break;
        }
        case 4:
        {
            application->release();
            obj->stopDrag();
            break;
        }
        case 7:
        {
            // Any grab will do (the original doesn't check that it is this thumb).
            if (application->grabbedObject() == nullptr)
            {
                break;
            }

            auto* textObject = static_cast<aScrollTextObject*>(obj->parent);
            const int32_t newY = (event->y - obj->parent->y()) - obj->dragStartY();
            obj->moveTo(obj->x(), newY, 0);

            if (obj->y() < 0x10)
            {
                obj->moveTo(obj->x(), 0x10, 0);
            }

            if (obj->y() > (-0x10 - textObject->scrollTab->height()) + textObject->height())
            {
                obj->moveTo(obj->x(), (-0x10 - textObject->scrollTab->height()) + textObject->height(), 0);
            }

            textObject->CalcFirstPixel(obj->y() - 0x10);
            break;
        }

        default:
            break;
    }
}

/// <remarks>MCX.EXE @ 0x00616db0</remarks>
auto aScrollTextObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) -> int32_t
{
    // aObject::init, inlined with an aScrollPort for the port.
    winHeight = height;
    maxHeight = height;
    normalHeight = height;
    iconHeight = height;
    initFailed = 0;
    winWidth = width;
    winX = xPos;
    winY = yPos;
    maxWidth = width;
    maxX = xPos;
    maxY = yPos;
    normalWidth = width;
    normalX = xPos;
    normalY = yPos;
    iconWidth = width;
    iconX = xPos;
    iconY = yPos;
    hideOffset = 0;
    homeX = xPos;
    homeY = yPos;
    winState = 0;
    showWindow = 1;
    dragOn = 0;
    transparent = 0;
    backgroundColor = 0xff;

    if (displayPort != nullptr)
    {
        displayPort->destroy();
        delete displayPort;
        displayPort = nullptr;
    }

    displayPort = new aScrollPort;

    if (displayPort == nullptr)
    {
        initFailed = 1;
        return 3;
    }

    int32_t result = DrawsLive() ? displayPort->initView(width, height) : displayPort->init(width, height);

    if (result != 0)
    {
        initFailed = 1;
        return result;
    }

    if (framePane != nullptr)
    {
        delete framePane;
        framePane = nullptr;
    }

    framePane = new (std::nothrow) _pane;

    if (framePane == nullptr)
    {
        initFailed = 1;
        return 3;
    }

    framePane->window = screenPort->bitmap();
    framePane->x0 = xPos;
    framePane->y0 = yPos;
    framePane->x1 = xPos + width;
    framePane->y1 = yPos + height;
    hidden = 0;
    hideDirection = 3;
    paintRoutine = nullptr;
    eventRoutine = nullptr;
    numChildren = 0;
    parent = nullptr;
    winDepth = 0;
    windowAnimation = nullptr;
    animating = 0;
    iconAnimation = nullptr;
    objectType = -1;

    scrollTab = new aObject;

    if (scrollTab == nullptr)
    {
        Fatal(0, "Not enough memory for scrollbar tab.");
    }

    scrollTab->SetDrawsLive();
    result = scrollTab->init(0, 0, 9, height - 0x20, nullptr);

    if (result != 0)
    {
        initFailed = 1;
        return result;
    }

    scrollTab->moveTo(this->width() + 2, 0x10, 0);
    scrollTab->setDepth(100);
    addChild(scrollTab);
    scrollTab->ShowGUIWindow(1);
    scrollTab->setEventRoutine(ScrollTabEventHandler);
    scrollTab->setPaintRoutine(PaintScrollTab);
    scrollTab->setDepth(1);

    textBuffer = static_cast<char*>(guiHeap->malloc(TextBufferSize + 1));

    if (textBuffer == nullptr)
    {
        Fatal(0, "Not enough memory for text.");
    }

    std::memset(textBuffer, 0, TextBufferSize);
    numLines = 0;
    textLength = 0;
    firstPixel = 0;
    fontIndex = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        sectionStarts[i] = -1;
        sectionColors[i] = 0xff;
    }

    if (text != nullptr)
    {
        Print(text, 0x1f);
    }

    return 0;
}

/// <remarks>MCX.EXE @ 0x006170b0</remarks>
auto aScrollTextObject::destroy() -> void
{
    releaseChild(scrollTab);

    if (textBuffer != nullptr)
    {
        guiHeap->free(textBuffer);
        textBuffer = nullptr;
    }

    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x00617110</remarks>
auto aScrollTextObject::draw() -> void
{
    int32_t lineY = 2;
    char* line = textBuffer;
    const int32_t lineHeight = fonts[0][fontIndex]->height() + 2;
    VFX_pane_wipe(port()->frame(), 0x10);

    // Each section highlights one line.
    for (int32_t i = 0; i < 4; i++)
    {
        const int32_t start = sectionStarts[i];

        if (start != -1 && start < start + 1)
        {
            FillBox(1, static_cast<int16_t>(lineHeight * start + 1), static_cast<int16_t>(width() - rOffset),
                    static_cast<int16_t>((start + 1) * lineHeight - 1), sectionColors[i]);
        }
    }

    // Lines are (colour byte, text, '\n'). A tab splits a line into two pieces, the second drawn at tabStop.
    while (line != nullptr && *line != '\0')
    {
        const uint8_t color = static_cast<uint8_t>(*line);
        line++;
        int32_t pieces = 0;
        int32_t lineX = 2;

        if (char* tab = std::strchr(line, '\t'); tab != nullptr)
        {
            if (tabStop < 0)
            {
                *tab = ' ';
            }
            else
            {
                *tab = '\n';
                pieces = 1;
            }
        }

        const int32_t fontRow = fontRowForColor(color);

        do
        {
            fonts[fontRow][fontIndex]->writeStringToNewline(port()->frame(), lineX, lineY,
                                                            reinterpret_cast<uint8_t*>(line));
            line = std::strchr(line, '\n');

            if (pieces > 0)
            {
                // Put the tab back.
                if (line != nullptr)
                {
                    *line = '\t';
                }

                lineX = tabStop;
            }

            pieces--;

            if (line != nullptr)
            {
                line++;
            }
        } while (pieces >= 0 && line != nullptr);

        lineY += lineHeight;
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        if (DrawsChild(childList[i]))
        {
            childList[i]->draw();
        }
    }
}

/// <remarks>MCX.EXE @ 0x00617420</remarks>
auto aScrollTextObject::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    // Port: the port is the whole text (taller than the object), drawn each frame scrolled by firstPixel.
    if (DrawsLive())
    {
        DrawInFramePass(port(), firstPixel);
        return;
    }

    if (port() != nullptr)
    {
        VFX_pane_copy(port()->frame(), 0, 0, framePane, 0, -firstPixel, -1);
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }
}

/// <remarks>MCX.EXE @ 0x006174a0</remarks>
auto aScrollTextObject::resize(int32_t newWidth, int32_t newHeight) -> void
{
    const int32_t fontHeight = fonts[0][fontIndex]->height();

    if (newWidth <= 0 || newHeight <= 0 || (newWidth == winWidth && newHeight == winHeight))
    {
        return;
    }

    winWidth = newWidth;
    winHeight = newHeight;
    framePane->x1 = framePane->x0 - 1 + newWidth;
    framePane->y1 = framePane->y0 - 1 + newHeight;
    int32_t portHeight = numLines * (fontHeight + 2);

    if (portHeight <= newHeight)
    {
        portHeight = newHeight;
    }

    port()->resize(newWidth, portHeight);
    scrollTab->moveTo(newWidth + 2, scrollTab->y(), 0);
    PositionScrollTab();
}

/// <remarks>MCX.EXE @ 0x00617550</remarks>
auto aScrollTextObject::ResetPortSize() -> void
{
    // The port only grows.
    int32_t portHeight = (fonts[0][fontIndex]->height() + 2) * numLines;

    if (portHeight <= port()->height())
    {
        portHeight = port()->height();
    }

    port()->resize(port()->width(), portHeight);
}

/// <remarks>MCX.EXE @ 0x006175e0</remarks>
auto aScrollTextObject::Print(char* line, uint8_t color) -> void
{
    const int32_t used = textLength;
    char* buffer = textBuffer;
    const int32_t fontHeight = fonts[0][fontIndex]->height();

    if (TextBufferSize - used <= 2)
    {
        return;
    }

    buffer[used] = static_cast<char>(color);
    char* dest = buffer + used + 1;
    const int32_t textStart = used + 1;
    textLength = textStart;

    if (line == nullptr)
    {
        if (TextBufferSize - textStart > 2)
        {
            // A blank line.
            numLines++;
            textLength = used + 2;
            textBuffer[used + 1] = '\n';
            return;
        }

        // Port fix: the original goes on to strlen(null) when the buffer is nearly full.
        return;
    }

    if (static_cast<int32_t>(std::strlen(line)) + textStart <= TextBufferSize)
    {
        std::sprintf(dest, "%s\n", line);
        textLength = static_cast<int32_t>(std::strlen(line)) + 1 + textStart;
    }
    else
    {
        // No room: the line is cut and the buffer is full (the length isn't advanced).
        std::strncpy(dest, line, static_cast<size_t>(TextBufferSize - textStart));
        textBuffer[TextBufferSize] = '\0';
    }

    numLines++;
    const int32_t needed = numLines * (fontHeight + 2);

    if (needed > port()->height())
    {
        if (port()->resize(port()->width(), needed) != 0)
        {
            initFailed = 1;
        }
    }

    PositionScrollTab();
}

/// <remarks>MCX.EXE @ 0x00617740</remarks>
auto aScrollTextObject::PrintWrapped(char* line, uint8_t color, int32_t wrapWidth) -> void
{
    if (wrapWidth == -1)
    {
        wrapWidth = width();
    }
    while (line != nullptr)
    {
        aFont* font = fonts[0][fontIndex];
        char* split = nullptr;

        if (font->width(reinterpret_cast<uint8_t*>(line)) > wrapWidth - 6)
        {
            split = std::strrchr(line, ' ');

            if (split != nullptr)
            {
                // Cut at the last space, then earlier spaces until the piece fits.
                *split = '\0';
                char* cut = split;

                while (font->width(reinterpret_cast<uint8_t*>(line)) > wrapWidth - 6)
                {
                    split = std::strrchr(line, ' ');

                    *cut = ' ';
                    if (split == nullptr)
                    {
                        break;
                    }

                    *split = '\0';
                    cut = split;
                }
            }
        }

        Print(line, color);

        if (split == nullptr)
        {
            return;
        }

        line = split + 1;
        *split = ' ';
    }
}

/// <remarks>MCX.EXE @ 0x00617810</remarks>
auto aScrollTextObject::Clear() -> void
{
    firstPixel = 0;
    textLength = 0;
    numLines = 0;
    std::memset(textBuffer, 0, TextBufferSize);

    for (int32_t& start : sectionStarts)
    {
        start = -1;
    }
}

/// <remarks>MCX.EXE @ 0x00617860</remarks>
auto aScrollTextObject::CalcFirstPixel(int32_t thumbY) -> void
{
    const int32_t track = (-0x20 - scrollTab->height()) + height();
    const int32_t range = port()->height() - height();

    if (track > 0 && range > 0)
    {
        firstPixel = (range * thumbY) / track;
        return;
    }

    firstPixel = 0;
}

/// <remarks>MCX.EXE @ 0x006178d0</remarks>
auto aScrollTextObject::PositionScrollTab() -> void
{
    if (application->grabbedObject() == scrollTab)
    {
        return;
    }

    const int32_t track = height() - 0x20;
    const int32_t range = port()->height() - height();

    if (range == 0)
    {
        scrollTab->ShowGUIWindow(0);
        return;
    }

    // The thumb's length is the visible fraction of the track (x87: float quotient, then times the track).
    const float shown = static_cast<float>(height());
    int32_t tabLength = static_cast<int32_t>(static_cast<double>(shown) / port()->height() * track);

    if (tabLength < 3)
    {
        tabLength = 3;
    }

    scrollTab->ShowGUIWindow(1);
    scrollTab->resize(scrollTab->width(), tabLength);
    scrollTab->moveTo(scrollTab->x(), ((track - tabLength) * firstPixel) / range + 0x10, 0);
}

/// <remarks>MCX.EXE @ 0x006179f0</remarks>
auto aScrollTextObject::ReceiveClick(int32_t direction, int32_t yPos) -> void
{
    if (height() == port()->height())
    {
        return;
    }

    const int32_t lineHeight = fonts[0][fontIndex]->height() + 2;
    auto clampToEnd = [this]()
    {
        if (firstPixel > port()->height() - height())
        {
            firstPixel = port()->height() - height();
        }
    };

    if (direction == -1)
    {
        firstPixel -= lineHeight;

        if (firstPixel < 0)
        {
            firstPixel = 0;
        }
    }
    else if (direction == 0)
    {
        if (yPos < scrollTab->y())
        {
            firstPixel -= height();

            if (firstPixel < 0)
            {
                firstPixel = 0;
            }
        }
        else if (yPos > scrollTab->bottom())
        {
            firstPixel += height();
            clampToEnd();
        }
    }
    else if (direction == 1)
    {
        firstPixel += lineHeight;
        clampToEnd();
    }

    PositionScrollTab();
}

auto aScrollTextObject::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (height() == port()->height())
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        ReceiveClick(steps < 0 ? -1 : 1, 0);
    }

    return true;
}

/// <remarks>MCX.EXE @ 0x00617b20</remarks>
auto aTransparentTextObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    const int32_t result = aObject::init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    VFX_pane_wipe(displayPort->frame(), 0xff);
    std::memset(text, 0, 0xfe);
    textColor = 0xfd;
    setText(newText);
    return 0;
}

/// <remarks>MCX.EXE @ 0x00617b90</remarks>
auto aTransparentTextObject::draw() -> void
{
    int32_t lineY = 0;
    char* line = text;
    const int32_t lineHeight = lgGreyFont->height();
    VFX_pane_wipe(displayPort->frame(), 0xff);

    while (line != nullptr)
    {
        lgGreyFont->writeStringToNewline(port()->frame(), 0, lineY, reinterpret_cast<uint8_t*>(line));
        line = std::strchr(line, '\n');

        if (line != nullptr)
        {
            line++;
        }

        lineY += lineHeight + 1;
    }

    // Everything drawn (not the 0xff background) becomes the text colour.
    uint8_t* pixel = port()->buffer();

    for (int32_t count = winHeight * winWidth; count > 0; count--, pixel++)
    {
        if (*pixel != 0xff)
        {
            *pixel = textColor;
        }
    }
}

/// <remarks>MCX.EXE @ 0x00617c40</remarks>
auto aTransparentTextObject::display() -> void
{
    if (IsShowing() == 0 || globalPane == nullptr)
    {
        return;
    }

    CopySprite(globalPane, displayPort->bitmap(), winX, winY, winWidth, winHeight, 0, 1);

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }
}

/// <remarks>MCX.EXE @ 0x00617cb0</remarks>
auto aTransparentTextObject::setText(char* newText) -> void
{
    int32_t textHeight = 0;
    int32_t textWidth = 0;
    const int32_t lineHeight = lgGreyFont->height();

    if (newText == nullptr)
    {
        text[0] = '\0';
        textLength = 0;
    }
    else
    {
        std::strncpy(text, newText, 0xfe);
        textLength = static_cast<int16_t>(std::strlen(text));
        // Measure the widest line.
        char* line = text;

        while (line != nullptr)
        {
            char* newline = std::strchr(line, '\n');

            if (newline != nullptr)
            {
                *newline = '\0';
            }

            const int32_t lineWidth = lgGreyFont->width(reinterpret_cast<uint8_t*>(line));

            if (textWidth < lineWidth)
            {
                textWidth = lineWidth;
            }

            textHeight += lineHeight + 1;

            if (newline == nullptr)
            {
                break;
            }

            *newline = '\n';
            line = newline + 1;
        }
    }

    resize(textWidth, textHeight);
    draw();
}

/// <remarks>MCX.EXE @ 0x00617d80</remarks>
auto ScenarioChatCallback(FIDPMessage* message, void*) -> void
{
    if (Terrain::terrainTacticalMap != nullptr)
    {
        Terrain::terrainTacticalMap->handleChatMessage(message->fromID, message->messageBuffer);
    }
}

/// <remarks>MCX.EXE @ 0x00617da0</remarks>
auto aChatInput::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");

    teamButton = new aToolButton;
    result = teamButton->init(1, 1, 0xc, 0x1a, nullptr);
    teamButton->framed = 0;
    // The picture loads return nothing, so every check repeats the init's.
    const int initOk = result == 0;
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    teamButton->setDownPicture(const_cast<char*>("mfdsbg00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load downstate team button for chatsend window");
    teamButton->setUpPicture(const_cast<char*>("mfdsbh00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    teamButton->setGrayPicture(const_cast<char*>("mfdsbn00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    teamButton->pushed = 0;
    addChild(teamButton);

    cursorX = 0x15;

    if (newText != nullptr)
    {
        std::strncpy(text, newText, 0xff);
    }

    inputFont = whiteFont;
    backgroundColor = 0x10;
    // Port: the original drew the line here, which also restarted the caret's blink (see draw).
    cursorVisible = 1;
    return 0;
}

/// <remarks>MCX.EXE @ 0x00617f00</remarks>
auto aChatInput::destroy() -> void
{
    releaseChild(teamButton);
    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x00617f40</remarks>
auto aChatInput::draw() -> void
{
    drawAndCheck(0);

    // Port: the caret, which the original drew into the picture in display (a vertical line a text line high; in
    // the background colour while cursorVisible, so it blinks).
    const int32_t bottom = inputFont->height() + 3 + cursorY;
    const int32_t color = cursorVisible != 0 ? 0x10 : 0x1f;
    VFX_line_draw(displayPort->frame(), cursorX, cursorY, cursorX, bottom, LD_DRAW, color);
    aObject::draw();
}

/// <remarks>MCX.EXE @ 0x00617f60</remarks>
auto aChatInput::drawAndCheck(int32_t maxLines) -> int
{
    // Port: drawing the whole line (maxLines 0) also set cursorVisible, restarting the caret's blink. The line is
    // drawn every frame now, so the places that redrew it after an edit set it themselves.
    if (maxLines == 0)
    {
        VFX_pane_wipe(displayPort->frame(), backgroundColor);
    }

    uint8_t* line = reinterpret_cast<uint8_t*>(text);
    int32_t lineY = 1;
    int32_t extraLines = 0;

    if (text[0] != '\0')
    {
        // The first line leaves room for the team button.
        int32_t lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
        int32_t fits = inputFont->charactersToWidth(line, width() - 0x18, 0);

        while (fits > 0 && fits < lineLength && (maxLines == 0 || extraLines < maxLines))
        {
            uint8_t* next = line + fits;

            if (maxLines == 0)
            {
                const uint8_t saved = *next;
                *next = 0;
                inputFont->writeString(displayPort->frame(), 0x14, lineY, line, -1);
                *next = saved;
            }

            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
            fits = inputFont->charactersToWidth(next, width() - 0x14, 1);
            lineY += 3 + inputFont->height();
            line = next;
            extraLines++;
        }

        if (maxLines == 0)
        {
            inputFont->writeString(displayPort->frame(), 0x14, lineY, line, -1);
        }
    }

    return extraLines < maxLines;
}

/// <remarks>MCX.EXE @ 0x006180d0</remarks>
auto aChatInput::display() -> void
{
    // The caret is drawn by draw, each frame (the original drew it into the picture here).
    aObject::display();
}

/// <remarks>MCX.EXE @ 0x00618130</remarks>
auto aChatInput::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            if (scenario != nullptr && EventsToMissionResultsScreen == 0 && gameAsked == 0)
            {
                application->setText(this);
            }
            break;
        }
        case 8:
        case 9:
        {
            // Alt combinations go to the game interface.
            if (event->altKey != 0 || event->scanCode == 0x38)
            {
                theInterface->handleEvent(event);
            }
            break;
        }
        case 10:
        {
            if (application->textObject() != this || EventsToMissionResultsScreen != 0)
            {
                break;
            }

            const uint8_t key = event->key;

            if (key == 8)
            {
                if (cursorPos != 0)
                {
                    text[cursorPos - 1] = '\0';
                    cursorPos--;
                    setCursorPos(cursorPos);
                    // The original redrew the line here, which restarted the caret's blink.
                    cursorVisible = 1;
                }
            }
            else if (key == 0xd)
            {
                if (FirstReturn != 0)
                {
                    FirstReturn = 0;
                    break;
                }

                if (MPlayer != nullptr && cursorPos > 0)
                {
                    auto* chatWindow = static_cast<aChatWindow*>(parent);
                    int32_t color;

                    if (teamButton->pushed == 0)
                    {
                        MPlayer->sendChat(0, text);
                        color = 6;
                    }
                    else
                    {
                        MPlayer->sendChat(MPlayer->homeTeamGroupID, text);
                        color = 4;
                    }

                    chatWindow->processChatString(MPlayer->sessionManager->myPlayer->id, text, color);
                }

                std::memset(text, 0, 0xff);
                cursorPos = 0;
                setCursorPos(0);
                application->releaseText();
                cursorVisible = 1;
            }
            else if (cursorPos < 0xff && ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%')
            {
                // '%' is the chat formatter's code character.
                text[cursorPos] = static_cast<char>(key);
                cursorPos++;
                const int32_t newPos = cursorPos;

                if (drawAndCheck(2) != 0)
                {
                    setCursorPos(newPos);
                    cursorVisible = 1;
                }
                else
                {
                    // A third line: take it back.
                    cursorPos = newPos - 1;
                    text[newPos - 1] = '\0';
                }
            }
            break;
        }

        case 0x13:
        {
            // The caret blink timer.
            if (event->data == 0)
            {
                cursorVisible = cursorVisible == 0;
            }
            break;
        }
        case 0x1e:
        {
            if (event->data == 7)
            {
                application->AddTimer(this, 0, static_cast<int32_t>(MCPort::CaretBlinkTime()), 0, 0, 0);
            }
            else if (event->data == 8)
            {
                application->RemoveTimer(this, 0);
                cursorVisible = 1;
            }
            break;
        }
        default:
            break;
    }

    aObject::handleEvent(event);
}

/// <remarks>MCX.EXE @ 0x00618480</remarks>
auto aChatInput::setCursorPos(int32_t pos) -> void
{
    // Measure the text up to pos.
    char saved = '\0';

    if (pos < cursorPos)
    {
        saved = text[pos];
        text[pos] = '\0';
    }

    uint8_t* line = reinterpret_cast<uint8_t*>(text);
    cursorX = 0x14;
    cursorY = 0;

    if (text[0] != '\0')
    {
        int32_t lineLength = static_cast<int32_t>(std::strlen(text));
        int32_t fits = inputFont->charactersToWidth(line, width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            line += fits;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
            fits = inputFont->charactersToWidth(line, width() - 0x14, 0);
            cursorY += inputFont->height() + 3;
        }

        cursorX = inputFont->width(line) + 0x15;
    }

    if (saved != '\0')
    {
        text[pos] = saved;
    }
}

/// <remarks>MCX.EXE @ 0x00618590</remarks>
auto aChatWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = aObject::init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    VFX_pane_wipe(port()->frame(), 0x10);
    chatInput = new aChatInput;
    result = chatInput->init(0, height + 4, this->width(), 0x1c, nullptr);

    if (result == 0)
    {
        addChild(chatInput);
        chatInput->ShowGUIWindow(1);
        // The original drew the input line here, restarting its caret's blink.
        chatInput->cursorVisible = 1;
    }

    return result;
}

/// <remarks>MCX.EXE @ 0x006186c0</remarks>
auto aChatWindow::destroy() -> void
{
    releaseChild(chatInput);
    aObject::destroy();
}

/// <remarks>MCX.EXE @ 0x00618700</remarks>
auto aChatWindow::handleNetworkMessage(uint32_t fromID, void* data) -> void
{
    auto* bytes = static_cast<char*>(data);
    processChatString(fromID, bytes + 9, bytes[8] != '\0' ? 6 : 4);
}

/// <remarks>MCX.EXE @ 0x00618730</remarks>
auto aChatWindow::processChatString(uint32_t playerId, char* text, int32_t color) -> void
{
    char* name = const_cast<char*>("?");

    if (playerId != 0)
    {
        name = MPlayer->sessionManager->GetPlayer(playerId)->name;
    }

    if (color == -1)
    {
        color = 6;
    }

    int32_t playerNumber = 0;

    if (playerId != 0)
    {
        playerNumber = MPlayer->sessionManager->GetPlayer(playerId)->playerNumber;
    }

    char line[0x800];
    std::sprintf(line, "%%fc%d%s: %%fc%d%s", playerColor[playerNumber], name, color, text);

    // The original scrolled its picture up by the new text's height, wiped the bottom and wrote the text there; the
    // line is kept and draw shows the lines that way. Lines scrolled wholly off the top are dropped.
    SMUTI& formatter = application->textFormatter;
    const int32_t textHeight = formatter.process(reinterpret_cast<uint8_t*>(line), nullptr, port()->width(), 0);
    chatLines.push_back(ChatLine{line, textHeight});
    int32_t below = 0;

    for (size_t i = chatLines.size(); i > 0; i--)
    {
        below += chatLines[i - 1].height;

        if (below > port()->height())
        {
            chatLines.erase(chatLines.begin(), chatLines.begin() + static_cast<std::ptrdiff_t>(i - 1));
            break;
        }
    }
}

auto aChatWindow::draw() -> void
{
    // The picture was wiped to 0x10 at init, and every scroll wiped the rows it uncovered.
    VFX_pane_wipe(port()->frame(), 0x10);
    SMUTI& formatter = application->textFormatter;
    // Each line lies above the ones after it; the newest ends a row above the bottom.
    int32_t lineY = port()->height() - 1;

    for (const ChatLine& chatLine : chatLines)
    {
        lineY -= chatLine.height;
    }

    for (const ChatLine& chatLine : chatLines)
    {
        char line[0x800];
        std::snprintf(line, sizeof(line), "%s", chatLine.text.c_str());
        formatter.process(reinterpret_cast<uint8_t*>(line), port(), 0, lineY);
        lineY += chatLine.height;
    }

    aObject::draw();
}
