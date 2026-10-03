#include "stdafx.h"
#include "platform/MCPresenter.h"

std::optional<MCRendererKind> MCRendererKindFromName(std::string_view name)
{
    auto equals = [name](std::string_view other)
    {
        return name.size() == other.size() && std::equal(name.begin(), name.end(), other.begin(), [](char a, char b)
                                                         { return std::tolower(static_cast<unsigned char>(a)) == b; });
    };

    if (equals("vulkan"))
    {
        return MCRendererKind::Vulkan;
    }

    if (equals("software"))
    {
        return MCRendererKind::Software;
    }

    return std::nullopt;
}

const char* MCRendererKindName(MCRendererKind kind)
{
    return kind == MCRendererKind::Software ? "software" : "vulkan";
}

MCViewport MCPresentViewport(int outputWidth, int outputHeight, int viewWidth, int viewHeight,
                             const MCPresentation& presentation)
{
    MCViewport view;

    if (outputWidth <= 0 || outputHeight <= 0 || viewWidth <= 0 || viewHeight <= 0)
    {
        return view;
    }

    const float outW = static_cast<float>(outputWidth);
    const float outH = static_cast<float>(outputHeight);

    if (presentation.Stretch)
    {
        return MCViewport{0.0f, 0.0f, outW, outH};
    }

    const float logW = static_cast<float>(viewWidth);
    const float logH = static_cast<float>(viewHeight);
    const float wantAspect = logW / logH;
    const float realAspect = outW / outH;

    if (presentation.IntegerScale)
    {
        float scale = wantAspect > realAspect ? static_cast<float>(outputWidth / viewWidth)
                                              : static_cast<float>(outputHeight / viewHeight);

        if (scale < 1.0f)
        {
            scale = 1.0f;
        }

        view.W = std::floor(logW * scale);
        view.H = std::floor(logH * scale);
        view.X = (outW - view.W) / 2.0f;
        view.Y = (outH - view.H) / 2.0f;
    }
    else if (std::fabs(wantAspect - realAspect) < 0.0001f)
    {
        view = {0.0f, 0.0f, outW, outH};
    }
    else if (wantAspect > realAspect)
    {
        const float scale = outW / logW;
        view.X = 0.0f;
        view.W = outW;
        view.H = std::floor(logH * scale);
        view.Y = (outH - view.H) / 2.0f;
    }
    else
    {
        const float scale = outH / logH;
        view.Y = 0.0f;
        view.H = outH;
        view.W = std::floor(logW * scale);
        view.X = (outW - view.W) / 2.0f;
    }

    return view;
}
