#include "stdafx.h"
#include "engine/ceglist.h"
#include "camera/camera.h"
#include "engine/celement.h"
#include "object/objque.h"

MCElementBuffer* ElementList = nullptr;
int32_t NumElements = 0;

auto MCElementGroup::Draw() -> void
{
    MCElement** element = Buffer->Elements.data() + FirstElement;

    for (int32_t count = NumElements; count > 0; count--)
    {
        // Port: a unit overlay (health bar, selection mark, strike timer) draws on the screen over the view, at the
        // screen's scale (its position was mapped there when it was made); the rest into the world surface.
        if (MCOverlay.Pane != nullptr && MCIsOverlayDepth((*element)->Depth))
        {
            MCPane* worldPane = GlobalPane;
            GlobalPane = MCOverlay.Pane;
            (*element)->Draw();
            GlobalPane = worldPane;
        }
        else
        {
            (*element)->Draw();
        }

        element++;
        ::NumElements++;
    }
}

auto MCElementGroup::Sort() -> void
{
    if (NumElements == 0 || SortElements == 0)
    {
        return;
    }

    if (NumElements > 1)
    {
        MCElement** first = Buffer->Elements.data() + FirstElement;
        MCElement** end = first + NumElements;

        // Deepest first: each slot takes the deepest of the elements from it on.
        for (MCElement** slot = first; slot != end; slot++)
        {
            for (MCElement** other = slot; other != end; other++)
            {
                MCElement* element = *slot;

                if (element->Depth < (*other)->Depth)
                {
                    *slot = *other;
                    *other = element;
                }
            }
        }
    }

    MinDepth = Buffer->Elements[FirstElement]->Depth;
    MaxDepth = Buffer->Elements[FirstElement + NumElements - 1]->Depth;
}

auto MCElementGroup::Reset(MCElementBuffer* buffer) -> void
{
    NumElements = 0;
    Buffer = buffer;
    Depth = 0.0f;
    SortElements = 1;
    FirstElement = buffer->NumElements;
}

auto MCElementBuffer::Add(MCElement* element) -> void
{
    if (element != nullptr && NumElements < MaxElements)
    {
        Elements[NumElements] = element;
        NumElements++;
        MCElementGroup* group = CurrentGroup;
        group->NumElements++;

        if (group->MaxDepth < element->Depth)
        {
            group->MaxDepth = element->Depth;
            return;
        }

        if (element->Depth < group->MinDepth)
        {
            group->MinDepth = element->Depth;
        }
    }
}

auto MCElementBuffer::Draw() -> void
{
    MCElementGroup** group = GroupList.data();

    for (int32_t count = NumGroups; count > 0; count--)
    {
        (*group)->Draw();
        group++;
    }
}

auto MCElementBuffer::Init(int32_t numElements, int32_t unused, int32_t numGroups) -> int32_t
{
    (void)unused;

    if (numGroups == 0)
    {
        MaxGroups = 1;
    }

    MaxGroups = numGroups;
    MaxElements = numElements;
    // The original laid the three lists out in one heap; reset uses the first group even when maxGroups is 0.
    const size_t groupCount = static_cast<size_t>(std::max(numGroups, 1));
    Elements.assign(static_cast<size_t>(std::max(numElements, 0)), nullptr);
    Groups.assign(groupCount, MCElementGroup{});
    GroupList.assign(groupCount, nullptr);
    Reset();
    return 0;
}

auto MCElementBuffer::Sort() -> void
{
    int32_t passes = NumGroups;

    if (passes == 0)
    {
        return;
    }

    CurrentGroup->Sort();
    MCElementGroup** last = LastGroupPtr;
    MCElementGroup** first = GroupList.data();

    // A bubble sort, deepest first, of at most numGroups passes.
    while (first != last)
    {
        bool swapped = false;

        for (MCElementGroup** group = first; group != last; group++)
        {
            MCElementGroup* current = group[0];

            if (current->Depth < group[1]->Depth)
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

auto MCElementBuffer::Free() -> void
{
    Elements = {};
    Groups = {};
    GroupList = {};
}

auto MCElementBuffer::Reset() -> void
{
    NumElements = 0;
    CurrentGroup = Groups.data();
    NumGroups = 1;
    Groups[0].Reset(this);
    LastGroupPtr = GroupList.data();
    *LastGroupPtr = CurrentGroup;
}

auto MCElementBuffer::OpenGroup() -> void
{
    if (MaxGroups == 1)
    {
        return;
    }

    CurrentGroup->Sort();
    NumGroups++;

    if (MaxGroups < NumGroups)
    {
        NumGroups--;
        MaxObjectsDrawn = 1;
        return;
    }

    CurrentGroup++;
    CurrentGroup->Reset(this);
    LastGroupPtr++;
    *LastGroupPtr = CurrentGroup;
}

auto MCElementBuffer::OpenGroup(int32_t depth, int sortElements) -> void
{
    if (MaxGroups == 1)
    {
        return;
    }

    CurrentGroup->Sort();
    NumGroups++;

    if (MaxGroups < NumGroups)
    {
        NumGroups--;
        MaxObjectsDrawn = 1;
        return;
    }

    CurrentGroup++;
    CurrentGroup->Reset(this);
    LastGroupPtr++;
    *LastGroupPtr = CurrentGroup;
    const float groupDepth = static_cast<float>(depth);
    CurrentGroup->Depth = groupDepth;
    CurrentGroup->SortElements = sortElements;
    CurrentGroup->MinDepth = groupDepth;
    CurrentGroup->MaxDepth = groupDepth;
}
