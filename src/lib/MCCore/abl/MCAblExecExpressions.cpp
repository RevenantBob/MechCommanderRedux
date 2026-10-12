#include "stdafx.h"
#include "abl/MCAblDebugger.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"

// Evaluating crunched expressions onto the runtime stack (see MCAblCode.h for the stack, memory and code layout).

auto MCAblRuntime::PromoteOperands(MCAblTypeId operand1TypePtr, MCAblTypeId operand2TypePtr) -> void
{
    if (operand1TypePtr == MCAblTypeId::Integer)
    {
        MCAblStackItem& operand1 = StackItem(_Tos - 1);
        operand1.Real = static_cast<float>(operand1.Integer);
    }

    if (operand2TypePtr == MCAblTypeId::Integer)
    {
        Top().Real = static_cast<float>(Top().Integer);
    }
}

auto MCAblRuntime::ExecSubscripts(MCAblTypeId typePtr) -> MCAblTypeId
{
    // Only called at a '['; otherwise the original steps into the element type without consuming anything.
    if (_Token != MCAblToken::LBracket)
    {
        return Type(typePtr).Array.ElementType;
    }

    while (_Token == MCAblToken::LBracket)
    {
        do
        {
            GetCodeToken();
            ExecExpression();
            const int32_t subscriptValue = Top().Integer;
            Pop();

            if (subscriptValue < 0 || subscriptValue >= Type(typePtr).Array.ElementCount)
            {
                RuntimeError(MCAblRuntimeError::ValueOutOfRange);
            }

            typePtr = Type(typePtr).Array.ElementType;
            Top().Address = Top().Address + Type(typePtr).Size * subscriptValue;
        } while (_Token == MCAblToken::Comma);

        GetCodeToken();
    }

    return typePtr;
}

auto MCAblRuntime::ExecConstant(const MCAblSymbol& idPtr) -> MCAblTypeId
{
    const MCAblTypeId typePtr = idPtr.Type;
    const MCAblValue& value = idPtr.Defn.Info.Constant.Value;

    if (typePtr == MCAblTypeId::Integer || Type(typePtr).Form == MCAblTypeForm::Enum)
    {
        PushInteger(value.Integer);
    }
    else if (typePtr == MCAblTypeId::Real)
    {
        PushReal(value.Real);
    }
    else if (typePtr == MCAblTypeId::Char)
    {
        PushInteger(value.Character);
    }
    else if (Type(typePtr).Form == MCAblTypeForm::Array)
    {
        PushAddress(MCAblAddress::Of(MCAblSpace::Literal, std::to_underlying(value.Text), 0));
    }

    if (_Debugger)
    {
        _Debugger->TraceDataFetch(idPtr, typePtr, MCAblAddress::StackItem(_Tos));
    }

    GetCodeToken();
    return typePtr;
}

auto MCAblRuntime::ExecVariable(const MCAblSymbol& idPtr, MCAblUse use) -> MCAblTypeId
{
    MCAblTypeId typePtr = idPtr.Type;
    MCAblAddress dataPtr{};

    switch (idPtr.Defn.Info.Data.VarType)
    {
        case MCAblStorage::Normal:
        {
            // Follow the static links out to the frame of the scope that declared it.
            int32_t framePtr = _Frame;

            for (int32_t delta = _Level - idPtr.Level; delta > 0; delta--)
            {
                framePtr = StaticLink(framePtr);
            }

            dataPtr = MCAblAddress::StackItem(framePtr + idPtr.Defn.Info.Data.Offset);
            break;
        }

        case MCAblStorage::Static:
        {
            // A static of a library lives in that library's static data.
            MCAblAddress staticData = _StaticData;

            if (idPtr.Library != MCAblLibraryId::None && idPtr.Library != _Module->Library())
            {
                staticData = Library(idPtr.Library)._StaticData;
            }

            dataPtr = staticData + idPtr.Defn.Info.Data.Offset * static_cast<int32_t>(sizeof(MCAblStackItem));
            break;
        }

        case MCAblStorage::Eternal:
        {
            dataPtr = MCAblAddress::StackItem(idPtr.Defn.Info.Data.Offset);
            break;
        }
    }

    // A reference parameter's slot holds the variable's address; an array's slot holds its memory's.
    if (idPtr.Defn.Key == MCAblSymbolKind::RefParam || Type(typePtr).Form == MCAblTypeForm::Array)
    {
        dataPtr = Load<MCAblAddress>(dataPtr);
    }

    PushAddress(dataPtr);
    GetCodeToken();

    while (_Token == MCAblToken::LBracket)
    {
        typePtr = ExecSubscripts(typePtr);
    }

    const int32_t valuePtr = _Tos;

    if (use == MCAblUse::Expression && Type(typePtr).Form != MCAblTypeForm::Array)
    {
        // Replace the address with the value. The slot is cleared first: the original overwrote only the value's
        // bytes (one for a char), leaving the rest of the address in the slot.
        const MCAblAddress address = Top().Address;
        Top() = MCAblStackItem{};

        if (typePtr == MCAblTypeId::Char)
        {
            Top().Byte = Load<uint8_t>(address);
        }
        else
        {
            Top().Integer = Load<int32_t>(address);
        }
    }

    if (_Debugger && use == MCAblUse::Expression)
    {
        if (Type(typePtr).Form == MCAblTypeForm::Array)
        {
            _Debugger->TraceDataFetch(idPtr, typePtr, StackItem(valuePtr).Address);
        }
        else
        {
            _Debugger->TraceDataFetch(idPtr, typePtr, MCAblAddress::StackItem(valuePtr));
        }
    }

    return typePtr;
}

auto MCAblRuntime::ExecFactor() -> MCAblTypeId
{
    switch (_Token)
    {
        case MCAblToken::Identifier:
        {
            const MCAblSymbol& idPtr = GetCodeSymbol();

            if (idPtr.Defn.Key == MCAblSymbolKind::Function)
            {
                const MCAblSymbolId thisRoutineIdPtr = _Routine;
                const MCAblTypeId resultTypePtr = ExecRoutineCall(idPtr);
                _Routine = thisRoutineIdPtr;
                return resultTypePtr;
            }

            if (idPtr.Defn.Key == MCAblSymbolKind::Const)
            {
                return ExecConstant(idPtr);
            }

            return ExecVariable(idPtr, MCAblUse::Expression);
        }

        case MCAblToken::Number:
        {
            const MCAblSymbol& numberPtr = GetCodeSymbol();
            MCAblTypeId resultTypePtr = MCAblTypeId::None;

            if (numberPtr.Type == MCAblTypeId::Integer)
            {
                PushInteger(numberPtr.Defn.Info.Constant.Value.Integer);
                resultTypePtr = MCAblTypeId::Integer;
            }
            else
            {
                PushReal(numberPtr.Defn.Info.Constant.Value.Real);
                resultTypePtr = MCAblTypeId::Real;
            }

            GetCodeToken();
            return resultTypePtr;
        }

        case MCAblToken::String:
        {
            // Literals are named by their text: one character is a char, anything longer a string.
            const MCAblSymbol& literalIdPtr = GetCodeSymbol();
            MCAblTypeId resultTypePtr = MCAblTypeId::None;

            if (literalIdPtr.Name.size() > 1)
            {
                PushAddress(MCAblAddress::Of(MCAblSpace::Literal, std::to_underlying(literalIdPtr.Id), 0));
                resultTypePtr = literalIdPtr.Type;
            }
            else
            {
                PushByte(literalIdPtr.Name[0]);
                resultTypePtr = MCAblTypeId::Char;
            }

            GetCodeToken();
            return resultTypePtr;
        }

        case MCAblToken::LParen:
        {
            GetCodeToken();
            const MCAblTypeId resultTypePtr = ExecExpression();
            GetCodeToken();
            return resultTypePtr;
        }

        case MCAblToken::Not:
        {
            GetCodeToken();
            const MCAblTypeId resultTypePtr = ExecFactor();
            Top().Integer = 1 - Top().Integer;
            return resultTypePtr;
        }

        default:
        {
            // Never happens (the compiler only crunches the tokens above); the original returns whatever ECX held.
            return MCAblTypeId::None;
        }
    }
}

auto MCAblRuntime::ExecTerm() -> MCAblTypeId
{
    MCAblTypeId resultTypePtr = ExecFactor();

    while (_Token == MCAblToken::Star || _Token == MCAblToken::Slash || _Token == MCAblToken::Div ||
           _Token == MCAblToken::Mod || _Token == MCAblToken::And)
    {
        const MCAblToken op = _Token;
        MCAblTypeId operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblTypeId operand2TypePtr = ExecFactor();
        MCAblStackItem& operand2Ptr = Top();
        MCAblStackItem& operand1Ptr = StackItem(_Tos - 1);
        const bool integers = operand1TypePtr == MCAblTypeId::Integer && operand2TypePtr == MCAblTypeId::Integer;

        switch (op)
        {
            case MCAblToken::And:
            {
                operand1Ptr.Integer = (operand1Ptr.Integer != 0 && operand2Ptr.Integer != 0) ? 1 : 0;
                resultTypePtr = MCAblTypeId::Boolean;
                break;
            }

            case MCAblToken::Star:
            {
                if (integers)
                {
                    operand1Ptr.Integer = operand2Ptr.Integer * operand1Ptr.Integer;
                    resultTypePtr = MCAblTypeId::Integer;
                }
                else
                {
                    PromoteOperands(operand1TypePtr, operand2TypePtr);
                    operand1Ptr.Real = operand2Ptr.Real * operand1Ptr.Real;
                    resultTypePtr = MCAblTypeId::Real;
                }
                break;
            }

            case MCAblToken::Slash:
            {
                // '/' on two integers divides as integers. Division by zero gives 0 (no runtime error).
                if (integers)
                {
                    operand1Ptr.Integer = operand2Ptr.Integer == 0 ? 0 : operand1Ptr.Integer / operand2Ptr.Integer;
                    resultTypePtr = MCAblTypeId::Integer;
                }
                else
                {
                    PromoteOperands(operand1TypePtr, operand2TypePtr);

                    if (operand2Ptr.Real == 0.0f)
                    {
                        operand1Ptr.Integer = 0;
                    }
                    else
                    {
                        operand1Ptr.Real = operand1Ptr.Real / operand2Ptr.Real;
                    }

                    resultTypePtr = MCAblTypeId::Real;
                }
                break;
            }

            case MCAblToken::Div:
            case MCAblToken::Mod:
            {
                if (operand2Ptr.Integer == 0)
                {
                    operand1Ptr.Integer = 0;
                }
                else if (op == MCAblToken::Div)
                {
                    operand1Ptr.Integer = operand1Ptr.Integer / operand2Ptr.Integer;
                }
                else
                {
                    operand1Ptr.Integer = operand1Ptr.Integer % operand2Ptr.Integer;
                }

                resultTypePtr = MCAblTypeId::Integer;
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

auto MCAblRuntime::ExecSimpleExpression() -> MCAblTypeId
{
    MCAblToken unaryOp = MCAblToken::Plus;

    if (_Token == MCAblToken::Plus || _Token == MCAblToken::Minus)
    {
        unaryOp = _Token;
        GetCodeToken();
    }

    MCAblTypeId resultTypePtr = ExecTerm();

    if (unaryOp == MCAblToken::Minus)
    {
        if (resultTypePtr == MCAblTypeId::Integer)
        {
            Top().Integer = -Top().Integer;
        }
        else
        {
            Top().Real = -Top().Real;
        }
    }

    while (_Token == MCAblToken::Plus || _Token == MCAblToken::Minus || _Token == MCAblToken::Or)
    {
        const MCAblToken op = _Token;
        MCAblTypeId operand1TypePtr = resultTypePtr;
        GetCodeToken();
        MCAblTypeId operand2TypePtr = ExecTerm();
        MCAblStackItem& operand2Ptr = Top();
        MCAblStackItem& operand1Ptr = StackItem(_Tos - 1);

        if (op == MCAblToken::Or)
        {
            operand1Ptr.Integer = (operand1Ptr.Integer == 0 && operand2Ptr.Integer == 0) ? 0 : 1;
            resultTypePtr = MCAblTypeId::Boolean;
        }
        else if (operand1TypePtr == MCAblTypeId::Integer && operand2TypePtr == MCAblTypeId::Integer)
        {
            if (op == MCAblToken::Plus)
            {
                operand1Ptr.Integer = operand1Ptr.Integer + operand2Ptr.Integer;
            }
            else
            {
                operand1Ptr.Integer = operand1Ptr.Integer - operand2Ptr.Integer;
            }

            resultTypePtr = MCAblTypeId::Integer;
        }
        else
        {
            PromoteOperands(operand1TypePtr, operand2TypePtr);

            if (op == MCAblToken::Plus)
            {
                operand1Ptr.Real = operand2Ptr.Real + operand1Ptr.Real;
            }
            else
            {
                operand1Ptr.Real = operand1Ptr.Real - operand2Ptr.Real;
            }

            resultTypePtr = MCAblTypeId::Real;
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

auto MCAblRuntime::ExecExpression() -> MCAblTypeId
{
    MCAblTypeId resultTypePtr = ExecSimpleExpression();
    const MCAblToken op = _Token;

    if (op != MCAblToken::EqualEqual && op != MCAblToken::Less && op != MCAblToken::Greater &&
        op != MCAblToken::NotEqual && op != MCAblToken::LessEqual && op != MCAblToken::GreaterEqual)
    {
        return resultTypePtr;
    }

    MCAblTypeId operand1TypePtr = resultTypePtr;
    GetCodeToken();
    MCAblTypeId operand2TypePtr = ExecSimpleExpression();
    MCAblStackItem& operand2Ptr = Top();
    MCAblStackItem& operand1Ptr = StackItem(_Tos - 1);

    // Anything unhandled (mismatched operand types, which the compiler rejects) reads an uninitialised local in the
    // original; the port gives false.
    bool result = false;

    if ((operand1TypePtr == MCAblTypeId::Integer && operand2TypePtr == MCAblTypeId::Integer) ||
        Type(operand1TypePtr).Form == MCAblTypeForm::Enum)
    {
        result = Compare(op, operand1Ptr.Integer, operand2Ptr.Integer);
    }
    else if (operand1TypePtr == MCAblTypeId::Char)
    {
        result = Compare(op, operand1Ptr.Byte, operand2Ptr.Byte);
    }
    else if (Type(operand1TypePtr).Form == MCAblTypeForm::Array &&
             Type(operand1TypePtr).Array.ElementType == MCAblTypeId::Char)
    {
        // Original behaviour (OB-039): string comparisons are never evaluated; every one is true.
        result = true;
    }
    else if (operand1TypePtr == MCAblTypeId::Real || operand2TypePtr == MCAblTypeId::Real)
    {
        PromoteOperands(operand1TypePtr, operand2TypePtr);
        result = CompareReals(op, operand1Ptr.Real, operand2Ptr.Real);
    }

    operand1Ptr.Integer = result ? 1 : 0;
    Pop();
    return MCAblTypeId::Boolean;
}
