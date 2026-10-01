#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "platform/MCInput.h"
#include "platform/MCStringTable.h"

TEST_CASE("input: key lParam and Windows-1252")
{
    MCInput::MCScanCode escape = MCInput::Win32ScanCode(SDL_SCANCODE_ESCAPE);
    CHECK_EQ(escape.Code, 0x01);
    CHECK_EQ(MCInput::MakeKeyLParam(escape, false, false, false), 0x00010001);
    MCInput::MCScanCode up = MCInput::Win32ScanCode(SDL_SCANCODE_UP);
    CHECK(up.Extended);
    CHECK_EQ(static_cast<uint32_t>(MCInput::MakeKeyLParam(up, false, true, true)), 0xc1480001u);
    CHECK_EQ(MCInput::VirtualKeyFromScancode(SDL_SCANCODE_KP_8, SDLK_UNKNOWN, true), VK_NUMPAD8);
    CHECK_EQ(MCInput::VirtualKeyFromScancode(SDL_SCANCODE_KP_8, SDLK_UNKNOWN, false), VK_UP);
    CHECK_EQ(MCInput::CodePointToWindows1252('A'), 'A');
    CHECK_EQ(MCInput::CodePointToWindows1252(0x20ac), 0x80);
    CHECK_EQ(MCInput::CodePointToWindows1252(0x00e9), 0xe9);
    CHECK_EQ(MCInput::CodePointToWindows1252(0x4e00), -1);
    CHECK_EQ(MCInput::MakePointLParam(-1, 2), 0x0002ffff);
}

TEST_CASE("input: posted messages reach the window procedure")
{
    MCInput::Reset();
    std::vector<uint32_t> seen;
    MCInput::SetWindowProc(
        [&seen](uint32_t message, uint32_t, int32_t)
        {
            seen.push_back(message);
            return 0;
        });
    MCInput::PostMessage(WM_USER + 1, 0, 0);
    CHECK(MCInput::PumpMessages());
    REQUIRE_EQ(seen.size(), size_t{1});
    CHECK_EQ(seen[0], WM_USER + 1);
    MCInput::PostMessage(WM_QUIT, 3, 0);
    CHECK(!MCInput::PumpMessages());
    CHECK_EQ(MCInput::QuitCode(), 3);
    MCInput::SetWindowProc({});
    MCInput::Reset();
}

TEST_CASE("game: MCX.EXE string table loads")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    const MCStringTable& table = MCStringTable::Game();
    CHECK(table.Count() > 100);
    char buffer[256];
    int found = 0;

    for (uint32_t id = 0; id < 0x1000 && found == 0; id++)
    {
        found = table.LoadString(id, buffer, sizeof(buffer));
    }

    CHECK(found > 0);
    CHECK_EQ(static_cast<int>(std::strlen(buffer)), found);
}
