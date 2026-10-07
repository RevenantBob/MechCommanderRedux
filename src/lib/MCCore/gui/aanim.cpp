#include "stdafx.h"
#include "gui/aanim.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "logistics/logbri.h"
#include "platform/MCRenderer.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>
    /// The original's <c>clock()</c>: MSVC's counts wall-clock milliseconds since the process started
    /// (<c>CLOCKS_PER_SEC</c> is 1000).
    /// </summary>
    int32_t ClockTicks()
    {
        return static_cast<int32_t>(MCPort::Milliseconds());
    }

    /// <summary>The fields every reset clears (the times are left alone).</summary>
    void Reset(MCGuiAnimation& animation)
    {
        animation.CurFrame = 0;
        animation.NumFrames = 0;
        animation.ShapeWidth = 0;
        animation.ShapeHeight = 0;
        animation.Rate = 15.0f;
    }

    /// <summary>Unregisters and frees the shape table.</summary>
    void FreeShapes(MCGuiAnimation& animation)
    {
        if (animation.Shapes != nullptr)
        {
            MCRenderer::UnregisterData(animation.Shapes.get());
            animation.Shapes.reset();
        }
    }
}

MCGuiAnimation::MCGuiAnimation()
{
}

MCGuiAnimation::~MCGuiAnimation()
{
    Reset(*this);
    FreeShapes(*this);
}

auto MCGuiAnimation::Init(char* fileName) -> int32_t
{
    Reset(*this);
    FreeShapes(*this);

    if (fileName != nullptr)
    {
        return LoadShape(fileName);
    }

    return 0;
}

auto MCGuiAnimation::Destroy() -> void
{
    Reset(*this);
    FreeShapes(*this);
}

auto MCGuiAnimation::ShapeTable() -> void*
{
    return Shapes.get();
}

auto MCGuiAnimation::LoadShape(char* fileName) -> int32_t
{
    FreeShapes(*this);
    MCFile file;
    char path[128];
    std::snprintf(path, sizeof(path), "%s%s", ArtPath, fileName);

    if (FileExists(path) == 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
        return -1;
    }

    file.Open(path, READ, 0x32);
    const uint32_t size = file.FileSize();

    if (size == 0)
    {
        return -2;
    }

    Shapes = std::make_unique<uint8_t[]>(size);
    file.Read(Shapes.get(), static_cast<int32_t>(size));
    file.Close();
    MCRenderer::RegisterData(Shapes.get(), size, MCDataKind::Shapes);
    NumFrames = VfxShapeCount(Shapes.get());
    CurFrame = 0;

    if (NumFrames != 0)
    {
        const int32_t bounds = VfxShapeBounds(Shapes.get(), 0);
        ShapeWidth = bounds >> 16;
        ShapeHeight = bounds & 0xffff;
    }

    LastTime = ClockTicks();
    return 0;
}

auto MCGuiAnimation::Width() -> int32_t
{
    return ShapeWidth;
}

auto MCGuiAnimation::DrawFrame(int32_t frame, MCPane* pane, int32_t xPos, int32_t yPos) -> void
{
    AGShapeDraw(pane, Shapes.get(), frame, xPos, yPos);
}

auto MCGuiAnimation::Draw(MCPane* pane, int32_t xPos, int32_t yPos) -> void
{
    AGShapeDraw(pane, Shapes.get(), CurFrame, xPos, yPos);
    ThisTime = ClockTicks();

    if (1.0f / Rate < static_cast<float>(ThisTime - LastTime) * 0.001f)
    {
        SetFrame(NextFrame());
        LastTime = ThisTime;
    }
}

auto MCGuiAnimation::NextFrame() -> int32_t
{
    const int32_t next = CurFrame + 1;
    return next < NumFrames ? next : 0;
}

auto MCGuiAnimation::SetFrame(int32_t frame) -> void
{
    CurFrame = frame;
}

auto MCGuiAnimation::SetFrameRate(float newRate) -> void
{
    Rate = newRate;
}

auto MCGuiAnimation::CurrentFrame() -> int32_t
{
    return CurFrame;
}

auto MCGuiAnimation::NumberOfFrames() -> int32_t
{
    return NumFrames;
}

auto MCGuiAnimation::FrameRate() -> float
{
    return Rate;
}
