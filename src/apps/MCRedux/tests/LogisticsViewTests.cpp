#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "gui/asystem.h"
#include "gui/scrlpane.h"
#include "logistics/logmain.h"
#include "logistics/logscrn.h"
#include "logistics/lport.h"
#include "main/logistics.h"
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
