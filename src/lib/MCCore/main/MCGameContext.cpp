#include "stdafx.h"
#include "main/MCGameContext.h"
#include "appear/MCAppearanceTypeList.h"
#include "abl/MCAblSymbolTable.h"
#include "camera/MCCameraList.h"
#include "color/MCPalette.h"
#include "engine/MCCraterManager.h"
#include "engine/MCElementBuffer.h"
#include "lib/MCFastFileSet.h"
#include "sprite/MCSpriteManager.h"
#include "terrain/MCTerrain.h"

namespace
{
    /// <summary>The context with the real services, made on first use and never destroyed (threads and static
    /// destructors may still ask for the clock while the process ends).</summary>
    MCGameContext* RootContext()
    {
        static MCGameContext* const root = []
        {
            auto* context = new MCGameContext(nullptr);
            context->SetClock(std::make_unique<MCSystemClock>());
            context->SetRandom(std::make_unique<MCCrtRandom>());
            context->SetFiles(std::make_unique<MCDiskFileSource>());
            context->SetAudio(std::make_unique<MCSdlAudioDevice>());
            context->SetNet(std::make_unique<MCSocketTransport>());
            context->SetFastFiles(std::make_unique<MCFastFileSet>());
            return context;
        }();
        return root;
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

MCGameContext::~MCGameContext() = default;

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

MCAblSymbolTable* MCGameContext::AblSymbols() const
{
    return FindSystem(_AblSymbols, _Parent, &MCGameContext::AblSymbols);
}

std::unique_ptr<MCAblSymbolTable> MCGameContext::SetAblSymbols(std::unique_ptr<MCAblSymbolTable> symbols)
{
    return std::exchange(_AblSymbols, std::move(symbols));
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

MCTestContextScope::MCTestContextScope() : _Previous(&MCGameContext::Current()), _Context(_Previous)
{
    MCGameContext::SetCurrent(&_Context);
}

MCTestContextScope::~MCTestContextScope()
{
    MCGameContext::SetCurrent(_Previous);
}
