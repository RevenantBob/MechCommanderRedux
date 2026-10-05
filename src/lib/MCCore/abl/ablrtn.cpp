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
int ProfileABL = 0;
int ABLenabled = 0;
int32_t* StaticVariablesSizes = nullptr;
int32_t NumStaticVariables = 0;
int32_t MaxStaticVariables = 0;
int blockFlag = 0;
BlockType blockType = BLOCK_MODULE;
SymTableNodePtr CurModuleIdPtr = nullptr;
SymTableNodePtr CurRoutineIdPtr = nullptr;
int InOrdersBlock = 0;
int eofFlag = 0;

TokenCodeType followHeaderList[] = {TKN_SEMICOLON, TKN_EOF, TKN_NONE};
TokenCodeType followModuleIdList[] = {TKN_LPAREN, TKN_COLON, TKN_SEMICOLON, TKN_EOF, TKN_NONE};
TokenCodeType followFunctionIdList[] = {TKN_LPAREN, TKN_COLON, TKN_SEMICOLON, TKN_EOF, TKN_NONE};
TokenCodeType followParamsList[] = {TKN_RPAREN, TKN_COMMA, TKN_EOF, TKN_NONE};
TokenCodeType followParamList[] = {TKN_COMMA, TKN_RPAREN, TKN_NONE};
TokenCodeType followModuleDeclsList[] = {TKN_SEMICOLON, TKN_CODE, TKN_EOF, TKN_NONE};
TokenCodeType followRoutineDeclsList[] = {TKN_SEMICOLON, TKN_CODE, TKN_EOF, TKN_NONE};

namespace
{
    /// <summary>Clears a new routine or module symbol's definition (no parameters, locals or code yet).</summary>
    void clearRoutineDefinition(SymTableNodePtr routineIdPtr, DefinitionType key)
    {
        routineIdPtr->defn.key = key;
        routineIdPtr->defn.info.routine.key = RTN_DECLARED;
        routineIdPtr->defn.info.routine.paramCount = 0;
        routineIdPtr->defn.info.routine.totalParamSize = 0;
        routineIdPtr->defn.info.routine.totalLocalSize = 0;
        routineIdPtr->defn.info.routine.params = nullptr;
        routineIdPtr->defn.info.routine.locals = nullptr;
        routineIdPtr->defn.info.routine.localSymTable = nullptr;
        routineIdPtr->defn.info.routine.codeSegment = nullptr;
        routineIdPtr->library = CurLibrary;
        routineIdPtr->typePtr = &DummyType;
        routineIdPtr->labelIndex = 0;
    }

    /// <summary>
    /// Compiles the statements of a code block up to <paramref name="endToken"/> (not included), resynchronising
    /// after each.
    /// </summary>
    void compileStatements(TokenCodeType endToken)
    {
        if (curToken == endToken)
        {
            return;
        }

        do
        {
            statement();

            while (curToken == TKN_SEMICOLON)
            {
                getToken();
            }

            if (curToken == endToken)
            {
                break;
            }

            synchronize(statementStartList, nullptr, nullptr);
        } while (tokenIn(statementStartList));
    }

    /// <summary>After a header, expects its semicolon (a declaration or statement there means it's missing).</summary>
    void headerSemicolon()
    {
        synchronize(followHeaderList, declarationStartList, statementStartList);

        if (curToken == TKN_SEMICOLON)
        {
            getToken();
        }
        else if (tokenIn(declarationStartList) || tokenIn(statementStartList))
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
        }
    }
}

auto ABLi_init(uint32_t, uint32_t, uint32_t, uint32_t stackSize, uint32_t maxCodeBufferSize, uint32_t maxModules,
               uint32_t maxStaticVariables, void (*debuggerPrintCallback)(char* s), int debugInfo, int debug,
               int profile) -> void
{
    MaxWatchesPerModule = 20;
    MaxBreakPointsPerModule = 20;
    ABLenabled = 1;
    MaxBreaks = 50;
    MaxWatches = 50;
    debugger = nullptr;
    NumModules = 0;
    ModuleRegistry = nullptr;
    ModuleInstanceRegistry = nullptr;
    LibraryInstanceRegistry = nullptr;
    MaxModules = 0;
    MaxLibraries = 0;
    NumModulesRegistered = 0;
    NumModuleInstances = 0;
    CurModule = nullptr;
    errorCount = 0;
    codeBuffer = nullptr;
    codeBufferPtr = nullptr;
    codeSegmentPtr = nullptr;
    codeSegmentLimit = nullptr;
    statementStartPtr = nullptr;
    execStatementCount = 0;
    stack = nullptr;
    tos = nullptr;
    stackFrameBasePtr = nullptr;
    StaticDataPtr = nullptr;
    StaticVariablesSizes = nullptr;
    eternalOffset = 0;
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
    ProfileABL = profile;
    Crunch = 1;
    level = 0;
    lineNumber = 0;
    FileNumber = 0;
    sourceFile = nullptr;
    printFlag = 1;
    blockFlag = 0;
    blockType = BLOCK_MODULE;
    CurModuleIdPtr = nullptr;
    CurRoutineIdPtr = nullptr;
    DumbGetCharOn = 0;
    NumOpenFiles = 0;
    NumSourceFiles = 0;
    bufferOffset = 0;
    bufferp = sourceBuffer;
    tokenp = tokenString;
    digitCount = 0;
    countError = 0;
    pageNumber = 0;
    lineCount = 50;
    IsUnitOrder = 0;
    TacOrderOrigin = 1;
    CurGroup = nullptr;
    CurObject = nullptr;
    CurObjectClass = 0;
    CurWarrior = nullptr;
    CurContact = nullptr;
    eofFlag = 0;
    ExitWithReturn = 0;
    ExitFromTacOrder = 0;
    numLibrariesLoaded = 0;

    for (auto& code : charTable)
    {
        code = CHR_SPECIAL;
    }

    for (int32_t ch = '0'; ch <= '9'; ch++)
    {
        charTable[ch] = CHR_DIGIT;
    }

    for (int32_t ch = 'A'; ch <= 'Z'; ch++)
    {
        charTable[ch] = CHR_LETTER;
    }

    for (int32_t ch = 'a'; ch <= 'z'; ch++)
    {
        charTable[ch] = CHR_LETTER;
    }

    charTable['"'] = CHR_DQUOTE;
    charTable[0x7f] = CHR_EOF;

    MaxCodeBufferSize = static_cast<int32_t>(maxCodeBufferSize);
    codeBuffer = AblMemory.AllocateArray<char>(maxCodeBufferSize);

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
    stack = AblMemory.AllocateArray<StackItem>(stackItems);

    initSymTable();
    initModuleRegistry(static_cast<int32_t>(maxModules));
    initLibraryRegistry(10);

    IncludeDebugInfo = debugInfo;

    if (debug)
    {
        IncludeDebugInfo = 1;
        debugger = new Debugger;

        if (!debugger)
        {
            Fatal(0, " Unable to initialize ABL Debugger. ");
        }

        debugger->init(debuggerPrintCallback, nullptr);
    }

    if (ProfileABL)
    {
        ABL_OpenProfileLog();
    }
}

auto ABLi_preProcess(char* sourceFileName, int32_t* numErrors, int32_t* numLinesProcessed, int32_t* numFilesProcessed,
                     int printLines) -> int32_t
{
    // Already compiled from this file?
    for (int32_t i = 0; i < NumModulesRegistered; i++)
    {
        if (std::strcmp(MCPort::StrLwr(sourceFileName), ModuleRegistry[i].fileName) == 0)
        {
            return i;
        }
    }

    PrintEnabled = debugger != nullptr;
    level = 0;
    printFlag = printLines;
    lineNumber = 0;
    FileNumber = 0;
    StringFunctionsEnabled = 1;
    NumSourceFiles = 0;
    NumLibrariesUsed = 0;
    sourceFile = nullptr;
    blockFlag = 0;
    blockType = BLOCK_MODULE;
    bufferOffset = 0;
    bufferp = sourceBuffer;
    tokenp = tokenString;
    digitCount = 0;
    countError = 0;
    pageNumber = 0;
    errorCount = 0;
    execStatementCount = 0;
    eofFlag = 0;
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
    int32_t openErr = openSourceFile(sourceFileName);

    if (openErr != 0)
    {
        return openErr;
    }

    codeBufferPtr = codeBuffer;
    getToken();
    SymTableNodePtr moduleIdPtr = moduleHeader();
    CurModuleIdPtr = moduleIdPtr;
    CurRoutineIdPtr = moduleIdPtr;
    headerSemicolon();

    declarations(moduleIdPtr, 1);
    synchronize(followModuleDeclsList, nullptr, nullptr);

    if (curToken != TKN_CODE)
    {
        syntaxError(ABL_ERR_SYNTAX_MISSING_CODE);
    }

    crunchToken();
    blockType = BLOCK_MODULE;
    blockFlag = 1;
    getToken();
    compileStatements(CurLibrary ? TKN_END_LIBRARY : TKN_END_MODULE);

    if (CurLibrary)
    {
        ifTokenGetElseError(TKN_END_LIBRARY, ABL_ERR_SYNTAX_MISSING_END_LIBRARY);
    }
    else
    {
        ifTokenGetElseError(TKN_END_MODULE, ABL_ERR_SYNTAX_MISSING_END_MODULE);
    }

    blockFlag = 0;
    moduleIdPtr->defn.info.routine.localSymTable = exitScope();
    moduleIdPtr->defn.info.routine.codeSegment = createCodeSegment();
    ifTokenGetElseError(TKN_PERIOD, ABL_ERR_SYNTAX_MISSING_PERIOD);

    while (curToken != TKN_EOF)
    {
        syntaxError(ABL_ERR_SYNTAX_VALUE_OUT_OF_RANGE);
        getToken();
    }

    closeSourceFile();

    // Register the module.
    int32_t moduleHandle = NumModulesRegistered;
    ModuleEntry& entry = ModuleRegistry[moduleHandle];
    entry.fileName = AblMemory.CopyString(MCPort::StrLwr(sourceFileName));
    entry.moduleIdPtr = moduleIdPtr;
    entry.numSourceFiles = NumSourceFiles;
    entry.sourceFiles = AblMemory.AllocateArray<char*>(static_cast<size_t>(NumSourceFiles));

    for (int32_t i = 0; i < NumSourceFiles; i++)
    {
        entry.sourceFiles[i] = AblMemory.CopyString(SourceFiles[i]);
    }

    if (NumLibrariesUsed > 0)
    {
        entry.numLibrariesUsed = NumLibrariesUsed;
        entry.librariesUsed = AblMemory.AllocateArray<ABLModule*>(static_cast<size_t>(NumLibrariesUsed));

        for (int32_t i = 0; i < NumLibrariesUsed; i++)
        {
            entry.librariesUsed[i] = LibrariesUsed[i];
        }
    }

    entry.numStaticVars = NumStaticVariables;
    entry.sizeStaticVars = nullptr;
    entry.totalSizeStaticVars = 0;

    if (NumStaticVariables != 0)
    {
        entry.sizeStaticVars = AblMemory.AllocateArray<int32_t>(static_cast<size_t>(NumStaticVariables));

        for (int32_t i = 0; i < NumStaticVariables; i++)
        {
            entry.sizeStaticVars[i] = StaticVariablesSizes[i];
        }

        entry.totalSizeStaticVars = NumStaticVariables * 4;

        for (int32_t i = 0; i < entry.numStaticVars; i++)
        {
            entry.totalSizeStaticVars += entry.sizeStaticVars[i];
        }
    }

    entry.numInstances = 0;
    NumModulesRegistered = moduleHandle + 1;

    if (numLinesProcessed)
    {
        *numLinesProcessed = lineNumber;
    }

    if (numFilesProcessed)
    {
        *numFilesProcessed = FileNumber;
    }

    if (numErrors)
    {
        *numErrors = errorCount;
    }

    return moduleHandle;
}

auto ABLi_execute(SymTableNodePtr moduleIdPtr, SymTableNodePtr, ABLParam* paramList, StackItemPtr returnVal) -> int32_t
{
    NumExecutions++;
    tos = stack + eternalOffset;
    CurModuleIdPtr = nullptr;
    stackFrameBasePtr = tos + 1;
    CurRoutineIdPtr = nullptr;
    errorCount = 0;
    execStatementCount = 0;
    level = 1;
    CallStackLevel = 0;

    // The module's frame header.
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
                        return 0;
                    }

                    pushInteger(param->integer);
                }

                // Faithful: nothing was pushed for an array parameter, so this copies the block the top item
                // points to.
                if (formalTypePtr->form == FRM_ARRAY)
                {
                    int32_t size = formalTypePtr->size;
                    Address copy = static_cast<Address>(AblMemory.Allocate(static_cast<size_t>(size)));

                    if (!copy)
                    {
                        Fatal(0, " ABL: Unable to AblStackHeap->malloc module formal array param ");
                    }

                    Address source = tos->address;
                    tos->address = copy;
                    std::memcpy(copy, source, static_cast<size_t>(size));
                }
            }
            else
            {
                // A reference parameter points into the list, so the module can write back.
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
                    return 0;
                }

                pushAddress(paramAddress);
            }
        }
    }

    execute(moduleIdPtr);

    if (returnVal)
    {
        *returnVal = returnValue;
    }

    return execStatementCount;
}

auto ABLi_close() -> void
{
    destroyModuleRegistry();
    destroyLibraryRegistry();

    StaticVariablesSizes = nullptr;
    codeBuffer = nullptr;
    stack = nullptr;

    if (debugger)
    {
        debugger->destroy();
        delete debugger;
        debugger = nullptr;
    }

    AblMemory.Clear();

    ABL_CloseProfileLog();
    ABLenabled = 0;
}

auto ABLi_loadLibrary(char* sourceFileName, int32_t* numErrors, int32_t* numLinesProcessed, int32_t* numFilesProcessed,
                      int printLines) -> int32_t
{
    ABLModule* library = new ABLModule;

    if (!library)
    {
        return static_cast<int32_t>(0xFAAF000B);
    }

    CurLibrary = library;
    int32_t moduleHandle = ABLi_preProcess(sourceFileName, numErrors, numLinesProcessed, numFilesProcessed, printLines);

    // Anything but the module just registered (a library compiled before, or an error code) fails.
    if (moduleHandle < NumModulesRegistered - 1)
    {
        library->destroy();
        delete library;
        CurLibrary = nullptr;
        return -1;
    }

    int32_t err = library->init(moduleHandle);
    Assert(err == 0, static_cast<uint32_t>(err), " Error Loading ABL Library ");
    library->setName(sourceFileName);
    CurLibrary = nullptr;
    LibraryInstanceRegistry[numLibrariesLoaded] = library;
    numLibrariesLoaded++;
    return 0;
}

auto ABLi_createParamList(int32_t numParameters) -> ABLParam*
{
    if (numParameters == 0)
    {
        return nullptr;
    }

    // Room for one parameter more than asked, as in the original.
    return AblMemory.AllocateArray<ABLParam>(static_cast<size_t>(numParameters + 1));
}

auto ABLi_setIntegerParam(ABLParam* paramList, int32_t index, int32_t value) -> void
{
    if (paramList)
    {
        paramList[index].type = ABL_PARAM_INTEGER;
        paramList[index].integer = value;
    }
}

auto ABLi_setRealParam(ABLParam* paramList, int32_t index, float value) -> void
{
    if (paramList)
    {
        paramList[index].type = ABL_PARAM_REAL;
        paramList[index].real = value;
    }
}

auto ABLi_deleteParamList(ABLParam* paramList) -> void
{
    if (paramList)
    {
        AblMemory.Free(paramList);
    }
}

auto ABLi_getModule(int32_t id) -> ABLModule*
{
    if (id > -1 && id < NumModules)
    {
        return ModuleInstanceRegistry[id];
    }

    return nullptr;
}

auto ABLi_enabled() -> int
{
    return ABLenabled;
}

auto moduleHeader() -> SymTableNodePtr
{
    SymTableNodePtr moduleIdPtr = nullptr;

    if (CurLibrary)
    {
        ifTokenGetElseError(TKN_LIBRARY, ABL_ERR_SYNTAX_MISSING_LIBRARY);
    }
    else
    {
        ifTokenGetElseError(TKN_MODULE, ABL_ERR_SYNTAX_MISSING_MODULE);
    }

    if (curToken == TKN_IDENTIFIER)
    {
        searchAndEnterLocalSymTable(moduleIdPtr);
        clearRoutineDefinition(moduleIdPtr, DFN_MODULE);
        getToken();
    }
    else
    {
        syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
    }

    synchronize(followModuleIdList, declarationStartList, statementStartList);
    enterScope(nullptr);

    if (curToken == TKN_LPAREN)
    {
        int32_t paramCount;
        int32_t totalParamSize;
        SymTableNodePtr params = formalParamList(&paramCount, &totalParamSize);
        moduleIdPtr->defn.info.routine.paramCount = paramCount;
        moduleIdPtr->defn.info.routine.totalParamSize = totalParamSize;
        moduleIdPtr->defn.info.routine.params = params;
    }

    // An optional result type.
    moduleIdPtr->typePtr = nullptr;

    if (curToken == TKN_COLON)
    {
        getToken();

        if (curToken == TKN_IDENTIFIER)
        {
            SymTableNodePtr typeIdPtr = nullptr;
            searchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->defn.key != DFN_TYPE)
            {
                syntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            moduleIdPtr->typePtr = typeIdPtr->typePtr;
            getToken();
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            moduleIdPtr->typePtr = &DummyType;
        }
    }

    return moduleIdPtr;
}

auto routine() -> void
{
    SymTableNodePtr routineIdPtr = functionHeader();
    SymTableNodePtr outerRoutineIdPtr = CurRoutineIdPtr;
    CurRoutineIdPtr = routineIdPtr;
    headerSemicolon();

    if (std::strcmp(wordString, "forward") == 0)
    {
        getToken();
        routineIdPtr->defn.info.routine.key = RTN_FORWARD;
    }
    else
    {
        routineIdPtr->defn.info.routine.key = RTN_DECLARED;
        routineIdPtr->defn.info.routine.locals = nullptr;
        declarations(routineIdPtr, 0);
        synchronize(followRoutineDeclsList, nullptr, nullptr);

        if (curToken != TKN_CODE)
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_CODE);
        }

        crunchToken();
        blockType = BLOCK_ROUTINE;
        blockFlag = 1;
        getToken();
        compileStatements(TKN_END_FUNCTION);
        ifTokenGetElseError(TKN_END_FUNCTION, ABL_ERR_SYNTAX_MISSING_END_FUNCTION);
        blockFlag = 0;
        routineIdPtr->defn.info.routine.codeSegment = createCodeSegment();
    }

    routineIdPtr->defn.info.routine.localSymTable = exitScope();
    CurRoutineIdPtr = outerRoutineIdPtr;
}

auto functionHeader() -> SymTableNodePtr
{
    getToken();
    bool forwardFlag = false;
    SymTableNodePtr functionIdPtr = nullptr;
    SymTableNodePtr typeIdPtr = nullptr;

    if (curToken == TKN_IDENTIFIER)
    {
        searchLocalSymTable(functionIdPtr);

        if (!functionIdPtr)
        {
            enterLocalSymTable(functionIdPtr);
            clearRoutineDefinition(functionIdPtr, DFN_FUNCTION);
        }
        else if (functionIdPtr->defn.key == DFN_FUNCTION && functionIdPtr->defn.info.routine.key == RTN_FORWARD)
        {
            forwardFlag = true;
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_REDEFINED_IDENTIFIER);
        }

        getToken();
    }

    synchronize(followFunctionIdList, declarationStartList, statementStartList);
    enterScope(nullptr);

    if (curToken == TKN_LPAREN)
    {
        int32_t paramCount;
        int32_t totalParamSize;
        SymTableNodePtr params = formalParamList(&paramCount, &totalParamSize);

        // A forwarded function had its parameters declared already.
        if (forwardFlag)
        {
            syntaxError(ABL_ERR_SYNTAX_ALREADY_FORWARDED);
        }
        else
        {
            functionIdPtr->defn.info.routine.paramCount = paramCount;
            functionIdPtr->defn.info.routine.totalParamSize = totalParamSize;
            functionIdPtr->defn.info.routine.params = params;
        }
    }
    else if (!forwardFlag)
    {
        functionIdPtr->defn.info.routine.paramCount = 0;
        functionIdPtr->defn.info.routine.totalParamSize = 0;
        functionIdPtr->defn.info.routine.params = nullptr;
    }

    // Faithful: the result type is cleared even for a forwarded function, whose own header must not repeat it.
    functionIdPtr->typePtr = nullptr;

    if (curToken == TKN_COLON)
    {
        getToken();

        if (curToken == TKN_IDENTIFIER)
        {
            searchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->defn.key != DFN_TYPE)
            {
                syntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            if (!forwardFlag)
            {
                functionIdPtr->typePtr = typeIdPtr->typePtr;
            }

            getToken();
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            functionIdPtr->typePtr = &DummyType;
        }

        if (forwardFlag)
        {
            syntaxError(ABL_ERR_SYNTAX_ALREADY_FORWARDED);
        }
    }

    return functionIdPtr;
}

auto formalParamList(int32_t* count, int32_t* totalSize) -> SymTableNodePtr
{
    SymTableNodePtr lastIdPtr = nullptr;
    SymTableNodePtr firstIdPtr = nullptr;
    int32_t paramCount = 0;
    // Parameters follow the 4-item frame header.
    int32_t paramOffset = 4;

    getToken();

    for (;;)
    {
        // Each parameter is "type name" or "@type name" (by reference).
        DefinitionType paramDefn;
        TypePtr paramTypePtr;

        if (curToken == TKN_IDENTIFIER)
        {
            paramDefn = DFN_VALPARAM;
        }
        else if (curToken == TKN_REF)
        {
            paramDefn = DFN_REFPARAM;
            getToken();
        }
        else
        {
            ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
            *count = paramCount;
            *totalSize = paramOffset - 4;
            return firstIdPtr;
        }

        if (curToken == TKN_IDENTIFIER)
        {
            SymTableNodePtr typeIdPtr = nullptr;
            searchAndFindAllSymTables(typeIdPtr);

            if (typeIdPtr->defn.key != DFN_TYPE)
            {
                syntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            }

            paramTypePtr = typeIdPtr->typePtr;
            getToken();
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            paramTypePtr = &DummyType;
        }

        if (curToken == TKN_IDENTIFIER)
        {
            SymTableNodePtr paramIdPtr = nullptr;
            searchAndEnterLocalSymTable(paramIdPtr);
            paramIdPtr->defn.key = paramDefn;
            paramIdPtr->labelIndex = 0;
            paramIdPtr->typePtr = paramTypePtr;
            paramIdPtr->defn.info.data.offset = paramOffset++;
            paramCount++;

            if (!firstIdPtr)
            {
                firstIdPtr = paramIdPtr;
            }

            if (lastIdPtr)
            {
                lastIdPtr->next = paramIdPtr;
            }

            lastIdPtr = paramIdPtr;
            getToken();
        }
        else
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
        }

        synchronize(followParamsList, nullptr, nullptr);
        ifTokenGet(TKN_COMMA);
    }
}

auto routineCall(SymTableNodePtr routineIdPtr, int paramCheck) -> TypePtr
{
    SymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
    TypePtr resultType;
    RoutineKey key = routineIdPtr->defn.info.routine.key;

    if (key != RTN_DECLARED && key != RTN_FORWARD && paramCheck)
    {
        resultType = standardRoutineCall(routineIdPtr);
    }
    else
    {
        resultType = declaredRoutineCall(routineIdPtr, paramCheck);
    }

    CurRoutineIdPtr = thisRoutineIdPtr;
    return resultType;
}

auto declaredRoutineCall(SymTableNodePtr routineIdPtr, int paramCheck) -> TypePtr
{
    actualParamList(routineIdPtr, paramCheck);
    return routineIdPtr->typePtr;
}

auto actualParamList(SymTableNodePtr routineIdPtr, int paramCheck) -> void
{
    SymTableNodePtr formalIdPtr = paramCheck ? routineIdPtr->defn.info.routine.params : nullptr;
    DefinitionType formalDefn = DFN_UNDEFINED;
    TypePtr formalTypePtr = nullptr;

    if (curToken == TKN_LPAREN)
    {
        do
        {
            if (paramCheck && formalIdPtr)
            {
                formalDefn = formalIdPtr->defn.key;
                formalTypePtr = formalIdPtr->typePtr;
            }

            getToken();

            if (!formalIdPtr || formalDefn == DFN_VALPARAM || !paramCheck)
            {
                // A value parameter (or no checking): any expression of a compatible type.
                TypePtr actualTypePtr = expression();

                if (paramCheck)
                {
                    if (formalIdPtr)
                    {
                        if (!isAssignTypeCompatible(formalTypePtr, actualTypePtr))
                        {
                            syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
                        }

                        formalIdPtr = formalIdPtr->next;
                    }
                    else
                    {
                        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
                    }
                }
            }
            else
            {
                // A reference parameter: a variable of exactly the formal's type.
                if (curToken == TKN_IDENTIFIER)
                {
                    SymTableNodePtr actualIdPtr = nullptr;
                    searchAndFindAllSymTables(actualIdPtr);

                    if (formalTypePtr != variable(actualIdPtr, USE_REFPARAM))
                    {
                        syntaxError(ABL_ERR_SYNTAX_INCOMPATIBLE_TYPES);
                    }
                }
                else
                {
                    expression();
                    syntaxError(ABL_ERR_SYNTAX_INVALID_REF_PARAM);
                }

                formalIdPtr = formalIdPtr->next;
            }

            synchronize(followParamList, statementEndList, nullptr);
        } while (curToken == TKN_COMMA);

        ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    }

    if (paramCheck && formalIdPtr)
    {
        syntaxError(ABL_ERR_SYNTAX_WRONG_NUMBER_OF_PARAMS);
    }
}
