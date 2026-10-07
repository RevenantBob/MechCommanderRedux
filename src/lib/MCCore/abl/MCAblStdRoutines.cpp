#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/move.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "iface/iface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/cmponent.h"
#include "object/comndr.h"
#include "object/contact.h"
#include "object/gameobj.h"
#include "object/gate.h"
#include "object/group.h"
#include "object/gvehicl.h"
#include "object/mover.h"
#include "object/object.h"
#include "object/objque.h"
#include "object/objtype.h"
#include "object/tbldng.h"
#include "object/team.h"
#include "object/terrobj.h"
#include "object/train.h"
#include "object/turret.h"
#include "object/warrior.h"
#include "sound/radio.h"
#include "sound/soundsys.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"

// The language's built-ins: return, print, concat, the maths, the module name and loop limit, fatal and assert.

namespace
{
    /// <summary>
    /// The value on top of the stack as print and concat show it: an integer, a char, a real (4 decimals) or a
    /// string.
    /// </summary>
    auto FormatValue(MCAblRuntime& abl, MCAblType* typePtr) -> std::string
    {
        if (typePtr == IntegerTypePtr)
        {
            return std::format("{}", abl.Top().Integer);
        }

        if (typePtr == CharTypePtr)
        {
            return std::string(1, static_cast<char>(abl.Top().Byte));
        }

        if (typePtr == RealTypePtr)
        {
            return std::format("{:.4f}", abl.Top().Real);
        }

        if (typePtr->Form == MCAblTypeForm::Array && typePtr->Array.ElementTypePtr == CharTypePtr)
        {
            return reinterpret_cast<const char*>(abl.Top().Address);
        }

        // The original returned its empty buffer.
        return {};
    }

    /// <summary>Reports where a print, fatal or assert ran: the module, file and line.</summary>
    auto PrintLocation(MCAblRuntime& abl, std::string_view what, bool withId) -> void
    {
        MCAblDebugger* debugger = abl.Debugger();
        debugger->Print(what);
        MCAblModule* module = abl.CurrentModule();
        debugger->Print(withId ? std::format("   MODULE ({}) {}", module->Id(), module->Name())
                               : std::format("   MODULE {}", module->Name()));
        debugger->Print(std::format("   FILE {}", abl.FileName()));
        debugger->Print(std::format("   LINE {}", abl.LineNumber()));
    }
}

auto ExecStdReturn(MCAblRuntime& abl) -> void
{
    abl.ExecReturn();
}

auto ExecStdPrint(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    MCAblType* typePtr = abl.ExecExpression();
    std::string text = FormatValue(abl, typePtr);
    abl.Pop();

    if (abl.Debugger())
    {
        PrintLocation(abl, std::format("PRINT:  \"{}\"", text), false);
        abl.GetCodeToken();
        return;
    }

    if (TacticalMap() && TacticalMap()->ChatWindow)
    {
        TacticalMap()->ChatWindow->ProcessChatString(0, text.data(), -1);
    }

    abl.GetCodeToken();
}

auto ExecStdConcat(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    char* destination = abl.Top().Address;
    abl.Pop();
    abl.GetCodeToken();
    MCAblType* typePtr = abl.ExecExpression();
    // Faithful: appended past the string's terminator whatever the array's size.
    const std::string text = FormatValue(abl, typePtr);
    const size_t length = std::char_traits<char>::length(destination);
    text.copy(destination + length, text.size());
    destination[length + text.size()] = '\0';
    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdAbs(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    MCAblType* resultTypePtr = IntegerTypePtr;

    if (abl.ExecExpression() == IntegerTypePtr)
    {
        if (abl.Top().Integer < 0)
        {
            abl.Top().Integer = -abl.Top().Integer;
        }
    }
    else
    {
        resultTypePtr = RealTypePtr;

        if (abl.Top().Real < 0.0f)
        {
            abl.Top().Real = -abl.Top().Real;
        }
    }

    abl.GetCodeToken();
    return resultTypePtr;
}

auto ExecStdRound(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();

    if (static_cast<double>(abl.Top().Real) > 0.0)
    {
        abl.Top().Integer = static_cast<int32_t>(static_cast<double>(abl.Top().Real) + 0.5);
    }
    else
    {
        abl.Top().Integer = static_cast<int32_t>(static_cast<double>(abl.Top().Real) - 0.5);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdSqrt(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();

    if (abl.ExecExpression() == IntegerTypePtr)
    {
        abl.Top().Real = static_cast<float>(abl.Top().Integer);
    }

    if (abl.Top().Real < 0.0f)
    {
        abl.RuntimeError(MCAblRuntimeError::InvalidFunctionArgument);
    }
    else
    {
        abl.Top().Real = std::sqrt(abl.Top().Real);
    }

    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecStdTrunc(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();

    if (abl.ExecExpression() == RealTypePtr)
    {
        abl.Top().Integer = static_cast<int32_t>(abl.Top().Real);
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdRandom(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = RandomNumber(abl.Top().Integer);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdGetModHandle(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(abl.CurrentModuleHandle());
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdGetModName(MCAblRuntime& abl) -> MCAblType*
{
    return nullptr;
}

auto ExecStdSetModName(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    MCAblType* typePtr = abl.ExecExpression();

    if (typePtr->Form != MCAblTypeForm::Array || typePtr->Array.ElementTypePtr != CharTypePtr)
    {
        abl.RuntimeError(MCAblRuntimeError::InvalidFunctionArgument);
    }

    // Original behaviour: the name is left on the stack and never used.
    abl.GetCodeToken();
}

auto ExecStdSetMaxLoops(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.SetMaxLoopIterations(abl.Top().Integer + 1);
    abl.Pop();
    abl.GetCodeToken();
    return nullptr;
}

auto ExecStdFatal(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    const int32_t code = abl.Top().Integer;
    abl.Pop();
    const char* text = abl.NextAddress();

    if (abl.Debugger())
    {
        PrintLocation(abl, std::format("FATAL:  [{}] \"{}\"", code, text), true);
        abl.Debugger()->DebugMode();
        abl.GetCodeToken();
        return nullptr;
    }

    // Original behaviour: the formatted message ("ABL FATAL: [code] text") is dropped; Fatal gets the script's text.
    Fatal(0, text);
}

auto ExecStdAssert(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    const int32_t expression = abl.Top().Integer;
    abl.Pop();
    const int32_t code = abl.NextInteger();
    const char* text = abl.NextAddress();

    if (expression == 0)
    {
        if (abl.Debugger())
        {
            PrintLocation(abl, std::format("ASSERT:  [{}] \"{}\"", code, text), true);
            abl.Debugger()->DebugMode();
            abl.GetCodeToken();
            return nullptr;
        }

        Fatal(0, std::format("ABL ASSERT: [{}] {}", code, text));
    }

    abl.GetCodeToken();
    return nullptr;
}
