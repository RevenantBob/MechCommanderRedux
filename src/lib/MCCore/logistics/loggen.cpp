#include "stdafx.h"
#include "logistics/loggen.h"
#include "color/MCPalette.h"
#include "gui/afont.h"
#include "gui/updisp.h"
#include "lib/MCFatal.h"
#include "lib/MCIDString.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "lib/MCPacketFile.h"
#include "linkup/linkedlist.hpp"
#include "linkup/session.h"
#include "linkup/sessionmanager.h"
#include "logistics/invblock.h"
#include "logistics/logbri.h"
#include "logistics/logmain.h"
#include "logistics/logsession.h"
#include "logistics/purchase.h"
#include "main/logistics.h"
#include "mission/mission.h"
#include "network/multplyr.h"
#include "platform/MCFileSystem.h"
#include "platform/MCRenderer.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

std::type_identity_t<char[256]> MCSplashScreen::_GenericPortFileName{};
std::type_identity_t<MCLogPort*> MCSplashScreen::_GenericPort{};
std::type_identity_t<int32_t> MCSplashScreen::_InstanceCount{};
_GUID DeletedSessions[50] = {};
int32_t NextDeletedSession = 0;
char* EmptyFile = nullptr;

namespace
{
    /// <summary>The width of a scroll pane's slider column.</summary>
    constexpr int32_t SliderWidth = 0xd;

    /// <summary>The size of an <see cref="MCLogScrollTextObject"/>'s text buffer (one more byte is allocated).</summary>
    constexpr int32_t TextBufferSize = 0x1000;

    void* LogAlloc(uint32_t size)
    {
        return GlobalLogPtr->LogisticsBlocks->Allocate(size);
    }

    void LogFree(void* block)
    {
        GlobalLogPtr->LogisticsBlocks->Free(block);
    }

    /// <summary>Frees a logistics port (destroy, then delete) and clears the pointer.</summary>
    void FreePort(MCLogPort*& port)
    {
        if (port != nullptr)
        {
            port->Destroy();
            delete port;
            port = nullptr;
        }
    }

    /// <summary>Loads <paramref name="fileName"/> into a new port at <paramref name="port"/>; on failure frees it.</summary>
    int32_t LoadPicture(MCLogPort*& port, char* fileName)
    {
        FreePort(port);
        port = new MCLogPort;
        const int32_t result = port->Init(fileName);

        if (result != 0)
        {
            FreePort(port);
        }

        return result;
    }

    /// <summary>
    /// The row of <c>fonts</c> a scroll text line is drawn in, from its colour byte (0x0b..0xf2 are the palette
    /// indices of the font colours; anything else is row 7).
    /// </summary>
    int32_t FontRowForColor(uint8_t color)
    {
        switch (color)
        {
            case 0x0b:
                return 3;
            case 0x0c:
                return 4;
            case 0x10:
                return 0;
            case 0x19:
                return 5;
            case 0x1f:
                return 6;
            case 0xef:
                return 1;
            case 0xf2:
                return 2;
            default:
                return 7;
        }
    }

    /// <summary>A new text entry field as the generic screens make them (black text on colour 0x1f).</summary>
    MCLogTextObject* MakeTextEntry(int32_t xPos, int32_t yPos, int32_t width, int32_t height)
    {
        auto* entry = new MCLogTextObject;
        entry->MCLogObject::Init(xPos, yPos, width, height, nullptr, nullptr);
        entry->CursorPos = 0;
        entry->CursorPixel = 0;
        entry->BackgroundColor = 0x1f;
        entry->Font = LgBlackFont;
        return entry;
    }

    /// <summary>
    /// Reads a button's four pictures (NormalArt was read already into <paramref name="art"/>), its sounds and its
    /// callback number. "NONE" skips a picture.
    /// </summary>
    /// <returns>Whether a Callback entry was found (in <paramref name="callback"/>).</returns>
    bool ReadButton(MCFitIniFile* file, MCLogButton* button, char* art, int32_t& callback)
    {
        int32_t result = 0;

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->SetUpPicture(art);
            Assert(result == 0, result, " Couldn't locate button upPicture image ");
        }

        result = file->ReadIdString("GreyArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find gray button art in Generic Screen ");

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->SetGrayPicture(art);
            Assert(result == 0, result, " Couldn't locate button grayPicture image ");
        }

        result = file->ReadIdString("PressArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find down button art in Generic Screen ");

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->SetDownPicture(art);
            Assert(result == 0, result, " Couldn't locate button downPicture image ");
        }

        result = file->ReadIdString("OverArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find button rollover art in Generic Screen ");

        if (MCPort::StrICmp(art, "NONE") != 0)
        {
            result = button->SetOverPicture(art);
            // The original reports the down picture's message here too.
            Assert(result == 0, result, " Couldn't locate button downPicture image ");
        }

        int32_t sound = 0;
        result = file->ReadIdLong("OverSFX", sound);
        Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ");
        button->OverSound = static_cast<uint32_t>(sound);
        result = file->ReadIdLong("PressSFX", sound);
        Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ");
        button->PressSound = static_cast<uint32_t>(sound);
        return file->ReadIdLong("Callback", callback) == 0;
    }

    /// <summary>
    /// Sets a generic screen button's callback (numbers 0..11, shared by both screen kinds). The load/save, delete
    /// and cancel buttons are remembered by the screen; the first two start disabled.
    /// </summary>
    /// <returns>Whether the number was one of these.</returns>
    bool SetScreenCallback(MCGenericScreen* screen, MCLogButton* button, int32_t callback)
    {
        void (*exec)() = nullptr;

        switch (callback)
        {
            case 0:
                exec = NewMcxCampaign;
                break;
            case 1:
                exec = SaveScreen;
                break;
            case 2:
                exec = LoadScreen;
                break;
            case 3:
                exec = ShowPreferences;
                break;
            case 4:
                exec = ConnectScreen;
                break;
            case 5:
                exec = ReplayCinema;
                break;
            case 6:
                exec = ReturnToGame;
                break;
            case 7:
                exec = GameOverMan;
                break;
            case 8:
            case 9:
            {
                button->Callback()->SetExec(callback == 8 ? LoadGame : SaveGame);
                screen->LoadSaveButton = button;
                button->Disabled = -1;
                return true;
            }
            case 10:
            {
                button->Callback()->SetExec(DeleteGame);
                screen->DeleteButton = button;
                button->Disabled = -1;
                return true;
            }
            case 11:
            {
                button->Callback()->SetExec(Cancel);
                screen->CancelButton = button;
                return true;
            }
            default:
                return false;
        }

        button->Callback()->SetExec(exec);
        return true;
    }

    /// <summary>Makes a file pane element (type 5) and reads its SavePane flag.</summary>
    MCFileScrollPane* MakeFilePane(MCFitIniFile* file, int32_t xPos, int32_t yPos, int32_t width, int32_t height)
    {
        auto* pane = new MCFileScrollPane;
        // The inlined constructors each clear the scroll pane.
        pane->MCScrollPane::Init();
        pane->Init(xPos, yPos, width, height);

        if (file->ReadIdBoolean("SavePane", pane->SavePane) != 0)
        {
            pane->SavePane = 0;
        }

        pane->SetStartDirectory(SavePath);
        return pane;
    }

    /// <summary>Reads the Element block header: type, rectangle and NormalArt.</summary>
    void ReadElement(MCFitIniFile* file, int32_t index, int32_t& type, int32_t& left, int32_t& top, int32_t& width,
                     int32_t& height, char* art)
    {
        char blockName[20];
        std::snprintf(blockName, sizeof(blockName), "Element%d", index);
        int32_t result = file->SeekBlock(blockName);
        Assert(result == 0, result, " Could not Find Element block in Generic Screen ");
        type = -1;
        result = file->ReadIdLong("ElementType", type);
        Assert(result == 0, result, " Could not Find Element Type in Generic Screen ");
        result = file->ReadIdLong("Left", left);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ");
        result = file->ReadIdLong("Top", top);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ");
        result = file->ReadIdLong("Width", width);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ");
        result = file->ReadIdLong("Height", height);
        Assert(result == 0, result, " Could not Find Element Coord in Generic Screen ");
        result = file->ReadIdString("NormalArt", art, 0xf9);
        Assert(result == 0, result, " Could not Find Element Art in Generic Screen ");
    }

    /// <summary>Reads the Elements block and allocates the element array.</summary>
    void ReadElementCount(MCFitIniFile* file, MCGenericScreen* screen)
    {
        int32_t result = file->SeekBlock("Elements");
        Assert(result == 0, result, " Could not Find Elements block in Generic Screen ");
        result = file->ReadIdLong("NumElements", screen->NumElements);
        Assert(result == 0, result, " Could not Find Elements number in Generic Screen ");
        // Port fix: sized by the port's pointer size (the original's count * 4 overran the array on 64-bit).
        screen->Elements = static_cast<MCGuiObject**>(
            LogAlloc(static_cast<uint32_t>(sizeof(MCGuiObject*) * static_cast<size_t>(screen->NumElements))));
        Assert(screen->Elements != nullptr, 0, " No RAM for Generic Screen Elements ");

        // Port fix: element types 2 and 3 leave their slot unset, which GenericScreen::destroy then deleted.
        if (screen->Elements != nullptr)
        {
            std::memset(screen->Elements, 0, sizeof(MCGuiObject*) * static_cast<size_t>(screen->NumElements));
        }
    }
}

// lCallback

// lButton

MCLogButton::~MCLogButton()
{
    MCLogButton::Destroy();
}

auto MCLogButton::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCLogObject::Init(xPos, yPos, width, height, name, nullptr);

    if (result != 0)
    {
        return result;
    }

    ButtonCallback = new MCLogCallback;
    UpPicture = nullptr;
    DownPicture = nullptr;
    GrayPicture = nullptr;
    OverPicture = nullptr;
    Disabled = 0;
    OverState = 0;
    Pressed = 0;
    BackgroundColor = 0;
    PressSound = 0xf;
    OverSound = 0xffffffff;
    return 0;
}

auto MCLogButton::Destroy() -> void
{
    FreePort(UpPicture);
    FreePort(DownPicture);
    FreePort(GrayPicture);
    FreePort(OverPicture);

    if (ButtonCallback != nullptr)
    {
        ButtonCallback->Destroy();
        delete ButtonCallback;
        ButtonCallback = nullptr;
    }

    if (HeldButton == this)
    {
        HeldButton = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCLogButton::Press() -> void
{
    LetGoPress();
    Pressed = -1;
    HeldButton = this;
}

auto MCLogButton::LetGoPress() -> void
{
    if (HeldButton != nullptr)
    {
        HeldButton->Pressed = 0;
        HeldButton = nullptr;
    }
}

auto MCLogButton::SetUpPicture(char* fileName) -> int32_t
{
    const int32_t result = LoadPicture(UpPicture, fileName);

    if (result == 0)
    {
        // The button takes the picture's size.
        BackgroundColor = 0xff;
        Resize(UpPicture->Width(), UpPicture->Height());
    }

    return result;
}

auto MCLogButton::SetOverPicture(char* fileName) -> int32_t
{
    return LoadPicture(OverPicture, fileName);
}

auto MCLogButton::SetGrayPicture(char* fileName) -> int32_t
{
    return LoadPicture(GrayPicture, fileName);
}

auto MCLogButton::SetDownPicture(char* fileName) -> int32_t
{
    return LoadPicture(DownPicture, fileName);
}

auto MCLogButton::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 1)
    {
        if (Disabled == 0)
        {
            // Shown pressed, and on screen before the callback runs.
            Press();
            SoundSystem()->PlayDigitalSample(PressSound, 1, nullptr, 0, 0);
            UpdateDisplay(0, 0, 0, 0, 0);
            ButtonCallback->Execute();
        }
        else
        {
            SoundSystem()->PlayDigitalSample(0x33, 1, nullptr, 0, 0);
        }
    }
    else if (event->Type == 4)
    {
        // The press shows until the button is let go (the original's next paint put the face back up).
        LetGoPress();
    }

    if (Disabled == 0 && EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCLogButton::Draw() -> void
{
    MCLogPort* picture = nullptr;

    if (Disabled != 0)
    {
        picture = GrayPicture;
    }
    else if (Pressed == 0 && (Application->GrabbedObject() != this || Application->CurrentObject() != this))
    {
        picture = OverState == 0 ? UpPicture : OverPicture;
    }
    else
    {
        picture = DownPicture;
    }

    DrawFace(picture, false);
}

auto MCLogButton::DrawFace(MCLogPort* picture, bool keyed) -> void
{
    if (picture != nullptr)
    {
        picture->CopyTo(_OwnPort->Frame(), 0, 0, keyed ? -1 : 0);
    }
    else
    {
        VfxPaneWipe(_OwnPort->Frame(), static_cast<uint32_t>(BackgroundColor));
    }

    MCLogObject::Draw();
}

auto MCLogButton::Enter() -> void
{
    if (Disabled == 0)
    {
        OverState = -1;

        if (HeldButton == this)
        {
            LetGoPress();
        }

        SoundSystem()->PlayDigitalSample(OverSound, 1, nullptr, 0, 0);
    }
}

// lTextObject

MCLogTextObject::~MCLogTextObject()
{
    MCLogTextObject::Destroy();
}

auto MCLogTextObject::Destroy() -> void
{
    MCLogObject::Destroy();
    BufferSize = 0;
    LogFree(Buffer);
    Buffer = nullptr;
    LogFree(OriginalBuffer);
    OriginalBuffer = nullptr;
}

auto MCLogTextObject::Draw() -> void
{
    VfxPaneWipe(_OwnPort->Frame(), static_cast<uint32_t>(BackgroundColor));
    Font->WriteString(_OwnPort->Frame(), 1, 1, reinterpret_cast<uint8_t*>(Buffer), -1);

    if (CursorPos > -1 && CursorPos < BufferSize)
    {
        const uint32_t color = CursorOn == 0 ? 0x1f : 0x10;
        VfxLineDraw(_OwnPort->Frame(), CursorPixel, 0, CursorPixel, Height(), color);
    }
}

auto MCLogTextObject::RestartBlink() -> void
{
    CursorOn = -1;
}

auto MCLogTextObject::Display() -> void
{
    MCLogObject::Display();
}

auto MCLogTextObject::SetCursorPos(int32_t pos) -> void
{
    CursorPos = pos;
    // The cursor sits one pixel after the text up to it.
    const char saved = Buffer[pos];
    Buffer[pos] = 0;
    const int32_t textWidth = Font->Width(reinterpret_cast<uint8_t*>(Buffer));
    Buffer[pos] = saved;
    CursorPixel = textWidth + 1;
}

auto MCLogTextObject::IsValid(char key) -> int
{
    switch (AllowedInput)
    {
        case INPUT_TEXT:
        {
            if (key >= ' ' && key < 0x7f)
            {
                return -1;
            }

            return 0;
        }
        case INPUT_DIGITS:
        {
            if (key >= '0' && key <= '9')
            {
                return -1;
            }

            return 0;
        }
        case INPUT_ANY:
            return -1;
        case INPUT_PORT:
        {
            if (key >= '2' && key <= '6')
            {
                return -1;
            }

            return 0;
        }
        default:
            return 0;
    }
}

auto MCLogTextObject::HandleEvent(MCGuiEvent* event) -> void
{
    // Tells the parent the entry is finished (Enter, or focus moving on).
    auto sendDone = [this]()
    {
        MCGuiEvent done;
        done.Clear();
        done.Type = 0x1e;
        done.Data = 5;
        Parent->HandleEvent(&done);
    };

    // Clears the whole buffer.
    auto clearBuffer = [this]()
    {
        std::memset(Buffer, 0, static_cast<size_t>(TextLength));
        TextLength = 0;
    };

    switch (event->Type)
    {
        case 1:
            Application->SetText(this);
            break;
        case 10:
        {
            if (AllowedInput == INPUT_NONE)
            {
                break;
            }

            const uint8_t key = event->Key;

            if (key == 8)
            {
                if (TextLength != 0)
                {
                    // Backspace on untouched text clears all of it.
                    int32_t newPos = 0;

                    if (std::strcmp(Buffer, OriginalBuffer) == 0)
                    {
                        clearBuffer();
                    }
                    else
                    {
                        Buffer[TextLength - 1] = 0;
                        newPos = CursorPos - 1;
                        TextLength--;
                    }

                    SetCursorPos(newPos);
                    RestartBlink();
                }
            }
            else if (key == 0x0d)
            {
                sendDone();
            }
            else if (key == 0x1b)
            {
                Cancel();
            }
            else if (TextLength + 1 < BufferSize && IsValid(static_cast<char>(key)) != 0)
            {
                Buffer[TextLength++] = static_cast<char>(key);
                SetCursorPos(CursorPos + 1);
                RestartBlink();
            }
            break;
        }

        case 0x10:
            sendDone();
            break;
        case 0x13:
        {
            // Timer 0: the cursor blink.
            if (event->Data == 0)
            {
                CursorOn = CursorOn == 0 ? 1 : 0;
            }
            break;
        }
        case 0x1e:
        {
            if (event->Data == 7)
            {
                // Focus: start the blink, and clear an empty-slot name so the player can type one.
                Application->AddTimer(this, 0, static_cast<int32_t>(MCPort::CaretBlinkTime()), 0, 0, 0);

                if (ClearEmptyOnFocus != 0 && std::strcmp(Buffer, EmptyFile) == 0)
                {
                    clearBuffer();
                    SetCursorPos(0);
                    RestartBlink();
                }
            }
            else if (event->Data == 8)
            {
                Application->RemoveTimer(this, 0);
                CursorOn = -1;
            }
            break;
        }
        default:
            break;
    }

    MCGuiObject::HandleEvent(event);
}

auto MCLogTextObject::InitBuffer(int32_t size, int32_t type) -> void
{
    if (BufferSize != 0)
    {
        LogFree(Buffer);
        Buffer = nullptr;
        LogFree(OriginalBuffer);
        OriginalBuffer = nullptr;
        BufferSize = 0;
    }

    if (size != 0)
    {
        BufferSize = size;
        Buffer = static_cast<char*>(LogAlloc(static_cast<uint32_t>(size)));
        OriginalBuffer = static_cast<char*>(LogAlloc(static_cast<uint32_t>(size)));
    }

    std::memset(Buffer, 0, static_cast<size_t>(BufferSize));
    std::memset(OriginalBuffer, 0, static_cast<size_t>(BufferSize));
    AllowedInput = type;
}

auto MCLogTextObject::SetStringBuffer(char* text) -> int32_t
{
    int32_t result = 0;
    int32_t length = static_cast<int32_t>(std::strlen(text));

    if (length < BufferSize)
    {
        std::strcpy(Buffer, text);
        std::strcpy(OriginalBuffer, text);
        TextLength = length;
    }
    else
    {
        // Too long: cut to the buffer (strncpy writes no terminator; the buffer's last byte stays 0). The length
        // is left as it was.
        std::strncpy(Buffer, text, static_cast<size_t>(BufferSize - 1));
        std::strncpy(OriginalBuffer, text, static_cast<size_t>(BufferSize - 1));
        length = BufferSize - 1;
        result = -1;
    }

    SetCursorPos(length);
    RestartBlink();
    return result;
}

// FileScrollPane

MCFileScrollPane::~MCFileScrollPane()
{
    MCFileScrollPane::Destroy();
}

auto MCFileScrollPane::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height) -> void
{
    if (LgWhiteFont != nullptr)
    {
        LineHeight = LgWhiteFont->Height() + 1;
    }

    MCScrollPane::Init(width, height, xPos, yPos, static_cast<char*>(nullptr));
    // The files are drawn into the content each frame (drawContent).
    ContentPort->InitView(width - SliderWidth, height);

    // The splash screens' own slider art over the scroll pane's.
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashscroll.tga", ArtPath);
    auto* art = new MCLogPort;
    art->Init(fileName);
    const int32_t numTiles = height / art->Height() - 1;

    for (int32_t i = 0; i < numTiles; i++)
    {
        art->CopyTo(SliderPort->Frame(), 0, art->Height() * i + 1, -1);
    }

    art->Destroy();
    delete art;

    UpArrowPort = new MCLogPort;
    DownArrowPort = new MCLogPort;
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashsupbup.tga", ArtPath);
    UpArrowPort->Init(fileName);
    UpArrowPort->CopyTo(SliderPort->Frame(), 0, 0, -1);
    std::snprintf(fileName, sizeof(fileName), "%slogart\\splashsdnbup.tga", ArtPath);
    DownArrowPort->Init(fileName);
    DownArrowPort->CopyTo(SliderPort->Frame(), 0, height - 0xf, -1);

    // The column headers (operation, mission, resource points). Each is added and removed again straight away:
    // they are drawn by the pane itself, not as children.
    static constexpr int32_t headerRects[3][4] = {
        {0, 0xa5, 0x37, 0xe}, {0x49, 0xa5, 0x37, 0xe}, {0x90, 0xa5, 0x60, 0xe}};

    for (int32_t i = 0; i < 3; i++)
    {
        auto* header = new MCFileColumnHeader;
        ColumnHeaders[i] = header;
        header->Init(headerRects[i][0], headerRects[i][1], headerRects[i][2], headerRects[i][3], nullptr, nullptr);
        header->Pane = this;
        header->Column = i;
        AddChild(header);
        RemoveChild(ColumnHeaders[i]);
    }

    // The slider's clean track, to erase the slider with.
    std::memcpy(TrackImage, SliderPort->Frame()->Window->Buffer, static_cast<size_t>(height * SliderWidth));
}

auto MCFileScrollPane::Destroy() -> void
{
    LogFree(StartDirectory);
    StartDirectory = nullptr;

    for (int32_t i = 0; i < NumFiles; i++)
    {
        LogFree(FileNames[i]);
        FileNames[i] = nullptr;
    }

    LogFree(FileNames);
    FileNames = nullptr;
    NumFiles = 0;

    if (NameEntry != nullptr)
    {
        NameEntry->Destroy();
        delete NameEntry;
        NameEntry = nullptr;
    }

    for (MCFileColumnHeader*& header : ColumnHeaders)
    {
        if (header != nullptr)
        {
            delete header;
            header = nullptr;
        }
    }

    if (FileOperations != nullptr)
    {
        LogFree(FileOperations);
        FileOperations = nullptr;
    }

    if (FileMissions != nullptr)
    {
        LogFree(FileMissions);
        FileMissions = nullptr;
    }

    if (FileResourcePoints != nullptr)
    {
        LogFree(FileResourcePoints);
        FileResourcePoints = nullptr;
    }

    FreePort(UpArrowPort);
    FreePort(DownArrowPort);
    MCScrollPane::Destroy();
}

auto MCFileColumnHeader::Draw() -> void
{
    VfxPaneWipe(Lport()->Frame(), 0x10);

    // The selected save's operation, mission and resource points (none for the multiplayer list, nor for a save
    // without an operation).
    const int32_t file = Pane->SelectedFile;

    if (Pane->Parent == nullptr || file < 0 || Pane->Multiplayer != 0 || Pane->FileOperations[file] <= 0)
    {
        return;
    }

    int32_t value = Pane->FileOperations[file];

    if (Column == 1)
    {
        value = Pane->FileMissions[file];
    }
    else if (Column == 2)
    {
        value = static_cast<int32_t>(Pane->FileResourcePoints[file]);
    }

    char text[16];
    std::snprintf(text, sizeof(text), "%i", value);
    LgWhiteFont->WriteString(Lport()->Frame(), 2, 2, reinterpret_cast<uint8_t*>(text), -1);
}

auto MCFileScrollPane::Draw() -> void
{
    MCScrollPane::Draw();
}

auto MCFileScrollPane::PressedArrowArt(bool down) -> MCLogPort*
{
    (void)down;
    return nullptr;
}

auto MCFileScrollPane::DrawContent() -> void
{
    DrawFiles();
}

auto MCFileScrollPane::Display() -> void
{
    if (IsShowing() != 0)
    {
        DrawInFramePass(PanePort, 0, false, false);
    }

    for (MCFileColumnHeader* header : ColumnHeaders)
    {
        header->Display();
    }

    for (int32_t i = 0; i < NumChildren; i++)
    {
        ChildList[i]->Display();
    }
}

auto MCFileScrollPane::HandleEvent(MCGuiEvent* event) -> void
{
    MCScrollPane::HandleEvent(event);

    switch (event->Type)
    {
        case 1:
        {
            if (NameEntry != nullptr && NameEntry->Parent == this)
            {
                NameEntry->Destroy();
            }

            const int32_t file = GetFileAtPosition(event->X - GlobalX(), event->Y - GlobalY());

            if (file < 0)
            {
                return;
            }

            if (file != SelectedFile)
            {
                SetSelectedFile(file);
            }

            if (SavePane == 0)
            {
                return;
            }

            // Saving: an entry field over the clicked name.
            if (NameEntry == nullptr)
            {
                NameEntry = new MCLogTextObject;
            }
            else
            {
                if (NameEntry->Parent != nullptr)
                {
                    return;
                }

                NameEntry->Destroy();
            }

            int32_t entryY = LineHeight * file - GetScrollOffset() - 1;
            int32_t entryHeight = LineHeight;

            if (entryY < 0)
            {
                entryY = 0;
                entryHeight--;
            }

            MCLogTextObject* entry = NameEntry;
            entry->MCLogObject::Init(1, entryY, Width() - 0x12, entryHeight, nullptr, nullptr);
            entry->CursorPos = 0;
            entry->CursorPixel = 0;
            entry->BackgroundColor = 0x1f;
            entry->Font = LgBlackFont;
            entry->ClearEmptyOnFocus = -1;
            entry->InitBuffer(0x20, MCLogTextObject::INPUT_ANY);
            entry->SetStringBuffer(FileNames[SelectedFile]);
            Application->SetText(entry);
            AddChild(entry);
            break;
        }

        case 0x10:
        {
            // A double click on the selected file presses the screen's load/save button.
            const int32_t file = GetFileAtPosition(event->X - GlobalX(), event->Y - GlobalY());

            if (file > -1 && file == SelectedFile)
            {
                MCLogButton* button = static_cast<MCGenericScreen*>(Parent)->LoadSaveButton;
                SoundSystem()->PlayDigitalSample(button->PressSound, 1, nullptr, 0, 0);
                button->Callback()->Execute();
                return;
            }
            break;
        }

        case 0x13:
            return;
        case 0x1e:
        {
            Parent->HandleEvent(event);
            return;
        }
        default:
            break;
    }
}

auto MCFileScrollPane::SetUpSlider() -> void
{
    const int32_t paneHeight = WinHeight;

    if (ContentPort->Height() <= paneHeight)
    {
        SliderHeight = 0;
        return;
    }

    if (SliderImage != nullptr)
    {
        LogFree(SliderImage);
    }

    const float paneHeightF = static_cast<float>(paneHeight);
    SliderHeight = static_cast<int32_t>(static_cast<double>(paneHeightF) / ContentPort->Height() * (paneHeight - 0x20));
    SliderPos = 0x10;

    if (SliderHeight < 3)
    {
        SliderHeight = 3;
    }

    const uint32_t size = static_cast<uint32_t>(SliderHeight * SliderWidth);
    SliderImageSize = size;
    auto* image = static_cast<uint8_t*>(LogAlloc(size));
    SliderImage = image;
    // As ScrollPane's slider, with the splash screens' edge colour (0xc0).
    static constexpr uint8_t sliderRow[SliderWidth] = {0xc0, 0x10, 0x1c, 0x1a, 0x1a, 0x1a, 0x1a,
                                                       0x1a, 0x1a, 0x1a, 0x17, 0x10, 0xc0};

    for (int32_t row = 0; row < SliderHeight; row++)
    {
        std::memcpy(image + row * SliderWidth, sliderRow, SliderWidth);
    }

    std::memset(image + 3, 0x1c, 8);
    std::memset(image + size - 11, 0x17, 9);
    // The pane draws the slider from its texture.
    MakeSliderTexture();
}

auto MCFileScrollPane::GetFileAtPosition(int32_t xPos, int32_t yPos) -> int32_t
{
    const int32_t contentY = GetScrollOffset() + yPos;

    for (int32_t i = 0; i < NumFiles; i++)
    {
        const tagRECT row = {1, LineHeight * i, Width() - SliderWidth, (i + 1) * LineHeight};

        if (PtInRect(&row, tagPOINT{xPos, contentY}))
        {
            return i;
        }
    }

    return -1;
}

auto MCFileScrollPane::SetStartDirectory(char* directory) -> void
{
    StartDirectory = static_cast<char*>(LogAlloc(static_cast<uint32_t>(std::strlen(directory) + 1)));
    std::sprintf(StartDirectory, "%s", directory);
    GetAllFiles(const_cast<char*>(Multiplayer != 0 ? ".mpk" : ".sav"), true);
}

auto MCFileScrollPane::LayoutFiles() -> void
{
    int32_t contentHeight = NumFiles * LineHeight;

    if (contentHeight < Height())
    {
        contentHeight = Height();
    }

    if (ContentPort->Height() != contentHeight)
    {
        MCLogPort* port = ContentPort;
        port->Resize(Width() - 0x12, contentHeight);
        SetDisplayPort(port, 0, -1);
    }
}

auto MCFileScrollPane::DrawFiles() -> void
{
    MCLogPort* port = ContentPort;
    VfxPaneWipe(port->Frame(), 0x10);

    for (int32_t i = 0; i < NumFiles; i++)
    {
        if (i == SelectedFile)
        {
            MCPane box = *_OwnPort->Frame();
            box.X0 = 1;
            box.X1 = Width() - 0x12;
            const int32_t rowY = LineHeight * i;
            box.Y0 = rowY - 1;
            box.Y1 = (i + 1) * LineHeight - 2;
            VfxPaneWipe(&box, 0x14);
            port = ContentPort;
            LgWhiteFont->WriteString(port->Frame(), 1, rowY, reinterpret_cast<uint8_t*>(FileNames[i]), -1);
        }
        else
        {
            LgGreyFont->WriteString(port->Frame(), 1, i * LineHeight, reinterpret_cast<uint8_t*>(FileNames[i]), -1);
        }
    }
}

auto MCFileScrollPane::GetAllFiles(char* extension, bool sort) -> void
{
    MCFitIniFile masterFiles[2];
    std::string pattern;
    std::string path;
    pattern = GamePath(StartDirectory, "*", extension);
    const std::vector<std::string> found = MCFileSystem::FindFiles(pattern);

    if (NumFiles != 0 && FileNames != nullptr)
    {
        for (int32_t i = 0; i < NumFiles; i++)
        {
            if (FileNames[i] != nullptr)
            {
                LogFree(FileNames[i]);
            }
        }

        LogFree(FileNames);
        FileNames = nullptr;
    }

    if (FileOperations != nullptr)
    {
        LogFree(FileOperations);
        FileOperations = nullptr;
    }

    if (FileMissions != nullptr)
    {
        LogFree(FileMissions);
        FileMissions = nullptr;
    }

    if (FileResourcePoints != nullptr)
    {
        LogFree(FileResourcePoints);
        FileResourcePoints = nullptr;
    }

    // Saving mid-campaign offers a new (empty) slot first.
    const bool newSlot = SavePane != 0 && GlobalLogPtr->CurrentMission >= 0;
    NumFiles = static_cast<int32_t>(found.size()) + (newSlot ? 1 : 0);

    if (NumFiles == 0)
    {
        SetSelectedFile(-1);
    }
    else
    {
        // Port fix: sized by the port's pointer size (the original: count * 4).
        FileNames = static_cast<char**>(LogAlloc(static_cast<uint32_t>(sizeof(char*) * static_cast<size_t>(NumFiles))));
        std::memset(FileNames, 0, sizeof(char*) * static_cast<size_t>(NumFiles));

        if (Multiplayer == 0)
        {
            const uint32_t size = static_cast<uint32_t>(NumFiles) << 2;
            FileOperations = static_cast<int32_t*>(LogAlloc(size));
            FileMissions = static_cast<int32_t*>(LogAlloc(size));
            FileResourcePoints = static_cast<uint32_t*>(LogAlloc(size));
        }
    }

    int32_t firstFile = 0;

    if (Multiplayer == 0)
    {
        // The operation and mission numbers come from the planets' master mission files (Port Arthur, Cermak).
        path = GamePath(MissionPath, "mechcmdr1", ".fit");
        int32_t result = masterFiles[0].Open(path);
        Assert(result == 0, 0, " could not open Port Arthur master mission file ");
        result = masterFiles[0].SeekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file");
        path = GamePath(MissionPath, "xmechcmdr1", ".fit");
        result = masterFiles[1].Open(path);
        Assert(result == 0, 0, " could not open Cermak master mission file ");
        result = masterFiles[1].SeekBlock("OpInfo");
        Assert(result == 0, 0, " could not find operation information in master mission file");

        if (newSlot)
        {
            MCFitIniFile& master = CurPlanet == 0 ? masterFiles[0] : masterFiles[1];
            // The empty slot's name block is 16 bytes (EmptyFile fits).
            FileNames[0] = static_cast<char*>(LogAlloc(0x10));
            std::strcpy(FileNames[0], EmptyFile);
            char key[64];
            int32_t operation = 0;
            int32_t mission = 0;
            std::snprintf(key, sizeof(key), "Scenario%iOperation", GlobalLogPtr->CurrentMission);
            result = master.ReadIdLong(key, operation);
            Assert(result == 0, 0, " could not find operation number in master mission file ");
            std::snprintf(key, sizeof(key), "Scenario%iMission", GlobalLogPtr->CurrentMission);
            result = master.ReadIdLong(key, mission);
            Assert(result == 0, 0, " could not find mission number in master mission file ");
            FileOperations[0] = operation;
            FileMissions[0] = mission;
            FileResourcePoints[0] = static_cast<uint32_t>(ResourcePoints);
            firstFile = 1;
        }
    }

    int32_t index = firstFile;

    for (const std::string& name : found)
    {
        // The name without its extension.
        const std::string stem = std::filesystem::path(name).stem().string();
        FileNames[index] = static_cast<char*>(LogAlloc(static_cast<uint32_t>(stem.size() + 1)));
        std::sprintf(FileNames[index], "%s", stem.c_str());
        path = GamePath(StartDirectory, stem.c_str(), extension);

        if (Multiplayer == 0)
        {
            MCPacketFile saveFile;
            MCFitIniFile saveFit;
            int32_t planet = 0;
            int32_t result = saveFile.Open(path);
            Assert(result == 0, result, " Could not find save game file ");
            result = saveFile.SeekPacket(0);
            Assert(result == 0, 0, " could not find packet 0 in save game file ");
            result = saveFit.Open(&saveFile, static_cast<uint32_t>(saveFile.GetPacketSize()));
            Assert(result == 0, 0, " could not open save game file ");

            if (saveFit.SeekBlock("Planet") == 0)
            {
                saveFit.ReadIdLong("Setting", planet);
            }

            result = saveFit.SeekBlock("General");
            Assert(result == 0, 0, " could not find General Block in campaign file ");
            int32_t missionNumber = 0;
            result = saveFit.ReadIdLong("MissionNumber", missionNumber);
            Assert(result == 0, 0, " Could not find MissionNumber in save game file ");
            result = saveFit.SeekBlock("ResourcePoints");
            Assert(result == 0, 0, " could not find ResourcePoints Block in save game file ");
            result = saveFit.ReadIdULong("numPoints", FileResourcePoints[index]);
            Assert(result == 0, 0, " Could not find numPoints in save game file ");
            saveFit.Close();
            saveFile.Close();
            char key[64];
            int32_t operation = 0;
            int32_t mission = 0;
            std::snprintf(key, sizeof(key), "Scenario%iOperation", missionNumber);
            result = masterFiles[planet].ReadIdLong(key, operation);
            Assert(result == 0, 0, " could not find operation number in master mission file ");
            std::snprintf(key, sizeof(key), "Scenario%iMission", missionNumber);
            result = masterFiles[planet].ReadIdLong(key, mission);
            Assert(result == 0, 0, " could not find mission number in master mission file ");
            FileOperations[index] = operation;
            FileMissions[index] = mission;
        }

        index++;
    }

    masterFiles[0].Close();
    masterFiles[1].Close();

    if (SelectedFile >= NumFiles)
    {
        SelectedFile = -1;
    }

    if (sort)
    {
        // Sorted by name; the selection follows its file.
        char selectedName[0x800];
        selectedName[0] = 0;

        if (SelectedFile != -1)
        {
            std::strcpy(selectedName, FileNames[SelectedFile]);
        }

        for (int32_t i = 0; i < NumFiles; i++)
        {
            for (int32_t j = i; j < NumFiles; j++)
            {
                if (std::strcmp(FileNames[i], FileNames[j]) > 0)
                {
                    std::swap(FileNames[i], FileNames[j]);

                    if (Multiplayer == 0)
                    {
                        std::swap(FileOperations[i], FileOperations[j]);
                        std::swap(FileMissions[i], FileMissions[j]);
                        std::swap(FileResourcePoints[i], FileResourcePoints[j]);
                    }
                }
            }
        }

        if (SelectedFile != -1)
        {
            for (int32_t i = 0; i < NumFiles; i++)
            {
                if (std::strcmp(FileNames[i], selectedName) == 0)
                {
                    SelectedFile = i;
                    break;
                }
            }
        }
    }

    LayoutFiles();
}

auto MCFileScrollPane::SetSelectedFile(int32_t file) -> void
{
    if (NameEntry != nullptr)
    {
        NameEntry->Destroy();
    }

    MCGuiEvent event;
    event.Clear();

    if (file < 0 || file >= NumFiles)
    {
        SelectedFile = -1;
        LayoutFiles();

        if (Parent != nullptr)
        {
            event.Type = 0x1e;
            event.Data = 2;
            event.LParam = file;
            Parent->HandleEvent(&event);
        }

        return;
    }

    // Scroll half a unit at a time until the row is in view.
    while (LineHeight * file - GetScrollOffset() < 0 && ScrollPos > 0.0f)
    {
        SetScrollPos(static_cast<float>(ScrollPos - 0.5));
    }
    while (Height() < (LineHeight + 1) * file - GetScrollOffset() && ScrollPos < MaxScroll)
    {
        SetScrollPos(static_cast<float>(ScrollPos + 0.5));
    }

    SelectedFile = file;
    LayoutFiles();
    event.Type = 0x1e;
    event.Data = 1;
    event.LParam = file;
    Parent->HandleEvent(&event);
}

auto MCFileScrollPane::SetMultiplayer(int newMultiplayer) -> void
{
    Multiplayer = newMultiplayer;
    GetAllFiles(const_cast<char*>(newMultiplayer != 0 ? ".mpk" : ".sav"), true);
}

// GenericScreen

MCGenericScreen::~MCGenericScreen()
{
    MCGenericScreen::Destroy();
}

auto MCGenericScreen::GetPaletteFromArt(char* fileName) -> uint8_t*
{
    char message[256];
    char path[252];
    MCFile file;
    std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

    if (file.Open(path) != 0)
    {
        std::snprintf(path, sizeof(path), "%s", fileName);

        if (file.Open(path) != 0)
        {
            std::snprintf(message, sizeof(message), "Error reading '%s'", path);
            GeneralMsg(message);
            return nullptr;
        }
    }

    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        std::snprintf(message, sizeof(message), "Error reading '%s'", path);
        GeneralMsg(message);
        return nullptr;
    }

    auto* data = static_cast<uint8_t*>(LogAlloc(size));

    if (data == nullptr)
    {
        return nullptr;
    }

    file.Read(data, static_cast<int32_t>(size));
    file.Close();
    // The TGA's palette (BGR after the 18-byte header), as 6-bit RGB.
    Palette = static_cast<uint8_t*>(LogAlloc(0x300));
    const uint8_t* source = data + 0x12;

    for (int32_t i = 0; i < 0x100; i++)
    {
        Palette[i * 3] = source[i * 3 + 2] >> 2;
        Palette[i * 3 + 1] = source[i * 3 + 1] >> 2;
        Palette[i * 3 + 2] = source[i * 3] >> 2;
    }

    LogFree(data);
    return Palette;
}

auto MCGenericScreen::Init(MCFitIniFile* screenFile) -> int32_t
{
    ReadElementCount(screenFile, this);

    for (int32_t i = 0; i < NumElements; i++)
    {
        int32_t type = -1;
        int32_t left = 0;
        int32_t top = 0;
        int32_t width = 0;
        int32_t height = 0;
        char art[256];
        ReadElement(screenFile, i, type, left, top, width, height, art);

        switch (type)
        {
            case 0:
            {
                // The background: the screen itself.
                Assert(i == 0, i, " Background MUST be first element ");
                int useBackPalette = 0;
                int32_t result = screenFile->ReadIdBoolean("UseBackPalette", useBackPalette);
                Assert(result == 0, result, " Could not find UseBackPalette for background Generic Screen");

                if (useBackPalette != 0)
                {
                    Palette = GetPaletteFromArt(art);
                }

                result = MCLogObject::Init(left, top, width, height, nullptr, nullptr);
                Assert(result == 0, result, " Could not start background Generic Screen ");
                ArtPort = new MCLogPort;
                result = ArtPort->Init(art);
                Assert(result == 0, result, " Could not find background Art in Generic Screen ");
                Elements[i] = this;
                break;
            }

            case 1:
            {
                auto* button = new MCLogButton;
                const int32_t result = button->Init(left, top, width, height, nullptr);
                Assert(result == 0, result, " Couldn't init new button ");
                int32_t callback = 0;

                if (ReadButton(screenFile, button, art, callback) && !SetScreenCallback(this, button, callback))
                {
                    Fatal(callback, " Illegal callback value");
                }

                Elements[i] = button;
                AddChild(button);
                break;
            }

            case 4:
            {
                MCLogTextObject* entry = MakeTextEntry(left, top, width, height);
                Elements[i] = entry;
                AddChild(entry);
                break;
            }

            case 5:
            {
                MCFileScrollPane* pane = MakeFilePane(screenFile, left, top, width, height);
                pane->ShowGuiWindow(-1);
                Elements[i] = pane;
                AddChild(pane);
                FilePane = pane;
                break;
            }

            default:
                break;
        }
    }

    ScreenWindow->AddChild(this);
    ShowGuiWindow(0);
    return 0;
}

auto MCGenericScreen::Destroy() -> void
{
    ScreenWindow->RemoveChild(this);

    // Element 0 is the background (the screen itself).
    for (int32_t i = 1; i < NumElements; i++)
    {
        MCGuiObject* element = Elements[i];
        RemoveChild(element);

        // Port fix: slots of skipped element types are empty (the original deleted whatever the heap held).
        if (element != nullptr)
        {
            element->Destroy();
            delete element;
        }

        Elements[i] = nullptr;
    }

    LogFree(Elements);
    Elements = nullptr;
    LogFree(Palette);
    Palette = nullptr;
    NumElements = 0;
    NumChildren = 0;
    FreePort(ArtPort);
    MCLogObject::Destroy();
}

auto MCGenericScreen::Draw() -> void
{
    if (ArtPort != nullptr && Lport()->ViewOpen())
    {
        ArtPort->CopyTo(Lport()->Frame(), 0, 0, 0);
    }

    MCLogObject::Draw();
}

auto MCLogImage::Destroy() -> void
{
    FreePort(Art);
    MCLogObject::Destroy();
}

auto MCLogImage::Draw() -> void
{
    if (Art != nullptr)
    {
        Art->CopyTo(Lport()->Frame(), 0, 0, 0);
    }

    MCLogObject::Draw();
}

auto MCGenericScreen::HandleEvent(MCGuiEvent* event) -> void
{
    if (event->Type == 9 && event->Key == 0x1b)
    {
        Cancel();
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

auto MCGenericScreen::ShowGuiWindow(int show) -> void
{
    // A screen shows its buttons up (the original painted it afresh), the one clicked to leave it included.
    MCLogButton::LetGoPress();

    if (show == 0)
    {
        // Hiding drops a half-typed save name.
        if (FilePane != nullptr && FilePane->NameEntry != nullptr)
        {
            FilePane->NameEntry->Destroy();
            ShowWindow = 0;
            return;
        }

        ShowWindow = show;
        return;
    }

    if (this == GlobalLogPtr->MainScreen)
    {
        // The main menu enables what the install and the campaign allow.
        const int noMission = GlobalLogPtr->CurrentMission < 0 ? 1 : 0;
        auto* saveButton = static_cast<MCLogButton*>(Elements[2]);
        saveButton->Disabled = Solo == 0 ? noMission : -1;
        auto* returnButton = static_cast<MCLogButton*>(Elements[7]);
        returnButton->Disabled = noMission;

        char pattern[256];
        std::snprintf(pattern, sizeof(pattern), "%s*.sav", SavePath);
        auto* loadButton = static_cast<MCLogButton*>(Elements[3]);
        loadButton->Disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;
        std::snprintf(pattern, sizeof(pattern), "%s*.sol", SavePath);
        auto* soloLoadButton = static_cast<MCLogButton*>(Elements[10]);
        soloLoadButton->Disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;

        // Multiplayer needs 30 MB.
        if (MCPort::TotalPhysicalMemory() < 30000000)
        {
            auto* multiplayerButton = static_cast<MCLogButton*>(Elements[5]);
            multiplayerButton->Disabled = -1;
        }

        if (InDemo != 0)
        {
            auto* button = static_cast<MCLogButton*>(Elements[4]);
            button->Disabled = -1;
            button = static_cast<MCLogButton*>(Elements[5]);
            button->Disabled = -1;
            std::snprintf(pattern, sizeof(pattern), "%sopening.smk", CDmoviePath);
            auto* cinemaButton = static_cast<MCLogButton*>(Elements[6]);
            cinemaButton->Disabled = MCFileSystem::FindFiles(pattern).empty() ? -1 : 0;
        }
    }
    else if (this == GlobalLogPtr->SaveScreen || this == GlobalLogPtr->LoadScreen)
    {
        if (FilePane->Multiplayer != 0)
        {
            FilePane->GetAllFiles(const_cast<char*>(".mpk"), true);
        }
        else
        {
            FilePane->GetAllFiles(const_cast<char*>(LoadingSolo == 0 ? ".sav" : ".sol"), true);
        }
    }

    // The load and save screens (single player) and the preferences keep the current palette.
    MCLogObject* current = GlobalLogPtr->CurrentScreen;
    const bool fileScreen = current == GlobalLogPtr->SaveScreen || current == GlobalLogPtr->LoadScreen;

    if ((!fileScreen || GlobalLogPtr->LoadScreen->FilePane->Multiplayer != 0) && current != GlobalLogPtr->PrefScreen)
    {
        if (Palette != nullptr)
        {
            Application->ActivatePalette(Palette, 0, 0x100);
            ShowWindow = show;
            return;
        }

        GamePalette()->Activate();
    }

    ShowWindow = show;
}

// MCSplashScreen

MCSplashScreen::MCSplashScreen()
{
    // The screens share one background port while any exists.
    if (_InstanceCount == 0 && _GenericPort == nullptr)
    {
        _GenericPort = new MCLogPort;
        std::strcpy(_GenericPortFileName, "None");
    }

    _InstanceCount++;
}

MCSplashScreen::~MCSplashScreen()
{
    _InstanceCount--;
    // The shared art isn't this screen's to free.
    ArtPort = nullptr;

    if (_InstanceCount == 0 && _GenericPort != nullptr)
    {
        _GenericPort->Destroy();
        delete _GenericPort;
        _GenericPort = nullptr;
    }

    if (Blocks != nullptr)
    {
        for (int32_t i = 0; i < NumBlocks; i++)
        {
            if (Blocks[i] != nullptr)
            {
                LogFree(Blocks[i]);
                Blocks[i] = nullptr;
            }
        }

        LogFree(Blocks);
        Blocks = nullptr;
    }

    MCGenericScreen::Destroy();
}

auto MCSplashScreen::Init(MCFitIniFile* screenFile) -> int32_t
{
    ReadElementCount(screenFile, this);

    // Blocks: which elements show together (showBlock), one byte per element.
    if (screenFile->SeekBlock("Blocks") == 0)
    {
        int32_t result = screenFile->ReadIdLong("Block Count", NumBlocks);
        Assert(result == 0, result, " Could not find block count in Generic Screen ");
        // Port fix: sized by the port's pointer size (the original: count * 4).
        Blocks =
            static_cast<uint8_t**>(LogAlloc(static_cast<uint32_t>(sizeof(uint8_t*) * static_cast<size_t>(NumBlocks))));
        Assert(Blocks != nullptr, 0, " No RAM for Generic Screen Block Array ");
        const uint32_t blockSize = static_cast<uint32_t>(NumElements);

        for (int32_t i = 0; i < NumBlocks; i++)
        {
            Blocks[i] = static_cast<uint8_t*>(LogAlloc(blockSize));
            char blockName[20];
            std::snprintf(blockName, sizeof(blockName), "Block%d", i);
            screenFile->ReadIdUCharArray(blockName, Blocks[i], blockSize);
        }
    }

    for (int32_t i = 0; i < NumElements; i++)
    {
        int32_t type = -1;
        int32_t left = 0;
        int32_t top = 0;
        int32_t width = 0;
        int32_t height = 0;
        char art[256];
        ReadElement(screenFile, i, type, left, top, width, height, art);
        MCGuiObject* element = nullptr;

        switch (type)
        {
            case 0:
            {
                // The background: the shared port, reloaded only when the art changes.
                Assert(i == 0, i, " If there's a background it MUST be the first element ");

                if (MCPort::StrICmp(_GenericPortFileName, art) != 0)
                {
                    std::strcpy(_GenericPortFileName, art);
                    const int32_t result = _GenericPort->Init(art);
                    Assert(result == 0, result, " Could not find background Art in Generic Screen ");
                }

                int useBackPalette = 0;
                int32_t result = screenFile->ReadIdBoolean("UseBackPalette", useBackPalette);
                Assert(result == 0, result, " Could not find UseBackPalette for background Generic Screen");

                if (useBackPalette != 0)
                {
                    Palette = GetPaletteFromArt(art);
                }

                // The screen draws the shared art each frame (the original made the shared port its own).
                result = MCLogObject::Init(left, top, width, height, nullptr, nullptr);
                Assert(result == 0, result, " Could not start background Generic Screen ");
                ArtPort = _GenericPort;
                Elements[i] = this;
                continue;
            }

            case 1:
            {
                auto* button = new MCLogButton;
                const int32_t result = button->Init(left, top, width, height, nullptr);
                button->SetTransparent(-1);
                Assert(result == 0, result, " Couldn't init new button ");
                int32_t callback = 0;

                if (ReadButton(screenFile, button, art, callback))
                {
                    void (*exec)() = nullptr;

                    switch (callback)
                    {
                        case 12:
                            exec = ShowModemScreen;
                            break;
                        case 13:
                            exec = ShowSerialScreen;
                            break;
                        case 14:
                            exec = ShowLanScreen;
                            break;
                        case 15:
                            exec = ShowInternet;
                            break;
                        case 16:
                            exec = CancelToConnect;
                            break;
                        case 17:
                            exec = CancelToLan;
                            break;
                        case 18:
                            exec = HostGame;
                            break;
                        case 19:
                            exec = JoinGame;
                            break;
                        case 20:
                            exec = CreateSession;
                            break;
                        case 21:
                            exec = Go;
                            break;
                        case 22:
                            exec = ::Leave;
                            break;
                        case 23:
                            exec = WaitForCall;
                            break;
                        case 24:
                            exec = GetNumber;
                            break;
                        case 25:
                            exec = Dial;
                            break;
                        case 35:
                            exec = CancelPrefs;
                            break;
                        case 36:
                            exec = WritePrefs;
                            break;
                        case 37:
                            exec = CreateSerialSession;
                            break;
                        case 38:
                            exec = SerialJoinButtonPressed;
                            break;
                        case 40:
                            exec = NewCampaign;
                            break;
                        case 41:
                            exec = SoloLoadScreen;
                            break;
                        default:
                            break;
                    }

                    if (exec != nullptr)
                    {
                        button->Callback()->SetExec(exec);
                    }
                    else if (!SetScreenCallback(this, button, callback))
                    {
                        Fatal(callback, " Illegal callback value");
                    }
                }

                Elements[i] = button;
                AddChild(button);
                continue;
            }

            case 4:
            {
                MCLogTextObject* entry = MakeTextEntry(left, top, width, height);
                Elements[i] = entry;
                entry->ShowGuiWindow(-1);
                AddChild(entry);
                continue;
            }

            case 5:
            {
                MCFileScrollPane* pane = MakeFilePane(screenFile, left, top, width, height);
                FilePane = pane;
                pane->ShowGuiWindow(-1);
                AddChild(pane);
                Elements[i] = pane;
                continue;
            }

            case 6:
            {
                // A picture; elements 6 and the others go just behind the rest.
                auto* image = new MCLogImage;
                Elements[i] = image;
                image->Init(left, top, width, height, nullptr, nullptr);
                image->Art = new MCLogPort;
                image->Art->Init(art);
                AddChild(image);
                Elements[i]->SetDepth(i == 6 ? -0xb : -0xa);
                Elements[i]->SetEventRoutine(ImageHandleEvent);
                continue;
            }

            case 7:
            {
                auto* text = new MCLogScrollTextObject;
                text->Init(left, top, width, height, nullptr);
                int scrolling = 0;
                const int32_t result = screenFile->ReadIdBoolean("Scrolling", scrolling);
                Assert(result == 0, result, " Couldn't locate Scrolling in textscrollpane ");
                text->ScrollTab->ShowGuiWindow(scrolling);
                text->Scrolling = scrolling;
                text->ShowGuiWindow(-1);
                AddChild(text);
                Elements[i] = text;
                continue;
            }

            case 8:
            {
                auto* list = new MCGameList;
                Elements[i] = list;
                list->Init(left, top, width, height, nullptr);
                element = list;
                break;
            }

            case 9:
            {
                auto* slider = new MCLogSlider;
                Elements[i] = slider;
                slider->Init(left, top, width, height, nullptr);
                int32_t value = 0;
                int32_t result = screenFile->ReadIdLong("MinValue", value);
                Assert(result == 0, result, " Couldn't locate min slider value");
                slider->MinValue = value;
                result = screenFile->ReadIdLong("MaxValue", value);
                Assert(result == 0, result, " Couldn't locate max slider value");
                slider->MaxValue = value;
                int32_t callback = 0;
                result = screenFile->ReadIdLong("Callback", callback);
                Assert(result == 0, result, " Couldn't locate callback value");

                switch (callback)
                {
                    case 0x1a:
                        slider->SetEventRoutine(SlideScreenBrightness);
                        break;
                    case 0x1b:
                        slider->SetEventRoutine(SlideMusicVolume);
                        break;
                    case 0x1c:
                        slider->SetEventRoutine(SlideRadioVolume);
                        break;
                    case 0x1d:
                        slider->SetEventRoutine(SlideFXVolume);
                        break;
                    default:
                        break;
                }

                element = slider;
                break;
            }

            case 10:
            {
                // A difficulty toggle.
                auto* toggle = new MCLogToolButton;
                Elements[i] = toggle;
                toggle->Init(left, top, width, height, nullptr);
                toggle->SetTransparent(-1);

                if (MCPort::StrICmp(art, "NONE") == 0)
                {
                    toggle->SetBackColor(0xff);
                }
                else
                {
                    const int32_t result = toggle->SetUpPicture(art);
                    Assert(result == 0, result, " Couldn't locate button upPicture image ");
                }

                // readButton would load NormalArt again; the rest is the same.
                int32_t result = screenFile->ReadIdString("GreyArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find gray button art in Generic Screen ");

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->SetGrayPicture(art);
                    Assert(result == 0, result, " Couldn't locate button grayPicture image ");
                }

                result = screenFile->ReadIdString("PressArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find down button art in Generic Screen ");

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->SetDownPicture(art);
                    Assert(result == 0, result, " Couldn't locate button downPicture image ");
                }

                result = screenFile->ReadIdString("OverArt", art, 0xf9);
                Assert(result == 0, result, " Could not Find button rollover art in Generic Screen ");

                if (MCPort::StrICmp(art, "NONE") != 0)
                {
                    result = toggle->SetOverPicture(art);
                    Assert(result == 0, result, " Couldn't locate button downPicture image ");
                }

                int32_t sound = 0;
                result = screenFile->ReadIdLong("OverSFX", sound);
                Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ");
                toggle->OverSound = static_cast<uint32_t>(sound);
                result = screenFile->ReadIdLong("PressSFX", sound);
                Assert(result == 0, result, " Could not Find Element Sound in Generic Screen ");
                toggle->PressSound = static_cast<uint32_t>(sound);
                int32_t callback = 0;

                if (screenFile->ReadIdLong("Callback", callback) == 0)
                {
                    switch (callback)
                    {
                        case 0x29:
                            toggle->Callback()->SetExec(EasyToggle);
                            break;
                        case 0x2a:
                            toggle->Callback()->SetExec(RegularToggle);
                            break;
                        case 0x2b:
                            toggle->Callback()->SetExec(HardToggle);
                            break;
                        default:
                            Fatal(callback, " Illegal callback value");
                            break;
                    }
                }

                element = toggle;
                break;
            }

            default:
                continue;
        }

        element->ShowGuiWindow(-1);
        AddChild(element);
    }

    ScreenWindow->AddChild(this);
    ShowGuiWindow(0);
    return 0;
}

auto MCSplashScreen::Destroy() -> void
{
    if (Blocks != nullptr)
    {
        for (int32_t i = 0; i < NumBlocks; i++)
        {
            if (Blocks[i] != nullptr)
            {
                LogFree(Blocks[i]);
                Blocks[i] = nullptr;
            }
        }

        LogFree(Blocks);
        Blocks = nullptr;
    }

    // The shared art isn't this screen's to free.
    ArtPort = nullptr;
    MCGenericScreen::Destroy();
}

auto MCSplashScreen::ShowGuiWindow(int show) -> void
{
    // The connection screens poll (timer 0, every 2 s) while shown: the connect screen through its element 3.
    if (this == GlobalLogPtr->ConnectScreen)
    {
        if (show != 0)
        {
            Application->AddTimer(Elements[3], 0, 2000, 0, 0, 0);
            MCGenericScreen::ShowGuiWindow(show);
            return;
        }

        Application->RemoveTimer(Elements[3], 0);
    }
    else if (this == GlobalLogPtr->LanScreen)
    {
        if (show != 0)
        {
            Application->AddTimer(this, 0, 2000, 0, 0, 0);
            MCGenericScreen::ShowGuiWindow(show);
            return;
        }

        Application->RemoveTimer(this, 0);
    }

    MCGenericScreen::ShowGuiWindow(show);
}

auto MCSplashScreen::ShowBlock(int32_t block) -> void
{
    if (block >= NumBlocks)
    {
        return;
    }

    const uint8_t* shown = Blocks[block];

    for (int32_t i = 1; i < NumElements; i++)
    {
        Elements[i]->ShowGuiWindow(0);
    }

    for (int32_t i = 1; i < NumElements; i++)
    {
        if (shown[i] != 0)
        {
            Elements[shown[i]]->ShowGuiWindow(-1);
        }
    }
}

// The scroll text thumb

auto LogPaintScrollTab(MCGuiObject* tab) -> void
{
    const int32_t width = tab->Width();
    const int32_t height = tab->Height();
    MCPane* pane = static_cast<MCLogObject*>(tab)->Lport()->Frame();
    VfxPaneWipe(pane, 0x1a);
    VfxLineDraw(pane, 0, 0, width - 2, 0, 0x1f);
    VfxLineDraw(pane, 0, 0, 0, height - 2, 0x1f);
    VfxLineDraw(pane, width - 1, 0, width - 1, height - 1, 0x16);
    VfxLineDraw(pane, 0, height - 1, width - 1, height - 1, 0x16);
}

auto LogScrollTabHandleEvent(MCGuiObject* tab, MCGuiEvent* event) -> void
{
    switch (event->Type)
    {
        case 1:
        {
            Application->Grab(tab);
            tab->StartDrag(0, event->Y - tab->GlobalY());
            break;
        }
        case 4:
        {
            Application->Release();
            tab->StopDrag();
            break;
        }
        case 7:
        {
            if (Application->GrabbedObject() == nullptr)
            {
                break;
            }

            // Port fix: the original is aScrollTextObject's handler compiled for this thumb: it reads the parent's
            // thumb at aScrollTextObject's offset (+0x4c8, here highlightLine[1]) and calls
            // aScrollTextObject::CalcFirstPixel, which writes over lObject's port pointer (OB-073). The port uses the
            // lScrollTextObject's own thumb and CalcFirstPixel.
            auto* textObject = static_cast<MCLogScrollTextObject*>(tab->Parent);
            tab->MoveTo(tab->X(), (event->Y - tab->Parent->Y()) - tab->DragStartY(), 0);

            if (tab->Y() < 0xf)
            {
                tab->MoveTo(tab->X(), 0xf, 0);
            }

            if (tab->Y() > (-0x10 - textObject->ScrollTab->Height()) + textObject->Height())
            {
                tab->MoveTo(tab->X(), (-0x10 - textObject->ScrollTab->Height()) + textObject->Height(), 0);
            }

            textObject->CalcFirstPixel(tab->Y() - 0xf);
            break;
        }

        default:
            break;
    }
}

// lScrollTextObject

MCLogScrollTextObject::~MCLogScrollTextObject()
{
    MCLogScrollTextObject::Destroy();
}

auto MCLogScrollTextObject::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    int32_t result = MCLogObject::Init(xPos, yPos, width, height, newText, nullptr);

    if (result != 0)
    {
        return result;
    }

    auto* tab = new MCLogObject;
    ScrollTab = tab;

    if (tab == nullptr)
    {
        Fatal(0, "Not enough memory for scrollbar tab.");
    }

    result = tab->Init(0, 0, 9, height - 0x1e, nullptr, nullptr);

    if (result != 0)
    {
        return result;
    }

    tab->MoveTo(this->Width() + 2, 0xf, 0);
    tab->SetDepth(100);
    AddChild(tab);
    tab->ShowGuiWindow(-1);
    tab->SetEventRoutine(LogScrollTabHandleEvent);
    tab->SetPaintRoutine(LogPaintScrollTab);
    tab->SetDrawsLive();
    tab->SetDepth(1);

    Text = static_cast<char*>(LogAlloc(TextBufferSize + 1));

    if (Text == nullptr)
    {
        Fatal(0, "Not enough memory for text.");
    }

    std::memset(Text, 0, TextBufferSize);
    FontIndex = 0;
    NumLines = 0;
    TextLength = 0;
    FirstPixel = 0;
    ScrollTab->ShowGuiWindow(-1);
    Scrolling = -1;

    for (int32_t i = 0; i < 4; i++)
    {
        HighlightLine[i] = -1;
        HighlightColor[i] = 0xff;
    }

    // Text given at init prints in colour 0x1f; without it the object doesn't scroll.
    if (newText != nullptr)
    {
        Print(newText, 0x1f);
    }
    else
    {
        Scrolling = 0;
    }

    TabColumn = -1;
    PositionScrollTab();
    return 0;
}

auto MCLogScrollTextObject::Destroy() -> void
{
    if (ScrollTab != nullptr)
    {
        ScrollTab->Destroy();
        delete ScrollTab;
        ScrollTab = nullptr;
    }

    if (Text != nullptr)
    {
        LogFree(Text);
        Text = nullptr;
    }

    MCLogObject::Destroy();
}

auto MCLogScrollTextObject::Draw() -> void
{
    int32_t lineY = 2;
    char* line = Text;
    const int32_t lineHeight = Fonts[0][FontIndex]->Height() + 4;
    VfxPaneWipe(Lport()->Frame(), 0x10);

    // Highlighted lines.
    for (int32_t i = 0; i < 4; i++)
    {
        const int32_t start = HighlightLine[i];

        if (start != -1 && start < start + 1)
        {
            MCPane box = *Lport()->Frame();
            box.X0 = 0;
            box.Y0 = start * lineHeight;
            box.X1 = Width();
            box.Y1 = (start + 1) * lineHeight;
            VfxPaneWipe(&box, HighlightColor[i]);
        }
    }

    // Lines are (colour byte, text, '\n'). A tab splits a line into two pieces, the second drawn at tabColumn.
    while (line != nullptr && *line != '\0')
    {
        const uint8_t color = static_cast<uint8_t>(*line);
        line++;
        int32_t pieces = 0;
        int32_t lineX = 2;

        if (char* tab = std::strchr(line, '\t'); tab != nullptr)
        {
            if (TabColumn < 0)
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
            Fonts[fontRow][FontIndex]->WriteStringToNewline(Lport()->Frame(), lineX, lineY,
                                                            reinterpret_cast<uint8_t*>(line));
            line = std::strchr(line, '\n');

            if (pieces > 0)
            {
                if (line != nullptr)
                {
                    *line = '\t';
                }

                lineX = TabColumn;
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
        DrawChild(ChildList[i]);
    }
}

auto MCLogScrollTextObject::Display() -> void
{
    if (ShowWindow == 0)
    {
        return;
    }

    if (IsHidden() != 0 && HideOffset == 0)
    {
        return;
    }

    // The lines scrolled by firstPixel (a list that doesn't scroll shows its top), then the children.
    DrawInFramePass(Lport(), Scrolling != 0 ? FirstPixel : 0);
}

auto MCLogScrollTextObject::Resize(int32_t width, int32_t height) -> void
{
    const int32_t fontHeight = Fonts[0][FontIndex]->Height();

    if (width <= 0 || height <= 0 || (width == WinWidth && height == WinHeight))
    {
        return;
    }

    WinWidth = width;
    WinHeight = height;
    FramePane->X1 = FramePane->X0 - 1 + width;
    FramePane->Y1 = FramePane->Y0 - 1 + height;
    // A scrolling object's port holds all its lines.
    int32_t portHeight = height;

    if (Scrolling != 0)
    {
        const int32_t textHeight = NumLines * (fontHeight + 4);

        if (textHeight > height)
        {
            portHeight = textHeight;
        }
    }

    Lport()->Resize(width, portHeight);
    ScrollTab->MoveTo(width + 2, ScrollTab->Y(), 0);
    PositionScrollTab();
}

auto MCLogScrollTextObject::ResetPortSize() -> void
{
    if (Scrolling == 0)
    {
        return;
    }

    // The port only grows.
    int32_t portHeight = (Fonts[0][FontIndex]->Height() + 4) * NumLines;

    if (portHeight <= Lport()->Height())
    {
        portHeight = Lport()->Height();
    }

    Lport()->Resize(Lport()->Width(), portHeight);
}

auto MCLogScrollTextObject::Print(char* line, uint8_t color) -> void
{
    const int32_t used = TextLength;
    const int32_t fontHeight = Fonts[0][FontIndex]->Height();

    if (TextBufferSize - used <= 2)
    {
        return;
    }

    Text[used] = static_cast<char>(color);
    char* dest = Text + used + 1;
    const int32_t textStart = used + 1;
    TextLength = textStart;

    if (line == nullptr)
    {
        if (TextBufferSize - textStart > 2)
        {
            // A blank line.
            NumLines++;
            TextLength = used + 2;
            Text[used + 1] = '\n';
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
        Text[TextBufferSize] = '\0';
    }

    NumLines++;
    const int32_t needed = NumLines * (fontHeight + 4);

    if (Scrolling != 0 && needed > Lport()->Height())
    {
        Lport()->Resize(Lport()->Width(), needed);
    }

    PositionScrollTab();
}

auto MCLogScrollTextObject::PrintWrapped(char* line, uint8_t color, int32_t wrapWidth) -> void
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

auto MCLogScrollTextObject::Clear() -> void
{
    FirstPixel = 0;
    TextLength = 0;
    NumLines = 0;
    std::memset(Text, 0, TextBufferSize);

    for (int32_t& line : HighlightLine)
    {
        line = -1;
    }
}

auto MCLogScrollTextObject::CalcFirstPixel(int32_t tabPos) -> void
{
    const int32_t track = (-0x1e - ScrollTab->Height()) + Height();
    const int32_t range = Lport()->Height() - Height();

    if (Scrolling != 0 && track > 0 && range > 0)
    {
        FirstPixel = (range * tabPos) / track;
        return;
    }

    FirstPixel = 0;
}

auto MCLogScrollTextObject::PositionScrollTab() -> void
{
    if (Application->GrabbedObject() == ScrollTab)
    {
        return;
    }

    const int32_t track = Height() - 0x1e;
    const int32_t range = Lport()->Height() - Height();

    if (range == 0)
    {
        ScrollTab->ShowGuiWindow(0);
        return;
    }

    // The thumb's length is the visible fraction of the track (x87: float quotient, then times the track).
    const float shown = static_cast<float>(Height());
    int32_t tabLength = static_cast<int32_t>(static_cast<double>(shown) / Lport()->Height() * track);

    if (tabLength < 3)
    {
        tabLength = 3;
    }

    ScrollTab->ShowGuiWindow(-1);
    ScrollTab->Resize(ScrollTab->Width(), tabLength);
    ScrollTab->MoveTo(ScrollTab->X(), ((track - tabLength) * FirstPixel) / range + 0xf, 0);
}

auto MCLogScrollTextObject::ReceiveClick(int32_t direction, int32_t yPos) -> void
{
    if (Height() == Lport()->Height())
    {
        return;
    }

    const int32_t lineHeight = Fonts[0][FontIndex]->Height() + 4;
    auto clampToEnd = [this]()
    {
        if (FirstPixel > Lport()->Height() - Height())
        {
            FirstPixel = Lport()->Height() - Height();
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
        // A page up isn't clamped at the top (unlike aScrollTextObject's).
        if (yPos < ScrollTab->Y())
        {
            FirstPixel -= Height();
        }

        if (yPos > ScrollTab->Bottom())
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

auto MCLogScrollTextObject::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    if (Height() == Lport()->Height())
    {
        return false;
    }

    for (; steps != 0; steps += steps < 0 ? 1 : -1)
    {
        ReceiveClick(steps < 0 ? -1 : 1, 0);
    }

    return true;
}

auto MCLogScrollTextObject::GetTextLine(int32_t line, char* dest, int32_t destSize) -> int
{
    char* p = Text;

    if (line < 0)
    {
        return 0;
    }

    // Lines 0 and 1 are both the first line.
    int32_t remaining = line - 1;

    while (p != nullptr)
    {
        if (*p == '\0' || remaining <= 0)
        {
            if (*p == '\0')
            {
                return 0;
            }

            if (dest != nullptr && destSize > 0)
            {
                std::strncpy(dest, p + 1, static_cast<size_t>(destSize - 1));

                if (char* end = std::strchr(dest, '\n'); end != nullptr)
                {
                    *end = '\0';
                }

                dest[destSize - 1] = '\0';
            }

            return -1;
        }

        p = std::strchr(p + 1, '\n');

        if (p != nullptr)
        {
            p++;
        }

        remaining--;
    }

    return 0;
}

// GameList

MCGameList::~MCGameList()
{
    MCLogScrollTextObject::Destroy();
}

auto MCGameList::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* newText) -> int32_t
{
    NumSessions = -1;
    SelectedSession = -1;
    const int32_t result = MCLogScrollTextObject::Init(xPos, yPos, width, height, newText);
    TabColumn = 0x73;
    HighlightColor[0] = 0x14;
    return result;
}

auto MCGameList::Draw() -> void
{
    MCLogScrollTextObject::Draw();
}

auto MCGameList::RebuildLines() -> void
{
    FirstPixel = 0;
    TextLength = 0;
    NumLines = 0;

    for (int32_t& line : HighlightLine)
    {
        line = -1;
    }

    std::memset(Text, 0, TextBufferSize);

    if (MPlayer != nullptr)
    {
        // "name <tab> free slots", or FULL; the selection is highlighted.
        for (int32_t i = 0; i < NumSessions; i++)
        {
            MCFidpSession* session = MPlayer->SessionManager->FindMatchingSession(&Sessions[i]);

            if (session == nullptr)
            {
                continue;
            }

            const int32_t freeSlots =
                static_cast<int32_t>(session->SessionDesc.dwMaxPlayers - session->SessionDesc.dwCurrentPlayers);
            char line[256];

            if (freeSlots == 0)
            {
                std::snprintf(line, sizeof(line), "%s\tFULL", session->SessionDesc.lpszSessionNameA);
            }
            else
            {
                std::snprintf(line, sizeof(line), "%s\t%d", session->SessionDesc.lpszSessionNameA, freeSlots);
            }

            if (i == SelectedSession)
            {
                Print(line, 0x1f);
                HighlightLine[0] = i;
            }
            else
            {
                Print(line, 0x0c);
            }
        }
    }
}

auto IsSessionDeleted(MCFidpSession* session) -> int
{
    for (int32_t i = 0; i < NextDeletedSession; i++)
    {
        if (std::memcmp(&session->SessionDesc.guidInstance, &DeletedSessions[i], sizeof(_GUID)) == 0)
        {
            return -1;
        }
    }

    return 0;
}

auto MCGameList::HandleEvent(MCGuiEvent* event) -> void
{
    MCMultiPlayer* multiPlayer = MPlayer;

    if (event->Type == 1)
    {
        // Selects the clicked session and tells the parent (event 0x1e, data 3).
        const int32_t rowHeight = Fonts[0][FontIndex]->Height() + 4;
        const int32_t clickX = event->X - GlobalX();
        const int32_t clickY = (FirstPixel + event->Y) - GlobalY();

        if (NumSessions <= 0)
        {
            return;
        }

        int32_t row = 0;
        int32_t rowTop = 0;

        for (;;)
        {
            const tagRECT rect = {1, rowTop, Width() - 0xd, rowTop + rowHeight};

            if (PtInRect(&rect, tagPOINT{clickX, clickY}))
            {
                break;
            }

            row++;
            rowTop += rowHeight;

            if (row >= NumSessions)
            {
                return;
            }
        }

        if (row < NumSessions)
        {
            SelectedSession = row;
            SelectedGuid = Sessions[row];
        }

        RebuildLines();
        MCGuiEvent selected;
        selected.Type = 0x1e;
        selected.Data = 3;
        Parent->HandleEvent(&selected);
    }
    else if (event->Type == 0x13)
    {
        // Refresh: the sessions that have players (an empty one is remembered as deleted and never listed again).
        NumSessions = 0;

        if (multiPlayer != nullptr && multiPlayer->SessionManager != nullptr)
        {
            MCFLinkedList<MCFidpSession>* list = multiPlayer->SessionManager->GetSessions();
            MCFLink<MCFidpSession>* link = list->HeadLink;
            MCFidpSession* session = link != nullptr ? link->Data : nullptr;

            // The scan stops at the first deleted session.
            while (session != nullptr && IsSessionDeleted(session) == 0)
            {
                if (multiPlayer->SessionManager->GetPlayers(session)->Count < 1)
                {
                    // Port fix: the list holds 50; the original ran past it.
                    if (NextDeletedSession < 50)
                    {
                        DeletedSessions[NextDeletedSession++] = session->SessionDesc.guidInstance;
                    }
                }
                else if (NumSessions < MAX_GAMES)
                {
                    // Port fix: bounded to the 64 slots (the original wasn't).
                    Sessions[NumSessions++] = session->SessionDesc.guidInstance;
                }

                link = link->Next;

                if (link == nullptr)
                {
                    break;
                }

                session = link->Data;
            }

            // Keep the selection on its session, or tell the parent it went (data 4).
            int32_t i = 0;

            for (; i < NumSessions; i++)
            {
                if (std::memcmp(&Sessions[i], &SelectedGuid, sizeof(_GUID)) == 0)
                {
                    SelectedSession = i;
                    break;
                }
            }

            if (i == NumSessions)
            {
                MCGuiEvent lost;
                lost.Type = 0x1e;
                lost.Data = 4;
                lost.LParam = -1;
                Parent->HandleEvent(&lost);
                SelectedSession = -1;
            }
        }

        RebuildLines();
    }
}

auto MCGameList::GetSelectedGame() -> _GUID*
{
    if (SelectedSession >= NumSessions)
    {
        SelectedSession = -1;
        MCGuiEvent refresh;
        refresh.Clear();
        refresh.Type = 0x13;
        HandleEvent(&refresh);
        return nullptr;
    }

    // Port fix: with no selection the original returned &sessions[-1] (inside this object).
    if (SelectedSession < 0)
    {
        return nullptr;
    }

    return &Sessions[SelectedSession];
}

// lSlider

MCLogSlider::MCLogSlider()
{
    MinValue = 0;
    MaxValue = 0;
    CurrentValue = 0;
    ThumbPort = new MCLogPort;
    char fileName[256];
    std::snprintf(fileName, sizeof(fileName), "%sprefs_02.tga", ArtPath);
    ThumbPort->Init(fileName);
}

MCLogSlider::~MCLogSlider()
{
    MCLogSlider::Destroy();
}

auto MCLogSlider::Init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    const int32_t result = MCLogObject::Init(xPos, yPos, width, height, name, nullptr);
    SetTransparent(-1);
    return result;
}

auto MCLogSlider::Destroy() -> void
{
    FreePort(ThumbPort);
    MCLogObject::Destroy();
}

auto MCLogSlider::Draw() -> void
{
    VfxPaneWipe(Lport()->Frame(), 0xff);
    // The thumb's x is the value's share of the travel (x87).
    const int32_t travel = Width() - ThumbPort->Width();
    const int32_t thumbX = static_cast<int32_t>(static_cast<double>(travel) *
                                                (static_cast<double>(CurrentValue - MinValue) / (MaxValue - MinValue)));
    ThumbPort->CopyTo(Lport()->Frame(), thumbX, 0, 0);
}

auto MCLogSlider::SetCurrentValue(int32_t value) -> void
{
    if (value < MinValue)
    {
        CurrentValue = MinValue;
    }
    else if (value > MaxValue)
    {
        CurrentValue = MaxValue;
    }
    else
    {
        CurrentValue = value;
    }
}

auto MCLogSlider::HandleEvent(MCGuiEvent* event) -> void
{
    // The value under the mouse: the offset (as a float) over the travel, times the range.
    auto valueAt = [this](int32_t mouseX)
    {
        const float offset = static_cast<float>(mouseX - X());
        const int32_t travel = Width() - ThumbPort->Width();
        return static_cast<int32_t>(static_cast<double>(offset) / travel * (MaxValue - MinValue)) + MinValue;
    };

    switch (event->Type)
    {
        case 1:
        {
            Application->Grab(this);

            if (Application->GrabbedObject() != nullptr)
            {
                SetCurrentValue(valueAt(event->X));
            }
            break;
        }
        case 4:
        {
            Application->Release();
            SetCurrentValue(valueAt(event->X));
            break;
        }
        case 7:
        {
            if (Application->GrabbedObject() != nullptr)
            {
                SetCurrentValue(valueAt(event->X));
            }
            break;
        }
        default:
            break;
    }

    if (EventRoutine != nullptr)
    {
        EventRoutine(this, event);
    }
}

// lComboBox (port-only)

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
                         void (*changed)(int32_t value)) -> void
{
    MCLogObject::Init(xPos, yPos, width, FieldHeight, nullptr, nullptr);
    _Setting = setting;
    _Items = std::move(items);
    _Changed = changed;
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
        static uint8_t colors[256];

        for (int32_t i = 0; i < 256; ++i)
        {
            colors[i] = i == 0xff ? 0xff : 0xe3;
        }

        MCRenderer::RegisterData(colors, sizeof(colors), MCDataKind::Tables);
        return colors;
    }();

    return table;
}

auto MCLogComboBox::WriteLabel(int32_t xPos, int32_t yPos, const std::string& text) -> void
{
    VfxStringDraw(_OwnPort->Frame(), xPos, yPos, WhiteFont->FontData.get(), text.c_str(), LabelColors());
}

auto MCLogComboBox::Draw() -> void
{
    if (!_OwnPort->ViewOpen())
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
        WriteLabel(2, 3, _Items[selected].Label);
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

        WriteLabel(2, top + 2, _Items[item].Label);
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
    Application->Grab(this);
    SoundSystem()->PlayDigitalSample(ComboClickSound, 1, nullptr, 0, 0);
}

auto MCLogComboBox::Close() -> void
{
    if (!_Open && !_HoldUntilRelease)
    {
        return;
    }

    CloseList();
    _HoldUntilRelease = false;

    if (Application->GrabbedObject() == this)
    {
        Application->Release();
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

    const int32_t value = _Items[index].Value;

    if (*_Setting == value)
    {
        return;
    }

    *_Setting = value;

    if (_Changed != nullptr)
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

    MCGuiObject** first = Parent->ChildList;
    MCGuiObject** last = first + Parent->NumChildren;
    MCGuiObject** at = std::find(first, last, this);

    if (at == last)
    {
        return;
    }

    // The children are sorted by depth, front-most last: this one goes after the others of its depth.
    MCGuiObject** end = at + 1;

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
        case 1:
        case 3:
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
        case 4:
        {
            const int32_t row = RowAt(event->X, event->Y);

            if (_HoldUntilRelease)
            {
                Close();
            }
            else if (row >= 0)
            {
                Close();
                SoundSystem()->PlayDigitalSample(ComboClickSound, 1, nullptr, 0, 0);
                Choose(row);
            }
            break;
        }
        case 7:
        {
            const int32_t row = RowAt(event->X, event->Y);

            if (row >= 0)
            {
                _Hovered = row;
            }
            break;
        }
        case 9:
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

auto MCLogComboBox::MouseWheel(int32_t steps, int32_t xPos, int32_t yPos) -> bool
{
    (void)xPos;
    (void)yPos;

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
