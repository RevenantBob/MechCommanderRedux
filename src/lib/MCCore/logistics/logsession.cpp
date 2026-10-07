#include "stdafx.h"
#include "logistics/logsession.h"
#include "gui/afont.h"
#include "lib/aerror.h"
#include "lib/cident.h"
#include "lib/file.h"
#include "lib/inifile.h"
#include "lib/packet.h"
#include "linkup/dpplayer.h"
#include "linkup/fidpgroup.h"
#include "linkup/sessionmanager.h"
#include "logistics/logdlg.h"
#include "logistics/logmain.h"
#include "logistics/logscrn.h"
#include "logistics/ticker.h"
#include "main/logistics.h"
#include "main/main.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "sound/soundsys.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>Until when (<c>timeGetTime</c>) the session screen keeps pinging after it opens (0x00808684).</summary>
    uint32_t pingUntil = 0;

    /// <summary>Set while the session screen still pings (0x00808688).</summary>
    int32_t pinging = 0;

    /// <summary>The number of team slots.</summary>
    constexpr int32_t TeamSlots = 3;

    /// <summary>The top of the unassigned list and the height of its rows.</summary>
    constexpr int32_t UnassignedTop = 0x162;
    constexpr int32_t UnassignedRow = 0x14;

    /// <summary>A literal for the functions that take a non-const name.</summary>
    char* art(const char* name)
    {
        return const_cast<char*>(name);
    }

    /// <summary>The slot of <paramref name="playerId"/> on <paramref name="team"/>, or -1.</summary>
    int32_t findSlot(const uint32_t* team, uint32_t playerId)
    {
        for (int32_t slot = 0; slot < TeamSlots; ++slot)
        {
            if (team[slot] == playerId)
            {
                return slot;
            }
        }

        return -1;
    }

    /// <summary>A copy of <paramref name="text"/> in a logistics block.</summary>
    char* heapCopy(const char* text)
    {
        size_t size = std::strlen(text) + 1;
        auto* copy = static_cast<char*>(globalLogPtr->logisticsBlocks->Allocate(static_cast<uint32_t>(size)));
        std::memcpy(copy, text, size);
        return copy;
    }

    /// <summary>
    /// Sends a guaranteed message carrying a file name (<paramref name="type"/> 0x1027 load mission, 0x1028 start) to
    /// every player.
    /// </summary>
    void sendFileName(uint16_t type, const char* fileName)
    {
        size_t length = std::strlen(fileName) + 1;
        auto* message = static_cast<MPFileNameMessage*>(
            globalLogPtr->logisticsBlocks->Allocate(static_cast<uint32_t>(length + 0xc)));
        message->tagger.Clear();
        message->header = type;
        std::memcpy(message->fileName, fileName, length);
        MPlayer->sessionManager->SendMessageToGroup(0, message,
                                                    static_cast<uint32_t>(std::strlen(message->fileName) + 1 + 0xc));
        globalLogPtr->logisticsBlocks->Free(message);
    }

    /// <summary>Sends a two-long guaranteed message to every player.</summary>
    void sendTwoLongs(uint16_t type, int32_t value1, int32_t value2)
    {
        MPTwoLongMessage message{};
        message.header = type;
        message.value1 = value1;
        message.value2 = value2;
        MPlayer->sessionManager->SendMessageToGroup(0, &message, sizeof(message));
    }

    /// <summary>The file name part of <paramref name="path"/> without folder or extension (<c>_splitpath</c>'s fname).</summary>
    void splitFileName(const char* path, char* fileName, size_t size)
    {
        const char* start = path;

        for (const char* scan = path; *scan != 0; ++scan)
        {
            if (*scan == '\\' || *scan == '/' || *scan == ':')
            {
                start = scan + 1;
            }
        }

        const char* end = std::strrchr(start, '.');
        size_t length = end != nullptr ? static_cast<size_t>(end - start) : std::strlen(start);

        if (length >= size)
        {
            length = size - 1;
        }

        std::memcpy(fileName, start, length);
        fileName[length] = 0;
    }

    /// <summary>
    /// Sets up the message dialog's single button: <paramref name="callback"/> on it, the given pictures, enabled; no
    /// result callback.
    /// </summary>
    void setDialogButton(void (*callback)(), const char* upArt, const char* downArt)
    {
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->okButton->callback()->setExec(callback);
        dialog->okButton->setUpPicture(art(upArt));
        dialog->okButton->setDownPicture(art(downArt));
        dialog->okButton->disabled = 0;
    }
}

// Event routines

void lToolButtonEventHandler(aObject* object, aEvent* event)
{
    auto* button = static_cast<lToolButton*>(object);

    if (event->type == 1)
    {
        if (button->disabled == 0)
        {
            button->toggled = button->toggled == 0 ? 1 : 0;
            button->callback()->execute();
        }
    }
    else if (event->type == 4)
    {
        if (application->grabbedObject() != nullptr)
        {
            application->release();
        }
    }
}

void ChatTeamButtonEventHandler(aObject* object, aEvent* event)
{
    lToolButtonEventHandler(object, event);
    object->parent->handleEvent(event);
}

void lScreenSwitchEventHandler(aObject* object, aEvent* event)
{
    auto* button = static_cast<lToolButton*>(object);

    if (event->type == 1 && button->disabled == 0 && button->toggled == 0)
    {
        button->toggled = -1;
        button->callback()->execute();
    }
}

// lToolButton

auto lToolButton::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    toggled = 0;
    int32_t result = lButton::init(xPos, yPos, width, height, name);

    if (result == 0)
    {
        setEventRoutine(lToolButtonEventHandler);
    }

    return result;
}

auto lToolButton::handleEvent(aEvent* event) -> void
{
    if (event->type == 1)
    {
        // Original behaviour: the press sound is the fixed 0x33 unless an event routine is set.
        uint32_t sample = eventRoutine == nullptr ? 0x33 : pressSound;
        soundSystem->playDigitalSample(sample, 1, nullptr, 0, 0);
    }
    else if (event->type != 4)
    {
        lButton::handleEvent(event);
        return;
    }

    aObject::handleEvent(event);
}

auto lToolButton::draw() -> void
{
    lPort* picture;

    if (disabled != 0)
    {
        picture = grayPicture;
    }
    else if (toggled != 0)
    {
        picture = downPicture;
    }
    else if (overState != 0)
    {
        picture = overPicture;
    }
    else
    {
        // The mouse can be over the button without overState (it was disabled when the mouse came).
        MCPoint cursor = MCInput::GetCursorPos();
        int32_t cursorX = cursor.x - globalX();
        int32_t cursorY = cursor.y - globalY();
        // OB-129 (fixed): the original tested only the right and bottom edges, so the cursor anywhere above or left
        // of the button counted as over it.
        const bool over = cursorX >= 0 && cursorY >= 0 && cursorX <= width() && cursorY <= height();
        picture = over ? overPicture : upPicture;
    }

    drawFace(picture, false);
}

// lSpinnerButton

auto lSpinnerButton::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            uint32_t sample = 0x33;

            if (disabled == 0)
            {
                // Run the callback now, then repeat it after half a second held.
                toggled = -1;
                application->grab(this);
                application->AddTimer(this, 1, 500, 0, 0, 0);
                buttonCallback->execute();
                sample = pressSound;
            }

            soundSystem->playDigitalSample(sample, 1, nullptr, 0, 0);
            break;
        }

        case 4:
        {
            toggled = 0;
            application->release();
            application->RemoveTimer(this, 1);
            application->RemoveTimer(this, 2);
            break;
        }
        case 0x13:
        {
            if (event->data == 1)
            {
                application->RemoveTimer(this, 1);
                application->AddTimer(this, 2, 100, 0, 0, 0);
            }

            if (event->data == 2)
            {
                buttonCallback->execute();
            }
            break;
        }
        default:
            break;
    }

    aObject::handleEvent(event);
}

auto lSpinnerButton::draw() -> void
{
    // Original behaviour (OB-130): the gray picture is never shown, so a disabled spinner looks enabled.
    lPort* picture = toggled != 0 ? downPicture : upPicture;

    if (picture != nullptr)
    {
        picture->copyTo(ownPort->frame(), 0, 0, 0);
    }

    lObject::draw();
}

// lChatInput

auto lChatInput::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = lObject::init(xPos, yPos, width, height, nullptr, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");
    auto* button = new lToolButton;
    teamButton = button;
    result = button->init(1, 1, 0xc, 0x1a, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    result = button->setOverPicture(art("lsbdw06.tga"));
    // The later picture loads' results are dropped: every check repeats the first one's.
    const int loadOk = result == 0;
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load upstate team button for chatsend window");
    button->setDownPicture(art("lsbdw07.tga"));
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load downstate team button for chatsend window");
    button->setUpPicture(art("lsbdw05.tga"));
    Assert(loadOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    button->toggled = 0;
    button->setEventRoutine(ChatTeamButtonEventHandler);
    addChild(button);
    cursorX = 0x15;

    if (newText != nullptr)
    {
        std::strncpy(text, newText, 0xff);
    }

    font = whiteFont;
    backgroundColor = 0x10;
    RestartBlink();
    return 0;
}

auto lChatInput::destroy() -> void
{
    if (teamButton != nullptr)
    {
        delete teamButton;
        teamButton = nullptr;
    }

    lObject::destroy();
}

auto lChatInput::draw() -> void
{
    VFX_pane_wipe(ownPort->frame(), backgroundColor);
    auto* line = reinterpret_cast<uint8_t*>(text);
    int32_t lineY = 1;

    if (text[0] != 0)
    {
        // The first line leaves room for the team button.
        int32_t lineLength = static_cast<int32_t>(std::strlen(text));
        int32_t fits = font->charactersToWidth(line, width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            uint8_t* next = line + fits;
            const uint8_t saved = *next;
            *next = 0;
            font->writeString(ownPort->frame(), 0x14, lineY, line, -1);
            *next = saved;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
            fits = font->charactersToWidth(next, width() - 0x14, -1);
            lineY += 3 + font->height();
            line = next;
        }

        font->writeString(ownPort->frame(), 0x14, lineY, line, -1);
    }

    // The caret, which the original's display drew into the picture each frame: a vertical line a text line high.
    const int32_t bottom = font->height() + 3 + cursorY;
    const int32_t color = cursorOn != 0 ? 0x10 : 0x1f;
    VFX_line_draw(ownPort->frame(), cursorX, cursorY, cursorX, bottom, LD_DRAW, color);
    lObject::draw();
}

auto lChatInput::RestartBlink() -> void
{
    cursorOn = -1;
}

auto lChatInput::display() -> void
{
    lObject::display();
}

auto lChatInput::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
            application->setText(this);
            break;
        case 10:
        {
            if (application->textObject() != this)
            {
                break;
            }

            const uint8_t key = event->key;

            if (key == 8)
            {
                if (textLength != 0)
                {
                    text[textLength - 1] = 0;
                    textLength--;
                    setCursorPos(textLength);
                    RestartBlink();
                }
            }
            else if (key == 0xd)
            {
                if (MPlayer != nullptr && textLength != 0)
                {
                    auto* chatWindow = static_cast<LogChatWindow*>(parent);
                    int32_t color;

                    if (teamButton->toggled == 0)
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
                textLength = 0;
                setCursorPos(0);
                RestartBlink();
            }
            else if (textLength < 0xff && ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%')
            {
                // '%' is the chat formatter's code character.
                text[textLength] = static_cast<char>(key);
                textLength++;
                setCursorPos(textLength);
                RestartBlink();
            }
            break;
        }

        case 0x13:
        {
            // The caret blink timer.
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
                application->AddTimer(this, 0, MCPort::CaretBlinkTime(), 0, 0, 0);
            }
            else if (event->data == 8)
            {
                application->RemoveTimer(this, 0);
            }
            break;
        }
        default:
            break;
    }

    aObject::handleEvent(event);
}

auto lChatInput::setCursorPos(int32_t position) -> void
{
    // Measure the text up to position.
    char saved = 0;

    if (position < textLength)
    {
        saved = text[position];
        text[position] = 0;
    }

    auto* line = reinterpret_cast<uint8_t*>(text);
    cursorX = 0x14;
    cursorY = 0;

    if (text[0] != 0)
    {
        int32_t lineLength = static_cast<int32_t>(std::strlen(text));
        int32_t fits = font->charactersToWidth(line, width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            line += fits;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
            fits = font->charactersToWidth(line, width() - 0x14, 0);
            cursorY += font->height() + 3;
        }

        cursorX = font->width(line) + 0x15;
    }

    if (saved != 0)
    {
        text[position] = saved;
    }
}

// PlayerNameObject

auto PlayerNameObject::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    if (playerName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(playerName);
        playerName = nullptr;
    }

    int32_t result = lObject::init(xPos, yPos, width, height, name, nullptr);
    setFont(lgWhiteFont);
    backgroundColor = 0xff;
    return result;
}

auto PlayerNameObject::destroy() -> void
{
    if (playerName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(playerName);
        playerName = nullptr;
    }

    lObject::destroy();
}

auto PlayerNameObject::draw() -> void
{
    if (!ownPort->viewOpen())
    {
        return;
    }

    if (numberArt != nullptr)
    {
        VFX_pane_wipe(ownPort->frame(), static_cast<uint32_t>(numberBack));
        numberArt->copyTo(ownPort->frame(), 1, 1, 0);
    }

    const auto color = static_cast<uint8_t>(backgroundColor);
    const auto bottom = static_cast<int16_t>(height() - 1);
    FillBox(0x14, 1, static_cast<int16_t>(width() - 1), bottom, color);

    // The name is left out while it is dragged.
    if (font != nullptr && application->grabbedObject() != this)
    {
        font->writeString(ownPort->frame(), 0x1b, 2, reinterpret_cast<uint8_t*>(playerName), -1);
    }
}

auto PlayerNameObject::handleEvent(aEvent* event) -> void
{
    switch (event->type)
    {
        case 1:
        {
            if (draggable != 0)
            {
                int32_t grabX = event->x - x();

                if (grabX > 0x14)
                {
                    grabX = 0x14;
                }

                application->grab(this);
                startDrag(grabX, event->y - y());
            }
            break;
        }
        case 4:
        {
            if (application->grabbedObject() == this)
            {
                application->release();
                ShowGUIWindow(0);
                // The original looks up the object under the drop and drops the result.
                screenWindow->findObject(event->x, event->y);
                ShowGUIWindow(-1);
                // Tell the session screen where the name was dropped.
                aEvent dropped;
                dropped.clear();
                dropped.type = 0x1d;
                dropped.target = this;
                parent->handleEvent(&dropped);
            }
            break;
        }
        case 7:
        {
            if (application->grabbedObject() == this)
            {
                moveTo(event->x - dragStartX(), event->y - dragStartY(), 0);
            }
            break;
        }
        default:
            break;
    }

    aObject::handleEvent(event);
}

auto PlayerNameObject::setPlayerName(char* name) -> void
{
    if (playerName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(playerName);
    }

    size_t size = std::strlen(name) + 1;
    playerName = static_cast<char*>(globalLogPtr->logisticsBlocks->Allocate(static_cast<uint32_t>(size)));

    if (playerName != nullptr)
    {
        std::memcpy(playerName, name, size);
    }
}

auto PlayerNameObject::setPlayerId(uint32_t newPlayerId) -> void
{
    playerId = newPlayerId;

    if (MPlayer != nullptr && MPlayer->sessionManager != nullptr &&
        MPlayer->sessionManager->GetPlayer(newPlayerId) != nullptr)
    {
        setPlayerName(MPlayer->sessionManager->GetPlayer(newPlayerId)->name);
    }
}

auto PlayerNameObject::setFont(aFont* newFont) -> void
{
    font = newFont;
}

// Callbacks

void MPCancelCallback()
{
    globalLogPtr->messageDialog->callback = nullptr;
    globalLogPtr->sessionScreen->cancelMission();
}

void MPLoadWorkedCallback(int32_t)
{
    // The saved multiplayer mission: back to the session screen, and every player loads it.
    const char* fileName = globalLogPtr->sessionScreen->missionFile;
    size_t length = std::strlen(fileName) + 1;
    auto* message =
        static_cast<MPFileNameMessage*>(globalLogPtr->logisticsBlocks->Allocate(static_cast<uint32_t>(length + 0xc)));
    MCSplashScreen* loadScreen = globalLogPtr->loadScreen;
    loadScreen->cancelButton->callback()->setExec(Cancel);
    loadScreen->loadSaveButton->callback()->setExec(LoadGame);
    loadScreen->filePane->setMultiplayer(0);
    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = globalLogPtr->sessionScreen;
    globalLogPtr->logisticsState = 8;
    globalLogPtr->showLogScreen(-1, -1);
    message->tagger.Clear();
    message->header = FIMSG_GUARANTEED | MPMSG_LOAD_MISSION;
    std::memcpy(message->fileName, fileName, length);
    MPlayer->sessionManager->SendMessageToGroup(0, message,
                                                static_cast<uint32_t>(std::strlen(message->fileName) + 1 + 0xc));
    globalLogPtr->logisticsBlocks->Free(message);
}

void LoadMissionCallback()
{
    MCSplashScreen* loadScreen = globalLogPtr->loadScreen;
    loadScreen->loadSaveButton->callback()->setExec(LoadMPGame);
    loadScreen->cancelButton->callback()->setExec(CancelToSession);
    loadScreen->filePane->setMultiplayer(-1);
    globalLogPtr->currentScreen->ShowGUIWindow(0);
    globalLogPtr->currentScreen = loadScreen;
    globalLogPtr->logisticsState = 5;
    globalLogPtr->showLogScreen(-1, 0);
}

void StartMissionCallback()
{
    char missionName[256];
    splitFileName(globalLogPtr->sessionScreen->missionFile, missionName, sizeof(missionName));
    sendFileName(FIMSG_GUARANTEED | MPMSG_START, missionName);
    application->RemoveTimer(globalLogPtr->sessionScreen, 0);
    MPlayer->sessionManager->SendLatencyInfo();
    soundSystem->playBettySample(0x19);
    globalLogPtr->initializeMultiplayer();
    globalLogPtr->loadCampaign(missionName, art(".MPK"), 0, 0);
    globalLogPtr->setUpBriefingScreen(0);
}

void techTabRoutine(aObject* object, aEvent* event)
{
    if (event->type != 1)
    {
        return;
    }

    auto* button = static_cast<lToolButton*>(object);
    int32_t techBase = button->value;
    int32_t team = button->group;
    globalLogPtr->sessionScreen->setTeamTechBase(static_cast<char>(team), static_cast<char>(techBase));

    if (MPlayer->isHost != 0)
    {
        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, team, techBase);
    }
}

void incrementTeam1RP()
{
    SessionScreen* screen = globalLogPtr->sessionScreen;
    screen->setTeam1RP(std::atol(screen->team1RPText->buffer) + 1000);
}

void incrementTeam2RP()
{
    SessionScreen* screen = globalLogPtr->sessionScreen;
    screen->setTeam2RP(std::atol(screen->team2RPText->buffer) + 1000);
}

void decrementTeam1RP()
{
    SessionScreen* screen = globalLogPtr->sessionScreen;
    int32_t points = std::atol(screen->team1RPText->buffer) - 1000;

    if (points < 0)
    {
        points = 0;
    }

    screen->setTeam1RP(points);
}

void decrementTeam2RP()
{
    SessionScreen* screen = globalLogPtr->sessionScreen;
    int32_t points = std::atol(screen->team2RPText->buffer) - 1000;

    if (points < 0)
    {
        points = 0;
    }

    screen->setTeam2RP(points);
}

void SessionScreenDrawRoutine(aObject* object)
{
    auto* screen = static_cast<lObject*>(object);
    screen->FillBox(0x1f, 0x162, 0xc6, 0x1d3, 0x10);
    int16_t top = 0x163;

    for (int32_t row = 0; row < globalLogPtr->sessionScreen->numUnassigned; ++row)
    {
        screen->FillBox(0x1f, top, 0xc6, static_cast<int16_t>(top + 0xd), 0x12);
        top += 0x14;
    }
}

int CompareLong(const void* first, const void* second)
{
    const uint32_t a = *static_cast<const uint32_t*>(first);
    const uint32_t b = *static_cast<const uint32_t*>(second);

    if (b == a)
    {
        return 0;
    }

    return b < a ? 1 : -1;
}

// MPPlayerLights (its destructor was emitted here)

MPPlayerLights::~MPPlayerLights()
{
    destroy();
}

// SessionScreen

auto SessionScreen::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = lObject::init(xPos, yPos, width, height, name, nullptr);
    setPaintRoutine(SessionScreenDrawRoutine);
    Assert(result == 0, static_cast<uint32_t>(result), " Error initing session screen ");

    // The screen tabs.
    auto* toolButton = new lToolButton;
    sessionButton = toolButton;

    if (toolButton != nullptr)
    {
        result = toolButton->init(2, 0x10, 0xcf, 0x12, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button on session screen ");
        result = toolButton->setUpPicture(art("bn_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->setDownPicture(art("bg_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->setOverPicture(art("bh_session.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        toolButton->toggled = -1;
        toolButton->setEventRoutine(lScreenSwitchEventHandler);
        addChild(toolButton);
    }

    toolButton = new lToolButton;
    exitButton = toolButton;

    if (toolButton != nullptr)
    {
        result = toolButton->init(2, 0x22, 0xcf, 0x12, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button on session screen ");
        result = toolButton->setUpPicture(art("bn_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->setDownPicture(art("bg_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        result = toolButton->setOverPicture(art("bh_exit.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing session button art on session screen ");
        toolButton->setEventRoutine(lScreenSwitchEventHandler);
        toolButton->callback()->setExec(launchedFromLobby == 0 ? Cancel : CancelToMPlayer);
        addChild(toolButton);
    }

    // The host's load and start buttons.
    auto* button = new lButton;
    loadMissionButton = button;

    if (button != nullptr)
    {
        result = button->init(0x107, 0xd1, 0x5e, 0x13, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button on session screen ");
        result = button->setUpPicture(art("ses_bh_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->setOverPicture(art("ses_bh_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->callback()->setExec(LoadMissionCallback);
        result = button->setDownPicture(art("ses_bg_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->setGrayPicture(art("ses_bn_map.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->SetTransparent(-1);
        addChild(button);
    }

    button = new lButton;
    startButton = button;

    if (button != nullptr)
    {
        result = button->init(0x1de, 0x1b4, 0x5e, 0x13, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button on session screen ");
        result = button->setUpPicture(art("ses_bh_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->setOverPicture(art("ses_bh_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        result = button->setDownPicture(art("ses_bg_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        button->callback()->setExec(StartMissionCallback);
        result = button->setGrayPicture(art("ses_bn_begin.tga"));
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing load button art on session screen ");
        addChild(button);
        startButton->disabled = -1;
        startButton->SetTransparent(-1);
    }

    // The mission description.
    auto* description = new lScrollTextObject;
    missionText = description;

    if (description != nullptr)
    {
        result = description->init(0x17f, 0x35, 0xdc, 0x82, nullptr);
        Assert(result == 0, static_cast<uint32_t>(result), " Error initing mission description on session screen ");
        description->scrollTab->ShowGUIWindow(0);
        description->scrolling = 0;
        addChild(description);
    }

    // Each team's resource points, a number the host can type.
    auto makeRPText = [this](int32_t textX)
    {
        auto* text = new lTextObject;

        if (text != nullptr)
        {
            text->lObject::init(textX, 0x184, 0x42, 0xe, nullptr, nullptr);
            text->font = lgBlackFont;
            text->backgroundColor = 0x1f;
            text->cursorPos = 0;
            text->cursorPixel = 0;
            VFX_pane_wipe(text->lport()->frame(), 0x1f);
            text->initBuffer(8, -1);
            text->setStringBuffer(art("0"));
            text->setBackColor(0x10);
            text->font = lgWhiteFont;
            addChild(text);
        }

        return text;
    };

    team1RPText = makeRPText(0x10f);
    team2RPText = makeRPText(0x1d1);

    auto makeSpinner = [this](int32_t spinX, int32_t spinY, const char* upArt, const char* downArt, const char* grayArt)
    {
        auto* spinner = new lSpinnerButton;

        if (spinner != nullptr)
        {
            spinner->init(spinX, spinY, 9, 7, nullptr);
            spinner->setUpPicture(art(upArt));
            spinner->setDownPicture(art(downArt));
            spinner->setGrayPicture(art(grayArt));
            spinner->setEventRoutine(nullptr);
            spinner->pressSound = 0xf;
            addChild(spinner);
        }

        return spinner;
    };

    team1RPUp = makeSpinner(0x102, 0x183, "Ses_bh_rpup.tga", "Ses_bg_rpup.tga", "Ses_bn_rpup.tga");
    team1RPDown = makeSpinner(0x102, 0x18b, "Ses_bh_rpdn.tga", "Ses_bg_rpdn.tga", "Ses_bn_rpdn.tga");
    team2RPUp = makeSpinner(0x1c4, 0x183, "Ses_bh_rpup.tga", "Ses_bg_rpup.tga", "Ses_bn_rpup.tga");
    team2RPDown = makeSpinner(0x1c4, 0x18b, "Ses_bh_rpdn.tga", "Ses_bg_rpdn.tga", "Ses_bn_rpdn.tga");

    // The tech base toggles (team 1: Inner Sphere, team 2: Clan).
    auto makeTechButton = [this](int32_t techX, int32_t team, int32_t techBase, int toggledOn)
    {
        auto* techButton = new lToolButton;

        if (techButton != nullptr)
        {
            techButton->init(techX, 0x10c, 0x27, 0xe, nullptr);
            const bool clan = techBase == -1;
            techButton->setUpPicture(art(clan ? "ses_bh_clan.tga" : "ses_bh_is.tga"));
            techButton->setOverPicture(art(clan ? "ses_bh_clan.tga" : "ses_bh_is.tga"));
            techButton->setDownPicture(art(clan ? "ses_bg_clan.tga" : "ses_bg_is.tga"));
            techButton->group = team;
            techButton->value = techBase;

            if (toggledOn != 0)
            {
                techButton->toggled = -1;
            }

            techButton->SetTransparent(-1);
            techButton->pressSound = 0xf;
            addChild(techButton);
        }

        return techButton;
    };

    team1TechBase = 1;
    team1ISButton = makeTechButton(0x153, 1, 1, -1);
    team1ClanButton = makeTechButton(0x179, 1, -1, 0);
    team2TechBase = -1;
    team2ISButton = makeTechButton(0x215, 2, 1, 0);
    team2ClanButton = makeTechButton(0x23b, 2, -1, -1);

    // Where names can be dropped: team 1's three slots, then team 2's.
    numDropTargets = 6;
    dropTargets = new tagRECT[6];

    if (dropTargets != nullptr)
    {
        dropTargets[0] = {0xf7, 0x132, 0x199, 0x144};
        dropTargets[1] = {0xf7, 0x148, 0x199, 0x15a};
        dropTargets[2] = {0xf7, 0x15d, 0x199, 0x16f};
        dropTargets[3] = {0x1b9, 0x132, 0x25b, 0x144};
        dropTargets[4] = {0x1b9, 0x148, 0x25b, 0x15a};
        dropTargets[5] = {0x1b9, 0x15d, 0x25a, 0x16f};
    }

    // The six name slots, each with its player number picture.
    int32_t number = 0;
    PlayerNameObject** slot = playerNames;

    for (int32_t slotY = UnassignedTop; slotY < 0x1da; slotY += UnassignedRow, ++slot)
    {
        auto* nameObject = new PlayerNameObject;
        *slot = nameObject;
        nameObject->init(0xb, UnassignedTop, 0xa0, 0x10, nullptr);
        addChild(nameObject);
        nameObject->moveTo(0xb, slotY, 0);
        nameObject->ShowGUIWindow(0);
        nameObject->SetTransparent(-1);
        ++number;
        // The original wiped the name's picture to the screen's background colour and pasted the number there; the
        // name draws them each frame.
        nameObject->numberBack = backgroundColor;
        nameObject->numberArt = logArtf("ses_p%i.tga", number);
    }

    setBackground(art("ses_bk00.tga"));
    screenWindow->addChild(this);
    ShowGUIWindow(0);
    numUnassigned = 0;
    return 0;
}

auto SessionScreen::destroy() -> void
{
    auto release = [](auto*& object)
    {
        if (object != nullptr)
        {
            delete object;
            object = nullptr;
        }
    };

    auto destroyAndRelease = [](auto*& object)
    {
        if (object != nullptr)
        {
            object->destroy();
            delete object;
            object = nullptr;
        }
    };

    release(sessionButton);
    release(exitButton);
    release(loadMissionButton);
    release(startButton);
    release(missionText);
    destroyAndRelease(team1RPText);
    destroyAndRelease(team2RPText);
    destroyAndRelease(team1RPUp);
    destroyAndRelease(team1RPDown);
    destroyAndRelease(team2RPUp);
    destroyAndRelease(team2RPDown);
    destroyAndRelease(team1ISButton);
    destroyAndRelease(team1ClanButton);
    destroyAndRelease(team2ISButton);
    destroyAndRelease(team2ClanButton);

    for (PlayerNameObject*& nameObject : playerNames)
    {
        release(nameObject);
    }

    if (dropTargets != nullptr)
    {
        delete[] dropTargets;
        dropTargets = nullptr;
    }

    // Original behaviour: the mission name isn't freed here (the logistics heap goes with the logistics object).
    if (missionFile != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(missionFile);
        missionFile = nullptr;
    }

    if (mapName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(mapName);
        mapName = nullptr;
    }

    ClearMap();
    lObject::destroy();
}

auto SessionScreen::draw() -> void
{
    if (MPlayer == nullptr || MPlayer->sessionManager->currentSession == nullptr)
    {
        return;
    }

    lObject::draw();
    lPort* port = ownPort;
    DrawMap(port->frame());
    medWhiteFont->writeString(port->frame(), 0x180, 199, reinterpret_cast<uint8_t*>(missionName), -1);
    medWhiteFont->writeString(port->frame(), 0x180, 0xe3, reinterpret_cast<uint8_t*>(mapName), -1);
    char noLabel[256];
    cLoadString(thisInstance, 0x37f, noLabel, 0xfe);
    char* label = missionLabel[0] != 0 ? missionLabel : noLabel;
    medWhiteFont->writeString(port->frame(), 0x20e, 199, reinterpret_cast<uint8_t*>(label), -1);

    // Each team's resource points per player.
    if (MPlayer->clanGroupID == 0)
    {
        globalLogPtr->drawScreenChrome(this, port->frame());
        return;
    }

    int32_t players = 1;
    FIDPGroup* group = MPlayer->sessionManager->GetGroup(MPlayer->innerSphereGroupID);
    int32_t divisor = players;

    if (group != nullptr)
    {
        divisor = group->players.count;

        if (divisor == 0)
        {
            divisor = players = 1;
        }
    }

    char text[12];
    std::snprintf(text, sizeof(text), "%d", static_cast<int32_t>(std::atol(team1RPText->buffer) / divisor));
    lgWhiteFont->writeString(port->frame(), 0x157, 0x185, reinterpret_cast<uint8_t*>(text), -1);
    group = MPlayer->sessionManager->GetGroup(MPlayer->clanGroupID);

    if (group != nullptr)
    {
        divisor = group->players.count;
        players = divisor;
    }

    if (divisor == 0)
    {
        divisor = players = 1;
    }

    std::snprintf(text, sizeof(text), "%d", static_cast<int32_t>(std::atol(team2RPText->buffer) / divisor));
    lgWhiteFont->writeString(port->frame(), 0x219, 0x185, reinterpret_cast<uint8_t*>(text), -1);
    globalLogPtr->drawScreenChrome(this, port->frame());
}

auto SessionScreen::handleEvent(aEvent* event) -> void
{
    if (event->type == 0x13)
    {
        if (event->data == 0)
        {
            if (MPlayer == nullptr)
            {
                application->RemoveTimer(this, 0);
            }
            else
            {
                // Ping for the first three seconds so the latencies are known.
                if (pinging != 0)
                {
                    if (MCPort::Milliseconds() < pingUntil)
                    {
                        if (MPlayer->sessionManager->isHost == 0)
                        {
                            MPlayer->sessionManager->SendPing();
                        }
                    }
                    else
                    {
                        pinging = 0;
                    }
                }

                // Tell the others when the host changed a team's points.
                int32_t points = std::atol(team1RPText->buffer);

                if (team1RP != points)
                {
                    team1RP = std::atol(team1RPText->buffer);

                    if (MPlayer->isHost != 0)
                    {
                        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_RP_UPDATE, team1RP, 1);
                    }
                }

                points = std::atol(team2RPText->buffer);

                if (team2RP != points)
                {
                    team2RP = std::atol(team2RPText->buffer);

                    if (MPlayer->isHost != 0)
                    {
                        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_RP_UPDATE, team2RP, 2);
                    }
                }
            }
        }
    }
    else if (event->type == 0x1d)
    {
        // A name was dropped: onto a team slot, or anywhere else (no team).
        auto* nameObject = static_cast<PlayerNameObject*>(event->target);
        uint32_t id = nameObject->playerId;
        RECT area;
        area.left = nameObject->globalX();
        area.top = nameObject->globalY();
        area.right = area.left + 0x14;
        area.bottom = nameObject->height() + area.top;
        RECT overlap;
        static constexpr char SlotTeam[6] = {1, 1, 1, 2, 2, 2};
        static constexpr char SlotIndex[6] = {0, 1, 2, 0, 1, 2};
        bool placed = false;

        for (int32_t target = 0; target < 6; ++target)
        {
            if (IntersectRect(&overlap, &area, &dropTargets[target]) != 0)
            {
                assignPlayer(id, SlotTeam[target], SlotIndex[target], 0);
                placed = true;
                break;
            }
        }

        if (!placed)
        {
            assignPlayer(id, 0, 0, 0);
        }
    }

    aObject::handleEvent(event);
}

auto SessionScreen::activate(int refresh) -> void
{
    const int32_t isHost = MPlayer->isHost;

    if (MPlayer->sessionManager->GetPlayers(nullptr)->count < 2)
    {
        Cancel();
        return;
    }

    pingUntil = MCPort::Milliseconds() + 3000;
    pinging = -1;
    sessionButton->disabled = 0;
    exitButton->toggled = 0;
    exitButton->disabled = 0;
    startButton->disabled = -1;

    if (launchedFromLobby != 0)
    {
        exitButton->callback()->setExec(CancelToMPlayer);
    }

    cancelMission();
    team2RP = 0;
    team1RP = 0;
    int32_t slotY = UnassignedTop;

    for (PlayerNameObject* nameObject : playerNames)
    {
        nameObject->moveTo(0xb, slotY, 0);
        nameObject->ShowGUIWindow(0);
        slotY += UnassignedRow;
    }

    for (int32_t slot = 0; slot < TeamSlots; ++slot)
    {
        team1Players[slot] = 0xffffffff;
        team2Players[slot] = 0xffffffff;
    }

    numUnassigned = 0;
    numPlayers = 0;

    if (isHost != 0 && refresh == 0)
    {
        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_SWITCH_SCREEN, 1, 0);
    }

    team1ISButton->toggled = -1;
    team1ClanButton->toggled = 0;
    team2ISButton->toggled = 0;
    team1TechBase = 1;
    team2ClanButton->toggled = -1;
    team2TechBase = -1;

    // The players' ids in ascending order (the empty slots, 0xffffffff, last).
    FLinkedList<FIDPPlayer>* players = MPlayer->sessionManager->GetPlayers(nullptr);
    numPlayers = players->count;
    uint32_t ids[6];
    int32_t count = 0;
    FLink<FIDPPlayer>* link = players->head;
    FIDPPlayer* player = link != nullptr ? link->data : nullptr;

    while (player != nullptr)
    {
        ids[count++] = player->id;
        Assert(link != nullptr, 0, nullptr);
        link = link->next;

        if (link == nullptr)
        {
            break;
        }

        player = link->data;
    }

    for (int32_t index = count; index < 6; ++index)
    {
        ids[index] = 0xffffffff;
    }

    std::qsort(ids, 6, sizeof(uint32_t), CompareLong);

    if (globalLogPtr->playerLights == nullptr)
    {
        auto* lights = new MPPlayerLights;

        if (lights != nullptr)
        {
            lights->init();
        }

        globalLogPtr->playerLights = lights;
    }

    if (globalLogPtr->playerLights->parent != nullptr)
    {
        globalLogPtr->playerLights->parent->removeChild(globalLogPtr->playerLights);
    }

    addChild(globalLogPtr->playerLights);
    globalLogPtr->playerLights->setNumPlayers(numPlayers);
    int32_t index = 0;

    do
    {
        PlayerNameObject* nameObject = playerNames[index];
        nameObject->setPlayerId(ids[index]);
        nameObject->ShowGUIWindow(-1);
        nameObject->draggable = 0;
        assignPlayer(ids[index], 0, 0, -1);
        globalLogPtr->playerLights->setPlayerID(index, ids[index]);
        globalLogPtr->playerLights->setPlayerStatus(ids[index], refresh == 0 ? 0 : 2);
        ++index;
    } while (index < 6 && ids[index] != 0xffffffff);

    if (isHost == 0 || refresh != 0)
    {
        controlsOff();

        if (refresh != 0)
        {
            // Back from a mission: wait for everyone to check in again.
            char text[1024];
            cLoadString(thisInstance, 0xb8, text, 0xfe);
            missionText->Clear();
            missionText->fontIndex = 2;
            missionText->Print(text, 0x1f);
            FIGuaranteedMessageHeader checkIn{};
            checkIn.header = FIMSG_GUARANTEED | MPMSG_SESSION_CHECK_IN;
            MPlayer->sessionManager->SendMessageToGroup(0, &checkIn, sizeof(checkIn));
            MPlayer->playerSessionCheckIn[MPlayer->sessionManager->myPlayer->playerNumber] = -1;
            someoneCheckedIn();
        }
    }
    else
    {
        controlsOn();
    }

    missionName = nullptr;
    mapName = nullptr;
    std::memset(missionLabel, 0, sizeof(missionLabel));
    LogChatWindow* chatWindow = globalLogPtr->chatWindow;

    if (chatWindow != nullptr)
    {
        chatWindow->moveTo(2, 0x42, 0);
        chatWindow->resize(0x10a);

        if (chatWindow->parent != nullptr)
        {
            chatWindow->parent->removeChild(chatWindow);
        }

        addChild(chatWindow);
        chatWindow->ShowGUIWindow(-1);
    }

    Ticker* ticker = globalLogPtr->ticker;

    if (ticker != nullptr)
    {
        if (ticker->parent != nullptr)
        {
            ticker->parent->removeChild(ticker);
        }

        addChild(ticker);
        ticker->setPort(lport());
        // The ticker paints into this screen now (what it shows is kept in the screen's chrome).
        ticker->setScreen(this);
        ticker->setPos(3, 3);
    }

    application->AddTimer(this, 0, 500, 0, 0, 0);
    MPlayer->chatCallback = LogisticsChatCallback;
}

auto SessionScreen::assignPlayer(uint32_t playerId, char team, char slot, int remote) -> void
{
    PlayerNameObject* nameObject = nullptr;

    for (PlayerNameObject* candidate : playerNames)
    {
        if (candidate->playerId == playerId)
        {
            nameObject = candidate;
            break;
        }
    }

    // Lays the names of the players on no team down the unassigned list, skipping skipId.
    auto layOutUnassigned = [this](uint32_t skipId, int32_t names, bool skipEmpty)
    {
        numUnassigned = 0;

        for (int32_t index = 0; index < names; ++index)
        {
            uint32_t id = playerNames[index]->playerId;

            if (skipEmpty ? id == 0xffffffff : id == skipId)
            {
                continue;
            }

            if (findSlot(team1Players, id) >= 0 || findSlot(team2Players, id) >= 0)
            {
                continue;
            }

            playerNames[index]->moveTo(0xb, numUnassigned * UnassignedRow + UnassignedTop, 0);
            numUnassigned++;
        }
    };

    if (team == 0)
    {
        if (remote != 0)
        {
            // Filling the screen: the player starts unassigned.
            numUnassigned++;
            checkGoodToGo();
            return;
        }

        int32_t index = findSlot(team1Players, playerId);
        uint32_t groupID = MPlayer->innerSphereGroupID;
        uint32_t* players = team1Players;

        if (index < 0)
        {
            index = findSlot(team2Players, playerId);
            groupID = MPlayer->clanGroupID;
            players = team2Players;
        }

        if (index >= 0)
        {
            nameObject->moveTo(0xb, numUnassigned * UnassignedRow + UnassignedTop, 0);
            players[index] = 0xffffffff;
            MPlayer->sessionManager->RemovePlayerFromGroup(groupID, playerId);
            numUnassigned++;
        }
        else
        {
            layOutUnassigned(0xffffffff, 6, true);
        }

        globalLogPtr->playerLights->setPlayerStatus(playerId, 0);
    }
    else if (team == 1 || team == 2)
    {
        uint32_t* teamPlayers = team == 1 ? team1Players : team2Players;

        if (teamPlayers[slot] != 0xffffffff)
        {
            assignPlayer(teamPlayers[slot], 0, 0, 0);
        }

        int32_t index = findSlot(team1Players, playerId);

        if (index >= 0)
        {
            team1Players[index] = 0xffffffff;
            MPlayer->sessionManager->RemovePlayerFromGroup(MPlayer->innerSphereGroupID, playerId);
        }
        else if ((index = findSlot(team2Players, playerId)) >= 0)
        {
            team2Players[index] = 0xffffffff;
            MPlayer->sessionManager->RemovePlayerFromGroup(MPlayer->clanGroupID, playerId);
        }
        else
        {
            layOutUnassigned(playerId, numPlayers, false);
        }

        teamPlayers[slot] = playerId;
        uint32_t groupID = team == 1 ? MPlayer->innerSphereGroupID : MPlayer->clanGroupID;
        MPlayer->sessionManager->AddPlayerToGroup(groupID, playerId);

        if (MPlayer->sessionManager->myPlayer->id == playerId)
        {
            MPlayer->homeTeam = team == 1 ? 0 : 1;
            MPlayer->homeTeamGroupID = groupID;
            MPlayer->enemyTeamGroupID = team == 1 ? MPlayer->clanGroupID : MPlayer->innerSphereGroupID;
        }

        nameObject->moveTo(team == 1 ? 0xf8 : 0x1ba, slot * 0x16 + 0x133, 0);
        globalLogPtr->playerLights->setPlayerStatus(playerId, 1);

        if (remote != 0)
        {
            checkGoodToGo();
            return;
        }
    }
    else if (remote != 0)
    {
        checkGoodToGo();
        return;
    }

    if (MPlayer->isHost != 0)
    {
        MPJoinTeamMessage message{};
        message.header = FIMSG_GUARANTEED | MPMSG_JOIN_TEAM;
        message.playerID = playerId;
        message.team = team;
        message.slot = slot;
        MPlayer->sessionManager->SendMessageToGroup(0, &message, sizeof(message));
    }

    checkGoodToGo();
}

auto SessionScreen::setTeam1RP(int32_t resourcePoints) -> void
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", resourcePoints);
    team1RPText->setStringBuffer(text);
}

auto SessionScreen::setTeam2RP(int32_t resourcePoints) -> void
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", resourcePoints);
    team2RPText->setStringBuffer(text);
}

auto SessionScreen::setMap(char* fileName) -> void
{
    if (fileName == nullptr)
    {
        // No mission: clear the map box (the original wiped it in the background picture).
        ClearMap();
        mapBoxWiped = true;
        return;
    }

    ClearMap();

    auto* picture = new lPort;
    FullPathFileName path;
    path.init(terrainPath, fileName, ".log.tga");
    int32_t result = picture->init(path);

    if (result != 0)
    {
        Fatal(result, " Unable to create Port for TacMap ");
    }

    // The original stretched the map picture over the box in the background picture; the screen keeps the picture
    // and draws it so each frame.
    mapPicture = picture;
}

auto SessionScreen::DrawMap(_pane* target) -> void
{
    if (mapBoxWiped)
    {
        _pane box = *target;
        box.x0 = 0xf4;
        box.y0 = 0x33;
        box.x1 = 0x177;
        box.y1 = 0xb6;
        VFX_pane_wipe(&box, 0x10);
    }

    if (mapPicture == nullptr)
    {
        return;
    }

    // Stretch the map picture over the box.
    const int32_t maxU = mapPicture->frame()->window->x_max;
    const int32_t maxV = mapPicture->frame()->window->y_max;
    SCRNVERTEX corners[4] = {
        {0xf4, 0x33, 0, 0, 0, 0},
        {0x177, 0x33, 0, maxU << 16, 0, 0},
        {0x177, 0xb6, 0, maxU << 16, maxV << 16, 0},
        {0xf4, 0xb6, 0, 0, maxV << 16, 0},
    };

    VFX_map_polygon(target, 4, corners, mapPicture->frame()->window, MP_XP);
}

auto SessionScreen::ClearMap() -> void
{
    if (mapPicture != nullptr)
    {
        mapPicture->destroy();
        delete mapPicture;
        mapPicture = nullptr;
    }
}

auto SessionScreen::setMissionName(char* name) -> void
{
    if (missionName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(missionName);
        missionName = nullptr;
    }

    if (name != nullptr)
    {
        missionName = heapCopy(name);
    }
}

auto SessionScreen::setMapName(char* name) -> void
{
    if (mapName != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(mapName);
        mapName = nullptr;
    }

    if (name != nullptr)
    {
        mapName = heapCopy(name);
    }
}

auto SessionScreen::someoneCheckedIn() -> void
{
    int32_t allIn = -1;

    for (int32_t index = 0; index < 6; ++index)
    {
        uint32_t id = playerNames[index]->playerId;

        if (id == 0xffffffff)
        {
            continue;
        }

        // Original behaviour: the check-in flags are read by name slot, not by player number.
        if (MPlayer->playerSessionCheckIn[index] == 0)
        {
            allIn = 0;
        }
        else
        {
            globalLogPtr->playerLights->setPlayerStatus(id, 0);
        }
    }

    if (allIn != 0)
    {
        missionText->Clear();
        missionText->fontIndex = 0;

        if (MPlayer->isHost != 0)
        {
            controlsOn();
        }
    }
}

auto SessionScreen::fileReport(uint32_t playerId, int haveFile) -> void
{
    // Nothing to do unless an inquiry is out.
    int32_t index = 0;

    while (playerNames[index]->fileStatus == -1)
    {
        if (++index > 5)
        {
            return;
        }
    }

    for (PlayerNameObject* nameObject : playerNames)
    {
        if (nameObject->playerId == playerId)
        {
            nameObject->fileStatus = haveFile == 0 ? 1 : 2;
            break;
        }
    }

    int32_t allHave = -1;

    for (PlayerNameObject* nameObject : playerNames)
    {
        if (nameObject->fileStatus == 0)
        {
            return;
        }

        if (nameObject->fileStatus == 1)
        {
            allHave = 0;
        }
    }

    ReusableDialog* dialog = globalLogPtr->messageDialog;

    if (allHave != 0)
    {
        // Everyone has it: the waiting dialog closes itself.
        application->AddTimer(dialog, 0, 1000, 0, 0, 0);
        dialog->okButton->callback()->setExec(nullptr);
        checkGoodToGo();
        return;
    }

    // Name the players missing the file.
    char text[1024];
    cLoadString(thisInstance, 0xab, text, 0xfe);

    for (PlayerNameObject* nameObject : playerNames)
    {
        if (nameObject->fileStatus == 1)
        {
            std::strcat(text, nameObject->playerName);
        }

        nameObject->fileStatus = -1;
    }

    dialog->setText(text);
    dialog->setTwoButton(0);
    setDialogButton(MPCancelCallback, "bh_okay.tga", "bg_okay.tga");
    dialog = globalLogPtr->messageDialog;
    dialog->callback = nullptr;
    dialog->activate();
}

auto SessionScreen::loadMission(char* fileName) -> void
{
    PacketFile packetFile;
    FitIniFile iniFile;
    File textFile;
    FullPathFileName path;

    if (MPlayer->isHost == 0)
    {
        path.init(fileName, "", "");
    }
    else
    {
        path.init(savePath, fileName, ".mpk");
    }

    char text[256];
    uint32_t value = 0;

    int32_t result = packetFile.open(path, READ, 0x32);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open .MPK file ");

    if (result == 0)
    {
        result = packetFile.seekPacket(0);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find packet 0 in .MPK file ");

    if (result == 0)
    {
        result = iniFile.open(&packetFile, packetFile.getPacketSize(), 0x32);
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open packet 0 in .MPK file ");

        if (result == 0)
        {
            result = iniFile.seekBlock("Multiplayer");
        }
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find Multiplayer block in file ");

    if (result == 0)
    {
        result = iniFile.readIdString("LongMissionName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read mission name in Multiplayer file ");

    if (result == 0)
    {
        setMissionName(text);
        result = iniFile.readIdString("LongMapName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read map name in Multiplayer file ");

    if (result == 0)
    {
        setMapName(text);
        result = iniFile.readIdString("MapFileName", text, 0xfe);
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't read map file name in Multiplayer file ");

    if (result == 0)
    {
        setMap(text);

        if (iniFile.readIdULong("MissionTime", value) == 0)
        {
            std::snprintf(text, sizeof(text), "%01d:%02d", value / 60, value % 60);
            // Port fix: the original strcpy'd the text into the 10-byte label unchecked.
            MCStrCopy(globalLogPtr->sessionScreen->missionLabel, text);
        }

        result = iniFile.seekBlock("ResourcePoints");
    }

    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find ResourcePoints block in MPK file ");

    if (result == 0)
    {
        int32_t team1Missing = iniFile.readIdULong("Team1Points", value);

        if (team1Missing == 0)
        {
            setTeam1RP(static_cast<int32_t>(value));
        }

        int32_t team2Missing = iniFile.readIdULong("Team2Points", value);

        if (team2Missing == 0)
        {
            setTeam2RP(static_cast<int32_t>(value));
        }

        if (team1Missing != 0 || team2Missing != 0)
        {
            // An older file: one figure for both teams.
            if (iniFile.readIdULong("numPoints", value) != 0)
            {
                value = 0;
            }

            if (team1Missing != 0)
            {
                setTeam1RP(static_cast<int32_t>(value));
            }

            if (team2Missing != 0)
            {
                setTeam2RP(static_cast<int32_t>(value));
            }
        }
    }

    if (MPlayer->isHost != 0)
    {
        lockControls(iniFile.seekBlock("Lock") == 0);
    }

    iniFile.close();

    // Packet 1: the mission description.
    result = packetFile.seekPacket(1);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find packet 1 in .MPK file ");

    if (result == 0)
    {
        result = textFile.open(&packetFile, packetFile.getPacketSize(), 0x32);
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open packet 1 in .MPK file ");

        if (result == 0)
        {
            missionText->Clear();
            missionText->fontIndex = 0;

            while (textFile.eof() == 0)
            {
                textFile.readLine(reinterpret_cast<uint8_t*>(text), 0xfe);
                missionText->Print(text, 0x1f);
            }

            Assert(textFile.eof(), 0, "Error reading MP mission description");

            if (MPlayer->isHost != 0)
            {
                // Ask everyone whether they have the file, and wait for the answers.
                if (missionFile != nullptr)
                {
                    globalLogPtr->logisticsBlocks->Free(missionFile);
                }

                missionFile = heapCopy(path);
                MPlayer->sendFileInquiry(path);
                // The server has it already.
                uint32_t serverId = MPlayer->sessionManager->serverID;

                for (PlayerNameObject* nameObject : playerNames)
                {
                    if (nameObject->playerId != 0xffffffff)
                    {
                        nameObject->fileStatus = 0;
                    }

                    if (nameObject->playerId == serverId)
                    {
                        nameObject->fileStatus = 2;
                    }
                }

                char message[256];
                cLoadString(thisInstance, 0xac, message, 0xfe);
                ReusableDialog* dialog = globalLogPtr->messageDialog;
                dialog->setText(message);
                dialog->setTwoButton(0);
                dialog = globalLogPtr->messageDialog;
                dialog->callback = MPLoadWorkedCallback;
                setDialogButton(MPCancelCallback, "bh_cancl.tga", "bg_cancl.tga");
                globalLogPtr->messageDialog->activate();
            }

            return;
        }
    }

    // The file couldn't be read.
    setMissionName(nullptr);
    setMapName(nullptr);
    setMap(nullptr);
    std::memset(missionLabel, 0, sizeof(missionLabel));
    setTeam1RP(0);
    setTeam2RP(0);
    missionText->Clear();
    char format[256];
    char message[1024];
    cLoadString(thisInstance, 0xad, format, 0xfe);
    std::snprintf(message, sizeof(message), format, fileName);
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    dialog->setText(message);
    dialog->setTwoButton(0);
    setDialogButton(MPCancelCallback, "bh_okay.tga", "bg_okay.tga");
    dialog = globalLogPtr->messageDialog;
    dialog->callback = nullptr;
    dialog->activate();
}

auto SessionScreen::cancelMission() -> void
{
    setMissionName(nullptr);
    setMapName(nullptr);
    setMap(nullptr);

    if (missionFile != nullptr)
    {
        globalLogPtr->logisticsBlocks->Free(missionFile);
    }

    missionFile = nullptr;
    std::memset(missionLabel, 0, sizeof(missionLabel));
    setTeam1RP(0);
    setTeam2RP(0);
    missionText->Clear();

    for (PlayerNameObject* nameObject : playerNames)
    {
        nameObject->fileStatus = -1;
    }
}

auto SessionScreen::fillDPIDArray(uint32_t* ids, int32_t* count, int myTeam) -> void
{
    // Team 2 when the local player is on it and myTeam is set, or when it isn't and myTeam is clear.
    const bool onTeam2 = findSlot(team2Players, MPlayer->sessionManager->myPlayer->id) >= 0;
    *count = 0;
    const uint32_t* players = (myTeam != 0) == onTeam2 ? team2Players : team1Players;

    for (int32_t slot = 0; slot < TeamSlots; ++slot)
    {
        if (players[slot] != 0xffffffff)
        {
            ids[(*count)++] = players[slot];
        }
    }
}

auto SessionScreen::checkGoodToGo() -> void
{
    int32_t goodToGo = 0;

    if (numUnassigned == 0 && missionFile != nullptr && MPlayer->playersOnHomeTeam()->count != 0 &&
        MPlayer->playersOnEnemyTeam()->count != 0)
    {
        int32_t team1Filled = 0;

        for (int32_t slot = 0; slot < TeamSlots; ++slot)
        {
            if (team1Players[slot] != 0xffffffff)
            {
                team1Filled = -1;
            }

            if (team2Players[slot] != 0xffffffff)
            {
                goodToGo = -1;
            }

            if (team1Filled != 0 && goodToGo != 0)
            {
                break;
            }
        }

        goodToGo = team1Filled != 0 && goodToGo != 0 ? 1 : 0;
    }

    startButton->disabled = goodToGo == 0 ? 1 : 0;
}

auto SessionScreen::removePlayer(uint32_t playerId) -> void
{
    numPlayers--;

    if (numPlayers < 2)
    {
        // Too few left: the session ends.
        char text[256];
        cLoadString(thisInstance, launchedFromLobby == 0 ? 0xb0 : 0xba, text, 0xfe);
        ReusableDialog* dialog = globalLogPtr->messageDialog;
        dialog->setText(text);
        dialog->setTwoButton(0);
        globalLogPtr->messageDialog->callback = nullptr;
        setDialogButton(launchedFromLobby == 0 ? Cancel : GameOverMan, "bh_okay.tga", "bg_okay.tga");
        globalLogPtr->messageDialog->activate();
        return;
    }

    for (PlayerNameObject* nameObject : playerNames)
    {
        if (nameObject->playerId == playerId)
        {
            nameObject->ShowGUIWindow(0);
            nameObject->fileStatus = -1;
            nameObject->setPlayerId(0xffffffff);
            someoneCheckedIn();
            fileReport(0xffffffff, 0);
            globalLogPtr->playerLights->setPlayerStatus(playerId, 0);
            break;
        }
    }

    int32_t index = findSlot(team1Players, playerId);

    if (index >= 0)
    {
        team1Players[index] = 0xffffffff;
    }
    else if ((index = findSlot(team2Players, playerId)) >= 0)
    {
        team2Players[index] = 0xffffffff;
    }
    else
    {
        // An unassigned player: close up the list.
        numUnassigned = 0;

        for (PlayerNameObject* nameObject : playerNames)
        {
            uint32_t id = nameObject->playerId;

            if (id == 0xffffffff || findSlot(team1Players, id) >= 0 || findSlot(team2Players, id) >= 0)
            {
                continue;
            }

            nameObject->moveTo(0xb, numUnassigned * UnassignedRow + UnassignedTop, 0);
            numUnassigned++;
        }
    }

    checkGoodToGo();
}

auto SessionScreen::setTeamTechBase(char team, char techBase) -> void
{
    if (techBase != 1 && techBase != -1)
    {
        return;
    }

    lToolButton* isButton;
    lToolButton* clanButton;

    if (team == 1)
    {
        team1TechBase = techBase;
        isButton = team1ISButton;
        clanButton = team1ClanButton;
    }
    else if (team == 2)
    {
        team2TechBase = techBase;
        isButton = team2ISButton;
        clanButton = team2ClanButton;
    }
    else
    {
        return;
    }

    // The chosen toggle on, the other off.
    lToolButton* chosen = techBase == -1 ? clanButton : isButton;
    lToolButton* other = techBase == -1 ? isButton : clanButton;
    chosen->toggled = -1;
    other->toggled = 0;
}

auto SessionScreen::controlsOn() -> void
{
    team1RPText->inputType = lTextObject::INPUT_DIGITS;
    team2RPText->inputType = lTextObject::INPUT_DIGITS;
    team1RPUp->callback()->setExec(incrementTeam1RP);
    team1RPDown->callback()->setExec(decrementTeam1RP);
    team2RPUp->callback()->setExec(incrementTeam2RP);
    team2RPDown->callback()->setExec(decrementTeam2RP);

    for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
    {
        spinner->disabled = 0;
    }

    for (lToolButton* techButton : {team1ISButton, team1ClanButton, team2ISButton, team2ClanButton})
    {
        techButton->setEventRoutine(techTabRoutine);
    }

    for (PlayerNameObject* nameObject : playerNames)
    {
        nameObject->draggable = -1;
    }

    loadMissionButton->disabled = 0;
}

auto SessionScreen::controlsOff() -> void
{
    team1RPText->inputType = lTextObject::INPUT_NONE;
    team2RPText->inputType = lTextObject::INPUT_NONE;

    for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
    {
        spinner->callback()->setExec(nullptr);
    }

    for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
    {
        spinner->disabled = -1;
    }

    for (lToolButton* techButton : {team1ISButton, team1ClanButton, team2ISButton, team2ClanButton})
    {
        techButton->setEventRoutine(nullptr);
    }

    for (PlayerNameObject* nameObject : playerNames)
    {
        nameObject->draggable = 0;
    }

    loadMissionButton->disabled = -1;
}

auto SessionScreen::lockControls(int lock) -> void
{
    if (lock != 0)
    {
        // A locked mission: fixed points and tech bases (team 1 Inner Sphere, team 2 Clan).
        team1RPText->inputType = lTextObject::INPUT_NONE;
        team2RPText->inputType = lTextObject::INPUT_NONE;

        for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
        {
            spinner->callback()->setExec(nullptr);
        }

        for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
        {
            spinner->disabled = -1;
        }

        for (lToolButton* techButton : {team1ISButton, team1ClanButton, team2ISButton, team2ClanButton})
        {
            techButton->setEventRoutine(nullptr);
        }

        setTeamTechBase(1, 1);
        setTeamTechBase(2, -1);
        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, 1, 1);
        sendTwoLongs(FIMSG_GUARANTEED | MPMSG_TECHBASE_CHANGE, 2, -1);
        return;
    }

    team1RPText->inputType = lTextObject::INPUT_DIGITS;
    team2RPText->inputType = lTextObject::INPUT_DIGITS;
    team1RPUp->callback()->setExec(incrementTeam1RP);
    team1RPDown->callback()->setExec(decrementTeam1RP);
    team2RPUp->callback()->setExec(incrementTeam2RP);
    team2RPDown->callback()->setExec(decrementTeam2RP);

    for (lSpinnerButton* spinner : {team1RPUp, team1RPDown, team2RPUp, team2RPDown})
    {
        spinner->disabled = 0;
    }

    for (lToolButton* techButton : {team1ISButton, team1ClanButton, team2ISButton, team2ClanButton})
    {
        techButton->setEventRoutine(techTabRoutine);
    }
}
