// Built without the precompiled header: MCCore's Win32 stand-ins (MCWin32Defs.h) and Windows.h cannot meet.
#include "MCCrashTrace.h"
#include "MCConsole.h"

#include <cstdarg>
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

    /// <summary>
    /// The thread that writes the report, made at <see cref="MCCrashTrace::Install"/> so a crash needs no stack, heap or
    /// thread creation of its own: the crashed thread may have overflowed its stack or broken the heap.
    /// </summary>
    HANDLE WorkerThread = nullptr;

    /// <summary>Set by the crashed thread to start the worker.</summary>
    HANDLE CrashEvent = nullptr;

    /// <summary>Set by the worker when the report and the dump are written.</summary>
    HANDLE DoneEvent = nullptr;

    /// <summary>The crash being reported, for the worker.</summary>
    EXCEPTION_POINTERS* CrashException = nullptr;

    /// <summary>The id of the thread that crashed.</summary>
    DWORD CrashThreadId = 0;

    /// <summary>Nonzero once a thread has entered the handler; a second one waits for the first to end the process.</summary>
    volatile LONG Handling = 0;

    /// <summary>Where the report goes: stderr's handle, or MCRedux.crash.txt.</summary>
    HANDLE ReportFile = INVALID_HANDLE_VALUE;

    /// <summary>The report's path when it goes to a file, else empty.</summary>
    wchar_t ReportPath[MAX_PATH] = {};

    /// <summary>Formats into a static buffer and writes it to the report, without touching the heap.</summary>
    void Print(const char* format, ...)
    {
        static char buffer[4096];
        va_list args;
        va_start(args, format);
        const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (length <= 0 || ReportFile == INVALID_HANDLE_VALUE)
        {
            return;
        }

        const DWORD size = static_cast<DWORD>(length < static_cast<int>(sizeof(buffer)) ? length : sizeof(buffer) - 1);
        DWORD written = 0;
        WriteFile(ReportFile, buffer, size, &written, nullptr);
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

    /// <summary>
    /// Writes MCRedux.dmp beside the executable: threads, stacks and the data segments. Returns whether it was written;
    /// <paramref name="path"/> gets its path.
    /// </summary>
    bool WriteDump(wchar_t (&path)[MAX_PATH])
    {
        if (!GetPathBesideExecutable(path, L".dmp"))
        {
            return false;
        }

        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId = CrashThreadId;
        info.ExceptionPointers = CrashException;
        info.ClientPointers = FALSE;
        const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                                                     MiniDumpWithThreadInfo);
        const bool written =
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &info, nullptr, nullptr) != FALSE;
        CloseHandle(file);
        return written;
    }

    /// <summary>Prints one stack frame: the function and source line when the PDB has them, else the module offset.</summary>
    void PrintFrame(HANDLE process, int index, DWORD64 address)
    {
        alignas(SYMBOL_INFO) static char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
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
            Print("  #%02d %s+0x%llx  %s(%lu)\n", index, symbol->Name, static_cast<unsigned long long>(displacement),
                  line.FileName, line.LineNumber);
        }
        else if (haveSymbol)
        {
            Print("  #%02d %s+0x%llx\n", index, symbol->Name, static_cast<unsigned long long>(displacement));
        }
        else
        {
            const DWORD64 base = SymGetModuleBase64(process, address);
            Print("  #%02d 0x%llx (module 0x%llx + 0x%llx)\n", index, static_cast<unsigned long long>(address),
                  static_cast<unsigned long long>(base), static_cast<unsigned long long>(address - base));
        }
    }

    /// <summary>Prints the exception and the crashed thread's symbolized stack.</summary>
    void PrintStack()
    {
        const EXCEPTION_RECORD* record = CrashException->ExceptionRecord;
        Print("\nCrash: exception 0x%08lx at 0x%p on thread %lu", record->ExceptionCode, record->ExceptionAddress,
              CrashThreadId);

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
        {
            Print(" (%s 0x%llx)", record->ExceptionInformation[0] == 1 ? "writing" : "reading",
                  static_cast<unsigned long long>(record->ExceptionInformation[1]));
        }
        else if (record->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
        {
            Print(" (stack overflow)");
        }

        Print("\n");

        HANDLE process = GetCurrentProcess();
        HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, CrashThreadId);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        SymInitialize(process, nullptr, TRUE);

        static CONTEXT context;
        context = *CrashException->ContextRecord;
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        // A stack overflow is deep recursion: the top frames show where it loops, the bottom ones how it got there.
        const int maxFrames = record->ExceptionCode == EXCEPTION_STACK_OVERFLOW ? 4096 : 64;

        for (int index = 0; index < maxFrames; ++index)
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

        SymCleanup(process);

        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }

    /// <summary>Runs the reporter, if any, its stderr going where the report goes.</summary>
    void RunReporter()
    {
        if (Reporter == nullptr)
        {
            return;
        }

        if (ReportPath[0] != L'\0')
        {
            // The reporter prints with stdio: point stderr at the end of the report.
            FILE* stream = nullptr;

            if (_wfreopen_s(&stream, ReportPath, L"a", stderr) != 0)
            {
                return;
            }
        }

        Reporter();
        std::fflush(stderr);
    }

    /// <summary>
    /// Writes the dump, then the report, then runs the reporter, each step guarded: a fault in one (the state it reads
    /// may be what broke) costs only what comes after it in that step.
    /// </summary>
    void WriteCrashReport()
    {
        wchar_t dumpPath[MAX_PATH] = {};
        bool dumped = false;

        __try
        {
            dumped = WriteDump(dumpPath);
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }

        // Started without a terminal, stderr goes nowhere: the report goes to MCRedux.crash.txt instead.
        if (!MCConsole::HasOutput() && GetPathBesideExecutable(ReportPath, L".crash.txt"))
        {
            ReportFile = CreateFileW(ReportPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (ReportFile == INVALID_HANDLE_VALUE)
            {
                ReportPath[0] = L'\0';
            }
        }
        else
        {
            ReportPath[0] = L'\0';
            std::fflush(stderr);
            ReportFile = GetStdHandle(STD_ERROR_HANDLE);
        }

        __try
        {
            PrintStack();
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Print("\n(the stack walk faulted)\n");
        }

        if (dumped)
        {
            Print("Minidump: %ls\n", dumpPath);
        }
        else
        {
            Print("Minidump: not written (error %lu)\n", GetLastError());
        }

        if (ReportPath[0] != L'\0')
        {
            CloseHandle(ReportFile);
            ReportFile = INVALID_HANDLE_VALUE;
        }

        __try
        {
            RunReporter();
        }

        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (ReportPath[0] != L'\0')
            {
                ReportFile = CreateFileW(ReportPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            }

            Print("\n(the reporter faulted)\n");

            if (ReportPath[0] != L'\0' && ReportFile != INVALID_HANDLE_VALUE)
            {
                CloseHandle(ReportFile);
            }
        }

        if (ReportPath[0] != L'\0')
        {
            wchar_t message[MAX_PATH + 64];
            swprintf_s(message, L"MCRedux crashed. The report is in\n%ls", ReportPath);
            MessageBoxW(nullptr, message, L"MCRedux", MB_OK | MB_ICONERROR | MB_TOPMOST);
        }
    }

    /// <summary>The worker: sleeps until a crash, then writes its report.</summary>
    DWORD WINAPI CrashWorker(void*)
    {
        WaitForSingleObject(CrashEvent, INFINITE);
        WriteCrashReport();
        SetEvent(DoneEvent);
        return 0;
    }

    LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception)
    {
        // This runs on the crashed thread, maybe at the end of its stack: it only hands the crash to the worker.
        if (InterlockedCompareExchange(&Handling, 1, 0) != 0)
        {
            // Another thread crashed first (or this one faulted again): its report ends the process.
            Sleep(INFINITE);
        }

        CrashException = exception;
        CrashThreadId = GetCurrentThreadId();

        if (WorkerThread != nullptr)
        {
            SetEvent(CrashEvent);
            WaitForSingleObject(DoneEvent, INFINITE);
        }
        else
        {
            WriteCrashReport();
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void MCCrashTrace::Install()
{
    CrashEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (CrashEvent != nullptr && DoneEvent != nullptr)
    {
        WorkerThread =
            CreateThread(nullptr, 1024 * 1024, CrashWorker, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    }

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
