#include "stdafx.h"
#include "gui/MCGuiComboBox.h"
#include "gui/MCGuiButton.h"
#include "gui/MCGuiListBox.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCGuiTextObject.h"

auto CloseListOnMousedown(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    if (event->Type == MCGuiEventType::LeftButtonDown)
    {
        APostMessage(obj->Parent, MCGuiComboBox::ToggleList);
    }
}

MCGuiComboBox::~MCGuiComboBox()
{
    MCGuiComboBox::Destroy();
}

auto MCGuiComboBox::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, const char* name) -> int32_t
{
    if (const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name); result != 0)
    {
        return result;
    }

    // The original uses each new part unchecked.
    ListBox = MCMakeGui<MCGuiListBox>();
    ListBox->Init(10, 10, width, height, nullptr);
    ListBox->ShowGuiWindow(false);
    AddChild(ListBox.get());
    ListBox->MoveTo(2, height + 2);
    ListBox->SetEventRoutine(CloseListOnMousedown);

    // The drop button is sized (height, width): its art sets the real size.
    DropButton = MCMakeGui<MCGuiToolButton>();
    DropButton->Init(10, 10, height, width, nullptr);
    DropButton->SetUpPicture(0xe);
    DropButton->SetDownPicture(0xf);
    DropButton->Callback()->SetMessage(this, ToggleList);
    AddChild(DropButton.get());
    const int32_t buttonWidth = DropButton->Width();
    DropButton->MoveTo(Width() - (buttonWidth + 2), 2);

    TextField = MCMakeGui<MCGuiTextObject>();
    TextField->Init(10, 10, (width - (buttonWidth + 2)) - 2, height - 4, nullptr);
    AddChild(TextField.get());
    TextField->MoveTo(2, 2);
    TextField->ReadOnly = true;
    SetBackColor(3);
    return 0;
}

auto MCGuiComboBox::Destroy() -> void
{
    TextField.reset();
    ListBox.reset();
    DropButton.reset();
    MCGuiObject::Destroy();
}

auto MCGuiComboBox::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type != ToggleList)
    {
        return;
    }

    if (ListBox->IsShowing())
    {
        // Close the list and take its selection.
        ListBox->ShowGuiWindow(false);
        const std::string* selected = ListBox->GetItemString(ListBox->SelectedItem);
        TextField->SetText(selected != nullptr ? std::string_view(*selected) : std::string_view());
        DropButton->Pushed = false;
        return;
    }

    ListBox->ShowGuiWindow(true);
    DropButton->Pushed = true;
}

auto MCGuiComboBox::Draw() -> void
{
    DrawFramed(false, true);
    MCGuiObject::Draw();
}

auto MCGuiComboBox::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    MCGuiObject::Resize(newWidth, newHeight);
    ListBox->Resize(newWidth, ListBox->Height());
    ListBox->MoveTo(2, newHeight + 2);
    const int32_t buttonX = newWidth - (DropButton->Width() + 2);
    DropButton->MoveTo(buttonX, 2);
    TextField->Resize(buttonX - 2, newHeight - 4);
}

auto MCGuiComboBox::ChangeItemString(int32_t item, std::string_view text) const -> bool
{
    if (!ListBox->ChangeItemString(item, text))
    {
        return false;
    }

    if (item == ListBox->SelectedItem)
    {
        TextField->SetText(text);
    }

    return true;
}

auto MCGuiComboBox::SelectItem(int32_t item) const -> bool
{
    if (!ListBox->SelectItem(item))
    {
        return false;
    }

    const std::string* selected = ListBox->GetItemString(item);
    TextField->SetText(selected != nullptr ? std::string_view(*selected) : std::string_view());
    return true;
}
