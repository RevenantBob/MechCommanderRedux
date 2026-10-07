#include "stdafx.h"
#include "abl/ablrtn.h"
#include "abl/MCAblCompiler.h"
#include "abl/abldbug.h"
#include "abl/ablexec.h"
#include "abl/ablxstd.h"
#include "abl/ablxstmt.h"
#include "lib/MCFatal.h"
#include "main/MCGameContext.h"

int32_t MaxBreaks = 50;
int32_t MaxWatches = 50;
int ProfileAbl = 0;
int ABLenabled = 0;
MCAblSymbol* CurRoutineIdPtr = nullptr;
int InOrdersBlock = 0;

namespace
{
    /// <summary>
    /// Compiles <paramref name="fileName"/> (the library <paramref name="library"/>, or a module) and registers it.
    /// </summary>
    /// <returns>Its handle; the handle compiled before from the same file; or -3 when the file won't open.</returns>
    auto CompileAndRegister(std::string_view fileName, MCAblModule* library) -> int32_t
    {
        // The registry holds the names lower-cased (the original lower-cased the caller's string in place).
        std::string lowerName(fileName);
        std::ranges::transform(lowerName, lowerName.begin(),
                               [](char ch) { return static_cast<char>(std::tolower(static_cast<uint8_t>(ch))); });

        for (int32_t i = 0; i < NumModulesRegistered; i++)
        {
            if (lowerName == ModuleRegistry[i].FileName)
            {
                return i;
            }
        }

        auto compiled = MCAblCompiler::Compile(lowerName, {library, Debugger != nullptr, IncludeDebugInfo != 0});

        if (!compiled)
        {
            // The module's own file won't open: the original returned openSourceFile's code, not a handle.
            if (compiled.error().Code == MCAblSyntaxError::SourceFileOpen && compiled.error().LineNumber == 0)
            {
                return -3;
            }

            // A syntax error ends the game, as in MCX.EXE.
            Fatal(0, compiled.error().Message());
        }

        const int32_t moduleHandle = NumModulesRegistered;
        MCModuleEntry& entry = ModuleRegistry[moduleHandle];
        entry.FileName = AblMemory.CopyString(lowerName);
        entry.ModuleIdPtr = compiled->Module;
        entry.NumSourceFiles = static_cast<int32_t>(compiled->SourceFiles.size());
        entry.SourceFiles = AblMemory.AllocateArray<char*>(compiled->SourceFiles.size());

        for (size_t i = 0; i < compiled->SourceFiles.size(); i++)
        {
            entry.SourceFiles[i] = AblMemory.CopyString(compiled->SourceFiles[i]);
        }

        if (!compiled->LibrariesUsed.empty())
        {
            entry.NumLibrariesUsed = static_cast<int32_t>(compiled->LibrariesUsed.size());
            entry.LibrariesUsed = AblMemory.AllocateArray<MCAblModule*>(compiled->LibrariesUsed.size());
            std::ranges::copy(compiled->LibrariesUsed, entry.LibrariesUsed);
        }

        entry.NumStaticVars = static_cast<int32_t>(compiled->StaticSizes.size());
        entry.SizeStaticVars = nullptr;
        entry.TotalSizeStaticVars = 0;

        if (entry.NumStaticVars != 0)
        {
            entry.SizeStaticVars = AblMemory.AllocateArray<int32_t>(compiled->StaticSizes.size());
            std::ranges::copy(compiled->StaticSizes, entry.SizeStaticVars);
            entry.TotalSizeStaticVars = entry.NumStaticVars * 4;

            for (const int32_t size : compiled->StaticSizes)
            {
                entry.TotalSizeStaticVars += size;
            }
        }

        entry.NumInstances = 0;
        NumModulesRegistered = moduleHandle + 1;
        return moduleHandle;
    }
}

auto AblInit(uint32_t, uint32_t, uint32_t, uint32_t stackSize, uint32_t, uint32_t maxModules, uint32_t,
             void (*debuggerPrintCallback)(char* s), int debugInfo, int debug, int profile) -> void
{
    MaxWatchesPerModule = 20;
    MaxBreakPointsPerModule = 20;
    ABLenabled = 1;
    MaxBreaks = 50;
    MaxWatches = 50;
    Debugger = nullptr;
    NumModules = 0;
    ModuleRegistry = nullptr;
    ModuleInstanceRegistry = nullptr;
    LibraryInstanceRegistry = nullptr;
    MaxModules = 0;
    MaxLibraries = 0;
    NumModulesRegistered = 0;
    NumModuleInstances = 0;
    CurModule = nullptr;
    CodeSegmentPtr = nullptr;
    StatementStartPtr = nullptr;
    ExecStatementCount = 0;
    Stack = nullptr;
    Tos = nullptr;
    StackFrameBasePtr = nullptr;
    StaticDataPtr = nullptr;
    EternalOffset = 0;
    CurModuleHandle = 0;
    CallModuleInit = 0;
    InOrdersBlock = 0;
    MaxLoopIterations = 100001;
    ProfileAbl = profile;
    Level = 0;
    ExecFileNumber = 0;
    CurRoutineIdPtr = nullptr;
    IsUnitOrder = 0;
    TacOrderOrigin = 1;
    CurGroup = nullptr;
    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurContact = nullptr;
    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    NumLibrariesLoaded = 0;

    // The original took stackSize bytes of 4-byte items; the port takes at least MAXSIZE_STACK items (see ablexec.h).
    size_t stackItems = std::max<size_t>((stackSize & ~3u) / 4, MAXSIZE_STACK);
    Stack = AblMemory.AllocateArray<MCStackItem>(stackItems);

    MCGameContext::Current().SetAblSymbols(std::make_unique<MCAblSymbolTable>());
    InitModuleRegistry(static_cast<int32_t>(maxModules));
    InitLibraryRegistry(10);

    IncludeDebugInfo = debugInfo;

    if (debug)
    {
        IncludeDebugInfo = 1;
        Debugger = new MCDebugger;
        Debugger->Init(debuggerPrintCallback, nullptr);
    }

    if (ProfileAbl)
    {
        AblOpenProfileLog();
    }
}

auto AblPreProcess(std::string_view sourceFileName) -> int32_t
{
    return CompileAndRegister(sourceFileName, nullptr);
}

auto AblExecute(MCAblSymbol* moduleIdPtr, MCAblSymbol*, MCAblParam* paramList, MCStackItemPtr returnVal) -> int32_t
{
    NumExecutions++;
    Tos = Stack + EternalOffset;
    StackFrameBasePtr = Tos + 1;
    CurRoutineIdPtr = nullptr;
    ExecStatementCount = 0;
    Level = 1;
    CallStackLevel = 0;

    // The module's frame header.
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
                        return 0;
                    }

                    PushInteger(param->Integer);
                }

                // Faithful: nothing was pushed for an array parameter, so this copies the block the top item
                // points to.
                if (formalTypePtr->Form == MCAblTypeForm::Array)
                {
                    int32_t size = formalTypePtr->Size;
                    MCAddress copy = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

                    if (!copy)
                    {
                        Fatal(0, " ABL: Unable to AblStackHeap->malloc module formal array param ");
                    }

                    MCAddress source = Tos->Address;
                    Tos->Address = copy;
                    std::memcpy(copy, source, static_cast<size_t>(size));
                }
            }
            else
            {
                // A reference parameter points into the list, so the module can write back.
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
                    return 0;
                }

                PushAddress(paramAddress);
            }
        }
    }

    Execute(moduleIdPtr);

    if (returnVal)
    {
        *returnVal = ReturnValue;
    }

    return ExecStatementCount;
}

auto AblClose() -> void
{
    DestroyModuleRegistry();
    DestroyLibraryRegistry();
    Stack = nullptr;

    if (Debugger)
    {
        Debugger->Destroy();
        delete Debugger;
        Debugger = nullptr;
    }

    MCGameContext::Current().SetAblSymbols(nullptr);
    AblMemory.Clear();
    AblCloseProfileLog();
    ABLenabled = 0;
}

auto AblLoadLibrary(std::string_view sourceFileName) -> int32_t
{
    auto library = std::make_unique<MCAblModule>();
    const int32_t moduleHandle = CompileAndRegister(sourceFileName, library.get());

    // Anything but the module just registered (a library compiled before, or a file that won't open) fails.
    if (moduleHandle < NumModulesRegistered - 1)
    {
        library->Destroy();
        return -1;
    }

    int32_t err = library->Init(moduleHandle);
    Assert(err == 0, static_cast<uint32_t>(err), " Error Loading ABL Library ");
    // The original named it with the caller's string, which the compile had lower-cased in place.
    std::string name(sourceFileName);
    std::ranges::transform(name, name.begin(),
                           [](char ch) { return static_cast<char>(std::tolower(static_cast<uint8_t>(ch))); });
    library->SetName(name);
    LibraryInstanceRegistry[NumLibrariesLoaded] = library.release();
    NumLibrariesLoaded++;
    return 0;
}

auto AblCreateParamList(int32_t numParameters) -> MCAblParam*
{
    if (numParameters == 0)
    {
        return nullptr;
    }

    // Room for one parameter more than asked, as in the original.
    return AblMemory.AllocateArray<MCAblParam>(static_cast<size_t>(numParameters + 1));
}

auto AblSetIntegerParam(MCAblParam* paramList, int32_t index, int32_t value) -> void
{
    if (paramList)
    {
        paramList[index].Type = ABL_PARAM_INTEGER;
        paramList[index].Integer = value;
    }
}

auto AblSetRealParam(MCAblParam* paramList, int32_t index, float value) -> void
{
    if (paramList)
    {
        paramList[index].Type = ABL_PARAM_REAL;
        paramList[index].Real = value;
    }
}

auto AblDeleteParamList(MCAblParam* paramList) -> void
{
    if (paramList)
    {
        AblMemory.Free(paramList);
    }
}

auto AblGetModule(int32_t id) -> MCAblModule*
{
    if (id > -1 && id < NumModules)
    {
        return ModuleInstanceRegistry[id];
    }

    return nullptr;
}

auto AblEnabled() -> int
{
    return ABLenabled;
}
