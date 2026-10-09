#include "stdafx.h"
#include "abl/MCAblDebuggerWindow.h"
#include "gui/MCGuiTextObject.h"

MCAblDebuggerWindow::~MCAblDebuggerWindow()
{
    _Input.reset();
    _Output.reset();
    MCGuiTitleWindow::Destroy();
}

auto MCAblDebuggerWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    int32_t err = MCGuiTitleWindow::Init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    SetBackColor(10);

    _Output = MCMakeGui<MCScrollingTextWindow>();
    err = _Output->Init(0, 0, width, height - 40, const_cast<char*>("ABL Out"));

    if (err != 0)
    {
        return err;
    }

    AddChild(_Output.get());

    _Input = MCMakeGui<MCGuiTextObject>();
    err = _Input->Init(0, Height() - 36, 260, 36, nullptr);

    if (err != 0)
    {
        return err;
    }

    _Input->SetText(const_cast<char*>("\"?\" for help"));
    AddChild(_Input.get());
    Draw();
    return 0;
}

auto MCAblDebuggerWindow::Destroy() -> void
{
    _Input.reset();
    _Output.reset();
    MCGuiTitleWindow::Destroy();
}

auto MCAblDebuggerWindow::Resize(int32_t width, int32_t height) -> void
{
    _Output->Resize(width, height - 40);
    _Input->Resize(width, 36);
    _Input->MoveTo(0, height - 36);
    MCGuiTitleWindow::Resize(width, height);
    Draw();
}
