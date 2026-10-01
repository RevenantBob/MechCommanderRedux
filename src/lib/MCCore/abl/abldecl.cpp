#include "stdafx.h"
#include "abl/abldecl.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablstmt.h"
#include "abl/ablsymt.h"
#include "lib/aerror.h"
#include "lib/heap.h"

TokenCodeType followRoutineList[] = {TKN_SEMICOLON, TKN_EOF, TKN_NONE};
TokenCodeType followDeclarationList[] = {TKN_SEMICOLON, TKN_IDENTIFIER, TKN_EOF, TKN_NONE};
TokenCodeType followVariablesList[] = {TKN_SEMICOLON, TKN_IDENTIFIER, TKN_EOF, TKN_NONE};
TokenCodeType followVarBlockList[] = {TKN_FUNCTION, TKN_CODE, TKN_EOF, TKN_NONE};
TokenCodeType followDimensionList[] = {TKN_COMMA, TKN_RBRACKET, TKN_EOF, TKN_NONE};
TokenCodeType indexTypeStartList[] = {TKN_IDENTIFIER, TKN_NUMBER, TKN_NONE};
TokenCodeType declarationStartList[] = {TKN_CONST, TKN_VAR, TKN_FUNCTION, TKN_NONE};

namespace
{
    /// <summary>After a definition: a semicolon is skipped; a missing one is reported if a declaration or statement
    /// follows.</summary>
    auto skipSemicolon() -> void
    {
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

auto ifTokenGet(TokenCodeType tokenCode) -> void
{
    if (curToken == tokenCode)
    {
        getToken();
    }
}

auto ifTokenGetElseError(TokenCodeType tokenCode, SyntaxErrorType errorCode) -> void
{
    if (curToken == tokenCode)
    {
        getToken();
    }
    else
    {
        syntaxError(errorCode);
    }
}

auto declarations(SymTableNodePtr routineIdPtr, int allowFunctions) -> void
{
    if (curToken == TKN_CONST)
    {
        getToken();
        constDefinitions();
    }

    if (curToken == TKN_TYPE)
    {
        getToken();
        typeDefinitions();
    }

    if (curToken == TKN_VAR)
    {
        getToken();
        varDeclarations(routineIdPtr);
    }

    if (allowFunctions == 0)
    {
        if (curToken == TKN_FUNCTION)
        {
            syntaxError(ABL_ERR_SYNTAX_NO_FUNCTION_NESTING);
        }

        return;
    }
    while (curToken == TKN_FUNCTION)
    {
        routine();
        synchronize(followRoutineList, declarationStartList, statementStartList);
        skipSemicolon();
    }
}

auto constDefinitions() -> void
{
    SymTableNodePtr constantIdPtr = nullptr;

    while (curToken == TKN_IDENTIFIER)
    {
        searchAndEnterLocalSymTable(constantIdPtr);
        constantIdPtr->defn.key = DFN_CONST;
        constantIdPtr->library = CurLibrary;
        getToken();
        ifTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
        doConst(constantIdPtr);
        synchronize(followDeclarationList, declarationStartList, statementStartList);
        skipSemicolon();
    }
}

auto makeStringType(int32_t length) -> TypePtr
{
    TypePtr stringTypePtr = createType();

    if (stringTypePtr == nullptr)
    {
        Fatal(0, " ABL: Unable to AblStackHeap->malloc stringType ");
    }

    stringTypePtr->form = FRM_ARRAY;
    stringTypePtr->size = length;
    stringTypePtr->typeIdPtr = nullptr;
    stringTypePtr->info.array.indexTypePtr = IntegerTypePtr;
    stringTypePtr->info.array.elementTypePtr = CharTypePtr;
    stringTypePtr->info.array.elementCount = length + 1;
    return stringTypePtr;
}

auto doConst(SymTableNodePtr constantIdPtr) -> void
{
    TokenCodeType sign = TKN_PLUS;
    bool sawSign = false;

    if (curToken == TKN_PLUS || curToken == TKN_MINUS)
    {
        sign = curToken;
        sawSign = true;
        getToken();
    }

    Value& value = constantIdPtr->defn.info.constant.value;

    if (curToken == TKN_NUMBER)
    {
        if (curLiteral.type == LIT_INTEGER)
        {
            value.integer = sign == TKN_PLUS ? curLiteral.value.integer : -curLiteral.value.integer;
            constantIdPtr->typePtr = setType(IntegerTypePtr);
        }
        else
        {
            value.real = sign == TKN_PLUS ? curLiteral.value.real : -curLiteral.value.real;
            constantIdPtr->typePtr = setType(RealTypePtr);
        }
    }
    else if (curToken == TKN_IDENTIFIER)
    {
        SymTableNodePtr idPtr = nullptr;
        searchAllSymTables(idPtr);

        if (idPtr == nullptr)
        {
            syntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        }
        else if (idPtr->defn.key != DFN_CONST)
        {
            syntaxError(ABL_ERR_SYNTAX_NOT_A_CONSTANT_IDENTIFIER);
        }
        else
        {
            const Value& otherValue = idPtr->defn.info.constant.value;
            TypePtr typePtr = idPtr->typePtr;

            if (typePtr == IntegerTypePtr)
            {
                value.integer = sign == TKN_PLUS ? otherValue.integer : -otherValue.integer;
                constantIdPtr->typePtr = setType(IntegerTypePtr);
            }
            else if (typePtr == CharTypePtr)
            {
                if (sawSign)
                {
                    syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
                }

                value.character = otherValue.character;
                constantIdPtr->typePtr = setType(CharTypePtr);
            }
            else if (typePtr == RealTypePtr)
            {
                value.real = sign == TKN_PLUS ? otherValue.real : -otherValue.real;
                constantIdPtr->typePtr = setType(RealTypePtr);
            }
            else if (typePtr->form == FRM_ENUM || typePtr->form == FRM_ARRAY)
            {
                // An enumeration value, or a string constant (the pointer to its text is shared).
                if (sawSign)
                {
                    syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
                }

                value = otherValue;
                constantIdPtr->typePtr = setType(typePtr);
            }
        }
    }
    else if (curToken == TKN_STRING)
    {
        if (sawSign)
        {
            syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
        }

        auto length = static_cast<int32_t>(strlen(curLiteral.value.string));

        if (length == 1)
        {
            value.character = curLiteral.value.string[0];
            constantIdPtr->typePtr = setType(CharTypePtr);
        }
        else
        {
            value.stringPtr = static_cast<char*>(AblSymTableHeap->malloc(static_cast<uint32_t>(length + 1)));

            if (value.stringPtr == nullptr)
            {
                Fatal(0, " ABL: Unable to AblStackHeap->malloc array string constant ");
            }

            strcpy(value.stringPtr, curLiteral.value.string);
            constantIdPtr->typePtr = makeStringType(length);
        }
    }
    else
    {
        constantIdPtr->typePtr = nullptr;
        syntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
    }

    getToken();
}

auto typeDefinitions() -> void
{
    SymTableNodePtr typeIdPtr = nullptr;

    while (curToken == TKN_IDENTIFIER)
    {
        searchAndEnterLocalSymTable(typeIdPtr);
        typeIdPtr->defn.key = DFN_TYPE;
        typeIdPtr->library = CurLibrary;
        getToken();
        ifTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
        typeIdPtr->typePtr = doType();

        if (typeIdPtr->typePtr->typeIdPtr == nullptr)
        {
            typeIdPtr->typePtr->typeIdPtr = typeIdPtr;
        }

        synchronize(followDeclarationList, declarationStartList, statementStartList);
        skipSemicolon();
    }
}

auto doType() -> TypePtr
{
    switch (curToken)
    {
        case TKN_IDENTIFIER:
            break;
        case TKN_NUMBER:
        case TKN_STRING:
        case TKN_MINUS:
        case TKN_PLUS:
            // Subrange types: never supported.
            exit(666);
        case TKN_LPAREN:
            return enumerationType();
        default:
        {
            syntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            return nullptr;
        }
    }

    SymTableNodePtr idPtr = nullptr;
    searchAllSymTables(idPtr);

    if (idPtr == nullptr)
    {
        syntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        return nullptr;
    }

    if (idPtr->defn.key != DFN_TYPE)
    {
        syntaxError(ABL_ERR_SYNTAX_NOT_A_TYPE_IDENTIFIER);
        return nullptr;
    }

    TypePtr elementTypePtr = setType(identifierType(idPtr));

    if (curToken != TKN_LBRACKET)
    {
        return elementTypePtr;
    }

    // "type[d1, d2, ...]": one array type per dimension, each the element type of the one before.
    TypePtr arrayTypePtr = createType();

    if (arrayTypePtr == nullptr)
    {
        Fatal(0, " ABL: Unable to AblStackHeap->malloc array type ");
    }

    TypePtr dimensionTypePtr = arrayTypePtr;

    while (true)
    {
        getToken();
        bool validIndex = false;

        if (tokenIn(indexTypeStartList))
        {
            dimensionTypePtr->form = FRM_ARRAY;
            dimensionTypePtr->size = 0;
            dimensionTypePtr->typeIdPtr = nullptr;
            dimensionTypePtr->info.array.indexTypePtr = setType(IntegerTypePtr);

            if (curToken == TKN_IDENTIFIER)
            {
                SymTableNodePtr countIdPtr = nullptr;
                searchAllSymTables(countIdPtr);

                if (countIdPtr == nullptr)
                {
                    syntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
                    validIndex = true;
                }
                else if (countIdPtr->defn.key == DFN_CONST && countIdPtr->typePtr == IntegerTypePtr)
                {
                    dimensionTypePtr->info.array.elementCount = countIdPtr->defn.info.constant.value.integer;
                    validIndex = true;
                }
            }
            else if (curToken == TKN_NUMBER && curLiteral.type == LIT_INTEGER)
            {
                dimensionTypePtr->info.array.elementCount = curLiteral.value.integer;
                validIndex = true;
            }
        }

        if (!validIndex)
        {
            dimensionTypePtr->form = FRM_NONE;
            dimensionTypePtr->size = 0;
            dimensionTypePtr->typeIdPtr = nullptr;
            dimensionTypePtr->info.array.indexTypePtr = nullptr;
            syntaxError(ABL_ERR_SYNTAX_INVALID_INDEX_TYPE);
        }

        getToken();
        synchronize(followDimensionList, nullptr, nullptr);

        if (curToken != TKN_COMMA)
        {
            break;
        }

        TypePtr nextDimensionTypePtr = createType();
        dimensionTypePtr->info.array.elementTypePtr = nextDimensionTypePtr;

        if (nextDimensionTypePtr == nullptr)
        {
            Fatal(0, " ABL: Unable to AblStackHeap->malloc array element Type ");
        }

        dimensionTypePtr = nextDimensionTypePtr;
    }

    ifTokenGetElseError(TKN_RBRACKET, ABL_ERR_SYNTAX_MISSING_RBRACKET);
    dimensionTypePtr->info.array.elementTypePtr = elementTypePtr;
    arrayTypePtr->size = arraySize(arrayTypePtr);
    return arrayTypePtr;
}

auto identifierType(SymTableNodePtr idPtr) -> TypePtr
{
    getToken();
    return idPtr->typePtr;
}

auto enumerationType() -> TypePtr
{
    SymTableNodePtr constantIdPtr = nullptr;
    SymTableNodePtr lastIdPtr = nullptr;
    int32_t constantValue = -1;

    TypePtr typePtr = createType();

    if (typePtr == nullptr)
    {
        Fatal(0, " ABL: Unable to AblStackHeap->malloc enumeration type ");
    }

    typePtr->form = FRM_ENUM;
    typePtr->size = 4;
    typePtr->typeIdPtr = nullptr;

    getToken();

    while (curToken == TKN_IDENTIFIER)
    {
        searchAndEnterLocalSymTable(constantIdPtr);
        constantIdPtr->defn.key = DFN_CONST;
        constantIdPtr->defn.info.constant.value.integer = ++constantValue;
        constantIdPtr->typePtr = typePtr;
        constantIdPtr->library = CurLibrary;

        if (lastIdPtr == nullptr)
        {
            typePtr->info.enumeration.constIdPtr = constantIdPtr;
        }
        else
        {
            lastIdPtr->next = constantIdPtr;
        }

        lastIdPtr = constantIdPtr;
        getToken();
        ifTokenGet(TKN_COMMA);
    }

    ifTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    typePtr->info.enumeration.max = constantValue;
    return typePtr;
}

auto subrangeType() -> TypePtr
{
    return nullptr;
}

auto getSubrangeLimit(SymTableNodePtr, int32_t*, TypePtr*) -> void
{
}

auto arraySize(TypePtr typePtr) -> int32_t
{
    TypePtr elementTypePtr = typePtr->info.array.elementTypePtr;

    if (elementTypePtr->size == 0)
    {
        elementTypePtr->size = arraySize(elementTypePtr);
    }

    if (typePtr->info.array.elementCount == -1)
    {
        typePtr->size = elementTypePtr->size;
    }
    else
    {
        typePtr->size = elementTypePtr->size * typePtr->info.array.elementCount;
    }

    return typePtr->size;
}

auto varDeclarations(SymTableNodePtr routineIdPtr) -> void
{
    // Locals follow the frame header (4 items) and the parameters.
    varOrFieldDeclarations(routineIdPtr, nullptr, routineIdPtr->defn.info.routine.totalParamSize + 4);
}

auto varOrFieldDeclarations(SymTableNodePtr routineIdPtr, TypePtr, int32_t offset) -> void
{
    bool varFlag = routineIdPtr != nullptr;
    SymTableNodePtr idPtr = nullptr;
    SymTableNodePtr prevIdPtr = nullptr;
    // The last variable of the previous (non-eternal) line, linked to the first of the next.
    SymTableNodePtr prevLineLastIdPtr = nullptr;
    int32_t totalSize = 0;

    while (curToken == TKN_IDENTIFIER || curToken == TKN_ETERNAL || curToken == TKN_STATIC)
    {
        VariableType varType = VAR_TYPE_NORMAL;

        if (curToken == TKN_ETERNAL || curToken == TKN_STATIC)
        {
            varType = curToken == TKN_ETERNAL ? VAR_TYPE_ETERNAL : VAR_TYPE_STATIC;
            getToken();

            if (curToken != TKN_IDENTIFIER)
            {
                syntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            }
        }

        // "type name, name, ...;"
        SymTableNodePtr firstIdPtr = nullptr;
        TypePtr typePtr = doType();
        int32_t size = typePtr->size;
        // doType's reference is dropped here; every variable below takes its own.
        typePtr->numInstances--;

        while (curToken == TKN_IDENTIFIER)
        {
            if (varFlag)
            {
                if (varType == VAR_TYPE_ETERNAL)
                {
                    // Eternals are global: entered at level 0.
                    int32_t saveLevel = level;
                    level = 0;
                    searchAndEnterThisTable(idPtr, SymTableDisplay[0]);
                    level = saveLevel;
                }
                else
                {
                    searchAndEnterLocalSymTable(idPtr);
                }

                idPtr->library = CurLibrary;
                idPtr->defn.key = DFN_VAR;
            }
            else
            {
                syntaxError(ABL_ERR_SYNTAX_NO_RECORD_TYPES);
            }

            idPtr->labelIndex = 0;

            if (firstIdPtr == nullptr)
            {
                firstIdPtr = idPtr;

                if (varFlag && varType != VAR_TYPE_ETERNAL && routineIdPtr->defn.info.routine.locals == nullptr)
                {
                    routineIdPtr->defn.info.routine.locals = idPtr;
                }
            }
            else
            {
                prevIdPtr->next = idPtr;
            }

            getToken();
            ifTokenGet(TKN_COMMA);
            prevIdPtr = idPtr;
        }

        for (idPtr = firstIdPtr; idPtr != nullptr; idPtr = idPtr->next)
        {
            idPtr->typePtr = setType(typePtr);

            if (!varFlag)
            {
                idPtr->defn.info.data.varType = VAR_TYPE_NORMAL;
                idPtr->defn.info.data.offset = offset;
                offset += size;
                continue;
            }

            idPtr->defn.info.data.varType = varType;

            switch (varType)
            {
                case VAR_TYPE_NORMAL:
                {
                    totalSize += size;
                    idPtr->defn.info.data.offset = offset++;
                    break;
                }
                case VAR_TYPE_STATIC:
                {
                    if (NumStaticVariables == MaxStaticVariables)
                    {
                        syntaxError(ABL_ERR_SYNTAX_TOO_MANY_STATIC_VARS);
                    }

                    idPtr->defn.info.data.offset = NumStaticVariables;
                    // Arrays record their byte size (ABLModule::init allocates them); scalars 0.
                    StaticVariablesSizes[NumStaticVariables++] = typePtr->form == FRM_ARRAY ? size : 0;
                    break;
                }
                case VAR_TYPE_ETERNAL:
                {
                    idPtr->defn.info.data.offset = eternalOffset;
                    StackItem& slot = stack[eternalOffset];
                    slot = StackItem{};

                    if (typePtr->form == FRM_ARRAY)
                    {
                        slot.address = static_cast<Address>(AblStackHeap->malloc(static_cast<uint32_t>(size)));

                        if (slot.address == nullptr)
                        {
                            Fatal(0, " ABL: Unable to AblStackHeap->malloc eternal array ");
                        }

                        memset(slot.address, 0, static_cast<size_t>(size));
                    }

                    eternalOffset++;
                    break;
                }
            }
        }

        if (varType != VAR_TYPE_ETERNAL)
        {
            if (prevLineLastIdPtr != nullptr)
            {
                prevLineLastIdPtr->next = firstIdPtr;
            }

            prevLineLastIdPtr = prevIdPtr;
        }

        if (varFlag)
        {
            synchronize(followVariablesList, declarationStartList, statementStartList);
        }

        if (curToken == TKN_SEMICOLON)
        {
            getToken();
        }
        else if (varFlag && (tokenIn(declarationStartList) || tokenIn(statementStartList)))
        {
            syntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
        }
    }

    synchronize(followVarBlockList, nullptr, nullptr);

    if (varFlag)
    {
        routineIdPtr->defn.info.routine.totalLocalSize = totalSize;
    }
}
