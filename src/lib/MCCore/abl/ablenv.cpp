#include "stdafx.h"
#include "abl/ablenv.h"
#include "main/fixes.h"
#include "abl/abldbug.h"
#include "abl/MCAblErrors.h"
#include "abl/ablexec.h"
#include "abl/MCAblCompiler.h"
#include "abl/ablrtn.h"
#include "abl/MCAblScanner.h"
#include "abl/MCAblSymbolTable.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"

int32_t MaxWatchesPerModule = 20;
int32_t MaxBreakPointsPerModule = 20;
MCModuleEntry* ModuleRegistry = nullptr;
int32_t MaxModules = 0;
int32_t NumModulesRegistered = 0;
int32_t NumModules = 0;
MCAblModule** ModuleInstanceRegistry = nullptr;
int32_t NumModuleInstances = 0;
MCAblModule** LibraryInstanceRegistry = nullptr;
int32_t MaxLibraries = 0;
int32_t NumLibrariesLoaded = 0;
MCAblModule* CurModule = nullptr;
int32_t CurModuleHandle = 0;
int32_t CallStackLevel = 0;
int CallModuleInit = 0;
int32_t EternalOffset = 0;
int32_t NumExecutions = 0;
MCFile* ProfileLog = nullptr;
char ProfileLogBuffer[MAX_PROFILE_LOG_LINES][MAX_PROFILE_LOG_LINE_LENGTH];
int32_t NumProfileLogLines = 0;
int32_t TotalProfileLogLines = 0;

namespace
{
    /// <summary>
    /// Starts a module execution: makes <paramref name="module"/> current, pushes the module's frame header and its
    /// parameters from <paramref name="paramList"/>.
    /// </summary>
    /// <returns>The module symbol, or null when a parameter doesn't match its type (the execution is abandoned).</returns>
    /// <remarks>Both ABLModule::execute overloads start with this (inline in the original).</remarks>
    auto BeginModuleExecution(MCAblModule* module, MCAblParam* paramList) -> MCAblSymbol*
    {
        CurModule = module;

        if (Debugger)
        {
            Debugger->SetModule(module);
        }

        StaticDataPtr = module->StaticData;
        CurRoutineIdPtr = nullptr;
        MCAblSymbol* moduleIdPtr = ModuleRegistry[module->Handle].ModuleIdPtr;
        NumExecutions++;
        ExecFileNumber = -1;
        Tos = Stack + EternalOffset;
        StackFrameBasePtr = Tos + 1;
        ExecStatementCount = 0;
        Level = 1;
        CallStackLevel = 0;

        PushInteger(0);
        PushAddress(nullptr);
        PushAddress(nullptr);
        PushAddress(nullptr);

        if (paramList)
        {
            MCAblParam* param = paramList;

            for (MCAblSymbol* formalIdPtr = moduleIdPtr->Defn.Info.Routine.Params; formalIdPtr;
                 formalIdPtr = formalIdPtr->Next, param++)
            {
                MCAblType* formalTypePtr = formalIdPtr->TypePtr;

                if (formalIdPtr->Defn.Key == MCAblSymbolKind::ValueParam)
                {
                    if (formalTypePtr == RealTypePtr)
                    {
                        if (param->Type == ABL_PARAM_INTEGER)
                        {
                            PushReal(static_cast<float>(param->Integer));
                        }
                        else if (param->Type == ABL_PARAM_REAL)
                        {
                            PushReal(param->Real);
                        }
                    }
                    else if (formalTypePtr == IntegerTypePtr)
                    {
                        if (param->Type != ABL_PARAM_INTEGER)
                        {
                            return nullptr;
                        }

                        PushInteger(param->Integer);
                    }

                    // Faithful: nothing was pushed for an array parameter, so this copies the block the top item
                    // points to.
                    if (formalTypePtr->Form == MCAblTypeForm::Array)
                    {
                        int32_t size = formalTypePtr->Size;
                        MCAddress copy = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

                        // An empty array got no block from the heap, which was fatal.
                        if (!copy)
                        {
                            char err[256];
                            std::snprintf(err, sizeof(err),
                                          "ABL: Unable to AblStackHeap->malloc array parameter [Module %d]",
                                          module->Id);
                            Fatal(0, err);
                        }

                        MCAddress source = Tos->Address;
                        Tos->Address = copy;
                        std::memcpy(copy, source, static_cast<size_t>(size));
                    }
                }
                else
                {
                    MCAddress paramAddress;

                    if (formalTypePtr == RealTypePtr)
                    {
                        paramAddress = reinterpret_cast<MCAddress>(&param->Real);
                    }
                    else if (formalTypePtr == IntegerTypePtr)
                    {
                        paramAddress = reinterpret_cast<MCAddress>(&param->Integer);
                    }
                    else
                    {
                        return nullptr;
                    }

                    PushAddress(paramAddress);
                }
            }
        }

        return moduleIdPtr;
    }

    /// <summary>
    /// <paramref name="name"/> lower-cased, as the symbol tables hold it. Port fix: the original lower-cased the
    /// caller's string in place, which the callers' literals ("handlemessage", the pilot alarm names) allowed only
    /// because MSVC 5 kept literals in writable data; the port lower-cases a copy.
    /// </summary>
    auto LowerCaseName(const char* name) -> std::string
    {
        std::string lower(name);
        MCPort::StrLwr(lower.data());
        return lower;
    }

    /// <summary>Finds <paramref name="name"/> (lower-cased) among the globals of the libraries <paramref name="entry"/> uses.</summary>
    auto SearchLibrariesUsed(const MCModuleEntry& entry, char* name) -> MCAblSymbol*
    {
        std::string lower = LowerCaseName(name);

        for (int32_t i = 0; i < entry.NumLibrariesUsed; i++)
        {
            MCAblSymbol* libraryIdPtr = ModuleRegistry[entry.LibrariesUsed[i]->Handle].ModuleIdPtr;
            MCAblSymbol* symbol = SearchSymTable(lower.data(), libraryIdPtr->Defn.Info.Routine.LocalSymTable);

            if (symbol)
            {
                return symbol;
            }
        }

        return nullptr;
    }
}

auto DumpProfileLog() -> void
{
    for (int32_t i = 0; i < NumProfileLogLines; i++)
    {
        ProfileLog->WriteString(ProfileLogBuffer[i]);
    }

    NumProfileLogLines = 0;
}

auto AblCloseProfileLog() -> void
{
    if (ProfileLog)
    {
        DumpProfileLog();
        char line[512];
        std::snprintf(line, sizeof(line), "\nNum Total Lines = %d\n", TotalProfileLogLines);
        ProfileLog->WriteString(line);
        ProfileLog->Close();
        delete ProfileLog;
        ProfileLog = nullptr;
        NumProfileLogLines = 0;
        TotalProfileLogLines = 0;
    }
}

auto AblOpenProfileLog() -> void
{
    if (ProfileLog)
    {
        AblCloseProfileLog();
    }

    NumProfileLogLines = 0;
    ProfileLog = new MCFile;

    if (!ProfileLog)
    {
        Fatal(0, " unable to malloc ABL ProfileLog ");
    }

    if (ProfileLog->Create("abl.log") != 0)
    {
        Fatal(0, " unable to create ABL ProfileLog ");
    }
}

auto AblAddToProfileLog(char* profileEntry) -> void
{
    if (NumProfileLogLines == MAX_PROFILE_LOG_LINES)
    {
        DumpProfileLog();
    }

    std::strncpy(ProfileLogBuffer[NumProfileLogLines], profileEntry, MAX_PROFILE_LOG_LINE_LENGTH - 1);
    ProfileLogBuffer[NumProfileLogLines][MAX_PROFILE_LOG_LINE_LENGTH - 1] = '\0';
    NumProfileLogLines++;
    TotalProfileLogLines++;
}

auto InitModuleRegistry(int32_t maxModules) -> void
{
    MaxModules = maxModules;
    ModuleRegistry = AblMemory.AllocateArray<MCModuleEntry>(static_cast<size_t>(maxModules));
    ModuleInstanceRegistry = AblMemory.AllocateArray<MCAblModule*>(static_cast<size_t>(MaxModules));
}

auto DestroyModuleRegistry() -> void
{
    if (!ModuleRegistry)
    {
        return;
    }

    for (int32_t i = 0; i < NumModulesRegistered; i++)
    {
        MCModuleEntry& entry = ModuleRegistry[i];
        AblMemory.Free(entry.FileName);
        entry.FileName = nullptr;
        entry.ModuleIdPtr = nullptr;

        // Port fix: frees module i's source file names. The original indexed the registry with the file counter
        // (ModuleRegistry[j].sourceFiles[j], while j < ModuleRegistry[j].numSourceFiles), freeing the wrong names
        // and reading past the used entries. Only frees memory: ABLi_close clears the rest right after.
        for (int32_t j = 0; j < entry.NumSourceFiles; j++)
        {
            AblMemory.Free(entry.SourceFiles[j]);
            entry.SourceFiles[j] = nullptr;
        }
    }

    AblMemory.Free(ModuleRegistry);
    ModuleRegistry = nullptr;
    AblMemory.Free(ModuleInstanceRegistry);
    ModuleInstanceRegistry = nullptr;
}

auto InitLibraryRegistry(int32_t maxLibraries) -> void
{
    MaxLibraries = maxLibraries;
    LibraryInstanceRegistry = AblMemory.AllocateArray<MCAblModule*>(static_cast<size_t>(maxLibraries));
}

auto DestroyLibraryRegistry() -> void
{
    if (!LibraryInstanceRegistry)
    {
        return;
    }

    for (int32_t i = 0; i < NumLibrariesLoaded; i++)
    {
        MCAblModule* library = LibraryInstanceRegistry[i];

        if (library)
        {
            library->Destroy();
            delete library;
        }

        LibraryInstanceRegistry[i] = nullptr;
    }

    AblMemory.Free(LibraryInstanceRegistry);
    LibraryInstanceRegistry = nullptr;
}

auto MCAblModule::Init(int32_t moduleHandle) -> int32_t
{
    Handle = moduleHandle;
    Id = NumModules++;
    StaticData = nullptr;

    // One item per static; a static array's item points to its own block.
    const MCModuleEntry& entry = ModuleRegistry[moduleHandle];
    int32_t numStatics = entry.NumStaticVars;

    if (numStatics != 0)
    {
        StaticData = AblMemory.AllocateArray<MCStackItem>(static_cast<size_t>(numStatics));

        for (int32_t i = 0; i < numStatics; i++)
        {
            int32_t size = entry.SizeStaticVars[i];
            StaticData[i] = MCStackItem{};

            if (size > 0)
            {
                StaticData[i].Address = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

#if !MCREDUX_FIX_ABL_UNINITIALIZED_STATICS
                std::memset(StaticData[i].Address, 0xff, static_cast<size_t>(size));
#endif
            }
        }
    }

    ModuleInstanceRegistry[NumModuleInstances++] = this;
    ModuleRegistry[moduleHandle].NumInstances++;
    InitCalled = 0;

    if (Debugger)
    {
        WatchManager = new MCWatchManager;

        if (WatchManager->Init(MaxWatchesPerModule) != 0)
        {
            Fatal(0, " Unable to AblStackHeap->malloc WatchManager ");
        }

        BreakPointManager = new MCBreakPointManager;

        if (BreakPointManager->Init(MaxBreakPointsPerModule) != 0)
        {
            Fatal(0, " Unable to AblStackHeap->malloc BreakPointManager ");
        }
    }

    return 0;
}

auto MCAblModule::Execute(MCAblParam* paramList) -> int32_t
{
    MCAblSymbol* moduleIdPtr = BeginModuleExecution(this, paramList);

    if (!moduleIdPtr)
    {
        return 0;
    }

    CurModuleHandle = Handle;
    CallModuleInit = InitCalled == 0;
    InitCalled = 1;
    ::Execute(moduleIdPtr);
    ReturnVal = ReturnValue.Integer;
    return ExecStatementCount;
}

auto MCAblModule::Execute(MCAblParam* moduleParamList, MCAblSymbol* function, MCAblParam* functionParamList) -> int32_t
{
    MCAblSymbol* moduleIdPtr = BeginModuleExecution(this, moduleParamList);

    if (!moduleIdPtr)
    {
        return 0;
    }

    CurModuleHandle = Handle;
    int32_t wasInitCalled = InitCalled;
    InitCalled = 1;
    CallModuleInit = wasInitCalled == 0;
    ExecuteChild(moduleIdPtr, function, functionParamList);
    ReturnVal = ReturnValue.Integer;
    return ExecStatementCount;
}

auto MCAblModule::FindSymbol(char* symbolName, MCAblSymbol* function, int searchLibraries) -> MCAblSymbol*
{
    std::string lower = LowerCaseName(symbolName);

    if (function)
    {
        MCAblSymbol* symbol = SearchSymTable(lower.data(), function->Defn.Info.Routine.LocalSymTable);

        if (symbol)
        {
            return symbol;
        }
    }

    const MCModuleEntry& entry = ModuleRegistry[Handle];
    MCAblSymbol* symbol = SearchSymTable(lower.data(), entry.ModuleIdPtr->Defn.Info.Routine.LocalSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = SearchLibrariesUsed(entry, symbolName);
    }

    return symbol;
}

auto MCAblModule::FindFunction(char* functionName, int searchLibraries) -> MCAblSymbol*
{
    // The module's own table is searched with the name as given (not lower-cased).
    const MCModuleEntry& entry = ModuleRegistry[Handle];
    MCAblSymbol* symbol = SearchSymTable(functionName, entry.ModuleIdPtr->Defn.Info.Routine.LocalSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = SearchLibrariesUsed(entry, functionName);
    }

    return symbol;
}

auto MCAblModule::SetStaticInteger(char* staticName, int32_t value) -> int32_t
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->TypePtr != IntegerTypePtr)
    {
        return 2;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return 3;
    }

    StaticData[idPtr->Defn.Info.Data.Offset].Integer = value;
    return 0;
}

auto MCAblModule::SetStaticReal(char* staticName, float value) -> int32_t
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->TypePtr != RealTypePtr)
    {
        return 2;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return 3;
    }

    StaticData[idPtr->Defn.Info.Data.Offset].Real = value;
    return 0;
}

auto MCAblModule::SetStaticIntegerArray(char* staticName, int32_t size, int32_t* values) -> int32_t
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return 3;
    }

    std::memcpy(StaticData[idPtr->Defn.Info.Data.Offset].Address, values, static_cast<size_t>(size) * sizeof(int32_t));
    return 0;
}

auto MCAblModule::SetStaticRealArray(char* staticName, int32_t size, float* values) -> int32_t
{
    MCAblSymbol* idPtr = FindSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->Defn.Info.Data.VarType != MCAblStorage::Static)
    {
        return 3;
    }

    std::memcpy(StaticData[idPtr->Defn.Info.Data.Offset].Address, values, static_cast<size_t>(size) * sizeof(float));
    return 0;
}

auto MCAblModule::GetSourceFile(int32_t fileNumber) -> char*
{
    return ModuleRegistry[Handle].SourceFiles[fileNumber];
}

auto MCAblModule::GetSourceDirectory(int32_t fileNumber, char* directory) -> char*
{
    char* fileName = ModuleRegistry[Handle].SourceFiles[fileNumber];
    int32_t curChar = static_cast<int32_t>(std::strlen(fileName)) - 1;

    while (curChar > -1 && fileName[curChar] != '\\')
    {
        curChar--;
    }

    if (curChar == -1)
    {
        return nullptr;
    }

    std::strcpy(directory, fileName);
    directory[curChar + 1] = '\0';
    return directory;
}

auto MCAblModule::GetInfo(int32_t& numStatics, int32_t& staticsSize, int32_t* sizeList) -> void
{
    const MCModuleEntry& entry = ModuleRegistry[Handle];
    numStatics = entry.NumStaticVars;
    staticsSize = entry.TotalSizeStaticVars;

    if (sizeList)
    {
        for (int32_t i = 0; i < numStatics; i++)
        {
            sizeList[i] = entry.SizeStaticVars[i];
        }
    }
}

auto MCAblModule::Destroy() -> void
{
    if (Id > -1 && ModuleInstanceRegistry)
    {
        for (int32_t i = 0; i < NumModuleInstances; i++)
        {
            if (ModuleInstanceRegistry[i] == this)
            {
                NumModuleInstances--;
                ModuleInstanceRegistry[i] = ModuleInstanceRegistry[NumModuleInstances];
                ModuleInstanceRegistry[NumModuleInstances] = nullptr;
                break;
            }
        }
    }

    if (WatchManager)
    {
        WatchManager->Destroy();
        delete WatchManager;
        WatchManager = nullptr;
    }

    if (BreakPointManager)
    {
        BreakPointManager->Destroy();
        delete BreakPointManager;
        BreakPointManager = nullptr;
    }

    // Faithful: the static arrays' own blocks are not freed.
    if (StaticData)
    {
        AblMemory.Free(StaticData);
        StaticData = nullptr;
    }
}
