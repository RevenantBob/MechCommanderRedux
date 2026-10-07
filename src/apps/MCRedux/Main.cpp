#include "stdafx.h"
#include "MCConsole.h"
#include "MCCrashTrace.h"
#include "MCVersion.h"
#include "abl/ablenv.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablxstmt.h"
#include "main/rmain.h"
#include "platform/MCAllocator.h"
#include "platform/MCFileSystem.h"

namespace
{
    /// <summary>What the command line asks for: the two folders and the words passed on to the game.</summary>
    struct MCLaunchOptions
    {
        std::filesystem::path DataFolder;
        std::filesystem::path UserFolder;
        std::string GameCommandLine;
    };

    /// <summary>The command line's help text.</summary>
    constexpr const char* UsageText =
        "MechCommander Redux " MC_VERSION_SEMVER " (build " MC_VERSION ", commit " MC_VERSION_COMMIT ")\n\n"
        "MCRedux [-d|--data <install folder>] [--user <save folder>] [game options]\n\n"
        "  -d, --data     - The MechCommander Gold install; without it, or when it has no\n"
        "                   SYSTEM.CFG, the current folder, then the executable's folder.\n"
        "  --user         - Where saves, prefs and temp files go (default: the SDL pref path)\n"
        "                   game options, as MCX.EXE takes them.\n"
        "  -mission <n>   - Start game segment n of SYSTEM.CFG's missionName (skips logistics)\n"
        "  -load <file>   - Load a saved game\n";

    /// <summary>
    /// Shows <paramref name="text"/> to the player: on the terminal MCRedux was started from, else in a message box.
    /// </summary>
    void Report(const std::string& text, bool isError)
    {
        if (MCConsole::HasOutput())
        {
            (isError ? std::cerr : std::cout) << text << "\n";
            return;
        }

        SDL_ShowSimpleMessageBox(isError ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, "MCRedux", text.c_str(),
                                 nullptr);
    }

    /// <summary>Whether <paramref name="folder"/> holds the game's data (its SYSTEM.CFG).</summary>
    bool HasGameData(const std::filesystem::path& folder)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(folder / "SYSTEM.CFG", error) ||
               std::filesystem::is_regular_file(folder / "system.cfg", error);
    }

    /// <summary>
    /// The first of the --data folder (when given), the current folder and the executable's folder that holds the
    /// game's data, or an error naming every folder tried.
    /// </summary>
    std::expected<std::filesystem::path, std::string> FindGameData(const std::filesystem::path& dataArgument)
    {
        std::vector<std::filesystem::path> candidates;

        if (!dataArgument.empty())
        {
            candidates.push_back(dataArgument);
        }

        std::error_code error;
        candidates.push_back(std::filesystem::current_path(error));

        if (const char* basePath = SDL_GetBasePath(); basePath != nullptr)
        {
            candidates.push_back(basePath);
        }

        std::string tried;

        for (const std::filesystem::path& folder : candidates)
        {
            if (!folder.empty() && HasGameData(folder))
            {
                return folder;
            }

            tried += std::format("\n  {}", folder.string());
        }

        return std::unexpected(std::format("No SYSTEM.CFG found in:{}\nRun MCRedux from the MechCommander Gold "
                                           "install, put it beside the install, or point -d/--data at it.",
                                           tried));
    }

    std::expected<MCLaunchOptions, std::string> ParseArguments(int argc, char** argv)
    {
        MCLaunchOptions options;

        for (int i = 1; i < argc; i++)
        {
            const std::string_view arg = argv[i];

            if (arg == "--data" || arg == "-d" || arg == "--user")
            {
                if (i + 1 >= argc)
                {
                    return std::unexpected(std::format("{} needs a folder", arg));
                }

                (arg == "--user" ? options.UserFolder : options.DataFolder) = argv[++i];
            }
            else if (arg == "--help" || arg == "-h")
            {
                return std::unexpected(std::string());
            }
            else
            {
                if (!options.GameCommandLine.empty())
                {
                    options.GameCommandLine += ' ';
                }

                options.GameCommandLine += arg;
            }
        }

        auto dataFolder = FindGameData(options.DataFolder);

        if (!dataFolder)
        {
            return std::unexpected(dataFolder.error());
        }

        options.DataFolder = *dataFolder;

        if (options.UserFolder.empty())
        {
            char* prefPath = SDL_GetPrefPath(nullptr, "MechCommander Redux");

            if (prefPath == nullptr)
            {
                return std::unexpected(std::format("No save folder: {}", SDL_GetError()));
            }

            options.UserFolder = prefPath;
            SDL_free(prefPath);
        }

        return options;
    }

    /// <summary>Crash reporter: where the ABL interpreter was (module, routine, source line, code pointer).</summary>
    void ReportAblState()
    {
        std::fprintf(stderr, "ABL: module %s (handle %d), library %s\n",
                     CurModule != nullptr ? CurModule->GetName() : "(none)", CurModuleHandle,
                     CurLibrary != nullptr ? CurLibrary->GetName() : "(none)");

        if (CurRoutineIdPtr != nullptr)
        {
            const char* segment = CurRoutineIdPtr->Defn.Info.Routine.CodeSegment;
            std::fprintf(stderr, "ABL: routine %s, code segment %p, codeSegmentPtr %p (+%lld), statement start +%lld\n",
                         CurRoutineIdPtr->Name, static_cast<const void*>(segment),
                         static_cast<const void*>(CodeSegmentPtr), static_cast<long long>(CodeSegmentPtr - segment),
                         static_cast<long long>(StatementStartPtr - segment));
        }

        const char* sourceFile = "?";

        if (CurModule != nullptr && CurModuleHandle >= 0 && ModuleRegistry != nullptr)
        {
            const MCModuleEntry& entry = ModuleRegistry[CurModuleHandle];

            if (entry.SourceFiles != nullptr && FileNumber >= 0 && FileNumber < entry.NumSourceFiles)
            {
                sourceFile = entry.SourceFiles[FileNumber];
            }
        }

        std::fprintf(stderr, "ABL: line %d of %s, statement %d, call depth %d\n", ExecLineNumber, sourceFile,
                     ExecStatementCount, CallStackLevel);
    }
}

int main(int argc, char** argv)
{
    MCConsole::AttachParent();

    if (const auto allocator = MCInitializeAllocator(); !allocator)
    {
        Report(allocator.error(), true);
        return 1;
    }

    MCCrashTrace::Install();
    MCCrashTrace::SetReporter(ReportAblState);
    auto options = ParseArguments(argc, argv);

    if (!options)
    {
        if (options.error().empty())
        {
            Report(UsageText, false);
            return 0;
        }

        // On a terminal the usage follows the error; a message box only needs the error.
        Report(MCConsole::HasOutput() ? std::format("{}\n\n{}", options.error(), UsageText) : options.error(), true);
        return 1;
    }

    MCFileSystem::SetGameRoot(options->DataFolder);
    MCFileSystem::SetUserRoot(options->UserFolder);
    SDL_Log("Game data: %s", options->DataFolder.string().c_str());
    SDL_Log("User data: %s", options->UserFolder.string().c_str());

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
    {
        Report(std::format("SDL_Init failed: {}", SDL_GetError()), true);
        return 1;
    }

    std::vector<char> commandLine(options->GameCommandLine.begin(), options->GameCommandLine.end());
    commandLine.push_back('\0');
    const int result = WinMain(nullptr, nullptr, commandLine.data(), 1);
    SDL_Quit();
    return result;
}
