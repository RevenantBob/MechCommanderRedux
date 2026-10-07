#include "stdafx.h"
#include "abl/ablrtn.h"
#include "abl/abldbug.h"
#include "abl/abldecl.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablscan.h"
#include "abl/ablstd.h"
#include "abl/ablstmt.h"
#include "abl/ablsymt.h"
#include "abl/ablxstd.h"
#include "abl/ablxstmt.h"
#include "lib/aerror.h"

int32_t MaxBreaks = 50;
int32_t MaxWatches = 50;
int PrintEnabled = 1;
int AssertEnabled = 0;
int StringFunctionsEnabled = 1;
int ProfileAbl = 0;
int ABLenabled = 0;
int32_t* StaticVariablesSizes = nullptr;
int32_t NumStaticVariables = 0;
int32_t MaxStaticVariables = 0;
int BlockFlag = 0;
MCBlockType BlockType = BLOCK_MODULE;
MCSymTableNodePtr CurModuleIdPtr = nullptr;
MCSymTableNodePtr CurRoutineIdPtr = nullptr;
int InOrdersBlock = 0;
int EofFlag = 0;

MCTokenCodeType FollowHeaderList[] = {TKN_SEMICOLON, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowModuleIdList[] = {TKN_LPAREN, TKN_COLON, TKN_SEMICOLON, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowFunctionIdList[] = {TKN_LPAREN, TKN_COLON, TKN_SEMICOLON, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowParamsList[] = {TKN_RPAREN, TKN_COMMA, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowParamList[] = {TKN_COMMA, TKN_RPAREN, TKN_NONE};
MCTokenCodeType FollowModuleDeclsList[] = {TKN_SEMICOLON, TKN_CODE, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowRoutineDeclsList[] = {TKN_SEMICOLON, TKN_CODE, TKN_EOF, TKN_NONE};

namespace
{
    /// <summary>Clears a new routine or module symbol's definition (no parameters, locals or code yet).</summary>
    void ClearRoutineDefinition(MCSymTableNodePtr routineIdPtr, MCDefinitionType key)
    {
        routineIdPtr->Defn.Key = key;
        routineIdPtr->Defn.Info.Routine.Key = RTN_DECLARED;
        routineIdPtr->Defn.Info.Routine.ParamCount = 0;
        routineIdPtr->Defn.Info.Routine.TotalParamSize = 0;
        routineIdPtr->Defn.Info.Routine.TotalLocalSize = 0;
        routineIdPtr->Defn.Info.Routine.Params = nullptr;
        routineIdPtr->Defn.Info.Routine.Locals = nullptr;
        routineIdPtr->Defn.Info.Routine.LocalSymTable = nullptr;
        routineIdPtr->Defn.Info.Routine.CodeSegment = nullptr;
        routineIdPtr->Library = CurLibrary;
        routineIdPtr->TypePtr = &DummyType;
        routineIdPtr->LabelIndex = 0;
    }

    /// <summary>
    /// Compiles the statements of a code block up to <paramref name="endToken"/> (not included), resynchronising
    /// after each.
    /// </summary>
    void CompileStatements(MCTokenCodeType endToken)
    {
        if (CurToken == endToken)
        {
            return;
        }

        do
        {
            Statement();

            while (CurToken == TKN_SEMICOLON)
            {
                GetToken();
            }

            if (CurToken == endToken)
            {
                break;
            }

            Synchronize(StatementStartList, nullptr, nullptr);
        } while (TokenIn(StatementStartList));
    }

    /// <summary>After a header, expects its semicolon (a declaration or statement there means it's missing).</summary>
    void HeaderSemicolon()
    {
        Synchronize(FollowHeaderList, DeclarationStartList, StatementStartList);

        if (CurToken == TKN_SEMICOLON)
        {
            GetToken();
        }
        else if (TokenIn(DeclarationStartList) || TokenIn(StatementStartList))
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
        }
    }
}

auto AblInit(uint32_t, uint32_t, uint32_t, uint32_t stackSize, uint32_t maxCodeBufferSize, uint32_t maxModules,
             uint32_t maxStaticVariables, void (*debuggerPrintCallback)(char* s), int debugInfo, int debug, int profile)
    -> void
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
    ErrorCount = 0;
    CodeBuffer = nullptr;
    CodeBufferPtr = nullptr;
    CodeSegmentPtr = nullptr;
    CodeSegmentLimit = nullptr;
    StatementStartPtr = nullptr;
    ExecStatementCount = 0;
    Stack = nullptr;
    Tos = nullptr;
    StackFrameBasePtr = nullptr;
    StaticDataPtr = nullptr;
    StaticVariablesSizes = nullptr;
    EternalOffset = 0;
    MaxStaticVariables = 0;
    NumStaticVariables = 0;
    CurModuleHandle = 0;
    CallModuleInit = 0;
    InOrdersBlock = 0;
    MaxLoopIterations = 100001;
    AssertEnabled = 0;
    PrintEnabled = 0;
    StringFunctionsEnabled = 1;
    IncludeDebugInfo = 1;
    ProfileAbl = profile;
    Crunch = 1;
    Level = 0;
    LineNumber = 0;
    FileNumber = 0;
    SourceFile = nullptr;
    PrintFlag = 1;
    BlockFlag = 0;
    BlockType = BLOCK_MODULE;
    CurModuleIdPtr = nullptr;
    CurRoutineIdPtr = nullptr;
    DumbGetCharOn = 0;
    NumOpenFiles = 0;
    NumSourceFiles = 0;
    BufferOffset = 0;
    Bufferp = SourceBuffer;
    Tokenp = TokenString;
    DigitCount = 0;
    CountError = 0;
    PageNumber = 0;
    LineCount = 50;
    IsUnitOrder = 0;
    TacOrderOrigin = 1;
    CurGroup = nullptr;
    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurContact = nullptr;
    EofFlag = 0;
    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    NumLibrariesLoaded = 0;

    for (auto& code : CharTable)
    {
        code = CHR_SPECIAL;
    }

    for (int32_t ch = '0'; ch <= '9'; ch++)
    {
        CharTable[ch] = CHR_DIGIT;
    }

    for (int32_t ch = 'A'; ch <= 'Z'; ch++)
    {
        CharTable[ch] = CHR_LETTER;
    }

    for (int32_t ch = 'a'; ch <= 'z'; ch++)
    {
        CharTable[ch] = CHR_LETTER;
    }

    CharTable['"'] = CHR_DQUOTE;
    CharTable[0x7f] = CHR_EOF;

    MaxCodeBufferSize = static_cast<int32_t>(maxCodeBufferSize);
    CodeBuffer = AblMemory.AllocateArray<char>(maxCodeBufferSize);

    NumStaticVariables = 0;
    MaxStaticVariables = static_cast<int32_t>(maxStaticVariables);
    StaticDataPtr = nullptr;
    StaticVariablesSizes = nullptr;

    if (MaxStaticVariables > 0)
    {
        StaticVariablesSizes = AblMemory.AllocateArray<int32_t>(static_cast<size_t>(MaxStaticVariables));
    }

    // The original took stackSize bytes of 4-byte items; the port takes at least MAXSIZE_STACK items (see ablexec.h).
    size_t stackItems = std::max<size_t>((stackSize & ~3u) / 4, MAXSIZE_STACK);
    Stack = AblMemory.AllocateArray<MCStackItem>(stackItems);

    InitSymTable();
    InitModuleRegistry(static_cast<int32_t>(maxModules));
    InitLibraryRegistry(10);

    IncludeDebugInfo = debugInfo;

    if (debug)
    {
        IncludeDebugInfo = 1;
        Debugger = new MCDebugger;

        if (!Debugger)
        {
            Fatal(0, " Unable to initialize ABL Debugger. ");
        }

        Debugger->Init(debuggerPrintCallback, nullptr);
    }

    if (ProfileAbl)
    {
        AblOpenProfileLog();
    }
}

auto AblPreProcess(char* sourceFileName, int32_t* numErrors, int32_t* numLinesProcessed, int32_t* numFilesProcessed,
                   int printLines) -> int32_t
{
    // Already compiled from this file?
    for (int32_t i = 0; i < NumModulesRegistered; i++)
    {
        if (std::strcmp(MCPort::StrLwr(sourceFileName), ModuleRegistry[i].FileName) == 0)
        {
            return i;
        }
    }

    PrintEnabled = Debugger != nullptr;
    Level = 0;
    PrintFlag = printLines;
    LineNumber = 0;
    FileNumber = 0;
    StringFunctionsEnabled = 1;
    NumSourceFiles = 0;
    NumLibrariesUsed = 0;
    SourceFile = nullptr;
    BlockFlag = 0;
    BlockType = BLOCK_MODULE;
    BufferOffset = 0;
    Bufferp = SourceBuffer;
    Tokenp = TokenString;
    DigitCount = 0;
    CountError = 0;
    PageNumber = 0;
    ErrorCount = 0;
    ExecStatementCount = 0;
    EofFlag = 0;
    NumStaticVariables = 0;
    AssertEnabled = PrintEnabled;

    if (numErrors)
    {
        *numErrors = 0;
    }

    if (numLinesProcessed)
    {
        *numLinesProcessed = 0;
    }

    // Faithful: a file that won't open returns openSourceFile's error code, not a handle.
    int32_t openErr = OpenSourceFile(sourceFileName);

    if (openErr != 0)
    {
        return openErr;
    }

    CodeBufferPtr = CodeBuffer;
    GetToken();
    MCSymTableNodePtr moduleIdPtr = ModuleHeader();
    CurModuleIdPtr = moduleIdPtr;
    CurRoutineIdPtr = moduleIdPtr;
    HeaderSemicolon();

    Declarations(moduleIdPtr, 1);
    Synchronize(FollowModuleDeclsList, nullptr, nullptr);

    if (CurToken != TKN_CODE)
    {
        SyntaxError(ABL_ERR_SYNTAX_MISSING_CODE);
    }

    CrunchToken();
    BlockType = BLOCK_MODULE;
    BlockFlag = 1;
    GetToken();
    CompileStatements(CurLibrary ? TKN_END_LIBRARY : TKN_END_MODULE);

    if (CurLibrary)
    {
        IfTokenGetElseError(TKN_END_LIBRARY, ABL_ERR_SYNTAX_MISSING_END_LIBRARY);
    }
    else
    {
        IfTokenGetElseError(TKN_END_MODULE, ABL_ERR_SYNTAX_MISSING_END_MODULE);
    }

    BlockFlag = 0;
    moduleIdPtr->Defn.Info.Routine.LocalSymTable = ExitScope();
    moduleIdPtr->Defn.Info.Routine.CodeSegment = CreateCodeSegment();
    IfTokenGetElseError(TKN_PERIOD, ABL_ERR_SYNTAX_MISSING_PERIOD);

    while (CurToken != TKN_EOF)
    {
        SyntaxError(ABL_ERR_SYNTAX_VALUE_OUT_OF_RANGE);
        GetToken();
    }

    CloseSourceFile();

    // Register the module.
    int32_t moduleHandle = NumModulesRegistered;
    MCModuleEntry& entry = ModuleRegistry[moduleHandle];
    entry.FileName = AblMemory.CopyString(MCPort::StrLwr(sourceFileName));
    entry.ModuleIdPtr = moduleIdPtr;
    entry.NumSourceFiles = NumSourceFiles;
    entry.SourceFiles = AblMemory.AllocateArray<char*>(static_cast<size_t>(NumSourceFiles));

    for (int32_t i = 0; i < NumSourceFiles; i++)
    {
        entry.SourceFiles[i] = AblMemory.CopyString(SourceFiles[i]);
    }

    if (NumLibrariesUsed > 0)
    {
        entry.NumLibrariesUsed = NumLibrariesUsed;
        entry.LibrariesUsed = AblMemory.AllocateArray<MCAblModule*>(static_cast<size_t>(NumLibrariesUsed));

        for (int32_t i = 0; i < NumLibrariesUsed; i++)
        {
            entry.LibrariesUsed[i] = LibrariesUsed[i];
        }
    }

    entry.NumStaticVars = NumStaticVariables;
    entry.SizeStaticVars = nullptr;
    entry.TotalSizeStaticVars = 0;

    if (NumStaticVariables != 0)
    {
        entry.SizeStaticVars = AblMemory.AllocateArray<int32_t>(static_cast<size_t>(NumStaticVariables));

        for (int32_t i = 0; i < NumStaticVariables; i++)
        {
            entry.SizeStaticVars[i] = StaticVariablesSizes[i];
        }

        entry.TotalSizeStaticVars = NumStaticVariables * 4;

        for (int32_t i = 0; i < entry.NumStaticVars; i++)
        {
            entry.TotalSizeStaticVars += entry.SizeStaticVars[i];
        }
    }

    entry.NumInstances = 0;
    NumModulesRegistered = moduleHandle + 1;

    if (numLinesProcessed)
    {
        *numLinesProcessed = LineNumber;
    }

    if (numFilesProcessed)
    {
        *numFilesProcessed = FileNumber;
    }

    if (numErrors)
    {
        *numErrors = ErrorCount;
    }

    return moduleHandle;
}

auto AblExecute(MCSymTableNodePtr moduleIdPtr, MCSymTableNodePtr, MCAblParam* paramList, MCStackItemPtr returnVal)
    -> int32_t
{
    NumExecutions++;
    Tos = Stack + EternalOffset;
    CurModuleIdPtr = nullptr;
    StackFrameBasePtr = Tos + 1;
    CurRoutineIdPtr = nullptr;
    ErrorCount = 0;
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

        for (MCSymTableNodePtr formalIdPtr = moduleIdPtr->Defn.Info.Routine.Params; formalIdPtr;
             formalIdPtr = formalIdPtr->Next, param++)
        {
            MCTypePtr formalTypePtr = formalIdPtr->TypePtr;

            if (formalIdPtr->Defn.Key == DFN_VALPARAM)
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
                if (formalTypePtr->Form == FRM_ARRAY)
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

    StaticVariablesSizes = nullptr;
    CodeBuffer = nullptr;
    Stack = nullptr;

    if (Debugger)
    {
        Debugger->Destroy();
        delete Debugger;
        Debugger = nullptr;
    }

    AblMemory.Clear();

    AblCloseProfileLog();
    ABLenabled = 0;
}

auto AblLoadLibrary(char* sourceFileName, int32_t* numErrors, int32_t* numLinesProcessed, int32_t* numFilesProcessed,
                    int printLines) -> int32_t
{
    MCAblModule* library = new MCAblModule;

    if (!library)
    {
        return static_cast<int32_t>(0xFAAF000B);
    }

    CurLibrary = library;
    int32_t moduleHandle = AblPreProcess(sourceFileName, numErrors, numLinesProcessed, numFilesProcessed, printLines);

    // Anything but the module just registered (a library compiled before, or an error code) fails.
    if (moduleHandle < NumModulesRegistered - 1)
    {
        library->Destroy();
        delete library;
        CurLibrary = nullptr;
        return -1;
    }

    int32_t err = library->Init(moduleHandle);
    Assert(err == 0, static_cast<uint32_t>(err), " Error Loading ABL Library ");
    library->SetName(sourceFileName);
    CurLibrary = nullptr;
    LibraryInstanceRegistry[NumLibrariesLoaded] = library;
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

auto ModuleHeader() -> MCSymTableNodePtr
{
    MCSymTableNodePtr moduleIdPtr = nullptr;

    if (CurLibrary)
    {
        IfTokenGetElseError(TKN_LIBRARY, ABL_ERR_SYNTAX_MISSING_LIBRARY);
    }
    else
    {
        IfTokenGetElseError(TKN_MODULE, ABL_ERR_SYNTAX_MISSING_MODULE);
    }

    if (CurToken == TKN_IDENTIFIER)
    {
        SearchAndEnterLocalSymTable(moduleIdPtr);
        ClearRoutineDefinition(moduleIdPtr, DFN_MODULE);
        GetToken();
    }
    else
    {
        SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
    }

    Synchronize(FollowModuleIdList, DeclarationStartList, StatementStartList);
    EnterScope(nullptr);

    if (CurToken == TKN_LPAREN)
    {
        int32_t paramCount;
        int32_t totalParamSize;
        MCSymTableNodePtr params = FormalParamList(&paramCount, &totalParamSize);
        moduleIdPtr->Defn.Info.Routine.ParamCount = paramCount;
        moduleIdPtr->Defn.Info.Routine.TotalParamSize = totalParamSize;
        moduleIdPtr->Defn.Info.Routine.Params = params;
    }

    // An optional result type.
    moduleIdPtr->TypePtr = nullptr;

    if (CurToken == TKN_COLON)
    {
        GetToken();

        if (CurToken == TKN_IDENTIFIER)
        {
            MCSymTableNodePtr typeIdPtr = nullptr;
            SearchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->Defn.Key != DFN_TYPE)
            {
                SyntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            moduleIdPtr->TypePtr = typeIdPtr->TypePtr;
            GetToken();
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            moduleIdPtr->TypePtr = &DummyType;
        }
    }

    return moduleIdPtr;
}

auto Routine() -> void
{
    MCSymTableNodePtr routineIdPtr = FunctionHeader();
    MCSymTableNodePtr outerRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = routineIdPtr;
    HeaderSemicolon();

    if (std::strcmp(WordString, "forward") == 0)
    {
        GetToken();
        routineIdPtr->Defn.Info.Routine.Key = RTN_FORWARD;
    }
    else
    {
        routineIdPtr->Defn.Info.Routine.Key = RTN_DECLARED;
        routineIdPtr->Defn.Info.Routine.Locals = nullptr;
        Declarations(routineIdPtr, 0);
        Synchronize(FollowRoutineDeclsList, nullptr, nullptr);

        if (CurToken != TKN_CODE)
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_CODE);
        }

        CrunchToken();
        BlockType = BLOCK_ROUTINE;
        BlockFlag = 1;
        GetToken();
        CompileStatements(TKN_END_FUNCTION);
        IfTokenGetElseError(TKN_END_FUNCTION, ABL_ERR_SYNTAX_MISSING_END_FUNCTION);
        BlockFlag = 0;
        routineIdPtr->Defn.Info.Routine.CodeSegment = CreateCodeSegment();
    }

    routineIdPtr->Defn.Info.Routine.LocalSymTable = ExitScope();
    CurRoutineIdPtr = outerRoutineIdPtr;
}

auto FunctionHeader() -> MCSymTableNodePtr
{
    GetToken();
    bool forwardFlag = false;
    MCSymTableNodePtr functionIdPtr = nullptr;
    MCSymTableNodePtr typeIdPtr = nullptr;

    if (CurToken == TKN_IDENTIFIER)
    {
        SearchLocalSymTable(functionIdPtr);

        if (!functionIdPtr)
        {
            EnterLocalSymTable(functionIdPtr);
            ClearRoutineDefinition(functionIdPtr, DFN_FUNCTION);
        }
        else if (functionIdPtr->Defn.Key == DFN_FUNCTION && functionIdPtr->Defn.Info.Routine.Key == RTN_FORWARD)
        {
            forwardFlag = true;
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
        }

        GetToken();
    }

    Synchronize(FollowFunctionIdList, DeclarationStartList, StatementStartList);
    EnterScope(nullptr);

    if (CurToken == TKN_LPAREN)
    {
        int32_t paramCount;
        int32_t totalParamSize;
        MCSymTableNodePtr params = FormalParamList(&paramCount, &totalParamSize);

        // A forwarded function had its parameters declared already.
        if (forwardFlag)
        {
            SyntaxError(ABL_ERR_SYNTAX_ALREADY_FORWARDED);
        }
        else
        {
            functionIdPtr->Defn.Info.Routine.ParamCount = paramCount;
            functionIdPtr->Defn.Info.Routine.TotalParamSize = totalParamSize;
            functionIdPtr->Defn.Info.Routine.Params = params;
        }
    }
    else if (!forwardFlag)
    {
        functionIdPtr->Defn.Info.Routine.ParamCount = 0;
        functionIdPtr->Defn.Info.Routine.TotalParamSize = 0;
        functionIdPtr->Defn.Info.Routine.Params = nullptr;
    }

    // Faithful: the result type is cleared even for a forwarded function, whose own header must not repeat it.
    functionIdPtr->TypePtr = nullptr;

    if (CurToken == TKN_COLON)
    {
        GetToken();

        if (CurToken == TKN_IDENTIFIER)
        {
            SearchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->Defn.Key != DFN_TYPE)
            {
                SyntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            if (!forwardFlag)
            {
                functionIdPtr->TypePtr = typeIdPtr->TypePtr;
            }

            GetToken();
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            functionIdPtr->TypePtr = &DummyType;
        }

        if (forwardFlag)
        {
            SyntaxError(ABL_ERR_SYNTAX_ALREADY_FORWARDED);
        }
    }

    return functionIdPtr;
}

auto FormalParamList(int32_t* count, int32_t* totalSize) -> MCSymTableNodePtr
{
    MCSymTableNodePtr lastIdPtr = nullptr;
    MCSymTableNodePtr firstIdPtr = nullptr;
    int32_t paramCount = 0;
    // Parameters follow the 4-item frame header.
    int32_t paramOffset = 4;

    GetToken();

    for (;;)
    {
        // Each parameter is "type name" or "@type name" (by reference).
        MCDefinitionType paramDefn;
        MCTypePtr paramTypePtr;

        if (CurToken == TKN_IDENTIFIER)
        {
            paramDefn = DFN_VALPARAM;
        }
        else if (CurToken == TKN_REF)
        {
            paramDefn = DFN_REFPARAM;
            GetToken();
        }
        else
        {
            IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
            *count = paramCount;
            *totalSize = paramOffset - 4;
            return firstIdPtr;
        }

        if (CurToken == TKN_IDENTIFIER)
        {
            MCSymTableNodePtr typeIdPtr = nullptr;
            SearchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->Defn.Key != DFN_TYPE)
            {
                SyntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            paramTypePtr = typeIdPtr->TypePtr;
            GetToken();
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            paramTypePtr = &DummyType;
        }

        if (CurToken == TKN_IDENTIFIER)
        {
            MCSymTableNodePtr paramIdPtr = nullptr;
            SearchAndEnterLocalSymTable(paramIdPtr);
            paramIdPtr->Defn.Key = paramDefn;
            paramIdPtr->LabelIndex = 0;
            paramIdPtr->TypePtr = paramTypePtr;
            paramIdPtr->Defn.Info.Data.Offset = paramOffset++;
            paramCount++;

            if (!firstIdPtr)
            {
                firstIdPtr = paramIdPtr;
            }

            if (lastIdPtr)
            {
                lastIdPtr->Next = paramIdPtr;
            }

            lastIdPtr = paramIdPtr;
            GetToken();
        }
        else
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
        }

        Synchronize(FollowParamsList, nullptr, nullptr);
        IfTokenGet(TKN_COMMA);
    }
}

auto RoutineCall(MCSymTableNodePtr routineIdPtr, int paramCheck) -> MCTypePtr
{
    MCSymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    MCTypePtr resultType;
    MCRoutineKey key = routineIdPtr->Defn.Info.Routine.Key;

    if (key != RTN_DECLARED && key != RTN_FORWARD && paramCheck)
    {
        resultType = StandardRoutineCall(routineIdPtr);
    }
    else
    {
        resultType = DeclaredRoutineCall(routineIdPtr, paramCheck);
    }

    CurRoutineIdPtr = thisRoutineIdPtr;
    return resultType;
}

auto DeclaredRoutineCall(MCSymTableNodePtr routineIdPtr, int paramCheck) -> MCTypePtr
{
    ActualParamList(routineIdPtr, paramCheck);
    return routineIdPtr->TypePtr;
}

auto ActualParamList(MCSymTableNodePtr routineIdPtr, int paramCheck) -> void
{
    MCSymTableNodePtr formalIdPtr = paramCheck ? routineIdPtr->Defn.Info.Routine.Params : nullptr;
    MCDefinitionType formalDefn = DFN_UNDEFINED;
    MCTypePtr formalTypePtr = nullptr;

    if (CurToken == TKN_LPAREN)
    {
        do
        {
            if (paramCheck && formalIdPtr)
            {
                formalDefn = formalIdPtr->Defn.Key;
                formalTypePtr = formalIdPtr->TypePtr;
            }

            GetToken();

            if (!formalIdPtr || formalDefn == DFN_VALPARAM || !paramCheck)
            {
                // A value parameter (or no checking): any expression of a compatible type.
                MCTypePtr actualTypePtr = Expression();

                if (paramCheck)
                {
                    if (formalIdPtr)
                    {
                        if (!IsAssignTypeCompatible(formalTypePtr, actualTypePtr))
                        {
                            SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
                        }

                        formalIdPtr = formalIdPtr->Next;
                    }
                    else
                    {
                        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
                    }
                }
            }
            else
            {
                // A reference parameter: a variable of exactly the formal's type.
                if (CurToken == TKN_IDENTIFIER)
                {
                    MCSymTableNodePtr actualIdPtr = nullptr;
                    SearchAndFindAllSymTables(actualIdPtr);

                    if (formalTypePtr != Variable(actualIdPtr, USE_REFPARAM))
                    {
                        SyntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
                    }
                }
                else
                {
                    Expression();
                    SyntaxError(ABL_ERR_SYNTAX_INVALID_REF_PARAM);
                }

                formalIdPtr = formalIdPtr->Next;
            }

            Synchronize(FollowParamList, StatementEndList, nullptr);
        } while (CurToken == TKN_COMMA);

        IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }

    if (paramCheck && formalIdPtr)
    {
        SyntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}
