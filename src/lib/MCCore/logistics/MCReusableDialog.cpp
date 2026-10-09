#include "stdafx.h"
#include "logistics/MCReusableDialog.h"
#include "gui/MCGuiEvent.h"
#include "gui/MCGuiFont.h"
#include "lib/MCFatal.h"
#include "main/main.h"

namespace
{
    /// <summary>Loads a frame piece of the dialog.</summary>
    std::unique_ptr<MCLogPort> LoadPiece(std::string_view fileName)
    {
        auto piece = std::make_unique<MCLogPort>();
        piece->Load(fileName);
        return piece;
    }

    /// <summary>The height of a line of the dialog's text.</summary>
    int32_t TextLineHeight()
    {
        return MedBlueFont->Height() + 3;
    }
}

// MCReusableDialog

auto MCReusableDialog::Init([[maybe_unused]] int32_t xPos, [[maybe_unused]] int32_t yPos,
                            [[maybe_unused]] int32_t width, [[maybe_unused]] int32_t height,
                            [[maybe_unused]] const char* name) -> int32_t
{
    // The box is its top, some middle pieces and its bottom, centred across the screen at y 200.
    TopPiece = LoadPiece("dbox_top.tga");
    MiddlePiece = LoadPiece("dbox_middle.tga");
    BottomPiece = LoadPiece("dbox_bottom.tga");
    const int32_t boxWidth = TopPiece->Width();
    int32_t result = MCLogObject::Init(GuiSystem()->Width() / 2 - boxWidth / 2, 200, boxWidth,
                                       BottomPiece->Height() + MiddlePiece->Height() + TopPiece->Height());
    Assert(result == 0, static_cast<uint32_t>(result), "Error initializing reusable dialog");

    OkButton = MCMakeGui<MCLogDialogButton>();
    result = OkButton->Init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), "Error initializing reusable dialog");
    AddChild(OkButton.get());
    CancelButton = MCMakeGui<MCLogDialogButton>();
    result = CancelButton->Init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), "Error initializing reusable dialog");
    AddChild(CancelButton.get());
    SetTwoButton(false);
    ShowGuiWindow(false);
    SetDepth(100);
    TimeoutResult = 0;
    KeepCallbacks = false;
    return 0;
}

auto MCReusableDialog::Destroy() -> void
{
    TopPiece.reset();
    MiddlePiece.reset();
    BottomPiece.reset();
    OkButton.reset();
    CancelButton.reset();
    Text.clear();
    MCLogObject::Destroy();
}

auto MCReusableDialog::DrawFrame() -> void
{
    int32_t pieceY = 0;

    if (TopPiece != nullptr)
    {
        TopPiece->CopyTo(Lport()->Frame(), 0, 0, true);
        pieceY = TopPiece->Height();
    }

    for (int32_t i = 0; i < NumMiddlePieces; i++)
    {
        if (MiddlePiece != nullptr)
        {
            MiddlePiece->CopyTo(Lport()->Frame(), 0, pieceY, true);
            pieceY += MiddlePiece->Height();
        }
    }

    if (BottomPiece != nullptr)
    {
        BottomPiece->CopyTo(Lport()->Frame(), 0, pieceY, true);
    }
}

auto MCReusableDialog::Draw() -> void
{
    DrawFrame();

    // The text, word-wrapped to the box: each line ends where the font says the words fit, and the character there
    // (a space) is skipped.
    int32_t lineY = TopPiece->Height() + 2;
    std::string_view rest = Text;
    auto fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(rest, WrapWidth(), true));

    if (fit == rest.size())
    {
        MedBlueFont->WriteString(_Port->Frame(), 0xc, lineY, rest);
    }
    else
    {
        while (fit > 0 && fit <= rest.size())
        {
            MedBlueFont->WriteString(_Port->Frame(), 0xc, lineY, rest.substr(0, fit));
            rest = rest.substr(fit < rest.size() ? fit + 1 : fit);
            fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(rest, WrapWidth(), true));
            lineY += TextLineHeight();
        }
    }

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        DrawChild(ChildList[i]);
    }
}

auto MCReusableDialog::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == MCGuiEventType::KeyDown)
    {
        if (event->Key == 0x0d)
        {
            OkButton->Callback()->Execute();
            Deactivate(TwoButton ? -1 : 0);
        }
        else if (event->Key == 0x1b)
        {
            // Original behaviour (OB-074): Escape runs the OK button's callback too.
            OkButton->Callback()->Execute();
            Deactivate(0);
        }
    }
    else if (event->Type == MCGuiEventType::Timer)
    {
        // Timed out.
        Deactivate(TimeoutResult);
    }

    // While grabbed, clicks go to the child under the mouse.
    if (GuiSystem()->GrabbedObject() == this)
    {
        MCGuiObject* target = FindObject(event->X, event->Y);

        if (target != nullptr && target != this)
        {
            target->HandleEvent(event);
            return;
        }
    }

    MCGuiObject::HandleEvent(event);
}

auto MCReusableDialog::Activate() -> void
{
    GuiSystem()->Grab(this);
    // The original painted the dialog afresh: a press left on its buttons was gone.
    OkButton->PressedDown = false;
    CancelButton->PressedDown = false;
    MoveTo(0x140 - Width() / 2, 0xf0 - Height() / 2, false);
    ShowGuiWindow(true);

    if (Timeout > 0)
    {
        GuiSystem()->AddTimer(this, TimeoutTimer, Timeout, 0, 0, 0);
    }
}

auto MCReusableDialog::Deactivate(int32_t dialogResult) -> void
{
    GuiSystem()->Release();
    ShowGuiWindow(false);

    // The original also skipped a callback pointer IsBadReadPtr rejected.
    if (Callback)
    {
        Callback(dialogResult);
    }

    if (KeepCallbacks)
    {
        KeepCallbacks = false;
        return;
    }

    Callback = nullptr;
    OkButton->Callback()->SetExec(nullptr);
    CancelButton->Callback()->SetExec(nullptr);
    GuiSystem()->RemoveTimer(this, TimeoutTimer);
    Timeout = 0;
    TimeoutResult = 0;
}

auto MCReusableDialog::SetText(std::string_view newText) -> void
{
    Text = newText.substr(0, newText.find('\0'));

    // Count the wrapped lines; each middle piece holds two.
    std::string_view rest = Text;
    auto fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(rest, WrapWidth(), true));
    int32_t lines = 1;

    while (fit >= 1 && fit < rest.size())
    {
        rest = rest.substr(fit + 1);
        lines++;
        fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(rest, WrapWidth(), true));
    }

    NumMiddlePieces = (lines + 1) / 2;
    Resize(Width(), MiddlePiece->Height() * NumMiddlePieces + BottomPiece->Height() + TopPiece->Height());
    SetTwoButton(TwoButton);
}

auto MCReusableDialog::SetTwoButton(bool twoButtons) -> void
{
    TwoButton = twoButtons;
    const int32_t buttonY = Height() - 0x17;

    if (TwoButton)
    {
        CancelButton->ShowGuiWindow(true);
        CancelButton->MoveTo(0x68, buttonY, false);
        OkButton->MoveTo(0x23, buttonY, false);
        return;
    }

    CancelButton->ShowGuiWindow(false);
    OkButton->MoveTo(0x68, buttonY, false);
}

// MCRefitDialog

auto MCRefitDialog::SetText(std::string_view newText) -> void
{
    Text = newText.substr(0, newText.find('\0'));
    // The text is a comma-separated list, one item per line, between six lines of framing text.
    const auto lines = static_cast<int32_t>(7 + std::ranges::count(Text, ','));
    NumMiddlePieces = lines / 2;
    NumItems = lines - 6;
    Resize(Width(), MiddlePiece->Height() * NumMiddlePieces + BottomPiece->Height() + TopPiece->Height());
    MCReusableDialog::SetTwoButton(TwoButton);
}

auto MCRefitDialog::Draw() -> void
{
    DrawFrame();
    int32_t lineY = WrapText(LoadGameString(0x54, 0xfe), TopPiece->Height() + 2);
    lineY += TextLineHeight();

    // One item per line, cut at the commas; the last item takes the rest.
    std::string_view items = Text;

    for (int32_t i = NumItems; i > 0; i--)
    {
        const size_t comma = items.find(',');
        MedBlueFont->WriteString(_Port->Frame(), 0x14, lineY, items.substr(0, comma));
        lineY += TextLineHeight();

        if (comma != std::string_view::npos)
        {
            items = items.substr(comma + 1);
        }
    }

    WrapText(LoadGameString(0x62, 0xfe), lineY + TextLineHeight());

    for (size_t i = 0; i < ChildList.size(); i++)
    {
        DrawChild(ChildList[i]);
    }
}

auto MCRefitDialog::WrapText(std::string_view text, int32_t yPos) -> int32_t
{
    text = text.substr(0, text.find('\0'));
    auto fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(text, WrapWidth(), true));

    if (fit == text.size())
    {
        MedBlueFont->WriteString(_Port->Frame(), 0xc, yPos, text);
        return yPos;
    }

    // Each piece ends where the font says the words fit; the space there is skipped. The last piece ends at the end
    // of the text (OB-075: the original wrote over the end and read on past it).
    while (fit > 0)
    {
        MedBlueFont->WriteString(_Port->Frame(), 0xc, yPos, text.substr(0, fit));
        yPos += TextLineHeight();

        if (fit >= text.size())
        {
            break;
        }

        text = text.substr(fit + 1);
        fit = static_cast<size_t>(MedBlueFont->CharactersToWidth(text, WrapWidth(), true));
    }

    return yPos;
}
