#include "stdafx.h"
#include "logistics/MCLogComboBox.h"
#include "engine/MCFont.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "platform/MCRenderer.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>The drop-down's colours: the panel's back, the outline and lit row, the text.</summary>
    constexpr uint8_t ComboBackColor = 0x10;
    constexpr uint8_t ComboLineColor = 0x14;
    /// <summary>The sample a drop-down plays when it opens and when a row is chosen (the check boxes' press sound).</summary>
    constexpr uint32_t ComboClickSound = 16;
    /// <summary>The width of the field's arrow button, its left line included.</summary>
    constexpr int32_t ComboArrowWidth = 11;
}

MCLogComboBox::~MCLogComboBox()
{
    MCLogComboBox::Destroy();
}

auto MCLogComboBox::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t* setting, std::vector<Item> items,
                         std::function<void(int32_t value)> changed) -> void
{
    MCLogObject::Init(xPos, yPos, width, FieldHeight);
    _Setting = setting;
    _Items = std::move(items);
    _Changed = std::move(changed);
}

auto MCLogComboBox::Destroy() -> void
{
    Close();
    MCLogObject::Destroy();
}

auto MCLogComboBox::LabelColors() -> uint8_t*
{
    static uint8_t* table = []
    {
        static std::array<uint8_t, 256> colors{};

        for (int32_t i = 0; i < 256; ++i)
        {
            colors[static_cast<size_t>(i)] = i == 0xff ? 0xff : 0xe3;
        }

        MCRenderer::RegisterData(colors.data(), colors.size(), MCDataKind::Tables);
        return colors.data();
    }();

    return table;
}

auto MCLogComboBox::WriteLabel(int32_t xPos, int32_t yPos, const std::string& text) -> void
{
    VfxStringDraw(_Port->Frame(), xPos, yPos, WhiteFont->FontData.get(), text.c_str(), LabelColors());
}

auto MCLogComboBox::Draw() -> void
{
    if (!_Port->ViewOpen())
    {
        return;
    }

    const auto outline = [this](int16_t left, int16_t top, int16_t right, int16_t bottom)
    {
        FillBox(left, top, right, top, ComboLineColor);
        FillBox(left, bottom, right, bottom, ComboLineColor);
        FillBox(left, top, left, bottom, ComboLineColor);
        FillBox(right, top, right, bottom, ComboLineColor);
    };

    const auto right = static_cast<int16_t>(Width() - 1);
    const auto bottom = static_cast<int16_t>(Height() - 1);
    FillBox(0, 0, right, bottom, ComboBackColor);

    // The field: the choice, then the arrow button at the right end (a triangle pointing down).
    outline(0, 0, right, FieldHeight - 1);
    const auto arrowLeft = static_cast<int16_t>(Width() - ComboArrowWidth);
    FillBox(arrowLeft, 0, arrowLeft, FieldHeight - 1, ComboLineColor);
    const auto arrowMiddle = static_cast<int16_t>(arrowLeft + ComboArrowWidth / 2);

    for (int16_t row = 0; row < 3; row++)
    {
        FillBox(arrowMiddle - 2 + row, 4 + row, arrowMiddle + 2 - row, 4 + row, 0xe3);
    }

    const int32_t selected = Selected();

    if (selected >= 0)
    {
        WriteLabel(2, 3, _Items[static_cast<size_t>(selected)].Label);
    }

    if (!_Open)
    {
        return;
    }

    // The list, under the field (sharing its bottom line): a row per item shown, the lit one filled.
    outline(0, FieldHeight - 1, right, bottom);
    const int32_t rows = VisibleRows();

    for (int32_t i = 0; i < rows; i++)
    {
        const int32_t item = _FirstRow + i;
        const auto top = static_cast<int16_t>(FieldHeight + i * RowHeight);

        if (item == _Hovered)
        {
            FillBox(1, top, right - 1, top + RowHeight - 1, ComboLineColor);
        }

        WriteLabel(2, top + 2, _Items[static_cast<size_t>(item)].Label);
    }

    // A long list shows where it is scrolled to: a thumb along its right edge.
    const auto count = static_cast<int32_t>(_Items.size());

    if (count > rows)
    {
        const int32_t track = rows * RowHeight;
        const int32_t thumbTop = FieldHeight + track * _FirstRow / count;
        const int32_t thumbBottom = FieldHeight + track * (_FirstRow + rows) / count - 1;
        FillBox(right - 2, static_cast<int16_t>(thumbTop), right - 2, static_cast<int16_t>(thumbBottom), 0xe3);
    }
}

auto MCLogComboBox::Selected() const -> int32_t
{
    for (size_t i = 0; i < _Items.size(); i++)
    {
        if (_Items[i].Value == *_Setting)
        {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

auto MCLogComboBox::VisibleRows() const -> int32_t
{
    return std::min(static_cast<int32_t>(_Items.size()), MaxRows);
}

auto MCLogComboBox::Open() -> void
{
    if (_Open || _Items.empty())
    {
        return;
    }

    _Open = true;
    _FirstRow = 0;
    Hover(std::max(Selected(), 0));
    Resize(Width(), FieldHeight + VisibleRows() * RowHeight + 1);
    RaiseAmongSiblings();
    GuiSystem()->Grab(this);
    PlayLogSound(ComboClickSound);
}

auto MCLogComboBox::Close() -> void
{
    if (!_Open && !_HoldUntilRelease)
    {
        return;
    }

    CloseList();
    _HoldUntilRelease = false;

    if (GuiSystem()->GrabbedObject() == this)
    {
        GuiSystem()->Release();
    }
}

auto MCLogComboBox::CloseList() -> void
{
    if (_Open)
    {
        _Open = false;
        Resize(Width(), FieldHeight);
    }
}

auto MCLogComboBox::Choose(int32_t index) -> void
{
    if (index < 0 || index >= static_cast<int32_t>(_Items.size()))
    {
        return;
    }

    const int32_t value = _Items[static_cast<size_t>(index)].Value;

    if (*_Setting == value)
    {
        return;
    }

    *_Setting = value;

    if (_Changed)
    {
        _Changed(value);
    }
}

auto MCLogComboBox::Hover(int32_t index) -> void
{
    const int32_t rows = VisibleRows();
    _Hovered = std::clamp(index, 0, static_cast<int32_t>(_Items.size()) - 1);

    if (_Hovered < _FirstRow)
    {
        _FirstRow = _Hovered;
    }
    else if (_Hovered >= _FirstRow + rows)
    {
        _FirstRow = _Hovered - rows + 1;
    }
}

auto MCLogComboBox::InField(int32_t xPos, int32_t yPos) -> bool
{
    const int32_t localX = xPos - GlobalX();
    const int32_t localY = yPos - GlobalY();
    return localX >= 0 && localX < Width() && localY >= 0 && localY < FieldHeight;
}

auto MCLogComboBox::RowAt(int32_t xPos, int32_t yPos) -> int32_t
{
    if (!_Open)
    {
        return -1;
    }

    const int32_t localX = xPos - GlobalX();
    const int32_t localY = yPos - FieldHeight - GlobalY();

    if (localX < 0 || localX >= Width() || localY < 0)
    {
        return -1;
    }

    const int32_t row = localY / RowHeight;
    return row < VisibleRows() ? _FirstRow + row : -1;
}

auto MCLogComboBox::RaiseAmongSiblings() -> void
{
    if (Parent == nullptr)
    {
        return;
    }

    std::vector<MCGuiObject*>& siblings = Parent->ChildList;
    const auto last = siblings.end();
    const auto at = std::ranges::find(siblings, this);

    if (at == last)
    {
        return;
    }

    // The children are sorted by depth, front-most last: this one goes after the others of its depth.
    auto end = at + 1;

    while (end != last && (*end)->Depth() <= WinDepth)
    {
        ++end;
    }

    std::rotate(at, at + 1, end);
}

auto MCLogComboBox::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case MCGuiEventType::LeftButtonDown:
        case MCGuiEventType::RightButtonDown:
        {
            // Closed, the control is the field alone. Open, a press on the field or outside closes the list and keeps
            // the mouse until it is let go, so neither the press nor its release reaches what is under it; a press on
            // a row chooses on its release.
            if (!_Open)
            {
                Open();
            }
            else if (RowAt(event->X, event->Y) < 0)
            {
                CloseList();
                _HoldUntilRelease = true;
            }
            break;
        }
        case MCGuiEventType::LeftButtonUp:
        {
            const int32_t row = RowAt(event->X, event->Y);

            if (_HoldUntilRelease)
            {
                Close();
            }
            else if (row >= 0)
            {
                Close();
                PlayLogSound(ComboClickSound);
                Choose(row);
            }
            break;
        }
        case MCGuiEventType::MouseMove:
        {
            const int32_t row = RowAt(event->X, event->Y);

            if (row >= 0)
            {
                _Hovered = row;
            }
            break;
        }
        case MCGuiEventType::KeyDown:
        {
            const uint8_t key = event->Key;

            if (key == VK_UP || key == VK_DOWN)
            {
                const int32_t step = key == VK_UP ? -1 : 1;

                if (_Open)
                {
                    Hover(_Hovered + step);
                }
                else
                {
                    Choose(std::clamp(Selected() + step, 0, static_cast<int32_t>(_Items.size()) - 1));
                }
            }
            else if (key == VK_RETURN)
            {
                if (_Open)
                {
                    const int32_t row = _Hovered;
                    Close();
                    Choose(row);
                }
                else
                {
                    Open();
                }
            }
            else if (key == VK_ESCAPE && _Open)
            {
                Close();
            }
            else if (!_Open && Parent != nullptr)
            {
                // Other keys are the screen's (Escape leaves it).
                Parent->HandleEvent(event);
            }
            break;
        }
        default:
            break;
    }
}

auto MCLogComboBox::MouseWheel(int32_t steps, [[maybe_unused]] int32_t xPos, [[maybe_unused]] int32_t yPos) -> bool
{
    if (_Items.empty() || _HoldUntilRelease)
    {
        return true;
    }

    if (_Open)
    {
        Hover(_Hovered + steps);
    }
    else
    {
        Choose(std::clamp(Selected() + steps, 0, static_cast<int32_t>(_Items.size()) - 1));
    }

    return true;
}
