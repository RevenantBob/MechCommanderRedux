#include "stdafx.h"
#include "gui/aanim.h"
#include "gui/asystem.h"
#include "lib/aerror.h"
#include "lib/file.h"
#include "lib/heap.h"
#include "logistics/logbri.h"
#include "vfx/vfxfuncs.h"

namespace
{
    /// <summary>
    /// The original's <c>clock()</c>: MSVC's counts wall-clock milliseconds since the process started
    /// (<c>CLOCKS_PER_SEC</c> is 1000).
    /// </summary>
    int32_t clockTicks()
    {
        return static_cast<int32_t>(MCPort::Milliseconds());
    }

    /// <summary>The fields every reset clears (the times are left alone).</summary>
    void reset(aAnimation& animation)
    {
        animation.curFrame = 0;
        animation.numFrames = 0;
        animation.shapeWidth = 0;
        animation.shapeHeight = 0;
        animation.rate = 15.0f;
    }
}

aAnimation::aAnimation()
{
}

aAnimation::~aAnimation()
{
    reset(*this);
    shapes = nullptr;
}

auto aAnimation::init(char* fileName) -> int32_t
{
    reset(*this);

    if (shapes != nullptr)
    {
        guiHeap->free(shapes);
        shapes = nullptr;
    }

    if (fileName != nullptr)
    {
        return loadShape(fileName);
    }

    return 0;
}

auto aAnimation::destroy() -> void
{
    reset(*this);

    if (shapes != nullptr)
    {
        guiHeap->free(shapes);
        shapes = nullptr;
    }
}

auto aAnimation::shapeTable() -> void*
{
    return shapes;
}

auto aAnimation::loadShape(char* fileName) -> int32_t
{
    if (shapes != nullptr)
    {
        guiHeap->free(shapes);
        shapes = nullptr;
    }

    File file;
    char path[128];
    std::snprintf(path, sizeof(path), "%s%s", artPath, fileName);

    if (fileExists(path) == 0)
    {
        char message[256];
        std::snprintf(message, sizeof(message), "Unable to find '%s'", path);
        GeneralMsg(message);
        return -1;
    }

    file.open(path, READ, 0x32);
    const uint32_t size = file.fileSize();

    if (size == 0)
    {
        return -2;
    }

    shapes = static_cast<uint8_t*>(guiHeap->malloc(size));

    if (shapes == nullptr)
    {
        return 3;
    }

    file.read(shapes, static_cast<int32_t>(size));
    file.close();
    numFrames = VFX_shape_count(shapes);
    curFrame = 0;

    if (numFrames != 0)
    {
        const int32_t bounds = VFX_shape_bounds(shapes, 0);
        shapeWidth = bounds >> 16;
        shapeHeight = bounds & 0xffff;
    }

    lastTime = clockTicks();
    return 0;
}

auto aAnimation::width() -> int32_t
{
    return shapeWidth;
}

auto aAnimation::drawFrame(int32_t frame, _pane* pane, int32_t xPos, int32_t yPos) -> void
{
    AG_shape_draw(pane, shapes, frame, xPos, yPos);
}

auto aAnimation::draw(_pane* pane, int32_t xPos, int32_t yPos) -> void
{
    AG_shape_draw(pane, shapes, curFrame, xPos, yPos);
    thisTime = clockTicks();

    if (1.0f / rate < static_cast<float>(thisTime - lastTime) * 0.001f)
    {
        setFrame(nextFrame());
        lastTime = thisTime;
    }
}

auto aAnimation::nextFrame() -> int32_t
{
    const int32_t next = curFrame + 1;
    return next < numFrames ? next : 0;
}

auto aAnimation::setFrame(int32_t frame) -> void
{
    curFrame = frame;
}

auto aAnimation::setFrameRate(float newRate) -> void
{
    rate = newRate;
}

auto aAnimation::currentFrame() -> int32_t
{
    return curFrame;
}

auto aAnimation::numberOfFrames() -> int32_t
{
    return numFrames;
}

auto aAnimation::frameRate() -> float
{
    return rate;
}
