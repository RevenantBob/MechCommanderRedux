// Built without the precompiled header: MCCore's Win32 stand-ins (MCWin32Defs.h) and Windows.h cannot meet.
#include "MCCrashTrace.h"
#include "MCConsole.h"

#include <cstdio>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace
{
    /// <summary>The game-state printer set by <see cref="MCCrashTrace::SetReporter"/>, or null.</summary>
    void (*Reporter)() = nullptr;

    /// <summary>Runs the reporter, if any. The state it reads may be what broke, so a fault inside it is caught.</summary>
    void RunReporter()
    {
        if (Reporter == nullptr)
        {
            return;
        }

        __try
        {
            Reporter();
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            std::fprintf(stderr, "\n(the reporter faulted)\n");
        }

        std::fflush(stderr);
    }

    /// <summary>
    /// Fills <paramref name="path"/> with the executable's path, its extension replaced by
    /// <paramref name="extension"/>. False when the path doesn't fit.
    /// </summary>
    bool GetPathBesideExecutable(wchar_t (&path)[MAX_PATH], const wchar_t* extension)
    {
        const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

        if (length == 0 || length >= MAX_PATH)
        {
            return false;
        }

        wchar_t* dot = wcsrchr(path, L'.');

        if (dot == nullptr)
        {
            return false;
        }

        return wcscpy_s(dot, MAX_PATH - (dot - path), extension) == 0;
    }

    /// <summary>Writes MCRedux.dmp beside the executable: threads, stacks and the data segments.</summary>
    void WriteDump(EXCEPTION_POINTERS* exception)
    {
        wchar_t path[MAX_PATH];

        if (!GetPathBesideExecutable(path, L".dmp"))
        {
            return;
        }

        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = exception;
        info.ClientPointers = FALSE;
        const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                                                     MiniDumpWithThreadInfo);

        if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr))
        {
            std::fprintf(stderr, "Minidump: %ls\n", path);
        }

        CloseHandle(file);
    }

    /// <summary>Prints one stack frame: the function and source line when the PDB has them, else the module offset.</summary>
    void PrintFrame(HANDLE process, int index, DWORD64 address)
    {
        alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        const bool haveSymbol = SymFromAddr(process, address, &displacement, symbol) != FALSE;
        const bool haveLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &line) != FALSE;

        if (haveSymbol && haveLine)
        {
            std::fprintf(stderr, "  #%02d %s+0x%llx  %s(%lu)\n", index, symbol->Name,
                         static_cast<unsigned long long>(displacement), line.FileName, line.LineNumber);
        }
        else if (haveSymbol)
        {
            std::fprintf(stderr, "  #%02d %s+0x%llx\n", index, symbol->Name,
                         static_cast<unsigned long long>(displacement));
        }
        else
        {
            const DWORD64 base = SymGetModuleBase64(process, address);
            std::fprintf(stderr, "  #%02d 0x%llx (module 0x%llx + 0x%llx)\n", index,
                         static_cast<unsigned long long>(address), static_cast<unsigned long long>(base),
                         static_cast<unsigned long long>(address - base));
        }
    }

    LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception)
    {
        // Started without a terminal, stderr goes nowhere: the report goes to MCRedux.crash.txt instead.
        wchar_t reportPath[MAX_PATH];
        bool toFile = false;

        if (!MCConsole::HasOutput() && GetPathBesideExecutable(reportPath, L".crash.txt"))
        {
            FILE* stream = nullptr;
            toFile = _wfreopen_s(&stream, reportPath, L"w", stderr) == 0;
        }

        const EXCEPTION_RECORD* record = exception->ExceptionRecord;
        std::fprintf(stderr, "\nCrash: exception 0x%08lx at 0x%p", record->ExceptionCode, record->ExceptionAddress);

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
        {
            std::fprintf(stderr, " (%s 0x%llx)", record->ExceptionInformation[0] == 1 ? "writing" : "reading",
                         static_cast<unsigned long long>(record->ExceptionInformation[1]));
        }

        std::fprintf(stderr, "\n");

        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);

        CONTEXT context = *exception->ContextRecord;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        for (int index = 0; index < 48; ++index)
        {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            {
                break;
            }

            if (frame.AddrPC.Offset == 0)
            {
                break;
            }

            PrintFrame(process, index, frame.AddrPC.Offset);
        }

        std::fflush(stderr);
        SymCleanup(process);

        RunReporter();

        WriteDump(exception);

        if (toFile)
        {
            std::fflush(stderr);
            wchar_t message[MAX_PATH + 64];
            swprintf_s(message, L"MCRedux crashed. The report is in\n%ls", reportPath);
            MessageBoxW(nullptr, message, L"MCRedux", MB_OK | MB_ICONERROR);
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void MCCrashTrace::Install()
{
    SetUnhandledExceptionFilter(OnUnhandledException);
}

void MCCrashTrace::SetReporter(void (*reporter)())
{
    Reporter = reporter;
}

#else

void MCCrashTrace::Install()
{
}

void MCCrashTrace::SetReporter(void (*)())
{
}

#endif
