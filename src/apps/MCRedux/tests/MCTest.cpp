#include "MCTest.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace
{
    struct RunState
    {
        std::vector<std::string> Scopes;
        size_t Passes = 0;
        size_t Failures = 0;
    };

    RunState& State()
    {
        static RunState state;
        return state;
    }

    std::string Lower(std::string_view text)
    {
        std::string lower(text);
        std::ranges::transform(lower, lower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    /// <summary>Whether a test runs: no filters, or its name holds any filter, ignoring case.</summary>
    bool Selected(const MCTest::TestCase& test, const std::vector<std::string>& filters)
    {
        if (filters.empty())
        {
            return true;
        }

        const std::string name = Lower(test.Name);
        return std::ranges::any_of(filters,
                                   [&](const std::string& filter) { return name.find(filter) != std::string::npos; });
    }
}

namespace MCTest
{
    std::vector<TestCase>& Registry()
    {
        static std::vector<TestCase> registry;
        return registry;
    }

    Registrar::Registrar(const char* name, void (*body)(), const char* file, int line, bool isolated)
    {
        Registry().push_back({name, body, file, line, isolated});
    }

    Scope::Scope(std::string text)
    {
        State().Scopes.push_back(std::move(text));
    }

    Scope::~Scope()
    {
        State().Scopes.pop_back();
    }

    void ReportFailure(const char* file, int line, std::string_view message)
    {
        RunState& state = State();
        ++state.Failures;
        std::cout << "  " << file << "(" << line << "): " << message << "\n";

        for (const std::string& scope : state.Scopes)
        {
            std::cout << "    in " << scope << "\n";
        }
    }

    void ReportPass()
    {
        ++State().Passes;
    }
}

namespace
{
    /// <summary>
    /// Runs an isolated test in a process of its own: this program again, with <c>--only</c> and the test's name.
    /// </summary>
    /// <returns>Whether it passed (the child exited 0).</returns>
    bool RunIsolated(const char* program, const MCTest::TestCase& test)
    {
        std::string command = std::format("\"{}\" --only \"{}\"", program, test.Name);
#ifdef _WIN32
        // cmd.exe drops the first and last quote of a command that starts with one.
        command = "\"" + command + "\"";
#endif
        std::cout << std::flush;
        return std::system(command.c_str()) == 0;
    }
}

/// <summary>
/// Runs the registered tests. Arguments are name filters (a test runs when its name holds any of them, ignoring case);
/// <c>--list</c> prints the names instead, and <c>--only &lt;name&gt;</c> runs the one test of exactly that name in this
/// process (how isolated tests are run). Exits 0 when every selected test passed.
/// </summary>
int main(int argc, char** argv)
{
    std::vector<std::string> filters;
    bool list = false;
    const char* only = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        if (arg == "--list")
        {
            list = true;
        }
        else if (arg == "--only" && i + 1 < argc)
        {
            only = argv[++i];
        }
        else
        {
            filters.push_back(Lower(arg));
        }
    }

    std::vector<const MCTest::TestCase*> selected;

    for (const MCTest::TestCase& test : MCTest::Registry())
    {
        if (only != nullptr ? std::string_view(test.Name) == only : Selected(test, filters))
        {
            selected.push_back(&test);
        }
    }

    if (list)
    {
        for (const MCTest::TestCase* test : selected)
        {
            std::cout << test->Name << "\n";
        }

        return 0;
    }

    RunState& state = State();
    std::vector<const MCTest::TestCase*> failed;
    const auto runStart = std::chrono::steady_clock::now();

    for (const MCTest::TestCase* test : selected)
    {
        if (test->Isolated && only == nullptr)
        {
            std::cout << "[ PROC ] " << test->Name << std::endl;

            if (!RunIsolated(argv[0], *test))
            {
                ++state.Failures;
                failed.push_back(test);
            }

            continue;
        }

        std::cout << "[ RUN  ] " << test->Name << std::endl;
        const size_t failuresBefore = state.Failures;
        const auto start = std::chrono::steady_clock::now();

        try
        {
            test->Body();
        }
        catch (const MCTest::RequireFailed&)
        {
        }
        catch (const std::exception& e)
        {
            MCTest::ReportFailure(test->File, test->Line, std::string("uncaught exception: ") + e.what());
        }
        catch (...)
        {
            MCTest::ReportFailure(test->File, test->Line, "uncaught exception");
        }

        state.Scopes.clear();
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        const bool passed = state.Failures == failuresBefore;

        if (!passed)
        {
            failed.push_back(test);
        }

        std::cout << (passed ? "[   OK ] " : "[ FAIL ] ") << test->Name << " (" << ms << " ms)" << std::endl;
    }

    const auto totalMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - runStart).count();
    std::cout << "\n"
              << selected.size() - failed.size() << " of " << selected.size() << " tests passed, " << state.Passes
              << " checks passed, " << state.Failures << " failed (" << totalMs << " ms)\n";

    for (const MCTest::TestCase* test : failed)
    {
        std::cout << "  FAILED: " << test->Name << "\n";
    }

    return failed.empty() ? 0 : 1;
}
