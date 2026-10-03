#include "stdafx.h"
#include "MCTest.h"
#include "TestGame.h"
#include "camera/camera.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "lib/fastfile.h"
#include "lib/heap.h"
#include "logistics/loggen.h"
#include "logistics/logmain.h"
#include "main/honorb.h"
#include "main/logistics.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/mover.h"
#include "platform/MCDisplay.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"

namespace MCTestGame
{
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
        maxFastFiles = 5;
        fastFiles = static_cast<FastFile**>(std::calloc(static_cast<size_t>(maxFastFiles), sizeof(FastFile*)));

        for (const char* name : {"art.fst", "mission.fst", "misc.fst", "terrain.fst", "shapes.fst"})
        {
            FastFileInit(name);
        }
    }

    namespace
    {
        /// <summary>What the process booted: 0 nothing yet, -1 logistics, else a mission segment.</summary>
        int32_t booted = 0;

        /// <summary>RealWinMain up to aSystem::run, with <paramref name="commandLine"/>, in a hidden window.</summary>
        bool Boot(std::string commandLine);
    }

    bool StartMission(int32_t segment)
    {
        if (booted != 0)
        {
            return booted == segment && scenario != nullptr;
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

            if (scenario != nullptr && mission->missionState == 7 && turn > 30)
            {
                return true;
            }
        }

        std::cout << "  the scenario never started (mission state " << mission->missionState << ")\n";
        return false;
    }

    bool StartLogistics()
    {
        if (booted != 0)
        {
            return booted == -1 && globalLogPtr != nullptr;
        }

        booted = -1;
        // The screen wipes draw frames until a quarter of a second has passed.
        MCPort::AdvanceManualClockOnPresent();

        if (!Boot(""))
        {
            return false;
        }

        // The intro movie (if any) and the main menu's entrance.
        for (int32_t frame = 0; frame < 3000; frame++)
        {
            RunFrame(1.0f / 15.0f);

            if (mission->missionState == 3 && globalLogPtr != nullptr &&
                globalLogPtr->currentScreen == globalLogPtr->mainScreen && application->smackerWindow == nullptr &&
                application->smackerWindow2 == nullptr)
            {
                return true;
            }
        }

        std::cout << "  logistics never came up (mission state " << mission->missionState << ")\n";
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
            gHiddenWindow = 1;
            gNoSound = 1;
            // Deterministic runs: game time only moves with RunFrame, and the dice start the same way (RealWinMain seeds
            // them from the time of day). --seed <n> picks other dice. With the default, mission 1's Uller has its
            // pilot knocked out (4 wounds) in the fight the mission tests stage.
            MCPort::UseManualClock();
            const char* seed = MCTest::Option("seed");
            std::srand(seed != nullptr ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 10u);
            // The world view shows 480 lines (one world pixel per screen pixel in the 640x480 window) at any zoom
            // request, so what is on screen, and so updated, is the same every run.
            MCFixedZoomHeight = 480.0f;
            globalHeapList = new HeapList();
            std::strcpy(paletteName, "palette.gif");
            application = ::new aSystem;

            if (application->start(nullptr, nullptr, commandLine.data(), 1, 640, 480) != 0)
            {
                std::cout << "  aSystem::start failed\n";
                return false;
            }

            for (int32_t i = 0; i < screenWindow->numberOfChildren(); i++)
            {
                screenWindow->child(i)->draw();
            }

            return true;
        }
    }

    void RunFrame(float seconds)
    {
        MCPort::AdvanceManualClock(static_cast<uint64_t>(static_cast<double>(seconds) * 1e9));
        frameLength = seconds;
        frameRate = 1.0f / seconds;
        MCInput::PumpMessages();

        if (application->smackerWindow2 == nullptr && application->smackerWindow == nullptr)
        {
            for (int32_t i = 0; i < application->numCallbacks; i++)
            {
                if (application->callbacks[i] != nullptr)
                {
                    application->callbacks[i]->execute();
                }
            }
        }

        int32_t staticNoise = 0;
        int32_t noiseChance = 0;

        if (scenario != nullptr)
        {
            staticNoise = scenario->startingUp;
            noiseChance = scenario->startUpCountdown;
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
            std::fprintf(frameLog, "frame %d turn %d time %.3f clock %u", frame++, turn,
                         static_cast<double>(scenarioTime), MCPort::Milliseconds());

            for (int32_t partId = 0x200; partId < MAX_MOVER_PART_ID && scenario != nullptr; partId++)
            {
                if (Mover* mover = getMoverFromPartId(partId); mover != nullptr)
                {
                    const vector_3d position = mover->getPosition();
                    std::fprintf(frameLog, " %d:%.2f,%.2f", partId, static_cast<double>(position.x),
                                 static_cast<double>(position.y));
                }
            }

            std::fprintf(frameLog, "\n");
            std::fflush(frameLog);
        }
    }
}
