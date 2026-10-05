#include "stdafx.h"
#include "abl/abldbug.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablsymt.h"
#include "abl/ablxexpr.h"
#include "gui/afont.h"
#include "gui/aport.h"
#include "gui/atextbox.h"
#include "gui/updisp.h"
#include "lib/aerror.h"
#include "network/multplyr.h"
#include "platform/MCInput.h"
#include "vfx/vfxfuncs.h"

char Debugger::message[MAXLEN_DEBUGGER_MESSAGE];
Debugger* debugger = nullptr;
ScrollingTextWindow* ABLDebuggerOut = nullptr;
aTextObject* ABLDebuggerIn = nullptr;

namespace
{
    /// <summary>
    /// Where variable <paramref name="idPtr"/> lives in the running frame (a local of an enclosing level through the
    /// static links), in the module's statics or among the eternals.
    /// </summary>
    /// <returns>Its item, or <paramref name="fallback"/> for any other variable type (the original then read the
    /// caller's buffer).</returns>
    auto variableItem(SymTableNodePtr idPtr, StackItemPtr fallback) -> StackItemPtr
    {
        switch (idPtr->defn.info.data.varType)
        {
            case VAR_TYPE_NORMAL:
            {
                StackItemPtr frame = stackFrameBasePtr;

                for (int32_t delta = level - idPtr->level; delta > 0; delta--)
                {
                    frame = reinterpret_cast<StackItemPtr>(
                        reinterpret_cast<StackFrameHeaderPtr>(frame)->staticLink.address);
                }

                return frame + idPtr->defn.info.data.offset;
            }

            case VAR_TYPE_STATIC:
                return StaticDataPtr + idPtr->defn.info.data.offset;
            case VAR_TYPE_ETERNAL:
                return stack + idPtr->defn.info.data.offset;
            default:
                return fallback;
        }
    }
}

auto WatchManager::init(int32_t max) -> int32_t
{
    maxWatches = max;
    numWatches = 0;
    watches = AblMemory.AllocateArray<Watch>(static_cast<size_t>(max));
    return watches ? 0 : -1;
}

auto WatchManager::destroy() -> void
{
    if (watches)
    {
        AblMemory.Free(watches);
        watches = nullptr;
    }

    maxWatches = 0;
    numWatches = 0;
}

auto WatchManager::add(SymTableNodePtr idPtr) -> WatchPtr
{
    DefinitionType idDefn = idPtr->defn.key;

    if (idDefn != DFN_CONST && idDefn != DFN_VAR && idDefn != DFN_VALPARAM && idDefn != DFN_REFPARAM)
    {
        return nullptr;
    }

    if (idPtr->info)
    {
        return idPtr->info;
    }

    if (numWatches >= maxWatches)
    {
        return nullptr;
    }

    WatchPtr watch = &watches[numWatches];
    idPtr->info = watch;
    watch->idPtr = idPtr;
    watch->store = 0;
    watch->breakOnStore = 0;
    watch->fetch = 0;
    watch->breakOnFetch = 0;
    numWatches++;
    return watch;
}

auto WatchManager::remove(SymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    if (!idPtr->info)
    {
        return 2;
    }

    int32_t removeIndex = 0;

    while (removeIndex < numWatches && &watches[removeIndex] != idPtr->info)
    {
        removeIndex++;
    }

    numWatches--;
    idPtr->info = nullptr;

    // Original behaviour (OB-040): the shift never advances, so only the next watch moves down (the original copied
    // it numWatches - removeIndex times); the ones after it stay put and the last one drops off the count.
    if (removeIndex < numWatches)
    {
        watches[removeIndex] = watches[removeIndex + 1];
        watches[removeIndex].idPtr->info = &watches[removeIndex];
    }

    return 0;
}

auto WatchManager::removeAll() -> int32_t
{
    int32_t removed = numWatches;

    for (int32_t i = 0; i < numWatches; i++)
    {
        SymTableNodePtr idPtr = watches[i].idPtr;
        watches[i] = Watch{};
        idPtr->info = nullptr;
    }

    numWatches = 0;
    return removed;
}

auto WatchManager::setStore(SymTableNodePtr idPtr, int on, int breakOnStore) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    WatchPtr watch = idPtr->info;

    if (on)
    {
        if (!watch)
        {
            watch = add(idPtr);

            if (!watch)
            {
                return 2;
            }
        }

        watch->store = 1;
        watch->breakOnStore = breakOnStore;
        return 0;
    }

    if (watch)
    {
        // Keep the watch while it still watches fetches.
        if (watch->fetch)
        {
            watch->store = 0;
            watch->breakOnStore = 0;
            return 0;
        }

        remove(idPtr);
    }

    return 0;
}

auto WatchManager::setFetch(SymTableNodePtr idPtr, int on, int breakOnFetch) -> int32_t
{
    if (!idPtr)
    {
        return 1;
    }

    WatchPtr watch = idPtr->info;

    if (on)
    {
        if (!watch)
        {
            watch = add(idPtr);

            if (!watch)
            {
                return 2;
            }
        }

        watch->fetch = 1;
        watch->breakOnFetch = breakOnFetch;
        return 0;
    }

    if (watch)
    {
        if (watch->store)
        {
            watch->fetch = 0;
            watch->breakOnFetch = 0;
            return 0;
        }

        remove(idPtr);
    }

    return 0;
}

auto WatchManager::getStore(SymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr->info)
    {
        return 0;
    }

    return idPtr->info->store;
}

auto WatchManager::getFetch(SymTableNodePtr idPtr) -> int32_t
{
    if (!idPtr->info)
    {
        return 0;
    }

    return idPtr->info->fetch;
}

auto WatchManager::print() -> void
{
}

auto BreakPointManager::init(int32_t max) -> int32_t
{
    maxBreakPoints = max;
    numBreakPoints = 0;
    breakPoints = AblMemory.AllocateArray<int32_t>(static_cast<size_t>(max));
    return breakPoints ? 0 : -1;
}

auto BreakPointManager::destroy() -> void
{
    if (breakPoints)
    {
        AblMemory.Free(breakPoints);
        breakPoints = nullptr;
    }

    maxBreakPoints = 0;
    numBreakPoints = 0;
}

auto BreakPointManager::add(int32_t lineNumber) -> int32_t
{
    if (numBreakPoints == maxBreakPoints)
    {
        return 1;
    }

    if (lineNumber < 1)
    {
        return 2;
    }

    int32_t index = 0;

    for (; index < numBreakPoints; index++)
    {
        if (breakPoints[index] == lineNumber)
        {
            return 0;
        }

        if (lineNumber < breakPoints[index])
        {
            break;
        }
    }

    // Original behaviour (OB-041): the make-room shift copies forwards, so every break point after the insertion
    // point becomes a copy of the one that was there.
    for (int32_t i = index; i < numBreakPoints; i++)
    {
        breakPoints[i + 1] = breakPoints[i];
    }

    numBreakPoints++;
    breakPoints[index] = lineNumber;
    return 0;
}

auto BreakPointManager::remove(int32_t lineNumber) -> int32_t
{
    int32_t index = 0;

    while (index < numBreakPoints && breakPoints[index] != lineNumber)
    {
        index++;
    }

    // Original behaviour (OB-042): the count drops even when there is no break point on that line, which loses the
    // last one.
    numBreakPoints--;

    for (int32_t i = index; i < numBreakPoints; i++)
    {
        breakPoints[i] = breakPoints[i + 1];
    }

    return 0;
}

auto BreakPointManager::removeAll() -> int32_t
{
    int32_t removed = numBreakPoints;
    numBreakPoints = 0;
    return removed;
}

auto BreakPointManager::isBreakPoint(int32_t lineNumber) -> int
{
    for (int32_t i = 0; i < numBreakPoints; i++)
    {
        if (breakPoints[i] == lineNumber)
        {
            return 1;
        }
    }

    return 0;
}

auto BreakPointManager::print() -> void
{
}

auto Debugger::init(void (*callback)(char* s), ABLModule* _module) -> int32_t
{
    printCallback = callback;
    module = _module;

    if (_module)
    {
        watchManager = _module->watchManager;
        breakPointManager = _module->breakPointManager;
    }

    return 0;
}

auto Debugger::destroy() -> void
{
}

auto Debugger::print(char* s) -> int32_t
{
    if (printCallback)
    {
        printCallback(s);
    }

    return 0;
}

auto Debugger::setModule(ABLModule* _module) -> void
{
    module = _module;
    breakPointManager = _module->breakPointManager;
    watchManager = _module->watchManager;
    // Faithful: the module's trace flag sets all three trace modes (its traceEntry/traceExit are not read).
    step = _module->step;
    traceExit = _module->trace;
    traceEntry = _module->trace;
    trace = _module->trace;
}

auto Debugger::setWatch(int32_t states) -> int32_t
{
    getToken();

    if (curToken != TKN_IDENTIFIER)
    {
        if (curToken == TKN_SEMICOLON)
        {
            print(const_cast<char*>("Variables currently watched:\n"));
            watchManager->print();
        }

        return 0;
    }

    SymTableNodePtr idPtr = nullptr;
    searchAndFindAllSymTables(idPtr);
    getToken();
    // Bits: 1 store off, 2 store on, 4 fetch off, 8 fetch on, 16 break.
    int breakFlag = (states >> 4) & 1;

    if (states & 1)
    {
        watchManager->setStore(idPtr, 0, breakFlag);
    }
    else if (states & 2)
    {
        watchManager->setStore(idPtr, 1, breakFlag);
    }

    if (states & 4)
    {
        watchManager->setFetch(idPtr, 0, breakFlag);
    }
    else if (states & 8)
    {
        watchManager->setFetch(idPtr, 1, breakFlag);
    }

    return 0;
}

auto Debugger::addBreakPoint() -> int32_t
{
    getToken();

    if (curToken == TKN_NUMBER)
    {
        if (curLiteral.type == LIT_INTEGER)
        {
            breakPointManager->add(curLiteral.value.integer);
        }

        getToken();
    }
    else if (curToken == TKN_SEMICOLON)
    {
        // Faithful: the break point list is announced with the watch list's title.
        print(const_cast<char*>("Variables currently watched:\n"));
        breakPointManager->print();
    }

    return 0;
}

auto Debugger::removeBreakPoint() -> int32_t
{
    getToken();

    if (curToken == TKN_NUMBER)
    {
        if (curLiteral.type == LIT_INTEGER)
        {
            breakPointManager->remove(curLiteral.value.integer);
        }

        getToken();
    }
    else if (curToken == TKN_SEMICOLON)
    {
        breakPointManager->removeAll();
    }

    return 0;
}

auto Debugger::sprintStatement(char* dest) -> void
{
    bool done = false;
    const char* code = statementStartPtr;

    do
    {
        TokenCodeType token = static_cast<TokenCodeType>(*code);
        const char* next = code + 1;

        switch (token)
        {
            case TKN_SEMICOLON:
            case TKN_END_IF:
            case TKN_END_WHILE:
            case TKN_END_FOR:
            case TKN_END_FUNCTION:
            case TKN_END_MODULE:
            case TKN_END_LIBRARY:
            case TKN_END_CASE:
            case TKN_END_SWITCH:
            case TKN_THEN:
                done = true;
                break;
            case TKN_STATEMENT_MARKER:
                // The next statement.
                return;
            default:
                break;
        }

        switch (token)
        {
            case TKN_IDENTIFIER:
            case TKN_NUMBER:
            case TKN_STRING:
            {
                SymTableNodePtr idPtr;
                std::memcpy(&idPtr, next, sizeof(idPtr));
                std::strcat(dest, " ");
                std::strcat(dest, idPtr->name);
                next += CODE_SYMBOL_PTR_SIZE;
                break;
            }

            case TKN_ADDRESS_MARKER:
                next += CODE_ADDRESS_SIZE;
                break;
            default:
            {
                std::strcat(dest, " ");
                std::strcat(dest, TokenStrings[token]);
                break;
            }
        }

        code = next;
    } while (!done);
}

auto Debugger::sprintLineNumber(char* dest) -> void
{
    std::sprintf(dest, "LINE#");
}

auto Debugger::sprintDataValue(char* dest, StackItemPtr data, TypePtr dataType) -> void
{
    if (dataType->form == FRM_ENUM && dataType != BooleanTypePtr)
    {
        dataType = IntegerTypePtr;
    }

    if (dataType == IntegerTypePtr)
    {
        std::sprintf(dest, "%d", data->integer);
    }
    else if (dataType == RealTypePtr)
    {
        std::sprintf(dest, "%0.6f", static_cast<double>(data->real));
    }
    else if (dataType == BooleanTypePtr)
    {
        std::sprintf(dest, "%s", data->integer == 1 ? "true" : "false");
    }
    else if (dataType == CharTypePtr)
    {
        std::sprintf(dest, "%c", data->byte);
    }
    else if (dataType->form == FRM_ARRAY)
    {
        if (dataType->info.array.elementTypePtr == CharTypePtr)
        {
            std::sprintf(dest, "CHAR ARRAY");
        }
        else
        {
            std::sprintf(dest, "ARRAY");
        }
    }
}

auto Debugger::sprintSimpleValue(char* dest, SymTableNodePtr symbol) -> int32_t
{
    TypePtr typePtr = symbol->typePtr;

    if (symbol->defn.key == DFN_CONST)
    {
        if (typePtr == IntegerTypePtr)
        {
            std::sprintf(dest, "%d", symbol->defn.info.constant.value.integer);
        }
        else if (typePtr == CharTypePtr)
        {
            std::sprintf(dest, "%c", symbol->defn.info.constant.value.character);
        }
        else
        {
            std::sprintf(dest, "%.4f", static_cast<double>(symbol->defn.info.constant.value.real));
        }

        return 0;
    }

    StackItemPtr valuePtr = variableItem(symbol, reinterpret_cast<StackItemPtr>(dest));

    if (symbol->defn.key == DFN_REFPARAM && typePtr->form != FRM_ARRAY)
    {
        valuePtr = reinterpret_cast<StackItemPtr>(valuePtr->address);
    }

    TypePtr baseTypePtr = baseType(typePtr);

    if (typePtr->form == FRM_ARRAY)
    {
        std::sprintf(dest, "ARRAY");
    }
    else if (baseTypePtr == IntegerTypePtr || typePtr->form == FRM_ENUM)
    {
        std::sprintf(dest, "%d", valuePtr->integer);
    }
    else if (baseTypePtr == CharTypePtr)
    {
        std::sprintf(dest, "\"%c\"", valuePtr->byte);
    }
    else
    {
        std::sprintf(dest, "%.4f", static_cast<double>(valuePtr->real));
    }

    return 0;
}

auto Debugger::sprintArrayValue(char* dest, SymTableNodePtr symbol, char* subscriptString) -> int32_t
{
    if (symbol->defn.key == DFN_CONST)
    {
        std::sprintf(dest, "\"%s\"", symbol->defn.info.constant.value.stringPtr);
        return 0;
    }

    StackItemPtr arrayItem = variableItem(symbol, reinterpret_cast<StackItemPtr>(dest));
    TypePtr typePtr = symbol->typePtr;
    char* element = arrayItem->address;

    if (subscriptString)
    {
        // "[i][j]..." (or "[i,j]"): skip the first bracket, then split on commas and closing brackets.
        for (char* subscript = std::strtok(subscriptString + 1, ",]"); subscript;
             subscript = std::strtok(nullptr, ",]"))
        {
            int32_t index = std::atoi(subscript);

            if (index < 0 || index >= typePtr->info.array.elementCount)
            {
                return 1;
            }

            typePtr = typePtr->info.array.elementTypePtr;
            element += typePtr->size * index;
        }
    }

    TypePtr baseTypePtr = baseType(typePtr);

    if (typePtr->form == FRM_ARRAY)
    {
        if (typePtr->info.array.elementTypePtr == CharTypePtr)
        {
            std::sprintf(dest, "\"%s\"", element);
        }
        else
        {
            std::sprintf(dest, "Could you be more specific?");
        }
    }
    else if (baseTypePtr == IntegerTypePtr || typePtr->form == FRM_ENUM)
    {
        std::sprintf(dest, "%d", *reinterpret_cast<int32_t*>(element));
    }
    else if (baseTypePtr == CharTypePtr)
    {
        std::sprintf(dest, "\"%c\"", *element);
    }
    else
    {
        std::sprintf(dest, "%.4f", static_cast<double>(*reinterpret_cast<float*>(element)));
    }

    return 0;
}

auto Debugger::sprintValue(char* dest, char* exprString) -> int32_t
{
    char* subscripts = std::strchr(exprString, '[');

    if (!subscripts)
    {
        SymTableNodePtr symbol = debugModule->findSymbol(exprString, CurRoutineIdPtr);

        if (!symbol)
        {
            return 1;
        }

        if (symbol->typePtr->form != FRM_ARRAY)
        {
            sprintSimpleValue(dest, symbol);
            return 0;
        }

        symbol = debugModule->findSymbol(exprString, CurRoutineIdPtr);

        if (!symbol)
        {
            return 1;
        }

        sprintArrayValue(dest, symbol, nullptr);
        return 0;
    }

    char subscriptString[256];
    std::strcpy(subscriptString, subscripts);
    *subscripts = '\0';
    SymTableNodePtr symbol = debugModule->findSymbol(exprString, CurRoutineIdPtr);

    if (!symbol)
    {
        return 1;
    }

    sprintArrayValue(dest, symbol, subscriptString);
    return 0;
}

auto Debugger::traceStatementExecution() -> int32_t
{
    int32_t stepping = step;

    if (breakPointManager && breakPointManager->isBreakPoint(execLineNumber))
    {
        std::sprintf(message, "HIT BP: (%d) %s [%d]", module->id, module->name, execLineNumber);
        print(message);
        debugMode();
        return 0;
    }

    if (stepping)
    {
        debugMode();
    }

    return 0;
}

auto Debugger::traceRoutineEntry(SymTableNodePtr idPtr) -> int32_t
{
    if (traceEntry)
    {
        std::sprintf(message, "ENTER (%d) %s:%s", module->id, module->name, idPtr->name);
        print(message);
    }

    return 0;
}

auto Debugger::traceRoutineExit(SymTableNodePtr idPtr) -> int32_t
{
    if (traceExit)
    {
        std::sprintf(message, "EXIT (%d) %s:%s", module->id, module->name, idPtr->name);
        print(message);
    }

    return 0;
}

auto Debugger::traceDataStore(SymTableNodePtr id, TypePtr idType, StackItemPtr target, TypePtr targetType) -> int32_t
{
    WatchPtr watch = id->info;

    if (watch && watch->store)
    {
        char valueString[256];
        sprintDataValue(valueString, target, targetType);
        const char* format =
            idType->form == FRM_ARRAY ? "STORE: (%d) %s [%d] -> %s[#] = %s\n" : "STORE: (%d) %s [%d] -> %s = %s\n";
        std::sprintf(message, format, module->id, module->name, execLineNumber, id->name, valueString);
        print(message);

        if (watch->breakOnStore)
        {
            debugMode();
        }
    }

    return 0;
}

auto Debugger::traceDataFetch(SymTableNodePtr id, TypePtr idType, StackItemPtr data) -> int32_t
{
    WatchPtr watch = id->info;

    if (watch && watch->fetch)
    {
        char valueString[256];
        sprintDataValue(valueString, data, idType);
        const char* format =
            id->typePtr->form == FRM_ARRAY ? "FETCH: (%d) %s [%d] - %s[#] = %s\n" : "FETCH: (%d) %s [%d] - %s = %s\n";
        std::sprintf(message, format, module->id, module->name, execLineNumber, id->name, valueString);
        print(message);

        if (watch->breakOnFetch)
        {
            debugMode();
        }
    }

    return 0;
}

auto Debugger::showValue() -> void
{
    getToken();

    if (curToken == TKN_SEMICOLON)
    {
        print(const_cast<char*>("Bad Expression.\n"));
        return;
    }

    TypePtr expressionTypePtr = expression();

    if (errorCount > 0)
    {
        return;
    }

    char* savedCodeSegmentPtr = codeSegmentPtr;
    TokenCodeType savedCodeToken = codeToken;
    execExpression();

    if (expressionTypePtr->form == FRM_ARRAY)
    {
        print(const_cast<char*>("SHOW ARRAY\n"));
    }
    else
    {
        char valueString[256];
        sprintDataValue(valueString, tos, expressionTypePtr);
        std::strcat(valueString, "\n");
        print(valueString);
    }

    pop();
    codeSegmentPtr = savedCodeSegmentPtr;
    codeToken = savedCodeToken;
}

auto Debugger::assignVariable() -> void
{
    getToken();
}

auto Debugger::displayModuleInstanceRegistry(int32_t) -> void
{
    // Two per line, whatever numCols says.
    for (int32_t row = 0; row < (NumModuleInstances + 1) / 2; row++)
    {
        char line[200];
        ABLModule* left = ModuleInstanceRegistry[row * 2];
        std::sprintf(line, "(%02d) %-20s ", left->id, left->name);

        if (row * 2 + 1 < NumModuleInstances)
        {
            char column[40];
            ABLModule* right = ModuleInstanceRegistry[row * 2 + 1];
            std::sprintf(column, "(%02d) %-20s ", right->id, right->name);
            std::strcat(line, column);
        }

        print(line);
    }
}

auto Debugger::processCommand(int32_t commandId, char* strParam1, int32_t numParam1, ABLModule* moduleParam1) -> void
{
    static char blankLine[] = " ";

    switch (commandId)
    {
        case 0:
        {
            // Select the module (or list them).
            if (!moduleParam1)
            {
                print(blankLine);
                displayModuleInstanceRegistry(2);
                std::sprintf(message, "CURRENT MODULE: %s", debugModule->name);
            }
            else
            {
                debugModule = moduleParam1;
                print(blankLine);
                std::sprintf(message, "SET MODULE: %s", debugModule->name);
            }
            break;
        }
        case 1:
        {
            // Trace on or off (tracing turns stepping off).
            if (numParam1 == 0)
            {
                debugModule->trace = 0;
                debugModule->traceEntry = 0;
                debugModule->traceExit = 0;

                if (module == debugModule)
                {
                    trace = 0;
                    traceEntry = 0;
                    traceExit = 0;
                }
            }
            else
            {
                debugModule->trace = 1;
                debugModule->traceEntry = 1;
                debugModule->traceExit = 1;
                debugModule->step = 0;

                if (module == debugModule)
                {
                    trace = 1;
                    traceEntry = 1;
                    traceExit = 1;
                    step = 0;
                }
            }

            return;
        }
        case 2:
        {
            // Step on or off (stepping turns tracing off).
            if (numParam1 == 0)
            {
                debugModule->step = 0;

                if (module == debugModule)
                {
                    step = 0;
                }
            }
            else
            {
                debugModule->step = 1;
                debugModule->trace = 0;
                debugModule->traceEntry = 0;
                debugModule->traceExit = 0;

                if (module == debugModule)
                {
                    step = 1;
                    trace = 0;
                    traceEntry = 0;
                    traceExit = 0;
                }
            }

            return;
        }
        case 3:
        {
            print(blankLine);
            debugModule->breakPointManager->add(numParam1);
            std::sprintf(message, "SET BP: %s (%d)", debugModule->name, numParam1);
            break;
        }
        case 4:
        {
            print(blankLine);
            debugModule->breakPointManager->remove(numParam1);
            std::sprintf(message, "REMOVE BP: %s (%d)", debugModule->name, numParam1);
            break;
        }
        case 5:
        {
            // Watch: numParam1 holds the setWatch bits.
            print(blankLine);
            ABLModule* watchModule = debugModule;
            SymTableNodePtr idPtr = watchModule->findSymbol(strParam1);

            if (!idPtr)
            {
                print(const_cast<char*>("Unknown identifier in current scope.\n"));
                return;
            }

            WatchManagerPtr moduleWatches = watchModule->watchManager;
            int breakFlag = (numParam1 >> 4) & 1;

            if ((numParam1 & 2) && moduleWatches->setStore(idPtr, 1, breakFlag) == 2)
            {
                print(const_cast<char*>("Reached max watch limit--unable to set watch.\n"));
                return;
            }

            if ((numParam1 & 8) && moduleWatches->setFetch(idPtr, 1, breakFlag) == 2)
            {
                print(const_cast<char*>("Reached max watch limit--unable to set watch.\n"));
                return;
            }

            if (numParam1 & 1)
            {
                moduleWatches->setStore(idPtr, 0, breakFlag);
            }

            if (numParam1 & 4)
            {
                moduleWatches->setFetch(idPtr, 0, breakFlag);
            }

            int32_t watchesStores = moduleWatches->getStore(idPtr);
            int32_t watchesFetches = moduleWatches->getFetch(idPtr);

            if (!watchesStores && !watchesFetches)
            {
                std::sprintf(message, "REMOVE WATCH: %s.%s", watchModule->name, strParam1);
            }
            else
            {
                std::sprintf(message, "SET WATCH: %s.%s (", watchModule->name, strParam1);

                if (watchesStores)
                {
                    std::strcat(message, "s");
                }

                if (watchesFetches)
                {
                    std::strcat(message, "f");
                }

                std::strcat(message, ")");
            }
            break;
        }

        case 6:
        {
            debugModule->watchManager->removeAll();
            return;
        }
        case 7:
        {
            print(blankLine);
            int32_t err = sprintValue(message, strParam1);

            if (err != 0)
            {
                if (err == 1)
                {
                    print(const_cast<char*>("Unknown identifier in current scope."));
                }

                return;
            }
            break;
        }

        case 8:
        {
            // Resume.
            debugCommand = 0;
            return;
        }
        case 9:
        {
            print(blankLine);
            print(const_cast<char*>("b{+|-} <line#>         set/remove breakpt"));
            print(const_cast<char*>("m [0 thru warrior #]   set current module (or list them)"));
            print(const_cast<char*>("w[f|s]{+|-}{.} <variable> set/remove variable watch (fetch & store)"));
            print(const_cast<char*>("p <variable>           display current value of variable"));
            print(const_cast<char*>("s{+|-}                 start/stop step mode"));
            print(const_cast<char*>("t{+|-}                 start/stop trace mode"));
            print(const_cast<char*>("??                     current module info"));
            print(const_cast<char*>("?                      help"));
            return;
        }
        case 10:
        {
            print(blankLine);
            std::sprintf(message, "CURRENT MODULE: %s", debugModule->name);
            print(message);
            int32_t numStatics;
            int32_t staticsSize;
            int32_t largestStatic = 0;
            debugModule->getInfo(numStatics, staticsSize, nullptr);

            if (numStatics <= 256)
            {
                int32_t sizeList[256];
                debugModule->getInfo(numStatics, staticsSize, sizeList);

                for (int32_t i = 0; i < numStatics; i++)
                {
                    largestStatic = std::max(largestStatic, sizeList[i]);
                }
            }

            std::sprintf(message, "%d static vars, %d bytes, %d largest", numStatics, staticsSize, largestStatic);
            break;
        }

        default:
            return;
    }

    print(message);
}

auto Debugger::debugMode() -> void
{
    message[0] = '\0';
    debugModule = module;
    sprintStatement(message);
    print(message);
    debugCommand = 1;

    do
    {
        // The game's own loop (aSystem::run) while the debugger window takes the commands.
        startTime = MCPort::PerformanceCounter();

        if (!MCInput::PumpMessages())
        {
            debugCommand = 0;
            halt = 0;
            trace = 0;
            step = 0;
            traceEntry = 0;
            traceExit = 0;
        }

        if (applicationActive)
        {
            UpdateDisplay(takeScreenShot, 0, 0, 0, 0);
            takeScreenShot = 0;
            stopTime = MCPort::PerformanceCounter();

            if (MPlayer)
            {
                MPlayer->processReceiveList();
            }

            CheckMouse();
        }

        prevStart = startTime;
        frameRate = static_cast<float>(countsPerSecond) / static_cast<float>(stopTime - startTime);
    } while (debugCommand);
}

auto ABLi_getDebugger() -> Debugger*
{
    return debugger;
}

#if ABL_DEBUGGER_WINDOWS

ScrollingTextWindow::~ScrollingTextWindow()
{
    aObject::destroy();
}

auto ScrollingTextWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t err = aObject::init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    numColumns = width / 10;
    numLines = height / 10;
    VFX_pane_wipe(port()->frame(), backColor());
    return 0;
}

auto ScrollingTextWindow::resize(int32_t width, int32_t height) -> void
{
    numColumns = width / 10;
    numLines = height / 10;
    aObject::resize(width, height);
    int32_t color = backColor();
    VFX_pane_wipe(port()->frame(), color);
}

auto ScrollingTextWindow::draw() -> void
{
    aObject::draw();
}

auto ScrollingTextWindow::handleEvent(aEvent* event) -> void
{
    aObject::handleEvent(event);
}

auto ScrollingTextWindow::clear() -> void
{
    numColumns = 0;
    numLines = 0;
}

auto ScrollingTextWindow::print(char* s) -> void
{
    _pane* pane = port()->frame();
    VFX_pane_scroll(pane, 0, -10, 0, backColor());
    systemFont->writeString(pane, 2, numLines * 10 - 10, reinterpret_cast<uint8_t*>(s), -1);
}

DebuggerWindow::~DebuggerWindow()
{
    aTitleWindow::destroy();
}

auto DebuggerWindow::init(int32_t xPos, int32_t yPos, int32_t width, int32_t height, char* name) -> int32_t
{
    int32_t err = aTitleWindow::init(xPos, yPos, width, height, name);

    if (err != 0)
    {
        return err;
    }

    setBackColor(10);

    ABLDebuggerOut = new ScrollingTextWindow;

    if (!ABLDebuggerOut)
    {
        Fatal(0, "Not enough memory to make debugger windows");
    }

    err = ABLDebuggerOut->init(0, 0, width, height - 40, const_cast<char*>("ABL Out"));

    if (err != 0)
    {
        return err;
    }

    addChild(ABLDebuggerOut);

    ABLDebuggerIn = new aTextObject;

    if (!ABLDebuggerIn)
    {
        Fatal(0, "Not enough memory to make debugger windows");
    }

    err = ABLDebuggerIn->init(0, this->height() - 36, 260, 36, nullptr);

    if (err != 0)
    {
        return err;
    }

    ABLDebuggerIn->setText(const_cast<char*>("\"?\" for help"));
    addChild(ABLDebuggerIn);
    draw();
    return 0;
}

auto DebuggerWindow::destroy() -> void
{
    if (ABLDebuggerIn)
    {
        ABLDebuggerIn->destroy();
        delete ABLDebuggerIn;
        ABLDebuggerIn = nullptr;
    }

    if (ABLDebuggerOut)
    {
        ABLDebuggerOut->destroy();
        delete ABLDebuggerOut;
        ABLDebuggerOut = nullptr;
    }

    aTitleWindow::destroy();
}

auto DebuggerWindow::resize(int32_t width, int32_t height) -> void
{
    ABLDebuggerOut->resize(width, height - 40);
    ABLDebuggerIn->resize(width, 36);
    ABLDebuggerIn->moveTo(0, height - 36);
    aTitleWindow::resize(width, height);
    draw();
}

#endif
