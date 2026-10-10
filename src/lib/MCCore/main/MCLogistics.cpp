#include "stdafx.h"
#include "main/MCLogistics.h"
#include "gui/MCGuiFont.h"
#include "gui/MCGuiSystem.h"
#include "gui/MCScrollPane.h"
#include "gui/MCUpdateDisplay.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "ai/MCMoveGeometry.h"
#include "logistics/MCBriefingBox.h"
#include "logistics/MCBriefingScreen.h"
#include "logistics/MCCompInventoryBlock.h"
#include "logistics/MCConnectMenu.h"
#include "logistics/MCFileScrollPane.h"
#include "logistics/MCGameList.h"
#include "logistics/MCLoadSaveMenu.h"
#include "logistics/MCLogButton.h"
#include "logistics/MCLogChatWindow.h"
#include "logistics/MCLogMenus.h"
#include "logistics/MCLogScrollTextObject.h"
#include "logistics/MCLogTextObject.h"
#include "logistics/MCMainMenu.h"
#include "logistics/MCMechBriefBlock.h"
#include "logistics/MCPreferencesMenu.h"
#include "logistics/MCLogChatInput.h"
#include "logistics/MCMechPurchaseBlock.h"
#include "logistics/MCPilotPurchaseBlock.h"
#include "logistics/MCPurMechList.h"
#include "logistics/MCVehiclePurchaseBlock.h"
#include "logistics/MCPurPilotList.h"
#include "logistics/MCPurVehicleList.h"
#include "logistics/MCPurchaseDlg.h"
#include "logistics/MCPurchaseScreen.h"
#include "logistics/MCRegistrySettings.h"
#include "logistics/MCRepairScreen.h"
#include "logistics/MCReusableDialog.h"
#include "logistics/MCSessionScreen.h"
#include "logistics/MCSplashScreen.h"
#include "logistics/MCTicker.h"
#include "logistics/MCUnitLimits.h"
#include "linkup/sessionmanager.h"
#include "main/MCLogisticsShared.h"
#include "main/honorb.h"
#include "mission/MCScenario.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectTypeManager.h"
#include "main/MCGamePaths.h"
#include "main/main.h"
#include "mission/MCMission.h"
#include "network/multplyr.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"
#include "platform/MCRegistry.h"
#include "platform/MCRenderer.h"
#include "sound/MCSoundSystem.h"
#include "vfx/MCVfxFunctions.h"

std::type_identity_t<std::array<bool, 7>> LogCheatActive{};
std::array<int32_t, 6> MultiPlayerColors{};
std::type_identity_t<int32_t> LogCurCheatChar{};
std::type_identity_t<int> InDemo{};
bool Solo = false;
MCLogistics* GlobalLogPtr = nullptr;
int32_t LastLogisticsMissionState = 0;
std::string HoldString;

MCLogistics::MCLogistics()
    : WarriorList(std::make_unique<MCLogWarriorList>())
    , AssignedWarriorList(std::make_unique<MCLogWarriorList>())
    , MechList(std::make_unique<MCLogMechList>())
    , VehicleList(std::make_unique<MCLogVehicleList>())
    , ForceMechList(std::make_unique<MCLogMechList>())
    , ForceVehicleList(std::make_unique<MCLogVehicleList>())
    , ComponentInventory(std::make_unique<MCInventoryList>())
    , PurchaseComponents(std::make_unique<MCInventoryList>())
{
}

namespace
{
    /// <summary>Screen element <paramref name="index"/> of <paramref name="screen"/> as a <typeparamref name="T"/>.</summary>
    template <typename T> T* ScreenElement(MCGenericScreen* screen, int32_t index)
    {
        return static_cast<T*>(screen->Elements[static_cast<size_t>(index)]);
    }

    /// <summary>A new <paramref name="width"/> x <paramref name="height"/> port with its own bitmap.</summary>
    std::unique_ptr<MCLogPort> NewPort(int32_t width, int32_t height)
    {
        auto port = std::make_unique<MCLogPort>();
        port->Init(width, height);
        return port;
    }

    /// <summary>A new port loaded from the art file <paramref name="name"/> under <c>ArtPath</c>.</summary>
    std::unique_ptr<MCLogPort> NewArtPort(std::string_view name)
    {
        auto port = std::make_unique<MCLogPort>();
        port->Load(ArtPath + std::string(name));
        return port;
    }

    /// <summary>Loads the whole of an opened shape file into a block registered as shapes (the repair and icon shapes).</summary>
    MCRegisteredBlock ReadShapeFile(MCFile& file, std::string_view sizeError)
    {
        const uint32_t length = file.GetLength();
        MCRegisteredBlock shapes(length, MCDataKind::Shapes);
        const int32_t read = file.Read(shapes.Data(), static_cast<int32_t>(length));
        Assert(static_cast<uint32_t>(read) == length, 0, sizeError);
        file.Close();
        return shapes;
    }

    /// <summary>Opens shape file <paramref name="fileName"/> (asserting it exists) and loads it (<see cref="ReadShapeFile"/>).</summary>
    MCRegisteredBlock LoadShapeFile(MCFile& file, std::string_view fileName, std::string_view openError,
                                    std::string_view sizeError)
    {
        const int32_t result = file.Open(fileName);
        Assert(result == 0, static_cast<uint32_t>(result), openError);
        return ReadShapeFile(file, sizeError);
    }

    /// <summary>Makes <paramref name="screen"/>'s elements from screen ini <paramref name="name"/><c>.fit</c> under <c>ArtPath</c>.</summary>
    void InitScreen(MCSplashScreen& screen, std::string_view name, std::string_view missingError,
                    std::string_view startError)
    {
        MCFitIniFile file;
        int32_t result = file.Open(GamePath(ArtPath, name, ".fit"));
        Assert(result == 0, static_cast<uint32_t>(result), missingError);
        result = screen.Init(file);
        Assert(result == 0, static_cast<uint32_t>(result), startError);
    }

    /// <summary>Sets up a multiplayer screen's text field: the white font and the background.</summary>
    MCLogTextObject* SetUpTextField(MCGenericScreen* screen, int32_t index)
    {
        auto* field = ScreenElement<MCLogTextObject>(screen, index);
        field->Font = MedWhiteFont;
        field->SetBackColor(0x10);
        return field;
    }
}

auto MCLogistics::Start() -> void
{
    if (EmptyFile.empty())
    {
        EmptyFile = LoadGameString(0x381, 0xfe);
    }

    _Started = true;
    GlobalLogPtr = this;
    AutoPlayMovie = false;
    CurrentScreen = nullptr;
    DragIcon = nullptr;
    CurrentInvTab = 0;
    CurrentMission = -1;
    NextWarriorID = 1;
    ResourcePoints = -9999;
    PlayerLights = nullptr;
    HammerDown = false;
    WindowTitle = AppName + " -- Logistics";

    // Port: SetWindowTextA -> the SDL window's title.
    if (MCDisplay* display = MCInput::Display())
    {
        display->SetTitle(WindowTitle.c_str());
    }

    LogisticsState = 0;
    WorkPort0 = NewPort(0x1ab, 0x1ce);
    WorkPort1 = NewPort(0x1ab, 0x1ce);

    for (size_t lance = 0; lance < NumLances; ++lance)
    {
        for (size_t slot = 0; slot < LanceSlots; ++slot)
        {
            DeploySlots[lance][slot] = {-1, -1};
            DeploySlotPlacements[lance][slot] = {};
        }
    }

    LocalDropSlot.fill(false);
    PlayerColors = {1, 3, 4, 2, 6, 5};
    MultiplayerInitialized = false;
    DefaultPlanningTime = 0xf0;
    PlanningTime = 0xf0;
    InventoryIconPorts[0] = NewArtPort("logart\\lsciim.tga");
    InventoryIconPorts[1] = NewArtPort("logart\\lsciip.tga");
    InventoryIconPorts[2] = NewArtPort("logart\\lsciic.tga");
    InventoryIconPorts[3] = NewArtPort("logart\\lsciiv.tga");
    LoadScreen = std::make_unique<MCSplashScreen>();
    SaveScreen = std::make_unique<MCSplashScreen>();

    if (InDemo == 0)
    {
        ChatWindow = MCMakeGui<MCLogChatWindow>();
        ChatWindow->Init(7, 0x44, 0xbf, 0x101, 100000);
        ChatWindow->ShowGuiWindow(0);
        MultiplayerScreen = std::make_unique<MCSplashScreen>();
        SerialScreen = std::make_unique<MCSplashScreen>();
        LanScreen = std::make_unique<MCSplashScreen>();
        ModemScreen = std::make_unique<MCSplashScreen>();
        SessionScreen = std::make_unique<MCSessionScreen>();
        ConnectScreen = std::make_unique<MCSplashScreen>();
        PrefScreen = std::make_unique<MCSplashScreen>();
    }

    MainScreen = std::make_unique<MCSplashScreen>();
    RepairScreen = std::make_unique<MCRepairScreen>();
    RepairScreen->Init();
    BriefingScreen = std::make_unique<MCBriefingScreen>();
    BriefingScreen->Init();
    PurchaseScreen = std::make_unique<MCPurchaseScreen>();
    PurchaseScreen->Init();
    CurrentScreen = MainScreen.get();
    LogisticsState = 1;
    InitScreen(*MainScreen, "mainScreen", " No Splash Screen FIT File ", " Unable to start splash screen ");

    if (InDemo == 0)
    {
        const std::optional<std::string> userName = MyGetUserName();
        InitScreen(*MultiplayerScreen, "mpscreen", " No Connection Screen FIT File ",
                   " Unable to start connect screen ");

        {
            MCSplashScreen* screen = LanScreen.get();
            InitScreen(*screen, "lanscreen", " No LAN Screen FIT File ", " Unable to start lanScreen screen ");
            screen->SetEventRoutine(LanScreenHandleEvent);
            auto* players = ScreenElement<MCLogScrollTextObject>(screen, 3);
            players->SetEventRoutine(PlayerListHandleEvent);
            players->FontIndex = 1;
            ScreenElement<MCGameList>(screen, 2)->FontIndex = 1;
            MCLogTextObject* nameField = SetUpTextField(screen, 4);
            MCLogTextObject* gameField = SetUpTextField(screen, 10);
            MCLogTextObject* playersField = SetUpTextField(screen, 11);
            nameField->InitBuffer(0x10, MCLogInputType::Text);
            gameField->InitBuffer(0x18, MCLogInputType::Text);
            nameField->SetStringBuffer(userName.value_or("Player"));

            // The game's name: "<player>'s game" from the string table, or "Game" without a user name.
            if (userName.has_value())
            {
                gameField->InitBuffer(0x18, MCLogInputType::Text);
                gameField->SetStringBuffer(MCFormatPrintf(LoadGameString(0x377, 0xfe).c_str(), userName->c_str()));
            }
            else
            {
                gameField->SetStringBuffer("Game");
            }

            playersField->InitBuffer(2, MCLogInputType::Port);
            playersField->SetStringBuffer("6");
            ScreenElement<MCLogButton>(screen, 6)->Disabled = 1;
            screen->ShowBlock(0);
        }

        {
            MCSplashScreen* screen = ModemScreen.get();
            InitScreen(*screen, "modem", " No modem Screen FIT File ", " Unable to start modemScreen screen ");
            screen->SetEventRoutine(ModemScreenHandleEvent);
            MCLogTextObject* nameField = SetUpTextField(screen, 4);
            MCLogTextObject* phoneField = SetUpTextField(screen, 5);
            nameField->InitBuffer(0x10, MCLogInputType::Text);
            phoneField->InitBuffer(0x18, MCLogInputType::Text);
            nameField->SetStringBuffer(userName.value_or("Player"));
            auto* modems = ScreenElement<MCLogScrollTextObject>(screen, 10);
            modems->FontIndex = 1;
            modems->HighlightColor[0] = 0x14;
            modems->HighlightLine[0] = 0;
            modems->SetEventRoutine(ModemListHandleEvent);
            screen->ShowBlock(0);
        }

        {
            // The serial screen reuses the modem screen's messages.
            MCSplashScreen* screen = SerialScreen.get();
            InitScreen(*screen, "serial", " No modem Screen FIT File ", " Unable to start modemScreen screen ");
            MCLogTextObject* nameField = SetUpTextField(screen, 4);
            nameField->InitBuffer(0x10, MCLogInputType::Text);
            nameField->SetStringBuffer(userName.value_or("Player"));
            MCLogTextObject* portField = SetUpTextField(screen, 5);
            portField->InitBuffer(2, MCLogInputType::Digits);
            portField->SetEventRoutine(ComPortTextHandleEvent);
            portField->SetStringBuffer("1");
            screen->SetEventRoutine(SerialScreenHandleEvent);
        }

        {
            MCSplashScreen* screen = ConnectScreen.get();
            InitScreen(*screen, "readyroom", " No Ready Room Screen FIT File ",
                       " Unable to start readyRoomScreen screen ");
            auto* players = ScreenElement<MCLogScrollTextObject>(screen, 3);
            players->SetEventRoutine(ReadyRoomPlayerListHandleEvent);
            players->FontIndex = 1;
            ScreenElement<MCLogButton>(screen, 2)->Disabled = 1;
        }
    }

    InitScreen(*LoadScreen, "loadScreen", " No Load Screen FIT File ", " Unable to start load screen ");
    LoadScreen->SetEventRoutine(LoadSaveScreenHandleEvent);
    InitScreen(*SaveScreen, "saveScreen", " No Save Screen FIT File ", " Unable to start save screen ");
    SaveScreen->SetEventRoutine(LoadSaveScreenHandleEvent);

    if (InDemo == 0)
    {
        // The preferences screen reuses the save screen's messages.
        InitScreen(*PrefScreen, "prefScreen", " No Save Screen FIT File ", " Unable to start save screen ");
        PrefScreen->SetEventRoutine(PrefScreenHandleEvent);
        // Port: the difficulty and renderer choices as drop-downs.
        AddPreferenceDropDowns(PrefScreen.get());
        SessionScreen->Init(0, 0, 0x280, 0x1e0, nullptr);
    }

    ShowLogScreen(false, false);

    // Under the process ID, as aSystem::init sets it: copies of the game on one machine share the user folder.
    *std::format_to_n(SaveTempPath, sizeof(SaveTempPath) - 1, "{}temp\\{}\\", SavePath, MCPort::ProcessId()).out = 0;
    // Port fix (OB-094): the original allocated a File here, and a FitIniFile after the sort tables, and never used or
    // freed either.

    char line[256];
    MCFile file;
    int32_t result = file.Open(ArtPath + "logart\\comp.rsp");
    Assert(result == 0, 0, " could not open componant name file ");
    // The table has one entry more than the file has lines; every entry is read from a line (the last from the end
    // of the file).
    int32_t numRangeSorted = 1;

    while (true)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0x28);

        if (file.Eof())
        {
            break;
        }

        ++numRangeSorted;
    }

    RangeSortList.assign(static_cast<size_t>(numRangeSorted), 0);
    file.Seek(0, 0);

    for (uint32_t& entry : RangeSortList)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0x28);
        entry = static_cast<uint32_t>(std::atol(line));
    }

    file.Close();
    result = file.Open(GamePath(ObjectPath, "objsort.rsp"));
    Assert(result == 0, 0, " could not open object sort file ");

    for (int32_t& sortOrder : ComponentSort)
    {
        file.ReadLine(reinterpret_cast<uint8_t*>(line), 0xfe);
        sortOrder = static_cast<int32_t>(std::atol(line));
    }

    file.Close();
    InvBlockPort = NewArtPort("logart\\invblock.tga");

    for (std::unique_ptr<MCLogPort>& port : InvTabPorts)
    {
        port.reset();
    }

    Ticker = std::make_unique<MCTicker>();
    Ticker->Init(3, 3, 0xcd, 1);
    Ticker->SetScreen(CurrentScreen);
    CurrentScreen->AddChild(Ticker.get());
    Ticker->SetFont(MedWhiteFont);
    Ticker->BringToFront(0);
    Ticker->ShowGuiWindow(1);
    {
        const std::unique_ptr<MCLogPort> tickerBack = NewPort(0xcd, MedWhiteFont->Height());
        VfxPaneWipe(tickerBack->Frame(), 0xed);
        Ticker->SetBackPane(tickerBack.get());
    }

    PurchaseDialog = std::make_unique<MCPurchaseDlg>();
    PurchaseDialog->MCLogDialogBox::Init(0xe5, 0xa2, 0xb5, 0x9c);
    ScreenWindow()->AddChild(PurchaseDialog.get());
    MessageDialog = MCMakeGui<MCReusableDialog>();
    MessageDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow()->AddChild(MessageDialog.get());
    QuestionDialog = MCMakeGui<MCReusableDialog>();
    QuestionDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow()->AddChild(QuestionDialog.get());
    RefitDialog = MCMakeGui<MCRefitDialog>();
    RefitDialog->Init(0, 0, 4, 4, nullptr);
    ScreenWindow()->AddChild(RefitDialog.get());

    ResourceBackPort = NewPort(0x3d, 0xc);
    VfxPaneWipe(ResourceBackPort->Frame(), 0x10);
    ClockBackPort = NewPort(0x32, 0xc);
    VfxPaneWipe(ClockBackPort->Frame(), 0x10);
    RepairBackPort = NewArtPort("logart\\lsrupm00.tga");

    for (size_t i = 0; i < MechRepShapes.size(); ++i)
    {
        MechRepShapes[i] = LoadShapeFile(file, std::format("{}mechrep{:02}.shp", ArtPath, i),
                                         "could not open mechrep shape file", "unexpected mechrep size");
        MechIconShapes[i] = LoadShapeFile(file, std::format("{}mi{:02}.shp", ArtPath, i),
                                          "could not open mechicon shape file", "unexpected mechicon size");
    }

    // A vehicle without a repair picture has no icon either.
    for (size_t i = 0; i < VehicleRepShapes.size(); ++i)
    {
        if (file.Open(std::format("{}vr1{:02}.shp", ArtPath, i)) != 0)
        {
            continue;
        }

        VehicleRepShapes[i] = ReadShapeFile(file, "unexpected vhclrep size");

        if (file.Open(std::format("{}vi1{:02}.shp", ArtPath, i)) == 0)
        {
            VehicleIconShapes[i] = ReadShapeFile(file, "unexpected vhclrep size");
        }
    }

    // Ten remap tables: each maps every colour to 0xff (transparent) except one, which it recolours.
    static constexpr std::array<std::pair<uint8_t, uint8_t>, 10> lookasideColors = {{{0xe8, 0xe8},
                                                                                     {0xe8, 0xf2},
                                                                                     {0xe8, 0xeb},
                                                                                     {0xe8, 0xef},
                                                                                     {0xe8, 0x13},
                                                                                     {0xe6, 0xe6},
                                                                                     {0xe6, 0xf1},
                                                                                     {0xe6, 0xf4},
                                                                                     {0xe6, 0xed},
                                                                                     {0xe6, 0x13}}};

    for (size_t i = 0; i < ShapeLookaside.size(); ++i)
    {
        ShapeLookaside[i].fill(0xff);
        ShapeLookaside[i][lookasideColors[i].first] = lookasideColors[i].second;
    }

    MCRenderer::RegisterData(ShapeLookaside.data(), sizeof(ShapeLookaside), MCDataKind::Tables);

    RepairPorts[0] = NewArtPort("logart\\lsrupm03.tga");
    RepairPorts[1] = NewArtPort("logart\\lsrupm01.tga");
    RepairPorts[2] = NewArtPort("logart\\lsrupm04.tga");
    RepairPorts[3] = NewArtPort("logart\\lsrupm02.tga");
    RepairPorts[4] = NewArtPort("logart\\lsrupm06.tga");
    RepairPorts[5] = NewArtPort("logart\\lsrupm07.tga");
    PurchasePorts[0] = NewArtPort("logart\\lspcb05.tga");
    PurchasePorts[1] = NewArtPort("logart\\lspcb07.tga");
    PurchasePorts[2] = NewArtPort("logart\\lspcb06.tga");
    PurchasePorts[3] = NewArtPort("logart\\lspcb09.tga");
    static constexpr std::array<std::string_view, 5> buttonNames = {
        "logart\\lscb{}00.tga", "b{}_exit.tga", "logart\\lscb{}01.tga", "logart\\lscb{}02.tga", "logart\\lscb{}03.tga"};
    static constexpr std::array<char, 3> buttonStates = {'n', 'h', 'g'};

    for (size_t button = 0; button < ScreenButtonPorts.size(); ++button)
    {
        for (size_t state = 0; state < buttonStates.size(); ++state)
        {
            ScreenButtonPorts[button][state] =
                NewArtPort(std::vformat(buttonNames[button], std::make_format_args(buttonStates[state])));
        }
    }

    {
        MCFitIniFile gameSystemFile;
        result = gameSystemFile.Open(GamePath(MissionPath, "gamesys.fit"));
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't open gamesys.fit");
        result = gameSystemFile.SeekBlock("Warrior");
        Assert(result == 0, static_cast<uint32_t>(result), " Couldn't find Warrior block ");
        MaxPilotSkill = ReadRequired<float>(gameSystemFile, "SkillMax",
                                            " Couldn't find SkillMax variable in Warrior block of gamesys.fit ");
        MinPilotSkill = ReadRequired<float>(gameSystemFile, "SkillMin",
                                            " Couldn't find SkillMin variable in Warrior block of gamesys.fit ");
        static_cast<void>(gameSystemFile.ReadArray("SkillWeightings", std::span<float>(SkillWeightings)));
        Assert(gameSystemFile.ReadArray("WarriorRankScale", std::span<float>(WarriorRankScale)).has_value(), 0,
               " Couldn't find WarriorRankScale variable in Warrior block of gamesys.fit ");
        result = gameSystemFile.SeekBlock("MultiPlayerColors");
        Assert(result == 0, static_cast<uint32_t>(result), " Could not find Multiplayer Colors block ");
        Assert(gameSystemFile.ReadArray("mPlayerColors", std::span<int32_t>(MultiPlayerColors)).has_value(), 0,
               " Could not find multiplayer colors data ");
    }

    ReadyRoomTicks = 0;

    if (LaunchedFromLobby == 0 || MPlayer == nullptr || Turn != 0)
    {
        LogisticsState = 1;
        CurrentScreen = MainScreen.get();
    }
    else
    {
        // Started from a lobby: straight to the ready room, whose cancel button quits the game.
        CurrentScreen = ConnectScreen.get();
        LogisticsState = 0xe;
        ScreenElement<MCLogButton>(ConnectScreen.get(), 1)->Callback()->SetExec(KillTheGame);
    }

    ShowLogScreen(false, false);
}

MCLogistics::~MCLogistics()
{
    if (!_Started)
    {
        return;
    }

    MCRenderer::UnregisterData(ShapeLookaside.data(), sizeof(ShapeLookaside));
    PlayerLights.reset();
    Ticker.reset();
    ChatWindow.reset();

    if (MultiplayerInitialized)
    {
        DestroyMultiplayer();
    }

    CampaignBriefingName.clear();
    OperationCinema.clear();
    RepairBackPort.reset();

    for (std::unique_ptr<MCLogPort>& port : RepairPorts)
    {
        port.reset();
    }

    for (std::unique_ptr<MCLogPort>& port : PurchasePorts)
    {
        port.reset();
    }

    WorkPort0.reset();
    WorkPort1.reset();

    for (auto& ports : ScreenButtonPorts)
    {
        for (std::unique_ptr<MCLogPort>& port : ports)
        {
            port.reset();
        }
    }

    for (std::unique_ptr<MCLogPort>& port : InventoryIconPorts)
    {
        port.reset();
    }

    for (size_t i = 0; i < MechRepShapes.size(); ++i)
    {
        MechRepShapes[i] = {};
        MechIconShapes[i] = {};
    }

    for (size_t i = 0; i < VehicleRepShapes.size(); ++i)
    {
        VehicleRepShapes[i] = {};
        VehicleIconShapes[i] = {};
    }

    ResourceBackPort.reset();
    ClockBackPort.reset();

    if (PurchaseDialog != nullptr)
    {
        ScreenWindow()->RemoveChild(PurchaseDialog.get());
        PurchaseDialog.reset();
    }

    MessageDialog.reset();
    QuestionDialog.reset();
    RefitDialog.reset();
    InvBlockPort.reset();
    CurrentScreen = nullptr;
    VehicleList.reset();
    // The force lists last until the object goes (the original left them for the mission in multiplayer).
    ForceVehicleList->Clear();
    PurMechList.reset();
    PurVehicleList.reset();
    PurchaseComponents.reset();
    PurPilotList.reset();
    ComponentInventory.reset();
    RangeSortList.clear();
    WarriorList.reset();
    MechList.reset();
    AssignedWarriorList.reset();
    ForceMechList->Clear();
    InvTabPorts[2].reset();
    InvTabPorts[0].reset();
    InvTabPorts[1].reset();
    InvTabPorts[3].reset();
    BriefingScreen.reset();
    PurchaseScreen.reset();
    RepairScreen.reset();
    MainScreen.reset();
    MultiplayerScreen.reset();
    LanScreen.reset();
    ModemScreen.reset();
    SerialScreen.reset();
    ConnectScreen.reset();
    SessionScreen.reset();
    PrefScreen.reset();
    LoadScreen.reset();
    SaveScreen.reset();
    GuiSystem()->SetCurrentObject(nullptr);
    ClearLogArt();
    EmptyFile.clear();
}

auto MCLogistics::ShowLogScreen(bool show, bool redraw) -> void
{
    if (redraw)
    {
        GuiSystem()->ActivatePaletteFromTga(ArtPath + "logart\\lsrupm05.tga");
    }

    CurrentScreen->ShowGuiWindow(show ? 1 : 0);
}

auto MCLogistics::SetUpMainScreen(bool fromMenu) -> int32_t
{
    if (CurrentScreen != nullptr)
    {
        ShowLogScreen(false, false);
    }

    CurrentScreen = MainScreen.get();

    if (!fromMenu)
    {
        PreviousState = LogisticsState;
    }

    LogisticsState = 1;
    ShowLogScreen(true, true);

    if (MPlayer != nullptr)
    {
        if (MultiplayerInitialized)
        {
            DestroyMultiplayer();
        }

        delete MPlayer;
        MPlayer = nullptr;
        MCBriefingScreen* briefing = BriefingScreen.get();
        briefing->ChatBlinking = 0;

        if (briefing->ChatTimerOn != 0)
        {
            GuiSystem()->RemoveTimer(briefing, 5);
        }

        if (PurchaseScreen->ChatBlinking != 0)
        {
            GuiSystem()->RemoveTimer(PurchaseScreen.get(), 7);
        }

        if (RepairScreen->ChatBlinking != 0)
        {
            GuiSystem()->RemoveTimer(RepairScreen.get(), 8);
        }

        briefing->BriefingBox = nullptr;

        if (PurchaseDialog != nullptr)
        {
            PurchaseDialog->ShowGuiWindow(0);
        }
    }

    if (ChatWindow != nullptr)
    {
        ChatWindow->Reset();
    }

    return 0;
}

namespace
{
    /// <summary>Moves the name ticker onto <paramref name="screen"/>, at its top left.</summary>
    void MoveTicker(MCTicker* ticker, MCLogObject* screen)
    {
        if (ticker->Parent != nullptr)
        {
            ticker->Parent->RemoveChild(ticker);
        }

        screen->AddChild(ticker);
        ticker->SetScreen(screen);
        ticker->SetPos(3, 3);
    }

    /// <summary>Moves the multiplayer ready lights onto <paramref name="screen"/>, in front.</summary>
    void MoveLights(MCMPPlayerLights* lights, MCLogObject* screen)
    {
        lights->Parent->RemoveChild(lights);
        screen->AddChild(lights);
        lights->SetDepth(100);
    }

    /// <summary>
    /// Draws what <paramref name="pane"/> shows into <paramref name="dest"/> for a screen change: its background
    /// copy at (<paramref name="backX"/>, 1), its scrolled contents (through <paramref name="scratch"/>) at (0, 1)
    /// and its slider at the right edge.
    /// </summary>
    void DrawPaneForTransition(MCScrollPane* pane, MCLogPort* scratch, MCLogPort* dest, int32_t backX)
    {
        if (pane->BackgroundCopy != nullptr)
        {
            pane->BackgroundCopy->CopyTo(dest->Frame(), backX, 1, 1);
        }

        VfxPaneWipe(scratch->Frame(), 0xff);
        pane->DrawContentTo(scratch->Frame(), 0, 0);
        scratch->CopyTo(dest->Frame(), 0, 1, 1);
        pane->DrawSliderColumn(dest->Frame(), dest->Width() - 0xe, 1, true);
    }

    /// <summary>A scratch port the size of a screen's unit pane (every screen's pane is that size).</summary>
    std::unique_ptr<MCLogPort> NewPaneScratch(MCScrollPane* pane)
    {
        return NewPort(pane->Width(), pane->Height());
    }
}

auto MCLogistics::SetUpPurchaseScreen(bool animate) -> int32_t
{
    MCBriefingScreen* briefing = BriefingScreen.get();
    briefing->StopSmackerMovies();
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));

    if (MPlayer != nullptr)
    {
        if (briefing->ChatBlinking == 0)
        {
            if (PurchaseScreen->ChatBlinking != 0)
            {
                GuiSystem()->RemoveTimer(PurchaseScreen.get(), 7);
                // Original behaviour (OB-096): clears the repair screen's flag instead of the purchase screen's, so
                // the purchase screen's chat button does not blink again until the flag is cleared elsewhere.
                RepairScreen->ChatBlinking = 0;
            }
        }
        else if (PurchaseScreen->ChatBlinking == 0)
        {
            GuiSystem()->AddTimer(PurchaseScreen.get(), 7, 0xfa, 0, 0, 0);
            PurchaseScreen->ChatBlinking = 1;
        }
    }

    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(false, false);
    }

    MCPurchaseScreen* screen = PurchaseScreen.get();
    MoveTicker(Ticker.get(), screen);
    CurrentScreen = screen;
    LogisticsState = 2;
    screen->DrawBackground();
    DrawScreenButtons();

    switch (CurrentInvTab)
    {
        case 0:
        {
            screen->SetUpMechInv(1, 1);
            screen->SetUpMechPurchase();
            break;
        }
        case 1:
        {
            screen->SetUpPilotInv(1, 1);
            screen->SetUpPilotPurchase();
            break;
        }
        case 2:
        {
            screen->SetUpCompInv(1, 1);
            screen->SetUpCompPurchase();
            break;
        }
        case 3:
        {
            screen->SetUpVhclInv(1, 1);
            screen->SetUpVehiclePurchase();
            break;
        }
    }

    if (MPlayer != nullptr)
    {
        MoveLights(PlayerLights.get(), screen);
    }

    ShowLogScreen(true, previous != RepairScreen.get() && previous != BriefingScreen.get());

    if (animate)
    {
        VfxPaneWipe(WorkPort0->Frame(), 0x10);
        VfxPaneWipe(WorkPort1->Frame(), 0x10);
        MCScrollPane* pane = screen->UnitPane;
        const std::unique_ptr<MCLogPort> scratch = NewPaneScratch(pane);
        DrawPaneForTransition(pane, scratch.get(), WorkPort0.get(), 0);
        int direction;

        if (previous == RepairScreen.get())
        {
            DrawPaneForTransition(RepairScreen->UnitPane, scratch.get(), WorkPort1.get(), 0);
            direction = 1;
        }
        else
        {
            const std::unique_ptr<MCLogPort> look = BriefingScreen->NewLookPicture();
            VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort1->Frame(), 0, 0, -1);
            direction = 0;
        }

        pane->ShowGuiWindow(0);
        Transition(WorkPort1.get(), WorkPort0.get(), direction);
        pane->ShowGuiWindow(1);
    }

    return 0;
}

auto MCLogistics::DrawScreenButtons() -> void
{
    MCLogObject* screen = CurrentScreen;

    if (screen != BriefingScreen.get() && screen != PurchaseScreen.get() && screen != RepairScreen.get())
    {
        return;
    }

    // (The original also made and freed an unused lPort here.)
    screen->Chrome()->HoveredButton = -1;
}

auto MCLogistics::HoverScreenButton(MCLogObject* screen, int32_t button) -> void
{
    if (MCLogScreenChrome* chrome = screen->Chrome(); chrome != nullptr)
    {
        chrome->HoveredButton = button;
    }
}

auto MCLogistics::DrawScreenChrome(MCLogObject* screen, MCPane* target) -> void
{
    const MCLogScreenChrome* chrome = screen->Chrome();

    // The ready lights' backing, under the screen's lights (the original's lights painted it into their parent).
    if (PlayerLights != nullptr && PlayerLights->Parent == screen)
    {
        if (MCLogPort* back = LogScreenArt("lsc_p0.tga"))
        {
            back->CopyTo(target, 0xd3, 0, 0);
        }
    }

    if (screen == BriefingScreen.get() || screen == PurchaseScreen.get() || screen == RepairScreen.get())
    {
        // Button 0 is the main menu in single player, exit in multiplayer; the screen's own button is grayed, the one
        // under the mouse lit, and the briefing button blinks while the chat is unread.
        const std::array<const std::array<std::unique_ptr<MCLogPort>, 3>*, 4> ports = {
            MPlayer == nullptr ? &ScreenButtonPorts[0] : &ScreenButtonPorts[1], &ScreenButtonPorts[2],
            &ScreenButtonPorts[3], &ScreenButtonPorts[4]};
        const int32_t own = screen == BriefingScreen.get() ? 1 : (screen == PurchaseScreen.get() ? 2 : 3);

        for (int32_t button = 0; button < 4; button++)
        {
            const int32_t top = 0x10 + button * 0x12;
            const std::array<std::unique_ptr<MCLogPort>, 3>& faces = *ports[static_cast<size_t>(button)];

            if (MCLogPort* face = faces[button == own ? 2 : 0].get(); face != nullptr)
            {
                face->CopyTo(target, 2, top, 0);
            }

            const bool blinking = button == 1 && chrome->BlinkLit && BriefingScreen->ChatBlinking != 0;

            if (button != own && (chrome->HoveredButton == button || blinking))
            {
                if (MCLogPort* lit = faces[1].get(); lit != nullptr)
                {
                    lit->CopyTo(target, 2, top, true);
                }
            }
        }
    }

    if (Ticker != nullptr && Ticker->PaintScreen == screen)
    {
        Ticker->DrawLine(target);
    }

    // The resource points (not on the session screen) and the clock.
    if (screen != SessionScreen.get())
    {
        const std::string text = ResourceFigureText();
        VfxPaneCopy(ResourceBackPort->Frame(), 0, 0, target, 0x209, 2, -1);
        MedWhiteFont->WriteString(target, 0x244 - MedWhiteFont->Width(text), 4, text);
    }

    char time[0xc];
    MCPort::StrTime(time);
    VfxPaneCopy(ClockBackPort->Frame(), 0, 0, target, 0x24c, 2, -1);
    MedWhiteFont->WriteString(target, 0x254, 4, time, -1);
}

auto MCLogistics::SetUpBriefingScreen(bool animate) -> int32_t
{
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(false, false);
    }

    MCBriefingScreen* briefing = BriefingScreen.get();
    MoveTicker(Ticker.get(), briefing);
    briefing->SetUpDeploy();

    // The original blanked the local player's empty drop slots here (the screen draws them each frame), then the box
    // area under them.
    briefing->BlankBox();

    CurrentScreen = briefing;
    LogisticsState = 3;
    ShowLogScreen(true, previous != RepairScreen.get() && previous != PurchaseScreen.get());
    briefing->MovieStarted = 0;
    briefing->SetUpMission();
    briefing->MissionPane->SetScrollPos(0.0f);
    briefing->DeployPane->SetScrollPos(0.0f);
    briefing->CalcTonnages();
    DrawScreenButtons();

    if (MPlayer == nullptr)
    {
        ChatWindow->ShowGuiWindow(0);
    }
    else
    {
        MCLogChatWindow* chat = ChatWindow.get();

        if (chat->Parent != briefing)
        {
            chat->Resize(0xe7);

            if (chat->Parent != nullptr)
            {
                chat->Parent->RemoveChild(chat);
            }

            briefing->AddChild(chat);
            chat->MoveTo(2, 0x65, 0);
        }

        MoveLights(PlayerLights.get(), briefing);

        if (briefing->ChatBlinking != 0 && briefing->ChatTimerOn == 0)
        {
            GuiSystem()->AddTimer(briefing, 5, 500, 0, 0, 0);
            briefing->ChatTimerOn = 1;
        }

        briefing->SetUpOperation();
    }

    // Show the briefing box of the first unit in the deploy pane.
    if (briefing->BriefingBox != nullptr)
    {
        briefing->RemoveChild(briefing->BriefingBox);
        briefing->BriefingBox = nullptr;
    }

    MCScrollPane* deployPane = briefing->DeployPane.get();

    if (deployPane->NumberOfChildren() != 0)
    {
        auto* block = static_cast<MCMechBriefBlock*>(deployPane->Child(0));
        MCBriefingBox* box =
            block->Mech != nullptr ? block->Mech->BriefingBox.get() : block->Vehicle->BriefingBox.get();
        briefing->AddChild(box);
        briefing->BriefingBox = box;
        box->DrawBackground();
    }

    if (animate)
    {
        const std::unique_ptr<MCLogPort> look = briefing->NewLookPicture();
        VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort0->Frame(), 0, 0, -1);
        MCLogPort* from = WorkPort1.get();
        VfxPaneWipe(from->Frame(), 0x10);
        const std::unique_ptr<MCLogPort> scratch = NewPaneScratch(PurchaseScreen->UnitPane);
        VfxPaneWipe(scratch->Frame(), 0xff);
        MCScrollPane* pane = previous == RepairScreen.get() ? RepairScreen->UnitPane : PurchaseScreen->UnitPane;
        DrawPaneForTransition(pane, scratch.get(), from, 1);
        Transition(from, WorkPort0.get(), 1);
    }

    return 0;
}

auto MCLogistics::SetUpSessionScreen() -> int32_t
{
    if (MultiplayerInitialized)
    {
        DestroyMultiplayer();
    }

    CurrentScreen->ShowGuiWindow(0);
    CurrentScreen = SessionScreen.get();
    ShowLogScreen(true, true);
    LogisticsState = 8;
    SessionScreen->Activate(0);
    return 0;
}

auto MCLogistics::SetUpRepairScreen(bool animate) -> int32_t
{
    MCBriefingScreen* briefing = BriefingScreen.get();
    briefing->StopSmackerMovies();
    GuiSystem()->SetCurrentCursor(static_cast<MCCursorType>(0));
    MCRepairScreen* repair = RepairScreen.get();

    if (MPlayer != nullptr)
    {
        if (briefing->ChatBlinking == 0)
        {
            if (repair->ChatBlinking != 0)
            {
                GuiSystem()->RemoveTimer(repair, 8);
                repair->ChatBlinking = 0;
            }
        }
        else if (repair->ChatBlinking == 0)
        {
            GuiSystem()->AddTimer(repair, 8, 0xfa, 0, 0, 0);
            repair->ChatBlinking = 1;
        }
    }

    MCLogObject* previous = CurrentScreen;

    if (previous != nullptr)
    {
        ShowLogScreen(false, false);
    }

    MoveTicker(Ticker.get(), repair);
    CurrentScreen = repair;
    LogisticsState = 4;
    repair->DrawBackground();
    DrawScreenButtons();

    switch (CurrentInvTab)
    {
        case 0:
            repair->SetUpMechInv(1, 1);
            break;
        case 1:
            repair->SetUpPilotInv(1, 1);
            break;
        case 2:
            repair->SetUpCompInv(1, 1);
            break;
        case 3:
            repair->SetUpVhclInv(1, 1);
            break;
    }

    // Nothing selected yet: the force's first mech (a null one when the force has none).
    if (repair->SelectedMech == nullptr && repair->SelectedVehicle == nullptr)
    {
        repair->SelectMech(ForceMechList->Mechs.empty() ? nullptr : ForceMechList->Mechs.front().get());
    }

    if (MPlayer != nullptr)
    {
        MoveLights(PlayerLights.get(), repair);
    }

    ShowLogScreen(true, previous != PurchaseScreen.get() && previous != BriefingScreen.get());

    if (animate)
    {
        VfxPaneWipe(WorkPort0->Frame(), 0x10);
        VfxPaneWipe(WorkPort1->Frame(), 0x10);
        const std::unique_ptr<MCLogPort> scratch = NewPaneScratch(PurchaseScreen->UnitPane);
        VfxPaneWipe(scratch->Frame(), 0xff);
        DrawPaneForTransition(repair->UnitPane, scratch.get(), WorkPort0.get(), 1);

        if (previous == PurchaseScreen.get())
        {
            DrawPaneForTransition(PurchaseScreen->UnitPane, scratch.get(), WorkPort1.get(), 0);
        }
        else
        {
            const std::unique_ptr<MCLogPort> look = BriefingScreen->NewLookPicture();
            VfxPaneCopy(look->Frame(), 0xd3, 0x10, WorkPort1->Frame(), 0, 0, -1);
        }

        repair->UnitPane->ShowGuiWindow(0);
        Transition(WorkPort1.get(), WorkPort0.get(), 0);
        repair->UnitPane->ShowGuiWindow(1);
    }

    return 0;
}

namespace
{
    /// <summary>
    /// Port: the pane a screen change slides over the screen's right part (<see cref="MCLogistics::Transition"/>). The
    /// original wrote both pictures into its own picture each frame; it draws them from the slide's state instead.
    /// </summary>
    class MCTransitionWipe : public MCLogObject
    {
    public:
        /// <summary>Draws the two pictures as the slide stands (the original's loop body).</summary>
        void Draw() override
        {
            if (!Lport()->ViewOpen())
            {
                return;
            }

            MCPane* target = Lport()->Frame();

            if (Direction == 0)
            {
                From->CopyTo(target, 0, 0, 1);
                VfxPaneCopy(To->Frame(), 0x1ab - Offset, 0, target, 0, 0, -1);
            }
            else
            {
                To->CopyTo(target, 0, 0, 1);
                VfxPaneCopy(From->Frame(), Offset, 0, target, 0, 0, -1);
            }
        }

        /// <summary>The wipe draws itself each frame (its port is a view).</summary>
        bool DrawsLive() override { return true; }

        /// <summary>The screen shown before the change, and the one after.</summary>
        MCLogPort* From = nullptr;
        MCLogPort* To = nullptr;
        /// <summary>0: the new picture slides in from the right; otherwise the old one slides out to the left.</summary>
        int Direction = 0;
        /// <summary>How far the slide has gone, in pixels.</summary>
        int32_t Offset = 0;
    };
}

auto MCLogistics::Transition(MCLogPort* from, MCLogPort* to, int direction) -> void
{
    // A pane over the screen's right part, redrawn each frame for a quarter of a second: direction 0 slides the new
    // picture in from the right over the old one, any other slides the old one out to the left off the new one.
    const MCGuiOwned<MCTransitionWipe> wipe = MCMakeGui<MCTransitionWipe>();
    wipe->Init(0xd3, 0x10, from->Width(), from->Height());
    wipe->From = from;
    wipe->To = to;
    wipe->Direction = direction;
    CurrentScreen->AddChild(wipe.get());
    wipe->ShowGuiWindow(1);
    wipe->SetDepth(100);
    SoundSystem()->PlayDigitalSample(0x36, 1, nullptr, 0, 0);
    const int64_t frequency = MCPort::PerformanceFrequency();
    float elapsed = 0.0f;

    do
    {
        const int64_t start = MCPort::PerformanceCounter();
        wipe->Offset = static_cast<int32_t>(static_cast<double>(elapsed) * 4.0 * 427.0);
        UpdateDisplay(0, 0, 0, 0, 0);
        const int64_t end = MCPort::PerformanceCounter();
        // The original divided the low 32 bits of the tick difference by the low 32 bits of the frequency.
        const auto ticks = static_cast<uint32_t>(end - start);
        elapsed = static_cast<float>(static_cast<double>(ticks) / static_cast<int32_t>(frequency) + elapsed);
    } while (elapsed < 0.25);
}

auto MCLogistics::Darken(int32_t amount, char* fadeTable, MCLogPort* port) -> void
{
    // Darkens row block amount (of the port's height) through the fade table; with no port, the repair screen's
    // unit pane (0x19d x 0x70 blocks).
    int32_t width;
    int32_t height;

    if (port == nullptr)
    {
        port = RepairScreen->UnitPane->ContentPort;
        width = 0x19d;
        height = 0x70;
    }
    else
    {
        height = port->Height();
        width = port->Width();
    }

    DarkenRect(port, 0, height * amount, width, height, fadeTable);
}

void MCLogistics::DarkenRect(MCLogPort* port, int32_t xPos, int32_t yPos, int32_t width, int32_t height,
                             char* fadeTable)
{
    std::array<MCScreenVertex, 4> corners{};
    corners[0].X = xPos;
    corners[0].Y = yPos;
    corners[1].X = xPos + width - 1;
    corners[1].Y = yPos;
    corners[2].X = xPos + width - 1;
    corners[2].Y = yPos + height - 1;
    corners[3].X = xPos;
    corners[3].Y = yPos + height - 1;
    VfxTranslatePolygon(port->Frame(), corners, reinterpret_cast<const uint8_t*>(fadeTable));
}

auto MCLogistics::ReIndexInventory() -> int32_t
{
    // Give each component with copies the next inventory row, in the order of the widgets' inventory indexes;
    // components with none get -1.
    const int32_t numItems = ComponentInventory->NumItems();
    int32_t row = 0;

    for (int32_t index = 0; index < numItems; index++)
    {
        const auto found =
            std::ranges::find_if(ComponentInventory->Items, [&](const std::unique_ptr<MCLogInventoryItem>& item)
                                 { return item->InventoryBlock->InventoryIndex == index; });
        Assert(found != ComponentInventory->Items.end(), 0,
               "Could not reindex player inventory. Probably an old savegame");

        if (found == ComponentInventory->Items.end())
        {
            continue;
        }

        MCLogInventoryItem& item = **found;
        item.InventoryBlock->ListIndex = item.Count == 0 ? -1 : row++;
    }

    return row;
}

auto MyGetUserName() -> std::optional<std::string>
{
    // The name buffers on the multiplayer screens hold 63 characters.
    constexpr uint32_t bufferSize = 0x3f;

    // The name last entered on the multiplayer screens (HKLM\Software\FASA Interactive\MechCommander Expansion,
    // "Player Name"; the port's registry file), else the logged-in user's name.
    if (const std::optional<std::string> saved =
            MCRegistry::Read("Software\\FASA Interactive\\MechCommander Expansion", "Player Name");
        saved.has_value() && saved->size() + 1 <= bufferSize)
    {
        return saved;
    }

    // Port fix: RegQueryValueExA set the size to the one it needed before failing, and GetUserNameA was then given
    // that larger size for the same buffer. The port keeps the buffer's size.
    std::array<char, bufferSize + 1> name{};
    uint32_t size = bufferSize;

    if (!MCPort::GetUserName(name.data(), &size))
    {
        return std::nullopt;
    }

    return std::string(name.data());
}

auto CancelBool(int32_t) -> void
{
    if (LaunchedFromLobby != 0 && MPlayer != nullptr)
    {
        KillTheGame();
    }

    Cancel();
}

auto BackToSession() -> void
{
    GlobalLogPtr->SetUpSessionScreen();
}

auto BackToSessionBool(int32_t) -> void
{
    GlobalLogPtr->SetUpSessionScreen();
}
