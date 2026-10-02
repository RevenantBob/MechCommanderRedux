#include "stdafx.h"
#include "TestGame.h"
#include "gui/asystem.h"
#include "gui/updisp.h"
#include "lib/fastfile.h"
#include "lib/heap.h"
#include "main/honorb.h"
#include "main/main.h"
#include "mission/mission.h"
#include "mission/scenario.h"
#include "object/mover.h"
#include "platform/MCFileSystem.h"
#include "platform/MCInput.h"

namespace MCTestGame
{
    bool Available()
    {
        static const bool available = []
        {
            const char* root = std::getenv("MC_GAME");

            if (root == nullptr || !std::filesystem::is_directory(root))
            {
                std::cout << "  (skipped: set MC_GAME to the MechCommander Gold install)\n";
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

    bool StartMission(int32_t segment)
    {
        static int32_t started = 0;

        if (started != 0)
        {
            return started == segment && scenario != nullptr;
        }

        started = segment;

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
        // them from the time of day). MC_TEST_SEED picks other dice. With the default, mission 1's Uller has its
        // pilot knocked out (4 wounds) in the fight the mission tests stage.
        MCPort::UseManualClock();
        const char* seed = std::getenv("MC_TEST_SEED");
        std::srand(seed != nullptr ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)) : 10u);
        globalHeapList = new HeapList();
        std::strcpy(paletteName, "palette.gif");
        application = ::new aSystem;
        std::string commandLine = "-mission " + std::to_string(segment);

        if (application->start(nullptr, nullptr, commandLine.data(), 1, 640, 480) != 0)
        {
            std::cout << "  aSystem::start failed\n";
            return false;
        }

        for (int32_t i = 0; i < screenWindow->numberOfChildren(); i++)
        {
            screenWindow->child(i)->draw();
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

        // MC_TEST_FRAMES=<file>: each frame's movers, to find where two runs part.
        static FILE* frameLog = []
        {
            const char* name = std::getenv("MC_TEST_FRAMES");
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
