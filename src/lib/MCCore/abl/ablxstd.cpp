#include "stdafx.h"
#include "abl/ablxstd.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/MCAblErrors.h"
#include "abl/ablexec.h"
#include "abl/MCAblCompiler.h"
#include "abl/ablrtn.h"
#include "abl/ablxexpr.h"
#include "abl/ablxstmt.h"
#include "ai/move.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "iface/iface.h"
#include "lib/MCFatal.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "lib/MCDice.h"
#include "lib/MCFile.h"
#include "logistics/logmain.h"
#include "main/main.h"
#include "mission/scenario.h"
#include "network/multplyr.h"
#include "object/artlry.h"
#include "object/bldng.h"
#include "object/bridge.h"
#include "object/cmponent.h"
#include "object/contact.h"
#include "object/comndr.h"
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
#include "terrain/MCTerrain.h"
#include "terrain/MCTacticalMap.h"

// Every exec routine reads its call the way the compiler wrote it: the routine's token, then "(", each argument
// expression evaluated onto the stack (by-reference arguments as the variable's address), a separator token
// between them, and ")". Most pop their arguments and leave the last slot on the stack holding the result.

int TacOrderOrigin = 1;
MCAblToken ExitRoutineCodeSegment[2] = {MCAblToken::EndFunction, MCAblToken::Semicolon};
MCAblToken ExitOrderCodeSegment[2] = {MCAblToken::EndFunction, MCAblToken::Semicolon};
int16_t MissionScriptMessageLog[1000][3] = {};
int32_t NumMissionScriptMessages = 0;
MCMover* MoverList[256] = {};
int IsUnitOrder = 0;
MCMoverGroup* CurGroup = nullptr;
MCGameObject* CurObject = nullptr;
int32_t CurObjectClass = 0;
int32_t CurAlarm = 0;
MCMechWarrior* CurWarrior = nullptr;
MCGameObject* CurContact = nullptr;
int32_t CurMultiplayCode = 0;
int32_t CurMultiplayParam = 0;
float GlobalMissionValues[50] = {};

namespace
{
    /// <summary>Evaluates the next argument (after its separator token) and pops it as an integer.</summary>
    auto NextInteger() -> int32_t
    {
        GetCodeToken();
        ExecExpression();
        int32_t value = Tos->Integer;
        Pop();
        return value;
    }

    /// <summary>Evaluates the next argument (after its separator token) and pops it as a real.</summary>
    auto NextReal() -> float
    {
        GetCodeToken();
        ExecExpression();
        float value = Tos->Real;
        Pop();
        return value;
    }

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an address (an array's
    /// memory or a string).</summary>
    auto NextAddress() -> MCAddress
    {
        GetCodeToken();
        ExecExpression();
        MCAddress value = Tos->Address;
        Pop();
        return value;
    }

    /// <summary>Evaluates the next by-reference argument: the variable's address, left on the stack.</summary>
    auto NextReference() -> MCAddress
    {
        MCAblSymbol* idPtr = GetCodeSymTableNodePtr();
        ExecVariable(idPtr, MCAblUse::RefParam);
        return Tos->Address;
    }

    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or mover).</summary>
    auto IsMover(MCBaseObject* object) -> bool
    {
        MCObjectClass objectClass = object->ObjectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>The object a script names by part id: -1 is the object whose brain runs.</summary>
    auto FindObject(int32_t partId) -> MCGameObject*
    {
        if (partId == -1)
        {
            return CurObject;
        }

        return static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(partId));
    }

    /// <summary>Whether <paramref name="partId"/> names a group of movers (1..0x1ff) rather than one object.</summary>
    auto IsGroupId(int32_t partId) -> bool
    {
        return partId >= 1 && partId <= 0x1ff;
    }

    /// <summary>
    /// Fills <see cref="MoverList"/> with the movers a group id names: 1..32 the player commander's groups,
    /// 0xa5..0xc4 commander 1's, 0x149..0x168 commander 2's, 500 / 501 / 502 the Inner Sphere, Clan and allied
    /// teams.
    /// </summary>
    /// <returns>How many movers.</returns>
    auto GetGroupMovers(int32_t groupId) -> int32_t
    {
        if (groupId < 0x21)
        {
            return CommanderTable[0]->GetGroup(groupId - 1)->GetMovers(MoverList);
        }

        if (groupId >= 0x149 && groupId < 0x169)
        {
            return CommanderTable[2]->GetGroup(groupId - 0x149)->GetMovers(MoverList);
        }

        if (groupId >= 0xa5 && groupId < 0xc5)
        {
            return CommanderTable[1]->GetGroup(groupId - 0xa5)->GetMovers(MoverList);
        }

        if (groupId == 500)
        {
            return InnerSphereTeam->GetRoster(reinterpret_cast<MCGameObject**>(MoverList));
        }

        if (groupId == 0x1f6)
        {
            if (AlliedTeam == nullptr)
            {
                return 0;
            }

            return AlliedTeam->GetRoster(reinterpret_cast<MCGameObject**>(MoverList));
        }

        if (groupId == 0x1f5)
        {
            return ClanTeam->GetRoster(reinterpret_cast<MCGameObject**>(MoverList));
        }

        return 0;
    }

    /// <summary>The frame of the routine running (<see cref="CurRoutineIdPtr"/>), for its function value.</summary>
    auto CurrentRoutineFrame() -> MCStackItemPtr
    {
        MCStackItemPtr framePtr = StackFrameBasePtr;

        for (int32_t delta = Level - CurRoutineIdPtr->Level - 1; delta > 0; delta--)
        {
            framePtr =
                reinterpret_cast<MCStackItemPtr>(reinterpret_cast<MCStackFrameHeaderPtr>(framePtr)->StaticLink.Address);
        }

        return framePtr;
    }

    /// <summary>
    /// Formats the value on top of the stack for print and concat: an integer, char or real into
    /// <paramref name="buffer"/>, a string as itself.
    /// </summary>
    auto FormatValue(MCAblType* typePtr, char* buffer, size_t bufferSize) -> char*
    {
        if (typePtr == IntegerTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%d", Tos->Integer);
        }
        else if (typePtr == CharTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%c", Tos->Byte);
        }
        else if (typePtr == RealTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%.4f", static_cast<double>(Tos->Real));
        }
        else if (typePtr->Form == MCAblTypeForm::Array && typePtr->Array.ElementTypePtr == CharTypePtr)
        {
            return reinterpret_cast<char*>(Tos->Address);
        }

        return buffer;
    }

    /// <summary>The debugger's report of where a print, fatal or assert ran.</summary>
    auto PrintLocation(char* message) -> void
    {
        Debugger->Print(message);
    }
}

auto ExecOrderReturn(MCAblSymbol* routineIdPtr, int32_t returnValue) -> void
{
    MCAblSymbol* curRoutineIdPtr = CurRoutineIdPtr;
    MCStackItemPtr framePtr = CurrentRoutineFrame();
    framePtr->Integer = returnValue;
    ::ReturnValue = {};
    ::ReturnValue.Integer = returnValue;

    if (Debugger)
    {
        Debugger->TraceDataStore(curRoutineIdPtr, curRoutineIdPtr->TypePtr, framePtr, curRoutineIdPtr->TypePtr);
    }

    ExitWithReturn = 1;
    ExitFromTacOrder = 1;

    if (returnValue != 1)
    {
        CodeSegmentPtr = reinterpret_cast<char*>(ExitOrderCodeSegment);
        GetCodeToken();
    }
}

auto ExecStdReturn(MCAblSymbol* routineIdPtr) -> void
{
    ReturnValue = {};
    MCAblType* returnTypePtr = CurRoutineIdPtr->TypePtr;

    if (returnTypePtr)
    {
        MCStackItemPtr framePtr = CurrentRoutineFrame();
        GetCodeToken();
        MCAblType* returnBaseTypePtr = returnTypePtr;
        GetCodeToken();
        MCAblType* expressionTypePtr = ExecExpression();

        if (returnTypePtr == RealTypePtr && expressionTypePtr == IntegerTypePtr)
        {
            framePtr->Real = static_cast<float>(Tos->Integer);
        }
        else if (returnTypePtr->Form == MCAblTypeForm::Array)
        {
            // Original behaviour: the array is copied over the frame's function value slot (and past it).
            std::memcpy(framePtr, Tos->Address, static_cast<size_t>(returnTypePtr->Size));
        }
        else if (returnBaseTypePtr == IntegerTypePtr || returnTypePtr->Form == MCAblTypeForm::Enum)
        {
            framePtr->Integer = Tos->Integer;
        }
        else
        {
            framePtr->Real = Tos->Real;
        }

        Pop();
        ReturnValue.Real = framePtr->Real;

        if (Debugger)
        {
            Debugger->TraceDataStore(CurRoutineIdPtr, CurRoutineIdPtr->TypePtr, framePtr, returnTypePtr);
        }
    }

    GetCodeToken();
    CodeSegmentPtr = reinterpret_cast<char*>(ExitRoutineCodeSegment);
    ExitWithReturn = 1;
    GetCodeToken();
}

auto ExecStdPrint(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    MCAblType* typePtr = ExecExpression();
    char buffer[20];
    char* text = FormatValue(typePtr, buffer, sizeof(buffer));
    Pop();

    if (Debugger)
    {
        char message[512];
        std::snprintf(message, sizeof(message), "PRINT:  \"%s\"", text);
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   MODULE %s", CurModule->GetName().c_str());
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   FILE %s", CurModule->GetSourceFile(ExecFileNumber));
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   LINE %d", ExecLineNumber);
        PrintLocation(message);
        GetCodeToken();
        return;
    }

    if (TacticalMap() && TacticalMap()->ChatWindow)
    {
        TacticalMap()->ChatWindow->ProcessChatString(0, text, -1);
    }

    GetCodeToken();
}

auto ExecStdConcat(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    char* destination = reinterpret_cast<char*>(Tos->Address);
    Pop();
    GetCodeToken();
    MCAblType* typePtr = ExecExpression();
    char buffer[20];
    char* text = FormatValue(typePtr, buffer, sizeof(buffer));
    std::strcat(destination, text);
    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdAbs(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    MCAblType* resultTypePtr = IntegerTypePtr;

    if (ExecExpression() == IntegerTypePtr)
    {
        if (Tos->Integer < 0)
        {
            Tos->Integer = -Tos->Integer;
        }
    }
    else
    {
        resultTypePtr = RealTypePtr;

        if (Tos->Real < 0.0f)
        {
            Tos->Real = -Tos->Real;
        }
    }

    GetCodeToken();
    return resultTypePtr;
}

auto ExecStdRound(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();

    if (static_cast<double>(Tos->Real) > 0.0)
    {
        Tos->Integer = static_cast<int32_t>(static_cast<double>(Tos->Real) + 0.5);
    }
    else
    {
        Tos->Integer = static_cast<int32_t>(static_cast<double>(Tos->Real) - 0.5);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdSqrt(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();

    if (ExecExpression() == IntegerTypePtr)
    {
        Tos->Real = static_cast<float>(Tos->Integer);
    }

    if (Tos->Real < 0.0f)
    {
        RuntimeError(MCAblRuntimeError::InvalidFunctionArgument);
    }
    else
    {
        Tos->Real = std::sqrt(Tos->Real);
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecStdTrunc(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();

    if (ExecExpression() == RealTypePtr)
    {
        Tos->Integer = static_cast<int32_t>(Tos->Real);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdRandom(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = RandomNumber(Tos->Integer);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdGetModHandle(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(CurModuleHandle);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStdGetModName(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    return nullptr;
}

auto ExecStdSetModName(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    MCAblType* typePtr = ExecExpression();

    if (typePtr->Form != MCAblTypeForm::Array || typePtr->Array.ElementTypePtr != CharTypePtr)
    {
        RuntimeError(MCAblRuntimeError::InvalidFunctionArgument);
    }

    // Original behaviour: the name is left on the stack and never used.
    GetCodeToken();
}

auto ExecStdSetMaxLoops(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MaxLoopIterations = Tos->Integer + 1;
    Pop();
    GetCodeToken();
    return nullptr;
}

auto ExecStdFatal(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t code = Tos->Integer;
    Pop();
    char* text = reinterpret_cast<char*>(NextAddress());

    char message[512];

    if (Debugger)
    {
        std::snprintf(message, sizeof(message), "FATAL:  [%d] \"%s\"", code, text);
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   MODULE (%d) %s", CurModule->GetId(), CurModule->GetName().c_str());
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   FILE %s", CurModule->GetSourceFile(ExecFileNumber));
        PrintLocation(message);
        std::snprintf(message, sizeof(message), "   LINE %d", ExecLineNumber);
        PrintLocation(message);
        Debugger->DebugMode();
        GetCodeToken();
        return nullptr;
    }

    std::snprintf(message, sizeof(message), "ABL FATAL: [%d] %s", code, text);
    // Original behaviour: the formatted message is dropped; Fatal gets the script's text.
    Fatal(0, text);
}

auto ExecStdAssert(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t expression = Tos->Integer;
    Pop();
    int32_t code = NextInteger();
    char* text = reinterpret_cast<char*>(NextAddress());

    if (expression == 0)
    {
        char message[512];

        if (Debugger)
        {
            std::snprintf(message, sizeof(message), "ASSERT:  [%d] \"%s\"", code, text);
            PrintLocation(message);
            std::snprintf(message, sizeof(message), "   MODULE (%d) %s", CurModule->GetId(),
                          CurModule->GetName().c_str());
            PrintLocation(message);
            std::snprintf(message, sizeof(message), "   FILE %s", CurModule->GetSourceFile(ExecFileNumber));
            PrintLocation(message);
            std::snprintf(message, sizeof(message), "   LINE %d", ExecLineNumber);
            PrintLocation(message);
            Debugger->DebugMode();
            GetCodeToken();
            return nullptr;
        }

        std::snprintf(message, sizeof(message), "ABL ASSERT: [%d] %s", code, text);
        Fatal(0, message);
    }

    GetCodeToken();
    return nullptr;
}

auto ExecHbGetId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(0);

    if (CurObject)
    {
        Tos->Integer = CurObject->PartId;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetTime(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushReal(ActualTime);
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTimeLeft(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    float timeLeft;

    if (Scenario->TimeLimit < 0)
    {
        timeLeft = -1.0f;
    }
    else
    {
        timeLeft = static_cast<float>(Scenario->TimeLimit) - ActualTime;

        if (timeLeft <= 0.0f)
        {
            timeLeft = 0.0f;
        }
    }

    PushReal(timeLeft);
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTarget(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    if (IsUnitOrder == 0)
    {
        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(partId);

            if (object && IsMover(object))
            {
                MCMechWarrior* pilot = object->GetPilot();
                Assert(pilot != nullptr, 0, " execHbGetTarget:No pilot in mover! ");
                MCGameObject* target = pilot->GetLastTarget();

                if (target)
                {
                    Tos->Integer = target->PartId;
                }
            }
        }
    }
    else
    {
        MCGameObject* target = CurGroup->GetPointPilot()->GetLastTarget();

        // Port fix: the original reads the part id of a null target.
        if (target)
        {
            Tos->Integer = target->PartId;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetTarget(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t targetId = NextInteger();
    MCGameObject* target = FindObject(targetId);

    if (IsGroupId(partId))
    {
        int32_t numMovers = GetGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMechWarrior* pilot = MoverList[i]->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
            }
        }
    }
    else
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            MCMechWarrior* pilot = object->GetPilot();

            if (pilot)
            {
                pilot->SetCurrentTarget(target);
                static_cast<MCMover*>(pilot->Vehicle)->CalcOptimalRange(nullptr);
            }
        }
    }

    GetCodeToken();
}

auto ExecHbSelectUnit(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = -1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectObject(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    int32_t previousId = 0;
    ExecExpression();

    if (CurObject)
    {
        previousId = CurObject->PartId;
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object)
    {
        CurObject = static_cast<MCGameObject*>(object);
        Tos->Integer = previousId;
    }
    else
    {
        Tos->Integer = -1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectWarrior(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t warriorIndex = Tos->Integer;
    Tos->Integer = -1;
    int32_t previousIndex = 0;

    if (CurWarrior)
    {
        previousIndex = CurWarrior->Index;
    }

    Tos->Integer = previousIndex;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario->NumWarriors)
    {
        CurWarrior = Scenario->Warriors[warriorIndex];
    }
    else
    {
        CurWarrior = nullptr;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWarriorStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t warriorIndex = Tos->Integer;
    Tos->Integer = -1;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= Scenario->NumWarriors)
    {
        MCMechWarrior* warrior = Scenario->Warriors[warriorIndex];

        if (warrior)
        {
            Tos->Integer = warrior->Status;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContacts(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    int32_t* contacts = reinterpret_cast<int32_t*>(NextReference());
    Pop();
    int32_t contactCriteria = NextInteger();
    GetCodeToken();
    ExecExpression();
    int32_t sortType = Tos->Integer;
    Tos->Integer = -1;

    if (IsMover(CurObject))
    {
        Tos->Integer = CurObject->GetTeam()->GetContacts(CurObject, contacts, contactCriteria, sortType);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetEnemyCount(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = -1;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            if (IsMover(object))
            {
                Tos->Integer = object->GetTeam()->NumLosContacts;
            }
            else if (object->ObjectClass == ARTILLERY || object->ObjectClass == BUILDING ||
                     object->ObjectClass == TREEBUILDING)
            {
                int32_t alignment = object->GetAlignment();

                if (alignment == -1)
                {
                    Tos->Integer = ClanTeam->NumLosContacts;
                }
                else if (alignment == 1)
                {
                    Tos->Integer = InnerSphereTeam->NumLosContacts;
                }
            }
        }
    }
    else if (partId == 500)
    {
        Tos->Integer = InnerSphereTeam->NumLosContacts;
    }
    else if (partId == 0x1f5)
    {
        Tos->Integer = ClanTeam->NumLosContacts;
    }
    else if (partId == 0x1f6 && AlliedTeam)
    {
        Tos->Integer = AlliedTeam->NumLosContacts;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSelectContact(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = -1;

    if (IsMover(CurObject))
    {
        MCGameObject* contact = FindObject(partId);

        if (contact && CurObject->GetTeam()->GetContactType(contact) != 0)
        {
            CurContact = contact;
            Tos->Integer = 0;
        }
        else
        {
            Tos->Integer = 1;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsContact(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t contactCriteria = NextInteger();
    GetCodeToken();
    ExecExpression();
    int32_t select = Tos->Integer;
    Tos->Integer = -1;

    if (IsMover(CurObject))
    {
        MCGameObject* object = FindObject(partId);

        if (CurObject->GetTeam()->IsContact(object, contactCriteria) == 0)
        {
            Tos->Integer = 0;
        }
        else
        {
            Tos->Integer = partId;

            if (select)
            {
                CurContact = object;
            }
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(CurContact ? CurContact->PartId : 0);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    int32_t* tagged = reinterpret_cast<int32_t*>(NextReference());
    Tos->Integer = 0;

    *tagged = 0;
    if (CurContact)
    {
        int taggedFlag;
        Tos->Integer = CurContact->GetContactType(CurObject->GetTeam()->Id, taggedFlag);
        *tagged = (taggedFlag == 1) ? 1 : 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetContactRelativePosition(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    float* range = reinterpret_cast<float*>(NextReference());
    Pop();
    GetCodeToken();
    float* angle = reinterpret_cast<float*>(NextReference());
    *range = -1.0f;
    Tos->Integer = 1;

    *angle = 0.0f;
    if (CurContact && CurObject)
    {
        MCVector3D contactPosition = CurContact->GetPosition();
        *range = static_cast<float>(CurObject->DistanceFrom(contactPosition));
        *angle = CurObject->RelFacingTo(CurContact->GetPosition(), -1);
        Tos->Integer = 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetPotentialContact(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t contactType = Tos->Integer;

    // Original behaviour: the contact type is left on the stack as the result.
    if (IsGroupId(partId))
    {
        int32_t numMovers = GetGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            MoverList[i]->SetPotentialContact(contactType);
        }
    }
    else
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            object->SetPotentialContact(contactType);
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeapons(MCAblSymbol* routineIdPtr, MCAblRoutineKey key) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(NextReference());
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t listSize = Tos->Integer;
    MCGameObject* target = CurWarrior->GetLastTarget();
    Tos->Integer = -1;

    if (IsMover(CurObject))
    {
        MCMover* mover = static_cast<MCMover*>(CurObject);

        if (key == MCAblRoutineKey::GetWeaponsReady)
        {
            Tos->Integer = mover->GetWeaponsReady(weaponList, listSize);
        }
        else if (key == MCAblRoutineKey::GetWeaponsInRange && target)
        {
            MCVector3D targetPosition = target->GetPosition();
            Tos->Integer =
                mover->GetWeaponsInRange(weaponList, listSize, static_cast<float>(mover->DistanceFrom(targetPosition)));
        }
        else
        {
            Tos->Integer = mover->GetWeaponsLocked(weaponList, listSize);
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponShots(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t weaponIndex = Tos->Integer;
    Tos->Integer = -1;

    if (IsMover(CurObject))
    {
        Tos->Integer = static_cast<MCMover*>(CurObject)->GetWeaponShots(weaponIndex);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponRanges(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    float* ranges = reinterpret_cast<float*>(NextReference());
    Pop();
    MCGameObject* object = FindObject(partId);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);

        if (mover->ShortestRangeWeapon == 0xff)
        {
            ranges[0] = -1.0f;
        }
        else
        {
            ranges[0] = MasterComponentList[mover->Inventory[mover->ShortestRangeWeapon].MasterID].WeaponRange[1];
        }

        ranges[1] = mover->GetFireRange(-1);

        if (ranges[1] == -1.0f)
        {
            mover->CalcOptimalRange(nullptr);
            ranges[1] = mover->GetFireRange(-1);
        }

        ranges[2] = mover->GetFireRange(-2);
    }
    else
    {
        ranges[2] = 0.0f;
        ranges[1] = 0.0f;
        ranges[0] = 0.0f;
    }

    GetCodeToken();
}

auto ExecHbSetMoveGoal(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    uint32_t goalType = static_cast<uint32_t>(Tos->Integer);
    Pop();
    GetCodeToken();
    float* location = reinterpret_cast<float*>(NextReference());

    if (CurWarrior)
    {
        MCVector3D goal;
        goal.X = location[0];
        goal.Y = location[1];
        goal.Z = location[2];
        CurWarrior->SetMoveGoal(goalType, &goal, nullptr);
        location[0] = goal.X;
        location[1] = goal.Y;
        location[2] = goal.Z;
        CurWarrior->MoveOrders.ScriptGoal = 1;
    }

    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetChallenger(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object && IsMover(object))
        {
            MCGameObject* challenger = static_cast<MCMover*>(object)->GetChallenger();

            if (challenger)
            {
                Tos->Integer = challenger->PartId;
            }
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetFireRanges(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    float* ranges = reinterpret_cast<float*>(NextReference());
    Pop();
    ranges[0] = WeaponRange[0];
    ranges[1] = WeaponRange[1];
    ranges[2] = WeaponRange[2];
    ranges[3] = Scenario->MaxWeaponRange;
    GetCodeToken();
    return nullptr;
}

auto ExecHbGetAttackers(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    uint32_t* attackerList = reinterpret_cast<uint32_t*>(NextReference());
    Pop();
    GetCodeToken();
    ExecExpression();
    float seconds = Tos->Real;
    Tos->Integer = 0;

    if (CurWarrior)
    {
        Tos->Integer = CurWarrior->GetAttackers(attackerList, seconds);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetAttackerInfo(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    uint32_t attackerId = static_cast<uint32_t>(Tos->Integer);
    Tos->Real = 1000000.0f;

    if ((attackerId == 0 || attackerId > 0x1ff) && CurWarrior)
    {
        MCAttackerRec* attackerRec = CurWarrior->GetAttackerInfo(attackerId);

        if (attackerRec)
        {
            Tos->Real = ScenarioTime - attackerRec->LastTime;
        }
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetTimeWithoutOrders(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushReal(0.0f);

    if (CurWarrior && CurWarrior->TimeOfLastOrders >= 0.0f)
    {
        Tos->Real = ScenarioTime - CurWarrior->TimeOfLastOrders;
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetChallenger(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t challengerId = Tos->Integer;
    Tos->Integer = 0;

    if (IsGroupId(challengerId))
    {
        Tos->Integer = -1;
    }
    else
    {
        MCGameObject* challenger = FindObject(challengerId);
        MCGameObject* object = FindObject(partId);

        if (object && IsMover(object))
        {
            static_cast<MCMover*>(object)->SetChallenger(challenger);
        }
        else
        {
            Tos->Integer = -2;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetMemoryInteger(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t cell = Tos->Integer;
    Pop();
    int32_t value = NextInteger();
    CurWarrior->Memory[cell].Integer = value;
    GetCodeToken();
}

auto ExecHbSetMemoryReal(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t cell = Tos->Integer;
    Pop();
    float value = NextReal();
    CurWarrior->Memory[cell].Real = value;
    GetCodeToken();
}

auto ExecHbHasMoveGoal(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    if (CurWarrior && CurWarrior->MoveOrders.ScriptGoal != 0 && CurWarrior->MoveOrders.GoalType != -1)
    {
        PushInteger(1);
    }
    else
    {
        PushInteger(0);
    }

    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbHasMovePath(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    if (CurWarrior && CurWarrior->GetMovePath() && CurWarrior->MoveOrders.ScriptGoal == 0)
    {
        PushInteger(1);
    }
    else
    {
        PushInteger(0);
    }

    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSortWeapons(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(NextReference());
    Pop();
    int32_t listSize = NextInteger();
    int32_t sortType = NextInteger();
    int32_t valueList[48];

    if (CurObject && IsMover(CurObject))
    {
        static_cast<MCMover*>(CurObject)->SortWeapons(weaponList, valueList, listSize, sortType, 1);
    }

    GetCodeToken();
}

auto ExecHbGetObjectPosition(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    float* position = reinterpret_cast<float*>(NextReference());
    Pop();
    PushInteger(0);

    if (IsGroupId(partId))
    {
        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;
    }
    else
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            MCVector3D objectPosition = object->GetPosition();
            position[0] = objectPosition.X;
            position[1] = objectPosition.Y;
            position[2] = objectPosition.Z;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetVisualRange(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;

    if (IsGroupId(partId))
    {
        Tos->Real = -1.0f;
    }
    else
    {
        // Original behaviour: for an object that isn't a mover the part id stays as the (real) result.
        MCGameObject* object = FindObject(partId);

        if (object && IsMover(object))
        {
            Tos->Real = static_cast<MCMover*>(object)->GetVisualRange();
        }
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetMemoryInteger(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = CurWarrior->Memory[Tos->Integer].Integer;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMemoryReal(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Real = CurWarrior->Memory[Tos->Integer].Real;
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetAlarmTriggers(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    uint32_t* triggerList = reinterpret_cast<uint32_t*>(NextReference());
    Tos->Integer = CurWarrior->GetAlarmTriggers(CurAlarm, triggerList);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetUnitMates(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    GetCodeToken();
    int32_t* mateList = reinterpret_cast<int32_t*>(NextReference());
    Tos->Integer = 0;
    int32_t numMates = 0;

    if (IsGroupId(partId))
    {
        numMates = GetGroupMovers(partId);
    }
    else
    {
        MCGameObject* object = FindObject(partId);

        if (!object || !IsMover(object) || !static_cast<MCMover*>(object)->Group)
        {
            GetCodeToken();
            return IntegerTypePtr;
        }

        numMates = static_cast<MCMover*>(object)->Group->GetMovers(MoverList);
    }

    for (int32_t i = 0; i < numMates; i++)
    {
        mateList[i] = MoverList[i]->PartId;
    }

    Tos->Integer = numMates;
    GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>gettacorder / getlasttacorder: the order's time stamp and parameters of a mover's pilot.</summary>
    auto GetTacOrderData(bool last) -> void
    {
        GetCodeToken();
        GetCodeToken();
        ExecExpression();
        int32_t partId = Tos->Integer;
        GetCodeToken();
        float* timeStamp = reinterpret_cast<float*>(NextReference());
        Pop();
        GetCodeToken();
        int32_t* paramList = reinterpret_cast<int32_t*>(NextReference());
        Tos->Integer = 0;

        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(partId);

            if (object && IsMover(object))
            {
                MCMechWarrior* pilot = object->GetPilot();

                if (pilot)
                {
                    MCTacticalOrder& order = last ? pilot->LastTacOrder : pilot->CurTacOrder;
                    Tos->Integer = order.GetParamData(timeStamp, paramList);
                }
            }
        }

        GetCodeToken();
    }
}

auto ExecHbGetTacOrder(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetTacOrderData(false);
    return IntegerTypePtr;
}

auto ExecHbGetLastTacOrder(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetTacOrderData(true);
    return IntegerTypePtr;
}

auto ExecHbSetOrderMode(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    // Original behaviour: the argument is ignored; the mode is always reset to pilot orders.
    int wasUnitOrder = IsUnitOrder != 0;
    IsUnitOrder = 0;
    Tos->Integer = wasUnitOrder;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbWait(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    float seconds = Tos->Real;
    Pop();
    GetCodeToken();
    ExecExpression();
    int clearLastTarget = Tos->Integer == 1;
    int32_t result = 0;

    if (IsUnitOrder == 0)
    {
        // The original rounds with the 1.5 * 2^52 addition trick: to nearest, ties to even.
        result = CurWarrior->OrderWait(0, 1, static_cast<int32_t>(std::nearbyint(seconds)), clearLastTarget);
    }
    else
    {
        Fatal(0, " Team orderwait needs support ");
    }

    Tos->Integer = result;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetAttackRadius(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    float radius = Tos->Real;
    Tos->Real = CurWarrior->AttackRadius;
    CurWarrior->AttackRadius = radius;
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbMoveToPoint(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    float* location = reinterpret_cast<float*>(NextReference());
    Pop();
    GetCodeToken();
    ExecExpression();
    MCVector3D goal;
    goal.X = location[0];
    goal.Y = location[1];
    goal.Z = location[2];
    uint32_t params = Tos->Integer == 1 ? 1 : 0;
    int32_t result;

    if (IsUnitOrder == 0)
    {
        result = CurWarrior->OrderMoveToPoint(0, 1, 1, goal, -1, params);
    }
    else
    {
        result = CurGroup->OrderMoveToPoint(1, 1, goal, params);
    }

    Tos->Integer = result;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbMoveToObject(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t flag = Tos->Integer;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            if (IsUnitOrder == 0)
            {
                Tos->Integer = CurWarrior->OrderMoveToObject(0, 1, 1, object, -1, flag == 1 ? 1 : 0);
            }
            else
            {
                Tos->Integer = CurGroup->OrderMoveToObject(1, 1, object, 1);
            }

            GetCodeToken();
            return IntegerTypePtr;
        }
    }

    Tos->Integer = 1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbMoveToContact(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t result = -1;

    if (CurContact)
    {
        if (IsUnitOrder != 0)
        {
            result = CurGroup->OrderMoveToObject(1, 1, CurContact, 1);
        }
        else
        {
            result = CurWarrior->OrderMoveToObject(0, 1, 1, CurContact, -1, Tos->Integer == 1 ? 1 : 0);
        }
    }

    Tos->Integer = result;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderPowerDown(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(0);

    if (IsUnitOrder != 0)
    {
        Tos->Integer = CurGroup->OrderPowerDown(TacOrderOrigin);
    }
    else
    {
        Tos->Integer = CurWarrior->OrderPowerDown(0, TacOrderOrigin);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderPowerUp(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(0);

    if (IsUnitOrder != 0)
    {
        Tos->Integer = CurGroup->OrderPowerUp(TacOrderOrigin);
    }
    else
    {
        Tos->Integer = CurWarrior->OrderPowerUp(0, TacOrderOrigin);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderAttackObject(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    uint32_t partId = static_cast<uint32_t>(Tos->Integer);
    Pop();
    int32_t attackType = NextInteger();
    int32_t attackMethod = NextInteger();
    int32_t attackRange = NextInteger();
    GetCodeToken();
    ExecExpression();
    uint32_t params = Tos->Integer != 0 ? 0x10 : 0;

    if (partId != 0 && partId < 0x200)
    {
        Tos->Integer = 1;
        GetCodeToken();
        return IntegerTypePtr;
    }

    // Unlike the other routines, -1 is looked up as a part id rather than meaning the current object.
    MCGameObject* target = nullptr;

    if (partId != 0)
    {
        target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(static_cast<int32_t>(partId)));
    }

    if (IsUnitOrder != 0)
    {
        Tos->Integer = CurGroup->OrderAttackObject(1, target, attackType, attackMethod, attackRange, -1, params);
    }
    else
    {
        Tos->Integer = CurWarrior->OrderAttackObject(0, 1, target, attackType, attackMethod, attackRange, -1, params);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderAttackContact(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t attackType = Tos->Integer;
    Pop();
    int32_t attackMethod = NextInteger();
    int32_t attackRange = NextInteger();
    GetCodeToken();
    ExecExpression();
    uint32_t params = Tos->Integer != 0 ? 0x10 : 0;
    int32_t result = -2;

    if (CurContact)
    {
        result = CurWarrior->OrderAttackObject(0, 1, CurContact, attackType, attackMethod, attackRange, -1, params);
    }

    Tos->Integer = result;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbOrderTest(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlaySmacker(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectChangeSides(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t alignment = NextInteger();

    if (IsGroupId(partId))
    {
        Fatal(0, " Cannot ABL:ObjectChangeSides for Mover Units ");
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && object->GetObjectType())
    {
        static_cast<MCGameObject*>(object)->SetAlignment(alignment);
    }

    GetCodeToken();
}

namespace
{
    /// <summary>The distance in world units from <paramref name="object"/> to (x, y), ignoring height.</summary>
    auto FlatDistance(MCGameObject* object, float x, float y) -> double
    {
        MCVector3D objectPosition = object->GetPosition();
        const float dx = x - objectPosition.X;
        const float dy = y - objectPosition.Y;
        return std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy + 0.0);
    }

    /// <summary>
    /// distancetoobject / distancetoposition: meters from (x, y) to an object, or to the nearest existing, awake
    /// mover of a group; <paramref name="result"/> keeps its value when there is none.
    /// </summary>
    auto DistanceFromId(int32_t partId, float x, float y, float& result) -> void
    {
        if (!IsGroupId(partId))
        {
            MCGameObject* object = FindObject(partId);

            if (object)
            {
                result = static_cast<float>(FlatDistance(object, x, y) * MetersPerWorldUnit);
            }

            return;
        }

        int32_t numMovers = GetGroupMovers(partId);
        float closest = 3.4e38f;

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (MoverList[i]->GetExistsAndAwake())
            {
                const auto distance = static_cast<float>(FlatDistance(MoverList[i], x, y));

                if (distance < closest)
                {
                    closest = distance;
                }
            }
        }

        if (static_cast<double>(closest) < 3.4e38)
        {
            result = MetersPerWorldUnit * closest;
        }
    }

    /// <summary>Takes <paramref name="object"/> out of the object list (it gets destroyed with it).</summary>
    auto RemoveFromObjectList(MCBaseObject* object) -> void
    {
        for (MCObjectQueueNode* node = ObjectList->Head; node; node = node->Next)
        {
            if (node->Remove(object))
            {
                break;
            }
        }
    }

    /// <summary>The pilot a script names by index: -1 is the current one, otherwise 1..numWarriors.</summary>
    /// <returns>Null for an index out of range.</returns>
    auto FindWarrior(int32_t warriorIndex) -> MCMechWarrior*
    {
        if (warriorIndex == -1)
        {
            return CurWarrior;
        }

        if (warriorIndex < 1 || static_cast<uint32_t>(warriorIndex) > Scenario->NumWarriors)
        {
            return nullptr;
        }

        return Scenario->Warriors[warriorIndex];
    }
}

auto ExecHbDistanceToObject(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t targetId = Tos->Integer;
    Tos->Real = -1.0f;
    MCGameObject* target = FindObject(targetId);

    if (target)
    {
        MCVector3D targetPosition = target->GetPosition();
        DistanceFromId(partId, targetPosition.X, targetPosition.Y, Tos->Real);
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbDistanceToPosition(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    float* position = reinterpret_cast<float*>(NextReference());
    Pop();
    PushReal(-1.0f);
    DistanceFromId(partId, position[0], position[1], Tos->Real);
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbObjectSuicide(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();

    if (IsGroupId(partId))
    {
        int32_t numMovers = GetGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            RemoveFromObjectList(MoverList[i]);
        }
    }
    else
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

        if (object)
        {
            RemoveFromObjectList(object);
        }
    }

    GetCodeToken();
}

auto ExecHbObjectCreate(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    for (int32_t i = 0; i < CurrentCreatorPart; i++)
    {
        if (CreatedPartRoster[i].PartId == partId)
        {
            if (CreatedPartRoster[i].Created == 0)
            {
                Scenario->CreateScenarioObject(partId);
                InnerSphereTeam->ScanBattlefield();

                if (AlliedTeam)
                {
                    AlliedTeam->ScanBattlefield();
                }

                Tos->Integer = partId;
            }
            break;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectExists(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    if (IsGroupId(partId))
    {
        if (GetGroupMovers(partId) > 0)
        {
            Tos->Integer = 1;
        }
    }
    else if (FindObject(partId))
    {
        Tos->Integer = 1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = -1;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            Tos->Integer = static_cast<uint8_t>(object->Status);
        }

        GetCodeToken();
        return IntegerTypePtr;
    }

    // A group is 1 (gone) unless one of its movers is neither disabled nor destroyed and has a pilot who hasn't
    // withdrawn.
    int32_t numMovers = GetGroupMovers(partId);

    for (int32_t i = 0; i < numMovers; i++)
    {
        uint8_t status = static_cast<uint8_t>(MoverList[i]->Status);

        if (status != 2 && status != 1)
        {
            MCMechWarrior* pilot = MoverList[i]->GetPilot();

            if (pilot && pilot->Status != 2)
            {
                Tos->Integer = 0;
                GetCodeToken();
                return IntegerTypePtr;
            }
        }
    }

    Tos->Integer = 1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectStatusCount(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    int32_t* counts = reinterpret_cast<int32_t*>(NextReference());
    Pop();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            counts[static_cast<uint8_t>(object->Status)]++;
        }
    }
    else if (partId < 0x21)
    {
        CommanderTable[0]->GetGroup(partId - 1)->StatusCount(counts);
    }
    else if (partId >= 0x149 && partId < 0x169)
    {
        CommanderTable[2]->GetGroup(partId - 0x149)->StatusCount(counts);
    }
    else if (partId >= 0xa5 && partId < 0xc5)
    {
        CommanderTable[1]->GetGroup(partId - 0xa5)->StatusCount(counts);
    }
    else if (partId == 500)
    {
        InnerSphereTeam->StatusCount(counts);
    }
    else if (partId == 0x1f6)
    {
        if (AlliedTeam)
        {
            AlliedTeam->StatusCount(counts);
        }
    }
    else if (partId == 0x1f5)
    {
        ClanTeam->StatusCount(counts);
    }

    GetCodeToken();
    return nullptr;
}

auto ExecHbObjectVisible(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t lookerId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    MCGameObject* target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(Tos->Integer));
    Tos->Integer = 0;

    if (target)
    {
        if (!IsGroupId(lookerId))
        {
            MCGameObject* looker = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(lookerId));

            if (looker)
            {
                Tos->Integer = looker->LineOfSight(target);
            }
        }
        else
        {
            for (MCBaseObject* looker = ObjectList->FindObjectInGroup(nullptr, lookerId); looker;
                 looker = ObjectList->FindObjectInGroup(looker, lookerId))
            {
                if (static_cast<MCGameObject*>(looker)->LineOfSight(target))
                {
                    Tos->Integer = 1;
                    break;
                }
            }
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectSide(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && object->GetObjectType())
    {
        Tos->Integer = static_cast<MCGameObject*>(object)->GetAlignment();
    }
    else
    {
        Tos->Integer = 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectCommander(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = -1;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            Tos->Integer = object->GetCommanderId();
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjectClass(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);
    Tos->Integer = object ? static_cast<int32_t>(object->ObjectClass) : -1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbInArea(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    float* position = reinterpret_cast<float*>(NextReference());
    Pop();
    float radius = NextReal();
    GetCodeToken();
    ExecExpression();
    int32_t numRequired = Tos->Integer;
    MCVector3D center;
    center.X = position[0];
    center.Y = position[1];
    center.Z = position[2];

    if (!IsGroupId(partId))
    {
        // Original behaviour: with a count of 0 a single object is always in the area.
        Tos->Integer = 1;

        if (numRequired != 0)
        {
            Tos->Integer = 0;
            MCGameObject* object = FindObject(partId);

            if (object && object->GetExists() && object->GetAwake() && !object->IsDisabled() &&
                !object->IsDestroyed() && object->DistanceFrom(center) <= radius)
            {
                Tos->Integer = 1;
            }
        }

        GetCodeToken();
        return BooleanTypePtr;
    }

    int32_t numMovers = GetGroupMovers(partId);

    if (numRequired == -1)
    {
        // All of the group's working movers: false if one that exists and is awake is outside, or there are none.
        Tos->Integer = 1;
        int32_t numWorking = 0;

        for (int32_t i = 0; i < numMovers; i++)
        {
            MCMover* mover = MoverList[i];

            if (!mover->IsDisabled())
            {
                numWorking++;

                if (mover->GetExists() && mover->GetAwake() && mover->DistanceFrom(center) > radius)
                {
                    Tos->Integer = 0;
                    break;
                }
            }
        }

        if (numWorking == 0)
        {
            Tos->Integer = 0;
        }

        GetCodeToken();
        return BooleanTypePtr;
    }

    // At least numRequired movers that exist, are awake and work.
    Tos->Integer = 0;
    int32_t numInside = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        MCMover* mover = MoverList[i];

        if (mover->GetExists() && mover->GetAwake() && !mover->IsDisabled() && !mover->IsDestroyed() &&
            mover->DistanceFrom(center) <= radius)
        {
            if (++numInside == numRequired)
            {
                Tos->Integer = 1;
                break;
            }
        }
    }

    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetTimer(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int16_t timerId = static_cast<int16_t>(Tos->Integer);
    Pop();
    GetCodeToken();
    ExecExpression();

    if (timerId < 7 || timerId > 14)
    {
        timerId = 0;
    }
    else
    {
        // Original behaviour: the time is read as a real even when the script passed an integer.
        Application->AddTimer(Application, timerId, static_cast<int32_t>(static_cast<double>(Tos->Real) * 1000.0),
                              0x1406, 0, 0);
    }

    Tos->Integer = timerId;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbChkTimer(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCGuiTimer* timer = Application->TimerManager->GetTimer(Application, static_cast<int16_t>(Tos->Integer));
    uint32_t remaining = 0;

    if (timer)
    {
        remaining = timer->Interval + timer->LastTime - MCPort::Milliseconds();
    }

    Tos->Real = static_cast<float>(static_cast<double>(remaining) * 0.001);
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbEndTimer(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int16_t timerId = static_cast<int16_t>(Tos->Integer);
    Pop();

    if (timerId > 6 && timerId < 15)
    {
        Application->RemoveTimer(Application, timerId);
    }

    GetCodeToken();
}

auto ExecHbSetObjectiveTimer(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t objectiveNumber = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = Scenario->SetObjectiveTimer(objectiveNumber, Tos->Real * 1000.0f);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveTimer(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Real = Scenario->CheckObjectiveTimer(Tos->Integer);
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetObjectiveStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t objectiveNumber = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = Scenario->SetObjectiveStatus(objectiveNumber, static_cast<uint32_t>(Tos->Integer));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = static_cast<int32_t>(Scenario->CheckObjectiveStatus(Tos->Integer));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetObjectiveType(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t objectiveNumber = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = Scenario->SetObjectiveType(objectiveNumber, static_cast<uint32_t>(Tos->Integer));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveType(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = static_cast<int32_t>(Scenario->CheckObjectiveType(Tos->Integer));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayDigitalMusic(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();

    if (SoundSystem)
    {
        SoundSystem->PlayAblDigitalMusic(Tos->Integer);
    }

    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbStopMusic(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();

    if (SoundSystem)
    {
        SoundSystem->StopAblMusic();
    }

    // Original behaviour: nothing was pushed, so this overwrites whatever is on top of the stack.
    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlaySoundEffect(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();

    if (SoundSystem)
    {
        SoundSystem->PlayAblsfx(Tos->Integer);
    }

    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayVideo(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();

    if (SoundSystem)
    {
        SoundSystem->PlayAblVideo(Tos->Integer);
    }

    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetRadio(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t warriorIndex = Tos->Integer;
    Pop();
    int32_t enable = NextInteger();
    MCMechWarrior* warrior = FindWarrior(warriorIndex);

    if (warrior && warrior->Radio)
    {
        warrior->Radio->Enabled = (enable == 1) ? 1 : 0;
    }

    GetCodeToken();
}

auto ExecHbPlaySpeech(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t warriorIndex = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    MCMechWarrior* warrior = FindWarrior(warriorIndex);

    if (warrior)
    {
        warrior->RadioMessage(Tos->Integer, 1);
    }

    Tos->Integer = 0;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayBetty(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    uint32_t bettyId = static_cast<uint32_t>(Tos->Integer);
    Pop();
    PushInteger(SoundSystem->PlayBettySample(bettyId));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetObjActive(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int active = Tos->Integer == 1 ? 1 : 0;
    int32_t numChanged = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object && object->GetAwake() != active)
        {
            object->SetAwake(active);
            TheInterface->ActivateMech(object->PartId);
            numChanged = 1;
        }
    }
    else
    {
        // Original behaviour: the walk stops at the first member already in the wanted state.
        MCBaseObject* object = ObjectList->FindObjectInGroup(nullptr, partId);

        while (object && static_cast<MCGameObject*>(object)->GetAwake() != active)
        {
            object->SetAwake(active);
            TheInterface->ActivateMech(object->PartId);
            numChanged++;
            object = ObjectList->FindObjectInGroup(object, partId);
        }
    }

    Tos->Integer = numChanged;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjWithdraw(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    // Original behaviour (OB-043): two items are pushed for the one result, so every call leaves one behind.
    PushInteger(0);
    PushInteger(0);
    MCVector3D nowhere;
    nowhere.X = 0.0f;
    nowhere.Y = 0.0f;
    nowhere.Z = 0.0f;

    if (IsUnitOrder == 0)
    {
        if (CurWarrior)
        {
            CurWarrior->OrderWithdraw(0, 1, nowhere);
        }
        else
        {
            Tos->Integer = -2;
        }
    }
    else if (CurGroup)
    {
        CurGroup->OrderWithdraw(1, nowhere);
    }
    else
    {
        Tos->Integer = -1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjInWithdraw(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 1;

    if (!IsGroupId(partId))
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

        if (object && object->GetObjectType() && !static_cast<MCGameObject*>(object)->IsWithdrawing())
        {
            Tos->Integer = 0;
        }
    }
    else
    {
        for (MCBaseObject* object = ObjectList->FindObjectInGroup(nullptr, partId); object && Tos->Integer == 1;
             object = ObjectList->FindObjectInGroup(object, partId))
        {
            if (!static_cast<MCGameObject*>(object)->IsWithdrawing())
            {
                Tos->Integer = 0;
            }
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbObjTypeId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = -1;
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && object->GetObjectType())
    {
        Tos->Integer = object->GetObjectType()->ObjTypeNum;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbTerrainObjectId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t blockNumber = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = (blockNumber * 400 + Tos->Integer) * 8 + 0x1000;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbVehicleId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Pop();
    GetCodeToken();
    ExecExpression();
    Tos->Integer = -1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetWeaponAmmo(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    // The weapon index goes through a float on its way to the call.
    float weaponIndex = static_cast<float>(Tos->Integer);
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        Tos->Integer = static_cast<MCMover*>(object)->GetWeaponShots(static_cast<int32_t>(weaponIndex));
    }
    else
    {
        Tos->Integer = -1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetSensors(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && IsMover(object) && static_cast<MCMover*>(object)->SensorSystem)
    {
        Tos->Integer = static_cast<MCMover*>(object)->SensorSystem->Enabled();
    }
    else
    {
        Tos->Integer = -1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetBRValue(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCGameObject* object = FindObject(Tos->Integer);
    Tos->Integer = object ? object->GetCurCV() : -1;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetBRValue(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t newCV = NextInteger();
    MCGameObject* object = FindObject(partId);

    if (object)
    {
        object->SetCurCV(newCV);
    }

    // Original behaviour: nothing is left on the stack for the integer result.
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetArmorPts(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->NumArmorLocations; i++)
        {
            total = static_cast<int32_t>(static_cast<float>(total) + mover->Armor[i].CurArmor);
        }

        Tos->Integer = total;
    }
    else
    {
        Tos->Integer = 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMaxArmor(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->NumArmorLocations; i++)
        {
            total += mover->Armor[i].MaxArmor;
        }

        Tos->Integer = total;
    }
    else
    {
        Tos->Integer = 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetPilotId(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && IsMover(object))
    {
        Tos->Integer = static_cast<MCGameObject*>(object)->GetPilot()->Index;
    }
    else
    {
        Tos->Integer = -1;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetPilotWounds(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object && IsMover(object))
    {
        Tos->Real = static_cast<MCGameObject*>(object)->GetPilot()->Wounds;
    }
    else
    {
        Tos->Integer = 0;
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetPilotWounds(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t wounds = NextInteger();

    if (wounds > 6)
    {
        wounds = 6;
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        static_cast<MCGameObject*>(object)->GetPilot()->Wounds = static_cast<float>(wounds);
    }

    // Original behaviour: nothing is left on the stack for the real result.
    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbGetObjActive(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    if (IsGroupId(partId))
    {
        int32_t numAwake = 0;

        for (MCBaseObject* object = ObjectList->FindObjectInGroup(nullptr, partId); object && Tos->Integer == 0;
             object = ObjectList->FindObjectInGroup(object, partId))
        {
            if (static_cast<MCGameObject*>(object)->GetAwake())
            {
                numAwake++;
            }
        }

        Tos->Integer = numAwake;
    }
    else
    {
        MCGameObject* object = FindObject(partId);

        if (object && object->GetAwake())
        {
            Tos->Integer = 1;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>Whether getobjectdamage and its kin handle <paramref name="object"/>: a typed building, terrain
    /// object or misc terrain object.</summary>
    auto IsDamageableScenery(MCBaseObject* object) -> bool
    {
        if (!object || !object->GetObjectType())
        {
            return false;
        }

        return static_cast<MCGameObject*>(object)->IsBuilding() || object->ObjectClass == TERRAINOBJECT ||
               object->ObjectClass == MISCTERRAINOBJECT;
    }

    /// <summary>
    /// The damage that destroys <paramref name="object"/>, from its type: the building's, turret's or terrain
    /// object's dmgLevel, or the misc terrain object's by kind (5 bridge, 6 forest, 7 wall, 8 medium wall, 9 light
    /// wall).
    /// </summary>
    /// <returns>False for any other class or kind.</returns>
    auto GetDamageLevel(MCGameObject* object, uint32_t& damageLevel) -> bool
    {
        MCObjectType* type = object->GetObjectType();

        switch (object->ObjectClass)
        {
            case BUILDING:
            {
                damageLevel = static_cast<MCBuildingType*>(type)->DmgLevel;
                return true;
            }
            case TURRET:
            {
                damageLevel = static_cast<MCTurretType*>(type)->DmgLevel;
                return true;
            }
            case TERRAINOBJECT:
            {
                damageLevel = static_cast<MCTerrainObjectType*>(type)->DmgLevel;
                return true;
            }
            case TREEBUILDING:
            {
                damageLevel = static_cast<MCTreeBuildingType*>(type)->DmgLevel;
                return true;
            }
            case MISCTERRAINOBJECT:
            {
                MCMiscTerrainObjectType* miscType = static_cast<MCMiscTerrainObjectType*>(type);

                switch (static_cast<MCMiscTerrainObject*>(object)->TerrainObjectKind)
                {
                    case 5:
                    {
                        damageLevel = miscType->BridgeDmgLevel;
                        return true;
                    }
                    case 6:
                    {
                        damageLevel = miscType->ForestDmgLevel;
                        return true;
                    }
                    case 7:
                    {
                        damageLevel = miscType->WallDmgLevel;
                        return true;
                    }
                    case 8:
                    {
                        damageLevel = miscType->MediumWallDmgLevel;
                        return true;
                    }
                    case 9:
                    {
                        damageLevel = miscType->LightWallDmgLevel;
                        return true;
                    }
                    default:
                        return false;
                }
            }

            default:
                return false;
        }
    }

    /// <summary>Applies <paramref name="shotInfo"/> to <paramref name="target"/> as the game's weapon hits do: in
    /// multiplayer only on the server, which sends it on.</summary>
    /// <returns>False on a multiplayer client (nothing applied).</returns>
    auto ApplyShot(MCGameObject* target, MCWeaponShotInfo* shotInfo) -> bool
    {
        if (MPlayer == nullptr)
        {
            target->HandleWeaponHit(shotInfo, 0);
            return true;
        }

        if (MPlayer->IsServer == 0)
        {
            return false;
        }

        target->HandleWeaponHit(shotInfo, 1);
        return true;
    }
}

auto ExecHbGetObjDamage(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* baseObject = ObjectList->FindObjectFromPart(Tos->Integer);

    if (!IsDamageableScenery(baseObject))
    {
        Tos->Integer = 0;
        GetCodeToken();
        return IntegerTypePtr;
    }

    MCGameObject* object = static_cast<MCGameObject*>(baseObject);
    double damage = object->GetDamage();
    uint32_t damageLevel;

    // Original behaviour: a misc terrain object getDamageLevel doesn't list gives its raw damage times 100.
    if (GetDamageLevel(object, damageLevel))
    {
        damage = damage / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    Tos->Integer = static_cast<int32_t>(std::floor(damage * 100.0));
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetObjDmgPts(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (IsDamageableScenery(object))
    {
        Tos->Integer = static_cast<int32_t>(static_cast<MCGameObject*>(object)->GetDamage());
    }
    else
    {
        Tos->Integer = 0;
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetMaxDmg(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);
    uint32_t damageLevel = 0;

    if (IsDamageableScenery(object))
    {
        GetDamageLevel(static_cast<MCGameObject*>(object), damageLevel);
    }

    Tos->Integer = static_cast<int32_t>(damageLevel);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetObjDamage(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t percent = NextInteger();

    if (percent > 100)
    {
        percent = 100;
    }

    MCBaseObject* baseObject = ObjectList->FindObjectFromPart(partId);

    if (baseObject && baseObject->GetObjectType() && percent > 0)
    {
        // Raises the damage to percent of the damage level (it never lowers it).
        MCGameObject* object = static_cast<MCGameObject*>(baseObject);
        uint32_t damageLevel;

        if (GetDamageLevel(object, damageLevel))
        {
            float currentDamage = object->GetDamage();
            float extraDamage = static_cast<float>(static_cast<double>(percent) * 0.01 *
                                                       static_cast<float>(static_cast<int32_t>(damageLevel)) -
                                                   currentDamage);

            if (extraDamage > 0.0f)
            {
                MCWeaponShotInfo shotInfo;
                shotInfo.Init(nullptr, -1, extraDamage, 0, 0.0f);
                ApplyShot(object, &shotInfo);
            }
        }
    }

    GetCodeToken();
}

auto ExecHbDamageObject(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t attackerId = NextInteger();
    int32_t weaponMasterId = NextInteger();
    float damage = NextReal();
    int32_t hitLocation = NextInteger();
    GetCodeToken();
    ExecExpression();
    Pop();
    GetCodeToken();
    ExecExpression();
    float entryAngle = Tos->Real;

    MCGameObject* attacker = FindObject(attackerId);

    if (!attacker)
    {
        Tos->Integer = -1;
        GetCodeToken();
        return IntegerTypePtr;
    }

    MCWeaponShotInfo shotInfo;

    if (!IsGroupId(partId))
    {
        MCGameObject* target = FindObject(partId);

        if (!target)
        {
            Tos->Integer = -2;
            GetCodeToken();
            return IntegerTypePtr;
        }

        shotInfo.Init(attacker, weaponMasterId, damage, hitLocation, entryAngle);
        ApplyShot(target, &shotInfo);
        Tos->Integer = 1;
        GetCodeToken();
        return IntegerTypePtr;
    }

    int32_t numMovers = GetGroupMovers(partId);
    shotInfo.Init(attacker, weaponMasterId, damage, hitLocation, entryAngle);

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (!ApplyShot(MoverList[i], &shotInfo))
        {
            break;
        }
    }

    Tos->Integer = numMovers;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetGlobalValue(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t index = Tos->Integer;
    Tos->Integer = 0;

    if (index > -1 && index < 50)
    {
        Tos->Real = GlobalMissionValues[index];
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetGlobalValue(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t index = Tos->Integer;
    Pop();
    // Original behaviour: the value is stored as a real even when the script passed an integer.
    float value = NextReal();

    if (index > -1 && index < 50)
    {
        GlobalMissionValues[index] = value;
    }

    GetCodeToken();
}

auto ExecHbSetObjectivePos(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t objectiveNumber = Tos->Integer;
    Pop();
    float x = NextReal();
    float y = NextReal();
    float z = NextReal();
    Scenario->SetObjectivePos(objectiveNumber, x, y, z);
    GetCodeToken();
}

auto ExecHbSetTonnage(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float tonnage = NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            object->SetTonnage(tonnage);
        }
    }

    GetCodeToken();
}

auto ExecHbSetSensorRange(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    float range = Tos->Real;
    Tos->Integer = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            switch (object->ObjectClass)
            {
                case BATTLEMECH:
                case GROUNDVEHICLE:
                case ELEMENTAL:
                {
                    if (static_cast<MCMover*>(object)->SensorSystem)
                    {
                        static_cast<MCMover*>(object)->SensorSystem->SetRange(range);
                    }
                    break;
                }
                case ARTILLERY:
                {
                    static_cast<MCArtillery*>(object)->SensorRange = range;
                    static_cast<MCArtillery*>(object)->SensorSystem->SetRange(range);
                    break;
                }
                case BUILDING:
                {
                    if (static_cast<MCBuilding*>(object)->SensorSystem)
                    {
                        static_cast<MCBuilding*>(object)->SensorSystem->SetRange(range);
                    }
                    else
                    {
                        Tos->Integer = -1;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    GetCodeToken();
}

auto ExecHbSetExplDmg(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float damage = NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            object->SetExplDmg(damage);
        }
    }

    GetCodeToken();
}

auto ExecHbSetExplRad(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float radius = NextReal();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            object->SetExplRad(radius);
        }
    }

    GetCodeToken();
}

auto ExecHbSetSalvage(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t itemId = NextInteger();
    GetCodeToken();
    ExecExpression();
    int32_t numItems = Tos->Integer;
    int added = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            MCSalvageItem* last = object->GetSalvage();
            MCSalvageItem* item = new (std::nothrow) MCSalvageItem;

            if (item)
            {
                item->Next = nullptr;
                added = 1;
                item->ItemId = static_cast<uint8_t>(itemId);
                item->NumItems = static_cast<uint8_t>(numItems);

                while (last && last->Next)
                {
                    last = last->Next;
                }

                if (object->GetSalvage() == nullptr)
                {
                    object->SetSalvage(item);
                }
                else
                {
                    last->Next = item;
                }
            }
        }
    }

    Tos->Integer = added;
    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetSalvageStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t status = Tos->Integer;
    int result = 0;

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object && TacticalMap() && (IsMover(object) || object->IsBuilding()))
        {
            if (status == 1)
            {
                result = TacticalMap()->AddSalvage(object);
            }
            else
            {
                result = TacticalMap()->RemoveSalvage(object, 1);
            }
        }
    }

    Tos->Integer = result;
    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbSetAnimation(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    uint32_t state = static_cast<uint32_t>(NextInteger());
    int32_t subState = NextInteger();

    if (!IsGroupId(partId))
    {
        MCGameObject* object = FindObject(partId);

        if (object)
        {
            if (object->ObjectClass == BUILDING)
            {
                auto* buildingAppearance =
                    static_cast<MCVfxBuildingAppearance*>(static_cast<MCBuilding*>(object)->Appearance);

                if (state >= buildingAppearance->BuildType->AnimStates.size())
                {
                    buildingAppearance->AnimState = -1;
                }
                else
                {
                    buildingAppearance->AnimState = static_cast<int32_t>(state);
                }

                buildingAppearance->CurrentFrame = 0;
            }
            else if (object->ObjectClass == TREEBUILDING)
            {
                static_cast<MCVfxAppearance*>(static_cast<MCTreeBuilding*>(object)->Appearance)
                    ->SetTypeId(static_cast<MCActorState>(state), static_cast<uint8_t>(subState));
            }
        }
    }

    GetCodeToken();
}

auto ExecHbPlayWave(MCAblSymbol* routineIdPtr) -> void
{
    // Original behaviour (OB-045): only the first of the two arguments is read; the code pointer is left on the
    // comma before the second.
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    Pop();
    GetCodeToken();
}

auto ExecHbSetRevealed(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t teamId = Tos->Integer;
    Pop();
    // Original behaviour: the radius is read as a real even when the script passed an integer.
    float radius = NextReal();
    GetCodeToken();
    float* position = reinterpret_cast<float*>(NextReference());
    Pop();
    MCVector3D looker;
    looker.X = position[0];
    looker.Y = position[1];
    looker.Z = 0.0f;
    MCVector3D lookVector;
    lookVector.X = 0.0f;
    lookVector.Y = 0.0f;
    lookVector.Z = 0.0f;
    Terrain()->MarkRadiusSeen(looker, lookVector, 360.0f, radius, static_cast<uint8_t>(teamId));

    if (teamId == 1)
    {
        InnerSphereTeam->ScanBattlefield();
    }
    else
    {
        ClanTeam->ScanBattlefield();
    }

    if (AlliedTeam)
    {
        AlliedTeam->ScanBattlefield();
    }

    GetCodeToken();
}

auto ExecHbGetSalvage(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t listSize = NextInteger();
    GetCodeToken();
    int32_t* itemIds = reinterpret_cast<int32_t*>(NextReference());
    Pop();
    GetCodeToken();
    int32_t* itemCounts = reinterpret_cast<int32_t*>(NextReference());
    Pop();

    for (int32_t i = 0; i < listSize; i++)
    {
        itemIds[i] = -1;
        itemCounts[i] = -1;
    }

    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object)
    {
        int32_t i = 0;

        for (MCSalvageItem* item = static_cast<MCGameObject*>(object)->GetSalvage(); item && i < listSize;
             item = item->Next, i++)
        {
            itemIds[i] = item->ItemId;
            itemCounts[i] = item->NumItems;
        }
    }

    GetCodeToken();
}

auto ExecHbRefit(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t targetId = Tos->Integer;
    Pop();
    uint32_t params = static_cast<uint32_t>(NextInteger());

    if (CurObject && IsMover(CurObject))
    {
        MCMechWarrior* pilot = CurObject->GetPilot();

        if (pilot)
        {
            MCBaseObject* target = ObjectList->FindObjectFromPart(targetId);

            if (target && target->ObjectClass == BATTLEMECH)
            {
                pilot->OrderRefit(1, static_cast<MCGameObject*>(target), params);
            }
        }
    }

    GetCodeToken();
}

auto ExecHbSetCaptured(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object)
    {
        static_cast<MCGameObject*>(object)->SetCaptured();
    }

    GetCodeToken();
}

auto ExecHbCaptureObject(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t targetId = Tos->Integer;
    Pop();
    uint32_t params = static_cast<uint32_t>(NextInteger());
    // Port fix: the original leaves the target register unset when the current object isn't a mover (and then
    // orders its pilot anyway).
    MCBaseObject* target = nullptr;

    if (CurObject && IsMover(CurObject))
    {
        target = ObjectList->FindObjectFromPart(targetId);
    }

    if (target)
    {
        CurObject->GetPilot()->OrderCapture(1, static_cast<MCGameObject*>(target), params);
    }

    GetCodeToken();
}

auto ExecHbSetCaptureable(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    int32_t captureable = NextInteger() == 1 ? 1 : 0;
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object)
    {
        if (MPlayer)
        {
            static_cast<MCGameObject*>(object)->ClearCaptured();
        }

        switch (object->ObjectClass)
        {
            case GROUNDVEHICLE:
                static_cast<MCGroundVehicle*>(object)->Captureable = captureable;
                break;
            case BUILDING:
                static_cast<MCBuilding*>(object)->Captureable = captureable;
                break;
            case TREEBUILDING:
                static_cast<MCTreeBuilding*>(object)->Captureable = captureable;
                break;
            case TURRET:
                // Original behaviour (OB-044): a turret's flag goes where tree buildings keep theirs, +0x110,
                // which is the turret's lastFireTime.
                static_cast<MCTurret*>(object)->LastFireTime = std::bit_cast<float>(captureable);
                break;
            default:
                break;
        }
    }

    GetCodeToken();
}

auto ExecHbIsCaptured(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    int32_t numCaptured = 0;

    if (IsGroupId(partId))
    {
        int32_t numMovers = GetGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (MoverList[i]->IsCaptured())
            {
                numCaptured++;
            }
        }
    }
    else
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

        if (object && static_cast<MCGameObject*>(object)->IsCaptured())
        {
            numCaptured = 1;
        }
    }

    Tos->Integer = numCaptured;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsCapturable(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int captureable = 0;
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object)
    {
        captureable = static_cast<MCGameObject*>(object)->IsCaptureable();
    }

    Tos->Integer = captureable != 0 ? 1 : 0;
    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbWasEverCapturable(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t captureable = 0;
    MCBaseObject* object = ObjectList->FindObjectFromPart(Tos->Integer);

    if (object)
    {
        switch (object->ObjectClass)
        {
            case GROUNDVEHICLE:
                captureable = static_cast<MCGroundVehicle*>(object)->Captureable;
                break;
            case BUILDING:
                captureable = static_cast<MCBuilding*>(object)->Captureable;
                break;
            case TREEBUILDING:
                captureable = static_cast<MCTreeBuilding*>(object)->Captureable;
                break;
            case TURRET:
                // Original behaviour (OB-044): reads the turret's lastFireTime bits.
                captureable = std::bit_cast<int32_t>(static_cast<MCTurret*>(object)->LastFireTime);
                break;
            default:
                break;
        }
    }

    Tos->Integer = captureable != 0 ? 1 : 0;
    GetCodeToken();
    return BooleanTypePtr;
}

namespace
{
    /// <summary>Replaces <paramref name="name"/> with string resource <paramref name="stringId"/>.</summary>
    auto SetNameFromResource(std::string& name, uint32_t stringId) -> void
    {
        char buffer[256];
        CLoadString(ThisInstance, stringId, buffer, 0xfe);
        name = buffer;
    }

    /// <summary>What <c>__ftol</c> gives: the value truncated, or 0x80000000 for NaN or out of range.</summary>
    auto X87Ftol(double value) -> int32_t
    {
        if (!(value > -2147483649.0 && value < 2147483648.0))
        {
            return INT32_MIN;
        }

        return static_cast<int32_t>(value);
    }
}

auto ExecHbSetBuildingName(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    uint32_t stringId = static_cast<uint32_t>(NextInteger());
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && static_cast<MCGameObject*>(object)->IsBuilding())
    {
        if (object->ObjectClass == BUILDING)
        {
            SetNameFromResource(static_cast<MCBuilding*>(object)->Name, stringId);
        }

        if (object->ObjectClass == TREEBUILDING)
        {
            SetNameFromResource(static_cast<MCTreeBuilding*>(object)->Name, stringId);
        }

        if (object->ObjectClass == TURRET)
        {
            SetNameFromResource(static_cast<MCTurret*>(object)->Name, stringId);
        }
    }

    GetCodeToken();
}

namespace
{
    /// <summary>callstrike / callstrikeex: an artillery strike on an object, or on a point at ground level.</summary>
    auto CallStrike(int32_t strikeType, int32_t targetId, MCVector3D& position, int forClansOnPoint,
                    int forClansOnTarget, float delay) -> void
    {
        MCGameObject* target = static_cast<MCGameObject*>(ObjectList->FindObjectFromPart(targetId));

        if (!target)
        {
            position.Z = Terrain()->GetTerrainElevation(position);
            TheInterface->CallStrike(strikeType, &position, nullptr, 0, forClansOnPoint, delay);
        }
        else
        {
            TheInterface->CallStrike(strikeType, nullptr, target, 0, forClansOnTarget, delay);
        }
    }
}

auto ExecHbCallStrike(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    GetCodeToken();
    ExecExpression();
    int32_t strikeType = Tos->Integer;
    Pop();
    int32_t targetId = NextInteger();
    MCVector3D position;
    position.X = NextReal();
    position.Y = NextReal();
    position.Z = NextReal();
    int forClans = NextInteger() == 1 ? 1 : 0;
    // Original behaviour: the clan flag only counts for a strike on a point.
    CallStrike(strikeType, targetId, position, forClans, 0, -1.0f);
    GetCodeToken();
}

auto ExecHbCallStrikeEx(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    GetCodeToken();
    ExecExpression();
    int32_t strikeType = Tos->Integer;
    Pop();
    int32_t targetId = NextInteger();
    MCVector3D position;
    position.X = NextReal();
    position.Y = NextReal();
    position.Z = NextReal();
    int forClans = NextInteger() == 1 ? 1 : 0;
    float delay = NextReal();

    if (delay < 0.0f)
    {
        delay = 0.0f;
    }

    CallStrike(strikeType, targetId, position, forClans, forClans, delay);
    GetCodeToken();
}

auto ExecHbLoadElementals(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t carrierId = Tos->Integer;
    Pop();

    if (CurObject && CurObject->ObjectClass == ELEMENTAL)
    {
        MCBaseObject* carrier = ObjectList->FindObjectFromPart(carrierId);

        if (carrier && carrier->ObjectClass == GROUNDVEHICLE &&
            static_cast<MCGroundVehicle*>(carrier)->ElementalCarrier != 0)
        {
            CurObject->GetPilot()->OrderLoadIntoCarrier(1, static_cast<MCGameObject*>(carrier), 0);
        }
    }

    GetCodeToken();
}

auto ExecHbDeployElementals(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    uint32_t params = static_cast<uint32_t>(Tos->Integer);
    Pop();

    if (CurObject && CurObject->ObjectClass == GROUNDVEHICLE &&
        static_cast<MCGroundVehicle*>(CurObject)->ElementalCarrier != 0)
    {
        CurObject->GetPilot()->OrderDeployElementals(1, params);
    }

    GetCodeToken();
}

auto ExecHbAddPrisoner(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t buildingId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t pilotIndex = Tos->Integer;
    int32_t result = -1;
    MCBaseObject* object = ObjectList->FindObjectFromPart(buildingId);

    if (object && static_cast<MCGameObject*>(object)->IsBuilding() && Scenario)
    {
        // Port fix: with no warriors at all the original fills the prison with the pointer -1.
        MCMechWarrior* prisoner = nullptr;

        for (uint32_t i = 1; i <= Scenario->NumWarriors; i++)
        {
            MCMechWarrior* warrior = Scenario->Warriors[i];

            if (warrior && warrior->Index == pilotIndex)
            {
                prisoner = warrior;
                break;
            }
        }

        if (prisoner)
        {
            // Original behaviour (OB-046): the prisoner goes into every empty slot, not just the first.
            MCMechWarrior** prisonSlots = nullptr;

            if (object->ObjectClass == BUILDING)
            {
                prisonSlots = static_cast<MCBuilding*>(object)->PrisonSlots;
            }
            else if (object->ObjectClass == TREEBUILDING)
            {
                prisonSlots = static_cast<MCTreeBuilding*>(object)->PrisonSlots;
            }

            if (prisonSlots)
            {
                for (int32_t slot = 0; slot < 4; slot++)
                {
                    if (prisonSlots[slot] == nullptr)
                    {
                        prisonSlots[slot] = prisoner;
                        result = 0;
                    }
                }
            }
        }
    }

    Tos->Integer = result;
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetTrainSpeed(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float speed = NextReal();
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && object->ObjectClass == TRAINCAR)
    {
        MCTrain* train = static_cast<MCTrainCar*>(object)->Train;

        if (std::fabs(speed) > train->MaxSpeed)
        {
            speed = speed > 0.0f ? train->MaxSpeed : -train->MaxSpeed;
        }

        train->DesiredSpeed = speed;
    }

    GetCodeToken();
}

namespace
{
    /// <summary>lockgateopen / lockgateclosed / releasegatelock: sets a gate's two lock flags.</summary>
    auto SetGateLocks(int32_t blownOpen, int32_t lockedClosed) -> void
    {
        GetCodeToken();
        GetCodeToken();
        ExecExpression();
        int32_t partId = Tos->Integer;
        Pop();
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

        if (object && object->ObjectClass == GATE)
        {
            static_cast<MCGate*>(object)->BlownOpen = blownOpen;
            static_cast<MCGate*>(object)->LockedClosed = lockedClosed;
        }

        GetCodeToken();
    }
}

auto ExecHbLockGateOpen(MCAblSymbol* routineIdPtr) -> void
{
    SetGateLocks(1, 0);
}

auto ExecHbLockGateClosed(MCAblSymbol* routineIdPtr) -> void
{
    SetGateLocks(0, 1);
}

auto ExecHbReleaseGateLock(MCAblSymbol* routineIdPtr) -> void
{
    SetGateLocks(0, 0);
}

auto ExecHbIsGateOpen(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Tos->Integer = 0;

    if (!IsGroupId(partId))
    {
        MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

        if (object && object->ObjectClass == GATE)
        {
            Tos->Integer = static_cast<MCGate*>(object)->IsOpen != 0 ? 1 : 0;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>How much of a pilot's worth is left by wounds (0 to 6).</summary>
    constexpr float WoundEffectiveness[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};

    /// <summary>An armor location's share left, scaled into 0.4 .. 1.0.</summary>
    auto ArmorFactor(const MCArmorLocation& location) -> double
    {
        return static_cast<double>(location.CurArmor) / static_cast<double>(location.MaxArmor) * 0.6 + 0.4;
    }

    /// <summary>getunitstatus of a mech: its armor state times its weapon effectiveness (without the pilot).</summary>
    auto MechStatus(MCMover* mech) -> float
    {
        // Armor locations: 0 head, 1 center torso, 2 / 3 arms, 4 / 5 side torsos, 8 rear center torso, 9 / 10 legs.
        MCArmorLocation* armor = mech->Armor.get();
        float centerArmor = armor[1].CurArmor;
        uint8_t centerMax = armor[1].MaxArmor;

        if (centerArmor > armor[8].CurArmor)
        {
            centerArmor = armor[8].CurArmor;
            centerMax = armor[8].MaxArmor;
        }

        double head = ArmorFactor(armor[0]);
        double sides = static_cast<double>(armor[5].CurArmor + armor[4].CurArmor) /
                           static_cast<double>(armor[5].MaxArmor + armor[4].MaxArmor) * 0.25 +
                       0.75;
        float sidesFactor = static_cast<float>(sides);
        int32_t limbMax = armor[10].MaxArmor + armor[9].MaxArmor + armor[3].MaxArmor + armor[2].MaxArmor;
        double limbs =
            static_cast<double>(armor[10].CurArmor + armor[9].CurArmor + armor[3].CurArmor + armor[2].CurArmor) /
                static_cast<double>(limbMax) * 0.25 +
            0.75;
        double center = (static_cast<double>(centerArmor) / static_cast<double>(centerMax) + 1.0) * 0.5;
        return static_cast<float>(center * limbs * sidesFactor * sidesFactor * static_cast<float>(head));
    }

    /// <summary>getunitstatus of a ground vehicle: the product of its five armor locations' factors.</summary>
    auto VehicleStatus(MCMover* vehicle) -> float
    {
        MCArmorLocation* armor = vehicle->Armor.get();
        double turret = 1.0;

        if (armor[4].MaxArmor != 0)
        {
            turret = ArmorFactor(armor[4]);
        }

        return static_cast<float>(turret * ArmorFactor(armor[0]) * ArmorFactor(armor[1]) * ArmorFactor(armor[2]) *
                                  ArmorFactor(armor[3]));
    }

    /// <summary>The share of <paramref name="damageLevel"/> the damage leaves (at least 0).</summary>
    auto HealthLeft(MCGameObject* object, uint32_t damageLevel) -> double
    {
        float maxDamage = static_cast<float>(static_cast<int32_t>(damageLevel));
        float left = maxDamage - object->GetDamage();

        if (left < 0.0f)
        {
            left = 0.0f;
        }

        return static_cast<double>(left) / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    /// <summary>The damage taken as a share of <paramref name="damageLevel"/>, both truncated, capped at 1.</summary>
    auto DamageTaken(MCGameObject* object, int32_t damageLevel) -> double
    {
        int32_t damage = X87Ftol(object->GetDamage());

        if (damage > damageLevel)
        {
            damage = damageLevel;
        }

        return static_cast<double>(damage) / static_cast<double>(damageLevel);
    }
}

auto ExecHbGetUnitStatus(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    MCBaseObject* baseObject = ObjectList->FindObjectFromPart(Tos->Integer);
    Tos->Integer = 0;

    if (baseObject)
    {
        MCGameObject* object = static_cast<MCGameObject*>(baseObject);
        // Port fix: other classes scale an uninitialized local in the original.
        double status = 0.0;

        switch (object->ObjectClass)
        {
            case BATTLEMECH:
            case GROUNDVEHICLE:
            {
                MCMover* mover = static_cast<MCMover*>(object);
                float weaponShare;
                float armorStatus;

                if (object->ObjectClass == BATTLEMECH)
                {
                    weaponShare = mover->WeaponEffectiveness / mover->MaxWeaponEffectiveness;
                    armorStatus = MechStatus(mover);
                }
                else
                {
                    weaponShare = mover->MaxWeaponEffectiveness == 0.0f
                                      ? 1.0f
                                      : mover->WeaponEffectiveness / mover->MaxWeaponEffectiveness;
                    armorStatus = VehicleStatus(mover);
                }

                // Port fix: wounds past 6 index past the table in the original.
                int32_t wounds = std::clamp(X87Ftol(object->GetPilot()->Wounds), 0, 6);
                float pilotShare = WoundEffectiveness[wounds];

                if (object->IsDestroyed() || object->IsDisabled())
                {
                    status = 0.0f * armorStatus * weaponShare;
                }
                else
                {
                    status = pilotShare * armorStatus * weaponShare;
                }
                break;
            }

            case BUILDING:
                status = HealthLeft(object, static_cast<MCBuildingType*>(object->GetObjectType())->DmgLevel);
                break;
            case TREEBUILDING:
                status = HealthLeft(object, static_cast<MCTreeBuildingType*>(object->GetObjectType())->DmgLevel);
                break;
            case MISCTERRAINOBJECT:
            {
                uint32_t damageLevel = 0;
                // Original behaviour: a kind getDamageLevel doesn't list divides 0 by 0.
                GetDamageLevel(object, damageLevel);
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(damageLevel));
                break;
            }

            case TRAINCAR:
                // Original behaviour (OB-047): a train car reports the damage taken, not the health left.
                status = DamageTaken(object, static_cast<MCTrainCarType*>(object->GetObjectType())->Damage);
                break;
            case TURRET:
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(
                                                       static_cast<MCTurretType*>(object->GetObjectType())->DmgLevel));
                break;
            case GATE:
                status = 1.0 - DamageTaken(object, static_cast<int32_t>(
                                                       static_cast<MCGateType*>(object->GetObjectType())->DmgLevel));
                break;
            default:
                break;
        }

        Tos->Real = static_cast<float>(status * 100.0);
    }

    GetCodeToken();
    return RealTypePtr;
}

auto ExecHbRelPosPoint(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    float* point = reinterpret_cast<float*>(NextReference());
    Pop();
    float angle = NextReal();
    float distance = NextReal();
    uint32_t flags = static_cast<uint32_t>(NextInteger());
    GetCodeToken();
    float* result = reinterpret_cast<float*>(NextReference());
    Pop();
    MCVector3D start;
    start.X = point[0];
    start.Y = point[1];
    start.Z = 0.0f;
    MCVector3D position = RelativePositionToPoint(start, angle, distance, flags);
    result[0] = position.X;
    result[1] = position.Y;
    result[2] = position.Z;
    GetCodeToken();
}

auto ExecHbRelPosObject(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float angle = NextReal();
    float distance = NextReal();
    uint32_t flags = static_cast<uint32_t>(NextInteger());
    GetCodeToken();
    float* result = reinterpret_cast<float*>(NextReference());
    Pop();
    MCGameObject* object = FindObject(partId);

    if (object)
    {
        MCVector3D position = object->RelativePosition(angle, distance, flags);
        result[0] = position.X;
        result[1] = position.Y;
        result[2] = position.Z;
    }

    GetCodeToken();
}

namespace
{
    /// <summary>The damage state of the body location an armor location covers (the rear locations, from
    /// numBodyLocations on, cover the torsos from 1 on).</summary>
    auto ArmorLocationState(MCMover* mover, int32_t armorIndex) -> uint8_t
    {
        if (armorIndex < mover->NumBodyLocations)
        {
            return mover->BodyAt(armorIndex).DamageState;
        }

        return mover->BodyAt(armorIndex - mover->NumBodyLocations + 1).DamageState;
    }
}

auto ExecHbRepair(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    float points = NextReal();
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && object->ObjectClass == BATTLEMECH)
    {
        // Fills internal structure first, then armor, location by location, skipping destroyed ones.
        MCMover* mech = static_cast<MCMover*>(object);

        for (int32_t i = 0; i < mech->NumBodyLocations; i++)
        {
            MCBodyLocation& location = mech->BodyAt(i);
            float needed = static_cast<float>(location.MaxInternalStructure) - location.CurInternalStructure;

            if (location.DamageState != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.CurInternalStructure += points;
                    points = 0.0f;
                    break;
                }

                points -= needed;
                location.CurInternalStructure = static_cast<float>(location.MaxInternalStructure);
            }
        }

        for (int32_t i = 0; i < mech->NumArmorLocations; i++)
        {
            MCArmorLocation& location = mech->Armor[i];
            float needed = static_cast<float>(location.MaxArmor) - location.CurArmor;

            if (ArmorLocationState(mech, i) != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.CurArmor += points;
                    break;
                }

                points -= needed;
                location.CurArmor = static_cast<float>(location.MaxArmor);
            }
        }
    }

    GetCodeToken();
}

auto ExecHbGetRepairState(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t partId = Tos->Integer;
    Pop();
    // The percentage of internal structure and armor left, over the locations that aren't destroyed.
    double sum = 0.0;
    int32_t maximum = 0;
    MCBaseObject* object = ObjectList->FindObjectFromPart(partId);

    if (object && IsMover(object))
    {
        MCMover* mover = static_cast<MCMover*>(object);

        for (int32_t i = 0; i < mover->NumBodyLocations; i++)
        {
            if (mover->BodyAt(i).DamageState != 2)
            {
                sum += mover->BodyAt(i).CurInternalStructure;
                maximum += mover->BodyAt(i).MaxInternalStructure;
            }
        }

        for (int32_t i = 0; i < mover->NumArmorLocations; i++)
        {
            if (ArmorLocationState(mover, i) != 2)
            {
                sum += mover->Armor[i].CurArmor;
                maximum += mover->Armor[i].MaxArmor;
            }
        }
    }

    if (maximum != 0)
    {
        PushInteger(X87Ftol(sum * 100.0 / static_cast<double>(maximum)));
    }
    else
    {
        // Original behaviour (OB-111): with no object, or no location left, MCX.EXE divides by zero and __ftol turns
        // the NaN or infinity into 0x80000000.
        PushInteger(INT32_MIN);
    }

    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbIsTeamTargeting(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t teamId = Tos->Integer;
    Pop();
    uint32_t targetId = static_cast<uint32_t>(NextInteger());
    uint32_t exceptId = static_cast<uint32_t>(NextInteger());
    MCTeam* team = nullptr;

    if (teamId == 500)
    {
        team = InnerSphereTeam;
    }
    else if (teamId == 0x1f6)
    {
        team = AlliedTeam;
    }
    else if (teamId == 0x1f5)
    {
        team = ClanTeam;
    }

    int targeting = 0;

    if (team)
    {
        targeting = team->IsTargeting(targetId, exceptId);
    }

    PushInteger(targeting != 0 ? 1 : 0);
    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbGetFixed(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t moverId = Tos->Integer;
    Pop();
    int32_t bayId = NextInteger();
    uint32_t params = static_cast<uint32_t>(NextInteger());

    // -1 ordered, 0 order refused, 1 bay out of points, 2 wrong kind of bay, 3 already this bay's, 4 bay busy,
    // 5 already being fixed, 6 needs nothing, 7 not a mech or vehicle, 8 not a repair bay, 9 other side.
    int32_t result = -1;
    MCBaseObject* bayObject = ObjectList->FindObjectFromPart(bayId);

    if (!bayObject || bayObject->ObjectClass != TREEBUILDING || static_cast<MCTreeBuilding*>(bayObject)->CanRefit == 0)
    {
        result = 8;
    }
    else
    {
        MCTreeBuilding* bay = static_cast<MCTreeBuilding*>(bayObject);

        if (bay->GetRefitPoints() > 0.0f)
        {
            MCBaseObject* moverObject = ObjectList->FindObjectFromPart(moverId);

            if (!moverObject || (moverObject->ObjectClass != BATTLEMECH && moverObject->ObjectClass != GROUNDVEHICLE))
            {
                result = 7;
            }
            else
            {
                MCMover* mover = static_cast<MCMover*>(moverObject);

                if (bay->GetAlignment() != mover->GetAlignment())
                {
                    result = 9;
                }
                else if (bay->RefitBuddy)
                {
                    result = (bay->RefitBuddy != mover) ? 4 : 3;
                }
                else
                {
                    // A mech bay fixes mechs only, a vehicle bay vehicles only.
                    bool rightBay = (mover->ObjectClass == BATTLEMECH) == (bay->MechBay != 0);

                    if (!rightBay)
                    {
                        result = 2;
                    }
                    else if (mover->RefitBuddy)
                    {
                        result = 5;
                    }
                    else if (mover->NeedsRefit(0) == 0)
                    {
                        result = 6;
                    }
                    else
                    {
                        result = mover->GetPilot()->OrderGetFixed(1, bay, params) != 0 ? -1 : 0;
                    }
                }
            }
        }
        else
        {
            result = 1;
        }
    }

    PushInteger(result);
    GetCodeToken();
    return IntegerTypePtr;
}

auto DebugMissionScriptMessages() -> void
{
    // Port fix (OB-048): the lines are appended only while they fit; the original's 1000 lines overrun
    // ChunkDebugMsg.
    constexpr size_t bufferSize = 0x1400;
    char line[512];
    ChunkDebugMsg[0] = '\0';
    std::snprintf(line, sizeof(line), "\n%d Mission Script Messages\n\n", NumMissionScriptMessages);
    std::strncat(ChunkDebugMsg, line, bufferSize - 1 - std::strlen(ChunkDebugMsg));

    for (int32_t i = 0; i < NumMissionScriptMessages; i++)
    {
        std::snprintf(line, sizeof(line), "line %5d: %5d, %5d\n", MissionScriptMessageLog[i][0],
                      MissionScriptMessageLog[i][1], MissionScriptMessageLog[i][2]);
        std::strncat(ChunkDebugMsg, line, bufferSize - 1 - std::strlen(ChunkDebugMsg));
    }

    auto* file = new MCFile;
    file->Create("scriptmsg.dbg");
    file->WriteString(ChunkDebugMsg);
    file->Close();
    delete file;
    ExceptionGameMsg = ChunkDebugMsg;
}

auto ExecHbSendMessage(MCAblSymbol* routineIdPtr) -> void
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    CurMultiplayCode = Tos->Integer;
    Pop();
    CurMultiplayParam = NextInteger();

    if (MPlayer && MPlayer->IsServer)
    {
        MPlayer->AddMissionScriptMessageChunk(CurMultiplayCode, CurMultiplayParam);

        if (NumMissionScriptMessages == 1000)
        {
            DebugMissionScriptMessages();
            Assert(0, static_cast<uint32_t>(NumMissionScriptMessages), " Way too many Mission Script Messages! ");
        }

        MissionScriptMessageLog[NumMissionScriptMessages][0] = static_cast<int16_t>(ExecLineNumber);
        MissionScriptMessageLog[NumMissionScriptMessages][1] = static_cast<int16_t>(CurMultiplayCode);
        MissionScriptMessageLog[NumMissionScriptMessages][2] = static_cast<int16_t>(CurMultiplayParam);
        NumMissionScriptMessages++;
    }

    GetCodeToken();
}

auto ExecHbGetMessage(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    int32_t* param = reinterpret_cast<int32_t*>(NextReference());
    Pop();
    *param = CurMultiplayParam;
    PushInteger(CurMultiplayCode);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetStrikes(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    GetCodeToken();
    GetCodeToken();
    ExecExpression();
    int32_t commanderId = Tos->Integer;
    Pop();
    GetCodeToken();
    ExecExpression();
    int32_t strikeType = Tos->Integer;
    Tos->Integer = 0;

    if (commanderId > -1 && commanderId < NumCommanders && strikeType > -1)
    {
        MCCommander* commander = CommanderTable[commanderId];

        switch (strikeType)
        {
            case 0:
                Tos->Integer = commander->NumSmallStrikes;
                break;
            case 1:
                Tos->Integer = commander->NumLargeStrikes;
                break;
            case 2:
                Tos->Integer = commander->NumSensorStrikes;
                break;
            case 3:
                Tos->Integer = commander->NumCameraDrones;
                break;
            default:
                break;
        }
    }

    GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>setstrikes / addstrikes: a commander's strikes of one type (0 small, 1 large, 2 sensor, 3 camera
    /// drones), set to <paramref name="count"/> or raised by it.</summary>
    auto ChangeStrikes(bool add) -> void
    {
        GetCodeToken();
        GetCodeToken();
        ExecExpression();
        int32_t commanderId = Tos->Integer;
        Pop();
        int32_t strikeType = NextInteger();
        int32_t count = NextInteger();

        if (commanderId > -1 && commanderId < NumCommanders && strikeType > -1)
        {
            MCCommander* commander = CommanderTable[commanderId];

            switch (strikeType)
            {
                case 0:
                    commander->SetNumSmallStrikes(add ? commander->NumSmallStrikes + count : count);
                    break;
                case 1:
                    commander->SetNumLargeStrikes(add ? commander->NumLargeStrikes + count : count);
                    break;
                case 2:
                    commander->SetNumSensorStrikes(add ? commander->NumSensorStrikes + count : count);
                    break;
                case 3:
                    commander->SetNumCameraDrones(add ? commander->NumCameraDrones + count : count);
                    break;
                default:
                    break;
            }
        }

        GetCodeToken();
    }
}

auto ExecHbSetStrikes(MCAblSymbol* routineIdPtr) -> void
{
    ChangeStrikes(false);
}

auto ExecHbAddStrikes(MCAblSymbol* routineIdPtr) -> void
{
    ChangeStrikes(true);
}

auto ExecHbIsServer(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(MPlayer && MPlayer->IsServer ? 1 : 0);
    GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbGetHomeTeam(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    PushInteger(HomeTeam->Id + 500);
    GetCodeToken();
    return IntegerTypePtr;
}

auto ExecStandardRoutineCall(MCAblSymbol* routineIdPtr) -> MCAblType*
{
    const MCAblRoutineKey key = routineIdPtr->Defn.Info.Routine.Key;

    switch (key)
    {
        case MCAblRoutineKey::Return:
        {
            ExecStdReturn(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::Print:
        {
            ExecStdPrint(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::Concat:
            return ExecStdConcat(routineIdPtr);
        case MCAblRoutineKey::Abs:
            return ExecStdAbs(routineIdPtr);
        case MCAblRoutineKey::Round:
            return ExecStdRound(routineIdPtr);
        case MCAblRoutineKey::Sqrt:
            return ExecStdSqrt(routineIdPtr);
        case MCAblRoutineKey::Trunc:
            return ExecStdTrunc(routineIdPtr);
        case MCAblRoutineKey::Random:
            return ExecStdRandom(routineIdPtr);
        case MCAblRoutineKey::SetMaxLoops:
            return ExecStdSetMaxLoops(routineIdPtr);
        case MCAblRoutineKey::Fatal:
            return ExecStdFatal(routineIdPtr);
        case MCAblRoutineKey::Assert:
            return ExecStdAssert(routineIdPtr);
        case MCAblRoutineKey::GetModuleHandle:
            return ExecStdGetModHandle(routineIdPtr);
        case MCAblRoutineKey::GetId:
            return ExecHbGetId(routineIdPtr);
        case MCAblRoutineKey::GetTime:
            return ExecHbGetTime(routineIdPtr);
        case MCAblRoutineKey::GetTimeLeft:
            return ExecHbGetTimeLeft(routineIdPtr);
        case MCAblRoutineKey::GetWarriorStatus:
            return ExecHbGetWarriorStatus(routineIdPtr);
        case MCAblRoutineKey::SelectUnit:
            return ExecHbSelectUnit(routineIdPtr);
        case MCAblRoutineKey::SelectWarrior:
            return ExecHbSelectWarrior(routineIdPtr);
        case MCAblRoutineKey::SelectObject:
            return ExecHbSelectObject(routineIdPtr);
        case MCAblRoutineKey::GetContacts:
            return ExecHbGetContacts(routineIdPtr);
        case MCAblRoutineKey::GetEnemyCount:
            return ExecHbGetEnemyCount(routineIdPtr);
        case MCAblRoutineKey::SelectContact:
            return ExecHbSelectContact(routineIdPtr);
        case MCAblRoutineKey::GetContactId:
            return ExecHbGetContactId(routineIdPtr);
        case MCAblRoutineKey::IsContact:
            return ExecHbIsContact(routineIdPtr);
        case MCAblRoutineKey::GetContactStatus:
            return ExecHbGetContactStatus(routineIdPtr);
        case MCAblRoutineKey::GetContactRelativePosition:
            return ExecHbGetContactRelativePosition(routineIdPtr);
        case MCAblRoutineKey::GetTarget:
            return ExecHbGetTarget(routineIdPtr);
        case MCAblRoutineKey::SetTarget:
        {
            ExecHbSetTarget(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetWeaponsReady:
        case MCAblRoutineKey::GetWeaponsLocked:
        case MCAblRoutineKey::GetWeaponsInRange:
            return ExecHbGetWeapons(routineIdPtr, key);
        case MCAblRoutineKey::GetWeaponShots:
            return ExecHbGetWeaponShots(routineIdPtr);
        case MCAblRoutineKey::GetWeaponRanges:
        {
            ExecHbGetWeaponRanges(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetObjectPosition:
            return ExecHbGetObjectPosition(routineIdPtr);
        case MCAblRoutineKey::GetIntegerMemory:
            return ExecHbGetMemoryInteger(routineIdPtr);
        case MCAblRoutineKey::GetRealMemory:
            return ExecHbGetMemoryReal(routineIdPtr);
        case MCAblRoutineKey::GetAlarmTriggers:
            return ExecHbGetAlarmTriggers(routineIdPtr);
        case MCAblRoutineKey::GetChallenger:
            return ExecHbGetChallenger(routineIdPtr);
        case MCAblRoutineKey::GetFireRanges:
            return ExecHbGetFireRanges(routineIdPtr);
        case MCAblRoutineKey::GetAttackers:
            return ExecHbGetAttackers(routineIdPtr);
        case MCAblRoutineKey::GetAttackerInfo:
            return ExecHbGetAttackerInfo(routineIdPtr);
        case MCAblRoutineKey::SetChallenger:
            return ExecHbSetChallenger(routineIdPtr);
        case MCAblRoutineKey::GetTimeWithoutOrders:
            return ExecHbGetTimeWithoutOrders(routineIdPtr);
        case MCAblRoutineKey::SetRadio:
        {
            ExecHbSetRadio(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetMoveGoal:
            return ExecHbSetMoveGoal(routineIdPtr);
        case MCAblRoutineKey::SetIntegerMemory:
        {
            ExecHbSetMemoryInteger(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetRealMemory:
        {
            ExecHbSetMemoryReal(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::HasMoveGoal:
            return ExecHbHasMoveGoal(routineIdPtr);
        case MCAblRoutineKey::HasMovePath:
            return ExecHbHasMovePath(routineIdPtr);
        case MCAblRoutineKey::SortWeapons:
        {
            ExecHbSortWeapons(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetVisualRange:
            return ExecHbGetVisualRange(routineIdPtr);
        case MCAblRoutineKey::GetUnitMates:
            return ExecHbGetUnitMates(routineIdPtr);
        case MCAblRoutineKey::GetTacOrder:
            return ExecHbGetTacOrder(routineIdPtr);
        case MCAblRoutineKey::GetLastTacOrder:
            return ExecHbGetLastTacOrder(routineIdPtr);
        case MCAblRoutineKey::SetOrderMode:
            return ExecHbSetOrderMode(routineIdPtr);
        case MCAblRoutineKey::OrderWait:
            return ExecHbWait(routineIdPtr);
        case MCAblRoutineKey::OrderMoveTo:
            return ExecHbMoveToPoint(routineIdPtr);
        case MCAblRoutineKey::OrderMoveToObject:
            return ExecHbMoveToObject(routineIdPtr);
        case MCAblRoutineKey::OrderMoveToContact:
            return ExecHbMoveToContact(routineIdPtr);
        case MCAblRoutineKey::OrderTraversePath:
        case MCAblRoutineKey::OrderPatrolPath:
        case MCAblRoutineKey::AttackClosestTarget:
        case MCAblRoutineKey::AttackPerOrders:
        case MCAblRoutineKey::Retreat:
        case MCAblRoutineKey::FireUponEnemyFireOnly:
            // Original behaviour: these do nothing, not even read their call's tokens.
            return nullptr;
        case MCAblRoutineKey::OrderPowerUp:
            return ExecHbOrderPowerUp(routineIdPtr);
        case MCAblRoutineKey::OrderPowerDown:
            return ExecHbOrderPowerDown(routineIdPtr);
        case MCAblRoutineKey::OrderAttackObject:
            return ExecHbOrderAttackObject(routineIdPtr);
        case MCAblRoutineKey::OrderAttackContact:
            return ExecHbOrderAttackContact(routineIdPtr);
        case MCAblRoutineKey::OrderWithdraw:
            return ExecHbObjWithdraw(routineIdPtr);
        case MCAblRoutineKey::DamageObject:
            return ExecHbDamageObject(routineIdPtr);
        case MCAblRoutineKey::SetAttackRadius:
            return ExecHbSetAttackRadius(routineIdPtr);
        case MCAblRoutineKey::OrderTest:
            return ExecHbOrderTest(routineIdPtr);
        case MCAblRoutineKey::PlaySmacker:
            return ExecHbPlaySmacker(routineIdPtr);
        case MCAblRoutineKey::ObjectChangeSides:
        {
            ExecHbObjectChangeSides(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::DistanceToObject:
            return ExecHbDistanceToObject(routineIdPtr);
        case MCAblRoutineKey::DistanceToPosition:
            return ExecHbDistanceToPosition(routineIdPtr);
        case MCAblRoutineKey::ObjectSuicide:
        {
            ExecHbObjectSuicide(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::ObjectCreate:
            return ExecHbObjectCreate(routineIdPtr);
        case MCAblRoutineKey::ObjectExists:
            return ExecHbObjectExists(routineIdPtr);
        case MCAblRoutineKey::ObjectStatus:
            return ExecHbObjectStatus(routineIdPtr);
        case MCAblRoutineKey::ObjectVisible:
            return ExecHbObjectVisible(routineIdPtr);
        case MCAblRoutineKey::ObjectClass:
            return ExecHbObjectClass(routineIdPtr);
        case MCAblRoutineKey::ObjectSide:
            return ExecHbObjectSide(routineIdPtr);
        case MCAblRoutineKey::ObjectCommander:
            return ExecHbObjectCommander(routineIdPtr);
        case MCAblRoutineKey::SetTimer:
            return ExecHbSetTimer(routineIdPtr);
        case MCAblRoutineKey::CheckTimer:
            return ExecHbChkTimer(routineIdPtr);
        case MCAblRoutineKey::EndTimer:
        {
            ExecHbEndTimer(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetObjectiveTimer:
            return ExecHbSetObjectiveTimer(routineIdPtr);
        case MCAblRoutineKey::CheckObjectiveTimer:
            return ExecHbCheckObjectiveTimer(routineIdPtr);
        case MCAblRoutineKey::SetObjectiveStatus:
            return ExecHbSetObjectiveStatus(routineIdPtr);
        case MCAblRoutineKey::CheckObjectiveStatus:
            return ExecHbCheckObjectiveStatus(routineIdPtr);
        case MCAblRoutineKey::SetObjectiveType:
            return ExecHbSetObjectiveType(routineIdPtr);
        case MCAblRoutineKey::CheckObjectiveType:
            return ExecHbCheckObjectiveType(routineIdPtr);
        case MCAblRoutineKey::PlayDigitalMusic:
            return ExecHbPlayDigitalMusic(routineIdPtr);
        case MCAblRoutineKey::StopMusic:
            return ExecHbStopMusic(routineIdPtr);
        case MCAblRoutineKey::PlaySoundEffect:
            return ExecHbPlaySoundEffect(routineIdPtr);
        case MCAblRoutineKey::PlayVideo:
            return ExecHbPlayVideo(routineIdPtr);
        case MCAblRoutineKey::PlaySpeech:
            return ExecHbPlaySpeech(routineIdPtr);
        case MCAblRoutineKey::PlayBetty:
            return ExecHbPlayBetty(routineIdPtr);
        case MCAblRoutineKey::SetObjectActive:
            return ExecHbSetObjActive(routineIdPtr);
        case MCAblRoutineKey::ObjectInWithdrawal:
            return ExecHbObjInWithdraw(routineIdPtr);
        case MCAblRoutineKey::ObjectTypeId:
            return ExecHbObjTypeId(routineIdPtr);
        case MCAblRoutineKey::GetTerrainObjectPartId:
            return ExecHbTerrainObjectId(routineIdPtr);
        case MCAblRoutineKey::GetVehiclePartId:
            return ExecHbVehicleId(routineIdPtr);
        case MCAblRoutineKey::GetWeaponAmmo:
            return ExecHbGetWeaponAmmo(routineIdPtr);
        case MCAblRoutineKey::ObjectStatusCount:
            return ExecHbObjectStatusCount(routineIdPtr);
        case MCAblRoutineKey::InArea:
            return ExecHbInArea(routineIdPtr);
        case MCAblRoutineKey::GetRelativePositionToPoint:
        {
            ExecHbRelPosPoint(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetRelativePositionToObject:
        {
            ExecHbRelPosObject(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetSensorsWorking:
            return ExecHbGetSensors(routineIdPtr);
        case MCAblRoutineKey::GetCurrentBRValue:
            return ExecHbGetBRValue(routineIdPtr);
        case MCAblRoutineKey::GetArmorPts:
            return ExecHbGetArmorPts(routineIdPtr);
        case MCAblRoutineKey::GetPilotId:
            return ExecHbGetPilotId(routineIdPtr);
        case MCAblRoutineKey::GetPilotWounds:
            return ExecHbGetPilotWounds(routineIdPtr);
        case MCAblRoutineKey::SetPilotWounds:
        {
            ExecHbSetPilotWounds(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetObjectActive:
            return ExecHbGetObjActive(routineIdPtr);
        case MCAblRoutineKey::GetObjectMaxDmg:
            // Original behaviour (OB-050): getobjectmaxdmg runs the damage points routine.
            return ExecHbGetObjDmgPts(routineIdPtr);
        case MCAblRoutineKey::GetObjectDamage:
            return ExecHbGetObjDamage(routineIdPtr);
        case MCAblRoutineKey::SetObjectDamage:
        {
            ExecHbSetObjDamage(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetGlobalValue:
            return ExecHbGetGlobalValue(routineIdPtr);
        case MCAblRoutineKey::SetGlobalValue:
        {
            ExecHbSetGlobalValue(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetObjectivePos:
        {
            ExecHbSetObjectivePos(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetPotentialContact:
            return ExecHbSetPotentialContact(routineIdPtr);
        case MCAblRoutineKey::SetSensorRange:
        {
            ExecHbSetSensorRange(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetTonnage:
        {
            ExecHbSetTonnage(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::PlayWaveFile:
        {
            ExecHbPlayWave(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetExplosionDamage:
        {
            ExecHbSetExplDmg(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetExplosionRadius:
        {
            ExecHbSetExplRad(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetSalvage:
        {
            ExecHbGetSalvage(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetSalvage:
        {
            // Original behaviour: the boolean result type is dropped.
            ExecHbSetSalvage(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetSalvageStatus:
        {
            ExecHbSetSalvageStatus(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetAnimation:
        {
            ExecHbSetAnimation(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetRevealed:
        {
            ExecHbSetRevealed(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::OrderRefit:
        {
            ExecHbRefit(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::OrderCapture:
        {
            ExecHbCaptureObject(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetCaptured:
        {
            ExecHbSetCaptured(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::SetCaptureable:
        {
            ExecHbSetCaptureable(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::IsCaptured:
            return ExecHbIsCaptured(routineIdPtr);
        case MCAblRoutineKey::IsCapturable:
            return ExecHbIsCapturable(routineIdPtr);
        case MCAblRoutineKey::WasEverCapturable:
            return ExecHbWasEverCapturable(routineIdPtr);
        case MCAblRoutineKey::SetBuildingName:
        {
            ExecHbSetBuildingName(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::CallStrike:
        {
            ExecHbCallStrike(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::OrderLoadElementals:
        {
            ExecHbLoadElementals(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::OrderDeployElementals:
        {
            ExecHbDeployElementals(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::AddPrisoner:
            return ExecHbAddPrisoner(routineIdPtr);
        case MCAblRoutineKey::SetTrainSpeed:
        {
            ExecHbSetTrainSpeed(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::LockGateOpen:
        {
            ExecHbLockGateOpen(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::LockGateClosed:
        {
            ExecHbLockGateClosed(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::ReleaseGateLock:
        {
            ExecHbReleaseGateLock(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::IsGateOpen:
            return ExecHbIsGateOpen(routineIdPtr);
        case MCAblRoutineKey::CallStrikeEx:
        {
            ExecHbCallStrikeEx(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetUnitStatus:
            return ExecHbGetUnitStatus(routineIdPtr);
        case MCAblRoutineKey::Repair:
        {
            ExecHbRepair(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetFixed:
            return ExecHbGetFixed(routineIdPtr);
        case MCAblRoutineKey::GetRepairState:
            return ExecHbGetRepairState(routineIdPtr);
        case MCAblRoutineKey::IsTeamTargeting:
            return ExecHbIsTeamTargeting(routineIdPtr);
        case MCAblRoutineKey::SendMessage:
        {
            ExecHbSendMessage(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetMessage:
            return ExecHbGetMessage(routineIdPtr);
        case MCAblRoutineKey::GetHomeTeam:
            return ExecHbGetHomeTeam(routineIdPtr);
        case MCAblRoutineKey::SetStrikes:
        {
            ExecHbSetStrikes(routineIdPtr);
            return nullptr;
        }
        case MCAblRoutineKey::GetStrikes:
            return ExecHbGetStrikes(routineIdPtr);
        case MCAblRoutineKey::IsServer:
            return ExecHbIsServer(routineIdPtr);
        case MCAblRoutineKey::AddStrikes:
        {
            ExecHbAddStrikes(routineIdPtr);
            return nullptr;
        }
        default:
        {
            // Original behaviour (OB-050): among others the module name and mode routines, the guard routines, the scans, getmaxarmor,
            // getobjectdmgpts and setcurrentbrvalue compile but have no runtime routine.
            char message[256];
            std::snprintf(message, sizeof(message), " ABL: Undefined ABL RoutineKey in %s:%d",
                          CurModule->GetName().c_str(), ExecLineNumber);
            Fatal(0, message);
        }
    }
}
