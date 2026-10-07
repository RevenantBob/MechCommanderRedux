#include "stdafx.h"
#include "gui/atextbox.h"
#include "camera/camera.h"
#include "gui/abutton.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/sessionmanager.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"
#include "vfx/MCVfx.h"
#include "vfx/MCVfxFunctions.h"

/// <summary>The gap aScrollTextObject's section highlight leaves at the right (3).</summary>
int16_t ROffset = 3;
int32_t PlayerColor[6] = {1, 3, 4, 2, 6, 5};
/// <summary>Nonzero to swallow the next Enter in the chat line (set by the interface when Enter opens the chat).</summary>
int FirstReturn = 0;

namespace
{
    /// <summary>The size of aScrollTextObject's text buffer (one more byte is allocated for a terminator).</summary>
    constexpr int32_t TextBufferSize = 0x1000;

    /// <summary>Destroys and deletes a child made with <c>new</c>, and clears the pointer.</summary>
    template <typename T> void ReleaseChild(T*& child)
    {
        if (child != nullptr)
        {
            child->Destroy();
            delete child;
            child = nullptr;
        }
    }

    /// <summary>The font row (the <c>fonts</c> table's first index) aScrollTextObject draws colour <paramref name="color"/> in.</summary>
    int32_t FontRowForColor(uint8_t color)
    {
        switch (color)
        {
            case 0x10:
                return 0;
            case 0xef:
                return 1;
            case 0xf2:
                return 2;
            case 0x0b:
                return 3;
            case 0x0c:
                return 4;
            case 0x19:
                return 5;
            case 0x1f:
                return 6;
            default:
                return 7;
        }
    }
}

auto MCGuiTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    ReadOnly = 0;
    TextFont = GreyFont;
    TextTerminator = 0;

    if (newText == nullptr)
    {
        Text[0] = '\0';
        TextLength = 0;
    }
    else
    {
        std::strncpy(Text, newText, 0xfe);
        TextLength = static_cast<int16_t>(std::strlen(newText));

        if (TextLength > 0xfe)
        {
            TextLength = 0xfe;
        }
    }

    BackgroundColor = 0;
    VfxPaneWipe(DisplayPort->Frame(), 0);
    return 0;
}

auto MCGuiTextObject::HandleEvent(MCGuiEvent* event) -> void
{
    // The event routine gets a copy taken before any editing.
    MCGuiEvent eventCopy = *event;

    if (ReadOnly == 0)
    {
        if (event->Type == 1)
        {
            Application->SetText(this);

            if (Parent != nullptr)
            {
                BringToFront(0);
                ARedrawScreen();
            }
        }
        else if (event->Type == 10 && Application->TextObject() == this)
        {
            const uint8_t key = event->Key;

            if (key == 8)
            {
                if (TextLength > 0)
                {
                    // Original behaviour (OB-067): clears the terminator, not the last character, so the character
                    // stays on show until the next one typed replaces it.
                    Text[TextLength] = '\0';
                    TextLength--;
                }
            }
            else
            {
                if (key == 0xd && Parent != nullptr)
                {
                    APostMessage(Parent, 0x17);
                }

                if (TextLength < 0xfe && (std::isprint(key) != 0 || key == ' '))
                {
                    Text[TextLength] = static_cast<char>(key);
                    TextLength++;
                    Text[TextLength] = '\0';
                }
            }
        }
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, &eventCopy);
    }
}

auto MCGuiTextObject::Draw() -> void
{
    // textTerminator ends a full 254 characters.
    char shown[256];
    const size_t length = strnlen(Text, sizeof(Text));
    std::memcpy(shown, Text, length);
    shown[length] = '\0';
    VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    DrawFramed(0, 0);

    if (Application->TextObject() == this)
    {
        // The caret.
        shown[TextLength] = 0x7f;
        shown[TextLength + 1] = '\0';
    }

    // Drop leading characters until the rest fits.
    uint8_t* start = reinterpret_cast<uint8_t*>(shown);

    while (*start != 0 && TextFont->Width(start) > Width() - 6)
    {
        start++;
    }

    if (*start != 0)
    {
        TextFont->WriteString(DisplayPort->Frame(), 3, 3, start, -1);
    }
}

auto MCGuiTextObject::SetText(char* newText) -> void
{
    if (newText == nullptr)
    {
        Text[0] = '\0';
        TextLength = 0;
        return;
    }

    std::strncpy(Text, newText, 0xfe);
    // textTerminator ends a full 254 characters.
    TextLength = static_cast<int16_t>(strnlen(Text, sizeof(Text)));
}

auto PaintScrollTab(MCGuiObject* obj) -> void
{
    const int32_t width = obj->Width();
    const int32_t height = obj->Height();
    MCPane* pane = obj->Port()->Frame();
    VfxPaneWipe(pane, 0x1a);
    VfxLineDraw(pane, 0, 0, width - 2, 0, 0x1f);
    VfxLineDraw(pane, 0, 0, 0, height - 2, 0x1f);
    VfxLineDraw(pane, width - 1, 0, width - 1, height - 1, 0x16);
    VfxLineDraw(pane, 0, height - 1, width - 1, height - 1, 0x16);
}

auto ScrollTabEventHandler(MCGuiObject* obj, MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            Application->Grab(obj);
            obj->StartDrag(0, event->Y - obj->GlobalY());
            break;
        }
        case 4:
        {
            Application->Release();
            obj->StopDrag();
            break;
        }
        case 7:
        {
            // Any grab will do (the original doesn't check that it is this thumb).
            if (Application->GrabbedObject() == nullptr)
            {
                break;
            }

            auto* textObject = static_cast<MCGuiScrollTextObject*>(obj->Parent);
            const int32_t newY = (event->Y - obj->Parent->Y()) - obj->DragStartY();
            obj->MoveTo(obj->X(), newY, 0);

            if (obj->Y() < 0x10)
            {
                obj->MoveTo(obj->X(), 0x10, 0);
            }

            if (obj->Y() > (-0x10 - textObject->ScrollTab->Height()) + textObject->Height())
            {
                obj->MoveTo(obj->X(), (-0x10 - textObject->ScrollTab->Height()) + textObject->Height(), 0);
            }

            textObject->CalcFirstPixel(obj->Y() - 0x10);
            break;
        }

        default:
            break;
    }
}

auto MCGuiScrollTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* text) -> int32_t
{
    // aObject::init, inlined with an aScrollPort for the port.
    WinHeight = height;
    MaxHeight = height;
    NormalHeight = height;
    IconHeight = height;
    InitFailed = 0;
    WinWidth = width;
    WinX = xPos;
    WinY = yPos;
    MaxWidth = width;
    MaxX = xPos;
    MaxY = yPos;
    NormalWidth = width;
    NormalX = xPos;
    NormalY = yPos;
    IconWidth = width;
    IconX = xPos;
    IconY = yPos;
    HideOffset = 0;
    HomeX = xPos;
    HomeY = yPos;
    WinState = 0;
    ShowWindow = 1;
    DragOn = 0;
    Transparent = 0;
    BackgroundColor = 0xff;

    if (DisplayPort != nullptr)
    {
        DisplayPort->Destroy();
        delete DisplayPort;
        DisplayPort = nullptr;
    }

    DisplayPort = new MCGuiScrollPort;

    if (DisplayPort == nullptr)
    {
        InitFailed = 1;
        return 3;
    }

    int32_t result = DrawsLive() ? DisplayPort->InitView(width, height) : DisplayPort->Init(width, height);

    if (result != 0)
    {
        InitFailed = 1;
        return result;
    }

    if (FramePane != nullptr)
    {
        delete FramePane;
        FramePane = nullptr;
    }

    FramePane = new (std::nothrow) MCPane;

    if (FramePane == nullptr)
    {
        InitFailed = 1;
        return 3;
    }

    FramePane->Window = ScreenPort->Bitmap();
    FramePane->X0 = xPos;
    FramePane->Y0 = yPos;
    FramePane->X1 = xPos + width;
    FramePane->Y1 = yPos + height;
    Hidden = 0;
    HideDirection = 3;
    PaintRoutine = nullptr;
    EventRoutine = nullptr;
    NumChildren = 0;
    Parent = nullptr;
    WinDepth = 0;
    WindowAnimation = nullptr;
    Animating = 0;
    IconAnimation = nullptr;
    ObjectType = -1;

    ScrollTab = new MCGuiObject;

    if (ScrollTab == nullptr)
    {
        Fatal(0, "Not enough memory for scrollbar tab.");
    }

    ScrollTab->SetDrawsLive();
    result = ScrollTab->Init(0, 0, 9, height - 0x20, nullptr);

    if (result != 0)
    {
        InitFailed = 1;
        return result;
    }

    ScrollTab->MoveTo(this->Width() + 2, 0x10, 0);
    ScrollTab->SetDepth(100);
    AddChild(ScrollTab);
    ScrollTab->ShowGuiWindow(1);
    ScrollTab->SetEventRoutine(ScrollTabEventHandler);
    ScrollTab->SetPaintRoutine(PaintScrollTab);
    ScrollTab->SetDepth(1);

    TextBuffer = std::make_unique<char[]>(TextBufferSize + 1);
    NumLines = 0;
    TextLength = 0;
    FirstPixel = 0;
    FontIndex = 0;

    for (int32_t i = 0; i < 4; i++)
    {
        SectionStarts[i] = -1;
        SectionColors[i] = 0xff;
    }

    if (text != nullptr)
    {
        Print(text, 0x1f);
    }

    return 0;
}

auto MCGuiScrollTextObject::Destroy() -> void
{
    ReleaseChild(ScrollTab);

    TextBuffer.reset();
    MCGuiObject::Destroy();
}

auto MCGuiScrollTextObject::Draw() -> void
{
    int32_t lineY = 2;
    char* line = TextBuffer.get();
    const int32_t lineHeight = Fonts[0][FontIndex]->Height() + 2;
    VfxPaneWipe(Port()->Frame(), 0x10);

    // Each section highlights one line.
    for (int32_t i = 0; i < 4; i++)
    {
        const int32_t start = SectionStarts[i];

        if (start != -1 && start < start + 1)
        {
            FillBox(1, static_cast<int16_t>(lineHeight * start + 1), static_cast<int16_t>(Width() - ROffset),
                    static_cast<int16_t>((start + 1) * lineHeight - 1), SectionColors[i]);
        }
    }

    // Lines are (colour byte, text, '\n'). A tab splits a line into two pieces, the second drawn at tabStop.
    while (line != nullptr && *line != '\0')
    {
        const uint8_t color = static_cast<uint8_t>(*line);
        line++;
        int32_t pieces = 0;
        int32_t lineX = 2;

        if (char* tab = std::strchr(line, '\t'); tab != nullptr)
        {
            if (TabStop < 0)
            {
                *tab = ' ';
            }
            else
            {
                *tab = '\n';
                pieces = 1;
            }
        }

        const int32_t fontRow = FontRowForColor(color);

        do
        {
            Fonts[fontRow][FontIndex]->WriteStringToNewline(Port()->Frame(), lineX, lineY,
                                                            reinterpret_cast<uint8_t*>(line));
            line = std::strchr(line, '\n');

            if (pieces > 0)
            {
                // Put the tab back.
                if (line != nullptr)
                {
                    *line = '\t';
                }

                lineX = TabStop;
            }

            pieces--;

            if (line != nullptr)
            {
                line++;
            }
        } while (pieces >= 0 && line != nullptr);

        lineY += lineHeight;
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        if (DrawsChild(ChildList[i]))
        {
            ChildList[i]->Draw();
        }
    }
}

auto MCGuiScrollTextObject::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // Port: the port is the whole text (taller than the object), drawn each frame scrolled by firstPixel.
    if (DrawsLive())
    {
        DrawInFramePass(Port(), FirstPixel);
        return;
    }

    if (Port() != nullptr)
    {
        VfxPaneCopy(Port()->Frame(), 0, 0, FramePane, 0, -FirstPixel, -1);
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        ChildList[i]->Display();
    }
}

auto MCGuiScrollTextObject::Resize(int32_t newWidth, int32_t newHeight) -> void
{
    const int32_t fontHeight = Fonts[0][FontIndex]->Height();

    if (newWidth <= 0 || newHeight <= 0 || (newWidth == WinWidth && newHeight == WinHeight))
    {
        return;
    }

    WinWidth = newWidth;
    WinHeight = newHeight;
    FramePane->X1 = FramePane->X0 - 1 + newWidth;
    FramePane->Y1 = FramePane->Y0 - 1 + newHeight;
    int32_t portHeight = NumLines * (fontHeight + 2);

    if (portHeight <= newHeight)
    {
        portHeight = newHeight;
    }

    Port()->Resize(newWidth, portHeight);
    ScrollTab->MoveTo(newWidth + 2, ScrollTab->Y(), 0);
    PositionScrollTab();
}

auto MCGuiScrollTextObject::ResetPortSize() -> void
{
    // The port only grows.
    int32_t portHeight = (Fonts[0][FontIndex]->Height() + 2) * NumLines;

    if (portHeight <= Port()->Height())
    {
        portHeight = Port()->Height();
    }

    Port()->Resize(Port()->Width(), portHeight);
}

auto MCGuiScrollTextObject::Print(char* line, uint8_t color) -> void
{
    const int32_t used = TextLength;
    char* buffer = TextBuffer.get();
    const int32_t fontHeight = Fonts[0][FontIndex]->Height();

    if (TextBufferSize - used <= 2)
    {
        return;
    }

    buffer[used] = static_cast<char>(color);
    char* dest = buffer + used + 1;
    const int32_t textStart = used + 1;
    TextLength = textStart;

    if (line == nullptr)
    {
        if (TextBufferSize - textStart > 2)
        {
            // A blank line.
            NumLines++;
            TextLength = used + 2;
            TextBuffer[used + 1] = '\n';
            return;
        }

        // Port fix: the original goes on to strlen(null) when the buffer is nearly full.
        return;
    }

    if (static_cast<int32_t>(std::strlen(line)) + textStart <= TextBufferSize)
    {
        std::sprintf(dest, "%s\n", line);
        TextLength = static_cast<int32_t>(std::strlen(line)) + 1 + textStart;
    }
    else
    {
        // No room: the line is cut and the buffer is full (the length isn't advanced).
        std::strncpy(dest, line, static_cast<size_t>(TextBufferSize - textStart));
        TextBuffer[TextBufferSize] = '\0';
    }

    NumLines++;
    const int32_t needed = NumLines * (fontHeight + 2);

    if (needed > Port()->Height())
    {
        if (Port()->Resize(Port()->Width(), needed) != 0)
        {
            InitFailed = 1;
        }
    }

    PositionScrollTab();
}

auto MCGuiScrollTextObject::PrintWrapped(char* line, uint8_t color, int32_t wrapWidth) -> void
{
    if (wrapWidth == -1)
    {
        wrapWidth = Width();
    }
    while (line != nullptr)
    {
        MCGuiFont* font = Fonts[0][FontIndex];
        char* split = nullptr;

        if (font->Width(reinterpret_cast<uint8_t*>(line)) > wrapWidth - 6)
        {
            split = std::strrchr(line, ' ');

            if (split != nullptr)
            {
                // Cut at the last space, then earlier spaces until the piece fits.
                *split = '\0';
                char* cut = split;

                while (font->Width(reinterpret_cast<uint8_t*>(line)) > wrapWidth - 6)
                {
                    split = std::strrchr(line, ' ');

                    *cut = ' ';
                    if (split == nullptr)
                    {
                        break;
                    }

                    *split = '\0';
                    cut = split;
                }
            }
        }

        Print(line, color);

        if (split == nullptr)
        {
            return;
        }

        line = split + 1;
        *split = ' ';
    }
}

auto MCGuiScrollTextObject::Clear() -> void
{
    FirstPixel = 0;
    TextLength = 0;
    NumLines = 0;
    std::memset(TextBuffer.get(), 0, TextBufferSize);

    for (int32_t& start : SectionStarts)
    {
        start = -1;
    }
}

auto MCGuiScrollTextObject::CalcFirstPixel(int32_t thumbY) -> void
{
    const int32_t track = (-0x20 - ScrollTab->Height()) + Height();
    const int32_t range = Port()->Height() - Height();

    if (track > 0 && range > 0)
    {
        FirstPixel = (range * thumbY) / track;
        return;
    }

    FirstPixel = 0;
}

auto MCGuiScrollTextObject::PositionScrollTab() -> void
{
    if (Application->GrabbedObject() == ScrollTab)
    {
        return;
    }

    const int32_t track = Height() - 0x20;
    const int32_t range = Port()->Height() - Height();

    if (range == 0)
    {
        ScrollTab->ShowGuiWindow(0);
        return;
    }

    // The thumb's length is the visible fraction of the track (x87: float quotient, then times the track).
    const float shown = static_cast<float>(Height());
    int32_t tabLength = static_cast<int32_t>(static_cast<double>(shown) / Port()->Height() * track);

    if (tabLength < 3)
    {
        tabLength = 3;
    }

    ScrollTab->ShowGuiWindow(1);
    ScrollTab->Resize(ScrollTab->Width(), tabLength);
    ScrollTab->MoveTo(ScrollTab->X(), ((track - tabLength) * FirstPixel) / range + 0x10, 0);
}

auto MCGuiScrollTextObject::ReceiveClick(int32_t direction, int32_t yPos) -> void
{
    if (Height() == Port()->Height())
    {
        return;
    }

    const int32_t lineHeight = Fonts[0][FontIndex]->Height() + 2;
    auto clampToEnd = [this]()
    {
        if (FirstPixel > Port()->Height() - Height())
        {
            FirstPixel = Port()->Height() - Height();
        }
    };

    if (direction == -1)
    {
        FirstPixel -= lineHeight;

        if (FirstPixel < 0)
        {
            FirstPixel = 0;
        }
    }
    else if (direction == 0)
    {
        if (yPos < ScrollTab->Y())
        {
            FirstPixel -= Height();

            if (FirstPixel < 0)
            {
                FirstPixel = 0;
            }
        }
        else if (yPos > ScrollTab->Bottom())
        {
            FirstPixel += Height();
            clampToEnd();
        }
    }
    else if (direction == 1)
    {
        FirstPixel += lineHeight;
        clampToEnd();
    }

    PositionScrollTab();
}

auto MCGuiScrollTextObject::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (Height() == Port()->Height())
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        ReceiveClick(steps < 0 ? -1 : 1, 0);
    }

    return true;
}

auto MCGuiTransparentTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText)
    -> int32_t
{
    const int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr);

    if (result != 0)
    {
        return result;
    }

    VfxPaneWipe(DisplayPort->Frame(), 0xff);
    std::memset(Text, 0, 0xfe);
    TextColor = 0xfd;
    SetText(newText);
    return 0;
}

auto MCGuiTransparentTextObject::Draw() -> void
{
    int32_t lineY = 0;
    char* line = Text;
    const int32_t lineHeight = LgGreyFont->Height();
    VfxPaneWipe(DisplayPort->Frame(), 0xff);

    while (line != nullptr)
    {
        LgGreyFont->WriteStringToNewline(Port()->Frame(), 0, lineY, reinterpret_cast<uint8_t*>(line));
        line = std::strchr(line, '\n');

        if (line != nullptr)
        {
            line++;
        }

        lineY += lineHeight + 1;
    }

    // Everything drawn (not the 0xff background) becomes the text colour.
    uint8_t* pixel = Port()->Buffer();

    for (int32_t count = WinHeight * WinWidth; count > 0; count--, pixel++)
    {
        if (*pixel != 0xff)
        {
            *pixel = TextColor;
        }
    }
}

auto MCGuiTransparentTextObject::Display() -> void
{
    if (IsShowing() == 0 || GlobalPane == nullptr)
    {
        return;
    }

    CopySprite(GlobalPane, DisplayPort->Bitmap(), WinX, WinY, WinWidth, WinHeight, 0, 1);

    for (int32_t i = 0; i < NumChildren; i++)
    {
        ChildList[i]->Display();
    }
}

auto MCGuiTransparentTextObject::SetText(char* newText) -> void
{
    int32_t textHeight = 0;
    int32_t textWidth = 0;
    const int32_t lineHeight = LgGreyFont->Height();

    if (newText == nullptr)
    {
        Text[0] = '\0';
        TextLength = 0;
    }
    else
    {
        std::strncpy(Text, newText, 0xfe);
        TextLength = static_cast<int16_t>(std::strlen(Text));
        // Measure the widest line.
        char* line = Text;

        while (line != nullptr)
        {
            char* newline = std::strchr(line, '\n');

            if (newline != nullptr)
            {
                *newline = '\0';
            }

            const int32_t lineWidth = LgGreyFont->Width(reinterpret_cast<uint8_t*>(line));

            if (textWidth < lineWidth)
            {
                textWidth = lineWidth;
            }

            textHeight += lineHeight + 1;

            if (newline == nullptr)
            {
                break;
            }

            *newline = '\n';
            line = newline + 1;
        }
    }

    Resize(textWidth, textHeight);
    Draw();
}

auto ScenarioChatCallback(MCFidpMessage* message, void*) -> void
{
    if (MCTerrain::TerrainTacticalMap != nullptr)
    {
        MCTerrain::TerrainTacticalMap->HandleChatMessage(message->FromID, message->MessageBuffer);
    }
}

auto MCGuiChatInput::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, nullptr);
    Assert(result == 0, static_cast<uint32_t>(result), " Couldn't init chatsend window");

    TeamButton = new MCGuiToolButton;
    result = TeamButton->Init(1, 1, 0xc, 0x1a, nullptr);
    TeamButton->Framed = 0;
    // The picture loads return nothing, so every check repeats the init's.
    const int initOk = result == 0;
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't init team button for chatsend window");
    TeamButton->SetDownPicture(const_cast<char*>("mfdsbg00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load downstate team button for chatsend window");
    TeamButton->SetUpPicture(const_cast<char*>("mfdsbh00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    TeamButton->SetGrayPicture(const_cast<char*>("mfdsbn00.tga"));
    Assert(initOk, static_cast<uint32_t>(result), " Couldn't load graystate team button for chatsend window");
    TeamButton->Pushed = 0;
    AddChild(TeamButton);

    CursorX = 0x15;

    if (newText != nullptr)
    {
        std::strncpy(Text, newText, 0xff);
    }

    InputFont = WhiteFont;
    BackgroundColor = 0x10;
    // Port: the original drew the line here, which also restarted the caret's blink (see draw).
    CursorVisible = 1;
    return 0;
}

auto MCGuiChatInput::Destroy() -> void
{
    ReleaseChild(TeamButton);
    MCGuiObject::Destroy();
}

auto MCGuiChatInput::Draw() -> void
{
    DrawAndCheck(0);

    // Port: the caret, which the original drew into the picture in display (a vertical line a text line high; in
    // the background colour while cursorVisible, so it blinks).
    const int32_t bottom = InputFont->Height() + 3 + CursorY;
    const int32_t color = CursorVisible != 0 ? 0x10 : 0x1f;
    VfxLineDraw(DisplayPort->Frame(), CursorX, CursorY, CursorX, bottom, color);
    MCGuiObject::Draw();
}

auto MCGuiChatInput::DrawAndCheck(int32_t maxLines) -> int
{
    // Port: drawing the whole line (maxLines 0) also set cursorVisible, restarting the caret's blink. The line is
    // drawn every frame now, so the places that redrew it after an edit set it themselves.
    if (maxLines == 0)
    {
        VfxPaneWipe(DisplayPort->Frame(), BackgroundColor);
    }

    uint8_t* line = reinterpret_cast<uint8_t*>(Text);
    int32_t lineY = 1;
    int32_t extraLines = 0;

    if (Text[0] != '\0')
    {
        // The first line leaves room for the team button.
        int32_t lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
        int32_t fits = InputFont->CharactersToWidth(line, Width() - 0x18, 0);

        while (fits > 0 && fits < lineLength && (maxLines == 0 || extraLines < maxLines))
        {
            uint8_t* next = line + fits;

            if (maxLines == 0)
            {
                const uint8_t saved = *next;
                *next = 0;
                InputFont->WriteString(DisplayPort->Frame(), 0x14, lineY, line, -1);
                *next = saved;
            }

            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(next)));
            fits = InputFont->CharactersToWidth(next, Width() - 0x14, 1);
            lineY += 3 + InputFont->Height();
            line = next;
            extraLines++;
        }

        if (maxLines == 0)
        {
            InputFont->WriteString(DisplayPort->Frame(), 0x14, lineY, line, -1);
        }
    }

    return extraLines < maxLines;
}

auto MCGuiChatInput::Display() -> void
{
    // The caret is drawn by draw, each frame (the original drew it into the picture here).
    MCGuiObject::Display();
}

auto MCGuiChatInput::HandleEvent(MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            if (Scenario != nullptr && EventsToMissionResultsScreen == 0 && GameAsked == 0)
            {
                Application->SetText(this);
            }
            break;
        }
        case 8:
        case 9:
        {
            // Alt combinations go to the game interface.
            if (event->AltKey != 0 || event->ScanCode == 0x38)
            {
                TheInterface->HandleEvent(event);
            }
            break;
        }
        case 10:
        {
            if (Application->TextObject() != this || EventsToMissionResultsScreen != 0)
            {
                break;
            }

            const uint8_t key = event->Key;

            if (key == 8)
            {
                if (CursorPos != 0)
                {
                    Text[CursorPos - 1] = '\0';
                    CursorPos--;
                    SetCursorPos(CursorPos);
                    // The original redrew the line here, which restarted the caret's blink.
                    CursorVisible = 1;
                }
            }
            else if (key == 0xd)
            {
                if (FirstReturn != 0)
                {
                    FirstReturn = 0;
                    break;
                }

                if (MPlayer != nullptr && CursorPos > 0)
                {
                    auto* chatWindow = static_cast<MCGuiChatWindow*>(Parent);
                    int32_t color;

                    if (TeamButton->Pushed == 0)
                    {
                        MPlayer->SendChat(0, Text);
                        color = 6;
                    }
                    else
                    {
                        MPlayer->SendChat(MPlayer->HomeTeamGroupID, Text);
                        color = 4;
                    }

                    chatWindow->ProcessChatString(MPlayer->SessionManager->MyPlayer->Id, Text, color);
                }

                std::memset(Text, 0, 0xff);
                CursorPos = 0;
                SetCursorPos(0);
                Application->ReleaseText();
                CursorVisible = 1;
            }
            else if (CursorPos < 0xff && ((key > 0x1f && key < 0x7f) || (key > 0xbe && key < 0xfe)) && key != '%')
            {
                // '%' is the chat formatter's code character.
                Text[CursorPos] = static_cast<char>(key);
                CursorPos++;
                const int32_t newPos = CursorPos;

                if (DrawAndCheck(2) != 0)
                {
                    SetCursorPos(newPos);
                    CursorVisible = 1;
                }
                else
                {
                    // A third line: take it back.
                    CursorPos = newPos - 1;
                    Text[newPos - 1] = '\0';
                }
            }
            break;
        }

        case 0x13:
        {
            // The caret blink timer.
            if (event->Data == 0)
            {
                CursorVisible = CursorVisible == 0;
            }
            break;
        }
        case 0x1e:
        {
            if (event->Data == 7)
            {
                Application->AddTimer(this, 0, static_cast<int32_t>(MCPort::CaretBlinkTime()), 0, 0, 0);
            }
            else if (event->Data == 8)
            {
                Application->RemoveTimer(this, 0);
                CursorVisible = 1;
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCGuiChatInput::SetCursorPos(int32_t pos) -> void
{
    // Measure the text up to pos.
    char saved = '\0';

    if (pos < CursorPos)
    {
        saved = Text[pos];
        Text[pos] = '\0';
    }

    uint8_t* line = reinterpret_cast<uint8_t*>(Text);
    CursorX = 0x14;
    CursorY = 0;

    if (Text[0] != '\0')
    {
        int32_t lineLength = static_cast<int32_t>(std::strlen(Text));
        int32_t fits = InputFont->CharactersToWidth(line, Width() - 0x18, 0);

        while (fits > 0 && fits < lineLength)
        {
            line += fits;
            lineLength = static_cast<int32_t>(std::strlen(reinterpret_cast<char*>(line)));
            fits = InputFont->CharactersToWidth(line, Width() - 0x14, 0);
            CursorY += InputFont->Height() + 3;
        }

        CursorX = InputFont->Width(line) + 0x15;
    }

    if (saved != '\0')
    {
        Text[pos] = saved;
    }
}

auto MCGuiChatWindow::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t result = MCGuiObject::Init(xPos, yPos, width, height, name);

    if (result != 0)
    {
        return result;
    }

    VfxPaneWipe(Port()->Frame(), 0x10);
    ChatInput = new MCGuiChatInput;
    result = ChatInput->Init(0, height + 4, this->Width(), 0x1c, nullptr);

    if (result == 0)
    {
        AddChild(ChatInput);
        ChatInput->ShowGuiWindow(1);
        // The original drew the input line here, restarting its caret's blink.
        ChatInput->CursorVisible = 1;
    }

    return result;
}

auto MCGuiChatWindow::Destroy() -> void
{
    ReleaseChild(ChatInput);
    MCGuiObject::Destroy();
}

auto MCGuiChatWindow::HandleNetworkMessage(uint32_t fromID, void* data) -> void
{
    auto* bytes = static_cast<char*>(data);
    ProcessChatString(fromID, bytes + 9, bytes[8] != '\0' ? 6 : 4);
}

auto MCGuiChatWindow::ProcessChatString(uint32_t playerId, char* text, int32_t color) -> void
{
    char* name = const_cast<char*>("?");

    if (playerId != 0)
    {
        name = MPlayer->SessionManager->GetPlayer(playerId)->Name;
    }

    if (color == -1)
    {
        color = 6;
    }

    int32_t playerNumber = 0;

    if (playerId != 0)
    {
        playerNumber = MPlayer->SessionManager->GetPlayer(playerId)->PlayerNumber;
    }

    char line[0x800];
    std::sprintf(line, "%%fc%d%s: %%fc%d%s", PlayerColor[playerNumber], name, color, text);

    // The original scrolled its picture up by the new text's height, wiped the bottom and wrote the text there; the
    // line is kept and draw shows the lines that way. Lines scrolled wholly off the top are dropped.
    MCSmuti& formatter = Application->TextFormatter;
    const int32_t textHeight = formatter.Process(reinterpret_cast<uint8_t*>(line), nullptr, Port()->Width(), 0);
    ChatLines.push_back(ChatLine{line, textHeight});
    int32_t below = 0;

    for (size_t i = ChatLines.size(); i > 0; i--)
    {
        below += ChatLines[i - 1].Height;

        if (below > Port()->Height())
        {
            ChatLines.erase(ChatLines.begin(), ChatLines.begin() + static_cast<std::ptrdiff_t>(i - 1));
            break;
        }
    }
}

auto MCGuiChatWindow::Draw() -> void
{
    // The picture was wiped to 0x10 at init, and every scroll wiped the rows it uncovered.
    VfxPaneWipe(Port()->Frame(), 0x10);
    MCSmuti& formatter = Application->TextFormatter;
    // Each line lies above the ones after it; the newest ends a row above the bottom.
    int32_t lineY = Port()->Height() - 1;

    for (const ChatLine& chatLine : ChatLines)
    {
        lineY -= chatLine.Height;
    }

    for (const ChatLine& chatLine : ChatLines)
    {
        char line[0x800];
        std::snprintf(line, sizeof(line), "%s", chatLine.Text.c_str());
        formatter.Process(reinterpret_cast<uint8_t*>(line), Port(), 0, lineY);
        lineY += chatLine.Height;
    }

    MCGuiObject::Draw();
}
