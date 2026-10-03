#include "stdafx.h"
#include "platform/MCRenderer.h"
#include "platform/MCSoftwareRenderer.h"

namespace
{
    /// <summary>Every renderer there is (only the software renderer so far).</summary>
    std::array<MCRenderer*, 1> AllRenderers()
    {
        return {&MCSoftwareRenderer::Instance()};
    }
}

MCRenderer& MCRenderer::For(const _window*)
{
    return MCSoftwareRenderer::Instance();
}

void MCRenderer::AlphaTableChanged()
{
    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnAlphaTableChanged();
    }
}

void MCRenderer::ForgetShapes(const void* begin, size_t size)
{
    for (MCRenderer* renderer : AllRenderers())
    {
        renderer->OnShapesForgotten(begin, size);
    }
}
