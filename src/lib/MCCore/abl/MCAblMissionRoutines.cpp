#include "stdafx.h"
#include "abl/MCAblRoutineList.h"
#include "abl/MCAblDebugger.h"
#include "ai/MCMoveSystem.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCDice.h"
#include "lib/MCFatal.h"
#include "lib/MCFile.h"
#include "lib/MCFrameOfRef.h"
#include "lib/MCVector2D.h"
#include "main/main.h"
#include "mission/MCScenario.h"
#include "network/multplyr.h"
#include "object/MCArtillery.h"
#include "object/MCArtilleryType.h"
#include "object/MCArtilleryChunk.h"
#include "object/MCCameraDrone.h"
#include "object/MCCameraDroneType.h"
#include "object/MCBuilding.h"
#include "object/MCBuildingType.h"
#include "object/MCBuildingMarines.h"
#include "object/MCObjectDrawing.h"
#include "object/MCMiscTerrainObject.h"
#include "object/MCMiscTerrainObjectType.h"
#include "object/MCMasterComponent.h"
#include "object/MCForces.h"
#include "object/MCContactSystem.h"
#include "object/MCBigGameObject.h"
#include "object/MCGate.h"
#include "object/MCGateType.h"
#include "object/MCMoverGroup.h"
#include "object/MCGroundVehicle.h"
#include "object/MCGroundVehicleType.h"
#include "object/MCGroundVehicleGameSystem.h"
#include "object/MCMover.h"
#include "object/MCMoverGameSystem.h"
#include "object/MCObjectSystem.h"
#include "object/MCObjectQueue.h"
#include "object/MCObjectType.h"
#include "object/MCTreeBuilding.h"
#include "object/MCTreeBuildingType.h"
#include "object/MCTerrainObject.h"
#include "object/MCTerrainObjectType.h"
#include "object/MCTrain.h"
#include "object/MCTrainCar.h"
#include "object/MCTrainCarType.h"
#include "object/MCTrainManager.h"
#include "object/MCTurret.h"
#include "object/MCTurretType.h"
#include "object/MCMechWarrior.h"
#include "sound/MCRadio.h"
#include "sound/MCSoundSystem.h"
#include "sprite/MCVfxAppearance.h"
#include "sprite/MCVfxBuildingAppearance.h"
#include "terrain/MCTacticalMap.h"
#include "terrain/MCTerrain.h"
#include "object/MCWeaponChunkDebug.h"

// The mission: timers, objectives, music, sound, video, radio, global values, multiplayer messages, strikes.

namespace
{
    /// <summary>
    /// The script's global values (getglobalvalue / setglobalvalue): kept, scripts address the slots by number (a
    /// slot out of range reads 0 and takes nothing). They last the whole session, as in MCX.EXE.
    /// </summary>
    std::array<float, AblGlobalValueCount> GlobalMissionValues{};

    /// <summary>The messages after which sendmessage reports "Way too many Mission Script Messages" (the original's log size).</summary>
    constexpr size_t MissionScriptMessageReport = 1000;
}

auto ExecHbSetTimer(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int16_t timerId = static_cast<int16_t>(abl.Top().Integer);
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();

    if (timerId < 7 || timerId > 14)
    {
        timerId = 0;
    }
    else
    {
        // Original behaviour: the time is read as a real even when the script passed an integer.
        GuiSystem()->AddTimer(GuiSystem(), timerId, static_cast<int32_t>(static_cast<double>(abl.Top().Real) * 1000.0),
                              0x1406, 0, 0);
    }

    abl.Top().Integer = timerId;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbChkTimer(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCGuiTimer* timer = GuiSystem()->TimerManager->GetTimer(GuiSystem(), static_cast<int16_t>(abl.Top().Integer));
    uint32_t remaining = 0;

    if (timer)
    {
        remaining = timer->Interval + timer->LastTime - MCPort::Milliseconds();
    }

    abl.Top().Real = static_cast<float>(static_cast<double>(remaining) * 0.001);
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbEndTimer(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int16_t timerId = static_cast<int16_t>(abl.Top().Integer);
    abl.Pop();

    if (timerId > 6 && timerId < 15)
    {
        GuiSystem()->RemoveTimer(GuiSystem(), timerId);
    }

    abl.GetCodeToken();
}

auto ExecHbSetObjectiveTimer(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t objectiveNumber = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = Scenario()->SetObjectiveTimer(objectiveNumber, abl.Top().Real * 1000.0f);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveTimer(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Real = Scenario()->CheckObjectiveTimer(abl.Top().Integer);
    abl.GetCodeToken();
    return RealTypePtr;
}

auto ExecHbSetObjectiveStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t objectiveNumber = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = Scenario()->Objectives.SetStatus(objectiveNumber, static_cast<uint32_t>(abl.Top().Integer));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveStatus(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = static_cast<int32_t>(Scenario()->Objectives.Status(abl.Top().Integer));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetObjectiveType(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t objectiveNumber = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = Scenario()->Objectives.SetType(objectiveNumber, static_cast<uint32_t>(abl.Top().Integer));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbCheckObjectiveType(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.Top().Integer = static_cast<int32_t>(Scenario()->Objectives.Type(abl.Top().Integer));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayDigitalMusic(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();

    if (SoundSystem())
    {
        SoundSystem()->PlayAblDigitalMusic(abl.Top().Integer);
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbStopMusic(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();

    if (SoundSystem())
    {
        SoundSystem()->StopAblMusic();
    }

    // Original behaviour: nothing was pushed, so this overwrites whatever is on top of the stack.
    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlaySoundEffect(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();

    if (SoundSystem())
    {
        SoundSystem()->PlayAblsfx(abl.Top().Integer);
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayVideo(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();

    if (SoundSystem())
    {
        SoundSystem()->PlayAblVideo(abl.Top().Integer);
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetRadio(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Pop();
    int32_t enable = abl.NextInteger();
    MCMechWarrior* warrior = FindWarrior(abl, warriorIndex);

    if (warrior && warrior->Radio)
    {
        warrior->Radio->Enabled = enable == 1;
    }

    abl.GetCodeToken();
}

auto ExecHbPlaySpeech(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t warriorIndex = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    MCMechWarrior* warrior = FindWarrior(abl, warriorIndex);

    if (warrior)
    {
        warrior->RadioMessage(abl.Top().Integer, 1);
    }

    abl.Top().Integer = 0;
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbPlayBetty(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    uint32_t bettyId = static_cast<uint32_t>(abl.Top().Integer);
    abl.Pop();
    abl.PushInteger(SoundSystem()->PlayBettySample(bettyId));
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetGlobalValue(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t index = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (index > -1 && index < AblGlobalValueCount)
    {
        abl.Top().Real = GlobalMissionValues[static_cast<size_t>(index)];
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbSetGlobalValue(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t index = abl.Top().Integer;
    abl.Pop();
    // Original behaviour: the value is stored as a real even when the script passed an integer.
    float value = abl.NextReal();

    if (index > -1 && index < AblGlobalValueCount)
    {
        GlobalMissionValues[static_cast<size_t>(index)] = value;
    }

    abl.GetCodeToken();
}

auto DebugMissionScriptMessages() -> void
{
    const MCAblRuntime* runtime = AblRuntime();
    const std::span<const MCAblMissionScriptMessage> messages =
        runtime ? std::span(runtime->MissionScriptMessages) : std::span<const MCAblMissionScriptMessage>{};
    std::string text = std::format("\n{} Mission Script Messages\n\n", messages.size());

    for (const MCAblMissionScriptMessage& message : messages)
    {
        text += std::format("line {:5}: {:5}, {:5}\n", message.Line, message.Code, message.Param);
    }

    // Port fix (OB-048): ChunkDebugMsg grows; the original's 1000 lines overran its 0x1400 bytes.
    ChunkDebugMsg = std::move(text);
    SaveChunkDebugMsg("scriptmsg.dbg");
}

auto ExecHbSendMessage(MCAblRuntime& abl) -> void
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    abl.MissionMessageCode = abl.Top().Integer;
    abl.Pop();
    abl.MissionMessageParam = abl.NextInteger();

    if (MPlayer && MPlayer->IsServer)
    {
        MPlayer->AddMissionScriptMessageChunk(abl.MissionMessageCode, abl.MissionMessageParam);

        // The original's log held 1000 messages and wrote past its end after this report; the port's grows.
        if (abl.MissionScriptMessages.size() == MissionScriptMessageReport)
        {
            DebugMissionScriptMessages();
            Assert(0, static_cast<uint32_t>(MissionScriptMessageReport), " Way too many Mission Script Messages! ");
        }

        abl.MissionScriptMessages.push_back({static_cast<int16_t>(abl.LineNumber()),
                                             static_cast<int16_t>(abl.MissionMessageCode),
                                             static_cast<int16_t>(abl.MissionMessageParam)});
    }

    abl.GetCodeToken();
}

auto ExecHbGetMessage(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    int32_t* param = reinterpret_cast<int32_t*>(abl.NextReference());
    abl.Pop();
    *param = abl.MissionMessageParam;
    abl.PushInteger(abl.MissionMessageCode);
    abl.GetCodeToken();
    return IntegerTypePtr;
}

auto ExecHbGetStrikes(MCAblRuntime& abl) -> MCAblType*
{
    abl.GetCodeToken();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t commanderId = abl.Top().Integer;
    abl.Pop();
    abl.GetCodeToken();
    abl.ExecExpression();
    int32_t strikeType = abl.Top().Integer;
    abl.Top().Integer = 0;

    if (commanderId > -1 && commanderId < NumCommanders() && strikeType > -1)
    {
        MCCommander* commander = CommanderById(commanderId);

        switch (strikeType)
        {
            case 0:
                abl.Top().Integer = commander->NumSmallStrikes;
                break;
            case 1:
                abl.Top().Integer = commander->NumLargeStrikes;
                break;
            case 2:
                abl.Top().Integer = commander->NumSensorStrikes;
                break;
            case 3:
                abl.Top().Integer = commander->NumCameraDrones;
                break;
            default:
                break;
        }
    }

    abl.GetCodeToken();
    return IntegerTypePtr;
}

namespace
{
    /// <summary>setstrikes / addstrikes: a commander's strikes of one type (0 small, 1 large, 2 sensor, 3 camera
    /// drones), set to <paramref name="count"/> or raised by it.</summary>
    auto ChangeStrikes(MCAblRuntime& abl, bool add) -> void
    {
        abl.GetCodeToken();
        abl.GetCodeToken();
        abl.ExecExpression();
        int32_t commanderId = abl.Top().Integer;
        abl.Pop();
        int32_t strikeType = abl.NextInteger();
        int32_t count = abl.NextInteger();

        if (commanderId > -1 && commanderId < NumCommanders() && strikeType > -1)
        {
            MCCommander* commander = CommanderById(commanderId);

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

        abl.GetCodeToken();
    }
}

auto ExecHbSetStrikes(MCAblRuntime& abl) -> void
{
    ChangeStrikes(abl, false);
}

auto ExecHbAddStrikes(MCAblRuntime& abl) -> void
{
    ChangeStrikes(abl, true);
}

auto ExecHbIsServer(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(MPlayer && MPlayer->IsServer ? 1 : 0);
    abl.GetCodeToken();
    return BooleanTypePtr;
}

auto ExecHbGetHomeTeam(MCAblRuntime& abl) -> MCAblType*
{
    abl.PushInteger(HomeTeam()->Id + 500);
    abl.GetCodeToken();
    return IntegerTypePtr;
}
