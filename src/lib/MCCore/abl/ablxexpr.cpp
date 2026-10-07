#include "stdafx.h"
#include "abl/ablxexpr.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/MCAblErrors.h"
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
    void PromoteOperands(MCAblType* operand1TypePtr, MCAblType* operand2TypePtr)
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

auto ExecField() -> MCAblType*
{
    GetCodeToken();
    MCAblSymbol* fieldIdPtr = GetCodeSymTableNodePtr();
    Tos->Address += fieldIdPtr->Defn.Info.Data.Offset;
    GetCodeToken();
    return fieldIdPtr->TypePtr;
}

auto ExecSubscripts(MCAblType* typePtr) -> MCAblType*
{
    // Only called at a '['; otherwise the original steps into the element type without consuming anything.
    if (CodeToken != MCAblToken::LBracket)
    {
        return typePtr->Array.ElementTypePtr;
    }

    while (CodeToken == MCAblToken::LBracket)
    {
        do
        {
            GetCodeToken();
            ExecExpression();
            int32_t subscriptValue = Tos->Integer;
            Pop();

            if (subscriptValue < 0 || subscriptValue >= typePtr->Array.ElementCount)
            {
                RuntimeError(MCAblRuntimeError::ValueOutOfRange);
            }

            typePtr = typePtr->Array.ElementTypePtr;
            Tos->Address += typePtr->Size * subscriptValue;
        } while (CodeToken == MCAblToken::Comma);

        GetCodeToken();
    }

    return typePtr;
}

auto ExecConstant(MCAblSymbol* idPtr) -> MCAblType*
{
    MCAblType* typePtr = idPtr->TypePtr;

    if (typePtr == IntegerTypePtr || typePtr->Form == MCAblTypeForm::Enum)
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
    else if (typePtr->Form == MCAblTypeForm::Array)
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

auto ExecVariable(MCAblSymbol* idPtr, MCAblUse use) -> MCAblType*
{
    MCAblType* typePtr = idPtr->TypePtr;
    MCStackItemPtr dataPtr;

    switch (idPtr->Defn.Info.Data.VarType)
    {
        case MCAblStorage::Normal:
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

        case MCAblStorage::Static:
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

        case MCAblStorage::Eternal:
            dataPtr = Stack + idPtr->Defn.Info.Data.Offset;
            break;
        default:
            // Never happens: the original falls back to the symbol node itself.
            dataPtr = reinterpret_cast<MCStackItemPtr>(idPtr);
            break;
    }

    // A reference parameter's slot holds the variable's address; an array's slot holds its memory.
    if (idPtr->Defn.Key == MCAblSymbolKind::RefParam || typePtr->Form == MCAblTypeForm::Array)
    {
        dataPtr = reinterpret_cast<MCStackItemPtr>(dataPtr->Address);
    }

    PushAddress(reinterpret_cast<MCAddress>(dataPtr));

    GetCodeToken();

    while (CodeToken == MCAblToken::LBracket)
    {
        typePtr = ExecSubscripts(typePtr);
    }

    MCAblType* baseTypePtr = typePtr;
    MCStackItemPtr valuePtr = Tos;

    if (use != MCAblUse::Target && use != MCAblUse::RefParam && typePtr->Form != MCAblTypeForm::Array)
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

    if (Debugger != nullptr && use != MCAblUse::Target && use != MCAblUse::RefParam)
    {
        if (typePtr->Form == MCAblTypeForm::Array)
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

auto ExecFactor() -> MCAblType*
{
    switch (CodeToken)
    {
        case MCAblToken::Identifier:
        {
            MCAblSymbol* idPtr = GetCodeSymTableNodePtr();

            if (idPtr->Defn.Key == MCAblSymbolKind::Function)
            {
                MCAblSymbol* thisRoutineIdPtr = CurRoutineIdPtr;
                MCAblType* resultTypePtr = ExecRoutineCall(idPtr);
                CurRoutineIdPtr = thisRoutineIdPtr;
                return resultTypePtr;
            }

            if (idPtr->Defn.Key == MCAblSymbolKind::Const)
            {
                return ExecConstant(idPtr);
            }

            return ExecVariable(idPtr, MCAblUse::Expression);
        }

        case MCAblToken::Number:
        {
            MCAblSymbol* numberPtr = GetCodeSymTableNodePtr();
            MCAblType* resultTypePtr;

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

        case MCAblToken::String:
        {
            // Literals are named by their text: one character is a char, anything longer a string.
            MCAblSymbol* literalIdPtr = GetCodeSymTableNodePtr();
            MCAblType* resultTypePtr;

            if (literalIdPtr->Name.size() > 1)
            {
                PushAddress(literalIdPtr->LiteralText.data());
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

        case MCAblToken::LParen:
        {
            GetCodeToken();
            MCAblType* resultTypePtr = ExecExpression();
            GetCodeToken();
            return resultTypePtr;
        }

        case MCAblToken::Not:
        {
            GetCodeToken();
            MCAblType* resultTypePtr = ExecFactor();
            Tos->Integer = 1 - Tos->Integer;
            return resultTypePtr;
        }

        default:
            // Never happens (the compiler only crunches the tokens above); the original returns whatever ECX held.
            return nullptr;
    }
}

auto ExecTerm() -> MCAblType*
{
    MCAblType* resultTypePtr = ExecFactor();

    while (CodeToken == MCAblToken::Star || CodeToken == MCAblToken::Slash || CodeToken == MCAblToken::Div ||
           CodeToken == MCAblToken::Mod || CodeToken == MCAblToken::And)
    {
        MCAblToken op = CodeToken;
        MCAblType* operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblType* operand2TypePtr = ExecFactor();
        MCStackItemPtr operand2Ptr = Tos;
        MCStackItemPtr operand1Ptr = Tos - 1;

        switch (op)
        {
            case MCAblToken::And:
            {
                operand1Ptr->Integer = (operand1Ptr->Integer != 0 && operand2Ptr->Integer != 0) ? 1 : 0;
                resultTypePtr = BooleanTypePtr;
                break;
            }
            case MCAblToken::Star:
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
            case MCAblToken::Slash:
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
            case MCAblToken::Div:
            case MCAblToken::Mod:
            {
                if (operand2Ptr->Integer == 0)
                {
                    operand1Ptr->Integer = 0;
                }
                else if (op == MCAblToken::Div)
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

auto ExecSimpleExpression() -> MCAblType*
{
    MCAblToken unaryOp = MCAblToken::Plus;

    if (CodeToken == MCAblToken::Plus || CodeToken == MCAblToken::Minus)
    {
        unaryOp = CodeToken;
        GetCodeToken();
    }

    MCAblType* resultTypePtr = ExecTerm();

    if (unaryOp == MCAblToken::Minus)
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

    while (CodeToken == MCAblToken::Plus || CodeToken == MCAblToken::Minus || CodeToken == MCAblToken::Or)
    {
        MCAblToken op = CodeToken;
        MCAblType* operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblType* operand2TypePtr = ExecTerm();
        MCStackItemPtr operand2Ptr = Tos;
        MCStackItemPtr operand1Ptr = Tos - 1;

        if (op == MCAblToken::Or)
        {
            operand1Ptr->Integer = (operand1Ptr->Integer == 0 && operand2Ptr->Integer == 0) ? 0 : 1;
            resultTypePtr = BooleanTypePtr;
        }
        else if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
        {
            if (op == MCAblToken::Plus)
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

            if (op == MCAblToken::Plus)
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

auto ExecExpression() -> MCAblType*
{
    MCAblType* resultTypePtr = ExecSimpleExpression();
    MCAblToken op = CodeToken;

    if (op != MCAblToken::EqualEqual && op != MCAblToken::Less && op != MCAblToken::Greater &&
        op != MCAblToken::NotEqual && op != MCAblToken::LessEqual && op != MCAblToken::GreaterEqual)
    {
        return resultTypePtr;
    }

    MCAblType* operand1TypePtr = resultTypePtr;
    GetCodeToken();
    MCAblType* operand2TypePtr = ExecSimpleExpression();
    MCStackItemPtr operand2Ptr = Tos;
    MCStackItemPtr operand1Ptr = Tos - 1;

    // Anything unhandled (mismatched operand types, which the compiler rejects) reads an uninitialised local in the
    // original; the port gives false.
    bool result = false;

    if ((operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr) ||
        operand1TypePtr->Form == MCAblTypeForm::Enum)
    {
        int32_t value1 = operand1Ptr->Integer;
        int32_t value2 = operand2Ptr->Integer;

        switch (op)
        {
            case MCAblToken::Less:
                result = value1 < value2;
                break;
            case MCAblToken::Greater:
                result = value1 > value2;
                break;
            case MCAblToken::EqualEqual:
                result = value1 == value2;
                break;
            case MCAblToken::LessEqual:
                result = value1 <= value2;
                break;
            case MCAblToken::GreaterEqual:
                result = value1 >= value2;
                break;
            case MCAblToken::NotEqual:
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
            case MCAblToken::Less:
                result = value1 < value2;
                break;
            case MCAblToken::Greater:
                result = value1 > value2;
                break;
            case MCAblToken::EqualEqual:
                result = value1 == value2;
                break;
            case MCAblToken::LessEqual:
                result = value1 <= value2;
                break;
            case MCAblToken::GreaterEqual:
                result = value1 >= value2;
                break;
            case MCAblToken::NotEqual:
                result = value1 != value2;
                break;
            default:
                break;
        }
    }
    else if (operand1TypePtr->Form == MCAblTypeForm::Array && operand1TypePtr->Array.ElementTypePtr == CharTypePtr)
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
            case MCAblToken::Less:
                result = unordered || value1 < value2;
                break;
            case MCAblToken::Greater:
                result = !unordered && value1 > value2;
                break;
            case MCAblToken::EqualEqual:
                result = unordered || value1 == value2;
                break;
            case MCAblToken::LessEqual:
                result = unordered || value1 <= value2;
                break;
            case MCAblToken::GreaterEqual:
                result = !unordered && value1 >= value2;
                break;
            case MCAblToken::NotEqual:
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
