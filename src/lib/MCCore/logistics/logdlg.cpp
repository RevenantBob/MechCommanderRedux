#include "stdafx.h"
#include "logistics/logdlg.h"
#include "gui/afont.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "main/main.h"
#include "sound/soundsys.h"
#include "vfx/MCVfxFunctions.h"

namespace
{
    /// <summary>Which way a held purchase spinner arrow counts (1 up, 0 down; 0x008080cc).</summary>
    int32_t SpinUp = 0;

    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            port->Destroy();
            delete port;
            port = nullptr;
        }
    }

    /// <summary>A copy of <paramref name="text"/> with the CRT's new (null stays null).</summary>
    char* CopyString(const char* text)
    {
        if (text == nullptr)
        {
            return nullptr;
        }

        auto* copy = new char[std::strlen(text) + 1];
        std::strcpy(copy, text);
        return copy;
    }
}

// 0x008015d0 is AlphaTable row 0x10c (AlphaTable is at 0x007f09d0): the alpha colour the dialogs fade through.
char* LogisticDlgfade = reinterpret_cast<char*>(AlphaTable.data()) + 0x10c * 256;

// lDialogButton

auto MCLogDialogButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    PressedDown = 0;
    Result = 0;
    return MCLogButton::Init(xPos, yPos, width, height, name);
}

auto MCLogDialogButton::Draw() -> void
{
    DrawFace(Disabled != 0 ? GrayPicture : (PressedDown != 0 ? DownPicture : UpPicture), true);
}

auto MCLogDialogButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (Disabled != 0)
    {
        return;
    }

    if (event->Type == 4)
    {
        PressedDown = 0;
    }

    if (event->Type == 1)
    {
        // Show the press, then close the dialog with this button's result.
        PressedDown = -1;
        UpdateDisplay(0, 0, 0, 0, 0);
        SoundSystem->PlayDigitalSample(0x34, 1, nullptr, 0, 0);
        Callback()->Execute();
        static_cast<MCReusableDialog*>(Parent)->Deactivate(Result);
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

// LogDialogBox

auto MCLogDialogBox::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    TwoButton = -1;
    Spinner = -1;
    Callback = nullptr;
    PicturePort = nullptr;
    // The original loaded the box's frame (lspcb00) as its port; the box draws the frame each frame instead.
    MCLogObject::Init(xPos, yPos, width, height, nullptr, nullptr);
    SetTransparent(-1);
    ShowGuiWindow(0);
    FadedBackground = nullptr;
}

auto MCLogDialogBox::Destroy() -> void
{
    FreePort(FadedBackground);
    FreePort(PicturePort);
    Callback = nullptr;
    MCLogObject::Destroy();
    FreePort(_OwnPort);
}

auto MCLogDialogBox::DrawBackground() -> void
{
    if (NeedBackground != 0)
    {
        // The original copied the screen under the box into fadedBackground here and darkened it, then filled it
        // with 0x10, which is all the box shows of it.
        Application->SetCursorVisible(0);
        UpdateDisplay(0, 0, 0, 0, 0);
        Application->SetCursorVisible(-1);
        NeedBackground = 0;
    }

    Pressed = PressedPart::None;
}

auto MCLogDialogBox::Draw() -> void
{
    DrawBox();
    DrawPressed();
}

auto MCLogDialogBox::DrawPressed() -> void
{
    // Each part's pressed art and where it goes.
    struct PressedArt
    {
        const char* Name = nullptr;
        int32_t X = 0;
        int32_t Y = 0;
    };

    static constexpr PressedArt arts[] = {{"lspcb03.tga", 0x3f, 0x82},
                                          {"lspcb04.tga", 0x76, 0x82},
                                          {"lspcb07.tga", 0x92, 0x53},
                                          {"lspcb09.tga", 0x92, 0x5b}};

    if (Pressed == PressedPart::None)
    {
        return;
    }

    const PressedArt& art = arts[static_cast<int32_t>(Pressed) - 1];

    if (MCLogPort* picture = LogArtf("%slogart\\%s", ArtPath, art.Name); picture != nullptr)
    {
        picture->CopyTo(_OwnPort->Frame(), art.X, art.Y, -1);
    }
}

auto MCLogDialogBox::DrawBox() -> void
{
    MCPane* port = _OwnPort->Frame();
    // The box was its frame's picture: the fill covers the frame, not the whole pane.
    MCLogPort* frameArt = LogArtf("%slogart\\lspcb00.tga", ArtPath);
    MCPane fill = *port;
    fill.X1 = fill.X0 + frameArt->Width() - 1;
    fill.Y1 = fill.Y0 + frameArt->Height() - 1;
    VfxPaneWipe(&fill, 0x10);
    frameArt->CopyTo(port, 0, 0, -1);

    // Without a spinner its place is left as the frame is (the original copied a transparent block there).
    if (Spinner != 0)
    {
        VfxPaneCopy(LogArtf("%slogart\\lspcb05.tga", ArtPath)->Frame(), 0, 0, port, 0x92, 0x53, -1);
        VfxPaneCopy(LogArtf("%slogart\\lspcb06.tga", ArtPath)->Frame(), 0, 0, port, 0x92, 0x5b, -1);
    }

    LogArtf("%slogart\\lspcb01.tga", ArtPath)->CopyTo(port, 0x3f, 0x82, -1);

    if (TwoButton != 0)
    {
        LogArtf("%slogart\\lspcb02.tga", ArtPath)->CopyTo(port, 0x76, 0x82, -1);
    }

    if (PicturePort != nullptr)
    {
        PicturePort->CopyTo(port, 10, 0x1b, -1);
    }
}

auto MCLogDialogBox::SetTwoButton(int twoButtons) -> void
{
    TwoButton = twoButtons;
}

auto MCLogDialogBox::SetSpinner(int newSpinner) -> void
{
    Spinner = newSpinner;
}

auto MCLogDialogBox::Activate() -> void
{
    NeedBackground = -1;
    FreePort(FadedBackground);
    Application->Grab(this);
    BringToFront(0);
    DrawBackground();
    ShowGuiWindow(-1);
}

auto MCLogDialogBox::Deactivate(int dialogResult) -> void
{
    Application->Release();
    ShowGuiWindow(0);

    if (Callback != nullptr)
    {
        Callback(dialogResult);
    }
}

auto MCLogDialogBox::SetCallback(void (*newCallback)(int)) -> void
{
    Callback = newCallback;
}

auto MCLogDialogBox::SetPort(MCLogPort* port) -> void
{
    _SharedPort = port;
}

// PurchaseDlg

auto MCPurchaseDlg::Init(int32_t newPurchaseType, int32_t newUnitCost, int32_t newMaxQuantity, char* newTitle,
                         char* newSubtitle, MCLogPort* picture) -> void
{
    if (newMaxQuantity < 0)
    {
        newMaxQuantity = 199;
    }

    Quantity = 1;
    PurchaseType = newPurchaseType;
    UnitCost = newUnitCost;
    Title = CopyString(newTitle);
    Subtitle = CopyString(newSubtitle);

    if (PicturePort != nullptr)
    {
        delete PicturePort;
    }

    if (picture == nullptr)
    {
        PicturePort = nullptr;
    }
    else
    {
        PicturePort = new MCLogPort;
        PicturePort->Init(picture->Width(), picture->Height(), -1);
        VfxPaneCopy(picture->Frame(), 0, 0, PicturePort->Frame(), 0, 0, -1);
    }

    MaxQuantity = newMaxQuantity;
    // A single item needs no spinner.
    Spinner = newMaxQuantity != 1 ? -1 : 0;
}

auto MCPurchaseDlg::Destroy() -> void
{
    delete[] Subtitle;
    Subtitle = nullptr;
    delete[] Title;
    Title = nullptr;
    MCLogDialogBox::Destroy();
}

auto MCPurchaseDlg::HandleEvent(MCGuiEvent* event) -> void
{
    const int32_t localX = event->X - GlobalX();
    const int32_t localY = event->Y - GlobalY();
    // Spinner arrows only work for purchases (even types) and type 5.
    const bool arrowsLocked = (PurchaseType & 1) != 0 && PurchaseType != 5;
    auto canAddOne = [this]() { return Quantity < MaxQuantity && UnitCost * (Quantity + 1) <= ResourcePoints; };

    switch (event->Type)
    {
        case 1:
        {
            if (localX >= 0x40 && localX <= 0x6e && localY >= 0x83 && localY <= 0x8e)
            {
                // OK.
                SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
                Pressed = PressedPart::Ok;
                UpdateDisplay(0, 0, 0, 0, 0);
                Deactivate(-1);
            }
            else if (localX >= 0x77 && localX <= 0xa5 && localY >= 0x83 && localY <= 0x8e)
            {
                // Cancel.
                SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
                Pressed = PressedPart::Cancel;
                UpdateDisplay(0, 0, 0, 0, 0);
                Deactivate(0);
            }
            else if (Spinner != 0)
            {
                if (localX >= 0x92 && localX <= 0x9a && localY >= 0x53 && localY <= 0x59)
                {
                    // Up: held down it repeats on timer 6.
                    if (arrowsLocked)
                    {
                        break;
                    }

                    DrawBackground();
                    Pressed = PressedPart::Up;
                    SpinUp = 1;
                    Application->AddTimer(this, 6, 200, 0, 0, 0);
                    Application->Grab(this);

                    if (canAddOne())
                    {
                        SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
                        Quantity++;
                        break;
                    }

                    SoundSystem->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
                }
                else if (localX >= 0x92 && localX <= 0x9a && localY >= 0x5b && localY <= 0x61 && !arrowsLocked)
                {
                    // Down.
                    DrawBackground();
                    Pressed = PressedPart::Down;
                    SpinUp = 0;
                    Application->AddTimer(this, 6, 200, 0, 0, 0);
                    Application->Grab(this);

                    if (Quantity != 0)
                    {
                        SoundSystem->PlayDigitalSample(0xf, 1, nullptr, 0, 0);
                        Quantity--;
                        break;
                    }

                    SoundSystem->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
                }
            }
            break;
        }
        case 4:
        {
            DrawBackground();
            Application->RemoveTimer(this, 6);
            break;
        }
        case 9:
        {
            if (event->Key == 0x0d)
            {
                Deactivate(TwoButton != 0 ? -1 : 0);
            }
            else if (event->Key == 0x1b)
            {
                Deactivate(0);
            }
            break;
        }
        case 0x13:
        {
            // The held arrow repeats (up to 200).
            if (SpinUp == 0)
            {
                if (Quantity != 0)
                {
                    Quantity--;
                }

                DrawBackground();
                Pressed = PressedPart::Down;
            }
            else
            {
                if (Quantity < MaxQuantity && Quantity < 200 && UnitCost * (Quantity + 1) <= ResourcePoints)
                {
                    Quantity++;
                }

                DrawBackground();
                Pressed = PressedPart::Up;
            }
            break;
        }

        default:
            break;
    }
}

auto MCPurchaseDlg::DrawBackground() -> void
{
    MCLogDialogBox::DrawBackground();
}

auto MCPurchaseDlg::Draw() -> void
{
    DrawBox();
    char text[256];
    MCPane* port = _OwnPort->Frame();
    // Labels: price, resource points, quantity, remaining.
    CLoadString(ThisInstance, 0x48, text, 0xfe);
    MedWhiteFont->WriteString(port, 0x15, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    CLoadString(ThisInstance, 0x4b, text, 0xfe);
    MedWhiteFont->WriteString(port, 0x91, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    CLoadString(ThisInstance, 0x49, text, 0xfe);
    MedWhiteFont->WriteString(port, 0x15, 0x57, reinterpret_cast<uint8_t*>(text), -1);
    CLoadString(ThisInstance, 0x4a, text, 0xfe);
    MedWhiteFont->WriteString(port, 0x16, 0x6c, reinterpret_cast<uint8_t*>(text), -1);
    CLoadString(ThisInstance, 0x4b, text, 0xfe);
    MedWhiteFont->WriteString(port, 0x91, 0x6c, reinterpret_cast<uint8_t*>(text), -1);

    if (Title != nullptr)
    {
        MedWhiteFont->WriteString(port, 0x2a, 0x20, reinterpret_cast<uint8_t*>(Title), -1);
    }

    if (Subtitle != nullptr)
    {
        MCGuiFont* font = PurchaseType == 3 ? MedRedFont : MedWhiteFont;
        font->WriteString(port, 0x2a, 0x2e, reinterpret_cast<uint8_t*>(Subtitle), -1);
    }

    // The numbers are right-aligned at 0x8e, measured in the black font.
    std::snprintf(text, sizeof(text), "%d", UnitCost < 0 ? -UnitCost : UnitCost);
    int32_t textWidth = MedBlackFont->Width(reinterpret_cast<uint8_t*>(text));
    MedWhiteFont->WriteString(port, 0x8e - textWidth, 0x47, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", ResourcePoints - Quantity * UnitCost);
    textWidth = MedBlackFont->Width(reinterpret_cast<uint8_t*>(text));
    MedWhiteFont->WriteString(port, 0x8e - textWidth, 0x6c, reinterpret_cast<uint8_t*>(text), -1);
    std::snprintf(text, sizeof(text), "%d", Quantity);
    textWidth = MedBlackFont->Width(reinterpret_cast<uint8_t*>(text));
    MedWhiteFont->WriteString(port, (0x11 - textWidth) / 2 + 0x7e, 0x55, reinterpret_cast<uint8_t*>(text), -1);

    // The item kind's icon (mech, part, component, vehicle; sell/buy). Another type loads the quantity text as a
    // file name, as the original did.
    static constexpr const char* icons[8] = {"lspcbm00.tga", "lspcbm01.tga", "lspcbp00.tga", "lspcbp01.tga",
                                             "lspcbc00.tga", "lspcbc01.tga", "lspcbv00.tga", "lspcbv01.tga"};
    MCLogPort* icon =
        PurchaseType >= 0 && PurchaseType < 8 ? LogArtf("%slogart\\%s", ArtPath, icons[PurchaseType]) : LogArt(text);

    if (icon != nullptr)
    {
        icon->CopyTo(port, 3, 3, -1);
    }

    DrawPressed();
}

auto MCPurchaseDlg::Activate() -> void
{
    NeedBackground = -1;
    FreePort(FadedBackground);
    Application->Grab(this);
    BringToFront(0);
    DrawBackground();
    ShowGuiWindow(-1);
}

auto MCPurchaseDlg::Deactivate(int dialogResult) -> void
{
    Application->RemoveTimer(this, 6);
    Application->Release();
    ShowGuiWindow(0);

    if (PurchaseCallback != nullptr)
    {
        PurchaseCallback(dialogResult, Quantity);
    }
}

auto MCPurchaseDlg::SetCallback(void (*newCallback)(int, int32_t)) -> void
{
    PurchaseCallback = newCallback;
}

// ReusableDialog

auto MCReusableDialog::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    (void)xPos;
    (void)yPos;
    (void)width;
    (void)height;
    (void)name;
    // The box is its top, some middle pieces and its bottom, centred across the screen at y 200.
    TopPiece = new MCLogPort;
    int32_t result = TopPiece->Init(const_cast<char*>("dbox_top.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog");
    MiddlePiece = new MCLogPort;
    result = MiddlePiece->Init(const_cast<char*>("dbox_middle.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog");
    BottomPiece = new MCLogPort;
    result = BottomPiece->Init(const_cast<char*>("dbox_bottom.tga"));
    Assert(result == 0, result, "Error initializing reusable dialog");
    const int32_t boxWidth = TopPiece->Width();
    result = MCLogObject::Init(Application->Width() / 2 - boxWidth / 2, 200, boxWidth,
                               BottomPiece->Height() + MiddlePiece->Height() + TopPiece->Height(), nullptr, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog");

    OkButton = new MCLogDialogButton;
    result = OkButton->Init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog");
    AddChild(OkButton);
    CancelButton = new MCLogDialogButton;
    result = CancelButton->Init(0, 0, 0x3f, 0xe, nullptr);
    Assert(result == 0, result, "Error initializing reusable dialog");
    AddChild(CancelButton);
    SetTwoButton(0);
    ShowGuiWindow(0);
    SetDepth(100);
    TimeoutResult = 0;
    KeepCallbacks = 0;
    return 0;
}

auto MCReusableDialog::Destroy() -> void
{
    FreePort(TopPiece);
    FreePort(MiddlePiece);
    FreePort(BottomPiece);

    if (OkButton != nullptr)
    {
        delete OkButton;
        OkButton = nullptr;
    }

    if (CancelButton != nullptr)
    {
        delete CancelButton;
        CancelButton = nullptr;
    }

    if (Text != nullptr)
    {
        LogFree(Text);
        Text = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCReusableDialog::Draw() -> void
{
    int32_t pieceY = 0;

    if (TopPiece != nullptr)
    {
        TopPiece->CopyTo(Lport()->Frame(), 0, 0, -1);
        pieceY = TopPiece->Height();
    }

    for (int32_t i = 0; i < NumMiddlePieces; i++)
    {
        if (MiddlePiece != nullptr)
        {
            MiddlePiece->CopyTo(Lport()->Frame(), 0, pieceY, -1);
            pieceY += MiddlePiece->Height();
        }
    }

    if (BottomPiece != nullptr)
    {
        BottomPiece->CopyTo(Lport()->Frame(), 0, pieceY, -1);
    }

    // The text, word-wrapped to the box.
    int32_t lineY = TopPiece->Height() + 2;

    if (Text != nullptr)
    {
        int32_t length = static_cast<int32_t>(std::strlen(Text));
        auto* line = reinterpret_cast<uint8_t*>(Text);
        int32_t fit = MedBlueFont->CharactersToWidth(line, Width() - 0x14, -1);

        if (fit == length)
        {
            MedBlueFont->WriteString(_OwnPort->Frame(), 0xc, lineY, line, -1);
        }
        else
        {
            while (fit > 0 && fit <= length)
            {
                const uint8_t saved = line[fit];
                uint8_t* next = line + fit;
                *next = 0;
                MedBlueFont->WriteString(_OwnPort->Frame(), 0xc, lineY, line, -1);

                *next = saved;
                if (saved != 0)
                {
                    next++;
                }

                length = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
                fit = MedBlueFont->CharactersToWidth(next, Width() - 0x14, -1);
                lineY += MedBlueFont->Height() + 3;
                line = next;
            }
        }
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        DrawChild(ChildList[i]);
    }
}

auto MCReusableDialog::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 9)
    {
        if (event->Key == 0x0d)
        {
            OkButton->Callback()->Execute();
            Deactivate(TwoButton != 0 ? -1 : 0);
        }
        else if (event->Key == 0x1b)
        {
            // Original behaviour (OB-074): Escape runs the OK button's callback too.
            OkButton->Callback()->Execute();
            Deactivate(0);
        }
    }
    else if (event->Type == 0x13)
    {
        // Timed out.
        Deactivate(TimeoutResult);
    }

    // While grabbed, clicks go to the child under the mouse.
    if (Application->GrabbedObject() == this)
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
    Application->Grab(this);
    // The original painted the dialog afresh: a press left on its buttons was gone.
    OkButton->PressedDown = 0;
    CancelButton->PressedDown = 0;
    MoveTo(0x140 - Width() / 2, 0xf0 - Height() / 2, 0);
    ShowGuiWindow(-1);

    if (Timeout > 0)
    {
        Application->AddTimer(this, 0, Timeout, 0, 0, 0);
    }
}

auto MCReusableDialog::Deactivate(int32_t dialogResult) -> void
{
    Application->Release();
    ShowGuiWindow(0);

    // Port: the original also skipped a callback pointer IsBadReadPtr rejected; a function pointer is always valid.
    if (Callback != nullptr)
    {
        Callback(dialogResult);
    }

    if (KeepCallbacks != 0)
    {
        KeepCallbacks = 0;
        return;
    }

    Callback = nullptr;
    OkButton->Callback()->SetExec(nullptr);
    CancelButton->Callback()->SetExec(nullptr);
    Application->RemoveTimer(this, 0);
    Timeout = 0;
    TimeoutResult = 0;
}

auto MCReusableDialog::SetText(char* newText) -> void
{
    if (Text != nullptr)
    {
        LogFree(Text);
    }

    size_t size = std::strlen(newText) + 1;
    Text = static_cast<char*>(LogAlloc(static_cast<uint32_t>(size)));
    std::strcpy(Text, newText);
    // Count the wrapped lines; each middle piece holds two.
    auto* line = reinterpret_cast<uint8_t*>(Text);
    int32_t fit = MedBlueFont->CharactersToWidth(line, Width() - 0x14, -1);
    int32_t lines = 1;

    while (fit >= 1 && fit < static_cast<int32_t>(size - 1))
    {
        line += fit + 1;
        lines++;
        size = std::strlen(reinterpret_cast<char*>(line)) + 1;
        fit = MedBlueFont->CharactersToWidth(line, Width() - 0x14, -1);
    }

    NumMiddlePieces = (lines + 1) / 2;
    Resize(Width(), MiddlePiece->Height() * ((lines + 1) / 2) + BottomPiece->Height() + TopPiece->Height());
    SetTwoButton(TwoButton);
}

auto MCReusableDialog::SetTwoButton(int twoButtons) -> void
{
    TwoButton = twoButtons;
    const int32_t buttonY = Height() - 0x17;

    if (TwoButton != 0)
    {
        CancelButton->ShowGuiWindow(-1);
        CancelButton->MoveTo(0x68, buttonY, 0);
        OkButton->MoveTo(0x23, buttonY, 0);
        return;
    }

    CancelButton->ShowGuiWindow(0);
    OkButton->MoveTo(0x68, buttonY, 0);
}

// RefitDialog

auto MCRefitDialog::SetText(char* newText) -> void
{
    if (Text != nullptr)
    {
        LogFree(Text);
    }

    Text = static_cast<char*>(LogAlloc(static_cast<uint32_t>(std::strlen(newText) + 1)));
    std::strcpy(Text, newText);
    // The text is a comma-separated list, one item per line, between six lines of framing text.
    int32_t lines = 7;
    NumItems = 0;

    for (char* comma = std::strchr(Text, ','); comma != nullptr; comma = std::strchr(Text, ','))
    {
        lines++;
        *comma = '.';
    }

    NumMiddlePieces = lines / 2;
    std::strcpy(Text, newText);
    NumItems = lines - 6;
    Resize(Width(), MiddlePiece->Height() * NumMiddlePieces + BottomPiece->Height() + TopPiece->Height());
    MCReusableDialog::SetTwoButton(TwoButton);
    Drawn = 0;
}

auto MCRefitDialog::Draw() -> void
{
    int32_t pieceY = 0;

    if (TopPiece != nullptr)
    {
        TopPiece->CopyTo(Lport()->Frame(), 0, 0, -1);
        pieceY = TopPiece->Height();
    }

    for (int32_t i = 0; i < NumMiddlePieces; i++)
    {
        if (MiddlePiece != nullptr)
        {
            MiddlePiece->CopyTo(Lport()->Frame(), 0, pieceY, -1);
            pieceY += MiddlePiece->Height();
        }
    }

    if (BottomPiece != nullptr)
    {
        BottomPiece->CopyTo(Lport()->Frame(), 0, pieceY, -1);
    }

    char message[264];
    CLoadString(ThisInstance, 0x54, message, 0xfe);
    int32_t lineY = WrapText(message, TopPiece->Height() + 2);
    lineY += MedBlueFont->Height() + 3;

    if (Text != nullptr)
    {
        // One item per line, cut at the commas (in a copy: the original cut the text itself, once).
        std::string items(Text);
        size_t item = 0;

        for (int32_t i = NumItems; i > 0; i--)
        {
            const size_t comma = items.find(',', item);

            if (comma != std::string::npos)
            {
                items[comma] = '\0';
            }

            MedBlueFont->WriteString(_OwnPort->Frame(), 0x14, lineY, reinterpret_cast<uint8_t*>(items.data() + item),
                                     -1);
            lineY += MedBlueFont->Height() + 3;

            if (comma != std::string::npos)
            {
                item = comma + 1;
            }
        }
    }

    const int32_t fontHeight = MedBlueFont->Height();
    CLoadString(ThisInstance, 0x62, message, 0xfe);
    WrapText(message, lineY + 3 + fontHeight);

    for (int32_t i = 0; i < NumChildren; i++)
    {
        DrawChild(ChildList[i]);
    }
}

auto MCRefitDialog::WrapText(char* string, int32_t yPos) -> int32_t
{
    auto* line = reinterpret_cast<uint8_t*>(string);
    const int32_t length = static_cast<int32_t>(std::strlen(string));
    int32_t fit = MedBlueFont->CharactersToWidth(line, Width() - 0x14, -1);

    if (fit == length)
    {
        MedBlueFont->WriteString(_OwnPort->Frame(), 0xc, yPos, line, -1);
        return yPos;
    }
    while (fit > 0)
    {
        uint8_t* end = line + fit;
        // Port fix: the last piece ends at the terminator, which the original overwrote with a space before reading
        // on past it (OB-075).
        const bool last = *end == 0;
        *end = 0;
        MedBlueFont->WriteString(_OwnPort->Frame(), 0xc, yPos, line, -1);
        yPos += MedBlueFont->Height() + 3;

        if (last)
        {
            break;
        }

        *end = ' ';
        line = end + 1;
        fit = MedBlueFont->CharactersToWidth(line, Width() - 0x14, -1);
    }

    return yPos;
}

auto MCRefitDialog::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    Drawn = 0;
    return MCReusableDialog::Init(xPos, yPos, width, height, name);
}
