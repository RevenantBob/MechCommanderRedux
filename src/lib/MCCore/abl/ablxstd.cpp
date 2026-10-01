#include "stdafx.h"
#include "abl/ablxstd.h"
#include "abl/abldbug.h"
#include "abl/ablenv.h"
#include "abl/ablerr.h"
#include "abl/ablexec.h"
#include "abl/ablexpr.h"
#include "abl/ablrtn.h"
#include "abl/ablxexpr.h"
#include "abl/ablxstmt.h"
#include "ai/move.h"
#include "gui/asystem.h"
#include "gui/atextbox.h"
#include "iface/iface.h"
#include "lib/aerror.h"
#include "lib/cvmath.h"
#include "lib/file.h"
#include "lib/heap.h"
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
#include "sprite/actor.h"
#include "sprite/bactor.h"
#include "terrain/terrain.h"
#include "terrain/terrmap.h"

// Every exec routine reads its call the way the compiler wrote it: the routine's token, then "(", each argument
// expression evaluated onto the stack (by-reference arguments as the variable's address), a separator token
// between them, and ")". Most pop their arguments and leave the last slot on the stack holding the result.

int TacOrderOrigin = 1;
TokenCodeType ExitRoutineCodeSegment[2] = {TKN_END_FUNCTION, TKN_SEMICOLON};
TokenCodeType ExitOrderCodeSegment[2] = {TKN_END_FUNCTION, TKN_SEMICOLON};
int16_t MissionScriptMessageLog[1000][3] = {};
int32_t NumMissionScriptMessages = 0;
Mover* moverList[256] = {};
int IsUnitOrder = 0;
MoverGroup* CurGroup = nullptr;
GameObject* CurObject = nullptr;
int32_t CurObjectClass = 0;
int32_t CurAlarm = 0;
MechWarrior* CurWarrior = nullptr;
GameObject* CurContact = nullptr;
int32_t CurMultiplayCode = 0;
int32_t CurMultiplayParam = 0;
float globalMissionValues[50] = {};

namespace
{
    /// <summary>Evaluates the next argument (after its separator token) and pops it as an integer.</summary>
    auto nextInteger() -> int32_t
    {
        getCodeToken();
        execExpression();
        int32_t value = tos->integer;
        pop();
        return value;
    }

    /// <summary>Evaluates the next argument (after its separator token) and pops it as a real.</summary>
    auto nextReal() -> float
    {
        getCodeToken();
        execExpression();
        float value = tos->real;
        pop();
        return value;
    }

    /// <summary>Evaluates the next argument (after its separator token) and pops it as an address (an array's
    /// memory or a string).</summary>
    auto nextAddress() -> Address
    {
        getCodeToken();
        execExpression();
        Address value = tos->address;
        pop();
        return value;
    }

    /// <summary>Evaluates the next by-reference argument: the variable's address, left on the stack.</summary>
    auto nextReference() -> Address
    {
        SymTableNodePtr idPtr = getCodeSymTableNodePtr();
        baseType(execVariable(idPtr, USE_REFPARAM));
        return tos->address;
    }

    /// <summary>Whether <paramref name="object"/> is a mover (mech, vehicle, elemental or mover).</summary>
    auto isMover(BaseObject* object) -> bool
    {
        ObjectClass objectClass = object->objectClass;
        return objectClass == BATTLEMECH || objectClass == GROUNDVEHICLE || objectClass == ELEMENTAL ||
               objectClass == MOVER;
    }

    /// <summary>The object a script names by part id: -1 is the object whose brain runs.</summary>
    auto findObject(int32_t partId) -> GameObject*
    {
        if (partId == -1)
        {
            return CurObject;
        }

        return static_cast<GameObject*>(objectList->findObjectFromPart(partId));
    }

    /// <summary>Whether <paramref name="partId"/> names a group of movers (1..0x1ff) rather than one object.</summary>
    auto isGroupId(int32_t partId) -> bool
    {
        return partId >= 1 && partId <= 0x1ff;
    }

    /// <summary>
    /// Fills <see cref="moverList"/> with the movers a group id names: 1..32 the player commander's groups,
    /// 0xa5..0xc4 commander 1's, 0x149..0x168 commander 2's, 500 / 501 / 502 the Inner Sphere, Clan and allied
    /// teams.
    /// </summary>
    /// <returns>How many movers.</returns>
    auto getGroupMovers(int32_t groupId) -> int32_t
    {
        if (groupId < 0x21)
        {
            return CommanderTable[0]->getGroup(groupId - 1)->getMovers(moverList);
        }

        if (groupId >= 0x149 && groupId < 0x169)
        {
            return CommanderTable[2]->getGroup(groupId - 0x149)->getMovers(moverList);
        }

        if (groupId >= 0xa5 && groupId < 0xc5)
        {
            return CommanderTable[1]->getGroup(groupId - 0xa5)->getMovers(moverList);
        }

        if (groupId == 500)
        {
            return innerSphereTeam->getRoster(reinterpret_cast<GameObject**>(moverList));
        }

        if (groupId == 0x1f6)
        {
            if (alliedTeam == nullptr)
            {
                return 0;
            }

            return alliedTeam->getRoster(reinterpret_cast<GameObject**>(moverList));
        }

        if (groupId == 0x1f5)
        {
            return clanTeam->getRoster(reinterpret_cast<GameObject**>(moverList));
        }

        return 0;
    }

    /// <summary>The frame of the routine running (<see cref="CurRoutineIdPtr"/>), for its function value.</summary>
    auto currentRoutineFrame() -> StackItemPtr
    {
        StackItemPtr framePtr = stackFrameBasePtr;

        for (int32_t delta = level - CurRoutineIdPtr->level - 1; delta > 0; delta--)
        {
            framePtr =
                reinterpret_cast<StackItemPtr>(reinterpret_cast<StackFrameHeaderPtr>(framePtr)->staticLink.address);
        }

        return framePtr;
    }

    /// <summary>
    /// Formats the value on top of the stack for print and concat: an integer, char or real into
    /// <paramref name="buffer"/>, a string as itself.
    /// </summary>
    auto formatValue(TypePtr typePtr, char* buffer, size_t bufferSize) -> char*
    {
        if (typePtr == IntegerTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%d", tos->integer);
        }
        else if (typePtr == CharTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%c", tos->byte);
        }
        else if (typePtr == RealTypePtr)
        {
            std::snprintf(buffer, bufferSize, "%.4f", static_cast<double>(tos->real));
        }
        else if (typePtr->form == FRM_ARRAY && typePtr->info.array.elementTypePtr == CharTypePtr)
        {
            return reinterpret_cast<char*>(tos->address);
        }

        return buffer;
    }

    /// <summary>The debugger's report of where a print, fatal or assert ran.</summary>
    auto printLocation(char* message) -> void
    {
        debugger->print(message);
    }
}

auto execOrderReturn(SymTableNodePtr routineIdPtr, int32_t returnValue) -> void
{
    SymTableNodePtr curRoutineIdPtr = CurRoutineIdPtr;
    StackItemPtr framePtr = currentRoutineFrame();
    framePtr->integer = returnValue;
    ::returnValue = {};
    ::returnValue.integer = returnValue;

    if (debugger)
    {
        debugger->traceDataStore(curRoutineIdPtr, curRoutineIdPtr->typePtr, framePtr, curRoutineIdPtr->typePtr);
    }

    ExitWithReturn = 1;
    ExitFromTacOrder = 1;

    if (returnValue != 1)
    {
        codeSegmentPtr = reinterpret_cast<char*>(ExitOrderCodeSegment);
        getCodeToken();
    }
}

auto execStdReturn(SymTableNodePtr routineIdPtr) -> void
{
    returnValue = {};
    TypePtr returnTypePtr = CurRoutineIdPtr->typePtr;

    if (returnTypePtr)
    {
        StackItemPtr framePtr = currentRoutineFrame();
        getCodeToken();
        TypePtr returnBaseTypePtr = baseType(returnTypePtr);
        getCodeToken();
        TypePtr expressionTypePtr = execExpression();

        if (returnTypePtr == RealTypePtr && baseType(expressionTypePtr) == IntegerTypePtr)
        {
            framePtr->real = static_cast<float>(tos->integer);
        }
        else if (returnTypePtr->form == FRM_ARRAY)
        {
            // Original behaviour: the array is copied over the frame's function value slot (and past it).
            std::memcpy(framePtr, tos->address, static_cast<size_t>(returnTypePtr->size));
        }
        else if (returnBaseTypePtr == IntegerTypePtr || returnTypePtr->form == FRM_ENUM)
        {
            framePtr->integer = tos->integer;
        }
        else
        {
            framePtr->real = tos->real;
        }

        pop();
        returnValue.real = framePtr->real;

        if (debugger)
        {
            debugger->traceDataStore(CurRoutineIdPtr, CurRoutineIdPtr->typePtr, framePtr, returnTypePtr);
        }
    }

    getCodeToken();
    codeSegmentPtr = reinterpret_cast<char*>(ExitRoutineCodeSegment);
    ExitWithReturn = 1;
    getCodeToken();
}

auto execStdPrint(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    TypePtr typePtr = baseType(execExpression());
    char buffer[20];
    char* text = formatValue(typePtr, buffer, sizeof(buffer));
    pop();

    if (debugger)
    {
        char message[512];
        std::snprintf(message, sizeof(message), "PRINT:  \"%s\"", text);
        printLocation(message);
        std::snprintf(message, sizeof(message), "   MODULE %s", CurModule->getName());
        printLocation(message);
        std::snprintf(message, sizeof(message), "   FILE %s", CurModule->getSourceFile(FileNumber));
        printLocation(message);
        std::snprintf(message, sizeof(message), "   LINE %d", execLineNumber);
        printLocation(message);
        getCodeToken();
        return;
    }

    if (Terrain::terrainTacticalMap && Terrain::terrainTacticalMap->chatWindow)
    {
        Terrain::terrainTacticalMap->chatWindow->processChatString(0, text, -1);
    }

    getCodeToken();
}

auto execStdConcat(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    char* destination = reinterpret_cast<char*>(tos->address);
    pop();
    getCodeToken();
    TypePtr typePtr = baseType(execExpression());
    char buffer[20];
    char* text = formatValue(typePtr, buffer, sizeof(buffer));
    std::strcat(destination, text);
    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execStdAbs(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    TypePtr resultTypePtr = IntegerTypePtr;

    if (baseType(execExpression()) == IntegerTypePtr)
    {
        if (tos->integer < 0)
        {
            tos->integer = -tos->integer;
        }
    }
    else
    {
        resultTypePtr = RealTypePtr;

        if (tos->real < 0.0f)
        {
            tos->real = -tos->real;
        }
    }

    getCodeToken();
    return resultTypePtr;
}

auto execStdRound(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();

    if (static_cast<double>(tos->real) > 0.0)
    {
        tos->integer = static_cast<int32_t>(static_cast<double>(tos->real) + 0.5);
    }
    else
    {
        tos->integer = static_cast<int32_t>(static_cast<double>(tos->real) - 0.5);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execStdSqrt(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();

    if (baseType(execExpression()) == IntegerTypePtr)
    {
        tos->real = static_cast<float>(tos->integer);
    }

    if (tos->real < 0.0f)
    {
        runtimeError(ABL_ERR_RUNTIME_INVALID_FUNCTION_ARGUMENT);
    }
    else
    {
        tos->real = std::sqrt(tos->real);
    }

    getCodeToken();
    return RealTypePtr;
}

auto execStdTrunc(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();

    if (baseType(execExpression()) == RealTypePtr)
    {
        tos->integer = static_cast<int32_t>(tos->real);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execStdRandom(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = RandomNumber(tos->integer);
    getCodeToken();
    return IntegerTypePtr;
}

auto execStdGetModHandle(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(CurModuleHandle);
    getCodeToken();
    return IntegerTypePtr;
}

auto execStdGetModName(SymTableNodePtr routineIdPtr) -> TypePtr
{
    return nullptr;
}

auto execStdSetModName(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    TypePtr typePtr = baseType(execExpression());

    if (typePtr->form != FRM_ARRAY || typePtr->info.array.elementTypePtr != CharTypePtr)
    {
        runtimeError(ABL_ERR_RUNTIME_INVALID_FUNCTION_ARGUMENT);
    }

    // Original behaviour: the name is left on the stack and never used.
    getCodeToken();
}

auto execStdSetMaxLoops(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    MaxLoopIterations = tos->integer + 1;
    pop();
    getCodeToken();
    return nullptr;
}

auto execStdFatal(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t code = tos->integer;
    pop();
    char* text = reinterpret_cast<char*>(nextAddress());

    char message[512];

    if (debugger)
    {
        std::snprintf(message, sizeof(message), "FATAL:  [%d] \"%s\"", code, text);
        printLocation(message);
        std::snprintf(message, sizeof(message), "   MODULE (%d) %s", CurModule->getId(), CurModule->getName());
        printLocation(message);
        std::snprintf(message, sizeof(message), "   FILE %s", CurModule->getSourceFile(FileNumber));
        printLocation(message);
        std::snprintf(message, sizeof(message), "   LINE %d", execLineNumber);
        printLocation(message);
        debugger->debugMode();
        getCodeToken();
        return nullptr;
    }

    std::snprintf(message, sizeof(message), "ABL FATAL: [%d] %s", code, text);
    // Original behaviour: the formatted message is dropped; Fatal gets the script's text.
    Fatal(0, text);
}

auto execStdAssert(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t expression = tos->integer;
    pop();
    int32_t code = nextInteger();
    char* text = reinterpret_cast<char*>(nextAddress());

    if (expression == 0)
    {
        char message[512];

        if (debugger)
        {
            std::snprintf(message, sizeof(message), "ASSERT:  [%d] \"%s\"", code, text);
            printLocation(message);
            std::snprintf(message, sizeof(message), "   MODULE (%d) %s", CurModule->getId(), CurModule->getName());
            printLocation(message);
            std::snprintf(message, sizeof(message), "   FILE %s", CurModule->getSourceFile(FileNumber));
            printLocation(message);
            std::snprintf(message, sizeof(message), "   LINE %d", execLineNumber);
            printLocation(message);
            debugger->debugMode();
            getCodeToken();
            return nullptr;
        }

        std::snprintf(message, sizeof(message), "ABL ASSERT: [%d] %s", code, text);
        Fatal(0, message);
    }

    getCodeToken();
    return nullptr;
}

auto execHbGetId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(0);

    if (CurObject)
    {
        tos->integer = CurObject->partId;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetTime(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushReal(actualTime);
    getCodeToken();
    return RealTypePtr;
}

auto execHbGetTimeLeft(SymTableNodePtr routineIdPtr) -> TypePtr
{
    float timeLeft;

    if (scenario->timeLimit < 0)
    {
        timeLeft = -1.0f;
    }
    else
    {
        timeLeft = static_cast<float>(scenario->timeLimit) - actualTime;

        if (timeLeft <= 0.0f)
        {
            timeLeft = 0.0f;
        }
    }

    pushReal(timeLeft);
    getCodeToken();
    return RealTypePtr;
}

auto execHbGetTarget(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    if (IsUnitOrder == 0)
    {
        if (!isGroupId(partId))
        {
            GameObject* object = findObject(partId);

            if (object && isMover(object))
            {
                MechWarrior* pilot = object->getPilot();
                Assert(pilot != nullptr, 0, " execHbGetTarget:No pilot in mover! ");
                GameObject* target = pilot->getLastTarget();

                if (target)
                {
                    tos->integer = target->partId;
                }
            }
        }
    }
    else
    {
        GameObject* target = CurGroup->getPointPilot()->getLastTarget();

        // Port fix: the original reads the part id of a null target.
        if (target)
        {
            tos->integer = target->partId;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetTarget(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t targetId = nextInteger();
    GameObject* target = findObject(targetId);

    if (isGroupId(partId))
    {
        int32_t numMovers = getGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            MechWarrior* pilot = moverList[i]->getPilot();

            if (pilot)
            {
                pilot->setCurrentTarget(target);
                static_cast<Mover*>(pilot->vehicle)->calcOptimalRange(nullptr);
            }
        }
    }
    else
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            MechWarrior* pilot = object->getPilot();

            if (pilot)
            {
                pilot->setCurrentTarget(target);
                static_cast<Mover*>(pilot->vehicle)->calcOptimalRange(nullptr);
            }
        }
    }

    getCodeToken();
}

auto execHbSelectUnit(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = -1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSelectObject(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    int32_t previousId = 0;
    execExpression();

    if (CurObject)
    {
        previousId = CurObject->partId;
    }

    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object)
    {
        CurObject = static_cast<GameObject*>(object);
        tos->integer = previousId;
    }
    else
    {
        tos->integer = -1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSelectWarrior(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t warriorIndex = tos->integer;
    tos->integer = -1;
    int32_t previousIndex = 0;

    if (CurWarrior)
    {
        previousIndex = CurWarrior->index;
    }

    tos->integer = previousIndex;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= scenario->numWarriors)
    {
        CurWarrior = scenario->warriors[warriorIndex];
    }
    else
    {
        CurWarrior = nullptr;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetWarriorStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t warriorIndex = tos->integer;
    tos->integer = -1;

    if (warriorIndex > 0 && static_cast<uint32_t>(warriorIndex) <= scenario->numWarriors)
    {
        MechWarrior* warrior = scenario->warriors[warriorIndex];

        if (warrior)
        {
            tos->integer = warrior->status;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetContacts(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    int32_t* contacts = reinterpret_cast<int32_t*>(nextReference());
    pop();
    int32_t contactCriteria = nextInteger();
    getCodeToken();
    execExpression();
    int32_t sortType = tos->integer;
    tos->integer = -1;

    if (isMover(CurObject))
    {
        tos->integer = CurObject->getTeam()->getContacts(CurObject, contacts, contactCriteria, sortType);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetEnemyCount(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = -1;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            if (isMover(object))
            {
                tos->integer = object->getTeam()->numLOSContacts;
            }
            else if (object->objectClass == ARTILLERY || object->objectClass == BUILDING ||
                     object->objectClass == TREEBUILDING)
            {
                int32_t alignment = object->getAlignment();

                if (alignment == -1)
                {
                    tos->integer = clanTeam->numLOSContacts;
                }
                else if (alignment == 1)
                {
                    tos->integer = innerSphereTeam->numLOSContacts;
                }
            }
        }
    }
    else if (partId == 500)
    {
        tos->integer = innerSphereTeam->numLOSContacts;
    }
    else if (partId == 0x1f5)
    {
        tos->integer = clanTeam->numLOSContacts;
    }
    else if (partId == 0x1f6 && alliedTeam)
    {
        tos->integer = alliedTeam->numLOSContacts;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSelectContact(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    pop();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = -1;

    if (isMover(CurObject))
    {
        GameObject* contact = findObject(partId);

        if (contact && CurObject->getTeam()->getContactType(contact) != 0)
        {
            CurContact = contact;
            tos->integer = 0;
        }
        else
        {
            tos->integer = 1;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbIsContact(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t contactCriteria = nextInteger();
    getCodeToken();
    execExpression();
    int32_t select = tos->integer;
    tos->integer = -1;

    if (isMover(CurObject))
    {
        GameObject* object = findObject(partId);

        if (CurObject->getTeam()->isContact(object, contactCriteria) == 0)
        {
            tos->integer = 0;
        }
        else
        {
            tos->integer = partId;

            if (select)
            {
                CurContact = object;
            }
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetContactId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(CurContact ? CurContact->partId : 0);
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetContactStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    int32_t* tagged = reinterpret_cast<int32_t*>(nextReference());
    tos->integer = 0;

    *tagged = 0;
    if (CurContact)
    {
        int taggedFlag;
        tos->integer = CurContact->getContactType(CurObject->getTeam()->id, taggedFlag);
        *tagged = (taggedFlag == 1) ? 1 : 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetContactRelativePosition(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    float* range = reinterpret_cast<float*>(nextReference());
    pop();
    getCodeToken();
    float* angle = reinterpret_cast<float*>(nextReference());
    *range = -1.0f;
    tos->integer = 1;

    *angle = 0.0f;
    if (CurContact && CurObject)
    {
        vector_3d contactPosition = CurContact->getPosition();
        *range = CurObject->distanceFrom(contactPosition);
        *angle = CurObject->relFacingTo(CurContact->getPosition(), -1);
        tos->integer = 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetPotentialContact(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t contactType = tos->integer;

    // Original behaviour: the contact type is left on the stack as the result.
    if (isGroupId(partId))
    {
        int32_t numMovers = getGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            moverList[i]->setPotentialContact(contactType);
        }
    }
    else
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            object->setPotentialContact(contactType);
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetWeapons(SymTableNodePtr routineIdPtr, int32_t key) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(nextReference());
    pop();
    getCodeToken();
    execExpression();
    int32_t listSize = tos->integer;
    GameObject* target = CurWarrior->getLastTarget();
    tos->integer = -1;

    if (isMover(CurObject))
    {
        Mover* mover = static_cast<Mover*>(CurObject);

        if (key == RTN_GET_WEAPONS_READY)
        {
            tos->integer = mover->getWeaponsReady(weaponList, listSize);
        }
        else if (key == RTN_GET_WEAPONS_IN_RANGE && target)
        {
            vector_3d targetPosition = target->getPosition();
            tos->integer = mover->getWeaponsInRange(weaponList, listSize, mover->distanceFrom(targetPosition));
        }
        else
        {
            tos->integer = mover->getWeaponsLocked(weaponList, listSize);
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetWeaponShots(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t weaponIndex = tos->integer;
    tos->integer = -1;

    if (isMover(CurObject))
    {
        tos->integer = static_cast<Mover*>(CurObject)->getWeaponShots(weaponIndex);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetWeaponRanges(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    float* ranges = reinterpret_cast<float*>(nextReference());
    pop();
    GameObject* object = findObject(partId);

    if (object && isMover(object))
    {
        Mover* mover = static_cast<Mover*>(object);

        if (mover->shortestRangeWeapon == 0xff)
        {
            ranges[0] = -1.0f;
        }
        else
        {
            ranges[0] = MasterComponentList[mover->inventory[mover->shortestRangeWeapon].masterID].weaponRange[1];
        }

        ranges[1] = mover->getFireRange(-1);

        if (ranges[1] == -1.0f)
        {
            mover->calcOptimalRange(nullptr);
            ranges[1] = mover->getFireRange(-1);
        }

        ranges[2] = mover->getFireRange(-2);
    }
    else
    {
        ranges[2] = 0.0f;
        ranges[1] = 0.0f;
        ranges[0] = 0.0f;
    }

    getCodeToken();
}

auto execHbSetMoveGoal(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    uint32_t goalType = static_cast<uint32_t>(tos->integer);
    pop();
    getCodeToken();
    float* location = reinterpret_cast<float*>(nextReference());

    if (CurWarrior)
    {
        vector_3d goal;
        goal.x = location[0];
        goal.y = location[1];
        goal.z = location[2];
        CurWarrior->setMoveGoal(goalType, &goal, nullptr);
        location[0] = goal.x;
        location[1] = goal.y;
        location[2] = goal.z;
        CurWarrior->moveOrders.scriptGoal = 1;
    }

    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetChallenger(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object && isMover(object))
        {
            GameObject* challenger = static_cast<Mover*>(object)->getChallenger();

            if (challenger)
            {
                tos->integer = challenger->partId;
            }
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetFireRanges(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    float* ranges = reinterpret_cast<float*>(nextReference());
    pop();
    ranges[0] = WeaponRange[0];
    ranges[1] = WeaponRange[1];
    ranges[2] = WeaponRange[2];
    ranges[3] = scenario->maxWeaponRange;
    getCodeToken();
    return nullptr;
}

auto execHbGetAttackers(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    uint32_t* attackerList = reinterpret_cast<uint32_t*>(nextReference());
    pop();
    getCodeToken();
    execExpression();
    float seconds = tos->real;
    tos->integer = 0;

    if (CurWarrior)
    {
        tos->integer = CurWarrior->getAttackers(attackerList, seconds);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetAttackerInfo(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    uint32_t attackerId = static_cast<uint32_t>(tos->integer);
    tos->real = 1000000.0f;

    if ((attackerId == 0 || attackerId > 0x1ff) && CurWarrior)
    {
        _AttackerRec* attackerRec = CurWarrior->getAttackerInfo(attackerId);

        if (attackerRec)
        {
            tos->real = scenarioTime - attackerRec->lastTime;
        }
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbGetTimeWithoutOrders(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushReal(0.0f);

    if (CurWarrior && CurWarrior->timeOfLastOrders >= 0.0f)
    {
        tos->real = scenarioTime - CurWarrior->timeOfLastOrders;
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbSetChallenger(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t challengerId = tos->integer;
    tos->integer = 0;

    if (isGroupId(challengerId))
    {
        tos->integer = -1;
    }
    else
    {
        GameObject* challenger = findObject(challengerId);
        GameObject* object = findObject(partId);

        if (object && isMover(object))
        {
            static_cast<Mover*>(object)->setChallenger(challenger);
        }
        else
        {
            tos->integer = -2;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetMemoryInteger(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t cell = tos->integer;
    pop();
    int32_t value = nextInteger();
    CurWarrior->memory[cell].integer = value;
    getCodeToken();
}

auto execHbSetMemoryReal(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t cell = tos->integer;
    pop();
    float value = nextReal();
    CurWarrior->memory[cell].real = value;
    getCodeToken();
}

auto execHbHasMoveGoal(SymTableNodePtr routineIdPtr) -> TypePtr
{
    if (CurWarrior && CurWarrior->moveOrders.scriptGoal != 0 && CurWarrior->moveOrders.goalType != -1)
    {
        pushInteger(1);
    }
    else
    {
        pushInteger(0);
    }

    getCodeToken();
    return BooleanTypePtr;
}

auto execHbHasMovePath(SymTableNodePtr routineIdPtr) -> TypePtr
{
    if (CurWarrior && CurWarrior->getMovePath() && CurWarrior->moveOrders.scriptGoal == 0)
    {
        pushInteger(1);
    }
    else
    {
        pushInteger(0);
    }

    getCodeToken();
    return BooleanTypePtr;
}

auto execHbSortWeapons(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    int32_t* weaponList = reinterpret_cast<int32_t*>(nextReference());
    pop();
    int32_t listSize = nextInteger();
    int32_t sortType = nextInteger();
    int32_t valueList[48];

    if (CurObject && isMover(CurObject))
    {
        static_cast<Mover*>(CurObject)->sortWeapons(weaponList, valueList, listSize, sortType, 1);
    }

    getCodeToken();
}

auto execHbGetObjectPosition(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    float* position = reinterpret_cast<float*>(nextReference());
    pop();
    pushInteger(0);

    if (isGroupId(partId))
    {
        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;
    }
    else
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            vector_3d objectPosition = object->getPosition();
            position[0] = objectPosition.x;
            position[1] = objectPosition.y;
            position[2] = objectPosition.z;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetVisualRange(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;

    if (isGroupId(partId))
    {
        tos->real = -1.0f;
    }
    else
    {
        // Original behaviour: for an object that isn't a mover the part id stays as the (real) result.
        GameObject* object = findObject(partId);

        if (object && isMover(object))
        {
            tos->real = static_cast<Mover*>(object)->getVisualRange();
        }
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbGetMemoryInteger(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = CurWarrior->memory[tos->integer].integer;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetMemoryReal(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->real = CurWarrior->memory[tos->integer].real;
    getCodeToken();
    return RealTypePtr;
}

auto execHbGetAlarmTriggers(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    uint32_t* triggerList = reinterpret_cast<uint32_t*>(nextReference());
    tos->integer = CurWarrior->getAlarmTriggers(CurAlarm, triggerList);
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetUnitMates(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    getCodeToken();
    int32_t* mateList = reinterpret_cast<int32_t*>(nextReference());
    tos->integer = 0;
    int32_t numMates = 0;

    if (isGroupId(partId))
    {
        numMates = getGroupMovers(partId);
    }
    else
    {
        GameObject* object = findObject(partId);

        if (!object || !isMover(object) || !static_cast<Mover*>(object)->group)
        {
            getCodeToken();
            return IntegerTypePtr;
        }

        numMates = static_cast<Mover*>(object)->group->getMovers(moverList);
    }

    for (int32_t i = 0; i < numMates; i++)
    {
        mateList[i] = moverList[i]->partId;
    }

    tos->integer = numMates;
    getCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>gettacorder / getlasttacorder: the order's time stamp and parameters of a mover's pilot.</summary>
    auto getTacOrderData(bool last) -> void
    {
        getCodeToken();
        getCodeToken();
        execExpression();
        int32_t partId = tos->integer;
        getCodeToken();
        float* timeStamp = reinterpret_cast<float*>(nextReference());
        pop();
        getCodeToken();
        int32_t* paramList = reinterpret_cast<int32_t*>(nextReference());
        tos->integer = 0;

        if (!isGroupId(partId))
        {
            GameObject* object = findObject(partId);

            if (object && isMover(object))
            {
                MechWarrior* pilot = object->getPilot();

                if (pilot)
                {
                    TacticalOrder& order = last ? pilot->lastTacOrder : pilot->curTacOrder;
                    tos->integer = order.getParamData(timeStamp, paramList);
                }
            }
        }

        getCodeToken();
    }
}

auto execHbGetTacOrder(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getTacOrderData(false);
    return IntegerTypePtr;
}

auto execHbGetLastTacOrder(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getTacOrderData(true);
    return IntegerTypePtr;
}

auto execHbSetOrderMode(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    // Original behaviour: the argument is ignored; the mode is always reset to pilot orders.
    int wasUnitOrder = IsUnitOrder != 0;
    IsUnitOrder = 0;
    tos->integer = wasUnitOrder;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbWait(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    float seconds = tos->real;
    pop();
    getCodeToken();
    execExpression();
    int clearLastTarget = tos->integer == 1;
    int32_t result = 0;

    if (IsUnitOrder == 0)
    {
        // The original rounds with the 1.5 * 2^52 addition trick: to nearest, ties to even.
        result = CurWarrior->orderWait(0, 1, static_cast<int32_t>(std::nearbyint(seconds)), clearLastTarget);
    }
    else
    {
        Fatal(0, " Team orderwait needs support ");
    }

    tos->integer = result;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetAttackRadius(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    float radius = tos->real;
    tos->real = CurWarrior->attackRadius;
    CurWarrior->attackRadius = radius;
    getCodeToken();
    return RealTypePtr;
}

auto execHbMoveToPoint(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    float* location = reinterpret_cast<float*>(nextReference());
    pop();
    getCodeToken();
    execExpression();
    vector_3d goal;
    goal.x = location[0];
    goal.y = location[1];
    goal.z = location[2];
    uint32_t params = tos->integer == 1 ? 1 : 0;
    int32_t result;

    if (IsUnitOrder == 0)
    {
        result = CurWarrior->orderMoveToPoint(0, 1, 1, goal, -1, params);
    }
    else
    {
        result = CurGroup->orderMoveToPoint(1, 1, goal, params);
    }

    tos->integer = result;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbMoveToObject(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t flag = tos->integer;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            if (IsUnitOrder == 0)
            {
                tos->integer = CurWarrior->orderMoveToObject(0, 1, 1, object, -1, flag == 1 ? 1 : 0);
            }
            else
            {
                tos->integer = CurGroup->orderMoveToObject(1, 1, object, 1);
            }

            getCodeToken();
            return IntegerTypePtr;
        }
    }

    tos->integer = 1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbMoveToContact(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t result = -1;

    if (CurContact)
    {
        if (IsUnitOrder != 0)
        {
            result = CurGroup->orderMoveToObject(1, 1, CurContact, 1);
        }
        else
        {
            result = CurWarrior->orderMoveToObject(0, 1, 1, CurContact, -1, tos->integer == 1 ? 1 : 0);
        }
    }

    tos->integer = result;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbOrderPowerDown(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(0);

    if (IsUnitOrder != 0)
    {
        tos->integer = CurGroup->orderPowerDown(TacOrderOrigin);
    }
    else
    {
        tos->integer = CurWarrior->orderPowerDown(0, TacOrderOrigin);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbOrderPowerUp(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(0);

    if (IsUnitOrder != 0)
    {
        tos->integer = CurGroup->orderPowerUp(TacOrderOrigin);
    }
    else
    {
        tos->integer = CurWarrior->orderPowerUp(0, TacOrderOrigin);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbOrderAttackObject(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    uint32_t partId = static_cast<uint32_t>(tos->integer);
    pop();
    int32_t attackType = nextInteger();
    int32_t attackMethod = nextInteger();
    int32_t attackRange = nextInteger();
    getCodeToken();
    execExpression();
    uint32_t params = tos->integer != 0 ? 0x10 : 0;

    if (partId != 0 && partId < 0x200)
    {
        tos->integer = 1;
        getCodeToken();
        return IntegerTypePtr;
    }

    // Unlike the other routines, -1 is looked up as a part id rather than meaning the current object.
    GameObject* target = nullptr;

    if (partId != 0)
    {
        target = static_cast<GameObject*>(objectList->findObjectFromPart(static_cast<int32_t>(partId)));
    }

    if (IsUnitOrder != 0)
    {
        tos->integer = CurGroup->orderAttackObject(1, target, attackType, attackMethod, attackRange, -1, params);
    }
    else
    {
        tos->integer = CurWarrior->orderAttackObject(0, 1, target, attackType, attackMethod, attackRange, -1, params);
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbOrderAttackContact(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t attackType = tos->integer;
    pop();
    int32_t attackMethod = nextInteger();
    int32_t attackRange = nextInteger();
    getCodeToken();
    execExpression();
    uint32_t params = tos->integer != 0 ? 0x10 : 0;
    int32_t result = -2;

    if (CurContact)
    {
        result = CurWarrior->orderAttackObject(0, 1, CurContact, attackType, attackMethod, attackRange, -1, params);
    }

    tos->integer = result;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbOrderTest(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbPlaySmacker(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectChangeSides(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t alignment = nextInteger();

    if (isGroupId(partId))
    {
        Fatal(0, " Cannot ABL:ObjectChangeSides for Mover Units ");
    }

    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && object->getObjectType())
    {
        static_cast<GameObject*>(object)->setAlignment(alignment);
    }

    getCodeToken();
}

namespace
{
    /// <summary>The distance in world units from <paramref name="object"/> to (x, y), ignoring height.</summary>
    auto flatDistance(GameObject* object, float x, float y) -> float
    {
        vector_3d objectPosition = object->getPosition();
        float dx = x - objectPosition.x;
        float dy = y - objectPosition.y;
        return std::sqrt(dx * dx + dy * dy + 0.0f * 0.0f);
    }

    /// <summary>
    /// distancetoobject / distancetoposition: meters from (x, y) to an object, or to the nearest existing, awake
    /// mover of a group; <paramref name="result"/> keeps its value when there is none.
    /// </summary>
    auto distanceFromId(int32_t partId, float x, float y, float& result) -> void
    {
        if (!isGroupId(partId))
        {
            GameObject* object = findObject(partId);

            if (object)
            {
                result = flatDistance(object, x, y) * metersPerWorldUnit;
            }

            return;
        }

        int32_t numMovers = getGroupMovers(partId);
        float closest = 3.4e38f;

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (moverList[i]->getExistsAndAwake())
            {
                float distance = flatDistance(moverList[i], x, y);

                if (distance < closest)
                {
                    closest = distance;
                }
            }
        }

        if (static_cast<double>(closest) < 3.4e38)
        {
            result = metersPerWorldUnit * closest;
        }
    }

    /// <summary>Takes <paramref name="object"/> out of the object list (it gets destroyed with it).</summary>
    auto removeFromObjectList(BaseObject* object) -> void
    {
        for (ObjectQueueNode* node = objectList->head; node; node = node->next)
        {
            if (node->remove(object))
            {
                break;
            }
        }
    }

    /// <summary>The pilot a script names by index: -1 is the current one, otherwise 1..numWarriors.</summary>
    /// <returns>Null for an index out of range.</returns>
    auto findWarrior(int32_t warriorIndex) -> MechWarrior*
    {
        if (warriorIndex == -1)
        {
            return CurWarrior;
        }

        if (warriorIndex < 1 || static_cast<uint32_t>(warriorIndex) > scenario->numWarriors)
        {
            return nullptr;
        }

        return scenario->warriors[warriorIndex];
    }
}

auto execHbDistanceToObject(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t targetId = tos->integer;
    tos->real = -1.0f;
    GameObject* target = findObject(targetId);

    if (target)
    {
        vector_3d targetPosition = target->getPosition();
        distanceFromId(partId, targetPosition.x, targetPosition.y, tos->real);
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbDistanceToPosition(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    float* position = reinterpret_cast<float*>(nextReference());
    pop();
    pushReal(-1.0f);
    distanceFromId(partId, position[0], position[1], tos->real);
    getCodeToken();
    return RealTypePtr;
}

auto execHbObjectSuicide(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();

    if (isGroupId(partId))
    {
        int32_t numMovers = getGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            removeFromObjectList(moverList[i]);
        }
    }
    else
    {
        BaseObject* object = objectList->findObjectFromPart(partId);

        if (object)
        {
            removeFromObjectList(object);
        }
    }

    getCodeToken();
}

auto execHbObjectCreate(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    for (int32_t i = 0; i < currentCreatorPart; i++)
    {
        if (createdPartRoster[i].partId == partId)
        {
            if (createdPartRoster[i].created == 0)
            {
                scenario->createScenarioObject(partId);
                innerSphereTeam->scanBattlefield();

                if (alliedTeam)
                {
                    alliedTeam->scanBattlefield();
                }

                tos->integer = partId;
            }
            break;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectExists(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    if (isGroupId(partId))
    {
        if (getGroupMovers(partId) > 0)
        {
            tos->integer = 1;
        }
    }
    else if (findObject(partId))
    {
        tos->integer = 1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = -1;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            tos->integer = static_cast<uint8_t>(object->status);
        }

        getCodeToken();
        return IntegerTypePtr;
    }

    // A group is 1 (gone) unless one of its movers is neither disabled nor destroyed and has a pilot who hasn't
    // withdrawn.
    int32_t numMovers = getGroupMovers(partId);

    for (int32_t i = 0; i < numMovers; i++)
    {
        uint8_t status = static_cast<uint8_t>(moverList[i]->status);

        if (status != 2 && status != 1)
        {
            MechWarrior* pilot = moverList[i]->getPilot();

            if (pilot && pilot->status != 2)
            {
                tos->integer = 0;
                getCodeToken();
                return IntegerTypePtr;
            }
        }
    }

    tos->integer = 1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectStatusCount(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    int32_t* counts = reinterpret_cast<int32_t*>(nextReference());
    pop();

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            counts[static_cast<uint8_t>(object->status)]++;
        }
    }
    else if (partId < 0x21)
    {
        CommanderTable[0]->getGroup(partId - 1)->statusCount(counts);
    }
    else if (partId >= 0x149 && partId < 0x169)
    {
        CommanderTable[2]->getGroup(partId - 0x149)->statusCount(counts);
    }
    else if (partId >= 0xa5 && partId < 0xc5)
    {
        CommanderTable[1]->getGroup(partId - 0xa5)->statusCount(counts);
    }
    else if (partId == 500)
    {
        innerSphereTeam->statusCount(counts);
    }
    else if (partId == 0x1f6)
    {
        if (alliedTeam)
        {
            alliedTeam->statusCount(counts);
        }
    }
    else if (partId == 0x1f5)
    {
        clanTeam->statusCount(counts);
    }

    getCodeToken();
    return nullptr;
}

auto execHbObjectVisible(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t lookerId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    GameObject* target = static_cast<GameObject*>(objectList->findObjectFromPart(tos->integer));
    tos->integer = 0;

    if (target)
    {
        if (!isGroupId(lookerId))
        {
            GameObject* looker = static_cast<GameObject*>(objectList->findObjectFromPart(lookerId));

            if (looker)
            {
                tos->integer = looker->lineOfSight(target);
            }
        }
        else
        {
            for (BaseObject* looker = objectList->findObjectInGroup(nullptr, lookerId); looker;
                 looker = objectList->findObjectInGroup(looker, lookerId))
            {
                if (static_cast<GameObject*>(looker)->lineOfSight(target))
                {
                    tos->integer = 1;
                    break;
                }
            }
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectSide(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && object->getObjectType())
    {
        tos->integer = static_cast<GameObject*>(object)->getAlignment();
    }
    else
    {
        tos->integer = 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectCommander(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = -1;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            tos->integer = object->getCommanderId();
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjectClass(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);
    tos->integer = object ? static_cast<int32_t>(object->objectClass) : -1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbInArea(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    float* position = reinterpret_cast<float*>(nextReference());
    pop();
    float radius = nextReal();
    getCodeToken();
    execExpression();
    int32_t numRequired = tos->integer;
    vector_3d center;
    center.x = position[0];
    center.y = position[1];
    center.z = position[2];

    if (!isGroupId(partId))
    {
        // Original behaviour: with a count of 0 a single object is always in the area.
        tos->integer = 1;

        if (numRequired != 0)
        {
            tos->integer = 0;
            GameObject* object = findObject(partId);

            if (object && object->getExists() && object->getAwake() && !object->isDisabled() &&
                !object->isDestroyed() && object->distanceFrom(center) <= radius)
            {
                tos->integer = 1;
            }
        }

        getCodeToken();
        return BooleanTypePtr;
    }

    int32_t numMovers = getGroupMovers(partId);

    if (numRequired == -1)
    {
        // All of the group's working movers: false if one that exists and is awake is outside, or there are none.
        tos->integer = 1;
        int32_t numWorking = 0;

        for (int32_t i = 0; i < numMovers; i++)
        {
            Mover* mover = moverList[i];

            if (!mover->isDisabled())
            {
                numWorking++;

                if (mover->getExists() && mover->getAwake() && mover->distanceFrom(center) > radius)
                {
                    tos->integer = 0;
                    break;
                }
            }
        }

        if (numWorking == 0)
        {
            tos->integer = 0;
        }

        getCodeToken();
        return BooleanTypePtr;
    }

    // At least numRequired movers that exist, are awake and work.
    tos->integer = 0;
    int32_t numInside = 0;

    for (int32_t i = 0; i < numMovers; i++)
    {
        Mover* mover = moverList[i];

        if (mover->getExists() && mover->getAwake() && !mover->isDisabled() && !mover->isDestroyed() &&
            mover->distanceFrom(center) <= radius)
        {
            if (++numInside == numRequired)
            {
                tos->integer = 1;
                break;
            }
        }
    }

    getCodeToken();
    return BooleanTypePtr;
}

auto execHbSetTimer(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int16_t timerId = static_cast<int16_t>(tos->integer);
    pop();
    getCodeToken();
    execExpression();

    if (timerId < 7 || timerId > 14)
    {
        timerId = 0;
    }
    else
    {
        // Original behaviour: the time is read as a real even when the script passed an integer.
        application->AddTimer(application, timerId, static_cast<int32_t>(static_cast<double>(tos->real) * 1000.0),
                              0x1406, 0, 0);
    }

    tos->integer = timerId;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbChkTimer(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    aTimer* timer = application->timerManager->GetTimer(application, static_cast<int16_t>(tos->integer));
    uint32_t remaining = 0;

    if (timer)
    {
        remaining = timer->interval + timer->lastTime - MCPort::Milliseconds();
    }

    tos->real = static_cast<float>(static_cast<double>(remaining) * 0.001);
    getCodeToken();
    return RealTypePtr;
}

auto execHbEndTimer(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int16_t timerId = static_cast<int16_t>(tos->integer);
    pop();

    if (timerId > 6 && timerId < 15)
    {
        application->RemoveTimer(application, timerId);
    }

    getCodeToken();
}

auto execHbSetObjectiveTimer(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t objectiveNumber = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    tos->integer = scenario->setObjectiveTimer(objectiveNumber, tos->real * 1000.0f);
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbCheckObjectiveTimer(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->real = scenario->checkObjectiveTimer(tos->integer);
    getCodeToken();
    return RealTypePtr;
}

auto execHbSetObjectiveStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t objectiveNumber = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    tos->integer = scenario->setObjectiveStatus(objectiveNumber, static_cast<uint32_t>(tos->integer));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbCheckObjectiveStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = static_cast<int32_t>(scenario->checkObjectiveStatus(tos->integer));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetObjectiveType(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t objectiveNumber = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    tos->integer = scenario->setObjectiveType(objectiveNumber, static_cast<uint32_t>(tos->integer));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbCheckObjectiveType(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    tos->integer = static_cast<int32_t>(scenario->checkObjectiveType(tos->integer));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbPlayDigitalMusic(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();

    if (soundSystem)
    {
        soundSystem->playABLDigitalMusic(tos->integer);
    }

    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbStopMusic(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();

    if (soundSystem)
    {
        soundSystem->stopABLMusic();
    }

    // Original behaviour: nothing was pushed, so this overwrites whatever is on top of the stack.
    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbPlaySoundEffect(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();

    if (soundSystem)
    {
        soundSystem->playABLSFX(tos->integer);
    }

    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbPlayVideo(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();

    if (soundSystem)
    {
        soundSystem->playABLVideo(tos->integer);
    }

    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetRadio(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t warriorIndex = tos->integer;
    pop();
    int32_t enable = nextInteger();
    MechWarrior* warrior = findWarrior(warriorIndex);

    if (warrior && warrior->radio)
    {
        warrior->radio->enabled = (enable == 1) ? 1 : 0;
    }

    getCodeToken();
}

auto execHbPlaySpeech(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t warriorIndex = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    MechWarrior* warrior = findWarrior(warriorIndex);

    if (warrior)
    {
        warrior->radioMessage(tos->integer, 1);
    }

    tos->integer = 0;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbPlayBetty(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    uint32_t bettyId = static_cast<uint32_t>(tos->integer);
    pop();
    pushInteger(soundSystem->playBettySample(bettyId));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetObjActive(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int active = tos->integer == 1 ? 1 : 0;
    int32_t numChanged = 0;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object && object->getAwake() != active)
        {
            object->setAwake(active);
            theInterface->ActivateMech(object->partId);
            numChanged = 1;
        }
    }
    else
    {
        // Original behaviour: the walk stops at the first member already in the wanted state.
        BaseObject* object = objectList->findObjectInGroup(nullptr, partId);

        while (object && static_cast<GameObject*>(object)->getAwake() != active)
        {
            object->setAwake(active);
            theInterface->ActivateMech(object->partId);
            numChanged++;
            object = objectList->findObjectInGroup(object, partId);
        }
    }

    tos->integer = numChanged;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjWithdraw(SymTableNodePtr routineIdPtr) -> TypePtr
{
    // Original behaviour (OB-043): two items are pushed for the one result, so every call leaves one behind.
    pushInteger(0);
    pushInteger(0);
    vector_3d nowhere;
    nowhere.x = 0.0f;
    nowhere.y = 0.0f;
    nowhere.z = 0.0f;

    if (IsUnitOrder == 0)
    {
        if (CurWarrior)
        {
            CurWarrior->orderWithdraw(0, 1, nowhere);
        }
        else
        {
            tos->integer = -2;
        }
    }
    else if (CurGroup)
    {
        CurGroup->orderWithdraw(1, nowhere);
    }
    else
    {
        tos->integer = -1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjInWithdraw(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 1;

    if (!isGroupId(partId))
    {
        BaseObject* object = objectList->findObjectFromPart(partId);

        if (object && object->getObjectType() && !static_cast<GameObject*>(object)->isWithdrawing())
        {
            tos->integer = 0;
        }
    }
    else
    {
        for (BaseObject* object = objectList->findObjectInGroup(nullptr, partId); object && tos->integer == 1;
             object = objectList->findObjectInGroup(object, partId))
        {
            if (!static_cast<GameObject*>(object)->isWithdrawing())
            {
                tos->integer = 0;
            }
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbObjTypeId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = -1;
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && object->getObjectType())
    {
        tos->integer = object->getObjectType()->objTypeNum;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbTerrainObjectId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t blockNumber = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    tos->integer = (blockNumber * 400 + tos->integer) * 8 + 0x1000;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbVehicleId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    pop();
    getCodeToken();
    execExpression();
    tos->integer = -1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetWeaponAmmo(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    // The weapon index goes through a float on its way to the call.
    float weaponIndex = static_cast<float>(tos->integer);
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && isMover(object))
    {
        tos->integer = static_cast<Mover*>(object)->getWeaponShots(static_cast<int32_t>(weaponIndex));
    }
    else
    {
        tos->integer = -1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetSensors(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && isMover(object) && static_cast<Mover*>(object)->sensorSystem)
    {
        tos->integer = static_cast<Mover*>(object)->sensorSystem->enabled();
    }
    else
    {
        tos->integer = -1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetBRValue(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    GameObject* object = findObject(tos->integer);
    tos->integer = object ? object->getCurCV() : -1;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetBRValue(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t newCV = nextInteger();
    GameObject* object = findObject(partId);

    if (object)
    {
        object->setCurCV(newCV);
    }

    // Original behaviour: nothing is left on the stack for the integer result.
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetArmorPts(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && isMover(object))
    {
        Mover* mover = static_cast<Mover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->numArmorLocations; i++)
        {
            total = static_cast<int32_t>(static_cast<float>(total) + mover->armor[i].curArmor);
        }

        tos->integer = total;
    }
    else
    {
        tos->integer = 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetMaxArmor(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && isMover(object))
    {
        Mover* mover = static_cast<Mover*>(object);
        int32_t total = 0;

        for (int32_t i = 0; i < mover->numArmorLocations; i++)
        {
            total += mover->armor[i].maxArmor;
        }

        tos->integer = total;
    }
    else
    {
        tos->integer = 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetPilotId(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && isMover(object))
    {
        tos->integer = static_cast<GameObject*>(object)->getPilot()->index;
    }
    else
    {
        tos->integer = -1;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetPilotWounds(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object && isMover(object))
    {
        tos->real = static_cast<GameObject*>(object)->getPilot()->wounds;
    }
    else
    {
        tos->integer = 0;
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbSetPilotWounds(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t wounds = nextInteger();

    if (wounds > 6)
    {
        wounds = 6;
    }

    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && isMover(object))
    {
        static_cast<GameObject*>(object)->getPilot()->wounds = static_cast<float>(wounds);
    }

    // Original behaviour: nothing is left on the stack for the real result.
    getCodeToken();
    return RealTypePtr;
}

auto execHbGetObjActive(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    if (isGroupId(partId))
    {
        int32_t numAwake = 0;

        for (BaseObject* object = objectList->findObjectInGroup(nullptr, partId); object && tos->integer == 0;
             object = objectList->findObjectInGroup(object, partId))
        {
            if (static_cast<GameObject*>(object)->getAwake())
            {
                numAwake++;
            }
        }

        tos->integer = numAwake;
    }
    else
    {
        GameObject* object = findObject(partId);

        if (object && object->getAwake())
        {
            tos->integer = 1;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>Whether getobjectdamage and its kin handle <paramref name="object"/>: a typed building, terrain
    /// object or misc terrain object.</summary>
    auto isDamageableScenery(BaseObject* object) -> bool
    {
        if (!object || !object->getObjectType())
        {
            return false;
        }

        return static_cast<GameObject*>(object)->isBuilding() || object->objectClass == TERRAINOBJECT ||
               object->objectClass == MISCTERRAINOBJECT;
    }

    /// <summary>
    /// The damage that destroys <paramref name="object"/>, from its type: the building's, turret's or terrain
    /// object's dmgLevel, or the misc terrain object's by kind (5 bridge, 6 forest, 7 wall, 8 medium wall, 9 light
    /// wall).
    /// </summary>
    /// <returns>False for any other class or kind.</returns>
    auto getDamageLevel(GameObject* object, uint32_t& damageLevel) -> bool
    {
        ObjectType* type = object->getObjectType();

        switch (object->objectClass)
        {
            case BUILDING:
            {
                damageLevel = static_cast<BuildingType*>(type)->dmgLevel;
                return true;
            }
            case TURRET:
            {
                damageLevel = static_cast<TurretType*>(type)->dmgLevel;
                return true;
            }
            case TERRAINOBJECT:
            {
                damageLevel = static_cast<TerrainObjectType*>(type)->dmgLevel;
                return true;
            }
            case TREEBUILDING:
            {
                damageLevel = static_cast<TreeBuildingType*>(type)->dmgLevel;
                return true;
            }
            case MISCTERRAINOBJECT:
            {
                MiscTerrainObjectType* miscType = static_cast<MiscTerrainObjectType*>(type);

                switch (static_cast<MiscTerrainObject*>(object)->terrainObjectKind)
                {
                    case 5:
                    {
                        damageLevel = miscType->bridgeDmgLevel;
                        return true;
                    }
                    case 6:
                    {
                        damageLevel = miscType->forestDmgLevel;
                        return true;
                    }
                    case 7:
                    {
                        damageLevel = miscType->wallDmgLevel;
                        return true;
                    }
                    case 8:
                    {
                        damageLevel = miscType->mediumWallDmgLevel;
                        return true;
                    }
                    case 9:
                    {
                        damageLevel = miscType->lightWallDmgLevel;
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
    auto applyShot(GameObject* target, _WeaponShotInfo* shotInfo) -> bool
    {
        if (MPlayer == nullptr)
        {
            target->handleWeaponHit(shotInfo, 0);
            return true;
        }

        if (MPlayer->isServer == 0)
        {
            return false;
        }

        target->handleWeaponHit(shotInfo, 1);
        return true;
    }
}

auto execHbGetObjDamage(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* baseObject = objectList->findObjectFromPart(tos->integer);

    if (!isDamageableScenery(baseObject))
    {
        tos->integer = 0;
        getCodeToken();
        return IntegerTypePtr;
    }

    GameObject* object = static_cast<GameObject*>(baseObject);
    double damage = object->getDamage();
    uint32_t damageLevel;

    // Original behaviour: an unknown kind of misc terrain object gives its raw damage times 100.
    if (getDamageLevel(object, damageLevel))
    {
        damage = damage / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    tos->integer = static_cast<int32_t>(std::floor(damage * 100.0));
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetObjDmgPts(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (isDamageableScenery(object))
    {
        tos->integer = static_cast<int32_t>(static_cast<GameObject*>(object)->getDamage());
    }
    else
    {
        tos->integer = 0;
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetMaxDmg(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* object = objectList->findObjectFromPart(tos->integer);
    uint32_t damageLevel = 0;

    if (isDamageableScenery(object))
    {
        getDamageLevel(static_cast<GameObject*>(object), damageLevel);
    }

    tos->integer = static_cast<int32_t>(damageLevel);
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetObjDamage(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t percent = nextInteger();

    if (percent > 100)
    {
        percent = 100;
    }

    BaseObject* baseObject = objectList->findObjectFromPart(partId);

    if (baseObject && baseObject->getObjectType() && percent > 0)
    {
        // Raises the damage to percent of the damage level (it never lowers it).
        GameObject* object = static_cast<GameObject*>(baseObject);
        uint32_t damageLevel;

        if (getDamageLevel(object, damageLevel))
        {
            float currentDamage = object->getDamage();
            float extraDamage = static_cast<float>(static_cast<double>(percent) * 0.01 *
                                                       static_cast<float>(static_cast<int32_t>(damageLevel)) -
                                                   currentDamage);

            if (extraDamage > 0.0f)
            {
                _WeaponShotInfo shotInfo;
                shotInfo.init(nullptr, -1, extraDamage, 0, 0.0f);
                applyShot(object, &shotInfo);
            }
        }
    }

    getCodeToken();
}

auto execHbDamageObject(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t attackerId = nextInteger();
    int32_t weaponMasterId = nextInteger();
    float damage = nextReal();
    int32_t hitLocation = nextInteger();
    getCodeToken();
    execExpression();
    pop();
    getCodeToken();
    execExpression();
    float entryAngle = tos->real;

    GameObject* attacker = findObject(attackerId);

    if (!attacker)
    {
        tos->integer = -1;
        getCodeToken();
        return IntegerTypePtr;
    }

    _WeaponShotInfo shotInfo;

    if (!isGroupId(partId))
    {
        GameObject* target = findObject(partId);

        if (!target)
        {
            tos->integer = -2;
            getCodeToken();
            return IntegerTypePtr;
        }

        shotInfo.init(attacker, weaponMasterId, damage, hitLocation, entryAngle);
        applyShot(target, &shotInfo);
        tos->integer = 1;
        getCodeToken();
        return IntegerTypePtr;
    }

    int32_t numMovers = getGroupMovers(partId);
    shotInfo.init(attacker, weaponMasterId, damage, hitLocation, entryAngle);

    for (int32_t i = 0; i < numMovers; i++)
    {
        if (!applyShot(moverList[i], &shotInfo))
        {
            break;
        }
    }

    tos->integer = numMovers;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetGlobalValue(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t index = tos->integer;
    tos->integer = 0;

    if (index > -1 && index < 50)
    {
        tos->real = globalMissionValues[index];
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetGlobalValue(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t index = tos->integer;
    pop();
    // Original behaviour: the value is stored as a real even when the script passed an integer.
    float value = nextReal();

    if (index > -1 && index < 50)
    {
        globalMissionValues[index] = value;
    }

    getCodeToken();
}

auto execHbSetObjectivePos(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t objectiveNumber = tos->integer;
    pop();
    float x = nextReal();
    float y = nextReal();
    float z = nextReal();
    scenario->setObjectivePos(objectiveNumber, x, y, z);
    getCodeToken();
}

auto execHbSetTonnage(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float tonnage = nextReal();

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            object->setTonnage(tonnage);
        }
    }

    getCodeToken();
}

auto execHbSetSensorRange(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    float range = tos->real;
    tos->integer = 0;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            switch (object->objectClass)
            {
                case BATTLEMECH:
                case GROUNDVEHICLE:
                case ELEMENTAL:
                {
                    if (static_cast<Mover*>(object)->sensorSystem)
                    {
                        static_cast<Mover*>(object)->sensorSystem->setRange(range);
                    }
                    break;
                }
                case ARTILLERY:
                {
                    static_cast<Artillery*>(object)->sensorRange = range;
                    static_cast<Artillery*>(object)->sensorSystem->setRange(range);
                    break;
                }
                case BUILDING:
                {
                    if (static_cast<Building*>(object)->sensorSystem)
                    {
                        static_cast<Building*>(object)->sensorSystem->setRange(range);
                    }
                    else
                    {
                        tos->integer = -1;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    getCodeToken();
}

auto execHbSetExplDmg(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float damage = nextReal();

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            object->setExplDmg(damage);
        }
    }

    getCodeToken();
}

auto execHbSetExplRad(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float radius = nextReal();

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            object->setExplRad(radius);
        }
    }

    getCodeToken();
}

auto execHbSetSalvage(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t itemId = nextInteger();
    getCodeToken();
    execExpression();
    int32_t numItems = tos->integer;
    int added = 0;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            SalvageItem* last = object->getSalvage();
            SalvageItem* item = new (std::nothrow) SalvageItem;

            if (item)
            {
                item->next = nullptr;
                added = 1;
                item->itemId = static_cast<uint8_t>(itemId);
                item->numItems = static_cast<uint8_t>(numItems);

                while (last && last->next)
                {
                    last = last->next;
                }

                if (object->getSalvage() == nullptr)
                {
                    object->setSalvage(item);
                }
                else
                {
                    last->next = item;
                }
            }
        }
    }

    tos->integer = added;
    getCodeToken();
    return BooleanTypePtr;
}

auto execHbSetSalvageStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t status = tos->integer;
    int result = 0;

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object && Terrain::terrainTacticalMap && (isMover(object) || object->isBuilding()))
        {
            if (status == 1)
            {
                result = Terrain::terrainTacticalMap->AddSalvage(object);
            }
            else
            {
                result = Terrain::terrainTacticalMap->RemoveSalvage(object, 1);
            }
        }
    }

    tos->integer = result;
    getCodeToken();
    return BooleanTypePtr;
}

auto execHbSetAnimation(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    uint32_t state = static_cast<uint32_t>(nextInteger());
    int32_t subState = nextInteger();

    if (!isGroupId(partId))
    {
        GameObject* object = findObject(partId);

        if (object)
        {
            if (object->objectClass == BUILDING)
            {
                auto* buildingAppearance =
                    static_cast<VFXBuildingAppearance*>(static_cast<Building*>(object)->appearance);

                if (state >= buildingAppearance->buildType->numAnimStates)
                {
                    buildingAppearance->animState = -1;
                }
                else
                {
                    buildingAppearance->animState = static_cast<int32_t>(state);
                }

                buildingAppearance->currentFrame = 0;
            }
            else if (object->objectClass == TREEBUILDING)
            {
                static_cast<VFXAppearance*>(static_cast<TreeBuilding*>(object)->appearance)
                    ->setTypeId(static_cast<ActorState>(state), static_cast<uint8_t>(subState));
            }
        }
    }

    getCodeToken();
}

auto execHbPlayWave(SymTableNodePtr routineIdPtr) -> void
{
    // Original behaviour (OB-045): only the first of the two arguments is read; the code pointer is left on the
    // comma before the second.
    getCodeToken();
    getCodeToken();
    execExpression();
    pop();
    getCodeToken();
}

auto execHbSetRevealed(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t teamId = tos->integer;
    pop();
    // Original behaviour: the radius is read as a real even when the script passed an integer.
    float radius = nextReal();
    getCodeToken();
    float* position = reinterpret_cast<float*>(nextReference());
    pop();
    vector_3d looker;
    looker.x = position[0];
    looker.y = position[1];
    looker.z = 0.0f;
    vector_3d lookVector;
    lookVector.x = 0.0f;
    lookVector.y = 0.0f;
    lookVector.z = 0.0f;
    land->markRadiusSeen(looker, lookVector, 360.0f, radius, static_cast<uint8_t>(teamId));

    if (teamId == 1)
    {
        innerSphereTeam->scanBattlefield();
    }
    else
    {
        clanTeam->scanBattlefield();
    }

    if (alliedTeam)
    {
        alliedTeam->scanBattlefield();
    }

    getCodeToken();
}

auto execHbGetSalvage(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t listSize = nextInteger();
    getCodeToken();
    int32_t* itemIds = reinterpret_cast<int32_t*>(nextReference());
    pop();
    getCodeToken();
    int32_t* itemCounts = reinterpret_cast<int32_t*>(nextReference());
    pop();

    for (int32_t i = 0; i < listSize; i++)
    {
        itemIds[i] = -1;
        itemCounts[i] = -1;
    }

    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object)
    {
        int32_t i = 0;

        for (SalvageItem* item = static_cast<GameObject*>(object)->getSalvage(); item && i < listSize;
             item = item->next, i++)
        {
            itemIds[i] = item->itemId;
            itemCounts[i] = item->numItems;
        }
    }

    getCodeToken();
}

auto execHbRefit(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t targetId = tos->integer;
    pop();
    uint32_t params = static_cast<uint32_t>(nextInteger());

    if (CurObject && isMover(CurObject))
    {
        MechWarrior* pilot = CurObject->getPilot();

        if (pilot)
        {
            BaseObject* target = objectList->findObjectFromPart(targetId);

            if (target && target->objectClass == BATTLEMECH)
            {
                pilot->orderRefit(1, static_cast<GameObject*>(target), params);
            }
        }
    }

    getCodeToken();
}

auto execHbSetCaptured(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object)
    {
        static_cast<GameObject*>(object)->setCaptured();
    }

    getCodeToken();
}

auto execHbCaptureObject(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t targetId = tos->integer;
    pop();
    uint32_t params = static_cast<uint32_t>(nextInteger());
    // Port fix: the original leaves the target register unset when the current object isn't a mover (and then
    // orders its pilot anyway).
    BaseObject* target = nullptr;

    if (CurObject && isMover(CurObject))
    {
        target = objectList->findObjectFromPart(targetId);
    }

    if (target)
    {
        CurObject->getPilot()->orderCapture(1, static_cast<GameObject*>(target), params);
    }

    getCodeToken();
}

auto execHbSetCaptureable(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    int32_t captureable = nextInteger() == 1 ? 1 : 0;
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object)
    {
        if (MPlayer)
        {
            static_cast<GameObject*>(object)->clearCaptured();
        }

        switch (object->objectClass)
        {
            case GROUNDVEHICLE:
                static_cast<GroundVehicle*>(object)->captureable = captureable;
                break;
            case BUILDING:
                static_cast<Building*>(object)->captureable = captureable;
                break;
            case TREEBUILDING:
                static_cast<TreeBuilding*>(object)->captureable = captureable;
                break;
            case TURRET:
                // Original behaviour (OB-044): a turret's flag goes where tree buildings keep theirs, +0x110,
                // which is the turret's lastFireTime.
                static_cast<Turret*>(object)->lastFireTime = std::bit_cast<float>(captureable);
                break;
            default:
                break;
        }
    }

    getCodeToken();
}

auto execHbIsCaptured(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    int32_t numCaptured = 0;

    if (isGroupId(partId))
    {
        int32_t numMovers = getGroupMovers(partId);

        for (int32_t i = 0; i < numMovers; i++)
        {
            if (moverList[i]->isCaptured())
            {
                numCaptured++;
            }
        }
    }
    else
    {
        BaseObject* object = objectList->findObjectFromPart(partId);

        if (object && static_cast<GameObject*>(object)->isCaptured())
        {
            numCaptured = 1;
        }
    }

    tos->integer = numCaptured;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbIsCapturable(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int captureable = 0;
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object)
    {
        captureable = static_cast<GameObject*>(object)->isCaptureable();
    }

    tos->integer = captureable != 0 ? 1 : 0;
    getCodeToken();
    return BooleanTypePtr;
}

auto execHbWasEverCapturable(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t captureable = 0;
    BaseObject* object = objectList->findObjectFromPart(tos->integer);

    if (object)
    {
        switch (object->objectClass)
        {
            case GROUNDVEHICLE:
                captureable = static_cast<GroundVehicle*>(object)->captureable;
                break;
            case BUILDING:
                captureable = static_cast<Building*>(object)->captureable;
                break;
            case TREEBUILDING:
                captureable = static_cast<TreeBuilding*>(object)->captureable;
                break;
            case TURRET:
                // Original behaviour (OB-044): reads the turret's lastFireTime bits.
                captureable = std::bit_cast<int32_t>(static_cast<Turret*>(object)->lastFireTime);
                break;
            default:
                break;
        }
    }

    tos->integer = captureable != 0 ? 1 : 0;
    getCodeToken();
    return BooleanTypePtr;
}

namespace
{
    /// <summary>Replaces <paramref name="name"/> (systemHeap) with a copy of string resource
    /// <paramref name="stringId"/>.</summary>
    auto setNameFromResource(char*& name, uint32_t stringId) -> void
    {
        char buffer[256];
        cLoadString(thisInstance, stringId, buffer, 0xfe);

        if (name)
        {
            systemHeap->free(name);
        }

        name = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(std::strlen(buffer) + 1)));

        if (name)
        {
            std::strcpy(name, buffer);
        }
    }

    /// <summary>What <c>__ftol</c> gives: the value truncated, or 0x80000000 for NaN or out of range.</summary>
    auto x87Ftol(double value) -> int32_t
    {
        if (!(value > -2147483649.0 && value < 2147483648.0))
        {
            return INT32_MIN;
        }

        return static_cast<int32_t>(value);
    }
}

auto execHbSetBuildingName(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    uint32_t stringId = static_cast<uint32_t>(nextInteger());
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && static_cast<GameObject*>(object)->isBuilding())
    {
        if (object->objectClass == BUILDING)
        {
            setNameFromResource(static_cast<Building*>(object)->name, stringId);
        }

        if (object->objectClass == TREEBUILDING)
        {
            setNameFromResource(static_cast<TreeBuilding*>(object)->name, stringId);
        }

        if (object->objectClass == TURRET)
        {
            setNameFromResource(static_cast<Turret*>(object)->name, stringId);
        }
    }

    getCodeToken();
}

namespace
{
    /// <summary>callstrike / callstrikeex: an artillery strike on an object, or on a point at ground level.</summary>
    auto callStrike(int32_t strikeType, int32_t targetId, vector_3d& position, int forClansOnPoint,
                    int forClansOnTarget, float delay) -> void
    {
        GameObject* target = static_cast<GameObject*>(objectList->findObjectFromPart(targetId));

        if (!target)
        {
            position.z = land->getTerrainElevation(position);
            theInterface->CallStrike(strikeType, &position, nullptr, 0, forClansOnPoint, delay);
        }
        else
        {
            theInterface->CallStrike(strikeType, nullptr, target, 0, forClansOnTarget, delay);
        }
    }
}

auto execHbCallStrike(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    getCodeToken();
    execExpression();
    int32_t strikeType = tos->integer;
    pop();
    int32_t targetId = nextInteger();
    vector_3d position;
    position.x = nextReal();
    position.y = nextReal();
    position.z = nextReal();
    int forClans = nextInteger() == 1 ? 1 : 0;
    // Original behaviour: the clan flag only counts for a strike on a point.
    callStrike(strikeType, targetId, position, forClans, 0, -1.0f);
    getCodeToken();
}

auto execHbCallStrikeEx(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();

    if (MPlayer)
    {
        Fatal(0, " ABL: Calling ArtilleryStrike in Multiplayer game ");
    }

    getCodeToken();
    execExpression();
    int32_t strikeType = tos->integer;
    pop();
    int32_t targetId = nextInteger();
    vector_3d position;
    position.x = nextReal();
    position.y = nextReal();
    position.z = nextReal();
    int forClans = nextInteger() == 1 ? 1 : 0;
    float delay = nextReal();

    if (delay < 0.0f)
    {
        delay = 0.0f;
    }

    callStrike(strikeType, targetId, position, forClans, forClans, delay);
    getCodeToken();
}

auto execHbLoadElementals(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t carrierId = tos->integer;
    pop();

    if (CurObject && CurObject->objectClass == ELEMENTAL)
    {
        BaseObject* carrier = objectList->findObjectFromPart(carrierId);

        if (carrier && carrier->objectClass == GROUNDVEHICLE &&
            static_cast<GroundVehicle*>(carrier)->elementalCarrier != 0)
        {
            CurObject->getPilot()->orderLoadIntoCarrier(1, static_cast<GameObject*>(carrier), 0);
        }
    }

    getCodeToken();
}

auto execHbDeployElementals(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    uint32_t params = static_cast<uint32_t>(tos->integer);
    pop();

    if (CurObject && CurObject->objectClass == GROUNDVEHICLE &&
        static_cast<GroundVehicle*>(CurObject)->elementalCarrier != 0)
    {
        CurObject->getPilot()->orderDeployElementals(1, params);
    }

    getCodeToken();
}

auto execHbAddPrisoner(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t buildingId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t pilotIndex = tos->integer;
    int32_t result = -1;
    BaseObject* object = objectList->findObjectFromPart(buildingId);

    if (object && static_cast<GameObject*>(object)->isBuilding() && scenario)
    {
        // Port fix: with no warriors at all the original fills the prison with the pointer -1.
        MechWarrior* prisoner = nullptr;

        for (uint32_t i = 1; i <= scenario->numWarriors; i++)
        {
            MechWarrior* warrior = scenario->warriors[i];

            if (warrior && warrior->index == pilotIndex)
            {
                prisoner = warrior;
                break;
            }
        }

        if (prisoner)
        {
            // Original behaviour (OB-046): the prisoner goes into every empty slot, not just the first.
            MechWarrior** prisonSlots = nullptr;

            if (object->objectClass == BUILDING)
            {
                prisonSlots = static_cast<Building*>(object)->prisonSlots;
            }
            else if (object->objectClass == TREEBUILDING)
            {
                prisonSlots = static_cast<TreeBuilding*>(object)->prisonSlots;
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

    tos->integer = result;
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbSetTrainSpeed(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float speed = nextReal();
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && object->objectClass == TRAINCAR)
    {
        Train* train = static_cast<TrainCar*>(object)->train;

        if (std::fabs(speed) > train->maxSpeed)
        {
            speed = speed > 0.0f ? train->maxSpeed : -train->maxSpeed;
        }

        train->desiredSpeed = speed;
    }

    getCodeToken();
}

namespace
{
    /// <summary>lockgateopen / lockgateclosed / releasegatelock: sets a gate's two lock flags.</summary>
    auto setGateLocks(int32_t blownOpen, int32_t lockedClosed) -> void
    {
        getCodeToken();
        getCodeToken();
        execExpression();
        int32_t partId = tos->integer;
        pop();
        BaseObject* object = objectList->findObjectFromPart(partId);

        if (object && object->objectClass == GATE)
        {
            static_cast<Gate*>(object)->blownOpen = blownOpen;
            static_cast<Gate*>(object)->lockedClosed = lockedClosed;
        }

        getCodeToken();
    }
}

auto execHbLockGateOpen(SymTableNodePtr routineIdPtr) -> void
{
    setGateLocks(1, 0);
}

auto execHbLockGateClosed(SymTableNodePtr routineIdPtr) -> void
{
    setGateLocks(0, 1);
}

auto execHbReleaseGateLock(SymTableNodePtr routineIdPtr) -> void
{
    setGateLocks(0, 0);
}

auto execHbIsGateOpen(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    tos->integer = 0;

    if (!isGroupId(partId))
    {
        BaseObject* object = objectList->findObjectFromPart(partId);

        if (object && object->objectClass == GATE)
        {
            tos->integer = static_cast<Gate*>(object)->isOpen != 0 ? 1 : 0;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>How much of a pilot's worth is left by wounds (0 to 6).</summary>
    constexpr float WoundEffectiveness[7] = {1.0f, 0.95f, 0.85f, 0.75f, 0.5f, 0.3f, 0.0f};

    /// <summary>An armor location's share left, scaled into 0.4 .. 1.0.</summary>
    auto armorFactor(const ArmorLocation& location) -> double
    {
        return static_cast<double>(location.curArmor) / static_cast<double>(location.maxArmor) * 0.6 + 0.4;
    }

    /// <summary>getunitstatus of a mech: its armor state times its weapon effectiveness (without the pilot).</summary>
    auto mechStatus(Mover* mech) -> float
    {
        // Armor locations: 0 head, 1 center torso, 2 / 3 arms, 4 / 5 side torsos, 8 rear center torso, 9 / 10 legs.
        ArmorLocation* armor = mech->armor;
        float centerArmor = armor[1].curArmor;
        uint8_t centerMax = armor[1].maxArmor;

        if (centerArmor > armor[8].curArmor)
        {
            centerArmor = armor[8].curArmor;
            centerMax = armor[8].maxArmor;
        }

        double head = armorFactor(armor[0]);
        double sides = static_cast<double>(armor[5].curArmor + armor[4].curArmor) /
                           static_cast<double>(armor[5].maxArmor + armor[4].maxArmor) * 0.25 +
                       0.75;
        float sidesFactor = static_cast<float>(sides);
        int32_t limbMax = armor[10].maxArmor + armor[9].maxArmor + armor[3].maxArmor + armor[2].maxArmor;
        double limbs =
            static_cast<double>(armor[10].curArmor + armor[9].curArmor + armor[3].curArmor + armor[2].curArmor) /
                static_cast<double>(limbMax) * 0.25 +
            0.75;
        double center = (static_cast<double>(centerArmor) / static_cast<double>(centerMax) + 1.0) * 0.5;
        return static_cast<float>(center * limbs * sidesFactor * sidesFactor * static_cast<float>(head));
    }

    /// <summary>getunitstatus of a ground vehicle: the product of its five armor locations' factors.</summary>
    auto vehicleStatus(Mover* vehicle) -> float
    {
        ArmorLocation* armor = vehicle->armor;
        double turret = 1.0;

        if (armor[4].maxArmor != 0)
        {
            turret = armorFactor(armor[4]);
        }

        return static_cast<float>(turret * armorFactor(armor[0]) * armorFactor(armor[1]) * armorFactor(armor[2]) *
                                  armorFactor(armor[3]));
    }

    /// <summary>The share of <paramref name="damageLevel"/> the damage leaves (at least 0).</summary>
    auto healthLeft(GameObject* object, uint32_t damageLevel) -> double
    {
        float maxDamage = static_cast<float>(static_cast<int32_t>(damageLevel));
        float left = maxDamage - object->getDamage();

        if (left < 0.0f)
        {
            left = 0.0f;
        }

        return static_cast<double>(left) / static_cast<double>(static_cast<int32_t>(damageLevel));
    }

    /// <summary>The damage taken as a share of <paramref name="damageLevel"/>, both truncated, capped at 1.</summary>
    auto damageTaken(GameObject* object, int32_t damageLevel) -> double
    {
        int32_t damage = x87Ftol(object->getDamage());

        if (damage > damageLevel)
        {
            damage = damageLevel;
        }

        return static_cast<double>(damage) / static_cast<double>(damageLevel);
    }
}

auto execHbGetUnitStatus(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    BaseObject* baseObject = objectList->findObjectFromPart(tos->integer);
    tos->integer = 0;

    if (baseObject)
    {
        GameObject* object = static_cast<GameObject*>(baseObject);
        // Port fix: other classes scale an uninitialized local in the original.
        double status = 0.0;

        switch (object->objectClass)
        {
            case BATTLEMECH:
            case GROUNDVEHICLE:
            {
                Mover* mover = static_cast<Mover*>(object);
                float weaponShare;
                float armorStatus;

                if (object->objectClass == BATTLEMECH)
                {
                    weaponShare = mover->weaponEffectiveness / mover->maxWeaponEffectiveness;
                    armorStatus = mechStatus(mover);
                }
                else
                {
                    weaponShare = mover->maxWeaponEffectiveness == 0.0f
                                      ? 1.0f
                                      : mover->weaponEffectiveness / mover->maxWeaponEffectiveness;
                    armorStatus = vehicleStatus(mover);
                }

                // Port fix: wounds past 6 index past the table in the original.
                int32_t wounds = std::clamp(x87Ftol(object->getPilot()->wounds), 0, 6);
                float pilotShare = WoundEffectiveness[wounds];

                if (object->isDestroyed() || object->isDisabled())
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
                status = healthLeft(object, static_cast<BuildingType*>(object->getObjectType())->dmgLevel);
                break;
            case TREEBUILDING:
                status = healthLeft(object, static_cast<TreeBuildingType*>(object->getObjectType())->dmgLevel);
                break;
            case MISCTERRAINOBJECT:
            {
                uint32_t damageLevel = 0;
                // Original behaviour: an unknown kind divides 0 by 0.
                getDamageLevel(object, damageLevel);
                status = 1.0 - damageTaken(object, static_cast<int32_t>(damageLevel));
                break;
            }

            case TRAINCAR:
                // Original behaviour (OB-047): a train car reports the damage taken, not the health left.
                status = damageTaken(object, static_cast<TrainCarType*>(object->getObjectType())->damage);
                break;
            case TURRET:
                status = 1.0 - damageTaken(object, static_cast<int32_t>(
                                                       static_cast<TurretType*>(object->getObjectType())->dmgLevel));
                break;
            case GATE:
                status =
                    1.0 - damageTaken(object,
                                      static_cast<int32_t>(static_cast<GateType*>(object->getObjectType())->dmgLevel));
                break;
            default:
                break;
        }

        tos->real = static_cast<float>(status * 100.0);
    }

    getCodeToken();
    return RealTypePtr;
}

auto execHbRelPosPoint(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    float* point = reinterpret_cast<float*>(nextReference());
    pop();
    float angle = nextReal();
    float distance = nextReal();
    uint32_t flags = static_cast<uint32_t>(nextInteger());
    getCodeToken();
    float* result = reinterpret_cast<float*>(nextReference());
    pop();
    vector_3d start;
    start.x = point[0];
    start.y = point[1];
    start.z = 0.0f;
    vector_3d position = relativePositionToPoint(start, angle, distance, flags);
    result[0] = position.x;
    result[1] = position.y;
    result[2] = position.z;
    getCodeToken();
}

auto execHbRelPosObject(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float angle = nextReal();
    float distance = nextReal();
    uint32_t flags = static_cast<uint32_t>(nextInteger());
    getCodeToken();
    float* result = reinterpret_cast<float*>(nextReference());
    pop();
    GameObject* object = findObject(partId);

    if (object)
    {
        vector_3d position = object->relativePosition(angle, distance, flags);
        result[0] = position.x;
        result[1] = position.y;
        result[2] = position.z;
    }

    getCodeToken();
}

namespace
{
    /// <summary>The damage state of the body location an armor location covers (the rear locations, from
    /// numBodyLocations on, cover the torsos from 1 on).</summary>
    auto armorLocationState(Mover* mover, int32_t armorIndex) -> uint8_t
    {
        if (armorIndex < mover->numBodyLocations)
        {
            return mover->bodyAt(armorIndex).damageState;
        }

        return mover->bodyAt(armorIndex - mover->numBodyLocations + 1).damageState;
    }
}

auto execHbRepair(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    float points = nextReal();
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && object->objectClass == BATTLEMECH)
    {
        // Fills internal structure first, then armor, location by location, skipping destroyed ones.
        Mover* mech = static_cast<Mover*>(object);

        for (int32_t i = 0; i < mech->numBodyLocations; i++)
        {
            BodyLocation& location = mech->bodyAt(i);
            float needed = static_cast<float>(location.maxInternalStructure) - location.curInternalStructure;

            if (location.damageState != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.curInternalStructure += points;
                    points = 0.0f;
                    break;
                }

                points -= needed;
                location.curInternalStructure = static_cast<float>(location.maxInternalStructure);
            }
        }

        for (int32_t i = 0; i < mech->numArmorLocations; i++)
        {
            ArmorLocation& location = mech->armor[i];
            float needed = static_cast<float>(location.maxArmor) - location.curArmor;

            if (armorLocationState(mech, i) != 2 && needed > 0.0f)
            {
                if (points <= needed)
                {
                    location.curArmor += points;
                    break;
                }

                points -= needed;
                location.curArmor = static_cast<float>(location.maxArmor);
            }
        }
    }

    getCodeToken();
}

auto execHbGetRepairState(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t partId = tos->integer;
    pop();
    // The percentage of internal structure and armor left, over the locations that aren't destroyed.
    float current = 0.0f;
    int32_t maximum = 0;
    BaseObject* object = objectList->findObjectFromPart(partId);

    if (object && isMover(object))
    {
        Mover* mover = static_cast<Mover*>(object);
        double sum = current;

        for (int32_t i = 0; i < mover->numBodyLocations; i++)
        {
            if (mover->bodyAt(i).damageState != 2)
            {
                sum += mover->bodyAt(i).curInternalStructure;
                maximum += mover->bodyAt(i).maxInternalStructure;
            }
        }

        for (int32_t i = 0; i < mover->numArmorLocations; i++)
        {
            if (armorLocationState(mover, i) != 2)
            {
                sum += mover->armor[i].curArmor;
                maximum += mover->armor[i].maxArmor;
            }
        }

        // Original behaviour: with no locations (or no object) this is 0 / 0, which __ftol turns into 0x80000000.
        pushInteger(x87Ftol(sum * 100.0 / static_cast<double>(maximum)));
    }
    else
    {
        pushInteger(x87Ftol(static_cast<double>(current) * 100.0 / static_cast<double>(maximum)));
    }

    getCodeToken();
    return IntegerTypePtr;
}

auto execHbIsTeamTargeting(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t teamId = tos->integer;
    pop();
    uint32_t targetId = static_cast<uint32_t>(nextInteger());
    uint32_t exceptId = static_cast<uint32_t>(nextInteger());
    Team* team = nullptr;

    if (teamId == 500)
    {
        team = innerSphereTeam;
    }
    else if (teamId == 0x1f6)
    {
        team = alliedTeam;
    }
    else if (teamId == 0x1f5)
    {
        team = clanTeam;
    }

    int targeting = 0;

    if (team)
    {
        targeting = team->isTargeting(targetId, exceptId);
    }

    pushInteger(targeting != 0 ? 1 : 0);
    getCodeToken();
    return BooleanTypePtr;
}

auto execHbGetFixed(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t moverId = tos->integer;
    pop();
    int32_t bayId = nextInteger();
    uint32_t params = static_cast<uint32_t>(nextInteger());

    // -1 ordered, 0 order refused, 1 bay out of points, 2 wrong kind of bay, 3 already this bay's, 4 bay busy,
    // 5 already being fixed, 6 needs nothing, 7 not a mech or vehicle, 8 not a repair bay, 9 other side.
    int32_t result = -1;
    BaseObject* bayObject = objectList->findObjectFromPart(bayId);

    if (!bayObject || bayObject->objectClass != TREEBUILDING || static_cast<TreeBuilding*>(bayObject)->canRefit == 0)
    {
        result = 8;
    }
    else
    {
        TreeBuilding* bay = static_cast<TreeBuilding*>(bayObject);

        if (bay->getRefitPoints() > 0.0f)
        {
            BaseObject* moverObject = objectList->findObjectFromPart(moverId);

            if (!moverObject || (moverObject->objectClass != BATTLEMECH && moverObject->objectClass != GROUNDVEHICLE))
            {
                result = 7;
            }
            else
            {
                Mover* mover = static_cast<Mover*>(moverObject);

                if (bay->getAlignment() != mover->getAlignment())
                {
                    result = 9;
                }
                else if (bay->refitBuddy)
                {
                    result = (bay->refitBuddy != mover) ? 4 : 3;
                }
                else
                {
                    // A mech bay fixes mechs only, a vehicle bay vehicles only.
                    bool rightBay = (mover->objectClass == BATTLEMECH) == (bay->mechBay != 0);

                    if (!rightBay)
                    {
                        result = 2;
                    }
                    else if (mover->refitBuddy)
                    {
                        result = 5;
                    }
                    else if (mover->needsRefit(0) == 0)
                    {
                        result = 6;
                    }
                    else
                    {
                        result = mover->getPilot()->orderGetFixed(1, bay, params) != 0 ? -1 : 0;
                    }
                }
            }
        }
        else
        {
            result = 1;
        }
    }

    pushInteger(result);
    getCodeToken();
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

    auto* file = new File;
    file->create("scriptmsg.dbg");
    file->writeString(ChunkDebugMsg);
    file->close();
    delete file;
    ExceptionGameMsg = ChunkDebugMsg;
}

auto execHbSendMessage(SymTableNodePtr routineIdPtr) -> void
{
    getCodeToken();
    getCodeToken();
    execExpression();
    CurMultiplayCode = tos->integer;
    pop();
    CurMultiplayParam = nextInteger();

    if (MPlayer && MPlayer->isServer)
    {
        MPlayer->addMissionScriptMessageChunk(CurMultiplayCode, CurMultiplayParam);

        if (NumMissionScriptMessages == 1000)
        {
            DebugMissionScriptMessages();
            Assert(0, static_cast<uint32_t>(NumMissionScriptMessages), " Way too many Mission Script Messages! ");
        }

        MissionScriptMessageLog[NumMissionScriptMessages][0] = static_cast<int16_t>(execLineNumber);
        MissionScriptMessageLog[NumMissionScriptMessages][1] = static_cast<int16_t>(CurMultiplayCode);
        MissionScriptMessageLog[NumMissionScriptMessages][2] = static_cast<int16_t>(CurMultiplayParam);
        NumMissionScriptMessages++;
    }

    getCodeToken();
}

auto execHbGetMessage(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    int32_t* param = reinterpret_cast<int32_t*>(nextReference());
    pop();
    *param = CurMultiplayParam;
    pushInteger(CurMultiplayCode);
    getCodeToken();
    return IntegerTypePtr;
}

auto execHbGetStrikes(SymTableNodePtr routineIdPtr) -> TypePtr
{
    getCodeToken();
    getCodeToken();
    execExpression();
    int32_t commanderId = tos->integer;
    pop();
    getCodeToken();
    execExpression();
    int32_t strikeType = tos->integer;
    tos->integer = 0;

    if (commanderId > -1 && commanderId < NumCommanders && strikeType > -1)
    {
        Commander* commander = CommanderTable[commanderId];

        switch (strikeType)
        {
            case 0:
                tos->integer = commander->numSmallStrikes;
                break;
            case 1:
                tos->integer = commander->numLargeStrikes;
                break;
            case 2:
                tos->integer = commander->numSensorStrikes;
                break;
            case 3:
                tos->integer = commander->numCameraDrones;
                break;
            default:
                break;
        }
    }

    getCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>setstrikes / addstrikes: a commander's strikes of one type (0 small, 1 large, 2 sensor, 3 camera
    /// drones), set to <paramref name="count"/> or raised by it.</summary>
    auto changeStrikes(bool add) -> void
    {
        getCodeToken();
        getCodeToken();
        execExpression();
        int32_t commanderId = tos->integer;
        pop();
        int32_t strikeType = nextInteger();
        int32_t count = nextInteger();

        if (commanderId > -1 && commanderId < NumCommanders && strikeType > -1)
        {
            Commander* commander = CommanderTable[commanderId];

            switch (strikeType)
            {
                case 0:
                    commander->setNumSmallStrikes(add ? commander->numSmallStrikes + count : count);
                    break;
                case 1:
                    commander->setNumLargeStrikes(add ? commander->numLargeStrikes + count : count);
                    break;
                case 2:
                    commander->setNumSensorStrikes(add ? commander->numSensorStrikes + count : count);
                    break;
                case 3:
                    commander->setNumCameraDrones(add ? commander->numCameraDrones + count : count);
                    break;
                default:
                    break;
            }
        }

        getCodeToken();
    }
}

auto execHbSetStrikes(SymTableNodePtr routineIdPtr) -> void
{
    changeStrikes(false);
}

auto execHbAddStrikes(SymTableNodePtr routineIdPtr) -> void
{
    changeStrikes(true);
}

auto execHbIsServer(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(MPlayer && MPlayer->isServer ? 1 : 0);
    getCodeToken();
    return BooleanTypePtr;
}

auto execHbGetHomeTeam(SymTableNodePtr routineIdPtr) -> TypePtr
{
    pushInteger(homeTeam->id + 500);
    getCodeToken();
    return IntegerTypePtr;
}

auto execStandardRoutineCall(SymTableNodePtr routineIdPtr) -> TypePtr
{
    int32_t key = routineIdPtr->defn.info.routine.key;

    switch (key)
    {
        case RTN_RETURN:
        {
            execStdReturn(routineIdPtr);
            return nullptr;
        }
        case RTN_PRINT:
        {
            execStdPrint(routineIdPtr);
            return nullptr;
        }
        case RTN_CONCAT:
            return execStdConcat(routineIdPtr);
        case RTN_ABS:
            return execStdAbs(routineIdPtr);
        case RTN_ROUND:
            return execStdRound(routineIdPtr);
        case RTN_SQRT:
            return execStdSqrt(routineIdPtr);
        case RTN_TRUNC:
            return execStdTrunc(routineIdPtr);
        case RTN_RANDOM:
            return execStdRandom(routineIdPtr);
        case RTN_SET_MAX_LOOPS:
            return execStdSetMaxLoops(routineIdPtr);
        case RTN_FATAL:
            return execStdFatal(routineIdPtr);
        case RTN_ASSERT:
            return execStdAssert(routineIdPtr);
        case RTN_GET_MODULE_HANDLE:
            return execStdGetModHandle(routineIdPtr);
        case RTN_GET_ID:
            return execHbGetId(routineIdPtr);
        case RTN_GET_TIME:
            return execHbGetTime(routineIdPtr);
        case RTN_GET_TIME_LEFT:
            return execHbGetTimeLeft(routineIdPtr);
        case RTN_GET_WARRIOR_STATUS:
            return execHbGetWarriorStatus(routineIdPtr);
        case RTN_SELECT_UNIT:
            return execHbSelectUnit(routineIdPtr);
        case RTN_SELECT_WARRIOR:
            return execHbSelectWarrior(routineIdPtr);
        case RTN_SELECT_OBJECT:
            return execHbSelectObject(routineIdPtr);
        case RTN_GET_CONTACTS:
            return execHbGetContacts(routineIdPtr);
        case RTN_GET_ENEMY_COUNT:
            return execHbGetEnemyCount(routineIdPtr);
        case RTN_SELECT_CONTACT:
            return execHbSelectContact(routineIdPtr);
        case RTN_GET_CONTACT_ID:
            return execHbGetContactId(routineIdPtr);
        case RTN_IS_CONTACT:
            return execHbIsContact(routineIdPtr);
        case RTN_GET_CONTACT_STATUS:
            return execHbGetContactStatus(routineIdPtr);
        case RTN_GET_CONTACT_RELATIVE_POSITION:
            return execHbGetContactRelativePosition(routineIdPtr);
        case RTN_GET_TARGET:
            return execHbGetTarget(routineIdPtr);
        case RTN_SET_TARGET:
        {
            execHbSetTarget(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_WEAPONS_READY:
        case RTN_GET_WEAPONS_LOCKED:
        case RTN_GET_WEAPONS_IN_RANGE:
            return execHbGetWeapons(routineIdPtr, key);
        case RTN_GET_WEAPON_SHOTS:
            return execHbGetWeaponShots(routineIdPtr);
        case RTN_GET_WEAPON_RANGES:
        {
            execHbGetWeaponRanges(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_OBJECT_POSITION:
            return execHbGetObjectPosition(routineIdPtr);
        case RTN_GET_INTEGER_MEMORY:
            return execHbGetMemoryInteger(routineIdPtr);
        case RTN_GET_REAL_MEMORY:
            return execHbGetMemoryReal(routineIdPtr);
        case RTN_GET_ALARM_TRIGGERS:
            return execHbGetAlarmTriggers(routineIdPtr);
        case RTN_GET_CHALLENGER:
            return execHbGetChallenger(routineIdPtr);
        case RTN_GET_FIRE_RANGES:
            return execHbGetFireRanges(routineIdPtr);
        case RTN_GET_ATTACKERS:
            return execHbGetAttackers(routineIdPtr);
        case RTN_GET_ATTACKER_INFO:
            return execHbGetAttackerInfo(routineIdPtr);
        case RTN_SET_CHALLENGER:
            return execHbSetChallenger(routineIdPtr);
        case RTN_GET_TIME_WITHOUT_ORDERS:
            return execHbGetTimeWithoutOrders(routineIdPtr);
        case RTN_SET_RADIO:
        {
            execHbSetRadio(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_MOVE_GOAL:
            return execHbSetMoveGoal(routineIdPtr);
        case RTN_SET_INTEGER_MEMORY:
        {
            execHbSetMemoryInteger(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_REAL_MEMORY:
        {
            execHbSetMemoryReal(routineIdPtr);
            return nullptr;
        }
        case RTN_HAS_MOVE_GOAL:
            return execHbHasMoveGoal(routineIdPtr);
        case RTN_HAS_MOVE_PATH:
            return execHbHasMovePath(routineIdPtr);
        case RTN_SORT_WEAPONS:
        {
            execHbSortWeapons(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_VISUAL_RANGE:
            return execHbGetVisualRange(routineIdPtr);
        case RTN_GET_UNIT_MATES:
            return execHbGetUnitMates(routineIdPtr);
        case RTN_GET_TAC_ORDER:
            return execHbGetTacOrder(routineIdPtr);
        case RTN_GET_LAST_TAC_ORDER:
            return execHbGetLastTacOrder(routineIdPtr);
        case RTN_SET_ORDER_MODE:
            return execHbSetOrderMode(routineIdPtr);
        case RTN_ORDER_WAIT:
            return execHbWait(routineIdPtr);
        case RTN_ORDER_MOVE_TO:
            return execHbMoveToPoint(routineIdPtr);
        case RTN_ORDER_MOVE_TO_OBJECT:
            return execHbMoveToObject(routineIdPtr);
        case RTN_ORDER_MOVE_TO_CONTACT:
            return execHbMoveToContact(routineIdPtr);
        case RTN_ORDER_TRAVERSE_PATH:
        case RTN_ORDER_PATROL_PATH:
        case RTN_ATTACK_CLOSEST_TARGET:
        case RTN_ATTACK_PER_ORDERS:
        case RTN_RETREAT:
        case RTN_FIRE_UPON_ENEMY_FIRE_ONLY:
            // Original behaviour: these do nothing, not even read their call's tokens.
            return nullptr;
        case RTN_ORDER_POWER_UP:
            return execHbOrderPowerUp(routineIdPtr);
        case RTN_ORDER_POWER_DOWN:
            return execHbOrderPowerDown(routineIdPtr);
        case RTN_ORDER_ATTACK_OBJECT:
            return execHbOrderAttackObject(routineIdPtr);
        case RTN_ORDER_ATTACK_CONTACT:
            return execHbOrderAttackContact(routineIdPtr);
        case RTN_ORDER_WITHDRAW:
            return execHbObjWithdraw(routineIdPtr);
        case RTN_DAMAGE_OBJECT:
            return execHbDamageObject(routineIdPtr);
        case RTN_SET_ATTACK_RADIUS:
            return execHbSetAttackRadius(routineIdPtr);
        case RTN_ORDER_TEST:
            return execHbOrderTest(routineIdPtr);
        case RTN_PLAY_SMACKER:
            return execHbPlaySmacker(routineIdPtr);
        case RTN_OBJECT_CHANGE_SIDES:
        {
            execHbObjectChangeSides(routineIdPtr);
            return nullptr;
        }
        case RTN_DISTANCE_TO_OBJECT:
            return execHbDistanceToObject(routineIdPtr);
        case RTN_DISTANCE_TO_POSITION:
            return execHbDistanceToPosition(routineIdPtr);
        case RTN_OBJECT_SUICIDE:
        {
            execHbObjectSuicide(routineIdPtr);
            return nullptr;
        }
        case RTN_OBJECT_CREATE:
            return execHbObjectCreate(routineIdPtr);
        case RTN_OBJECT_EXISTS:
            return execHbObjectExists(routineIdPtr);
        case RTN_OBJECT_STATUS:
            return execHbObjectStatus(routineIdPtr);
        case RTN_OBJECT_VISIBLE:
            return execHbObjectVisible(routineIdPtr);
        case RTN_OBJECT_CLASS:
            return execHbObjectClass(routineIdPtr);
        case RTN_OBJECT_SIDE:
            return execHbObjectSide(routineIdPtr);
        case RTN_OBJECT_COMMANDER:
            return execHbObjectCommander(routineIdPtr);
        case RTN_SET_TIMER:
            return execHbSetTimer(routineIdPtr);
        case RTN_CHECK_TIMER:
            return execHbChkTimer(routineIdPtr);
        case RTN_END_TIMER:
        {
            execHbEndTimer(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_TIMER:
            return execHbSetObjectiveTimer(routineIdPtr);
        case RTN_CHECK_OBJECTIVE_TIMER:
            return execHbCheckObjectiveTimer(routineIdPtr);
        case RTN_SET_OBJECTIVE_STATUS:
            return execHbSetObjectiveStatus(routineIdPtr);
        case RTN_CHECK_OBJECTIVE_STATUS:
            return execHbCheckObjectiveStatus(routineIdPtr);
        case RTN_SET_OBJECTIVE_TYPE:
            return execHbSetObjectiveType(routineIdPtr);
        case RTN_CHECK_OBJECTIVE_TYPE:
            return execHbCheckObjectiveType(routineIdPtr);
        case RTN_PLAY_DIGITAL_MUSIC:
            return execHbPlayDigitalMusic(routineIdPtr);
        case RTN_STOP_MUSIC:
            return execHbStopMusic(routineIdPtr);
        case RTN_PLAY_SOUND_EFFECT:
            return execHbPlaySoundEffect(routineIdPtr);
        case RTN_PLAY_VIDEO:
            return execHbPlayVideo(routineIdPtr);
        case RTN_PLAY_SPEECH:
            return execHbPlaySpeech(routineIdPtr);
        case RTN_PLAY_BETTY:
            return execHbPlayBetty(routineIdPtr);
        case RTN_SET_OBJECT_ACTIVE:
            return execHbSetObjActive(routineIdPtr);
        case RTN_OBJECT_IN_WITHDRAWAL:
            return execHbObjInWithdraw(routineIdPtr);
        case RTN_OBJECT_TYPE_ID:
            return execHbObjTypeId(routineIdPtr);
        case RTN_GET_TERRAIN_OBJECT_PART_ID:
            return execHbTerrainObjectId(routineIdPtr);
        case RTN_GET_VEHICLE_PART_ID:
            return execHbVehicleId(routineIdPtr);
        case RTN_GET_WEAPON_AMMO:
            return execHbGetWeaponAmmo(routineIdPtr);
        case RTN_OBJECT_STATUS_COUNT:
            return execHbObjectStatusCount(routineIdPtr);
        case RTN_IN_AREA:
            return execHbInArea(routineIdPtr);
        case RTN_GET_RELATIVE_POSITION_TO_POINT:
        {
            execHbRelPosPoint(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_RELATIVE_POSITION_TO_OBJECT:
        {
            execHbRelPosObject(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_SENSORS_WORKING:
            return execHbGetSensors(routineIdPtr);
        case RTN_GET_CURRENT_BR_VALUE:
            return execHbGetBRValue(routineIdPtr);
        case RTN_GET_ARMOR_PTS:
            return execHbGetArmorPts(routineIdPtr);
        case RTN_GET_PILOT_ID:
            return execHbGetPilotId(routineIdPtr);
        case RTN_GET_PILOT_WOUNDS:
            return execHbGetPilotWounds(routineIdPtr);
        case RTN_SET_PILOT_WOUNDS:
        {
            execHbSetPilotWounds(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_OBJECT_ACTIVE:
            return execHbGetObjActive(routineIdPtr);
        case RTN_GET_OBJECT_MAX_DMG:
            // Original behaviour (OB-050): getobjectmaxdmg runs the damage points routine.
            return execHbGetObjDmgPts(routineIdPtr);
        case RTN_GET_OBJECT_DAMAGE:
            return execHbGetObjDamage(routineIdPtr);
        case RTN_SET_OBJECT_DAMAGE:
        {
            execHbSetObjDamage(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_GLOBAL_VALUE:
            return execHbGetGlobalValue(routineIdPtr);
        case RTN_SET_GLOBAL_VALUE:
        {
            execHbSetGlobalValue(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_OBJECTIVE_POS:
        {
            execHbSetObjectivePos(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_POTENTIAL_CONTACT:
            return execHbSetPotentialContact(routineIdPtr);
        case RTN_SET_SENSOR_RANGE:
        {
            execHbSetSensorRange(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_TONNAGE:
        {
            execHbSetTonnage(routineIdPtr);
            return nullptr;
        }
        case RTN_PLAY_WAVE_FILE:
        {
            execHbPlayWave(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_EXPLOSION_DAMAGE:
        {
            execHbSetExplDmg(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_EXPLOSION_RADIUS:
        {
            execHbSetExplRad(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_SALVAGE:
        {
            execHbGetSalvage(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_SALVAGE:
        {
            // Original behaviour: the boolean result type is dropped.
            execHbSetSalvage(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_SALVAGE_STATUS:
        {
            execHbSetSalvageStatus(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_ANIMATION:
        {
            execHbSetAnimation(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_REVEALED:
        {
            execHbSetRevealed(routineIdPtr);
            return nullptr;
        }
        case RTN_ORDER_REFIT:
        {
            execHbRefit(routineIdPtr);
            return nullptr;
        }
        case RTN_ORDER_CAPTURE:
        {
            execHbCaptureObject(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_CAPTURED:
        {
            execHbSetCaptured(routineIdPtr);
            return nullptr;
        }
        case RTN_SET_CAPTUREABLE:
        {
            execHbSetCaptureable(routineIdPtr);
            return nullptr;
        }
        case RTN_IS_CAPTURED:
            return execHbIsCaptured(routineIdPtr);
        case RTN_IS_CAPTURABLE:
            return execHbIsCapturable(routineIdPtr);
        case RTN_WAS_EVER_CAPTURABLE:
            return execHbWasEverCapturable(routineIdPtr);
        case RTN_SET_BUILDING_NAME:
        {
            execHbSetBuildingName(routineIdPtr);
            return nullptr;
        }
        case RTN_CALL_STRIKE:
        {
            execHbCallStrike(routineIdPtr);
            return nullptr;
        }
        case RTN_ORDER_LOAD_ELEMENTALS:
        {
            execHbLoadElementals(routineIdPtr);
            return nullptr;
        }
        case RTN_ORDER_DEPLOY_ELEMENTALS:
        {
            execHbDeployElementals(routineIdPtr);
            return nullptr;
        }
        case RTN_ADD_PRISONER:
            return execHbAddPrisoner(routineIdPtr);
        case RTN_SET_TRAIN_SPEED:
        {
            execHbSetTrainSpeed(routineIdPtr);
            return nullptr;
        }
        case RTN_LOCK_GATE_OPEN:
        {
            execHbLockGateOpen(routineIdPtr);
            return nullptr;
        }
        case RTN_LOCK_GATE_CLOSED:
        {
            execHbLockGateClosed(routineIdPtr);
            return nullptr;
        }
        case RTN_RELEASE_GATE_LOCK:
        {
            execHbReleaseGateLock(routineIdPtr);
            return nullptr;
        }
        case RTN_IS_GATE_OPEN:
            return execHbIsGateOpen(routineIdPtr);
        case RTN_CALL_STRIKE_EX:
        {
            execHbCallStrikeEx(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_UNIT_STATUS:
            return execHbGetUnitStatus(routineIdPtr);
        case RTN_REPAIR:
        {
            execHbRepair(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_FIXED:
            return execHbGetFixed(routineIdPtr);
        case RTN_GET_REPAIR_STATE:
            return execHbGetRepairState(routineIdPtr);
        case RTN_IS_TEAM_TARGETING:
            return execHbIsTeamTargeting(routineIdPtr);
        case RTN_SEND_MESSAGE:
        {
            execHbSendMessage(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_MESSAGE:
            return execHbGetMessage(routineIdPtr);
        case RTN_GET_HOME_TEAM:
            return execHbGetHomeTeam(routineIdPtr);
        case RTN_SET_STRIKES:
        {
            execHbSetStrikes(routineIdPtr);
            return nullptr;
        }
        case RTN_GET_STRIKES:
            return execHbGetStrikes(routineIdPtr);
        case RTN_IS_SERVER:
            return execHbIsServer(routineIdPtr);
        case RTN_ADD_STRIKES:
        {
            execHbAddStrikes(routineIdPtr);
            return nullptr;
        }
        default:
        {
            // Original behaviour (OB-050): among others the module name and mode routines, the guard routines, the scans, getmaxarmor,
            // getobjectdmgpts and setcurrentbrvalue compile but have no runtime routine.
            char message[256];
            std::snprintf(message, sizeof(message), " ABL: Undefined ABL RoutineKey in %s:%d", CurModule->getName(),
                          execLineNumber);
            Fatal(0, message);
        }
    }
}
