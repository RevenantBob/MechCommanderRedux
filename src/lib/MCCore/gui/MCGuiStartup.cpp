#include "stdafx.h"
#include "gui/MCGuiStartup.h"
#include "color/MCPalette.h"
#include "gui/MCGuiSystem.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFitIniFile.h"
#include "linkup/dpmessage.h"
#include "linkup/dpplayer.h"
#include "linkup/ficommonnetwork.h"
#include "linkup/sessionmanager.h"
#include "main/MCGamePaths.h"
#include "main/main.h"
#include "logistics/MCConnectMenu.h"
#include "main/MCGameContext.h"
#include "mission/MCMission.h"
#include "network/multplyr.h"
#include "platform/MCFrameLog.h"
#include "platform/MCInput.h"
#include "platform/MCPresenter.h"
#include "platform/MCRenderer.h"
#include "platform/MCWin32Defs.h"

namespace
{
    /// <summary>The test message <see cref="SendAndReceiveTestMessages"/> sends: a header, the frame and a count.</summary>
#pragma pack(push, 1)
    struct MCTestMessage
    {
        MCFIGuaranteedMessageHeader Header;
        uint32_t Frame = 0;
        uint32_t Index = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(MCTestMessage) == 0x10);

    /// <summary>The network test's frame count.</summary>
    uint32_t NetworkFrame = 0;

    /// <summary>Takes a test message (looks its sender up).</summary>
    void TestMsgCallback(MCFidpMessage* message, [[maybe_unused]] void* data)
    {
        MPlayer->SessionManager->GetPlayer(message->FromID);
    }

    /// <summary>The network test: sends two test messages to the group every 50 ms, forever (Escape asserts out).</summary>
    [[noreturn]] void SendAndReceiveTestMessages()
    {
        MCSessionManager* manager = MPlayer->SessionManager;
        MCTestMessage test = {};
        test.Header.Header = 0x1064;
        manager->ApplicationCallback = TestMsgCallback;
        manager->ApplicationCallbackData = nullptr;

        for (;;)
        {
            uint32_t frameStart;

            do
            {
                Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 1) == 0, 0, "User exited");
                manager->ProcessMessages();

                for (uint32_t i = 0; i < 2; i++)
                {
                    test.Frame = NetworkFrame;
                    test.Index = i;
                    manager->SendMessageToGroup(0, &test.Header, sizeof(test));
                }

                NetworkFrame++;
                frameStart = MCPort::Milliseconds();
            } while (frameStart + 50 <= MCPort::Milliseconds());

            while (MCPort::Milliseconds() < frameStart + 50)
            {
            }
        }
    }

    /// <summary>
    /// The command line's <c>-network</c> test: joins or hosts the session the mission file's "Multiplayer" block
    /// names, waits for the players, then runs <see cref="SendAndReceiveTestMessages"/>.
    /// </summary>
    void StartMultiplayerGame(std::string_view missionFile)
    {
        MCFitIniFile gameFile;
        MCInput::GetAsyncKeyState(VK_ESCAPE);

        if (gameFile.Open(GamePath(MissionPath, missionFile, "")) != 0 || gameFile.SeekBlock("Multiplayer") != 0)
        {
            return;
        }

        MPlayer = new MCMultiPlayer;
        Assert(MPlayer->Init(0x7d000, 0x100, 100) == 0, 0, "could not initialize multiplayer");

        if (MPlayer->Init(&gameFile) == static_cast<int32_t>(0x8877042e))
        {
            const int32_t numPlayers = MPlayer->NumPlayers();
            uint32_t tries = 0;

            if (MPlayer->IsServer == 0)
            {
                uint32_t result;

                do
                {
                    tries += 50;
                    Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0, 0, "User exited");
                    result = static_cast<uint32_t>(MPlayer->JoinSession(nullptr, nullptr));
                    Assert(result != 0xfffffffe, result, "Error joining session!");
                } while (result != 0);
            }
            else
            {
                MPlayer->CreateSession(nullptr, nullptr, 6);

                while (MPlayer->PlayersInSession() < numPlayers)
                {
                    tries++;

                    if (tries % 50 == 0)
                    {
                        MPlayer->ProcessReceiveList();
                        Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 1) == 0, 0, "User exited");
                    }
                }

                Assert(MPlayer->PlayersInSession() > 1, 0, "No other players joined in time.");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        MPlayer->ProcessReceiveList();

        while (MPlayer->SessionManager->MyPlayer->PlayerNumber < 0)
        {
            MPlayer->ProcessReceiveList();
            Assert((MCInput::GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0, 0, "User exited");
        }

        SendAndReceiveTestMessages();
    }

    /// <summary>A mission number from the command line: <paramref name="last"/> at most, else 0 (none).</summary>
    int32_t SegmentNumber(std::string_view word, int32_t last)
    {
        const int32_t segment = std::atoi(std::string(word).c_str());
        return segment >= 1 && segment <= last ? segment : 0;
    }
}

auto ParseCommandLine(std::string_view commandLine) -> void
{
    // The words, split at runs of spaces (a leading space makes an empty first word, as the original's).
    std::vector<std::string_view> words;
    size_t pos = 0;

    while (pos < commandLine.size())
    {
        const size_t end = std::min(commandLine.find(' ', pos), commandLine.size());
        words.push_back(commandLine.substr(pos, end - pos));
        pos = commandLine.find_first_not_of(' ', end);

        if (pos == std::string_view::npos)
        {
            break;
        }
    }

    std::optional<std::string_view> networkFile;

    // A switch's argument: the next word, if there is one.
    for (size_t i = 0; i < words.size(); i++)
    {
        const std::string_view word = words[i];
        const bool hasArgument = i + 1 < words.size();

        if (MCIEquals(word, "-mission"))
        {
            if (hasArgument)
            {
                GlobalGameSegment = SegmentNumber(words[++i], 99);
            }
            else
            {
                i++;
            }
        }
        else if (word == "+")
        {
            if (hasArgument)
            {
                GlobalGameSegment = SegmentNumber(words[++i], 50);
            }
            else
            {
                i++;
            }
        }
        else if (MCIEquals(word, "-renderer"))
        {
            // Port: "-renderer vulkan|software" picks the renderer over PREFS "Renderer".
            if (hasArgument)
            {
                if (const std::optional<MCRendererKind> kind = MCRendererKindFromName(std::string(words[i + 1])))
                {
                    GRenderer = static_cast<int>(*kind);
                }
            }

            i++;
        }
        else if (MCIEquals(word, "-gpudraw"))
        {
            // Port: "-gpudraw off|on|mirror": who draws the frame with the Vulkan renderer (see MCGpuDrawing).
            if (hasArgument)
            {
                if (const std::optional<MCGpuDrawing> drawing = MCGpuDrawingFromName(std::string(words[i + 1])))
                {
                    MCRenderer::RequestGpuDrawing(*drawing);
                }
            }

            i++;
        }
        else if (MCIEquals(word, "-fps"))
        {
            // Port: "-fps" draws the frame counter (as PREFS "ShowFps").
            GShowFps = 1;
        }
        else if (MCIEquals(word, "-framelog"))
        {
            // Port: "-framelog <file>" writes the slow frames there, with what took their time (MCFrameLog).
            if (hasArgument && !MCFrameLog::Open(std::string(words[i + 1])))
            {
                SDL_Log("-framelog: can't write %s", std::string(words[i + 1]).c_str());
            }

            i++;
        }
        else if (MCIEquals(word, "-novsync"))
        {
            // Port: "-novsync" shows frames as soon as they are drawn instead of at the display's refresh.
            GVSync = false;
        }
        else if (MCIEquals(word, "-gpudump"))
        {
            // Port: "-gpudump <folder>": mirror mode saves the first frame that differs there.
            if (hasArgument)
            {
                MCRenderer::SetMirrorDumpFolder(std::string(words[i + 1]));
            }

            i++;
        }
        else if (MCIEquals(word, "-network"))
        {
            if (hasArgument)
            {
                networkFile = words[i + 1];
            }

            i++;
        }
        else if (MCIEquals(word, "-load"))
        {
            if (hasArgument)
            {
                StartupPakFile = words[i + 1];
            }

            i++;
        }
    }

    if (networkFile)
    {
        StartMultiplayerGame(*networkFile);
    }
}

auto RealWinMain(void* instance, std::string_view commandLine) -> int
{
    // The original noted the stack top and warned when the page file was under 48,000,000 bytes (GlobalMemoryStatus,
    // string 0x355), and refused a second instance; none of that applies.
    MCPort::SeedRand(static_cast<uint32_t>(std::time(nullptr)));
    ThisInstance = instance;
    SavePath = "c:\\Program Files\\Honor Bound\\";
    DirectXPath = "\\honorb\\directx\\";
    TerrainPath = "data\\terrain\\";
    PalettePath = "data\\palette\\";
    ArtPath = "data\\art\\";
    FontPath = "data\\fonts\\";
    SoundPath = "data\\sound\\";
    SpritePath = "data\\sprites\\";
    InterfacePath = "data\\iface\\";
    PaletteName = "palette.gif";
    OldMouseY = -1;
    OldMouseX = -1;

    MCGameContext& context = MCGameContext::Current();
    context.SetGuiSystem(std::make_unique<MCGuiSystem>());

    if (GuiSystem()->Start(commandLine, 640, 480) != 0)
    {
        return -4;
    }

    GuiSystem()->Run();
    GuiSystem()->Stop();
    context.SetGuiSystem(nullptr);
    return 0;
}
