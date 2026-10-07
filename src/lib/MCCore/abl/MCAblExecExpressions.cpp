#include "stdafx.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"

// Evaluating crunched expressions onto the runtime stack (see MCAblCode.h for the stack and code layout).

auto MCAblRuntime::PromoteOperands(MCAblType* operand1TypePtr, MCAblType* operand2TypePtr) -> void
{
    if (operand1TypePtr == IntegerTypePtr)
    {
        _Tos[-1].Real = static_cast<float>(_Tos[-1].Integer);
    }

    if (operand2TypePtr == IntegerTypePtr)
    {
        _Tos[0].Real = static_cast<float>(_Tos[0].Integer);
    }
}

auto MCAblRuntime::ExecSubscripts(MCAblType* typePtr) -> MCAblType*
{
    // Only called at a '['; otherwise the original steps into the element type without consuming anything.
    if (_Token != MCAblToken::LBracket)
    {
        return typePtr->Array.ElementTypePtr;
    }

    while (_Token == MCAblToken::LBracket)
    {
        do
        {
            GetCodeToken();
            ExecExpression();
            const int32_t subscriptValue = _Tos->Integer;
            Pop();

            if (subscriptValue < 0 || subscriptValue >= typePtr->Array.ElementCount)
            {
                RuntimeError(MCAblRuntimeError::ValueOutOfRange);
            }

            typePtr = typePtr->Array.ElementTypePtr;
            _Tos->Address += typePtr->Size * subscriptValue;
        } while (_Token == MCAblToken::Comma);

        GetCodeToken();
    }

    return typePtr;
}

auto MCAblRuntime::ExecConstant(MCAblSymbol* idPtr) -> MCAblType*
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

    if (_Debugger)
    {
        _Debugger->TraceDataFetch(idPtr, typePtr, _Tos);
    }

    GetCodeToken();
    return typePtr;
}

auto MCAblRuntime::ExecVariable(MCAblSymbol* idPtr, MCAblUse use) -> MCAblType*
{
    MCAblType* typePtr = idPtr->TypePtr;
    MCAblStackItem* dataPtr = nullptr;

    switch (idPtr->Defn.Info.Data.VarType)
    {
        case MCAblStorage::Normal:
        {
            // Follow the static links out to the frame of the scope that declared it.
            MCAblStackItem* framePtr = _Frame;

            for (int32_t delta = _Level - idPtr->Level; delta > 0; delta--)
            {
                framePtr = reinterpret_cast<MCAblStackItem*>(
                    reinterpret_cast<MCAblStackFrameHeader*>(framePtr)->StaticLink.Address);
            }

            dataPtr = framePtr + idPtr->Defn.Info.Data.Offset;
            break;
        }

        case MCAblStorage::Static:
        {
            // A static of a library lives in that library's static data.
            MCAblModule* library = idPtr->Library;
            MCAblStackItem* staticData = _StaticData;

            if (library != nullptr && library != _Module)
            {
                staticData = library->_StaticData.data();
            }

            dataPtr = staticData + idPtr->Defn.Info.Data.Offset;
            break;
        }

        case MCAblStorage::Eternal:
        {
            dataPtr = _Stack.data() + idPtr->Defn.Info.Data.Offset;
            break;
        }
    }

    // A reference parameter's slot holds the variable's address; an array's slot holds its memory.
    if (idPtr->Defn.Key == MCAblSymbolKind::RefParam || typePtr->Form == MCAblTypeForm::Array)
    {
        dataPtr = reinterpret_cast<MCAblStackItem*>(dataPtr->Address);
    }

    PushAddress(reinterpret_cast<MCAddress>(dataPtr));
    GetCodeToken();

    while (_Token == MCAblToken::LBracket)
    {
        typePtr = ExecSubscripts(typePtr);
    }

    MCAblStackItem* valuePtr = _Tos;

    if (use == MCAblUse::Expression && typePtr->Form != MCAblTypeForm::Array)
    {
        // Replace the address with the value. The slot is cleared first: the original overwrote only the value's
        // bytes (one for a char), leaving the rest of the address in the slot.
        const MCAddress address = _Tos->Address;
        _Tos->Address = nullptr;

        if (typePtr == CharTypePtr)
        {
            _Tos->Byte = *reinterpret_cast<uint8_t*>(address);
        }
        else
        {
            _Tos->Integer = *reinterpret_cast<int32_t*>(address);
        }
    }

    if (_Debugger && use == MCAblUse::Expression)
    {
        if (typePtr->Form == MCAblTypeForm::Array)
        {
            _Debugger->TraceDataFetch(idPtr, typePtr, reinterpret_cast<MCAblStackItem*>(valuePtr->Address));
        }
        else
        {
            _Debugger->TraceDataFetch(idPtr, typePtr, valuePtr);
        }
    }

    return typePtr;
}

auto MCAblRuntime::ExecFactor() -> MCAblType*
{
    switch (_Token)
    {
        case MCAblToken::Identifier:
        {
            MCAblSymbol* idPtr = GetCodeSymbol();

            if (idPtr->Defn.Key == MCAblSymbolKind::Function)
            {
                MCAblSymbol* thisRoutineIdPtr = _Routine;
                MCAblType* resultTypePtr = ExecRoutineCall(idPtr);
                _Routine = thisRoutineIdPtr;
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
            MCAblSymbol* numberPtr = GetCodeSymbol();
            MCAblType* resultTypePtr = nullptr;

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
            MCAblSymbol* literalIdPtr = GetCodeSymbol();
            MCAblType* resultTypePtr = nullptr;

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
            _Tos->Integer = 1 - _Tos->Integer;
            return resultTypePtr;
        }

        default:
        {
            // Never happens (the compiler only crunches the tokens above); the original returns whatever ECX held.
            return nullptr;
        }
    }
}

auto MCAblRuntime::ExecTerm() -> MCAblType*
{
    MCAblType* resultTypePtr = ExecFactor();

    while (_Token == MCAblToken::Star || _Token == MCAblToken::Slash || _Token == MCAblToken::Div ||
           _Token == MCAblToken::Mod || _Token == MCAblToken::And)
    {
        const MCAblToken op = _Token;
        MCAblType* operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblType* operand2TypePtr = ExecFactor();
        MCAblStackItem* operand2Ptr = _Tos;
        MCAblStackItem* operand1Ptr = _Tos - 1;
        const bool integers = operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr;

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
                if (integers)
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
                if (integers)
                {
                    operand1Ptr->Integer = operand2Ptr->Integer == 0 ? 0 : operand1Ptr->Integer / operand2Ptr->Integer;
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
            {
                resultTypePtr = operand1TypePtr;
                break;
            }
        }

        Pop();
    }

    return resultTypePtr;
}

auto MCAblRuntime::ExecSimpleExpression() -> MCAblType*
{
    MCAblToken unaryOp = MCAblToken::Plus;

    if (_Token == MCAblToken::Plus || _Token == MCAblToken::Minus)
    {
        unaryOp = _Token;
        GetCodeToken();
    }

    MCAblType* resultTypePtr = ExecTerm();

    if (unaryOp == MCAblToken::Minus)
    {
        if (resultTypePtr == IntegerTypePtr)
        {
            _Tos->Integer = -_Tos->Integer;
        }
        else
        {
            _Tos->Real = -_Tos->Real;
        }
    }

    while (_Token == MCAblToken::Plus || _Token == MCAblToken::Minus || _Token == MCAblToken::Or)
    {
        const MCAblToken op = _Token;
        MCAblType* operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblType* operand2TypePtr = ExecTerm();
        MCAblStackItem* operand2Ptr = _Tos;
        MCAblStackItem* operand1Ptr = _Tos - 1;

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

namespace
{
    /// <summary>Applies relational operator <paramref name="op"/> to two ordered values.</summary>
    template <typename T> auto Compare(MCAblToken op, T value1, T value2) -> bool
    {
        switch (op)
        {
            case MCAblToken::Less:
                return value1 < value2;
            case MCAblToken::Greater:
                return value1 > value2;
            case MCAblToken::EqualEqual:
                return value1 == value2;
            case MCAblToken::LessEqual:
                return value1 <= value2;
            case MCAblToken::GreaterEqual:
                return value1 >= value2;
            case MCAblToken::NotEqual:
                return value1 != value2;
            default:
                return false;
        }
    }

    /// <summary>
    /// Applies relational operator <paramref name="op"/> to two reals as the original's x87 code did: an unordered
    /// pair (a NaN) is less, less-or-equal and equal, and not greater, greater-or-equal or unequal.
    /// </summary>
    auto CompareReals(MCAblToken op, float value1, float value2) -> bool
    {
        if (std::isnan(value1) || std::isnan(value2))
        {
            return op == MCAblToken::Less || op == MCAblToken::EqualEqual || op == MCAblToken::LessEqual;
        }

        return Compare(op, value1, value2);
    }
}

auto MCAblRuntime::ExecExpression() -> MCAblType*
{
    MCAblType* resultTypePtr = ExecSimpleExpression();
    const MCAblToken op = _Token;

    if (op != MCAblToken::EqualEqual && op != MCAblToken::Less && op != MCAblToken::Greater &&
        op != MCAblToken::NotEqual && op != MCAblToken::LessEqual && op != MCAblToken::GreaterEqual)
    {
        return resultTypePtr;
    }

    MCAblType* operand1TypePtr = resultTypePtr;
    GetCodeToken();
    MCAblType* operand2TypePtr = ExecSimpleExpression();
    MCAblStackItem* operand2Ptr = _Tos;
    MCAblStackItem* operand1Ptr = _Tos - 1;

    // Anything unhandled (mismatched operand types, which the compiler rejects) reads an uninitialised local in the
    // original; the port gives false.
    bool result = false;

    if ((operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr) ||
        operand1TypePtr->Form == MCAblTypeForm::Enum)
    {
        result = Compare(op, operand1Ptr->Integer, operand2Ptr->Integer);
    }
    else if (operand1TypePtr == CharTypePtr)
    {
        result = Compare(op, operand1Ptr->Byte, operand2Ptr->Byte);
    }
    else if (operand1TypePtr->Form == MCAblTypeForm::Array && operand1TypePtr->Array.ElementTypePtr == CharTypePtr)
    {
        // Original behaviour (OB-039): string comparisons are never evaluated; every one is true.
        result = true;
    }
    else if (operand1TypePtr == RealTypePtr || operand2TypePtr == RealTypePtr)
    {
        PromoteOperands(operand1TypePtr, operand2TypePtr);
        result = CompareReals(op, operand1Ptr->Real, operand2Ptr->Real);
    }

    operand1Ptr->Integer = result ? 1 : 0;
    Pop();
    return BooleanTypePtr;
}
