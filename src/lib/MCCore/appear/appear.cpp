#include "stdafx.h"
#include "appear/appear.h"
#include "appear/apprtype.h"
#include "camera/camera.h"
#include "engine/ceglist.h"
#include "engine/celine.h"
#include "lib/cvmath.h"
#include "lib/heap.h"
#include "object/gameobj.h"

namespace
{
    /// <summary>The depth the selection marks draw at (in front of everything).</summary>
    constexpr int32_t SELECT_DEPTH = -50000;

    /// <summary>Adds a one-colour line from (x0, y0) to (x1, y1) at <see cref="SELECT_DEPTH"/>.</summary>
    void addSelectLine(float x0, float y0, float x1, float y1, uint8_t color)
    {
        vector_2d start;
        start.x = x0;
        start.y = y0;
        vector_2d end;
        end.x = x1;
        end.y = y1;
        ElementList->add(new LineElement(start, end, color, nullptr, SELECT_DEPTH, -1));
    }

    /// <summary>
    /// The screen box the selection marks go around: the type's "Bounds" (halved when zoomed out) about the screen
    /// position, or the appearance's own bounds when the type has none.
    /// </summary>
    void selectBounds(Appearance* appearance, float& left, float& top, float& right, float& bottom)
    {
        AppearanceType* type = appearance->getAppearanceType();

        if (type == nullptr || (type->boundsUpperLeftX == 0 && type->boundsUpperLeftY == 0 &&
                                type->boundsLowerRightX == 0 && type->boundsLowerRightY == 0))
        {
            left = appearance->upperLeft.x;
            top = appearance->upperLeft.y;
            right = appearance->lowerRight.x;
            bottom = appearance->lowerRight.y;
            return;
        }

        const int32_t shift = eye->cameraScale == 1 ? 1 : 0;
        left = static_cast<float>(type->boundsUpperLeftX >> shift) + appearance->screenPos.x;
        top = static_cast<float>(type->boundsUpperLeftY >> shift) + appearance->screenPos.y;
        right = static_cast<float>(type->boundsLowerRightX >> shift) + appearance->screenPos.x;
        bottom = static_cast<float>(type->boundsLowerRightY >> shift) + appearance->screenPos.y;
    }

    /// <summary>The marks' distance from the box: 5 pixels at full size, 2.5 zoomed out.</summary>
    float selectMargin()
    {
        float scale = 0.5f;

        if (eye->cameraScale != 1)
        {
            scale = 1.0f;
        }

        return scale * 5.0f;
    }
}

auto Appearance::operator new(size_t size) noexcept -> void*
{
    void* block = nullptr;

    if (AppearanceTypeList::appearanceHeap != nullptr && AppearanceTypeList::appearanceHeap->heapSize != 0)
    {
        block = AppearanceTypeList::appearanceHeap->malloc(static_cast<uint32_t>(size));
    }

    return block;
}

auto Appearance::operator delete(void* block) -> void
{
    if (AppearanceTypeList::appearanceHeap != nullptr && AppearanceTypeList::appearanceHeap->heapSize != 0)
    {
        AppearanceTypeList::appearanceHeap->free(block);
    }
}

auto Appearance::drawBars() -> void
{
}

auto Appearance::getScreenPos(Camera* cam) -> vector_2d
{
    vector_2d result;

    if (cam == nullptr)
    {
        result.x = screenPos.x;
        result.y = screenPos.y;
        return result;
    }

    const vector_3d position = owner->getPosition();
    float scale = 0.5f;

    if (cam->cameraScale != 1)
    {
        scale = 1.0f;
    }

    const float sx = (position.x - cam->position.x) * scale;
    const float sy = (position.y - cam->position.y) * scale;
    result.x = sx * cam->cosAngle + sy * cam->cosAngle + cam->halfWidth;
    result.y = ((sx * cam->sinAngle + cam->halfHeight) - sy * cam->sinAngle) - scale * (position.z - cam->position.z);
    return result;
}

auto Appearance::drawSelectBox(uint8_t color) -> void
{
    float left;
    float top;
    float right;
    float bottom;
    selectBounds(this, left, top, right, bottom);
    ElementList->openGroup(SELECT_DEPTH, 1);
    const float margin = selectMargin();
    const float outLeft = left - margin;
    const float outTop = top - margin;
    const float outRight = margin + right;
    const float outBottom = margin + bottom;
    // A corner mark at each corner of the box grown by the margin.
    addSelectLine(outLeft, outTop, outLeft, top, color);
    addSelectLine(outLeft, outTop, left, outTop, color);
    addSelectLine(outRight, outTop, outRight, top, color);
    addSelectLine(outRight, outTop, right, outTop, color);
    addSelectLine(outRight, outBottom, outRight, bottom, color);
    addSelectLine(outRight, outBottom, right, outBottom, color);
    addSelectLine(outLeft, outBottom, outLeft, bottom, color);
    addSelectLine(outLeft, outBottom, left, outBottom, color);
}

auto Appearance::drawSelectBrackets(uint8_t color) -> void
{
    const float margin = selectMargin();
    float left;
    float top;
    float right;
    float bottom;
    selectBounds(this, left, top, right, bottom);
    ElementList->openGroup(SELECT_DEPTH, 1);
    const float outLeft = left - margin;
    const float outTop = top - margin;
    const float outRight = right + margin;
    const float outBottom = bottom + margin;
    // A bar over and under the box, each with short ends turned toward it. (The second line lies on the third.)
    addSelectLine(outLeft, top, outLeft, outTop, color);
    addSelectLine(outLeft, outTop, left, outTop, color);
    addSelectLine(outLeft, outTop, outRight, outTop, color);
    addSelectLine(outRight, top, outRight, outTop, color);
    addSelectLine(outLeft, bottom, outLeft, outBottom, color);
    addSelectLine(outLeft, outBottom, outRight, outBottom, color);
    addSelectLine(outRight, outBottom, outRight, bottom, color);
}
