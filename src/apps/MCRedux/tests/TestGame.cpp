#include "stdafx.h"
#include "TestGame.h"
#include "lib/fastfile.h"
#include "platform/MCFileSystem.h"

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
}
