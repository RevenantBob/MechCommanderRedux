#include "stdafx.h"
#include "logistics/loggen.h"
#include "color/color.h"
#include "gui/afont.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "linkup/linkedlist.hpp"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/logsession.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "platform/MCFileSystem.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

std::type_identity_t<char[256]> MCSplashScreen::genericPortFileName{};
std::type_identity_t<lPort*> MCSplashScreen::genericPort{};
std::type_identity_t<int32_t> MCSplashScreen::instanceCount{};
_GUID deletedSessions[50] = {};
int32_t nextDeletedSession = 0;
char* EmptyFile = nullptr;

namespace
{
    /// <summary>The width of a scroll pane's slider column.</summary>
    constexpr int32_t SliderWidth = 0xd;

    /// <summary>The size of an <see cref="lScrollTextObject"/>'s text buffer (one more byte is allocated).</summary>
    constexpr int32_t TextBufferSize = 0x1000;

    void* logAlloc(uint32_t size)
    {
        return globalLogPtr->logisticsHeap->malloc(size);
    }

    void logFree(void* block)
    {
        globalLogPtr->logisticsHeap->free(block);
    }

    /// <summary>Frees a logistics port (destroy, then delete) and clears the pointer.</summary>
    void freePort(lPort*& port)
    {
        if (port != nullptr)
        {
            port->destroy();
            delete port;
            port = nullptr;
        }
    }

    /// <summary>Loads <paramref name="fileName"/> into a new port at <paramref name="port"/>; on failure frees it.</summary>
    int32_t loadPicture(lPort*& port, char* fileName)
    {
        freePort(port);
        port = new lPort;
        const int32_t result = port->init(fileName);

        if (result != 0)
        {
            freePort(port);
        }

        return result;
    }

    /// <summary>
    /// The row of <c>fonts</c> a scroll text line is drawn in, from its colour byte (0x0b..0xf2 are the palette
    /// indices of the font colours; anything else is row 7).
    /// </summary>
    int32_t fontRowForColor(uint8_t color)
    {
        switch (color)
        {
            case 0x0b:
                return 3;
            case 0x0c:
                return 4;
            case 0x10:
                return 0;
            case 0x19:
                return 5;
            case 0x1f:
                return 6;
            case 0xef:
                return 1;
            case 0xf2:
                return 2;
            default:
                return 7;
        }
    }

    /// <summary>A new text entry field as the generic screens make them (black text on colour 0x1f).</summary>
    lTextObject* makeTextEntry(int32_t xPos, int32_t yPos, int32_t width, int32_t height)
    {
        auto* entry = new lTextObject;
        entry->lObject::init(xPos, yPos, width, height, nullptr, nullptr);
        entry->cursorPos = 0;
        entry->cursorPixel = 0;
        entry->backgroundColor = 0x1f;
        entry->font = lgBlackFont;
        VFX_pane_wipe(entry->lport()->frame(), 0x1f);
        return entry;
    }

    /// <summary>
    /// Reads a button's four pictures (NormalArt was read already into <paramref name="art"/>), its sounds and its
    /// callback number. "NONE" skips a picture.
    /// </summary>
    /// <returns>Whether a Callback entry was found (in <paramref name="callback"/>).</returns>
    bool readButton(FitIniFile* file, lButton* button, char* art, int32_t& callback)
    {
        int32_t result = 0;

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->setUpPicture(art);
            Assert(result == 0, result, " Couldn't locate button upPicture image ", nullptr);
        }

        result = file->readIdString("GreyArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find gray button art in Generic Screen ", nullptr);

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->setGrayPicture(art);
            Assert(result == 0, result, " Couldn't locate button grayPicture image ", nullptr);
        }

        result = file->readIdString("PressArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find down button art in Generic Screen ", nullptr);

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->setDownPicture(art);
            Assert(result == 0, result, " Couldn't locate button downPicture image ", nullptr);
        }

        result = file->readIdString("OverArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find button rollover art in Generic Screen ", nullptr);

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->setOverPicture(art);
            // The original reports the down picture's message here too.
            Assert(result == 0, result, " Couldn't locate button downPicture image ", nullptr);
        }

        int32_t sound = 0;
        result = file->readIdLong("OverSFX", sound);
        Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ", nullptr);
        button->overSound = static_cast<uint32_t>(sound);
        result = file->readIdLong("PressSFX", sound);
        Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ", nullptr);
        button->pressSound = static_cast<uint32_t>(sound);
        return file->readIdLong("Callback", callback) == 0;
    }

    /// <summary>
    /// Sets a generic screen button's callback (numbers 0..11, shared by both screen kinds). The load/save, delete
    /// and cancel buttons are remembered by the screen; the first two start disabled.
    /// </summary>
    /// <returns>Whether the number was one of these.</returns>
    bool setScreenCallback(GenericScreen* screen, lButton* button, int32_t callback)
    {
        void (*exec)() = nullptr;

        switch (callback)
        {
            case 0:
                exec = NewMCXCampaign;
                break;
            case 1:
                exec = SaveScreen;
                break;
            case 2:
                exec = LoadScreen;
                break;
            case 3:
                exec = ShowPreferences;
                break;
            case 4:
                exec = ConnectScreen;
                break;
            case 5:
                exec = ReplayCinema;
                break;
            case 6:
                exec = ReturnToGame;
                break;
            case 7:
                exec = GameOverMan;
                break;
            case 8:
            case 9:
            {
                button->callback()->setExec(callback == 8 ? LoadGame : SaveGame);
                screen->loadSaveButton = button;
                button->disabled = -1;
                button->draw();
                return true;
            }
            case 10:
            {
                button->callback()->setExec(DeleteGame);
                screen->deleteButton = button;
                button->disabled = -1;
                button->draw();
                return true;
            }
            case 11:
            {
                button->callback()->setExec(Cancel);
                screen->cancelButton = button;
                return true;
            }
            default:
                return false;
        }

        button->callback()->setExec(exec);
        return true;
    }

    /// <summary>Makes a file pane element (type 5) and reads its SavePane flag.</summary>
    FileScrollPane* makeFilePane(FitIniFile* file, int32_t xPos, int32_t yPos, int32_t width, int32_t height)
    {
        auto* pane = new FileScrollPane;
        // The inlined constructors each clear the scroll pane.
        pane->ScrollPane::init();
        pane->init(xPos, yPos, width, height);

        if (file->readIdBoolean("SavePane", pane->savePane) != 0)
        {
            pane->savePane = 0;
        }

        pane->setStartDirectory(savePath);
        return pane;
    }

    /// <summary>Reads the Element block header: type, rectangle and NormalArt.</summary>
    void readElement(FitIniFile* file, int32_t index, int32_t& type, int32_t& left, int32_t& top, int32_t& width,
                     int32_t& height, char* art)
    {
        char blockName[20];
        std::snprintf(blockName, sizeof(blockName), "Element%d", index);
        int32_t result = file->seekBlock(blockName);
        Assert(result == 0, result, " Could not Find Element block in Generic Screen ", nullptr);
        type = -1;
        result = file->readIdLong("ElementType", type);
        Assert(result == 0, result, " Could not Find Element Type in Generic Screen ", nullptr);
        result = file->readIdLong("Left", left);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ", nullptr);
        result = file->readIdLong("Top", top);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ", nullptr);
        result = file->readIdLong("Width", width);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ", nullptr);
        result = file->readIdLong("Height", height);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ", nullptr);
        result = file->readIdString("NormalArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find Element Art in Generic Screen ", nullptr);
    }

    /// <summary>Reads the Elements block and allocates the element array.</summary>
    void readElementCount(FitIniFile* file, GenericScreen* screen)
    {
        int32_t result = file->seekBlock("Elements");
        Assert(result == 0, result, " Could not Find Elements block in Generic Screen ", nullptr);
        result = file->readIdLong("NumElements", screen->numElements);
        Assert(result == 0, result, " Could not Find Elements number in Generic Screen ", nullptr);
        // Port fix: sized by the port's pointer size (the original's count * 4 overran the array on 64-bit).
        screen->elements = static_cast<aObject**>(
            logAlloc(static_cast<uint32_t>(sizeof(aObject*) * static_cast<size_t>(screen->numElements))));
        Assert(screen->elements != nullptr, 0, " No RAM for Generic Screen Elements ", nullptr);

        // Port fix: element types 2 and 3 leave their slot unset, which GenericScreen::destroy then deleted.
        if (screen->elements != nullptr)
        {
            std::memset(screen->elements, 0, sizeof(aObject*) * static_cast<size_t>(screen->numElements));
        }
    }
}

// lCallback

auto lCallback::operator new(size_t size) noexcept -> void*
{
    return logAlloc(static_cast<uint32_t>(size));
}

auto lCallback::operator delete(void* ptr) -> void
{
    logFree(ptr);
}

// lButton

lButton::~lButton()
{
    lButton::destroy();
}

auto lButton::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = lObject::init(xPos, yPos, width, height, name, nullptr);

    if (result != 0)
    {
        return result;
    }

    buttonCallback = new lCallback;
    upPicture = nullptr;
    downPicture = nullptr;
    grayPicture = nullptr;
    overPicture = nullptr;
    disabled = 0;
    overState = 0;
    pressed = 0;
    backgroundColor = 0;
    pressSound = 0xf;
    overSound = 0xffffffff;
    VFX_pane_wipe(ownPort->frame(), 0);
    return 0;
}

auto lButton::destroy() -> void
{
    freePort(upPicture);
    freePort(downPicture);
    freePort(grayPicture);
    freePort(overPicture);

    if (buttonCallback != nullptr)
    {
        buttonCallback->destroy();
        delete buttonCallback;
        buttonCallback = nullptr;
    }

    lObject::destroy();
}

auto lButton::setUpPicture(char* fileName) -> int32_t
{
    const int32_t result = loadPicture(upPicture, fileName);

    if (result == 0)
    {
        // The button takes the picture's size.
        backgroundColor = 0xff;
        resize(upPicture->width(), upPicture->height());
    }

    return result;
}

auto lButton::setOverPicture(char* fileName) -> int32_t
{
    return loadPicture(overPicture, fileName);
}

auto lButton::setGrayPicture(char* fileName) -> int32_t
{
    return loadPicture(grayPicture, fileName);
}

auto lButton::setDownPicture(char* fileName) -> int32_t
{
    return loadPicture(downPicture, fileName);
}

auto lButton::handleEvent(aEvent* event) -> void
{
    if (event->type == 1)
    {
        if (disabled == 0)
        {
            pressed = -1;
            soundSystem->playDigitalSample(pressSound, 1, nullptr, 0, 0);
            draw();
            UpdateDisplay(0, 0, 0, 0, 0);
            buttonCallback->execute();
        }
        else
        {
            soundSystem->playDigitalSample(0x33, 1, nullptr, 0, 0);
        }
    }

    if (disabled == 0 && eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

auto lButton::draw() -> void
{
    lPort* picture = nullptr;

    if (disabled != 0)
    {
        picture = grayPicture;
    }
    else if (pressed == 0 && (application->grabbedObject() != this || application->currentObject() != this))
    {
        // Up, or rolled over; without the picture the button is its background colour.
        picture = overState == 0 ? upPicture : overPicture;

        if (picture == nullptr)
        {
            VFX_pane_wipe(ownPort->frame(), static_cast<uint32_t>(backgroundColor));
            lObject::draw();
            return;
        }
    }
    else
    {
        // The press shows once.
        picture = downPicture;
        pressed = 0;
    }

    if (picture != nullptr)
    {
        picture->copyTo(ownPort->frame(), 0, 0, 0);
        lObject::draw();
        return;
    }

    VFX_pane_wipe(ownPort->frame(), static_cast<uint32_t>(backgroundColor));
    lObject::draw();
}

auto lButton::enter() -> void
{
    if (disabled == 0)
    {
        overState = -1;
        soundSystem->playDigitalSample(overSound, 1, nullptr, 0, 0);
        draw();
    }
}

// lTextObject

lTextObject::~lTextObject()
{
    lTextObject::destroy();
}

auto lTextObject::destroy() -> void
{
    lObject::destroy();
    bufferSize = 0;
    logFree(buffer);
    buffer = nullptr;
    logFree(originalBuffer);
    originalBuffer = nullptr;
}

auto lTextObject::draw() -> void
{
    VFX_pane_wipe(ownPort->frame(), static_cast<uint32_t>(backgroundColor));
    cursorOn = -1;
    font->writeString(ownPort->frame(), 1, 1, reinterpret_cast<uint8_t*>(buffer), -1);
}

auto lTextObject::display() -> void
{
    if (cursorPos > -1 && cursorPos < bufferSize)
    {
        const uint32_t color = cursorOn == 0 ? 0x1f : 0x10;
        VFX_line_draw(ownPort->frame(), cursorPixel, 0, cursorPixel, height(), LD_DRAW, color);
    }

    lObject::display();
}

auto lTextObject::setCursorPos(int32_t pos) -> void
{
    cursorPos = pos;
    // The cursor sits one pixel after the text up to it.
    const char saved = buffer[pos];
    buffer[pos] = 0;
    const int32_t textWidth = font->width(reinterpret_cast<uint8_t*>(buffer));
    buffer[pos] = saved;
    cursorPixel = textWidth + 1;
}

auto lTextObject::isValid(char key) -> int
{
    switch (inputType)
    {
        case INPUT_TEXT:
        {
            if (key >= ' ' && key < 0x7f)
            {
                return -1;
            }

            return 0;
        }
        case INPUT_DIGITS:
        {
            if (key >= '0' && key <= '9')
            {
                return -1;
            }

            return 0;
        }
        case INPUT_ANY:
            return -1;
        case INPUT_PORT:
        {
            if (key >= '2' && key <= '6')
            {
                return -1;
            }

            return 0;
        }
        default:
            return 0;
    }
}

auto lTextObject::handleEvent(aEvent* event) -> void
{
    // Tells the parent the entry is finished (Enter, or focus moving on).
    auto sendDone = [this]()
    {
        aEvent done;
        done.clear();
        done.type = 0x1e;
        done.data = 5;
        parent->handleEvent(&done);
    };

    // Clears the whole buffer.
    auto clearBuffer = [this]()
    {
        std::memset(buffer, 0, static_cast<size_t>(textLength));
        textLength = 0;
    };

    switch (event->type)
    {
        case 1:
            application->setText(this);
            break;
        case 10:
        {
            if (inputType == INPUT_NONE)
            {
                break;
            }

            const uint8_t key = event->key;

            if (key == 8)
            {
                if (textLength != 0)
                {
                    // Backspace on untouched text clears all of it.
                    int32_t newPos = 0;

                    if (std::strcmp(buffer, originalBuffer) == 0)
                    {
                        clearBuffer();
                    }
                    else
                    {
                        buffer[textLength - 1] = 0;
                        newPos = cursorPos - 1;
                        textLength--;
                    }

                    setCursorPos(newPos);
                    draw();
                }
            }
            else if (key == 0x0d)
            {
                sendDone();
            }
            else if (key == 0x1b)
            {
                Cancel();
            }
            else if (textLength + 1 < bufferSize && isValid(static_cast<char>(key)) != 0)
            {
                buffer[textLength++] = static_cast<char>(key);
                setCursorPos(cursorPos + 1);
                draw();
            }
            break;
        }

        case 0x10:
            sendDone();
            break;
        case 0x13:
        {
            // Timer 0: the cursor blink.
            if (event->data == 0)
            {
                cursorOn = cursorOn == 0 ? 1 : 0;
            }
            break;
        }
        case 0x1e:
        {
            if (event->data == 7)
            {
                // Focus: start the blink, and clear an empty-slot name so the player can type one.
                application->AddTimer(this, 0, static_cast<int32_t>(MCPort::CaretBlinkTime()), 0, 0, 0);

                if (clearEmptyOnFocus != 0 && std::strcmp(buffer, EmptyFile) == 0)
                {
                    clearBuffer();
                    setCursorPos(0);
                    draw();
                }
            }
            else if (event->data == 8)
            {
                application->RemoveTimer(this, 0);
                cursorOn = -1;
            }
            break;
        }
        default:
            break;
    }

    aObject::handleEvent(event);
}

auto lTextObject::initBuffer(int32_t size, int32_t type) -> void
{
    if (bufferSize != 0)
    {
        logFree(buffer);
        buffer = nullptr;
        logFree(originalBuffer);
        originalBuffer = nullptr;
        bufferSize = 0;
    }

    if (size != 0)
    {
        bufferSize = size;
        buffer = static_cast<char*>(logAlloc(static_cast<uint32_t>(size)));
        originalBuffer = static_cast<char*>(logAlloc(static_cast<uint32_t>(size)));
    }

    std::memset(buffer, 0, static_cast<size_t>(bufferSize));
    std::memset(originalBuffer, 0, static_cast<size_t>(bufferSize));
    inputType = type;
}

auto lTextObject::setStringBuffer(char* text) -> int32_t
{
    int32_t result = 0;
    int32_t length = static_cast<int32_t>(std::strlen(text));

    if (length < bufferSize)
    {
        std::strcpy(buffer, text);
        std::strcpy(originalBuffer, text);
        textLength = length;
    }
    else
    {
        // Too long: cut to the buffer (strncpy writes no terminator; the buffer's last byte stays 0). The length
        // is left as it was.
        std::strncpy(buffer, text, static_cast<size_t>(bufferSize - 1));
        std::strncpy(originalBuffer, text, static_cast<size_t>(bufferSize - 1));
        length = bufferSize - 1;
        result = -1;
    }

    setCursorPos(length);
    draw();
    return result;
}

// FileScrollPane

FileScrollPane::~FileScrollPane()
{
    FileScrollPane::destroy();
}

auto FileScrollPane::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    if (lgWhiteFont != nullptr)
    {
        lineHeight = lgWhiteFont->height() + 1;
    }

    ScrollPane::init(width, height, xPos, yPos, static_cast<char*>(nullptr));

    // The splash screens' own slider art over the scroll pane's.
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashscroll.tga", artPath);
    auto* art = new lPort;
    art->init(fileName);
    const int32_t numTiles = height / art->height() - 1;

    for (int32_t i = 0; i < numTiles; i++)
    {
        art->copyTo(sliderPort->frame(), 0, art->height() * i + 1, -1);
    }

    art->destroy();
    delete art;

    upArrowPort = new lPort;
    downArrowPort = new lPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashsupbup.tga", artPath);
    upArrowPort->init(fileName);
    upArrowPort->copyTo(sliderPort->frame(), 0, 0, -1);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashsdnbup.tga", artPath);
    downArrowPort->init(fileName);
    downArrowPort->copyTo(sliderPort->frame(), 0, height - 0xf, -1);

    // The column headers (operation, mission, resource points). Each is added and removed again straight away:
    // they are drawn by the pane itself, not as children.
    static constexpr int32_t headerRects[3][4] = {
        {0, 0xa5, 0x37, 0xe}, {0x49, 0xa5, 0x37, 0xe}, {0x90, 0xa5, 0x60, 0xe}};

    for (int32_t i = 0; i < 3; i++)
    {
        auto* header = new lObject;
        columnHeaders[i] = header;
        header->init(headerRects[i][0], headerRects[i][1], headerRects[i][2], headerRects[i][3], nullptr, nullptr);
        addChild(header);
        removeChild(columnHeaders[i]);
    }

    // The slider's clean track, to erase the slider with.
    std::memcpy(trackImage, sliderPort->frame()->window->buffer, static_cast<size_t>(height * SliderWidth));
}

auto FileScrollPane::destroy() -> void
{
    logFree(startDirectory);
    startDirectory = nullptr;

    for (int32_t i = 0; i < numFiles; i++)
    {
        logFree(fileNames[i]);
        fileNames[i] = nullptr;
    }

    logFree(fileNames);
    fileNames = nullptr;
    numFiles = 0;

    if (nameEntry != nullptr)
    {
        nameEntry->destroy();
        delete nameEntry;
        nameEntry = nullptr;
    }

    for (lObject*& header : columnHeaders)
    {
        if (header != nullptr)
        {
            delete header;
            header = nullptr;
        }
    }

    if (fileOperations != nullptr)
    {
        logFree(fileOperations);
        fileOperations = nullptr;
    }

    if (fileMissions != nullptr)
    {
        logFree(fileMissions);
        fileMissions = nullptr;
    }

    if (fileResourcePoints != nullptr)
    {
        logFree(fileResourcePoints);
        fileResourcePoints = nullptr;
    }

    freePort(upArrowPort);
    freePort(downArrowPort);
    ScrollPane::destroy();
}

auto FileScrollPane::draw() -> void
{
    if (IsShowing() == 0)
    {
        return;
    }

    if (parent != nullptr)
    {
        // The selected save's operation, mission and resource points (none for the multiplayer list).
        int32_t operation = -1;
        int32_t mission = -1;
        uint32_t resourcePoints = 0xffffffff;

        if (selectedFile >= 0 && multiplayer == 0)
        {
            operation = fileOperations[selectedFile];
            mission = fileMissions[selectedFile];
            resourcePoints = fileResourcePoints[selectedFile];
        }

        for (lObject* header : columnHeaders)
        {
            VFX_pane_wipe(header->lport()->frame(), 0x10);
        }

        if (operation > 0 && multiplayer == 0)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%i", operation);
            lgWhiteFont->writeString(columnHeaders[0]->lport()->frame(), 2, 2, reinterpret_cast<uint8_t*>(text), -1);
            std::snprintf(text, sizeof(text), "%i", mission);
            lgWhiteFont->writeString(columnHeaders[1]->lport()->frame(), 2, 2, reinterpret_cast<uint8_t*>(text), -1);
            std::snprintf(text, sizeof(text), "%i", static_cast<int32_t>(resourcePoints));
            lgWhiteFont->writeString(columnHeaders[2]->lport()->frame(), 2, 2, reinterpret_cast<uint8_t*>(text), -1);
        }
    }

    drawFiles();
    upArrowPort->copyTo(sliderPort->frame(), 0, 0, -1);
    downArrowPort->copyTo(sliderPort->frame(), 0, height() - 0xf, -1);
    ScrollPane::draw();
}

auto FileScrollPane::display() -> void
{
    if (IsShowing() != 0)
    {
        if (backgroundCopy != nullptr)
        {
            backgroundCopy->copyTo(framePane, 0, 0, -1);
        }

        contentPort->copyTo(framePane, 0, static_cast<int32_t>(-(static_cast<double>(scrollPos) * scrollUnit)), -1);
        sliderPort->copyTo(framePane, winWidth - SliderWidth, 0, -1);
    }

    for (lObject* header : columnHeaders)
    {
        header->lport()->copyTo(header->frame(), 0, 0, 0);
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }
}

auto FileScrollPane::handleEvent(aEvent* event) -> void
{
    ScrollPane::handleEvent(event);

    switch (event->type)
    {
        case 1:
        {
            upArrowPort->copyTo(sliderPort->frame(), 0, 0, -1);
            downArrowPort->copyTo(sliderPort->frame(), 0, height() - 0xf, -1);

            if (nameEntry != nullptr && nameEntry->parent == this)
            {
                nameEntry->destroy();
            }

            const int32_t file = getFileAtPosition(event->x - globalX(), event->y - globalY());

            if (file < 0)
            {
                return;
            }

            if (file != selectedFile)
            {
                setSelectedFile(file);
            }

            draw();

            if (savePane == 0)
            {
                return;
            }

            // Saving: an entry field over the clicked name.
            if (nameEntry == nullptr)
            {
                nameEntry = new lTextObject;
            }
            else
            {
                if (nameEntry->parent != nullptr)
                {
                    return;
                }

                nameEntry->destroy();
            }

            int32_t entryY = lineHeight * file - getScrollOffset() - 1;
            int32_t entryHeight = lineHeight;

            if (entryY < 0)
            {
                entryY = 0;
                entryHeight--;
            }

            lTextObject* entry = nameEntry;
            entry->lObject::init(1, entryY, width() - 0x12, entryHeight, nullptr, nullptr);
            entry->cursorPos = 0;
            entry->cursorPixel = 0;
            entry->backgroundColor = 0x1f;
            entry->font = lgBlackFont;
            VFX_pane_wipe(entry->lport()->frame(), 0x1f);
            entry->clearEmptyOnFocus = -1;
            entry->initBuffer(0x20, lTextObject::INPUT_ANY);
            entry->setStringBuffer(fileNames[selectedFile]);
            application->setText(entry);
            addChild(entry);
            break;
        }

        case 0x10:
        {
            // A double click on the selected file presses the screen's load/save button.
            const int32_t file = getFileAtPosition(event->x - globalX(), event->y - globalY());

            if (file > -1 && file == selectedFile)
            {
                lButton* button = static_cast<GenericScreen*>(parent)->loadSaveButton;
                soundSystem->playDigitalSample(button->pressSound, 1, nullptr, 0, 0);
                button->callback()->execute();
                return;
            }
            break;
        }

        case 0x13:
        {
            upArrowPort->copyTo(sliderPort->frame(), 0, 0, -1);
            downArrowPort->copyTo(sliderPort->frame(), 0, height() - 0xf, -1);
            return;
        }
        case 0x1e:
        {
            parent->handleEvent(event);
            return;
        }
        default:
            break;
    }
}

auto FileScrollPane::setUpSlider() -> void
{
    const int32_t paneHeight = winHeight;

    if (contentPort->height() <= paneHeight)
    {
        sliderHeight = 0;
        return;
    }

    if (sliderImage != nullptr)
    {
        logFree(sliderImage);
    }

    const float paneHeightF = static_cast<float>(paneHeight);
    sliderHeight = static_cast<int32_t>(static_cast<double>(paneHeightF) / contentPort->height() * (paneHeight - 0x20));
    sliderPos = 0x10;

    if (sliderHeight < 3)
    {
        sliderHeight = 3;
    }

    const uint32_t size = static_cast<uint32_t>(sliderHeight * SliderWidth);
    sliderImageSize = size;
    auto* image = static_cast<uint8_t*>(logAlloc(size));
    sliderImage = image;
    // As ScrollPane's slider, with the splash screens' edge colour (0xc0).
    static constexpr uint8_t sliderRow[SliderWidth] = {0xc0, 0x10, 0x1c, 0x1a, 0x1a, 0x1a, 0x1a,
                                                       0x1a, 0x1a, 0x1a, 0x17, 0x10, 0xc0};

    for (int32_t row = 0; row < sliderHeight; row++)
    {
        std::memcpy(image + row * SliderWidth, sliderRow, SliderWidth);
    }

    std::memset(image + 3, 0x1c, 8);
    std::memset(image + size - 11, 0x17, 9);
}

auto FileScrollPane::getFileAtPosition(int32_t xPos, int32_t yPos) -> int32_t
{
    const int32_t contentY = getScrollOffset() + yPos;

    for (int32_t i = 0; i < numFiles; i++)
    {
        const tagRECT row = {1, lineHeight * i, width() - SliderWidth, (i + 1) * lineHeight};

        if (PtInRect(&row, tagPOINT{xPos, contentY}))
        {
            return i;
        }
    }

    return -1;
}

auto FileScrollPane::setStartDirectory(char* directory) -> void
{
    startDirectory = static_cast<char*>(logAlloc(static_cast<uint32_t>(std::strlen(directory) + 1)));
    std::sprintf(startDirectory, "%s", directory);
    getAllFiles(const_cast<char*>(multiplayer != 0 ? ".mpk" : ".sav"), true);
}

auto FileScrollPane::drawFiles() -> void
{
    int32_t contentHeight = numFiles * lineHeight;

    if (contentHeight < height())
    {
        contentHeight = height();
    }

    if (contentPort->height() != contentHeight)
    {
        lPort* port = contentPort;
        port->resize(width() - 0x12, contentHeight);
        setDisplayPort(port, 0, -1);
    }

    lPort* port = contentPort;
    VFX_pane_wipe(port->frame(), 0x10);

    for (int32_t i = 0; i < numFiles; i++)
    {
        if (i == selectedFile)
        {
            _pane box = *ownPort->frame();
            box.x0 = 1;
            box.x1 = width() - 0x12;
            const int32_t rowY = lineHeight * i;
            box.y0 = rowY - 1;
            box.y1 = (i + 1) * lineHeight - 2;
            VFX_pane_wipe(&box, 0x14);
            port = contentPort;
            lgWhiteFont->writeString(port->frame(), 1, rowY, reinterpret_cast<uint8_t*>(fileNames[i]), -1);
        }
        else
        {
            lgGreyFont->writeString(port->frame(), 1, i * lineHeight, reinterpret_cast<uint8_t*>(fileNames[i]), -1);
        }
    }
}

auto FileScrollPane::getAllFiles(char* extension, bool sort) -> void
{
    FitIniFile masterFiles[2];
    FullPathFileName pattern;
    FullPathFileName path;
    pattern.init(startDirectory, "*", extension);
    const std::vector<std::string> found = MCFileSystem::FindFiles(static_cast<char*>(pattern));

    if (numFiles != 0 && fileNames != nullptr)
    {
        for (int32_t i = 0; i < numFiles; i++)
        {
            if (fileNames[i] != nullptr)
            {
                logFree(fileNames[i]);
            }
        }

        logFree(fileNames);
        fileNames = nullptr;
    }

    if (fileOperations != nullptr)
    {
        logFree(fileOperations);
        fileOperations = nullptr;
    }

    if (fileMissions != nullptr)
    {
        logFree(fileMissions);
        fileMissions = nullptr;
    }

    if (fileResourcePoints != nullptr)
    {
        logFree(fileResourcePoints);
        fileResourcePoints = nullptr;
    }

    // Saving mid-campaign offers a new (empty) slot first.
    const bool newSlot = savePane != 0 && globalLogPtr->currentMission >= 0;
    numFiles = static_cast<int32_t>(found.size()) + (newSlot ? 1 : 0);

    if (numFiles == 0)
    {
        setSelectedFile(-1);
    }
    else
    {
        // Port fix: sized by the port's pointer size (the original: count * 4).
        fileNames = static_cast<char**>(logAlloc(static_cast<uint32_t>(sizeof(char*) * static_cast<size_t>(numFiles))));
        std::memset(fileNames, 0, sizeof(char*) * static_cast<size_t>(numFiles));

        if (multiplayer == 0)
        {
            const uint32_t size = static_cast<uint32_t>(numFiles) << 2;
            fileOperations = static_cast<int32_t*>(logAlloc(size));
            fileMissions = static_cast<int32_t*>(logAlloc(size));
            fileResourcePoints = static_cast<uint32_t*>(logAlloc(size));
        }
    }

    int32_t firstFile = 0;

    if (multiplayer == 0)
    {
        // The operation and mission numbers come from the planets' master mission files (Port Arthur, Cermak).
        path.init(missionPath, "mechcmdr1", ".fit");
        int32_t result = masterFiles[0].open(static_cast<char*>(path), READ, 0x32);
        Assert(result == 0, 0, " could not open Port Arthur master mission file ", nullptr);
        result = masterFiles[0].seekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file", nullptr);
        path.init(missionPath, "xmechcmdr1", ".fit");
        result = masterFiles[1].open(static_cast<char*>(path), READ, 0x32);
        Assert(result == 0, 0, " could not open Cermak master mission file ", nullptr);
        result = masterFiles[1].seekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file", nullptr);

        if (newSlot)
        {
            FitIniFile& master = CurPlanet == 0 ? masterFiles[0] : masterFiles[1];
            // The empty slot's name block is 16 bytes (EmptyFile fits).
            fileNames[0] = static_cast<char*>(logAlloc(0x10));
            std::strcpy(fileNames[0], EmptyFile);
            char key[64];
            int32_t operation = 0;
            int32_t mission = 0;
            std::snprintf(key, sizeof(key), "Scenario%iOperation", globalLogPtr->currentMission);
            result = master.readIdLong(key, operation);
            Assert(result == 0, 0, " could not find operation number in master mission file ", nullptr);
            std::snprintf(key, sizeof(key), "Scenario%iMission", globalLogPtr->currentMission);
            result = master.readIdLong(key, mission);
            Assert(result == 0, 0, " could not find mission number in master mission file ", nullptr);
            fileOperations[0] = operation;
            fileMissions[0] = mission;
            fileResourcePoints[0] = static_cast<uint32_t>(ResourcePoints);
            firstFile = 1;
        }
    }

    int32_t index = firstFile;

    for (const std::string& name : found)
    {
        // The name without its extension.
        const std::string stem = std::filesystem::path(name).stem().string();
        fileNames[index] = static_cast<char*>(logAlloc(static_cast<uint32_t>(stem.size() + 1)));
        std::sprintf(fileNames[index], "%s", stem.c_str());
        path.init(startDirectory, stem.c_str(), extension);

        if (multiplayer == 0)
        {
            PacketFile saveFile;
            FitIniFile saveFit;
            int32_t planet = 0;
            int32_t result = saveFile.open(static_cast<char*>(path), READ, 0x32);
            Assert(result == 0, result, " Could not find save game file ", nullptr);
            result = saveFile.seekPacket(0);
            Assert(result == 0, 0, " could not find packet 0 in save game file ", nullptr);
            result = saveFit.open(&saveFile, static_cast<uint32_t>(saveFile.getPacketSize()), 0x32);
            Assert(result == 0, 0, " could not open save game file ", nullptr);

            if (saveFit.seekBlock("Planet") == 0)
            {
                saveFit.readIdLong("Setting", planet);
            }

            result = saveFit.seekBlock("General");
            Assert(result == 0, 0, " could not find General Block in campaign file ", nullptr);
            int32_t missionNumber = 0;
            result = saveFit.readIdLong("MissionNumber", missionNumber);
            Assert(result == 0, 0, " Could not find MissionNumber in save game file ", nullptr);
            result = saveFit.seekBlock("ResourcePoints");
            Assert(result == 0, 0, " could not find ResourcePoints Block in save game file ", nullptr);
            result = saveFit.readIdULong("numPoints", fileResourcePoints[index]);
            Assert(result == 0, 0, " Could not find numPoints in save game file ", nullptr);
            saveFit.close();
            saveFile.close();
            char key[64];
            int32_t operation = 0;
            int32_t mission = 0;
            std::snprintf(key, sizeof(key), "Scenario%iOperation", missionNumber);
            result = masterFiles[planet].readIdLong(key, operation);
            Assert(result == 0, 0, " could not find operation number in master mission file ", nullptr);
            std::snprintf(key, sizeof(key), "Scenario%iMission", missionNumber);
            result = masterFiles[planet].readIdLong(key, mission);
            Assert(result == 0, 0, " could not find mission number in master mission file ", nullptr);
            fileOperations[index] = operation;
            fileMissions[index] = mission;
        }

        index++;
    }

    masterFiles[0].close();
    masterFiles[1].close();

    if (selectedFile >= numFiles)
    {
        selectedFile = -1;
    }

    if (sort)
    {
        // Sorted by name; the selection follows its file.
        char selectedName[0x800];
        selectedName[0] = 0;

        if (selectedFile != -1)
        {
            std::strcpy(selectedName, fileNames[selectedFile]);
        }

        for (int32_t i = 0; i < numFiles; i++)
        {
            for (int32_t j = i; j < numFiles; j++)
            {
                if (std::strcmp(fileNames[i], fileNames[j]) > 0)
                {
                    std::swap(fileNames[i], fileNames[j]);

                    if (multiplayer == 0)
                    {
                        std::swap(fileOperations[i], fileOperations[j]);
                        std::swap(fileMissions[i], fileMissions[j]);
                        std::swap(fileResourcePoints[i], fileResourcePoints[j]);
                    }
                }
            }
        }

        if (selectedFile != -1)
        {
            for (int32_t i = 0; i < numFiles; i++)
            {
                if (std::strcmp(fileNames[i], selectedName) == 0)
                {
                    selectedFile = i;
                    break;
                }
            }
        }
    }

    drawFiles();
}

auto FileScrollPane::setSelectedFile(int32_t file) -> void
{
    if (nameEntry != nullptr)
    {
        nameEntry->destroy();
    }

    aEvent event;
    event.clear();

    if (file < 0 || file >= numFiles)
    {
        selectedFile = -1;
        drawFiles();

        if (parent != nullptr)
        {
            event.type = 0x1e;
            event.data = 2;
            event.lParam = file;
            parent->handleEvent(&event);
        }

        return;
    }

    // Scroll half a unit at a time until the row is in view.
    while (lineHeight * file - getScrollOffset() < 0 && scrollPos > 0.0f)
    {
        setScrollPos(static_cast<float>(scrollPos - 0.5));
    }
    while (height() < (lineHeight + 1) * file - getScrollOffset() && scrollPos < maxScroll)
    {
        setScrollPos(static_cast<float>(scrollPos + 0.5));
    }

    selectedFile = file;
    drawFiles();
    event.type = 0x1e;
    event.data = 1;
    event.lParam = file;
    parent->handleEvent(&event);
}

auto FileScrollPane::setMultiplayer(int newMultiplayer) -> void
{
    multiplayer = newMultiplayer;
    getAllFiles(const_cast<char*>(newMultiplayer != 0 ? ".mpk" : ".sav"), true);
}

// GenericScreen

GenericScreen::~GenericScreen()
{
    GenericScreen::destroy();
}

auto GenericScreen::getPaletteFromArt(char* fileName) -> uint8_t*
{
    char message[256];
    char path[252];
    File file;
    std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

    if (file.open(path, READ, 0x32) != 0)
    {
        std::snprintf(path, sizeof(path), "%s", fileName);

        if (file.open(path, READ, 0x32) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
            return nullptr;
        }
    }

    const uint32_t size = file.fileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
        return nullptr;
    }

    auto* data = static_cast<uint8_t*>(logAlloc(size));

    if (data == nullptr)
    {
        return nullptr;
    }

    file.read(data, static_cast<int32_t>(size));
    file.close();
    // The TGA's palette (BGR after the 18-byte header), as 6-bit RGB.
    palette = static_cast<uint8_t*>(logAlloc(0x300));
    const uint8_t* source = data + 0x12;

    for (int32_t i = 0; i < 0x100; i++)
    {
        palette[i * 3] = source[i * 3 + 2] >> 2;
        palette[i * 3 + 1] = source[i * 3 + 1] >> 2;
        palette[i * 3 + 2] = source[i * 3] >> 2;
    }

    logFree(data);
    return palette;
}

auto GenericScreen::init(FitIniFile* screenFile) -> int32_t
{
    readElementCount(screenFile, this);

    for (int32_t i = 0; i < numElements; i++)
    {
        int32_t type = -1;
        int32_t left = 0;
        int32_t top = 0;
        int32_t width = 0;
        int32_t height = 0;
        char art[256];
        readElement(screenFile, i, type, left, top, width, height, art);

        switch (type)
        {
            case 0:
            {
                // The background: the screen itself.
                Assert(i == 0, i, " Background MUST be first element ", nullptr);
                int useBackPalette = 0;
                int32_t result = screenFile->readIdBoolean("UseBackPalette", useBackPalette);
                Assert(result == 0, result, " Could not find UseBackPalette for background Generic Screen", nullptr);

                if (useBackPalette != 0)
                {
                    palette = getPaletteFromArt(art);
                }

                result = lObject::init(left, top, width, height, nullptr, nullptr);
                Assert(result == 0, result, " Could not start background Generic Screen ", nullptr);
                result = lport()->init(art);
                Assert(result == 0, result, " Could not find background Art in Generic Screen ", nullptr);
                elements[i] = this;
                break;
            }

            case 1:
            {
                auto* button = new lButton;
                const int32_t result = button->init(left, top, width, height, nullptr);
                Assert(result == 0, result, " Couldn't init new button ", nullptr);
                int32_t callback = 0;

                if (readButton(screenFile, button, art, callback) && !setScreenCallback(this, button, callback))
                {
                    Fatal(callback, " Illegal callback value");
                }

                elements[i] = button;
                addChild(button);
                break;
            }

            case 4:
            {
                lTextObject* entry = makeTextEntry(left, top, width, height);
                elements[i] = entry;
                addChild(entry);
                break;
            }

            case 5:
            {
                FileScrollPane* pane = makeFilePane(screenFile, left, top, width, height);
                pane->ShowGUIWindow(-1);
                elements[i] = pane;
                addChild(pane);
                filePane = pane;
                break;
            }

            default:
                break;
        }
    }

    screenWindow->addChild(this);
    ShowGUIWindow(0);
    return 0;
}

auto GenericScreen::destroy() -> void
{
    screenWindow->removeChild(this);

    // Element 0 is the background (the screen itself).
    for (int32_t i = 1; i < numElements; i++)
    {
        aObject* element = elements[i];
        removeChild(element);

        // Port fix: slots of skipped element types are empty (the original deleted whatever the heap held).
        if (element != nullptr)
        {
            element->destroy();
            delete element;
        }

        elements[i] = nullptr;
    }

    logFree(elements);
    elements = nullptr;
    logFree(palette);
    palette = nullptr;
    numElements = 0;
    unknown4C4 = -1;
    numChildren = 0;
    lObject::destroy();
}

auto GenericScreen::handleEvent(aEvent* event) -> void
{
    if (event->type == 9 && event->key == 0x1b)
    {
        Cancel();
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}

auto GenericScreen::ShowGUIWindow(int show) -> void
{
    if (show == 0)
    {
        // Hiding drops a half-typed save name.
        if (filePane != nullptr && filePane->nameEntry != nullptr)
        {
            filePane->nameEntry->destroy();
            showWindow = 0;
            return;
        }

        showWindow = show;
        return;
    }

    if (this == globalLogPtr->mainScreen)
    {
        // The main menu enables what the install and the campaign allow.
        const int noMission = globalLogPtr->currentMission < 0 ? 1 : 0;
        auto* saveButton = static_cast<lButton*>(elements[2]);
        saveButton->disabled = Solo == 0 ? noMission : -1;
        saveButton->draw();
        auto* returnButton = static_cast<lButton*>(elements[7]);
        returnButton->disabled = noMission;
        returnButton->draw();

        char pattern[256];
        std::snprintf(pattern, sizeof(pattern), "%s*.sav", savePath);
        auto* loadButton = static_cast<lButton*>(elements[3]);
        loadButton->disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;
        loadButton->draw();
        std::snprintf(pattern, sizeof(pattern), "%s*.sol", savePath);
        auto* soloLoadButton = static_cast<lButton*>(elements[10]);
        soloLoadButton->disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;
        soloLoadButton->draw();

        // Multiplayer needs 30 MB.
        if (MCPort::TotalPhysicalMemory() < 30000000)
        {
            auto* multiplayerButton = static_cast<lButton*>(elements[5]);
            multiplayerButton->disabled = -1;
            multiplayerButton->draw();
        }

        if (InDemo != 0)
        {
            auto* button = static_cast<lButton*>(elements[4]);
            button->disabled = -1;
            button->draw();
            button = static_cast<lButton*>(elements[5]);
            button->disabled = -1;
            button->draw();
            std::snprintf(pattern, sizeof(pattern), "%sopening.smk", CDmoviePath);
            auto* cinemaButton = static_cast<lButton*>(elements[6]);
            cinemaButton->disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;
            cinemaButton->draw();
        }

        draw();
    }
    else if (this == globalLogPtr->saveScreen || this == globalLogPtr->loadScreen)
    {
        if (filePane->multiplayer != 0)
        {
            filePane->getAllFiles(const_cast<char*>(".mpk"), true);
        }
        else
        {
            filePane->getAllFiles(const_cast<char*>(LoadingSolo == 0 ? ".sav" : ".sol"), true);
        }

        draw();
    }
    else
    {
        draw();
    }

    // The load and save screens (single player) and the preferences keep the current palette.
    lObject* current = globalLogPtr->currentScreen;
    const bool fileScreen = current == globalLogPtr->saveScreen || current == globalLogPtr->loadScreen;

    if ((!fileScreen || globalLogPtr->loadScreen->filePane->multiplayer != 0) && current != globalLogPtr->prefScreen)
    {
        if (palette != nullptr)
        {
            application->activatePalette(palette, 0, 0x100);
            draw();
            showWindow = show;
            return;
        }

        gamePalette->activate(0, 0);
    }

    showWindow = show;
}

// MCSplashScreen

MCSplashScreen::MCSplashScreen()
{
    // The screens share one background port while any exists.
    if (instanceCount == 0 && genericPort == nullptr)
    {
        genericPort = new lPort;
        std::strcpy(genericPortFileName, "None");
    }

    instanceCount++;
}

MCSplashScreen::~MCSplashScreen()
{
    instanceCount--;
    ownPort = nullptr;

    if (instanceCount == 0 && genericPort != nullptr)
    {
        genericPort->destroy();
        delete genericPort;
        genericPort = nullptr;
    }

    if (blocks != nullptr)
    {
        for (int32_t i = 0; i < numBlocks; i++)
        {
            if (blocks[i] != nullptr)
            {
                logFree(blocks[i]);
                blocks[i] = nullptr;
            }
        }

        logFree(blocks);
        blocks = nullptr;
    }

    GenericScreen::destroy();
}

auto MCSplashScreen::init(FitIniFile* screenFile) -> int32_t
{
    readElementCount(screenFile, this);

    // Blocks: which elements show together (showBlock), one byte per element.
    if (screenFile->seekBlock("Blocks") == 0)
    {
        int32_t result = screenFile->readIdLong("Block Count", numBlocks);
        Assert(result == 0, result, " Could not find block count in Generic Screen ", nullptr);
        // Port fix: sized by the port's pointer size (the original: count * 4).
        blocks =
            static_cast<uint8_t**>(logAlloc(static_cast<uint32_t>(sizeof(uint8_t*) * static_cast<size_t>(numBlocks))));
        Assert(blocks != nullptr, 0, " No RAM for Generic Screen Block Array ", nullptr);
        const uint32_t blockSize = static_cast<uint32_t>(numElements);

        for (int32_t i = 0; i < numBlocks; i++)
        {
            blocks[i] = static_cast<uint8_t*>(logAlloc(blockSize));
            char blockName[20];
            std::snprintf(blockName, sizeof(blockName), "Block%d", i);
            screenFile->readIdUCharArray(blockName, blocks[i], blockSize);
        }
    }

    for (int32_t i = 0; i < numElements; i++)
    {
        int32_t type = -1;
        int32_t left = 0;
        int32_t top = 0;
        int32_t width = 0;
        int32_t height = 0;
        char art[256];
        readElement(screenFile, i, type, left, top, width, height, art);
        aObject* element = nullptr;

        switch (type)
        {
            case 0:
            {
                // The background: the shared port, reloaded only when the art changes.
                Assert(i == 0, i, " If there's a background it MUST be the first element ", nullptr);

                if (MCPort::StrICmp(genericPortFileName, art) != 0)
                {
                    std::strcpy(genericPortFileName, art);
                    const int32_t result = genericPort->init(art);
                    Assert(result == 0, result, " Could not find background Art in Generic Screen ", nullptr);
                }

                int useBackPalette = 0;
                int32_t result = screenFile->readIdBoolean("UseBackPalette", useBackPalette);
                Assert(result == 0, result, " Could not find UseBackPalette for background Generic Screen", nullptr);

                if (useBackPalette != 0)
                {
                    palette = getPaletteFromArt(art);
                }

                result = lObject::init(left, top, width, height, nullptr, genericPort);
                Assert(result == 0, result, " Could not start background Generic Screen ", nullptr);
                sharedPort = nullptr;
                ownPort = genericPort;
                elements[i] = this;
                continue;
            }

            case 1:
            {
                auto* button = new lButton;
                const int32_t result = button->init(left, top, width, height, nullptr);
                button->SetTransparent(-1);
                Assert(result == 0, result, " Couldn't init new button ", nullptr);
                int32_t callback = 0;

                if (readButton(screenFile, button, art, callback))
                {
                    void (*exec)() = nullptr;

                    switch (callback)
                    {
                        case 12:
                            exec = ShowModemScreen;
                            break;
                        case 13:
                            exec = ShowSerialScreen;
                            break;
                        case 14:
                            exec = ShowLANScreen;
                            break;
                        case 15:
                            exec = ShowInternet;
                            break;
                        case 16:
                            exec = CancelToConnect;
                            break;
                        case 17:
                            exec = CancelToLAN;
                            break;
                        case 18:
                            exec = HostGame;
                            break;
                        case 19:
                            exec = JoinGame;
                            break;
                        case 20:
                            exec = CreateSession;
                            break;
                        case 21:
                            exec = GO;
                            break;
                        case 22:
                            exec = Leave;
                            break;
                        case 23:
                            exec = WaitForCall;
                            break;
                        case 24:
                            exec = GetNumber;
                            break;
                        case 25:
                            exec = Dial;
                            break;
                        case 35:
                            exec = CancelPrefs;
                            break;
                        case 36:
                            exec = WritePrefs;
                            break;
                        case 37:
                            exec = CreateSerialSession;
                            break;
                        case 38:
                            exec = SerialJoinButtonPressed;
                            break;
                        case 40:
                            exec = NewCampaign;
                            break;
                        case 41:
                            exec = SoloLoadScreen;
                            break;
                        default:
                            break;
                    }

                    if (exec != nullptr)
                    {
                        button->callback()->setExec(exec);
                    }
                    else if (!setScreenCallback(this, button, callback))
                    {
                        Fatal(callback, " Illegal callback value");
                    }
                }

                elements[i] = button;
                addChild(button);
                continue;
            }

            case 4:
            {
                lTextObject* entry = makeTextEntry(left, top, width, height);
                elements[i] = entry;
                entry->ShowGUIWindow(-1);
                addChild(entry);
                continue;
            }

            case 5:
            {
                FileScrollPane* pane = makeFilePane(screenFile, left, top, width, height);
                filePane = pane;
                pane->ShowGUIWindow(-1);
                addChild(pane);
                elements[i] = pane;
                continue;
            }

            case 6:
            {
                // A picture; elements 6 and the others go just behind the rest.
                auto* image = new lObject;
                elements[i] = image;
                image->init(left, top, width, height, nullptr, nullptr);
                image->lport()->init(art);
                addChild(image);
                elements[i]->setDepth(i == 6 ? -0xb : -0xa);
                elements[i]->setEventRoutine(ImageHandleEvent);
                continue;
            }

            case 7:
            {
                auto* text = new lScrollTextObject;
                text->init(left, top, width, height, nullptr);
                int scrolling = 0;
                const int32_t result = screenFile->readIdBoolean("Scrolling", scrolling);
                Assert(result == 0, result, " Couldn't locate Scrolling in textscrollpane ", nullptr);
                text->scrollTab->ShowGUIWindow(scrolling);
                text->scrolling = scrolling;
                text->ShowGUIWindow(-1);
                addChild(text);
                elements[i] = text;
                continue;
            }

            case 8:
            {
                auto* list = new GameList;
                elements[i] = list;
                list->init(left, top, width, height, nullptr);
                element = list;
                break;
            }

            case 9:
            {
                auto* slider = new lSlider;
                elements[i] = slider;
                slider->init(left, top, width, height, nullptr);
                int32_t value = 0;
                int32_t result = screenFile->readIdLong("MinValue", value);
                Assert(result == 0, result, " Couldn't locate min slider value", nullptr);
                slider->minValue = value;
                result = screenFile->readIdLong("MaxValue", value);
                Assert(result == 0, result, " Couldn't locate max slider value", nullptr);
                slider->maxValue = value;
                int32_t callback = 0;
                result = screenFile->readIdLong("Callback", callback);
                Assert(result == 0, result, " Couldn't locate callback value", nullptr);

                switch (callback)
                {
                    case 0x1a:
                        slider->setEventRoutine(SlideScreenBrightness);
                        break;
                    case 0x1b:
                        slider->setEventRoutine(SlideMusicVolume);
                        break;
                    case 0x1c:
                        slider->setEventRoutine(SlideRadioVolume);
                        break;
                    case 0x1d:
                        slider->setEventRoutine(SlideFXVolume);
                        break;
                    default:
                        break;
                }

                element = slider;
                break;
            }

            case 10:
            {
                // A difficulty toggle.
                auto* toggle = new lToolButton;
                elements[i] = toggle;
                toggle->init(left, top, width, height, nullptr);
                toggle->SetTransparent(-1);

                if (MCPort::StrICmp(art, "NONE") == 0)
                {
                    toggle->setBackColor(0xff);
                }
                else
                {
                    const int32_t result = toggle->setUpPicture(art);
                    Assert(result == 0, result, " Couldn't locate button upPicture image ", nullptr);
                }

                // readButton would load NormalArt again; the rest is the same.
                int32_t result = screenFile->readIdString("GreyArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find gray button art in Generic Screen ", nullptr);

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->setGrayPicture(art);
                    Assert(result == 0, result, " Couldn't locate button grayPicture image ", nullptr);
                }

                result = screenFile->readIdString("PressArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find down button art in Generic Screen ", nullptr);

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->setDownPicture(art);
                    Assert(result == 0, result, " Couldn't locate button downPicture image ", nullptr);
                }

                result = screenFile->readIdString("OverArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find button rollover art in Generic Screen ", nullptr);

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->setOverPicture(art);
                    Assert(result == 0, result, " Couldn't locate button downPicture image ", nullptr);
                }

                int32_t sound = 0;
                result = screenFile->readIdLong("OverSFX", sound);
                Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ", nullptr);
                toggle->overSound = static_cast<uint32_t>(sound);
                result = screenFile->readIdLong("PressSFX", sound);
                Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ", nullptr);
                toggle->pressSound = static_cast<uint32_t>(sound);
                int32_t callback = 0;

                if (screenFile->readIdLong("Callback", callback) == 0)
                {
                    switch (callback)
                    {
                        case 0x29:
                            toggle->callback()->setExec(EasyToggle);
                            break;
                        case 0x2a:
                            toggle->callback()->setExec(RegularToggle);
                            break;
                        case 0x2b:
                            toggle->callback()->setExec(HardToggle);
                            break;
                        default:
                            Fatal(callback, " Illegal callback value");
                            break;
                    }
                }

                element = toggle;
                break;
            }

            default:
                continue;
        }

        element->ShowGUIWindow(-1);
        addChild(element);
    }

    screenWindow->addChild(this);
    ShowGUIWindow(0);
    return 0;
}

auto MCSplashScreen::destroy() -> void
{
    if (blocks != nullptr)
    {
        for (int32_t i = 0; i < numBlocks; i++)
        {
            if (blocks[i] != nullptr)
            {
                logFree(blocks[i]);
                blocks[i] = nullptr;
            }
        }

        logFree(blocks);
        blocks = nullptr;
    }

    GenericScreen::destroy();
}

auto MCSplashScreen::ShowGUIWindow(int show) -> void
{
    // The connection screens poll (timer 0, every 2 s) while shown: the connect screen through its element 3.
    if (this == globalLogPtr->connectScreen)
    {
        if (show != 0)
        {
            application->AddTimer(elements[3], 0, 2000, 0, 0, 0);
            GenericScreen::ShowGUIWindow(show);
            return;
        }

        application->RemoveTimer(elements[3], 0);
    }
    else if (this == globalLogPtr->lanScreen)
    {
        if (show != 0)
        {
            application->AddTimer(this, 0, 2000, 0, 0, 0);
            GenericScreen::ShowGUIWindow(show);
            return;
        }

        application->RemoveTimer(this, 0);
    }

    GenericScreen::ShowGUIWindow(show);
}

auto MCSplashScreen::showBlock(int32_t block) -> void
{
    if (block >= numBlocks)
    {
        return;
    }

    const uint8_t* shown = blocks[block];

    for (int32_t i = 1; i < numElements; i++)
    {
        elements[i]->ShowGUIWindow(0);
    }

    for (int32_t i = 1; i < numElements; i++)
    {
        if (shown[i] != 0)
        {
            elements[shown[i]]->ShowGUIWindow(-1);
        }
    }
}

// The scroll text thumb

auto LogPaintScrollTab(aObject* tab) -> void
{
    const int32_t width = tab->width();
    const int32_t height = tab->height();
    _pane* pane = static_cast<lObject*>(tab)->lport()->frame();
    VFX_pane_wipe(pane, 0x1a);
    VFX_line_draw(pane, 0, 0, width - 2, 0, LD_DRAW, 0x1f);
    VFX_line_draw(pane, 0, 0, 0, height - 2, LD_DRAW, 0x1f);
    VFX_line_draw(pane, width - 1, 0, width - 1, height - 1, LD_DRAW, 0x16);
    VFX_line_draw(pane, 0, height - 1, width - 1, height - 1, LD_DRAW, 0x16);
}

auto LogScrollTabHandleEvent(aObject* tab, aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            application->grab(tab);
            tab->startDrag(0, event->y - tab->globalY());
            break;
        }
        case 4:
        {
            application->release();
            tab->stopDrag();
            break;
        }
        case 7:
        {
            if (application->grabbedObject() == nullptr)
            {
                break;
            }

            // Port fix: the original is aScrollTextObject's handler compiled for this thumb: it reads the parent's
            // thumb at aScrollTextObject's offset (+0x4c8, here highlightLine[1]) and calls
            // aScrollTextObject::CalcFirstPixel, which writes over lObject's port pointer (OB-073). The port uses the
            // lScrollTextObject's own thumb and CalcFirstPixel.
            auto* textObject = static_cast<lScrollTextObject*>(tab->parent);
            tab->moveTo(tab->x(), (event->y - tab->parent->y()) - tab->dragStartY(), 0);

            if (tab->y() < 0xf)
            {
                tab->moveTo(tab->x(), 0xf, 0);
            }

            if (tab->y() > (-0x10 - textObject->scrollTab->height()) + textObject->height())
            {
                tab->moveTo(tab->x(), (-0x10 - textObject->scrollTab->height()) + textObject->height(), 0);
            }

            textObject->CalcFirstPixel(tab->y() - 0xf);
            break;
        }

        default:
            break;
    }
}

// lScrollTextObject

lScrollTextObject::~lScrollTextObject()
{
    lScrollTextObject::destroy();
}

auto lScrollTextObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = lObject::init(xPos, yPos, width, height, newText, nullptr);

    if (result != 0)
    {
        return result;
    }

    auto* tab = new lObject;
    scrollTab = tab;

    if (tab == nullptr)
    {
        Fatal(0, "Not enough memory for scrollbar tab.");
    }

    result = tab->init(0, 0, 9, height - 0x1e, nullptr, nullptr);

    if (result != 0)
    {
        return result;
    }

    tab->moveTo(this->width() + 2, 0xf, 0);
    tab->setDepth(100);
    addChild(tab);
    tab->ShowGUIWindow(-1);
    tab->setEventRoutine(LogScrollTabHandleEvent);
    tab->setPaintRoutine(LogPaintScrollTab);
    tab->setDepth(1);

    text = static_cast<char*>(logAlloc(TextBufferSize + 1));

    if (text == nullptr)
    {
        Fatal(0, "Not enough memory for text.");
    }

    std::memset(text, 0, TextBufferSize);
    fontIndex = 0;
    numLines = 0;
    textLength = 0;
    firstPixel = 0;
    scrollTab->ShowGUIWindow(-1);
    scrolling = -1;

    for (int32_t i = 0; i < 4; i++)
    {
        highlightLine[i] = -1;
        highlightColor[i] = 0xff;
    }

    // Text given at init prints in colour 0x1f; without it the object doesn't scroll.
    if (newText != nullptr)
    {
        Print(newText, 0x1f);
    }
    else
    {
        scrolling = 0;
    }

    tabColumn = -1;
    PositionScrollTab();
    return 0;
}

auto lScrollTextObject::destroy() -> void
{
    if (scrollTab != nullptr)
    {
        scrollTab->destroy();
        delete scrollTab;
        scrollTab = nullptr;
    }

    if (text != nullptr)
    {
        logFree(text);
        text = nullptr;
    }

    lObject::destroy();
}

auto lScrollTextObject::draw() -> void
{
    int32_t lineY = 2;
    char* line = text;
    const int32_t lineHeight = fonts[0][fontIndex]->height() + 4;
    VFX_pane_wipe(lport()->frame(), 0x10);

    // Highlighted lines.
    for (int32_t i = 0; i < 4; i++)
    {
        const int32_t start = highlightLine[i];

        if (start != -1 && start < start + 1)
        {
            _pane box = *lport()->frame();
            box.x0 = 0;
            box.y0 = start * lineHeight;
            box.x1 = width();
            box.y1 = (start + 1) * lineHeight;
            VFX_pane_wipe(&box, highlightColor[i]);
        }
    }

    // Lines are (colour byte, text, '\n'). A tab splits a line into two pieces, the second drawn at tabColumn.
    while (line != nullptr && *line != '\0')
    {
        const uint8_t color = static_cast<uint8_t>(*line);
        line++;
        int32_t pieces = 0;
        int32_t lineX = 2;

        if (char* tab = std::strchr(line, '\t'); tab != nullptr)
        {
            if (tabColumn < 0)
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
            fonts[fontRow][fontIndex]->writeStringToNewline(lport()->frame(), lineX, lineY,
                                                            reinterpret_cast<uint8_t*>(line));
            line = std::strchr(line, '\n');

            if (pieces > 0)
            {
                if (line != nullptr)
                {
                    *line = '\t';
                }

                lineX = tabColumn;
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
        childList[i]->draw();
    }
}

auto lScrollTextObject::display() -> void
{
    if (showWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && hideOffset == 0)
    {
        return;
    }

    if (lport() != nullptr)
    {
        VFX_pane_copy(lport()->frame(), 0, 0, framePane, 0, scrolling != 0 ? -firstPixel : 0, -1);
    }

    for (int32_t i = 0; i < numChildren; i++)
    {
        childList[i]->display();
    }
}

auto lScrollTextObject::resize(int32_t width, int32_t height) -> void
{
    const int32_t fontHeight = fonts[0][fontIndex]->height();

    if (width <= 0 || height <= 0 || (width == winWidth && height == winHeight))
    {
        return;
    }

    winWidth = width;
    winHeight = height;
    framePane->x1 = framePane->x0 - 1 + width;
    framePane->y1 = framePane->y0 - 1 + height;
    // A scrolling object's port holds all its lines.
    int32_t portHeight = height;

    if (scrolling != 0)
    {
        const int32_t textHeight = numLines * (fontHeight + 4);

        if (textHeight > height)
        {
            portHeight = textHeight;
        }
    }

    lport()->resize(width, portHeight);
    scrollTab->moveTo(width + 2, scrollTab->y(), 0);
    PositionScrollTab();
}

auto lScrollTextObject::ResetPortSize() -> void
{
    if (scrolling == 0)
    {
        return;
    }

    // The port only grows.
    int32_t portHeight = (fonts[0][fontIndex]->height() + 4) * numLines;

    if (portHeight <= lport()->height())
    {
        portHeight = lport()->height();
    }

    lport()->resize(lport()->width(), portHeight);
}

auto lScrollTextObject::Print(char* line, uint8_t color) -> void
{
    const int32_t used = textLength;
    const int32_t fontHeight = fonts[0][fontIndex]->height();

    if (TextBufferSize - used <= 2)
    {
        return;
    }

    text[used] = static_cast<char>(color);
    char* dest = text + used + 1;
    const int32_t textStart = used + 1;
    textLength = textStart;

    if (line == nullptr)
    {
        if (TextBufferSize - textStart > 2)
        {
            // A blank line.
            numLines++;
            textLength = used + 2;
            text[used + 1] = '\n';
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
        text[TextBufferSize] = '\0';
    }

    numLines++;
    const int32_t needed = numLines * (fontHeight + 4);

    if (scrolling != 0 && needed > lport()->height())
    {
        lport()->resize(lport()->width(), needed);
    }

    PositionScrollTab();
}

auto lScrollTextObject::PrintWrapped(char* line, uint8_t color, int32_t wrapWidth) -> void
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

auto lScrollTextObject::Clear() -> void
{
    firstPixel = 0;
    textLength = 0;
    numLines = 0;
    std::memset(text, 0, TextBufferSize);

    for (int32_t& line : highlightLine)
    {
        line = -1;
    }

    draw();
}

auto lScrollTextObject::CalcFirstPixel(int32_t tabPos) -> void
{
    const int32_t track = (-0x1e - scrollTab->height()) + height();
    const int32_t range = lport()->height() - height();

    if (scrolling != 0 && track > 0 && range > 0)
    {
        firstPixel = (range * tabPos) / track;
        return;
    }

    firstPixel = 0;
}

auto lScrollTextObject::PositionScrollTab() -> void
{
    if (application->grabbedObject() == scrollTab)
    {
        return;
    }

    const int32_t track = height() - 0x1e;
    const int32_t range = lport()->height() - height();

    if (range == 0)
    {
        scrollTab->ShowGUIWindow(0);
        return;
    }

    // The thumb's length is the visible fraction of the track (x87: float quotient, then times the track).
    const float shown = static_cast<float>(height());
    int32_t tabLength = static_cast<int32_t>(static_cast<double>(shown) / lport()->height() * track);

    if (tabLength < 3)
    {
        tabLength = 3;
    }

    scrollTab->ShowGUIWindow(-1);
    scrollTab->resize(scrollTab->width(), tabLength);
    scrollTab->moveTo(scrollTab->x(), ((track - tabLength) * firstPixel) / range + 0xf, 0);
}

auto lScrollTextObject::ReceiveClick(int32_t direction, int32_t yPos) -> void
{
    if (height() == lport()->height())
    {
        return;
    }

    const int32_t lineHeight = fonts[0][fontIndex]->height() + 4;
    auto clampToEnd = [this]()
    {
        if (firstPixel > lport()->height() - height())
        {
            firstPixel = lport()->height() - height();
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
        // A page up isn't clamped at the top (unlike aScrollTextObject's).
        if (yPos < scrollTab->y())
        {
            firstPixel -= height();
        }

        if (yPos > scrollTab->bottom())
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
    draw();
}

auto lScrollTextObject::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (height() == lport()->height())
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        ReceiveClick(steps < 0 ? -1 : 1, 0);
    }

    return true;
}

auto lScrollTextObject::getTextLine(int32_t line, char* dest, int32_t destSize) -> int
{
    char* p = text;

    if (line < 0)
    {
        return 0;
    }

    // Lines 0 and 1 are both the first line.
    int32_t remaining = line - 1;

    while (p != nullptr)
    {
        if (*p == '\0' || remaining <= 0)
        {
            if (*p == '\0')
            {
                return 0;
            }

            if (dest != nullptr && destSize > 0)
            {
                std::strncpy(dest, p + 1, static_cast<size_t>(destSize - 1));

                if (char* end = std::strchr(dest, '\n'); end != nullptr)
                {
                    *end = '\0';
                }

                dest[destSize - 1] = '\0';
            }

            return -1;
        }

        p = std::strchr(p + 1, '\n');

        if (p != nullptr)
        {
            p++;
        }

        remaining--;
    }

    return 0;
}

// GameList

GameList::~GameList()
{
    lScrollTextObject::destroy();
}

auto GameList::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    numSessions = -1;
    selectedSession = -1;
    const int32_t result = lScrollTextObject::init(xPos, yPos, width, height, newText);
    tabColumn = 0x73;
    highlightColor[0] = 0x14;
    return result;
}

auto GameList::draw() -> void
{
    firstPixel = 0;
    textLength = 0;
    numLines = 0;

    for (int32_t& line : highlightLine)
    {
        line = -1;
    }

    std::memset(text, 0, TextBufferSize);

    if (MPlayer != nullptr)
    {
        // "name <tab> free slots", or FULL; the selection is highlighted.
        for (int32_t i = 0; i < numSessions; i++)
        {
            FIDPSession* session = MPlayer->sessionManager->FindMatchingSession(&sessions[i]);

            if (session == nullptr)
            {
                continue;
            }

            const int32_t freeSlots =
                static_cast<int32_t>(session->sessionDesc.dwMaxPlayers - session->sessionDesc.dwCurrentPlayers);
            char line[256];

            if (freeSlots == 0)
            {
                std::snprintf(line, sizeof(line), "%s\tFULL", session->sessionDesc.lpszSessionNameA);
            }
            else
            {
                std::snprintf(line, sizeof(line), "%s\t%d", session->sessionDesc.lpszSessionNameA, freeSlots);
            }

            if (i == selectedSession)
            {
                Print(line, 0x1f);
                highlightLine[0] = i;
            }
            else
            {
                Print(line, 0x0c);
            }
        }
    }

    lScrollTextObject::draw();
}

auto IsSessionDeleted(FIDPSession* session) -> int
{
    for (int32_t i = 0; i < nextDeletedSession; i++)
    {
        if (std::memcmp(&session->sessionDesc.guidInstance, &deletedSessions[i], sizeof(_GUID)) == 0)
        {
            return -1;
        }
    }

    return 0;
}

auto GameList::handleEvent(aEvent* event) -> void
{
    MultiPlayer* multiPlayer = MPlayer;

    if (event->type == 1)
    {
        // Selects the clicked session and tells the parent (event 0x1e, data 3).
        const int32_t rowHeight = fonts[0][fontIndex]->height() + 4;
        const int32_t clickX = event->x - globalX();
        const int32_t clickY = (firstPixel + event->y) - globalY();

        if (numSessions <= 0)
        {
            return;
        }

        int32_t row = 0;
        int32_t rowTop = 0;

        for (;;)
        {
            const tagRECT rect = {1, rowTop, width() - 0xd, rowTop + rowHeight};

            if (PtInRect(&rect, tagPOINT{clickX, clickY}))
            {
                break;
            }

            row++;
            rowTop += rowHeight;

            if (row >= numSessions)
            {
                return;
            }
        }

        if (row < numSessions)
        {
            selectedSession = row;
            selectedGuid = sessions[row];
        }

        draw();
        aEvent selected;
        selected.type = 0x1e;
        selected.data = 3;
        parent->handleEvent(&selected);
    }
    else if (event->type == 0x13)
    {
        // Refresh: the sessions that have players (an empty one is remembered as deleted and never listed again).
        numSessions = 0;

        if (multiPlayer != nullptr && multiPlayer->sessionManager != nullptr)
        {
            FLinkedList<FIDPSession>* list = multiPlayer->sessionManager->GetSessions();
            FLink<FIDPSession>* link = list->head;
            FIDPSession* session = link != nullptr ? link->data : nullptr;

            // The scan stops at the first deleted session.
            while (session != nullptr && IsSessionDeleted(session) == 0)
            {
                if (multiPlayer->sessionManager->GetPlayers(session)->count < 1)
                {
                    // Port fix: the list holds 50; the original ran past it.
                    if (nextDeletedSession < 50)
                    {
                        deletedSessions[nextDeletedSession++] = session->sessionDesc.guidInstance;
                    }
                }
                else if (numSessions < MAX_GAMES)
                {
                    // Port fix: bounded to the 64 slots (the original wasn't).
                    sessions[numSessions++] = session->sessionDesc.guidInstance;
                }

                link = link->next;

                if (link == nullptr)
                {
                    break;
                }

                session = link->data;
            }

            // Keep the selection on its session, or tell the parent it went (data 4).
            int32_t i = 0;

            for (; i < numSessions; i++)
            {
                if (std::memcmp(&sessions[i], &selectedGuid, sizeof(_GUID)) == 0)
                {
                    selectedSession = i;
                    break;
                }
            }

            if (i == numSessions)
            {
                aEvent lost;
                lost.type = 0x1e;
                lost.data = 4;
                lost.lParam = -1;
                parent->handleEvent(&lost);
                selectedSession = -1;
            }
        }

        draw();
    }
}

auto GameList::getSelectedGame() -> _GUID*
{
    if (selectedSession >= numSessions)
    {
        selectedSession = -1;
        aEvent refresh;
        refresh.clear();
        refresh.type = 0x13;
        handleEvent(&refresh);
        return nullptr;
    }

    // Port fix: with no selection the original returned &sessions[-1] (inside this object).
    if (selectedSession < 0)
    {
        return nullptr;
    }

    return &sessions[selectedSession];
}

// lSlider

lSlider::lSlider()
{
    minValue = 0;
    maxValue = 0;
    currentValue = 0;
    thumbPort = new lPort;
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%sprefs_02.tga", artPath);
    thumbPort->init(fileName);
}

lSlider::~lSlider()
{
    lSlider::destroy();
}

auto lSlider::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = lObject::init(xPos, yPos, width, height, name, nullptr);
    SetTransparent(-1);
    return result;
}

auto lSlider::destroy() -> void
{
    freePort(thumbPort);
    lObject::destroy();
}

auto lSlider::draw() -> void
{
    VFX_pane_wipe(lport()->frame(), 0xff);
    // The thumb's x is the value's share of the travel (x87).
    const int32_t travel = width() - thumbPort->width();
    const int32_t thumbX = static_cast<int32_t>(static_cast<double>(travel) *
                                                (static_cast<double>(currentValue - minValue) / (maxValue - minValue)));
    thumbPort->copyTo(lport()->frame(), thumbX, 0, 0);
}

auto lSlider::setCurrentValue(int32_t value) -> void
{
    if (value < minValue)
    {
        currentValue = minValue;
    }
    else if (value > maxValue)
    {
        currentValue = maxValue;
    }
    else
    {
        currentValue = value;
    }

    draw();
}

auto lSlider::handleEvent(aEvent* event) -> void
{
    // The value under the mouse: the offset (as a float) over the travel, times the range.
    auto valueAt = [this](int32_t mouseX)
    {
        const float offset = static_cast<float>(mouseX - x());
        const int32_t travel = width() - thumbPort->width();
        return static_cast<int32_t>(static_cast<double>(offset) / travel * (maxValue - minValue)) + minValue;
    };

    switch (event->type)
    {
        case 1:
        {
            application->grab(this);

            if (application->grabbedObject() != nullptr)
            {
                setCurrentValue(valueAt(event->x));
            }
            break;
        }
        case 4:
        {
            application->release();
            setCurrentValue(valueAt(event->x));
            break;
        }
        case 7:
        {
            if (application->grabbedObject() != nullptr)
            {
                setCurrentValue(valueAt(event->x));
            }
            break;
        }
        default:
            break;
    }

    if (eventRoutine != nullptr)
    {
        eventRoutine(this, event);
    }
}
