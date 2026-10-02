#include "stdafx.h"
#include "abl/ablenv.h"
#include "main/fixes.h"
#include "abl/abldbug.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "lib/heap.h"

int32_t MaxWatchesPerModule = 20;
int32_t MaxBreakPointsPerModule = 20;
ModuleEntry* ModuleRegistry = nullptr;
int32_t MaxModules = 0;
int32_t NumModulesRegistered = 0;
int32_t NumModules = 0;
ABLModule** ModuleInstanceRegistry = nullptr;
int32_t NumModuleInstances = 0;
ABLModule** LibraryInstanceRegistry = nullptr;
int32_t MaxLibraries = 0;
int32_t numLibrariesLoaded = 0;
ABLModule* CurModule = nullptr;
int32_t CurModuleHandle = 0;
ABLModule* CurLibrary = nullptr;
int32_t CallStackLevel = 0;
int CallModuleInit = 0;
int32_t eternalOffset = 0;
int32_t NumExecutions = 0;
File* ProfileLog = nullptr;
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
    auto beginModuleExecution(ABLModule* module, ABLParam* paramList) -> SymTableNodePtr
    {
        CurModule = module;

        if (debugger)
        {
            debugger->setModule(module);
        }

        StaticDataPtr = module->staticData;
        CurModuleIdPtr = nullptr;
        CurRoutineIdPtr = nullptr;
        SymTableNodePtr moduleIdPtr = ModuleRegistry[module->handle].moduleIdPtr;
        NumExecutions++;
        FileNumber = -1;
        tos = stack + eternalOffset;
        errorCount = 0;
        stackFrameBasePtr = tos + 1;
        execStatementCount = 0;
        level = 1;
        CallStackLevel = 0;

        pushInteger(0);
        pushAddress(nullptr);
        pushAddress(nullptr);
        pushAddress(nullptr);

        if (paramList)
        {
            ABLParam* param = paramList;

            for (SymTableNodePtr formalIdPtr = moduleIdPtr->defn.info.routine.params; formalIdPtr;
                 formalIdPtr = formalIdPtr->next, param++)
            {
                TypePtr formalTypePtr = formalIdPtr->typePtr;

                if (formalIdPtr->defn.key == DFN_VALPARAM)
                {
                    if (formalTypePtr == RealTypePtr)
                    {
                        if (param->type == ABL_PARAM_INTEGER)
                        {
                            pushReal(static_cast<float>(param->integer));
                        }
                        else if (param->type == ABL_PARAM_REAL)
                        {
                            pushReal(param->real);
                        }
                    }
                    else if (formalTypePtr == IntegerTypePtr)
                    {
                        if (param->type != ABL_PARAM_INTEGER)
                        {
                            return nullptr;
                        }

                        pushInteger(param->integer);
                    }

                    // Faithful: nothing was pushed for an array parameter, so this copies the block the top item
                    // points to.
                    if (formalTypePtr->form == FRM_ARRAY)
                    {
                        int32_t size = formalTypePtr->size;
                        Address copy = static_cast<Address>(AblStackHeap->malloc(size));

                        if (!copy)
                        {
                            char err[256];
                            std::snprintf(err, sizeof(err),
                                          "ABL: Unable to AblStackHeap->malloc array parameter [Module %d]",
                                          module->id);
                            Fatal(0, err);
                        }

                        Address source = tos->address;
                        tos->address = copy;
                        std::memcpy(copy, source, static_cast<size_t>(size));
                    }
                }
                else
                {
                    Address paramAddress;

                    if (formalTypePtr == RealTypePtr)
                    {
                        paramAddress = reinterpret_cast<Address>(&param->real);
                    }
                    else if (formalTypePtr == IntegerTypePtr)
                    {
                        paramAddress = reinterpret_cast<Address>(&param->integer);
                    }
                    else
                    {
                        return nullptr;
                    }

                    pushAddress(paramAddress);
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
    auto lowerCaseName(const char* name) -> std::string
    {
        std::string lower(name);
        MCPort::StrLwr(lower.data());
        return lower;
    }

    /// <summary>Finds <paramref name="name"/> (lower-cased) among the globals of the libraries <paramref name="entry"/> uses.</summary>
    auto searchLibrariesUsed(const ModuleEntry& entry, char* name) -> SymTableNodePtr
    {
        std::string lower = lowerCaseName(name);

        for (int32_t i = 0; i < entry.numLibrariesUsed; i++)
        {
            SymTableNodePtr libraryIdPtr = ModuleRegistry[entry.librariesUsed[i]->handle].moduleIdPtr;
            SymTableNodePtr symbol = searchSymTable(lower.data(), libraryIdPtr->defn.info.routine.localSymTable);

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
        ProfileLog->writeString(ProfileLogBuffer[i]);
    }

    NumProfileLogLines = 0;
}

auto ABL_CloseProfileLog() -> void
{
    if (ProfileLog)
    {
        DumpProfileLog();
        char line[512];
        std::snprintf(line, sizeof(line), "\nNum Total Lines = %d\n", TotalProfileLogLines);
        ProfileLog->writeString(line);
        ProfileLog->close();
        delete ProfileLog;
        ProfileLog = nullptr;
        NumProfileLogLines = 0;
        TotalProfileLogLines = 0;
    }
}

auto ABL_OpenProfileLog() -> void
{
    if (ProfileLog)
    {
        ABL_CloseProfileLog();
    }

    NumProfileLogLines = 0;
    ProfileLog = new File;

    if (!ProfileLog)
    {
        Fatal(0, " unable to malloc ABL ProfileLog ");
    }

    if (ProfileLog->create("abl.log") != 0)
    {
        Fatal(0, " unable to create ABL ProfileLog ");
    }
}

auto ABL_AddToProfileLog(char* profileEntry) -> void
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

auto initModuleRegistry(int32_t maxModules) -> void
{
    MaxModules = maxModules;
    ModuleRegistry =
        static_cast<ModuleEntry*>(AblStackHeap->malloc(static_cast<uint32_t>(maxModules * sizeof(ModuleEntry))));

    if (!ModuleRegistry)
    {
        Fatal(0, " ABL: Unable to AblStackHeap->malloc Module Registry ");
    }

    std::memset(ModuleRegistry, 0, static_cast<size_t>(MaxModules) * sizeof(ModuleEntry));

    ModuleInstanceRegistry =
        static_cast<ABLModule**>(AblStackHeap->malloc(static_cast<uint32_t>(MaxModules * sizeof(ABLModule*))));

    if (!ModuleInstanceRegistry)
    {
        Fatal(0, " ABL: Unable to malloc AblStackHeap->Module Instance Registry ");
    }

    for (int32_t i = 0; i < MaxModules; i++)
    {
        ModuleInstanceRegistry[i] = nullptr;
    }
}

auto destroyModuleRegistry() -> void
{
    if (!AblStackHeap)
    {
        return;
    }

    for (int32_t i = 0; i < NumModulesRegistered; i++)
    {
        ModuleEntry& entry = ModuleRegistry[i];
        AblStackHeap->free(entry.fileName);
        entry.fileName = nullptr;
        entry.moduleIdPtr = nullptr;

        // Port fix: frees module i's source file names. The original indexed the registry with the file counter
        // (ModuleRegistry[j].sourceFiles[j], while j < ModuleRegistry[j].numSourceFiles), freeing the wrong names
        // and reading past the used entries. Only frees memory: the heap goes away right after.
        for (int32_t j = 0; j < entry.numSourceFiles; j++)
        {
            AblStackHeap->free(entry.sourceFiles[j]);
            entry.sourceFiles[j] = nullptr;
        }
    }

    AblStackHeap->free(ModuleRegistry);
    ModuleRegistry = nullptr;
    AblStackHeap->free(ModuleInstanceRegistry);
    ModuleInstanceRegistry = nullptr;
}

auto initLibraryRegistry(int32_t maxLibraries) -> void
{
    MaxLibraries = maxLibraries;
    LibraryInstanceRegistry =
        static_cast<ABLModule**>(AblStackHeap->malloc(static_cast<uint32_t>(maxLibraries * sizeof(ABLModule*))));

    if (!LibraryInstanceRegistry)
    {
        Fatal(0, " ABL: Unable to malloc AblStackHeap->Library Instance Registry ");
    }

    for (int32_t i = 0; i < MaxLibraries; i++)
    {
        LibraryInstanceRegistry[i] = nullptr;
    }
}

auto destroyLibraryRegistry() -> void
{
    if (!AblStackHeap)
    {
        return;
    }

    for (int32_t i = 0; i < numLibrariesLoaded; i++)
    {
        ABLModule* library = LibraryInstanceRegistry[i];

        if (library)
        {
            library->destroy();
            delete library;
        }

        LibraryInstanceRegistry[i] = nullptr;
    }

    AblStackHeap->free(LibraryInstanceRegistry);
    LibraryInstanceRegistry = nullptr;
}

auto ABLModule::operator new(size_t mySize) noexcept -> void*
{
    if (systemHeap && systemHeap->heapSize != 0)
    {
        return systemHeap->malloc(static_cast<uint32_t>(mySize));
    }

    return nullptr;
}

auto ABLModule::operator delete(void* us) -> void
{
    if (systemHeap && systemHeap->heapSize != 0)
    {
        systemHeap->free(us);
        return;
    }

    std::free(us);
}

auto ABLModule::init(int32_t moduleHandle) -> int32_t
{
    handle = moduleHandle;
    id = NumModules++;
    staticData = nullptr;

    // One item per static; a static array's item points to its own block.
    const ModuleEntry& entry = ModuleRegistry[moduleHandle];
    int32_t numStatics = entry.numStaticVars;

    if (numStatics != 0)
    {
        staticData =
            static_cast<StackItemPtr>(AblStackHeap->malloc(static_cast<uint32_t>(numStatics * sizeof(StackItem))));

        if (!staticData)
        {
            char err[256];
            std::snprintf(err, sizeof(err), "ABL: Unable to AblStackHeap->malloc staticData [Module %d]", id);
            Fatal(0, err);
        }

        for (int32_t i = 0; i < numStatics; i++)
        {
            int32_t size = entry.sizeStaticVars[i];
            staticData[i] = StackItem{};

            if (size > 0)
            {
                staticData[i].address = static_cast<Address>(AblStackHeap->malloc(size));

                // Port fix: tests the new block (the original tested staticData again).
                if (!staticData[i].address)
                {
                    char err[256];
                    std::snprintf(err, sizeof(err),
                                  "ABL: Unable to AblStackHeap->malloc staticData address [Module %d]", id);
                    Fatal(0, err);
                }

#if !MCREDUX_FIX_ABL_UNINITIALIZED_STATICS
                std::memset(staticData[i].address, 0xff, static_cast<size_t>(size));
#endif
            }
        }
    }

    ModuleInstanceRegistry[NumModuleInstances++] = this;
    ModuleRegistry[moduleHandle].numInstances++;
    initCalled = 0;

    if (debugger)
    {
        watchManager = new WatchManager;

        if (!watchManager)
        {
            Fatal(0, " Unable to AblStackHeap->malloc WatchManager ");
        }

        if (watchManager->init(MaxWatchesPerModule) != 0)
        {
            Fatal(0, " Unable to AblStackHeap->malloc WatchManager ");
        }

        breakPointManager = new BreakPointManager;

        if (!breakPointManager)
        {
            Fatal(0, " Unable to AblStackHeap->malloc BreakPointManager ");
        }

        if (breakPointManager->init(MaxBreakPointsPerModule) != 0)
        {
            Fatal(0, " Unable to AblStackHeap->malloc BreakPointManager ");
        }
    }

    return 0;
}

auto ABLModule::setName(char* _name) -> void
{
    std::strncpy(name, _name, MAX_ABLMODULE_NAME - 1);
    name[MAX_ABLMODULE_NAME - 1] = '\0';
}

auto ABLModule::execute(ABLParam* paramList) -> int32_t
{
    SymTableNodePtr moduleIdPtr = beginModuleExecution(this, paramList);

    if (!moduleIdPtr)
    {
        return 0;
    }

    CurModuleHandle = handle;
    CallModuleInit = initCalled == 0;
    initCalled = 1;
    ::execute(moduleIdPtr);
    returnVal = returnValue.integer;
    return execStatementCount;
}

auto ABLModule::execute(ABLParam* moduleParamList, SymTableNodePtr function, ABLParam* functionParamList) -> int32_t
{
    SymTableNodePtr moduleIdPtr = beginModuleExecution(this, moduleParamList);

    if (!moduleIdPtr)
    {
        return 0;
    }

    CurModuleHandle = handle;
    int32_t wasInitCalled = initCalled;
    initCalled = 1;
    CallModuleInit = wasInitCalled == 0;
    executeChild(moduleIdPtr, function, functionParamList);
    returnVal = returnValue.integer;
    return execStatementCount;
}

auto ABLModule::findSymbol(char* symbolName, SymTableNodePtr function, int searchLibraries) -> SymTableNodePtr
{
    std::string lower = lowerCaseName(symbolName);

    if (function)
    {
        SymTableNodePtr symbol = searchSymTable(lower.data(), function->defn.info.routine.localSymTable);

        if (symbol)
        {
            return symbol;
        }
    }

    const ModuleEntry& entry = ModuleRegistry[handle];
    SymTableNodePtr symbol = searchSymTable(lower.data(), entry.moduleIdPtr->defn.info.routine.localSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = searchLibrariesUsed(entry, symbolName);
    }

    return symbol;
}

auto ABLModule::findFunction(char* functionName, int searchLibraries) -> SymTableNodePtr
{
    // The module's own table is searched with the name as given (not lower-cased).
    const ModuleEntry& entry = ModuleRegistry[handle];
    SymTableNodePtr symbol = searchSymTable(functionName, entry.moduleIdPtr->defn.info.routine.localSymTable);

    if (!symbol && searchLibraries)
    {
        symbol = searchLibrariesUsed(entry, functionName);
    }

    return symbol;
}

auto ABLModule::setStaticInteger(char* staticName, int32_t value) -> int32_t
{
    SymTableNodePtr idPtr = findSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (baseType(idPtr->typePtr) != IntegerTypePtr)
    {
        return 2;
    }

    if (idPtr->defn.info.data.varType != VAR_TYPE_STATIC)
    {
        return 3;
    }

    staticData[idPtr->defn.info.data.offset].integer = value;
    return 0;
}

auto ABLModule::setStaticReal(char* staticName, float value) -> int32_t
{
    SymTableNodePtr idPtr = findSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (baseType(idPtr->typePtr) != RealTypePtr)
    {
        return 2;
    }

    if (idPtr->defn.info.data.varType != VAR_TYPE_STATIC)
    {
        return 3;
    }

    staticData[idPtr->defn.info.data.offset].real = value;
    return 0;
}

auto ABLModule::setStaticIntegerArray(char* staticName, int32_t size, int32_t* values) -> int32_t
{
    SymTableNodePtr idPtr = findSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->defn.info.data.varType != VAR_TYPE_STATIC)
    {
        return 3;
    }

    std::memcpy(staticData[idPtr->defn.info.data.offset].address, values, static_cast<size_t>(size) * sizeof(int32_t));
    return 0;
}

auto ABLModule::setStaticRealArray(char* staticName, int32_t size, float* values) -> int32_t
{
    SymTableNodePtr idPtr = findSymbol(staticName);

    if (!idPtr)
    {
        return 1;
    }

    if (idPtr->defn.info.data.varType != VAR_TYPE_STATIC)
    {
        return 3;
    }

    std::memcpy(staticData[idPtr->defn.info.data.offset].address, values, static_cast<size_t>(size) * sizeof(float));
    return 0;
}

auto ABLModule::getSourceFile(int32_t fileNumber) -> char*
{
    return ModuleRegistry[handle].sourceFiles[fileNumber];
}

auto ABLModule::getSourceDirectory(int32_t fileNumber, char* directory) -> char*
{
    char* fileName = ModuleRegistry[handle].sourceFiles[fileNumber];
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

auto ABLModule::getInfo(int32_t& numStatics, int32_t& staticsSize, int32_t* sizeList) -> void
{
    const ModuleEntry& entry = ModuleRegistry[handle];
    numStatics = entry.numStaticVars;
    staticsSize = entry.totalSizeStaticVars;

    if (sizeList)
    {
        for (int32_t i = 0; i < numStatics; i++)
        {
            sizeList[i] = entry.sizeStaticVars[i];
        }
    }
}

auto ABLModule::destroy() -> void
{
    if (id > -1 && ModuleInstanceRegistry)
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

    if (watchManager)
    {
        watchManager->destroy();
        delete watchManager;
        watchManager = nullptr;
    }

    if (breakPointManager)
    {
        breakPointManager->destroy();
        delete breakPointManager;
        breakPointManager = nullptr;
    }

    // Faithful: the static arrays' own blocks are not freed.
    if (staticData)
    {
        AblStackHeap->free(staticData);
        staticData = nullptr;
    }
}
