#include "stdafx.h"
#include "logistics/logdlg.h"
#include "gui/afont.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "lib/heap.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "main/main.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Which way a held purchase spinner arrow counts (1 up, 0 down; DAT_008080cc).</summary>
    int32_t spinUp = 0;

    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsHeap->malloc(size);
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsHeap->free(block);
    }

    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            port->destroy();
            delete port;
            port = nullptr;
        }
    }

    /// <summary>A copy of <paramref name="text"/> with the CRT's new (null stays null).</summary>
    char* copyString(const char* text)
    {
        if (text == nullptr)
        {
            return nullptr;
        }

        auto* copy = new char[std::strlen(text) + 1];
        std::strcpy(copy, text);
        return copy;
    }
}

// 0x008015d0 is AlphaTable row 0x10c (AlphaTable is at 0x007f09d0): the alpha colour the dialogs fade through.
char* g_logistic_dlgfade = AlphaTable + 0x10c * 256;

// lDialogButton

auto lDialogButton::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    pressedDown = 0;
    result = 0;
    return lButton::init(xPos, yPos, width, height, name);
}

auto lDialogButton::updateFace() -> void
{
    facePicture = disabled != 0 ? grayPicture : (pressedDown != 0 ? downPicture : upPicture);
    faceColor = static_cast<uint8_t>(backgroundColor);
    faceKeyed = true;
}

auto lDialogButton::handleEvent(aEvent* event) -> void
{
    if (disabled != 0)
    {
        return;
    }

    if (event->type == 1)
    {
        // Flash the press, then close the dialog with this button's result.
        pressedDown = -1;
        Refresh();
        pressedDown = 0;
        UpdateDisplay(0, 0, 0, 0, 0);
        soundSystem->playDigitalSample(0x34, 1, nullptr, 0, 0);
        callback()->execute();
        static_cast<ReusableDialog*>(parent)->deactivate(result);
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

// LogDialogBox

auto LogDialogBox::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    twoButton = -1;
    spinner = -1;
    callback = nullptr;
    picturePort = nullptr;
    // The original loaded the box's frame (lspcb00) as its port; the box draws the frame each frame instead.
    lObject::init(xPos, yPos, width, height, nullptr, nullptr);
    SetTransparent(-1);
    ShowGUIWindow(0);
    fadedBackground = nullptr;
}

auto LogDialogBox::destroy() -> void
{
    freePort(fadedBackground);
    freePort(picturePort);
    callback = nullptr;
    lObject::destroy();
    freePort(ownPort);
}

auto LogDialogBox::drawBackground() -> void
{
    if (needBackground != 0)
    {
        // The original copied the screen under the box into fadedBackground here and darkened it, then filled it
        // with 0x10, which is all the box shows of it.
        application->showCursor(0);
        UpdateDisplay(0, 0, 0, 0, 0);
        application->showCursor(-1);
        needBackground = 0;
    }

    pressedArt = nullptr;
}

auto LogDialogBox::draw() -> void
{
    drawBox();
    drawPressed();
}

auto LogDialogBox::drawPressed() -> void
{
    if (pressedArt != nullptr)
    {
        pressedArt->copyTo(ownPort->frame(), pressedX, pressedY, -1);
    }
}

auto LogDialogBox::drawBox() -> void
{
    _pane* port = ownPort->frame();
    // The box was its frame's picture: the fill covers the frame, not the whole pane.
    lPort* frameArt = logArtf("%slogart\\lspcb00.tga", artPath);
    _pane fill = *port;
    fill.x1 = fill.x0 + frameArt->width() - 1;
    fill.y1 = fill.y0 + frameArt->height() - 1;
    VFX_pane_wipe(&fill, 0x10);
    frameArt->copyTo(port, 0, 0, -1);

    // Without a spinner its place is left as the frame is (the original copied a transparent block there).
    if (spinner != 0)
    {
        VFX_pane_copy(logArtf("%slogart\\lspcb05.tga", artPath)->frame(), 0, 0, port, 0x92, 0x53, -1);
        VFX_pane_copy(logArtf("%slogart\\lspcb06.tga", artPath)->frame(), 0, 0, port, 0x92, 0x5b, -1);
    }

    logArtf("%slogart\\lspcb01.tga", artPath)->copyTo(port, 0x3f, 0x82, -1);

    if (twoButton != 0)
    {
        logArtf("%slogart\\lspcb02.tga", artPath)->copyTo(port, 0x76, 0x82, -1);
    }

    if (picturePort != nullptr)
    {
        picturePort->copyTo(port, 10, 0x1b, -1);
    }
}

auto LogDialogBox::showPressed(const char* name, int32_t xPos, int32_t yPos) -> void
{
    pressedArt = logArtf("%slogart\\%s", artPath, name);
    pressedX = xPos;
    pressedY = yPos;
}

auto LogDialogBox::setTwoButton(int twoButtons) -> void
{
    twoButton = twoButtons;
}

auto LogDialogBox::setSpinner(int newSpinner) -> void
{
    spinner = newSpinner;
}

auto LogDialogBox::activate() -> void
{
    needBackground = -1;
    freePort(fadedBackground);
    application->grab(this);
    bringToFront(0);
    drawBackground();
    ShowGUIWindow(-1);
}

auto LogDialogBox::deactivate(int dialogResult) -> void
{
    application->release();
    ShowGUIWindow(0);

    if (callback != nullptr)
    {
        callback(dialogResult);
    }
}

auto LogDialogBox::setCallback(void (*newCallback)(int)) -> void
{
    callback = newCallback;
}

auto LogDialogBox::setPort(lPort* port) -> void
{
    sharedPort = port;
}

// PurchaseDlg

auto PurchaseDlg::init(int32_t newPurchaseType, int32_t newUnitCost, int32_t newMaxQuantity, char* newTitle,
                       char* newSubtitle, lPort* picture) -> void
{
    if (newMaxQuantity < 0)
    {
        newMaxQuantity = 199;
    }

    quantity = 1;
    purchaseType = newPurchaseType;
    unitCost = newUnitCost;
    title = copyString(newTitle);
    subtitle = copyString(newSubtitle);

    if (picturePort != nullptr)
    {
        delete picturePort;
    }

    if (picture == nullptr)
    {
        picturePort = nullptr;
    }
    else
    {
        picturePort = new lPort;
        picturePort->init(picture->width(), picture->height(), -1);
        VFX_pane_copy(picture->frame(), 0, 0, picturePort->frame(), 0, 0, -1);
    }

    maxQuantity = newMaxQuantity;
    // A single item needs no spinner.
    spinner = newMaxQuantity != 1 ? -1 : 0;
}

auto PurchaseDlg::destroy() -> void
{
    delete[] subtitle;
    subtitle = nullptr;
    delete[] title;
    title = nullptr;
    LogDialogBox::destroy();
}

auto PurchaseDlg::handleEvent(aEvent* event) -> void
{
    const int32_t localX = event->x - globalX();
    const int32_t localY = event->y - globalY();
    // Spinner arrows only work for purchases (even types) and type 5.
    const bool arrowsLocked = (purchaseType & 1) != 0 && purchaseType != 5;
    auto canAddOne = [this]() { return quantity < maxQuantity && unitCost * (quantity + 1) <= ResourcePoints; };

    switch (event->type)
    {
        case 1:
        {
            if (localX >= 0x40 && localX <= 0x6e && localY >= 0x83 && localY <= 0x8e)
            {
                // OK.
                soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);
                showPressed("lspcb03.tga", 0x3f, 0x82);
                UpdateDisplay(0, 0, 0, 0, 0);
                deactivate(-1);
            }
            else if (localX >= 0x77 && localX <= 0xa5 && localY >= 0x83 && localY <= 0x8e)
            {
                // Cancel.
                soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);
                showPressed("lspcb04.tga", 0x76, 0x82);
                UpdateDisplay(0, 0, 0, 0, 0);
                deactivate(0);
            }
            else if (spinner != 0)
            {
                if (localX >= 0x92 && localX <= 0x9a && localY >= 0x53 && localY <= 0x59)
                {
                    // Up: held down it repeats on timer 6.
                    if (arrowsLocked)
                    {
                        break;
                    }

                    drawBackground();
                    showPressed("lspcb07.tga", 0x92, 0x53);
                    spinUp = 1;
                    application->AddTimer(this, 6, 200, 0, 0, 0);
                    application->grab(this);

                    if (canAddOne())
                    {
                        soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);
                        quantity++;
                        break;
                    }

                    soundSystem->playDigitalSample(0x33, 1, nullptr, 0, 0);
                }
                else if (localX >= 0x92 && localX <= 0x9a && localY >= 0x5b && localY <= 0x61 && !arrowsLocked)
                {
                    // Down.
                    drawBackground();
                    showPressed("lspcb09.tga", 0x92, 0x5b);
                    spinUp = 0;
                    application->AddTimer(this, 6, 200, 0, 0, 0);
                    application->grab(this);

                    if (quantity != 0)
                    {
                        soundSystem->playDigitalSample(0xf, 1, nullptr, 0, 0);
                        quantity--;
                        break;
                    }

                    soundSystem->playDigitalSample(0x33, 1, nullptr, 0, 0);
                }
            }
            break;
        }
        case 4:
        {
            drawBackground();
            application->RemoveTimer(this, 6);
            break;
        }
        case 9:
        {
            if (event->key == 0x0d)
            {
                deactivate(twoButton != 0 ? -1 : 0);
            }
            else if (event->key == 0x1b)
            {
                deactivate(0);
            }
            break;
        }
        case 0x13:
        {
            // The held arrow repeats (up to 200).
            if (spinUp == 0)
            {
                if (quantity != 0)
                {
                    quantity--;
                }

                drawBackground();
                showPressed("lspcb09.tga", 0x92, 0x5b);
            }
            else
            {
                if (quantity < maxQuantity && quantity < 200 && unitCost * (quantity + 1) <= ResourcePoints)
                {
                    quantity++;
                }

                drawBackground();
                showPressed("lspcb07.tga", 0x92, 0x53);
            }
            break;
        }

        default:
            break;
    }
}

auto PurchaseDlg::drawBackground() -> void
{
    LogDialogBox::drawBackground();
    shownQuantity = quantity;
    shownResourcePoints = ResourcePoints;
}

auto PurchaseDlg::draw() -> void
{
    drawBox();
    char text[256];
    _pane* port = ownPort->frame();
    // Labels: price, resource points, quantity, remaining.
    cLoadString(thisInstance, 0x48, text, 0xfe);
    medWhiteFont->writeString(port, 0x15, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    cLoadString(thisInstance, 0x4b, text, 0xfe);
    medWhiteFont->writeString(port, 0x91, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    cLoadString(thisInstance, 0x49, text, 0xfe);
    medWhiteFont->writeString(port, 0x15, 0x57, reinterpret_cast<uint8_t*>(text), -1);
    cLoadString(thisInstance, 0x4a, text, 0xfe);
    medWhiteFont->writeString(port, 0x16, 0x6c, reinterpret_cast<uint8_t*>(text), -1);
    cLoadString(thisInstance, 0x4b, text, 0xfe);
    medWhiteFont->writeString(port, 0x91, 0x6c, reinterpret_cast<uint8_t*>(text), -1);

    if (title != nullptr)
    {
        medWhiteFont->writeString(port, 0x2a, 0x20, reinterpret_cast<uint8_t*>(title), -1);
    }

    if (subtitle != nullptr)
    {
        aFont* font = purchaseType == 3 ? medRedFont : medWhiteFont;
        font->writeString(port, 0x2a, 0x2e, reinterpret_cast<uint8_t*>(subtitle), -1);
    }

    // The numbers are right-aligned at 0x8e, measured in the black font.
    std::snprintf(text, sizeof(text), "%d", unitCost < 0 ? -unitCost : unitCost);
    int32_t textWidth = medBlackFont->width(reinterpret_cast<uint8_t*>(text));
    medWhiteFont->writeString(port, 0x8e - textWidth, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", shownResourcePoints - shownQuantity * unitCost);
    textWidth = medBlackFont->width(reinterpret_cast<uint8_t*>(text));
    medWhiteFont->writeString(port, 0x8e - textWidth, 0x6c, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", shownQuantity);
    textWidth = medBlackFont->width(reinterpret_cast<uint8_t*>(text));
    medWhiteFont->writeString(port, (0x11 - textWidth) / 2 + 0x7e, 0x55, reinterpret_cast<uint8_t*>(text), -1);

    // The item kind's icon (mech, part, component, vehicle; sell/buy). Another type loads the quantity text as a
    // file name, as the original did.
    static constexpr const char* icons[8] = {"lspcbm00.tga", "lspcbm01.tga", "lspcbp00.tga", "lspcbp01.tga",
                                             "lspcbc00.tga", "lspcbc01.tga", "lspcbv00.tga", "lspcbv01.tga"};
    lPort* icon =
        purchaseType >= 0 && purchaseType < 8 ? logArtf("%slogart\\%s", artPath, icons[purchaseType]) : logArt(text);

    if (icon != nullptr)
    {
        icon->copyTo(port, 3, 3, -1);
    }

    drawPressed();
}

auto PurchaseDlg::activate() -> void
{
    needBackground = -1;
    freePort(fadedBackground);
    application->grab(this);
    bringToFront(0);
    drawBackground();
    ShowGUIWindow(-1);
}

auto PurchaseDlg::deactivate(int dialogResult) -> void
{
    application->RemoveTimer(this, 6);
    application->release();
    ShowGUIWindow(0);

    if (purchaseCallback != nullptr)
    {
        purchaseCallback(dialogResult, quantity);
    }
}

auto PurchaseDlg::setCallback(void (*newCallback)(int, int32_t)) -> void
{
    purchaseCallback = newCallback;
}

// ReusableDialog

auto ReusableDialog::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)xPos;
    (void)yPos;
    (void)width;
    (void)height;
    (void)name;
    // The box is its top, some middle pieces and its bottom, centred across the screen at y 200.
    topPiece = new lPort;
    int32_t result = topPiece->init(const_cast<char*>("dbox_top.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);
    middlePiece = new lPort;
    result = middlePiece->init(const_cast<char*>("dbox_middle.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);
    bottomPiece = new lPort;
    result = bottomPiece->init(const_cast<char*>("dbox_bottom.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);
    const int32_t boxWidth = topPiece->width();
    result = lObject::init(application->width() / 2 - boxWidth / 2, 200, boxWidth,
                           bottomPiece->height() + middlePiece->height() + topPiece->height(), nullptr, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);

    okButton = new lDialogButton;
    result = okButton->init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);
    addChild(okButton);
    cancelButton = new lDialogButton;
    result = cancelButton->init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog", nullptr);
    addChild(cancelButton);
    setTwoButton(0);
    ShowGUIWindow(0);
    setDepth(100);
    timeoutResult = 0;
    keepCallbacks = 0;
    return 0;
}

auto ReusableDialog::destroy() -> void
{
    freePort(topPiece);
    freePort(middlePiece);
    freePort(bottomPiece);

    if (okButton != nullptr)
    {
        delete okButton;
        okButton = nullptr;
    }

    if (cancelButton != nullptr)
    {
        delete cancelButton;
        cancelButton = nullptr;
    }

    if (text != nullptr)
    {
        logFree(text);
        text = nullptr;
    }

    lObject::destroy();
}

auto ReusableDialog::draw() -> void
{
    int32_t pieceY = 0;

    if (topPiece != nullptr)
    {
        topPiece->copyTo(lport()->frame(), 0, 0, -1);
        pieceY = topPiece->height();
    }

    for (int32_t i = 0; i < numMiddlePieces; i++)
    {
        if (middlePiece != nullptr)
        {
            middlePiece->copyTo(lport()->frame(), 0, pieceY, -1);
            pieceY += middlePiece->height();
        }
    }

    if (bottomPiece != nullptr)
    {
        bottomPiece->copyTo(lport()->frame(), 0, pieceY, -1);
    }

    // The text, word-wrapped to the box.
    int32_t lineY = topPiece->height() + 2;

    if (text != nullptr)
    {
        int32_t length = static_cast<int32_t>(std::strlen(text));
        auto* line = reinterpret_cast<uint8_t*>(text);
        int32_t fit = medBlueFont->charactersToWidth(line, width() - 0x14, -1);

        if (fit == length)
        {
            medBlueFont->writeString(ownPort->frame(), 0xc, lineY, line, -1);
        }
        else
        {
            while (fit > 0 && fit <= length)
            {
                const uint8_t saved = line[fit];
                uint8_t* next = line + fit;
                *next = 0;
                medBlueFont->writeString(ownPort->frame(), 0xc, lineY, line, -1);

                *next = saved;
                if (saved != 0)
                {
                    next++;
                }

                length = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
                fit = medBlueFont->charactersToWidth(next, width() - 0x14, -1);
                lineY += medBlueFont->height() + 3;
                line = next;
            }
        }
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        DrawChild(childList[i]);
    }
}

auto ReusableDialog::handleEvent(aEvent* event) -> void
{
    if (event->type == 9)
    {
        if (event->key == 0x0d)
        {
            okButton->callback()->execute();
            deactivate(twoButton != 0 ? -1 : 0);
        }
        else if (event->key == 0x1b)
        {
            // Original behaviour (OB-074): Escape runs the OK button's callback too.
            okButton->callback()->execute();
            deactivate(0);
        }
    }
    else if (event->type == 0x13)
    {
        // Timed out.
        deactivate(timeoutResult);
    }

    // While grabbed, clicks go to the child under the mouse.
    if (application->grabbedObject() == this)
    {
        aObject* target = findObject(event->x, event->y);

        if (target != nullptr && target != this)
        {
            target->handleEvent(event);
            return;
        }
    }

    aObject::handleEvent(event);
}

auto ReusableDialog::activate() -> void
{
    application->grab(this);
    Refresh();
    moveTo(0x140 - width() / 2, 0xf0 - height() / 2, 0);
    ShowGUIWindow(-1);

    if (timeout > 0)
    {
        application->AddTimer(this, 0, timeout, 0, 0, 0);
    }
}

auto ReusableDialog::deactivate(int32_t dialogResult) -> void
{
    application->release();
    ShowGUIWindow(0);

    // Port: the original also skipped a callback pointer IsBadReadPtr rejected; a function pointer is always valid.
    if (callback != nullptr)
    {
        callback(dialogResult);
    }

    if (keepCallbacks != 0)
    {
        keepCallbacks = 0;
        return;
    }

    callback = nullptr;
    okButton->callback()->setExec(nullptr);
    cancelButton->callback()->setExec(nullptr);
    application->RemoveTimer(this, 0);
    timeout = 0;
    timeoutResult = 0;
}

auto ReusableDialog::setText(char* newText) -> void
{
    if (text != nullptr)
    {
        logFree(text);
    }

    size_t size = std::strlen(newText) + 1;
    text = static_cast<char*>(logAlloc(static_cast<uint32_t>(size)));
    std::strcpy(text, newText);
    // Count the wrapped lines; each middle piece holds two.
    auto* line = reinterpret_cast<uint8_t*>(text);
    int32_t fit = medBlueFont->charactersToWidth(line, width() - 0x14, -1);
    int32_t lines = 1;

    while (fit >= 1 && fit < static_cast<int32_t>(size - 1))
    {
        line += fit + 1;
        lines++;
        size = std::strlen(reinterpret_cast<char*>(line)) + 1;
        fit = medBlueFont->charactersToWidth(line, width() - 0x14, -1);
    }

    numMiddlePieces = (lines + 1) / 2;
    resize(width(), middlePiece->height() * ((lines + 1) / 2) + bottomPiece->height() + topPiece->height());
    setTwoButton(twoButton);
}

auto ReusableDialog::setTwoButton(int twoButtons) -> void
{
    twoButton = twoButtons;
    const int32_t buttonY = height() - 0x17;

    if (twoButton != 0)
    {
        cancelButton->ShowGUIWindow(-1);
        cancelButton->moveTo(0x68, buttonY, 0);
        okButton->moveTo(0x23, buttonY, 0);
        return;
    }

    cancelButton->ShowGUIWindow(0);
    okButton->moveTo(0x68, buttonY, 0);
}

// RefitDialog

auto RefitDialog::setText(char* newText) -> void
{
    if (text != nullptr)
    {
        logFree(text);
    }

    text = static_cast<char*>(logAlloc(static_cast<uint32_t>(std::strlen(newText) + 1)));
    std::strcpy(text, newText);
    // The text is a comma-separated list, one item per line, between six lines of framing text.
    int32_t lines = 7;
    numItems = 0;

    for (char* comma = std::strchr(text, ','); comma != nullptr; comma = std::strchr(text, ','))
    {
        lines++;
        *comma = '.';
    }

    numMiddlePieces = lines / 2;
    std::strcpy(text, newText);
    numItems = lines - 6;
    resize(width(), middlePiece->height() * numMiddlePieces + bottomPiece->height() + topPiece->height());
    ReusableDialog::setTwoButton(twoButton);
    drawn = 0;
}

auto RefitDialog::draw() -> void
{
    int32_t pieceY = 0;

    if (topPiece != nullptr)
    {
        topPiece->copyTo(lport()->frame(), 0, 0, -1);
        pieceY = topPiece->height();
    }

    for (int32_t i = 0; i < numMiddlePieces; i++)
    {
        if (middlePiece != nullptr)
        {
            middlePiece->copyTo(lport()->frame(), 0, pieceY, -1);
            pieceY += middlePiece->height();
        }
    }

    if (bottomPiece != nullptr)
    {
        bottomPiece->copyTo(lport()->frame(), 0, pieceY, -1);
    }

    char message[264];
    cLoadString(thisInstance, 0x54, message, 0xfe);
    int32_t lineY = wrapText(message, topPiece->height() + 2);
    lineY += medBlueFont->height() + 3;

    if (text != nullptr)
    {
        // One item per line, cut at the commas (in a copy: the original cut the text itself, once).
        std::string items(text);
        size_t item = 0;

        for (int32_t i = numItems; i > 0; i--)
        {
            const size_t comma = items.find(',', item);

            if (comma != std::string::npos)
            {
                items[comma] = '\0';
            }

            medBlueFont->writeString(ownPort->frame(), 0x14, lineY, reinterpret_cast<uint8_t*>(items.data() + item),
                                     -1);
            lineY += medBlueFont->height() + 3;

            if (comma != std::string::npos)
            {
                item = comma + 1;
            }
        }
    }

    const int32_t fontHeight = medBlueFont->height();
    cLoadString(thisInstance, 0x62, message, 0xfe);
    wrapText(message, lineY + 3 + fontHeight);

    for (int32_t i = 0; i < numChildren; i++)
    {
        DrawChild(childList[i]);
    }
}

auto RefitDialog::Refresh() -> void
{
    if (drawn != 0)
    {
        return;
    }

    if (text != nullptr)
    {
        drawn = -1;
    }

    ReusableDialog::Refresh();
}

auto RefitDialog::wrapText(char* string, int32_t yPos) -> int32_t
{
    auto* line = reinterpret_cast<uint8_t*>(string);
    const int32_t length = static_cast<int32_t>(std::strlen(string));
    int32_t fit = medBlueFont->charactersToWidth(line, width() - 0x14, -1);

    if (fit == length)
    {
        medBlueFont->writeString(ownPort->frame(), 0xc, yPos, line, -1);
        return yPos;
    }
    while (fit > 0)
    {
        uint8_t* end = line + fit;
        // Port fix: the last piece ends at the terminator, which the original overwrote with a space before reading
        // on past it (OB-075).
        const bool last = *end == 0;
        *end = 0;
        medBlueFont->writeString(ownPort->frame(), 0xc, yPos, line, -1);
        yPos += medBlueFont->height() + 3;

        if (last)
        {
            break;
        }

        *end = ' ';
        line = end + 1;
        fit = medBlueFont->charactersToWidth(line, width() - 0x14, -1);
    }

    return yPos;
}

auto RefitDialog::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    drawn = 0;
    return ReusableDialog::init(xPos, yPos, width, height, name);
}
