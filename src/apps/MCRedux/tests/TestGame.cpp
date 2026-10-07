#include "stdafx.h"
#include "MCTest.h"
#include "ScreenInput.h"
#include "TestGame.h"
#include "fakes/MCManualClock.h"
#include "camera/MCCamera.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "lib/MCFastFileSet.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "main/honorb.h"
#include "main/logistics.h"
#include "main/MCGameContext.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/mover.h"
#include "object/warrior.h"
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"

namespace
{
    /// <summary>
    /// A failed SDL_assert prints what failed and ends the process (exit 42), so the test fails. SDL's own handler
    /// would open a prompt that nobody answers in a headless run, and its abort shows the Debug CRT's abort() box.
    /// </summary>
    SDL_AssertState SDLCALL FailOnAssert(const SDL_AssertData* data, void*)
    {
        std::cout << std::format("  {}({}): assertion failed in {}: {}\n", data->filename, data->linenum,
                                 data->function, data->condition)
                  << std::flush;
        std::_Exit(42);
    }

    /// <summary>Installs <see cref="FailOnAssert"/> before any test runs.</summary>
    const bool AssertHandlerInstalled = []
    {
        SDL_SetAssertionHandler(FailOnAssert, nullptr);
        return true;
    }();
}

namespace MCTestGame
{
    std::function<void()> OnPresent;

    namespace
    {
        /// <summary>
        /// The log file named by option <paramref name="option"/>, opened for appending with a <c>== &lt;test&gt;</c>
        /// line first (each isolated test runs in a process of its own and adds its section), or null without the
        /// option.
        /// </summary>
        FILE* OpenLog(const char* option)
        {
            const char* name = MCTest::Option(option);

            if (name == nullptr)
            {
                return nullptr;
            }

            FILE* log = std::fopen(name, "a");

            if (log != nullptr)
            {
                std::fprintf(log, "== %s\n", MCTest::CurrentTestName());
            }

            return log;
        }

        /// <summary>Every present: the --present-log line, the --present-shot screenshot, then <see cref="OnPresent"/>.</summary>
        void Presented()
        {
            static FILE* presentLog = OpenLog("present-log");
            static const std::string shotsWanted = []
            {
                const char* shotList = MCTest::Option("present-shot");
                return shotList != nullptr ? std::format(",{},", shotList) : std::string();
            }();
            static int32_t presentIndex = 0;

            if (presentLog != nullptr)
            {
                std::fprintf(presentLog, "%d 0x%08x\n", presentIndex, MCScreenInput::ScreenHash());
                std::fflush(presentLog);
            }

            const char* shots = MCTest::Option("shots");

            if (shots != nullptr && shotsWanted.contains(std::format(",{},", presentIndex)))
            {
                (void)MCInput::Display()->SaveScreenshot(std::filesystem::path(shots) /
                                                         std::format("present{}.bmp", presentIndex));
            }

            presentIndex++;

            if (OnPresent)
            {
                OnPresent();
            }
        }

        /// <summary>Folds <paramref name="value"/>'s bytes into the FNV-1a hash <paramref name="hash"/>.</summary>
        template <typename T> void Fold(uint32_t& hash, const T& value)
        {
            const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);

            for (const uint8_t byte : bytes)
            {
                hash = (hash ^ byte) * 0x01000193;
            }
        }

        /// <summary>Folds a vector's three floats into <paramref name="hash"/>.</summary>
        void FoldVector(uint32_t& hash, const MCVector3D& vector)
        {
            Fold(hash, vector.X);
            Fold(hash, vector.Y);
            Fold(hash, vector.Z);
        }
    }

    uint32_t StateHash()
    {
        uint32_t hash = 0x811c9dc5;
        Fold(hash, Turn);
        Fold(hash, ScenarioTime);
        Fold(hash, MCPort::RandState());

        if (Scenario == nullptr)
        {
            return hash;
        }

        for (int32_t partId = 0x200; partId < MAX_MOVER_PART_ID; partId++)
        {
            MCMover* mover = GetMoverFromPartId(partId);

            if (mover == nullptr)
            {
                continue;
            }

            Fold(hash, partId);
            FoldVector(hash, mover->Position);
            FoldVector(hash, mover->Frame.I);
            FoldVector(hash, mover->Frame.J);
            FoldVector(hash, mover->Frame.K);
            Fold(hash, mover->Status);

            for (int32_t i = 0; mover->Body != nullptr && i < mover->NumBodyLocations; i++)
            {
                Fold(hash, mover->Body[i].CurInternalStructure);
                Fold(hash, mover->Body[i].DamageState);
            }

            for (int32_t i = 0; mover->Armor != nullptr && i < mover->NumArmorLocations; i++)
            {
                Fold(hash, mover->Armor[i].CurArmor);
            }

            if (MCMechWarrior* pilot = mover->GetPilot(); pilot != nullptr)
            {
                Fold(hash, pilot->Status);
                Fold(hash, pilot->OrderState);
                Fold(hash, pilot->CurTacOrder.Id);
                Fold(hash, pilot->CurTacOrder.Code);
            }
        }

        for (uint32_t i = 0; Scenario->Objectives != nullptr && i < Scenario->NumObjectives; i++)
        {
            Fold(hash, Scenario->Objectives[i].Status);
        }

        return hash;
    }

    bool Available()
    {
        static const bool available = []
        {
            // --game <folder>, else the current folder when it holds the game (as the game looks for SYSTEM.CFG).
            const char* option = MCTest::Option("game");
            const std::filesystem::path root =
                option != nullptr ? std::filesystem::path(option) : std::filesystem::current_path();

            if (!std::filesystem::is_regular_file(root / "SYSTEM.CFG"))
            {
                std::cout << "  (skipped: pass --game <the MechCommander Gold install>)\n";
                return false;
            }

            MCFileSystem::SetGameRoot(root);
            return true;
        }();
        return available;
    }

    void OpenFastFiles()
    {
        static bool opened = false;

        if (opened)
        {
            return;
        }

        opened = true;
        MCFastFileSet& fastFiles = MCGameContext::Current().FastFiles();

        for (const char* name : {"art.fst", "mission.fst", "misc.fst", "terrain.fst", "shapes.fst"})
        {
            (void)fastFiles.Open(name);
        }
    }

    namespace
    {
        /// <summary>What the process booted: 0 nothing yet, -1 logistics, else a mission segment.</summary>
        int32_t booted = 0;

        /// <summary>The boot's clock (in the context current at the boot), once booted.</summary>
        MCManualClock* clock = nullptr;

        /// <summary>Set by StartLogistics: the boot's clock moves on with the presents.</summary>
        bool clockFollowsPresents = false;

        /// <summary>RealWinMain up to aSystem::run, with <paramref name="commandLine"/>, in a hidden window.</summary>
        bool Boot(std::string commandLine);
    }

    bool StartMission(int32_t segment)
    {
        if (booted != 0)
        {
            return booted == segment && Scenario != nullptr;
        }

        booted = segment;

        if (!Boot("-mission " + std::to_string(segment)))
        {
            return false;
        }

        // The hidden window has no mouse, so the cursor stays at (0, 0), where it scrolls the camera off the
        // player's units into the map's corner. Park it in the middle of the screen, as a player's would be.
        if (MCDisplay* display = MCInput::Display(); display != nullptr)
        {
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = SDL_GetWindowID(display->Window());
            display->LogicalToWindow(320.5f, 240.5f, motion.motion.x, motion.motion.y);
            MCInput::HandleEvent(motion);
        }

        // The scenario loads and then counts down its start-up; run until units are moving.
        for (int32_t frame = 0; frame < 3000; frame++)
        {
            RunFrame(1.0f / 15.0f);

            if (Scenario != nullptr && Mission->MissionState == 7 && Turn > 30)
            {
                return true;
            }
        }

        std::cout << "  the scenario never started (mission state " << Mission->MissionState << ")\n";
        return false;
    }

    bool StartLogistics()
    {
        if (booted != 0)
        {
            return booted == -1 && GlobalLogPtr != nullptr;
        }

        booted = -1;
        // The screen wipes draw frames until a quarter of a second has passed.
        clockFollowsPresents = true;

        if (!Boot(""))
        {
            return false;
        }

        // The intro movie (if any) and the main menu's entrance.
        for (int32_t frame = 0; frame < 3000; frame++)
        {
            RunFrame(1.0f / 15.0f);

            if (Mission->MissionState == 3 && GlobalLogPtr != nullptr &&
                GlobalLogPtr->CurrentScreen == GlobalLogPtr->MainScreen && Application->SmackerWindow == nullptr &&
                Application->SmackerWindow2 == nullptr)
            {
                return true;
            }
        }

        std::cout << "  logistics never came up (mission state " << Mission->MissionState << ")\n";
        return false;
    }

    namespace
    {
        bool Boot(std::string commandLine)
        {
            // The game writes its saves and settings under the user folder; keep them out of the real one.
            const std::filesystem::path userRoot = std::filesystem::temp_directory_path() / "mc_tests_user";
            std::filesystem::create_directories(userRoot);
            MCFileSystem::SetUserRoot(userRoot);

            // The sound system still runs (as in play), into SDL's silent driver.
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");

            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
            {
                std::cout << "  SDL_Init failed: " << SDL_GetError() << "\n";
                return false;
            }

            // RealWinMain, up to aSystem::run. systemInit reads the paths from SYSTEM.CFG; the FastFiles it opens are
            // every *.fst, so none may be open yet.
            GHiddenWindow = 1;
            GNoSound = 1;
            // Deterministic runs: game time only moves with RunFrame, and the dice start the same way (RealWinMain seeds
            // them from the time of day). --seed <n> picks other dice. With the default, mission 1's Uller has its
            // pilot knocked out (4 wounds) in the fight the mission tests stage.
            clock = &MCGameContext::Current().SetClock(std::make_unique<MCManualClock>());

            if (clockFollowsPresents)
            {
                clock->AdvanceOnPresent();
            }

            const char* seed = MCTest::Option("seed");
            MCPort::SeedRand(seed != nullptr ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 10u);
            // The world view shows 480 lines (one world pixel per screen pixel in the 640x480 window) at any zoom
            // request, so what is on screen, and so updated, is the same every run.
            MCFixedZoomHeight = 480.0f;
            std::strcpy(PaletteName, "palette.gif");
            Application = new MCGuiSystem;

            if (Application->Start(nullptr, nullptr, commandLine.data(), 1, 640, 480) != 0)
            {
                std::cout << "  aSystem::start failed\n";
                return false;
            }

            if (MCDisplay* display = MCInput::Display(); display != nullptr)
            {
                display->OnPresent = Presented;
            }

            for (int32_t i = 0; i < ScreenWindow->NumberOfChildren(); i++)
            {
                ScreenWindow->Child(i)->Draw();
            }

            return true;
        }
    }

    void RunFrame(float seconds)
    {
        clock->Advance(static_cast<uint64_t>(static_cast<double>(seconds) * 1e9));
        FrameLength = seconds;
        FrameRate = 1.0f / seconds;
        MCInput::PumpMessages();

        if (Application->SmackerWindow2 == nullptr && Application->SmackerWindow == nullptr)
        {
            for (int32_t i = 0; i < Application->NumCallbacks; i++)
            {
                if (Application->Callbacks[i] != nullptr)
                {
                    Application->Callbacks[i]->Execute();
                }
            }
        }

        int32_t staticNoise = 0;
        int32_t noiseChance = 0;

        if (Scenario != nullptr)
        {
            staticNoise = Scenario->StartingUp;
            noiseChance = Scenario->StartUpCountdown;
        }

        UpdateDisplay(0, staticNoise, noiseChance, 0, 0);

        // --frames <file>: each frame's movers, to find where two runs part.
        static FILE* frameLog = []
        {
            const char* name = MCTest::Option("frames");
            return name != nullptr ? std::fopen(name, "w") : nullptr;
        }();

        if (frameLog != nullptr)
        {
            static int32_t frame = 0;
            std::fprintf(frameLog, "frame %d turn %d time %.3f clock %u", frame++, Turn,
                         static_cast<double>(ScenarioTime), MCPort::Milliseconds());

            for (int32_t partId = 0x200; partId < MAX_MOVER_PART_ID && Scenario != nullptr; partId++)
            {
                if (MCMover* mover = GetMoverFromPartId(partId); mover != nullptr)
                {
                    const MCVector3D position = mover->GetPosition();
                    std::fprintf(frameLog, " %d:%.2f,%.2f", partId, static_cast<double>(position.X),
                                 static_cast<double>(position.Y));
                }
            }

            std::fprintf(frameLog, "\n");
            std::fflush(frameLog);
        }

        // --state-log <file>: each frame's state hash, to find where two builds part (tools/ci/baseline.py).
        static FILE* stateLog = OpenLog("state-log");

        if (stateLog != nullptr && Scenario != nullptr)
        {
            static int32_t stateFrame = 0;
            std::fprintf(stateLog, "%d 0x%08x\n", stateFrame++, StateHash());
            std::fflush(stateLog);
        }
    }
}
