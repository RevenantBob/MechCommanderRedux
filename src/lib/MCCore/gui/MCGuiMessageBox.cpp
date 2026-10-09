#include "stdafx.h"
#include "gui/MCGuiMessageBox.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "vfx/MCVfxFunctions.h"

MCGuiMessageBox::~MCGuiMessageBox()
{
    OkButton.reset();
}

auto MCGuiMessageBox::Init(std::string_view text) -> int32_t
{
    if (WhiteFont == nullptr)
    {
        return -3;
    }

    const int32_t boxWidth = std::max(WhiteFont->Width(text) + 0xc, 0x48);
    const int32_t fontHeight = WhiteFont->Height();
    const int32_t screenWidth = GuiSystem()->Width();
    const int32_t screenHeight = GuiSystem()->Height();
    int32_t result = MCGuiObject::Init((screenWidth - boxWidth) / 2, (screenHeight - (fontHeight + 0x28)) / 2, boxWidth,
                                       fontHeight + 0x28, nullptr);

    if (result != 0)
    {
        return result;
    }

    OkButton = MCMakeGui<MCGuiButton>();
    result = OkButton->Init((boxWidth - 0x30) / 2, fontHeight + 0xe, 0x3c, 0x14, nullptr);

    if (result != 0)
    {
        return result;
    }

    OkButton->SetUpPicture(0x10);
    OkButton->SetDownPicture(0x11);
    OkButton->Callback()->SetExec(DestroyVersion);
    OkButton->SetDepth(100);
    AddChild(OkButton.get());
    // The box is drawn by Draw, each frame.
    Message = text;
    return 0;
}

auto MCGuiMessageBox::Draw() -> void
{
    MCPane* pane = DisplayPort->Frame();
    VfxPaneWipe(pane, 0x11);
    WhiteFont->WriteString(pane, (Width() - WhiteFont->Width(Message)) / 2, 8, Message);
    DrawBox(0x1f, -1, -1, -1, -1);
    MCGuiObject::Draw();
}

auto MCGuiMessageBox::Destroy() -> void
{
    OkButton.reset();
    MCGuiObject::Destroy();
}

auto MCGuiMessageBox::HandleEvent(MCGuiEvent* event) -> void
{
    if (PointInside(event->X, event->Y))
    {
        OkButton->HandleEvent(event);
    }
}
