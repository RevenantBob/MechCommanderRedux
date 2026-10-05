#include "stdafx.h"
#include "main/MCGameContext.h"

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

MCTestContextScope::MCTestContextScope() : _Previous(&MCGameContext::Current()), _Context(_Previous)
{
    MCGameContext::SetCurrent(&_Context);
}

MCTestContextScope::~MCTestContextScope()
{
    MCGameContext::SetCurrent(_Previous);
}
