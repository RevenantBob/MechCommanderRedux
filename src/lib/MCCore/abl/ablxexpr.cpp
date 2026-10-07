#include "stdafx.h"
#include "abl/ablxexpr.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablrtn.h"
#include "abl/ablxstmt.h"

namespace
{
    /// <summary>Converts the integer in <paramref name="item"/> to a real in place.</summary>
    void PromoteToReal(MCStackItem& item)
    {
        item.Real = static_cast<float>(item.Integer);
    }

    /// <summary>
    /// Brings both operands of an arithmetic operator to reals: the integer one(s) of the given base types are
    /// converted in place.
    /// </summary>
    void PromoteOperands(MCTypePtr operand1TypePtr, MCTypePtr operand2TypePtr)
    {
        if (operand1TypePtr == IntegerTypePtr)
        {
            PromoteToReal(Tos[-1]);
        }

        if (operand2TypePtr == IntegerTypePtr)
        {
            PromoteToReal(Tos[0]);
        }
    }
}

auto ExecField() -> MCTypePtr
{
    GetCodeToken();
    MCSymTableNodePtr fieldIdPtr = GetCodeSymTableNodePtr();
    Tos->Address += fieldIdPtr->Defn.Info.Data.Offset;
    GetCodeToken();
    return fieldIdPtr->TypePtr;
}

auto ExecSubscripts(MCTypePtr typePtr) -> MCTypePtr
{
    // Only called at a '['; otherwise the original steps into the element type without consuming anything.
    if (CodeToken != TKN_LBRACKET)
    {
        return typePtr->Info.Array.ElementTypePtr;
    }

    while (CodeToken == TKN_LBRACKET)
    {
        do
        {
            GetCodeToken();
            ExecExpression();
            int32_t subscriptValue = Tos->Integer;
            Pop();

            if (subscriptValue < 0 || subscriptValue >= typePtr->Info.Array.ElementCount)
            {
                RuntimeError(ABL_ERR_RUNTIME_VALUE_OUT_OF_RANGE);
            }

            typePtr = typePtr->Info.Array.ElementTypePtr;
            Tos->Address += typePtr->Size * subscriptValue;
        } while (CodeToken == TKN_COMMA);

        GetCodeToken();
    }

    return typePtr;
}

auto ExecConstant(MCSymTableNodePtr idPtr) -> MCTypePtr
{
    MCTypePtr typePtr = idPtr->TypePtr;

    if (BaseType(typePtr) == IntegerTypePtr || typePtr->Form == FRM_ENUM)
    {
        PushInteger(idPtr->Defn.Info.Constant.Value.Integer);
    }
    else if (typePtr == RealTypePtr)
    {
        PushReal(idPtr->Defn.Info.Constant.Value.Real);
    }
    else if (typePtr == CharTypePtr)
    {
        PushInteger(idPtr->Defn.Info.Constant.Value.Character);
    }
    else if (typePtr->Form == FRM_ARRAY)
    {
        PushAddress(idPtr->Defn.Info.Constant.Value.StringPtr);
    }

    if (Debugger != nullptr)
    {
        Debugger->TraceDataFetch(idPtr, typePtr, Tos);
    }

    GetCodeToken();
    return typePtr;
}

auto ExecVariable(MCSymTableNodePtr idPtr, MCUseType use) -> MCTypePtr
{
    MCTypePtr typePtr = idPtr->TypePtr;
    MCStackItemPtr dataPtr;

    switch (idPtr->Defn.Info.Data.VarType)
    {
        case VAR_TYPE_NORMAL:
        {
            // Follow the static links out to the frame of the scope that declared it.
            MCStackItemPtr framePtr = StackFrameBasePtr;

            for (int32_t delta = Level - idPtr->Level; delta > 0; delta--)
            {
                framePtr = reinterpret_cast<MCStackItemPtr>(
                    reinterpret_cast<MCStackFrameHeaderPtr>(framePtr)->StaticLink.Address);
            }

            dataPtr = framePtr + idPtr->Defn.Info.Data.Offset;
            break;
        }

        case VAR_TYPE_STATIC:
        {
            // A static of a library lives in that library's static data.
            MCAblModule* library = idPtr->Library;

            if (library != nullptr && library != CurModule)
            {
                StaticDataPtr = library->StaticData;
            }

            dataPtr = StaticDataPtr + idPtr->Defn.Info.Data.Offset;

            if (library != nullptr && library != CurModule)
            {
                StaticDataPtr = CurModule->StaticData;
            }
            break;
        }

        case VAR_TYPE_ETERNAL:
            dataPtr = Stack + idPtr->Defn.Info.Data.Offset;
            break;
        default:
            // Never happens: the original falls back to the symbol node itself.
            dataPtr = reinterpret_cast<MCStackItemPtr>(idPtr);
            break;
    }

    // A reference parameter's slot holds the variable's address; an array's slot holds its memory.
    if (idPtr->Defn.Key == DFN_REFPARAM || typePtr->Form == FRM_ARRAY)
    {
        dataPtr = reinterpret_cast<MCStackItemPtr>(dataPtr->Address);
    }

    PushAddress(reinterpret_cast<MCAddress>(dataPtr));

    GetCodeToken();

    while (CodeToken == TKN_LBRACKET)
    {
        typePtr = ExecSubscripts(typePtr);
    }

    MCTypePtr baseTypePtr = BaseType(typePtr);
    MCStackItemPtr valuePtr = Tos;

    if (use != USE_TARGET && use != USE_REFPARAM && typePtr->Form != FRM_ARRAY)
    {
        // Replace the address with the value. Port fix: the slot is cleared first. The original overwrote only the
        // value's bytes (one for a char), leaving the rest of the address in the slot.
        MCAddress address = Tos->Address;
        Tos->Address = nullptr;

        if (baseTypePtr == CharTypePtr)
        {
            Tos->Byte = *reinterpret_cast<uint8_t*>(address);
        }
        else
        {
            Tos->Integer = *reinterpret_cast<int32_t*>(address);
        }
    }

    if (Debugger != nullptr && use != USE_TARGET && use != USE_REFPARAM)
    {
        if (typePtr->Form == FRM_ARRAY)
        {
            Debugger->TraceDataFetch(idPtr, typePtr, reinterpret_cast<MCStackItemPtr>(valuePtr->Address));
        }
        else
        {
            Debugger->TraceDataFetch(idPtr, typePtr, valuePtr);
        }
    }

    return typePtr;
}

auto ExecFactor() -> MCTypePtr
{
    switch (CodeToken)
    {
        case TKN_IDENTIFIER:
        {
            MCSymTableNodePtr idPtr = GetCodeSymTableNodePtr();

            if (idPtr->Defn.Key == DFN_FUNCTION)
            {
                MCSymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
                MCTypePtr resultTypePtr = ExecRoutineCall(idPtr);
                CurRoutineIdPtr = thisRoutineIdPtr;
                return resultTypePtr;
            }

            if (idPtr->Defn.Key == DFN_CONST)
            {
                return ExecConstant(idPtr);
            }

            return ExecVariable(idPtr, USE_EXPR);
        }

        case TKN_NUMBER:
        {
            MCSymTableNodePtr numberPtr = GetCodeSymTableNodePtr();
            MCTypePtr resultTypePtr;

            if (numberPtr->TypePtr == IntegerTypePtr)
            {
                PushInteger(numberPtr->Defn.Info.Constant.Value.Integer);
                resultTypePtr = IntegerTypePtr;
            }
            else
            {
                PushReal(numberPtr->Defn.Info.Constant.Value.Real);
                resultTypePtr = RealTypePtr;
            }

            GetCodeToken();
            return resultTypePtr;
        }

        case TKN_STRING:
        {
            // Literals are named by their text: one character is a char, anything longer a string.
            MCSymTableNodePtr literalIdPtr = GetCodeSymTableNodePtr();
            MCTypePtr resultTypePtr;

            if (static_cast<int32_t>(strlen(literalIdPtr->Name)) > 1)
            {
                PushAddress(literalIdPtr->LiteralString);
                resultTypePtr = literalIdPtr->TypePtr;
            }
            else
            {
                PushByte(literalIdPtr->Name[0]);
                resultTypePtr = CharTypePtr;
            }

            GetCodeToken();
            return resultTypePtr;
        }

        case TKN_LPAREN:
        {
            GetCodeToken();
            MCTypePtr resultTypePtr = ExecExpression();
            GetCodeToken();
            return resultTypePtr;
        }

        case TKN_NOT:
        {
            GetCodeToken();
            MCTypePtr resultTypePtr = ExecFactor();
            Tos->Integer = 1 - Tos->Integer;
            return resultTypePtr;
        }

        default:
            // Never happens (the compiler only crunches the tokens above); the original returns whatever ECX held.
            return nullptr;
    }
}

auto ExecTerm() -> MCTypePtr
{
    MCTypePtr resultTypePtr = ExecFactor();

    while (CodeToken == TKN_STAR || CodeToken == TKN_FSLASH || CodeToken == TKN_DIV || CodeToken == TKN_MOD ||
           CodeToken == TKN_AND)
    {
        MCTokenCodeType op = CodeToken;
        MCTypePtr operand1TypePtr = BaseType(resultTypePtr);
        GetCodeToken();
        MCTypePtr operand2TypePtr = BaseType(ExecFactor());
        MCStackItemPtr operand2Ptr = Tos;
        MCStackItemPtr operand1Ptr = Tos - 1;

        switch (op)
        {
            case TKN_AND:
            {
                operand1Ptr->Integer = (operand1Ptr->Integer != 0 && operand2Ptr->Integer != 0) ? 1 : 0;
                resultTypePtr = BooleanTypePtr;
                break;
            }
            case TKN_STAR:
            {
                if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
                {
                    operand1Ptr->Integer = operand2Ptr->Integer * operand1Ptr->Integer;
                    resultTypePtr = IntegerTypePtr;
                }
                else
                {
                    PromoteOperands(operand1TypePtr, operand2TypePtr);
                    operand1Ptr->Real = operand2Ptr->Real * operand1Ptr->Real;
                    resultTypePtr = RealTypePtr;
                }
                break;
            }
            case TKN_FSLASH:
            {
                // '/' on two integers divides as integers. Division by zero gives 0 (no runtime error).
                if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
                {
                    if (operand2Ptr->Integer == 0)
                    {
                        operand1Ptr->Integer = 0;
                    }
                    else
                    {
                        operand1Ptr->Integer = operand1Ptr->Integer / operand2Ptr->Integer;
                    }

                    resultTypePtr = IntegerTypePtr;
                }
                else
                {
                    PromoteOperands(operand1TypePtr, operand2TypePtr);

                    if (operand2Ptr->Real == 0.0f)
                    {
                        operand1Ptr->Integer = 0;
                    }
                    else
                    {
                        operand1Ptr->Real = operand1Ptr->Real / operand2Ptr->Real;
                    }

                    resultTypePtr = RealTypePtr;
                }
                break;
            }
            case TKN_DIV:
            case TKN_MOD:
            {
                if (operand2Ptr->Integer == 0)
                {
                    operand1Ptr->Integer = 0;
                }
                else if (op == TKN_DIV)
                {
                    operand1Ptr->Integer = operand1Ptr->Integer / operand2Ptr->Integer;
                }
                else
                {
                    operand1Ptr->Integer = operand1Ptr->Integer % operand2Ptr->Integer;
                }

                resultTypePtr = IntegerTypePtr;
                break;
            }
            default:
                resultTypePtr = operand1TypePtr;
                break;
        }

        Pop();
    }

    return resultTypePtr;
}

auto ExecSimpleExpression() -> MCTypePtr
{
    MCTokenCodeType unaryOp = TKN_PLUS;

    if (CodeToken == TKN_PLUS || CodeToken == TKN_MINUS)
    {
        unaryOp = CodeToken;
        GetCodeToken();
    }

    MCTypePtr resultTypePtr = ExecTerm();

    if (unaryOp == TKN_MINUS)
    {
        if (resultTypePtr == IntegerTypePtr)
        {
            Tos->Integer = -Tos->Integer;
        }
        else
        {
            Tos->Real = -Tos->Real;
        }
    }

    while (CodeToken == TKN_PLUS || CodeToken == TKN_MINUS || CodeToken == TKN_OR)
    {
        MCTokenCodeType op = CodeToken;
        MCTypePtr operand1TypePtr = BaseType(resultTypePtr);
        GetCodeToken();
        MCTypePtr operand2TypePtr = BaseType(ExecTerm());
        MCStackItemPtr operand2Ptr = Tos;
        MCStackItemPtr operand1Ptr = Tos - 1;

        if (op == TKN_OR)
        {
            operand1Ptr->Integer = (operand1Ptr->Integer == 0 && operand2Ptr->Integer == 0) ? 0 : 1;
            resultTypePtr = BooleanTypePtr;
        }
        else if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
        {
            if (op == TKN_PLUS)
            {
                operand1Ptr->Integer = operand1Ptr->Integer + operand2Ptr->Integer;
            }
            else
            {
                operand1Ptr->Integer = operand1Ptr->Integer - operand2Ptr->Integer;
            }

            resultTypePtr = IntegerTypePtr;
        }
        else
        {
            PromoteOperands(operand1TypePtr, operand2TypePtr);

            if (op == TKN_PLUS)
            {
                operand1Ptr->Real = operand2Ptr->Real + operand1Ptr->Real;
            }
            else
            {
                operand1Ptr->Real = operand1Ptr->Real - operand2Ptr->Real;
            }

            resultTypePtr = RealTypePtr;
        }

        Pop();
    }

    return resultTypePtr;
}

auto ExecExpression() -> MCTypePtr
{
    MCTypePtr resultTypePtr = ExecSimpleExpression();
    MCTokenCodeType op = CodeToken;

    if (op != TKN_EQUALEQUAL && op != TKN_LT && op != TKN_GT && op != TKN_NE && op != TKN_LE && op != TKN_GE)
    {
        return resultTypePtr;
    }

    MCTypePtr operand1TypePtr = BaseType(resultTypePtr);
    GetCodeToken();
    MCTypePtr operand2TypePtr = BaseType(ExecSimpleExpression());
    MCStackItemPtr operand2Ptr = Tos;
    MCStackItemPtr operand1Ptr = Tos - 1;

    // Anything unhandled (mismatched operand types, which the compiler rejects) reads an uninitialised local in the
    // original; the port gives false.
    bool result = false;

    if ((operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr) || operand1TypePtr->Form == FRM_ENUM)
    {
        int32_t value1 = operand1Ptr->Integer;
        int32_t value2 = operand2Ptr->Integer;

        switch (op)
        {
            case TKN_LT:
                result = value1 < value2;
                break;
            case TKN_GT:
                result = value1 > value2;
                break;
            case TKN_EQUALEQUAL:
                result = value1 == value2;
                break;
            case TKN_LE:
                result = value1 <= value2;
                break;
            case TKN_GE:
                result = value1 >= value2;
                break;
            case TKN_NE:
                result = value1 != value2;
                break;
            default:
                break;
        }
    }
    else if (operand1TypePtr == CharTypePtr)
    {
        uint8_t value1 = operand1Ptr->Byte;
        uint8_t value2 = operand2Ptr->Byte;

        switch (op)
        {
            case TKN_LT:
                result = value1 < value2;
                break;
            case TKN_GT:
                result = value1 > value2;
                break;
            case TKN_EQUALEQUAL:
                result = value1 == value2;
                break;
            case TKN_LE:
                result = value1 <= value2;
                break;
            case TKN_GE:
                result = value1 >= value2;
                break;
            case TKN_NE:
                result = value1 != value2;
                break;
            default:
                break;
        }
    }
    else if (operand1TypePtr->Form == FRM_ARRAY && operand1TypePtr->Info.Array.ElementTypePtr == CharTypePtr)
    {
        // Original behaviour (OB-039): string comparisons are never evaluated; every one is true.
        result = true;
    }
    else if (operand1TypePtr == RealTypePtr || operand2TypePtr == RealTypePtr)
    {
        PromoteOperands(operand1TypePtr, operand2TypePtr);
        float value1 = operand1Ptr->Real;
        float value2 = operand2Ptr->Real;
        const bool unordered = value1 != value1 || value2 != value2;

        switch (op)
        {
            case TKN_LT:
                result = unordered || value1 < value2;
                break;
            case TKN_GT:
                result = !unordered && value1 > value2;
                break;
            case TKN_EQUALEQUAL:
                result = unordered || value1 == value2;
                break;
            case TKN_LE:
                result = unordered || value1 <= value2;
                break;
            case TKN_GE:
                result = !unordered && value1 >= value2;
                break;
            case TKN_NE:
                result = !unordered && value1 != value2;
                break;
            default:
                break;
        }
    }

    operand1Ptr->Integer = result ? 1 : 0;
    Pop();
    return BooleanTypePtr;
}
