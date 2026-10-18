#include "stdafx.h"
#include "logistics/MCLogLabel.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiGlobals.h"
#include "logistics/MCLogComboBox.h"
#include "vfx/MCVfxFunctions.h"

MCLogLabel::MCLogLabel(std::string_view text) : _Text(text)
{
}

auto MCLogLabel::InitAt(int32_t xPos, int32_t yPos) -> void
{
    MCLogObject::Init(xPos, yPos, std::max(WhiteFont->Width(_Text), 1), std::max(WhiteFont->Height(), 1));
    SetTransparent(true);
}

auto MCLogLabel::Draw() -> void
{
    VfxStringDraw(Lport()->Frame(), 0, 0, WhiteFont->FontData.Bytes(), _Text, MCLogComboBox::LabelColors());
}
