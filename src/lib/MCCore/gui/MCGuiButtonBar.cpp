#include "stdafx.h"
#include "gui/MCGuiButtonBar.h"

template <typename TButton>
auto MCGuiButtonLayout::LayOut(MCGuiObject* bar, const std::vector<MCGuiOwned<TButton>>& buttons) const -> void
{
    const auto numButtons = static_cast<int32_t>(buttons.size());
    const int32_t across = std::min(numButtons, MaxLength);
    const int32_t lines = (numButtons + MaxLength - 1) / MaxLength;
    const int32_t columns = Horizontal ? across : lines;
    const int32_t rows = Horizontal ? lines : across;
    bar->Resize(columns * ButtonWidth, rows * ButtonHeight);

    for (int32_t i = 0; i < numButtons; i++)
    {
        const int32_t line = i / MaxLength;
        const int32_t step = i % MaxLength;
        const int32_t column = Horizontal ? step : line;
        const int32_t row = Horizontal ? line : step;
        buttons[i]->MoveTo(column * ButtonWidth, row * ButtonHeight);
    }
}

// MCGuiToolBar

auto MCGuiToolBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    const int32_t result = MCGuiTitleWindow::Init(xPos, yPos, width, height, name);
    SetBackColor(0);

    if (TitleBar != nullptr)
    {
        TitleBar->ShowCloseButton(false);
    }

    MaxLength = 8;
    return result;
}

auto MCGuiToolBar::Destroy() -> void
{
    Buttons.clear();
    MCGuiTitleWindow::Destroy();
}

auto MCGuiToolBar::AddButton(MCGuiOwned<MCGuiToolButton> button) -> void
{
    AddChild(button.get());
    Buttons.push_back(std::move(button));
    PlaceButtons();
}

auto MCGuiToolBar::RemoveButton(int32_t index) -> bool
{
    if (index < 0 || index >= NumButtons())
    {
        return false;
    }

    Buttons.erase(Buttons.begin() + index);
    PlaceButtons();
    return true;
}

auto MCGuiToolBar::GetButton(int32_t index) -> MCGuiToolButton*
{
    return index >= 0 && index < NumButtons() ? Buttons[index].get() : nullptr;
}

auto MCGuiToolBar::IsPushed(int32_t index) -> bool
{
    const MCGuiToolButton* button = GetButton(index);
    return button != nullptr && button->Pushed;
}

auto MCGuiToolBar::SetHorizontal(bool on) -> void
{
    Horizontal = on;
    PlaceButtons();
}

auto MCGuiToolBar::SetMaxLength(int32_t length) -> void
{
    MaxLength = length;
    PlaceButtons();
}

auto MCGuiToolBar::PlaceButtons() -> void
{
    LayOut(this, Buttons);
}

auto MCGuiToolBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    ButtonWidth = width;
    ButtonHeight = height;
}

// MCGuiWindowBar

auto MCGuiWindowBar::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);
    SetBackColor(0);
    MaxLength = 8;
    return result;
}

auto MCGuiWindowBar::Destroy() -> void
{
    Buttons.clear();
    MCGuiObject::Destroy();
}

auto MCGuiWindowBar::InsertButton(MCGuiOwned<MCGuiButton> button, int32_t index) -> bool
{
    if (index < 0 || index > NumButtons())
    {
        return false;
    }

    AddChild(button.get());
    Buttons.insert(Buttons.begin() + index, std::move(button));
    PlaceButtons();
    return true;
}

auto MCGuiWindowBar::AddButton(MCGuiOwned<MCGuiButton> button) -> void
{
    InsertButton(std::move(button), NumButtons());
}

auto MCGuiWindowBar::RemoveButton(int32_t index) -> bool
{
    if (index < 0 || index >= NumButtons())
    {
        return false;
    }

    Buttons.erase(Buttons.begin() + index);
    PlaceButtons();
    return true;
}

auto MCGuiWindowBar::GetButton(int32_t index) -> MCGuiButton*
{
    return index >= 0 && index < NumButtons() ? Buttons[index].get() : nullptr;
}

auto MCGuiWindowBar::SetHorizontal(bool on) -> void
{
    Horizontal = on;
    PlaceButtons();
}

auto MCGuiWindowBar::SetMaxLength(int32_t length) -> void
{
    MaxLength = length;
    PlaceButtons();
}

auto MCGuiWindowBar::PlaceButtons() -> void
{
    LayOut(this, Buttons);
}

auto MCGuiWindowBar::SetButtonSize(int32_t width, int32_t height) -> void
{
    ButtonWidth = width;
    ButtonHeight = height;
}
