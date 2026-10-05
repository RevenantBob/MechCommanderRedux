#include "stdafx.h"
#include "engine/ceglist.h"
#include "camera/camera.h"
#include "engine/celement.h"
#include "object/objque.h"

ElementBuffer* ElementList = nullptr;
int32_t numElements = 0;

auto ElementGroup::draw() -> void
{
    Element** element = buffer->elements.data() + firstElement;

    for (int32_t count = numElements; count > 0; count--)
    {
        // Port: a unit overlay (health bar, selection mark, strike timer) draws on the screen over the view, at the
        // screen's scale (its position was mapped there when it was made); the rest into the world surface.
        if (MCOverlay.Pane != nullptr && MCIsOverlayDepth((*element)->depth))
        {
            _pane* worldPane = globalPane;
            globalPane = MCOverlay.Pane;
            (*element)->draw();
            globalPane = worldPane;
        }
        else
        {
            (*element)->draw();
        }

        element++;
        ::numElements++;
    }
}

auto ElementGroup::sort() -> void
{
    if (numElements == 0 || sortElements == 0)
    {
        return;
    }

    if (numElements > 1)
    {
        Element** first = buffer->elements.data() + firstElement;
        Element** end = first + numElements;

        // Deepest first: each slot takes the deepest of the elements from it on.
        for (Element** slot = first; slot != end; slot++)
        {
            for (Element** other = slot; other != end; other++)
            {
                Element* element = *slot;

                if (element->depth < (*other)->depth)
                {
                    *slot = *other;
                    *other = element;
                }
            }
        }
    }

    minDepth = buffer->elements[firstElement]->depth;
    maxDepth = buffer->elements[firstElement + numElements - 1]->depth;
}

auto ElementGroup::reset(ElementBuffer* _buffer) -> void
{
    numElements = 0;
    buffer = _buffer;
    depth = 0.0f;
    sortElements = 1;
    firstElement = _buffer->numElements;
}

auto ElementBuffer::add(Element* element) -> void
{
    if (element != nullptr && numElements < maxElements)
    {
        elements[numElements] = element;
        numElements++;
        ElementGroup* group = currentGroup;
        group->numElements++;

        if (group->maxDepth < element->depth)
        {
            group->maxDepth = element->depth;
            return;
        }

        if (element->depth < group->minDepth)
        {
            group->minDepth = element->depth;
        }
    }
}

auto ElementBuffer::draw() -> void
{
    ElementGroup** group = groupList.data();

    for (int32_t count = numGroups; count > 0; count--)
    {
        (*group)->draw();
        group++;
    }
}

auto ElementBuffer::init(int32_t numElements, int32_t unused, int32_t numGroups) -> int32_t
{
    (void)unused;

    if (numGroups == 0)
    {
        maxGroups = 1;
    }

    maxGroups = numGroups;
    maxElements = numElements;
    // The original laid the three lists out in one heap; reset uses the first group even when maxGroups is 0.
    const size_t groupCount = static_cast<size_t>(std::max(numGroups, 1));
    elements.assign(static_cast<size_t>(std::max(numElements, 0)), nullptr);
    groups.assign(groupCount, ElementGroup{});
    groupList.assign(groupCount, nullptr);
    reset();
    return 0;
}

auto ElementBuffer::sort() -> void
{
    int32_t passes = numGroups;

    if (passes == 0)
    {
        return;
    }

    currentGroup->sort();
    ElementGroup** last = lastGroupPtr;
    ElementGroup** first = groupList.data();

    // A bubble sort, deepest first, of at most numGroups passes.
    while (first != last)
    {
        bool swapped = false;

        for (ElementGroup** group = first; group != last; group++)
        {
            ElementGroup* current = group[0];

            if (current->depth < group[1]->depth)
            {
                swapped = true;
                group[0] = group[1];
                group[1] = current;
            }
        }

        if (!swapped)
        {
            return;
        }

        passes--;

        if (passes == 0)
        {
            return;
        }
    }
}

auto ElementBuffer::free() -> void
{
    elements = {};
    groups = {};
    groupList = {};
}

auto ElementBuffer::reset() -> void
{
    numElements = 0;
    currentGroup = groups.data();
    numGroups = 1;
    groups[0].reset(this);
    lastGroupPtr = groupList.data();
    *lastGroupPtr = currentGroup;
}

auto ElementBuffer::openGroup() -> void
{
    if (maxGroups == 1)
    {
        return;
    }

    currentGroup->sort();
    numGroups++;

    if (maxGroups < numGroups)
    {
        numGroups--;
        MaxObjectsDrawn = 1;
        return;
    }

    currentGroup++;
    currentGroup->reset(this);
    lastGroupPtr++;
    *lastGroupPtr = currentGroup;
}

auto ElementBuffer::openGroup(int32_t depth, int sortElements) -> void
{
    if (maxGroups == 1)
    {
        return;
    }

    currentGroup->sort();
    numGroups++;

    if (maxGroups < numGroups)
    {
        numGroups--;
        MaxObjectsDrawn = 1;
        return;
    }

    currentGroup++;
    currentGroup->reset(this);
    lastGroupPtr++;
    *lastGroupPtr = currentGroup;
    const float groupDepth = static_cast<float>(depth);
    currentGroup->depth = groupDepth;
    currentGroup->sortElements = sortElements;
    currentGroup->minDepth = groupDepth;
    currentGroup->maxDepth = groupDepth;
}
