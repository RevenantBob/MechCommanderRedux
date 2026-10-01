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
    void promoteToReal(StackItem& item)
    {
        item.real = static_cast<float>(item.integer);
    }

    /// <summary>
    /// Brings both operands of an arithmetic operator to reals: the integer one(s) of the given base types are
    /// converted in place.
    /// </summary>
    void promoteOperands(TypePtr operand1TypePtr, TypePtr operand2TypePtr)
    {
        if (operand1TypePtr == IntegerTypePtr)
        {
            promoteToReal(tos[-1]);
        }

        if (operand2TypePtr == IntegerTypePtr)
        {
            promoteToReal(tos[0]);
        }
    }
}

auto execField() -> TypePtr
{
    getCodeToken();
    SymTableNodePtr fieldIdPtr = getCodeSymTableNodePtr();
    tos->address += fieldIdPtr->defn.info.data.offset;
    getCodeToken();
    return fieldIdPtr->typePtr;
}

auto execSubscripts(TypePtr typePtr) -> TypePtr
{
    // Only called at a '['; otherwise the original steps into the element type without consuming anything.
    if (codeToken != TKN_LBRACKET)
    {
        return typePtr->info.array.elementTypePtr;
    }

    while (codeToken == TKN_LBRACKET)
    {
        do
        {
            getCodeToken();
            execExpression();
            int32_t subscriptValue = tos->integer;
            pop();

            if (subscriptValue < 0 || subscriptValue >= typePtr->info.array.elementCount)
            {
                runtimeError(ABL_ERR_RUNTIME_VALUE_OUT_OF_RANGE);
            }

            typePtr = typePtr->info.array.elementTypePtr;
            tos->address += typePtr->size * subscriptValue;
        } while (codeToken == TKN_COMMA);

        getCodeToken();
    }

    return typePtr;
}

auto execConstant(SymTableNodePtr idPtr) -> TypePtr
{
    TypePtr typePtr = idPtr->typePtr;

    if (baseType(typePtr) == IntegerTypePtr || typePtr->form == FRM_ENUM)
    {
        pushInteger(idPtr->defn.info.constant.value.integer);
    }
    else if (typePtr == RealTypePtr)
    {
        pushReal(idPtr->defn.info.constant.value.real);
    }
    else if (typePtr == CharTypePtr)
    {
        pushInteger(idPtr->defn.info.constant.value.character);
    }
    else if (typePtr->form == FRM_ARRAY)
    {
        pushAddress(idPtr->defn.info.constant.value.stringPtr);
    }

    if (debugger != nullptr)
    {
        debugger->traceDataFetch(idPtr, typePtr, tos);
    }

    getCodeToken();
    return typePtr;
}

auto execVariable(SymTableNodePtr idPtr, UseType use) -> TypePtr
{
    TypePtr typePtr = idPtr->typePtr;
    StackItemPtr dataPtr;

    switch (idPtr->defn.info.data.varType)
    {
        case VAR_TYPE_NORMAL:
        {
            // Follow the static links out to the frame of the scope that declared it.
            StackItemPtr framePtr = stackFrameBasePtr;

            for (int32_t delta = level - idPtr->level; delta > 0; delta--)
            {
                framePtr =
                    reinterpret_cast<StackItemPtr>(reinterpret_cast<StackFrameHeaderPtr>(framePtr)->staticLink.address);
            }

            dataPtr = framePtr + idPtr->defn.info.data.offset;
            break;
        }

        case VAR_TYPE_STATIC:
        {
            // A static of a library lives in that library's static data.
            ABLModule* library = idPtr->library;

            if (library != nullptr && library != CurModule)
            {
                StaticDataPtr = library->staticData;
            }

            dataPtr = StaticDataPtr + idPtr->defn.info.data.offset;

            if (library != nullptr && library != CurModule)
            {
                StaticDataPtr = CurModule->staticData;
            }
            break;
        }

        case VAR_TYPE_ETERNAL:
            dataPtr = stack + idPtr->defn.info.data.offset;
            break;
        default:
            // Never happens: the original falls back to the symbol node itself.
            dataPtr = reinterpret_cast<StackItemPtr>(idPtr);
            break;
    }

    // A reference parameter's slot holds the variable's address; an array's slot holds its memory.
    if (idPtr->defn.key == DFN_REFPARAM || typePtr->form == FRM_ARRAY)
    {
        dataPtr = reinterpret_cast<StackItemPtr>(dataPtr->address);
    }

    pushAddress(reinterpret_cast<Address>(dataPtr));

    getCodeToken();

    while (codeToken == TKN_LBRACKET)
    {
        typePtr = execSubscripts(typePtr);
    }

    TypePtr baseTypePtr = baseType(typePtr);
    StackItemPtr valuePtr = tos;

    if (use != USE_TARGET && use != USE_REFPARAM && typePtr->form != FRM_ARRAY)
    {
        // Replace the address with the value. Port fix: the slot is cleared first. The original overwrote only the
        // value's bytes (one for a char), leaving the rest of the address in the slot.
        Address address = tos->address;
        tos->address = nullptr;

        if (baseTypePtr == CharTypePtr)
        {
            tos->byte = *reinterpret_cast<uint8_t*>(address);
        }
        else
        {
            tos->integer = *reinterpret_cast<int32_t*>(address);
        }
    }

    if (debugger != nullptr && use != USE_TARGET && use != USE_REFPARAM)
    {
        if (typePtr->form == FRM_ARRAY)
        {
            debugger->traceDataFetch(idPtr, typePtr, reinterpret_cast<StackItemPtr>(valuePtr->address));
        }
        else
        {
            debugger->traceDataFetch(idPtr, typePtr, valuePtr);
        }
    }

    return typePtr;
}

auto execFactor() -> TypePtr
{
    switch (codeToken)
    {
        case TKN_IDENTIFIER:
        {
            SymTableNodePtr idPtr = getCodeSymTableNodePtr();

            if (idPtr->defn.key == DFN_FUNCTION)
            {
                SymTableNodePtr thisRoutineIdPtr = CurRoutineIdPtr;
                TypePtr resultTypePtr = execRoutineCall(idPtr);
                CurRoutineIdPtr = thisRoutineIdPtr;
                return resultTypePtr;
            }

            if (idPtr->defn.key == DFN_CONST)
            {
                return execConstant(idPtr);
            }

            return execVariable(idPtr, USE_EXPR);
        }

        case TKN_NUMBER:
        {
            SymTableNodePtr numberPtr = getCodeSymTableNodePtr();
            TypePtr resultTypePtr;

            if (numberPtr->typePtr == IntegerTypePtr)
            {
                pushInteger(numberPtr->defn.info.constant.value.integer);
                resultTypePtr = IntegerTypePtr;
            }
            else
            {
                pushReal(numberPtr->defn.info.constant.value.real);
                resultTypePtr = RealTypePtr;
            }

            getCodeToken();
            return resultTypePtr;
        }

        case TKN_STRING:
        {
            // Literals are named by their text: one character is a char, anything longer a string.
            SymTableNodePtr literalIdPtr = getCodeSymTableNodePtr();
            TypePtr resultTypePtr;

            if (static_cast<int32_t>(strlen(literalIdPtr->name)) > 1)
            {
                pushAddress(literalIdPtr->literalString);
                resultTypePtr = literalIdPtr->typePtr;
            }
            else
            {
                pushByte(literalIdPtr->name[0]);
                resultTypePtr = CharTypePtr;
            }

            getCodeToken();
            return resultTypePtr;
        }

        case TKN_LPAREN:
        {
            getCodeToken();
            TypePtr resultTypePtr = execExpression();
            getCodeToken();
            return resultTypePtr;
        }

        case TKN_NOT:
        {
            getCodeToken();
            TypePtr resultTypePtr = execFactor();
            tos->integer = 1 - tos->integer;
            return resultTypePtr;
        }

        default:
            // Never happens (the compiler only crunches the tokens above); the original returns whatever ECX held.
            return nullptr;
    }
}

auto execTerm() -> TypePtr
{
    TypePtr resultTypePtr = execFactor();

    while (codeToken == TKN_STAR || codeToken == TKN_FSLASH || codeToken == TKN_DIV || codeToken == TKN_MOD ||
           codeToken == TKN_AND)
    {
        TokenCodeType op = codeToken;
        TypePtr operand1TypePtr = baseType(resultTypePtr);
        getCodeToken();
        TypePtr operand2TypePtr = baseType(execFactor());
        StackItemPtr operand2Ptr = tos;
        StackItemPtr operand1Ptr = tos - 1;

        switch (op)
        {
            case TKN_AND:
            {
                operand1Ptr->integer = (operand1Ptr->integer != 0 && operand2Ptr->integer != 0) ? 1 : 0;
                resultTypePtr = BooleanTypePtr;
                break;
            }
            case TKN_STAR:
            {
                if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
                {
                    operand1Ptr->integer = operand2Ptr->integer * operand1Ptr->integer;
                    resultTypePtr = IntegerTypePtr;
                }
                else
                {
                    promoteOperands(operand1TypePtr, operand2TypePtr);
                    operand1Ptr->real = operand2Ptr->real * operand1Ptr->real;
                    resultTypePtr = RealTypePtr;
                }
                break;
            }
            case TKN_FSLASH:
            {
                // '/' on two integers divides as integers. Division by zero gives 0 (no runtime error).
                if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
                {
                    if (operand2Ptr->integer == 0)
                    {
                        operand1Ptr->integer = 0;
                    }
                    else
                    {
                        operand1Ptr->integer = operand1Ptr->integer / operand2Ptr->integer;
                    }

                    resultTypePtr = IntegerTypePtr;
                }
                else
                {
                    promoteOperands(operand1TypePtr, operand2TypePtr);

                    if (operand2Ptr->real == 0.0f)
                    {
                        operand1Ptr->integer = 0;
                    }
                    else
                    {
                        operand1Ptr->real = operand1Ptr->real / operand2Ptr->real;
                    }

                    resultTypePtr = RealTypePtr;
                }
                break;
            }
            case TKN_DIV:
            case TKN_MOD:
            {
                if (operand2Ptr->integer == 0)
                {
                    operand1Ptr->integer = 0;
                }
                else if (op == TKN_DIV)
                {
                    operand1Ptr->integer = operand1Ptr->integer / operand2Ptr->integer;
                }
                else
                {
                    operand1Ptr->integer = operand1Ptr->integer % operand2Ptr->integer;
                }

                resultTypePtr = IntegerTypePtr;
                break;
            }
            default:
                resultTypePtr = operand1TypePtr;
                break;
        }

        pop();
    }

    return resultTypePtr;
}

auto execSimpleExpression() -> TypePtr
{
    TokenCodeType unaryOp = TKN_PLUS;

    if (codeToken == TKN_PLUS || codeToken == TKN_MINUS)
    {
        unaryOp = codeToken;
        getCodeToken();
    }

    TypePtr resultTypePtr = execTerm();

    if (unaryOp == TKN_MINUS)
    {
        if (resultTypePtr == IntegerTypePtr)
        {
            tos->integer = -tos->integer;
        }
        else
        {
            tos->real = -tos->real;
        }
    }

    while (codeToken == TKN_PLUS || codeToken == TKN_MINUS || codeToken == TKN_OR)
    {
        TokenCodeType op = codeToken;
        TypePtr operand1TypePtr = baseType(resultTypePtr);
        getCodeToken();
        TypePtr operand2TypePtr = baseType(execTerm());
        StackItemPtr operand2Ptr = tos;
        StackItemPtr operand1Ptr = tos - 1;

        if (op == TKN_OR)
        {
            operand1Ptr->integer = (operand1Ptr->integer == 0 && operand2Ptr->integer == 0) ? 0 : 1;
            resultTypePtr = BooleanTypePtr;
        }
        else if (operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr)
        {
            if (op == TKN_PLUS)
            {
                operand1Ptr->integer = operand1Ptr->integer + operand2Ptr->integer;
            }
            else
            {
                operand1Ptr->integer = operand1Ptr->integer - operand2Ptr->integer;
            }

            resultTypePtr = IntegerTypePtr;
        }
        else
        {
            promoteOperands(operand1TypePtr, operand2TypePtr);

            if (op == TKN_PLUS)
            {
                operand1Ptr->real = operand2Ptr->real + operand1Ptr->real;
            }
            else
            {
                operand1Ptr->real = operand1Ptr->real - operand2Ptr->real;
            }

            resultTypePtr = RealTypePtr;
        }

        pop();
    }

    return resultTypePtr;
}

auto execExpression() -> TypePtr
{
    TypePtr resultTypePtr = execSimpleExpression();
    TokenCodeType op = codeToken;

    if (op != TKN_EQUALEQUAL && op != TKN_LT && op != TKN_GT && op != TKN_NE && op != TKN_LE && op != TKN_GE)
    {
        return resultTypePtr;
    }

    TypePtr operand1TypePtr = baseType(resultTypePtr);
    getCodeToken();
    TypePtr operand2TypePtr = baseType(execSimpleExpression());
    StackItemPtr operand2Ptr = tos;
    StackItemPtr operand1Ptr = tos - 1;

    // Anything unhandled (mismatched operand types, which the compiler rejects) reads an uninitialised local in the
    // original; the port gives false.
    bool result = false;

    if ((operand1TypePtr == IntegerTypePtr && operand2TypePtr == IntegerTypePtr) || operand1TypePtr->form == FRM_ENUM)
    {
        int32_t value1 = operand1Ptr->integer;
        int32_t value2 = operand2Ptr->integer;

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
        uint8_t value1 = operand1Ptr->byte;
        uint8_t value2 = operand2Ptr->byte;

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
    else if (operand1TypePtr->form == FRM_ARRAY && operand1TypePtr->info.array.elementTypePtr == CharTypePtr)
    {
        // Original behaviour (OB-039): string comparisons are never evaluated; every one is true.
        result = true;
    }
    else if (operand1TypePtr == RealTypePtr || operand2TypePtr == RealTypePtr)
    {
        promoteOperands(operand1TypePtr, operand2TypePtr);
        float value1 = operand1Ptr->real;
        float value2 = operand2Ptr->real;

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

    operand1Ptr->integer = result ? 1 : 0;
    pop();
    return BooleanTypePtr;
}
