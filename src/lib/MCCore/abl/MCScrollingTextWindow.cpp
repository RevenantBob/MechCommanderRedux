#include "stdafx.h"
#include "abl/MCScrollingTextWindow.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiPort.h"
#include "vfx/MCVfxFunctions.h"

MCScrollingTextWindow::~MCScrollingTextWindow()
{
    MCGuiObject::Destroy();
}

auto MCScrollingTextWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    const int32_t err = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    NumColumns = width / 10;
    NumLines = height / 10;
    VfxPaneWipe(Port()->Frame(), BackColor());
    return 0;
}

auto MCScrollingTextWindow::Resize(int32_t width, int32_t height) -> void
{
    NumColumns = width / 10;
    NumLines = height / 10;
    MCGuiObject::Resize(width, height);
    VfxPaneWipe(Port()->Frame(), BackColor());
}

auto MCScrollingTextWindow::Draw() -> void
{
    MCGuiObject::Draw();
}

auto MCScrollingTextWindow::HandleEvent(MCGuiEvent* event) -> void
{
    MCGuiObject::HandleEvent(event);
}

auto MCScrollingTextWindow::Clear() -> void
{
    NumColumns = 0;
    NumLines = 0;
}

auto MCScrollingTextWindow::Print(char* s) -> void
{
    MCPane* pane = Port()->Frame();
    VfxPaneScroll(pane, 0, -10, 0, BackColor());
    SystemFont->WriteString(pane, 2, NumLines * 10 - 10, s, -1);
}
