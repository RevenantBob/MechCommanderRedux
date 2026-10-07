#include "stdafx.h"
#include "abl/abldecl.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablscan.h"
#include "abl/ablstmt.h"
#include "abl/ablsymt.h"
#include "lib/MCFatal.h"

MCTokenCodeType FollowRoutineList[] = {TKN_SEMICOLON, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowDeclarationList[] = {TKN_SEMICOLON, TKN_IDENTIFIER, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowVariablesList[] = {TKN_SEMICOLON, TKN_IDENTIFIER, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowVarBlockList[] = {TKN_FUNCTION, TKN_CODE, TKN_EOF, TKN_NONE};
MCTokenCodeType FollowDimensionList[] = {TKN_COMMA, TKN_RBRACKET, TKN_EOF, TKN_NONE};
MCTokenCodeType IndexTypeStartList[] = {TKN_IDENTIFIER, TKN_NUMBER, TKN_NONE};
MCTokenCodeType DeclarationStartList[] = {TKN_CONST, TKN_VAR, TKN_FUNCTION, TKN_NONE};

namespace
{
    /// <summary>After a definition: a semicolon is skipped; a missing one is reported if a declaration or statement
    /// follows.</summary>
    auto SkipSemicolon() -> void
    {
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

auto IfTokenGet(MCTokenCodeType tokenCode) -> void
{
    if (CurToken == tokenCode)
    {
        GetToken();
    }
}

auto IfTokenGetElseError(MCTokenCodeType tokenCode, MCSyntaxErrorType errorCode) -> void
{
    if (CurToken == tokenCode)
    {
        GetToken();
    }
    else
    {
        SyntaxError(errorCode);
    }
}

auto Declarations(MCSymTableNodePtr routineIdPtr, int allowFunctions) -> void
{
    if (CurToken == TKN_CONST)
    {
        GetToken();
        ConstDefinitions();
    }

    if (CurToken == TKN_TYPE)
    {
        GetToken();
        TypeDefinitions();
    }

    if (CurToken == TKN_VAR)
    {
        GetToken();
        VarDeclarations(routineIdPtr);
    }

    if (allowFunctions == 0)
    {
        if (CurToken == TKN_FUNCTION)
        {
            SyntaxError(ABL_ERR_SYNTAX_NO_FUNCTION_NESTING);
        }

        return;
    }
    while (CurToken == TKN_FUNCTION)
    {
        Routine();
        Synchronize(FollowRoutineList, DeclarationStartList, StatementStartList);
        SkipSemicolon();
    }
}

auto ConstDefinitions() -> void
{
    MCSymTableNodePtr constantIdPtr = nullptr;

    while (CurToken == TKN_IDENTIFIER)
    {
        SearchAndEnterLocalSymTable(constantIdPtr);
        constantIdPtr->Defn.Key = DFN_CONST;
        constantIdPtr->Library = CurLibrary;
        GetToken();
        IfTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
        DoConst(constantIdPtr);
        Synchronize(FollowDeclarationList, DeclarationStartList, StatementStartList);
        SkipSemicolon();
    }
}

auto MakeStringType(int32_t length) -> MCTypePtr
{
    MCTypePtr stringTypePtr = CreateType();
    stringTypePtr->Form = FRM_ARRAY;
    stringTypePtr->Size = length;
    stringTypePtr->TypeIdPtr = nullptr;
    stringTypePtr->Info.Array.IndexTypePtr = IntegerTypePtr;
    stringTypePtr->Info.Array.ElementTypePtr = CharTypePtr;
    stringTypePtr->Info.Array.ElementCount = length + 1;
    return stringTypePtr;
}

auto DoConst(MCSymTableNodePtr constantIdPtr) -> void
{
    MCTokenCodeType sign = TKN_PLUS;
    bool sawSign = false;

    if (CurToken == TKN_PLUS || CurToken == TKN_MINUS)
    {
        sign = CurToken;
        sawSign = true;
        GetToken();
    }

    MCValue& value = constantIdPtr->Defn.Info.Constant.Value;

    if (CurToken == TKN_NUMBER)
    {
        if (CurLiteral.Type == LIT_INTEGER)
        {
            value.Integer = sign == TKN_PLUS ? CurLiteral.Value.Integer : -CurLiteral.Value.Integer;
            constantIdPtr->TypePtr = SetType(IntegerTypePtr);
        }
        else
        {
            value.Real = sign == TKN_PLUS ? CurLiteral.Value.Real : -CurLiteral.Value.Real;
            constantIdPtr->TypePtr = SetType(RealTypePtr);
        }
    }
    else if (CurToken == TKN_IDENTIFIER)
    {
        MCSymTableNodePtr idPtr = nullptr;
        SearchAllSymTables(idPtr);

        if (idPtr == nullptr)
        {
            SyntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        }
        else if (idPtr->Defn.Key != DFN_CONST)
        {
            SyntaxError(ABL_ERR_SYNTAX_NOT_A_CONSTANT_IDENTIFIER);
        }
        else
        {
            const MCValue& otherValue = idPtr->Defn.Info.Constant.Value;
            MCTypePtr typePtr = idPtr->TypePtr;

            if (typePtr == IntegerTypePtr)
            {
                value.Integer = sign == TKN_PLUS ? otherValue.Integer : -otherValue.Integer;
                constantIdPtr->TypePtr = SetType(IntegerTypePtr);
            }
            else if (typePtr == CharTypePtr)
            {
                if (sawSign)
                {
                    SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
                }

                value.Character = otherValue.Character;
                constantIdPtr->TypePtr = SetType(CharTypePtr);
            }
            else if (typePtr == RealTypePtr)
            {
                value.Real = sign == TKN_PLUS ? otherValue.Real : -otherValue.Real;
                constantIdPtr->TypePtr = SetType(RealTypePtr);
            }
            else if (typePtr->Form == FRM_ENUM || typePtr->Form == FRM_ARRAY)
            {
                // An enumeration value, or a string constant (the pointer to its text is shared).
                if (sawSign)
                {
                    SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
                }

                value = otherValue;
                constantIdPtr->TypePtr = SetType(typePtr);
            }
        }
    }
    else if (CurToken == TKN_STRING)
    {
        if (sawSign)
        {
            SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
        }

        auto length = static_cast<int32_t>(strlen(CurLiteral.Value.String));

        if (length == 1)
        {
            value.Character = CurLiteral.Value.String[0];
            constantIdPtr->TypePtr = SetType(CharTypePtr);
        }
        else
        {
            value.StringPtr = AblMemory.CopyString(CurLiteral.Value.String);
            constantIdPtr->TypePtr = MakeStringType(length);
        }
    }
    else
    {
        constantIdPtr->TypePtr = nullptr;
        SyntaxError(ABL_ERR_SYNTAX_INVALID_CONSTANT);
    }

    GetToken();
}

auto TypeDefinitions() -> void
{
    MCSymTableNodePtr typeIdPtr = nullptr;

    while (CurToken == TKN_IDENTIFIER)
    {
        SearchAndEnterLocalSymTable(typeIdPtr);
        typeIdPtr->Defn.Key = DFN_TYPE;
        typeIdPtr->Library = CurLibrary;
        GetToken();
        IfTokenGetElseError(TKN_EQUAL, ABL_ERR_SYNTAX_MISSING_EQUAL);
        typeIdPtr->TypePtr = DoType();

        if (typeIdPtr->TypePtr->TypeIdPtr == nullptr)
        {
            typeIdPtr->TypePtr->TypeIdPtr = typeIdPtr;
        }

        Synchronize(FollowDeclarationList, DeclarationStartList, StatementStartList);
        SkipSemicolon();
    }
}

auto DoType() -> MCTypePtr
{
    switch (CurToken)
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
            return EnumerationType();
        default:
        {
            SyntaxError(ABL_ERR_SYNTAX_INVALID_TYPE);
            return nullptr;
        }
    }

    MCSymTableNodePtr idPtr = nullptr;
    SearchAllSymTables(idPtr);

    if (idPtr == nullptr)
    {
        SyntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
        return nullptr;
    }

    if (idPtr->Defn.Key != DFN_TYPE)
    {
        SyntaxError(ABL_ERR_SYNTAX_NOT_A_TYPE_IDENTIFIER);
        return nullptr;
    }

    MCTypePtr elementTypePtr = SetType(IdentifierType(idPtr));

    if (CurToken != TKN_LBRACKET)
    {
        return elementTypePtr;
    }

    // "type[d1, d2, ...]": one array type per dimension, each the element type of the one before.
    MCTypePtr arrayTypePtr = CreateType();
    MCTypePtr dimensionTypePtr = arrayTypePtr;

    while (true)
    {
        GetToken();
        bool validIndex = false;

        if (TokenIn(IndexTypeStartList))
        {
            dimensionTypePtr->Form = FRM_ARRAY;
            dimensionTypePtr->Size = 0;
            dimensionTypePtr->TypeIdPtr = nullptr;
            dimensionTypePtr->Info.Array.IndexTypePtr = SetType(IntegerTypePtr);

            if (CurToken == TKN_IDENTIFIER)
            {
                MCSymTableNodePtr countIdPtr = nullptr;
                SearchAllSymTables(countIdPtr);

                if (countIdPtr == nullptr)
                {
                    SyntaxError(ABL_ERR_SYNTAX_UNDEFINED_IDENTIFIER);
                    validIndex = true;
                }
                else if (countIdPtr->Defn.Key == DFN_CONST && countIdPtr->TypePtr == IntegerTypePtr)
                {
                    dimensionTypePtr->Info.Array.ElementCount = countIdPtr->Defn.Info.Constant.Value.Integer;
                    validIndex = true;
                }
            }
            else if (CurToken == TKN_NUMBER && CurLiteral.Type == LIT_INTEGER)
            {
                dimensionTypePtr->Info.Array.ElementCount = CurLiteral.Value.Integer;
                validIndex = true;
            }
        }

        if (!validIndex)
        {
            dimensionTypePtr->Form = FRM_NONE;
            dimensionTypePtr->Size = 0;
            dimensionTypePtr->TypeIdPtr = nullptr;
            dimensionTypePtr->Info.Array.IndexTypePtr = nullptr;
            SyntaxError(ABL_ERR_SYNTAX_INVALID_INDEX_TYPE);
        }

        GetToken();
        Synchronize(FollowDimensionList, nullptr, nullptr);

        if (CurToken != TKN_COMMA)
        {
            break;
        }

        MCTypePtr nextDimensionTypePtr = CreateType();
        dimensionTypePtr->Info.Array.ElementTypePtr = nextDimensionTypePtr;
        dimensionTypePtr = nextDimensionTypePtr;
    }

    IfTokenGetElseError(TKN_RBRACKET, ABL_ERR_SYNTAX_MISSING_RBRACKET);
    dimensionTypePtr->Info.Array.ElementTypePtr = elementTypePtr;
    arrayTypePtr->Size = ArraySize(arrayTypePtr);
    return arrayTypePtr;
}

auto IdentifierType(MCSymTableNodePtr idPtr) -> MCTypePtr
{
    GetToken();
    return idPtr->TypePtr;
}

auto EnumerationType() -> MCTypePtr
{
    MCSymTableNodePtr constantIdPtr = nullptr;
    MCSymTableNodePtr lastIdPtr = nullptr;
    int32_t constantValue = -1;

    MCTypePtr typePtr = CreateType();
    typePtr->Form = FRM_ENUM;
    typePtr->Size = 4;
    typePtr->TypeIdPtr = nullptr;

    GetToken();

    while (CurToken == TKN_IDENTIFIER)
    {
        SearchAndEnterLocalSymTable(constantIdPtr);
        constantIdPtr->Defn.Key = DFN_CONST;
        constantIdPtr->Defn.Info.Constant.Value.Integer = ++constantValue;
        constantIdPtr->TypePtr = typePtr;
        constantIdPtr->Library = CurLibrary;

        if (lastIdPtr == nullptr)
        {
            typePtr->Info.Enumeration.ConstIdPtr = constantIdPtr;
        }
        else
        {
            lastIdPtr->Next = constantIdPtr;
        }

        lastIdPtr = constantIdPtr;
        GetToken();
        IfTokenGet(TKN_COMMA);
    }

    IfTokenGetElseError(TKN_RPAREN, ABL_ERR_SYNTAX_MISSING_RPAREN);
    typePtr->Info.Enumeration.Max = constantValue;
    return typePtr;
}

auto SubrangeType() -> MCTypePtr
{
    return nullptr;
}

auto GetSubrangeLimit(MCSymTableNodePtr, int32_t*, MCTypePtr*) -> void
{
}

auto ArraySize(MCTypePtr typePtr) -> int32_t
{
    MCTypePtr elementTypePtr = typePtr->Info.Array.ElementTypePtr;

    if (elementTypePtr->Size == 0)
    {
        elementTypePtr->Size = ArraySize(elementTypePtr);
    }

    if (typePtr->Info.Array.ElementCount == -1)
    {
        typePtr->Size = elementTypePtr->Size;
    }
    else
    {
        typePtr->Size = elementTypePtr->Size * typePtr->Info.Array.ElementCount;
    }

    return typePtr->Size;
}

auto VarDeclarations(MCSymTableNodePtr routineIdPtr) -> void
{
    // Locals follow the frame header (4 items) and the parameters.
    VarOrFieldDeclarations(routineIdPtr, nullptr, routineIdPtr->Defn.Info.Routine.TotalParamSize + 4);
}

auto VarOrFieldDeclarations(MCSymTableNodePtr routineIdPtr, MCTypePtr, int32_t offset) -> void
{
    bool varFlag = routineIdPtr != nullptr;
    MCSymTableNodePtr idPtr = nullptr;
    MCSymTableNodePtr prevIdPtr = nullptr;
    // The last variable of the previous (non-eternal) line, linked to the first of the next.
    MCSymTableNodePtr prevLineLastIdPtr = nullptr;
    int32_t totalSize = 0;

    while (CurToken == TKN_IDENTIFIER || CurToken == TKN_ETERNAL || CurToken == TKN_STATIC)
    {
        MCVariableType varType = VAR_TYPE_NORMAL;

        if (CurToken == TKN_ETERNAL || CurToken == TKN_STATIC)
        {
            varType = CurToken == TKN_ETERNAL ? VAR_TYPE_ETERNAL : VAR_TYPE_STATIC;
            GetToken();

            if (CurToken != TKN_IDENTIFIER)
            {
                SyntaxError(ABL_ERR_SYNTAX_MISSING_IDENTIFIER);
            }
        }

        // "type name, name, ...;"
        MCSymTableNodePtr firstIdPtr = nullptr;
        MCTypePtr typePtr = DoType();
        int32_t size = typePtr->Size;
        // doType's reference is dropped here; every variable below takes its own.
        typePtr->NumInstances--;

        while (CurToken == TKN_IDENTIFIER)
        {
            if (varFlag)
            {
                if (varType == VAR_TYPE_ETERNAL)
                {
                    // Eternals are global: entered at level 0.
                    int32_t saveLevel = Level;
                    Level = 0;
                    SearchAndEnterThisTable(idPtr, SymTableDisplay[0]);
                    Level = saveLevel;
                }
                else
                {
                    SearchAndEnterLocalSymTable(idPtr);
                }

                idPtr->Library = CurLibrary;
                idPtr->Defn.Key = DFN_VAR;
            }
            else
            {
                SyntaxError(ABL_ERR_SYNTAX_NO_RECORD_TYPES);
            }

            idPtr->LabelIndex = 0;

            if (firstIdPtr == nullptr)
            {
                firstIdPtr = idPtr;

                if (varFlag && varType != VAR_TYPE_ETERNAL && routineIdPtr->Defn.Info.Routine.Locals == nullptr)
                {
                    routineIdPtr->Defn.Info.Routine.Locals = idPtr;
                }
            }
            else
            {
                prevIdPtr->Next = idPtr;
            }

            GetToken();
            IfTokenGet(TKN_COMMA);
            prevIdPtr = idPtr;
        }

        for (idPtr = firstIdPtr; idPtr != nullptr; idPtr = idPtr->Next)
        {
            idPtr->TypePtr = SetType(typePtr);

            if (!varFlag)
            {
                idPtr->Defn.Info.Data.VarType = VAR_TYPE_NORMAL;
                idPtr->Defn.Info.Data.Offset = offset;
                offset += size;
                continue;
            }

            idPtr->Defn.Info.Data.VarType = varType;

            switch (varType)
            {
                case VAR_TYPE_NORMAL:
                {
                    totalSize += size;
                    idPtr->Defn.Info.Data.Offset = offset++;
                    break;
                }
                case VAR_TYPE_STATIC:
                {
                    if (NumStaticVariables == MaxStaticVariables)
                    {
                        SyntaxError(ABL_ERR_SYNTAX_TOO_MANY_STATIC_VARS);
                    }

                    idPtr->Defn.Info.Data.Offset = NumStaticVariables;
                    // Arrays record their byte size (ABLModule::init allocates them); scalars 0.
                    StaticVariablesSizes[NumStaticVariables++] = typePtr->Form == FRM_ARRAY ? size : 0;
                    break;
                }
                case VAR_TYPE_ETERNAL:
                {
                    idPtr->Defn.Info.Data.Offset = EternalOffset;
                    MCStackItem& slot = Stack[EternalOffset];
                    slot = MCStackItem{};

                    if (typePtr->Form == FRM_ARRAY)
                    {
                        slot.Address = static_cast<MCAddress>(AblMemory.Allocate(static_cast<size_t>(size)));

                        // An empty array got no block from the heap, which was fatal.
                        if (slot.Address == nullptr)
                        {
                            Fatal(0, " ABL: Unable to AblStackHeap->malloc eternal array ");
                        }
                    }

                    EternalOffset++;
                    break;
                }
            }
        }

        if (varType != VAR_TYPE_ETERNAL)
        {
            if (prevLineLastIdPtr != nullptr)
            {
                prevLineLastIdPtr->Next = firstIdPtr;
            }

            prevLineLastIdPtr = prevIdPtr;
        }

        if (varFlag)
        {
            Synchronize(FollowVariablesList, DeclarationStartList, StatementStartList);
        }

        if (CurToken == TKN_SEMICOLON)
        {
            GetToken();
        }
        else if (varFlag && (TokenIn(DeclarationStartList) || TokenIn(StatementStartList)))
        {
            SyntaxError(ABL_ERR_SYNTAX_MISSING_SEMICOLON);
        }
    }

    Synchronize(FollowVarBlockList, nullptr, nullptr);

    if (varFlag)
    {
        routineIdPtr->Defn.Info.Routine.TotalLocalSize = totalSize;
    }
}
