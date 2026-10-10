#include "stdafx.h"
#include "main/MCGameContext.h"
#include "appear/MCAppearanceTypeList.h"
#include "abl/MCAblRuntime.h"
#include "abl/MCAblSymbolTable.h"
#include "ai/MCMoveSystem.h"
#include "camera/MCCameraList.h"
#include "color/MCPalette.h"
#include "engine/MCCraterManager.h"
#include "engine/MCElementBuffer.h"
#include "object/MCCollisionSystem.h"
#include "object/MCContactSystem.h"
#include "object/MCEffectSystem.h"
#include "object/MCForces.h"
#include "object/MCObjectSystem.h"
#include "object/MCTrainManager.h"
#include "gameos/MCSoundRenderer.h"
#include "sound/MCSoundSystem.h"
#include "mission/MCMission.h"
#include "mission/MCScenario.h"
#include "network/MCMultiPlayer.h"
#include "gui/MCGuiSystem.h"
#include "iface/MCTacticalInterface.h"
#include "lib/MCFastFileSet.h"
#include "sprite/MCSpriteManager.h"
#include "terrain/MCTerrain.h"
#include "platform/MCNeverDestroyed.h"

namespace
{
    /// <summary>The context with the real services, made on first use and never destroyed (threads and static
    /// destructors may still ask for the clock while the process ends).</summary>
    MCGameContext* RootContext()
    {
        static MCNeverDestroyed<std::unique_ptr<MCGameContext>> root(
            []
            {
                auto context = std::make_unique<MCGameContext>(nullptr);
                context->SetClock(std::make_unique<MCSystemClock>());
                context->SetRandom(std::make_unique<MCCrtRandom>());
                context->SetFiles(std::make_unique<MCDiskFileSource>());
                context->SetAudio(std::make_unique<MCSdlAudioDevice>());
                context->SetNet(std::make_unique<MCSocketTransport>());
                context->SetFastFiles(std::make_unique<MCFastFileSet>());
                return context;
            }());
        return root->get();
    }

    /// <summary>The installed context, or null for the root one.</summary>
    std::atomic<MCGameContext*> CurrentContext = nullptr;

    /// <summary>The service in <paramref name="slot"/>, else the one <paramref name="parent"/> has.</summary>
    template <typename T>
    T& Find(const std::unique_ptr<T>& slot, MCGameContext* parent, T& (MCGameContext::*get)() const)
    {
        if (slot != nullptr)
        {
            return *slot;
        }

        SDL_assert(parent != nullptr);
        return (parent->*get)();
    }

    /// <summary>The system in <paramref name="slot"/>, else the one <paramref name="parent"/> has (null when none).</summary>
    template <typename T>
    T* FindSystem(const std::unique_ptr<T>& slot, MCGameContext* parent, T* (MCGameContext::*get)() const)
    {
        if (slot != nullptr)
        {
            return slot.get();
        }

        return parent != nullptr ? (parent->*get)() : nullptr;
    }
}

MCGameContext& MCGameContext::Current()
{
    MCGameContext* context = CurrentContext.load(std::memory_order_acquire);
    return context != nullptr ? *context : *RootContext();
}

void MCGameContext::SetCurrent(MCGameContext* context)
{
    CurrentContext.store(context, std::memory_order_release);
}

MCGameContext::MCGameContext(MCGameContext* parent) : _Parent(parent)
{
}

MCGameContext::~MCGameContext()
{
    // The scenario and the mission go first, then the objects, while every system they reach for is still here. Then
    // each slot is emptied, last to first, so a system going finds the ones after it gone (null, or the parent's),
    // never deleted ones.
    _Scenario.reset();
    _Mission.reset();
    _MultiPlayer.reset();

    if (_ObjectSystem != nullptr)
    {
        _ObjectSystem->Unload();
    }

    _TacticalInterface.reset();
    _SoundSystem.reset();
    _SoundRenderer.reset();
    _AblRuntime.reset();
    _AblSymbols.reset();
    _ObjectSystem.reset();
    _TrainManager.reset();
    _EffectSystem.reset();
    _CollisionSystem.reset();
    _ContactSystem.reset();
    _Forces.reset();
    _MoveSystem.reset();
    _CameraList.reset();
    _Terrain.reset();
    _AppearanceTypeList.reset();
    _SpriteManager.reset();
    _CraterManager.reset();
    _ElementList.reset();
    _GuiSystem.reset();
    _Palette.reset();
    _FastFiles.reset();
    _Net.reset();
    _Audio.reset();
    _Files.reset();
    _Random.reset();
    _Clock.reset();
}

MCClock& MCGameContext::Clock() const
{
    return Find(_Clock, _Parent, &MCGameContext::Clock);
}

MCRandom& MCGameContext::Random() const
{
    return Find(_Random, _Parent, &MCGameContext::Random);
}

MCFileSource& MCGameContext::Files() const
{
    return Find(_Files, _Parent, &MCGameContext::Files);
}

MCAudioDevice& MCGameContext::Audio() const
{
    return Find(_Audio, _Parent, &MCGameContext::Audio);
}

MCNetTransport& MCGameContext::Net() const
{
    return Find(_Net, _Parent, &MCGameContext::Net);
}

MCFastFileSet& MCGameContext::FastFiles() const
{
    return Find(_FastFiles, _Parent, &MCGameContext::FastFiles);
}

MCFastFileSet& MCGameContext::SetFastFiles(std::unique_ptr<MCFastFileSet> fastFiles)
{
    return Install(_FastFiles, std::move(fastFiles));
}

MCPalette* MCGameContext::Palette() const
{
    return FindSystem(_Palette, _Parent, &MCGameContext::Palette);
}

MCElementBuffer* MCGameContext::ElementList() const
{
    return FindSystem(_ElementList, _Parent, &MCGameContext::ElementList);
}

MCCraterManager* MCGameContext::CraterManager() const
{
    return FindSystem(_CraterManager, _Parent, &MCGameContext::CraterManager);
}

MCSpriteManager* MCGameContext::SpriteManager() const
{
    return FindSystem(_SpriteManager, _Parent, &MCGameContext::SpriteManager);
}

MCAppearanceTypeList* MCGameContext::AppearanceTypeList() const
{
    return FindSystem(_AppearanceTypeList, _Parent, &MCGameContext::AppearanceTypeList);
}

MCTerrain* MCGameContext::Terrain() const
{
    return FindSystem(_Terrain, _Parent, &MCGameContext::Terrain);
}

MCCameraList* MCGameContext::CameraList() const
{
    return FindSystem(_CameraList, _Parent, &MCGameContext::CameraList);
}

MCMoveSystem* MCGameContext::MoveSystem() const
{
    return FindSystem(_MoveSystem, _Parent, &MCGameContext::MoveSystem);
}

std::unique_ptr<MCMoveSystem> MCGameContext::SetMoveSystem(std::unique_ptr<MCMoveSystem> moveSystem)
{
    return std::exchange(_MoveSystem, std::move(moveSystem));
}

MCForces* MCGameContext::Forces() const
{
    return FindSystem(_Forces, _Parent, &MCGameContext::Forces);
}

std::unique_ptr<MCForces> MCGameContext::SetForces(std::unique_ptr<MCForces> forces)
{
    return std::exchange(_Forces, std::move(forces));
}

MCContactSystem* MCGameContext::ContactSystem() const
{
    return FindSystem(_ContactSystem, _Parent, &MCGameContext::ContactSystem);
}

std::unique_ptr<MCContactSystem> MCGameContext::SetContactSystem(std::unique_ptr<MCContactSystem> contactSystem)
{
    return std::exchange(_ContactSystem, std::move(contactSystem));
}

MCCollisionSystem* MCGameContext::CollisionSystem() const
{
    return FindSystem(_CollisionSystem, _Parent, &MCGameContext::CollisionSystem);
}

std::unique_ptr<MCCollisionSystem> MCGameContext::SetCollisionSystem(std::unique_ptr<MCCollisionSystem> collisionSystem)
{
    return std::exchange(_CollisionSystem, std::move(collisionSystem));
}

MCEffectSystem* MCGameContext::EffectSystem() const
{
    return FindSystem(_EffectSystem, _Parent, &MCGameContext::EffectSystem);
}

std::unique_ptr<MCEffectSystem> MCGameContext::SetEffectSystem(std::unique_ptr<MCEffectSystem> effectSystem)
{
    return std::exchange(_EffectSystem, std::move(effectSystem));
}

MCTrainManager* MCGameContext::TrainManager() const
{
    return FindSystem(_TrainManager, _Parent, &MCGameContext::TrainManager);
}

std::unique_ptr<MCTrainManager> MCGameContext::SetTrainManager(std::unique_ptr<MCTrainManager> trainManager)
{
    return std::exchange(_TrainManager, std::move(trainManager));
}

MCObjectSystem* MCGameContext::ObjectSystem() const
{
    return FindSystem(_ObjectSystem, _Parent, &MCGameContext::ObjectSystem);
}

std::unique_ptr<MCObjectSystem> MCGameContext::SetObjectSystem(std::unique_ptr<MCObjectSystem> objectSystem)
{
    return std::exchange(_ObjectSystem, std::move(objectSystem));
}

MCAblSymbolTable* MCGameContext::AblSymbols() const
{
    return FindSystem(_AblSymbols, _Parent, &MCGameContext::AblSymbols);
}

std::unique_ptr<MCAblSymbolTable> MCGameContext::SetAblSymbols(std::unique_ptr<MCAblSymbolTable> symbols)
{
    return std::exchange(_AblSymbols, std::move(symbols));
}

MCAblRuntime* MCGameContext::AblRuntime() const
{
    return FindSystem(_AblRuntime, _Parent, &MCGameContext::AblRuntime);
}

std::unique_ptr<MCAblRuntime> MCGameContext::SetAblRuntime(std::unique_ptr<MCAblRuntime> runtime)
{
    return std::exchange(_AblRuntime, std::move(runtime));
}

MCSoundRenderer* MCGameContext::SoundRenderer() const
{
    return FindSystem(_SoundRenderer, _Parent, &MCGameContext::SoundRenderer);
}

std::unique_ptr<MCSoundRenderer> MCGameContext::SetSoundRenderer(std::unique_ptr<MCSoundRenderer> renderer)
{
    return std::exchange(_SoundRenderer, std::move(renderer));
}

MCMission* MCGameContext::Mission() const
{
    return FindSystem(_Mission, _Parent, &MCGameContext::Mission);
}

std::unique_ptr<MCMission> MCGameContext::SetMission(std::unique_ptr<MCMission> mission)
{
    return std::exchange(_Mission, std::move(mission));
}

MCMultiPlayer* MCGameContext::MultiPlayer() const
{
    return FindSystem(_MultiPlayer, _Parent, &MCGameContext::MultiPlayer);
}

std::unique_ptr<MCMultiPlayer> MCGameContext::SetMultiPlayer(std::unique_ptr<MCMultiPlayer> multiPlayer)
{
    return std::exchange(_MultiPlayer, std::move(multiPlayer));
}

MCScenario* MCGameContext::Scenario() const
{
    return FindSystem(_Scenario, _Parent, &MCGameContext::Scenario);
}

std::unique_ptr<MCScenario> MCGameContext::SetScenario(std::unique_ptr<MCScenario> scenario)
{
    return std::exchange(_Scenario, std::move(scenario));
}

MCTacticalInterface* MCGameContext::TacticalInterface() const
{
    return FindSystem(_TacticalInterface, _Parent, &MCGameContext::TacticalInterface);
}

std::unique_ptr<MCTacticalInterface> MCGameContext::SetTacticalInterface(
    std::unique_ptr<MCTacticalInterface> tacticalInterface)
{
    return std::exchange(_TacticalInterface, std::move(tacticalInterface));
}

MCGuiSystem* MCGameContext::GuiSystem() const
{
    return FindSystem(_GuiSystem, _Parent, &MCGameContext::GuiSystem);
}

std::unique_ptr<MCGuiSystem> MCGameContext::SetGuiSystem(std::unique_ptr<MCGuiSystem> guiSystem)
{
    return std::exchange(_GuiSystem, std::move(guiSystem));
}

MCSoundSystem* MCGameContext::SoundSystem() const
{
    return FindSystem(_SoundSystem, _Parent, &MCGameContext::SoundSystem);
}

std::unique_ptr<MCSoundSystem> MCGameContext::SetSoundSystem(std::unique_ptr<MCSoundSystem> soundSystem)
{
    return std::exchange(_SoundSystem, std::move(soundSystem));
}

std::unique_ptr<MCTerrain> MCGameContext::SetTerrain(std::unique_ptr<MCTerrain> terrain)
{
    return std::exchange(_Terrain, std::move(terrain));
}

std::unique_ptr<MCCameraList> MCGameContext::SetCameraList(std::unique_ptr<MCCameraList> cameraList)
{
    return std::exchange(_CameraList, std::move(cameraList));
}

std::unique_ptr<MCSpriteManager> MCGameContext::SetSpriteManager(std::unique_ptr<MCSpriteManager> spriteManager)
{
    return std::exchange(_SpriteManager, std::move(spriteManager));
}

std::unique_ptr<MCAppearanceTypeList> MCGameContext::SetAppearanceTypeList(
    std::unique_ptr<MCAppearanceTypeList> typeList)
{
    return std::exchange(_AppearanceTypeList, std::move(typeList));
}

std::unique_ptr<MCPalette> MCGameContext::SetPalette(std::unique_ptr<MCPalette> palette)
{
    return std::exchange(_Palette, std::move(palette));
}

std::unique_ptr<MCElementBuffer> MCGameContext::SetElementList(std::unique_ptr<MCElementBuffer> elementList)
{
    return std::exchange(_ElementList, std::move(elementList));
}

std::unique_ptr<MCCraterManager> MCGameContext::SetCraterManager(std::unique_ptr<MCCraterManager> craterManager)
{
    return std::exchange(_CraterManager, std::move(craterManager));
}

MCTestContextScope::MCTestContextScope()
    : _Previous(&MCGameContext::Current()), _Context(std::make_unique<MCGameContext>(_Previous))
{
    MCGameContext::SetCurrent(_Context.get());
}

MCTestContextScope::~MCTestContextScope()
{
    _Context.reset();
    MCGameContext::SetCurrent(_Previous);
}
