#include "stdafx.h"
#include "engine/ceglist.h"
#include "camera/camera.h"
#include "engine/celement.h"
#include "lib/heap.h"
#include "object/objque.h"

Element** ElementBuffer::sortQueue = nullptr;
ElementBuffer* ElementList = nullptr;
int32_t numElements = 0;

namespace
{
    /// <summary>The group pointer list, which follows the groups in the buffer's heap.</summary>
    ElementGroup** groupList(ElementBuffer* buffer)
    {
        return reinterpret_cast<ElementGroup**>(buffer->groups + buffer->maxGroups);
    }
}

auto ElementGroup::draw() -> void
{
    Element** element = buffer->elements + firstElement;

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

    if (numElements > 1 && ElementBuffer::sortQueue == nullptr)
    {
        Element** first = buffer->elements + firstElement;
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

auto ElementBuffer::operator new(size_t size) noexcept -> void*
{
    return systemHeap->malloc(static_cast<uint32_t>(size));
}

auto ElementBuffer::operator delete(void* block) -> void
{
    systemHeap->free(block);
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
    ElementGroup** group = groupList(this);

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
    // Port fix: sized from the port's types. The original's (numElements + numGroups * 12) * 4 counts 4-byte
    // pointers and 0x2c-byte groups.
    const uint32_t size = static_cast<uint32_t>(numElements * sizeof(Element*) +
                                                numGroups * (sizeof(ElementGroup) + sizeof(ElementGroup*)));
    elementHeap = new HeapManager();

    if (elementHeap == nullptr)
    {
        return -0x1114fffd;
    }

    int32_t result = elementHeap->createHeap(size);

    if (result == 0)
    {
        result = elementHeap->commitHeap(size);

        if (result == 0)
        {
            elements = reinterpret_cast<Element**>(elementHeap->getHeapPtr());
            groups = reinterpret_cast<ElementGroup*>(elementHeap->getHeapPtr() + numElements * sizeof(Element*));
            reset();
            result = 0;
        }
    }

    return result;
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
    ElementGroup** first = groupList(this);

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
    if (elementHeap != nullptr)
    {
        delete elementHeap;
        elementHeap = nullptr;
    }

    if (sortQueue != nullptr)
    {
        ::operator delete(sortQueue);
        sortQueue = nullptr;
    }
}

auto ElementBuffer::reset() -> void
{
    numElements = 0;
    currentGroup = groups;
    numGroups = 1;
    groups->reset(this);
    lastGroupPtr = groupList(this);
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
