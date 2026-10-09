#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "gui/MCGuiSystem.h"
#include "linkup/sessionmanager.h"
#include "logistics/MCFileScrollPane.h"
#include "logistics/MCGameList.h"
#include "logistics/MCLogComboBox.h"
#include "logistics/MCLogSlider.h"
#include "logistics/MCSplashScreen.h"
#include "logistics/MCConnectMenu.h"
#include "main/logistics.h"
#include "network/multplyr.h"
#include "platform/MCDirectPlay.h"
#include "platform/MCDisplay.h"
#include "platform/MCInput.h"

using namespace MCScreenInput;

namespace
{
    /// <summary>The loopback address the game hosts on (not the default port, so a running game is left alone).</summary>
    constexpr const char* HostAddress = "127.0.0.1:28877";

    /// <summary>Selects the TCP/IP connection to <paramref name="address"/> ("host:port").</summary>
    uint32_t Connect(MCDirectPlay& directPlay, const char* address)
    {
        DPCOMPOUNDADDRESSELEMENT elements[2];
        elements[0].guidDataType = MCDirectPlayGuids::DPAID_ServiceProvider;
        elements[0].dwDataSize = sizeof(_GUID);
        elements[0].lpData = const_cast<_GUID*>(&MCDirectPlayGuids::DPSPGUID_TCPIP);
        elements[1].guidDataType = MCDirectPlayGuids::DPAID_INet;
        elements[1].dwDataSize = static_cast<uint32_t>(std::strlen(address) + 1);
        elements[1].lpData = const_cast<char*>(address);
        uint8_t connection[MCDirectPlay::ConnectionDataSize];
        uint32_t size = sizeof(connection);
        const uint32_t result = MCDirectPlay::CreateCompoundAddress(elements, 2, connection, &size);
        return result != DP_OK ? result : directPlay.InitializeConnection(connection, 0);
    }

    /// <summary>Runs <paramref name="call"/> while another thread pumps <paramref name="other"/> (the game is not running).</summary>
    template <class Call> auto WhilePumping(MCDirectPlay& other, Call&& call)
    {
        std::atomic<bool> stop = false;
        std::thread pumper(
            [&]
            {
                while (!stop)
                {
                    other.Pump();
                    SDL_Delay(1);
                }
            });
        auto result = call();
        stop = true;
        pumper.join();
        return result;
    }

    /// <summary>
    /// A second player in the game's session, on the test's side: it joins over the loopback and then only takes in
    /// (and drops) what it is sent, on its own thread, so the host keeps it.
    /// </summary>
    class Guest
    {
    public:
        bool Join(MCDirectPlay& host)
        {
            if (Connect(_DirectPlay, HostAddress) != DP_OK)
            {
                return false;
            }

            DPSESSIONDESC2 found{};
            bool foundOne = false;

            for (int attempt = 0; attempt < 40 && !foundOne; attempt++)
            {
                DPSESSIONDESC2 desc{};
                desc.dwSize = sizeof(desc);
                desc.guidApplication = ThisAppGuid;
                struct Context
                {
                    DPSESSIONDESC2* Found;
                    bool* FoundOne;
                } context{&found, &foundOne};
                _DirectPlay.EnumSessions(
                    &desc, 0,
                    [](const DPSESSIONDESC2* session, uint32_t*, uint32_t flags, void* data)
                    {
                        auto* context = static_cast<Context*>(data);

                        if ((flags & DPESC_TIMEDOUT) != 0)
                        {
                            return 0;
                        }

                        *context->Found = *session;
                        *context->FoundOne = true;
                        return 0;
                    },
                    &context, DPENUMSESSIONS_AVAILABLE);
                host.Pump();
                SDL_Delay(5);
            }

            if (!foundOne)
            {
                return false;
            }

            if (WhilePumping(host, [&] { return _DirectPlay.Open(&found, DPOPEN_JOIN | DPOPEN_RETURNSTATUS); }) !=
                DP_OK)
            {
                return false;
            }

            DPNAME name{sizeof(DPNAME), 0, const_cast<char*>("Guest"), nullptr};
            uint32_t id = 0;

            if (WhilePumping(host, [&] { return _DirectPlay.CreatePlayer(&id, &name, nullptr, nullptr, 0, 0); }) !=
                DP_OK)
            {
                return false;
            }

            _Listener = std::thread(
                [this]
                {
                    while (!_Stop)
                    {
                        uint8_t buffer[4096];
                        uint32_t size = sizeof(buffer);
                        uint32_t from = 0;
                        uint32_t to = 0;

                        while (_DirectPlay.Receive(&from, &to, DPRECEIVE_ALL, buffer, &size) == DP_OK)
                        {
                            size = sizeof(buffer);
                        }

                        _DirectPlay.Pump();
                        SDL_Delay(1);
                    }
                });
            return true;
        }

        ~Guest()
        {
            _Stop = true;

            if (_Listener.joinable())
            {
                _Listener.join();
            }
        }

    private:
        MCDirectPlay _DirectPlay;
        std::thread _Listener;
        std::atomic<bool> _Stop = false;
    };

    /// <summary>Runs frames for two seconds of game time, giving the network a moment each; the last frame's hash.</summary>
    uint32_t Settle()
    {
        uint32_t hash = 0;

        for (int32_t frame = 0; frame < 30; frame++)
        {
            SDL_Delay(3);
            MCTestGame::RunFrame(1.0f / 15.0f);
            hash = ScreenHash();
        }

        return hash;
    }

    /// <summary>Types <paramref name="text"/> into the text object that has the focus.</summary>
    void Type(const char* text)
    {
        for (const char* c = text; *c != 0; c++)
        {
            SendKey(static_cast<uint8_t>(*c));
            MCTestGame::RunFrame(1.0f / 15.0f);
        }
    }
}

/// <summary>
/// Renderer phase 3 step 4g: the multiplayer session screen, its player names, ready lights, chat window and chat
/// input draw from their state each frame. The game hosts a LAN session on the loopback, a second player joins from
/// the test, and the host moves the players onto teams, changes the points and the tech base and chats. Each step's
/// screen was recorded on the code before step 4 (when these drew into pictures at the time of each event).
/// </summary>
TEST_CASE_ISOLATED("game: the multiplayer session screen matches the pre-renderer frames")
{
    if (!MCTestGame::Available())
    {
        return;
    }

    REQUIRE(MCTestGame::StartLogistics());

    // The LAN screen as the TCP/IP button leaves it, hosting on the loopback.
    ConnectScreen();
    Settle();
    REQUIRE(MPlayer != nullptr);
    MCSessionManager* manager = MPlayer->SessionManager;
    manager->ConnectTcp(const_cast<char*>(HostAddress));
    MCSplashScreen* lanScreen = GlobalLogPtr->LanScreen;
    GlobalLogPtr->MultiplayerScreen->ShowGuiWindow(0);
    lanScreen->ShowGuiWindow(1);
    lanScreen->ShowBlock(0);
    GlobalLogPtr->CurrentScreen = lanScreen;
    GlobalLogPtr->LogisticsState = 0xb;
    static_cast<MCLogTextObject*>(lanScreen->Elements[4])->SetStringBuffer("Commander");
    static_cast<MCLogTextObject*>(lanScreen->Elements[10])->SetStringBuffer("Test game");
    static_cast<MCLogTextObject*>(lanScreen->Elements[11])->SetStringBuffer("6");
    CreateSession();
    Settle();
    REQUIRE(manager->CurrentSession != nullptr);
    REQUIRE(manager->DirectPlay != nullptr);

    Guest guest;
    REQUIRE(guest.Join(*manager->DirectPlay));
    Settle();

    // Every frame presented from here on is folded into one more hash.
    uint32_t presents = 0x811c9dc5;
    MCTestGame::OnPresent = [&] { presents = (presents ^ ScreenHash()) * 0x01000193; };

    struct Step
    {
        const char* Name;
        std::function<void()> Action;
        uint32_t Expected;
    };

    // The cursor hasn't moved yet in the first step: at (0, 0) the original's over test (OB-129, fixed) took it as over
    // every tool button, so its frame was 0x3d56c0c0 before the fix.
    const Step steps[] = {
        {"session", [] { Go(); }, 0xa6b1053fu},
        {"first name to team 1", [] { Drag(0x15, 0x16a, 0x100, 0x13a); }, 0x7548a837u},
        {"next name to team 2", [] { Drag(0x15, 0x16a, 0x1c4, 0x13a); }, 0xf143dbb4u},
        {"team 1 points up", [] { Click(0x106, 0x186); }, 0x1a793ae4u},
        {"team 2 Inner Sphere", [] { Click(0x225, 0x112); }, 0xa6d18671u},
        {"hover a light", [] { SendMouse(7, 0xdc, 5, false); }, 0x4adcfb49u},
        {"chat typed",
         []
         {
             Click(0x40, 0x130);
             Type("hello there");
         },
         0x74b8c0d0u},
        {"chat sent", [] { SendKey(0xd); }, 0x2d2edef8u},
        {"name back off the team", [] { Drag(0x105, 0x13a, 0x40, 0x1c0); }, 0x983fc7f9u},
    };

    int32_t index = 0;

    for (const Step& step : steps)
    {
        MCTest::Scope scope(step.Name);
        step.Action();
        const uint32_t hash = Settle();
        SaveShot(std::format("session{:02} {}", index, step.Name), hash);
        CHECK_EQ(hash, step.Expected);
        index++;
    }

    MCTestGame::OnPresent = nullptr;
    // 0xa176a052 before the OB-129 fix (the first step's frames), 0x87982e74 before the control audit (2026-10-04: the
    // steps are unchanged; between them presses end at the release, and the ready lights' backing and the clock show
    // at once).
    CHECK_EQ(presents, 0x8a6bba0eu);
}
