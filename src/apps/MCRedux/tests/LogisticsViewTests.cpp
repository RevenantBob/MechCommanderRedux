#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "gui/asystem.h"
#include "lib/inifile.h"
#include "gui/scrlpane.h"
#include "logistics/logdlg.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "logistics/logsession.h"
#include "logistics/logscrn.h"
#include "logistics/lport.h"
#include "main/logistics.h"
#include "platform/MCFileSystem.h"
#include "platform/MCPresenter.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>
    /// The chat history as the original kept it: a picture wiped to 0x10, moved up by each line's height, the strip
    /// along the bottom wiped and the line written there (LogChatWindow::processChatString @ 0x0070b2a0).
    /// </summary>
    void AddLineAsOriginal(lPort* picture, const char* line)
    {
        std::string text = line;
        auto* bytes = reinterpret_cast<uint8_t*>(text.data());
        const int32_t width = picture->width();
        const int32_t height = picture->height();
        const int32_t used = application->textFormatter.process(bytes, nullptr, width, 0);
        uint8_t* pixels = picture->bitmap()->buffer;
        std::memmove(pixels, pixels + width * used, static_cast<size_t>((height - used) * width));
        _pane bottom = *picture->frame();
        bottom.x0 = 0;
        bottom.y0 = height - used - 1;
        bottom.x1 = width - 1;
        bottom.y1 = height - 1;
        VFX_pane_wipe(&bottom, 0x10);
        application->textFormatter.process(bytes, picture, 0, height - used - 1);
    }

    /// <summary>Draws the chat window's history view into a picture of its size (scrolled to its top).</summary>
    std::vector<uint8_t> DrawHistory(LogChatWindow* chat)
    {
        lPort* view = chat->historyPane->contentPort;
        lPort picture;
        picture.init(view->width(), view->height(), -1);
        VFX_pane_wipe(picture.frame(), 0xff);
        view->openView(picture.bitmap(), 0, 0, MCRect{0, 0, view->width() - 1, view->height() - 1}, false);
        view->DrawContent(view);
        view->closeView();
        const uint8_t* pixels = picture.bitmap()->buffer;
        std::vector<uint8_t> result(pixels, pixels + static_cast<size_t>(view->width()) * view->height());
        picture.destroy();
        return result;
    }
}

/// <summary>
/// Renderer phase 3 step 4g: the logistics chat history, now a list of lines drawn each frame, shows exactly what the
/// original's picture held after the same lines: wrapped and coloured lines, lines pushed off the top, and the history
/// kept across a resize and gone after a reset.
/// </summary>
TEST_CASE_ISOLATED("game: the logistics chat history draws as the original's picture")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    LogChatWindow* chat = globalLogPtr->chatWindow;
    REQUIRE(chat != nullptr);
    lPort* view = chat->historyPane->contentPort;
    REQUIRE(view->isView());

    lPort original;
    original.init(view->width(), view->height(), -1);
    VFX_pane_wipe(original.frame(), 0x10);

    const auto same = [&]
    {
        const std::vector<uint8_t> drawn = DrawHistory(chat);
        const uint8_t* expected = original.bitmap()->buffer;
        return std::equal(drawn.begin(), drawn.end(), expected);
    };

    CHECK(same());

    std::vector<std::string> lines = {
        "%fc4Alpha: %fc6hello",
        "%fc2Bravo: %fc4a line long enough to wrap onto a second line and then onto a third one in the history",
        "%fc5Charlie: %fc6short",
        "%fc3Delta: %fc4%%100 sure",
    };

    // Enough lines to push the first ones off the top.
    for (int32_t i = 0; i < 80; i++)
    {
        lines.push_back(std::format("%fc{}Echo: %fc6message number {} of the flood", i % 8, i));
    }

    for (size_t i = 0; i < lines.size(); i++)
    {
        MCTest::Scope scope(std::format("line {}", i));
        AddLineAsOriginal(&original, lines[i].c_str());
        chat->AddLine(lines[i].c_str());
        CHECK(same());
    }

    // A resize keeps the history (the original copied its picture into the new pane).
    chat->resize(0xe7);
    CHECK(chat->historyPane->contentPort->isView());
    CHECK(same());

    // A reset clears it.
    chat->reset();
    VFX_pane_wipe(original.frame(), 0x10);
    CHECK(same());
    AddLineAsOriginal(&original, lines[1].c_str());
    chat->AddLine(lines[1].c_str());
    CHECK(same());
    original.destroy();
}

namespace
{
    /// <summary>
    /// The preferences screen's drop-downs, as AddPreferenceDropDowns places them: each field 76 wide and 11 tall at
    /// (512, top), its list's rows 10 tall from top + 11. DIFFICULTY's field is at y 176 (EASY, REGULAR, HARD),
    /// RENDERER's at y 243 (VULKAN, SOFTWARE).
    /// </summary>
    struct PrefsDropDown
    {
        int32_t Top = 0;

        int32_t FieldX() const { return 540; }
        int32_t FieldY() const { return Top + 5; }
        int32_t RowY(int32_t row) const { return Top + 11 + row * 10 + 5; }
    };

    constexpr PrefsDropDown DifficultyDropDown{176};
    constexpr PrefsDropDown RendererDropDown{243};

    /// <summary>The preferences screen's drop-down whose field lies at <paramref name="dropDown"/>.</summary>
    lComboBox* FindDropDown(const PrefsDropDown& dropDown)
    {
        GenericScreen* screen = globalLogPtr->prefScreen;

        for (int32_t i = 0; i < screen->numberOfChildren(); i++)
        {
            if (auto* combo = dynamic_cast<lComboBox*>(screen->child(i));
                combo != nullptr && combo->globalY() == dropDown.Top)
            {
                return combo;
            }
        }

        return nullptr;
    }
}

/// <summary>
/// The preferences screen's RENDERER drop-down: choosing SOFTWARE says it takes effect after a restart, CANCEL puts
/// the old choice back (and the field shows it), ACCEPT writes it to PREFS "Renderer"; the running game keeps its
/// renderer.
/// </summary>
TEST_CASE_ISOLATED("game: the preferences screen chooses the renderer and saves it in PREFS")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    // The tests' user folder overlays the install, so a PREFS written here would be every later test's.
    const std::filesystem::path prefsPath = MCFileSystem::UserRoot() / "prefs.cfg";
    std::filesystem::remove(prefsPath);
    const int32_t running = gRenderer;
    const int32_t chosen = gRendererPreference;
    REQUIRE_EQ(chosen, static_cast<int32_t>(MCRendererKind::Vulkan));

    const auto frames = []
    {
        for (int32_t frame = 0; frame < 5; frame++)
        {
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    };

    // Choosing another renderer than the running one says it needs a restart; OK closes the message.
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    const auto closeRestartNotice = [&](const char* shot)
    {
        CHECK(dialog->showWindow != 0);
        MCScreenInput::SaveShot(shot, MCScreenInput::ScreenHash());
        lDialogButton* ok = dialog->okButton;
        MCScreenInput::Click(ok->globalX() + ok->width() / 2, ok->globalY() + ok->height() / 2);
        frames();
        CHECK(dialog->showWindow == 0);
    };

    const auto choose = [&](int32_t row)
    {
        MCScreenInput::Click(RendererDropDown.FieldX(), RendererDropDown.FieldY());
        frames();
        MCScreenInput::Click(RendererDropDown.FieldX(), RendererDropDown.RowY(row));
        frames();
    };

    ShowPreferences();
    frames();
    const uint32_t vulkanShown = MCScreenInput::ScreenHash();
    MCScreenInput::SaveShot("prefs vulkan", vulkanShown);
    choose(1);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Software));
    closeRestartNotice("prefs restart notice");
    const uint32_t softwareShown = MCScreenInput::ScreenHash();
    MCScreenInput::SaveShot("prefs software", softwareShown);
    CHECK(softwareShown != vulkanShown);

    // VULKAN back: no message (it's the running renderer); VULKAN again: no change, no message.
    choose(0);
    CHECK(dialog->showWindow == 0);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    CHECK_EQ(MCScreenInput::ScreenHash(), vulkanShown);
    choose(0);
    CHECK(dialog->showWindow == 0);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    CHECK_EQ(MCScreenInput::ScreenHash(), vulkanShown);
    choose(1);
    closeRestartNotice("prefs restart notice again");
    CHECK_EQ(MCScreenInput::ScreenHash(), softwareShown);

    // CANCEL puts the choice back, and the field shows it when the screen opens again.
    CancelPrefs();
    frames();
    CHECK_EQ(gRendererPreference, chosen);
    ShowPreferences();
    frames();
    CHECK_EQ(MCScreenInput::ScreenHash(), vulkanShown);

    choose(1);
    closeRestartNotice("prefs restart notice before accept");
    WritePrefs();
    frames();
    CHECK_EQ(gRenderer, running);

    char renderer[32] = {};
    {
        FitIniFile prefs;
        REQUIRE_EQ(prefs.open(prefsPath.string().c_str()), 0);
        REQUIRE_EQ(prefs.seekBlock("MechCommander"), 0);
        CHECK_EQ(prefs.readIdString("Renderer", renderer, sizeof(renderer) - 1), 0);
    }

    std::filesystem::remove(prefsPath);
    CHECK(std::string_view(renderer) == "software");
}

/// <summary>
/// The preferences screen's drop-downs with a player's mouse and keys (SDL events through CheckMouse and the key
/// messages): a click on the field opens the list in front of the screen and takes the mouse, a click on a row chooses
/// it and closes, a press outside closes without a change and goes no further, a drag from the field to a row chooses
/// on the release; up/down and the wheel move the choice of a closed field under the mouse and the lit row of an open
/// list, Return chooses the lit row and Escape closes. Switching back and forth many times keeps working, the restart
/// message shows each time SOFTWARE is chosen and never for the same item again, and the screen looks as it did once
/// everything is back.
/// </summary>
TEST_CASE_ISOLATED("game: the preferences drop-downs work with a player's mouse and keys")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    REQUIRE_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    REQUIRE(gRenderer != static_cast<int32_t>(MCRendererKind::Software));
    ReusableDialog* dialog = globalLogPtr->messageDialog;
    GenericScreen* screen = globalLogPtr->prefScreen;
    ShowPreferences();
    MCScreenInput::RealMove(320, 400);
    lComboBox* renderer = FindDropDown(RendererDropDown);
    lComboBox* difficulty = FindDropDown(DifficultyDropDown);
    REQUIRE(renderer != nullptr);
    REQUIRE(difficulty != nullptr);
    const int32_t startDifficulty = GameDifficulty;
    const uint32_t startShown = MCScreenInput::ScreenHash();
    MCScreenInput::SaveShot("dropdown closed", startShown);

    // The original's DIFFICULTY checks are gone from the screen.
    for (int32_t i = 0; i < 3; i++)
    {
        CHECK(screen->elements[8 + i]->IsShowing() == 0);
    }

    const auto closeNotice = [&]
    {
        if (dialog->showWindow != 0)
        {
            lDialogButton* ok = dialog->okButton;
            MCScreenInput::RealClick(ok->globalX() + ok->width() / 2, ok->globalY() + ok->height() / 2);
            CHECK(dialog->showWindow == 0);
        }
    };

    const auto chooseRenderer = [&](int32_t row, MCRendererKind expected, bool notice)
    {
        MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
        CHECK(renderer->IsOpen());
        CHECK(application->grabbedObject() == renderer);
        MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.RowY(row));
        CHECK(!renderer->IsOpen());
        // (The message, when it shows, takes the mouse itself.)
        CHECK(application->grabbedObject() != renderer);
        CHECK_EQ(gRendererPreference, static_cast<int32_t>(expected));
        CHECK_EQ(dialog->showWindow != 0, notice);
        closeNotice();
        CHECK(application->grabbedObject() == nullptr);
    };

    // Open: in front of every other child of the screen, with its list; the look differs from the closed one.
    MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    REQUIRE(renderer->IsOpen());
    CHECK(screen->child(screen->numberOfChildren() - 1) == renderer);
    CHECK_EQ(renderer->height(), lComboBox::FieldHeight + 2 * lComboBox::RowHeight + 1);
    CHECK_EQ(renderer->Hovered(), 0);
    MCScreenInput::RealMove(RendererDropDown.FieldX(), RendererDropDown.RowY(1));
    CHECK_EQ(renderer->Hovered(), 1);
    const uint32_t openShown = MCScreenInput::ScreenHash();
    MCScreenInput::SaveShot("dropdown open", openShown);
    CHECK(openShown != startShown);

    // A press outside closes it with no change, and doesn't reach what's there (ACCEPT would leave the screen).
    constexpr int32_t AcceptX = 546;
    constexpr int32_t AcceptY = 283;
    CHECK(dynamic_cast<lButton*>(screen->findObject(AcceptX, AcceptY)) != nullptr);
    MCScreenInput::RealClick(AcceptX, AcceptY);
    CHECK(!renderer->IsOpen());
    CHECK(application->grabbedObject() == nullptr);
    CHECK(screen->IsShowing() != 0);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    MCScreenInput::RealMove(320, 400);
    CHECK_EQ(MCScreenInput::ScreenHash(), startShown);

    // A click on the field again closes it.
    MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    CHECK(renderer->IsOpen());
    MCScreenInput::RealClick(RendererDropDown.FieldX() + 30, RendererDropDown.FieldY());
    CHECK(!renderer->IsOpen());
    CHECK(application->grabbedObject() == nullptr);

    for (int32_t round = 0; round < 3; round++)
    {
        MCTest::Scope scope(std::format("round {}", round));
        chooseRenderer(1, MCRendererKind::Software, true);
        chooseRenderer(0, MCRendererKind::Vulkan, false);
        // The boxes' titles and outlines are not controls: clicks there change nothing.
        MCScreenInput::RealClick(540, 232);
        MCScreenInput::RealClick(481, 250);
        MCScreenInput::RealClick(540, 165);
        CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
        CHECK(dialog->showWindow == 0);
        CHECK(application->grabbedObject() == nullptr);
    }

    // The chosen item again: no change, no message.
    chooseRenderer(0, MCRendererKind::Vulkan, false);
    chooseRenderer(1, MCRendererKind::Software, true);
    chooseRenderer(1, MCRendererKind::Software, false);
    chooseRenderer(0, MCRendererKind::Vulkan, false);

    // A drag from the field to a row chooses it on the release.
    MCScreenInput::RealMove(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    MCScreenInput::RealButton(true);
    CHECK(renderer->IsOpen());
    MCScreenInput::RealMove(RendererDropDown.FieldX(), RendererDropDown.RowY(1));
    MCScreenInput::RealButton(false);
    CHECK(!renderer->IsOpen());
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Software));
    CHECK(dialog->showWindow != 0);
    closeNotice();

    // Keys on the open list: down/up move the lit row, Escape closes with no change, Return chooses.
    MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    CHECK_EQ(renderer->Hovered(), 1);
    MCScreenInput::RealKey(SDL_SCANCODE_UP);
    CHECK_EQ(renderer->Hovered(), 0);
    MCScreenInput::RealKey(SDL_SCANCODE_UP);
    CHECK_EQ(renderer->Hovered(), 0);
    MCScreenInput::RealKey(SDL_SCANCODE_ESCAPE);
    CHECK(!renderer->IsOpen());
    CHECK(screen->IsShowing() != 0);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Software));
    MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    MCScreenInput::RealKey(SDL_SCANCODE_UP);
    MCScreenInput::RealKey(SDL_SCANCODE_RETURN);
    CHECK(!renderer->IsOpen());
    CHECK(application->grabbedObject() == nullptr);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    CHECK(dialog->showWindow == 0);

    // The wheel on the open list moves the lit row (down is the next row); Return chooses it.
    MCScreenInput::RealClick(RendererDropDown.FieldX(), RendererDropDown.FieldY());
    CHECK_EQ(renderer->Hovered(), 0);
    MCScreenInput::RealWheel(-1);
    CHECK_EQ(renderer->Hovered(), 1);
    CHECK(renderer->IsOpen());
    MCScreenInput::RealKey(SDL_SCANCODE_RETURN);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Software));
    CHECK(dialog->showWindow != 0);
    closeNotice();

    // A closed field under the mouse: the wheel and up/down move the choice, clamped at the ends.
    MCScreenInput::RealMove(DifficultyDropDown.FieldX(), DifficultyDropDown.FieldY());
    GameDifficulty = 1;
    MCScreenInput::RealWheel(1);
    CHECK_EQ(GameDifficulty, 0);
    MCScreenInput::RealWheel(1);
    CHECK_EQ(GameDifficulty, 0);
    MCScreenInput::RealWheel(-1);
    CHECK_EQ(GameDifficulty, 1);
    MCScreenInput::RealKey(SDL_SCANCODE_DOWN);
    CHECK_EQ(GameDifficulty, 2);
    MCScreenInput::RealKey(SDL_SCANCODE_DOWN);
    CHECK_EQ(GameDifficulty, 2);
    MCScreenInput::RealKey(SDL_SCANCODE_UP);
    CHECK_EQ(GameDifficulty, 1);
    CHECK(!difficulty->IsOpen());

    // DIFFICULTY with the mouse: each row in turn, several rounds.
    for (int32_t round = 0; round < 2; round++)
    {
        for (int32_t level : {2, 0, 1, 1, 2})
        {
            MCTest::Scope scope(std::format("round {}, difficulty {}", round, level));
            MCScreenInput::RealClick(DifficultyDropDown.FieldX(), DifficultyDropDown.FieldY());
            CHECK(difficulty->IsOpen());
            CHECK(screen->child(screen->numberOfChildren() - 1) == difficulty);
            MCScreenInput::RealClick(DifficultyDropDown.FieldX(), DifficultyDropDown.RowY(level));
            CHECK(!difficulty->IsOpen());
            CHECK_EQ(GameDifficulty, level);
            CHECK(dialog->showWindow == 0);
        }
    }

    // Back to where it started: the screen shows what it showed then.
    GameDifficulty = startDifficulty;
    chooseRenderer(0, MCRendererKind::Vulkan, false);
    MCScreenInput::RealMove(320, 400);
    CHECK_EQ(MCScreenInput::ScreenHash(), startShown);
    CancelPrefs();
    MCScreenInput::RealMove(320, 400);
    CHECK_EQ(gRendererPreference, static_cast<int32_t>(MCRendererKind::Vulkan));
    CHECK_EQ(GameDifficulty, startDifficulty);
}

/// <summary>
/// OB-131: a scroll pane's arrow lets go wherever the mouse button comes up. The original let go only on a release
/// over the slider column: let go anywhere else, the arrow stayed pressed and the slider would not drag.
/// </summary>
TEST_CASE_ISOLATED("game: a scroll pane's arrow lets go wherever the mouse button comes up")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());

    // A pane 0x80 x 0x60 at (100, 100) showing content four times its height: the slider is a quarter of the track.
    auto* pane = new ScrollPane;
    pane->init();
    pane->init(0x80, 0x60, 100, 100, static_cast<char*>(nullptr));
    auto* content = new lPort;
    content->init(0x80 - 13, 0x180, -1);
    pane->setDisplayPort(content, -1, -1);
    REQUIRE(pane->sliderHeight > 0);

    const auto send = [pane](int32_t type, int32_t xPos, int32_t yPos)
    {
        aEvent event;
        event.clear();
        event.type = type;
        event.x = xPos;
        event.y = yPos;
        pane->handleEvent(&event);
    };

    // Press the down arrow (it shows pressed while held), then let go left of the column.
    const int32_t column = pane->globalX() + pane->width() - 6;
    send(1, column, pane->globalY() + pane->height() - 5);
    CHECK_EQ(pane->HeldArrow(), 2);
    send(4, pane->globalX() + 10, pane->globalY() + 10);
    CHECK_EQ(pane->HeldArrow(), 0);

    // The slider drags.
    const int32_t before = pane->sliderPos;
    const int32_t sliderY = pane->globalY() + before + pane->sliderHeight / 2;
    send(1, column, sliderY);
    send(7, column, sliderY + 10);
    send(4, column, sliderY + 10);
    CHECK_EQ(pane->sliderPos, before + 10);

    delete pane;
}

/// <summary>
/// The save list's slider: a list longer than the pane gets a slider with its picture (the port made none for the
/// file panes, so drawing the column read a null texture).
/// </summary>
TEST_CASE_ISOLATED("game: a save list longer than its pane draws its slider")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());
    auto* pane = new FileScrollPane;
    pane->ScrollPane::init();
    pane->init(10, 10, 0xc0, 0x80);

    // Four panes' worth of files.
    lPort* content = pane->contentPort;
    content->initView(0xc0 - 13, 0x200);
    pane->setDisplayPort(content, 0, -1);
    REQUIRE(pane->sliderHeight > 0);
    CHECK(pane->sliderTexture != nullptr);
    CHECK_EQ(pane->sliderTexture->Height, pane->sliderHeight);

    // The column draws: the slider's rows land at its place.
    lPort picture;
    picture.init(13, 0x80, -1);
    VFX_pane_wipe(picture.frame(), 0);
    pane->DrawSliderColumn(picture.frame(), 0, 0, false);
    const uint8_t* pixels = picture.bitmap()->buffer;
    CHECK_EQ(pixels[(pane->sliderPos + 1) * 13 + 5], uint8_t{0x1a});
    picture.destroy();
    delete pane;
}
