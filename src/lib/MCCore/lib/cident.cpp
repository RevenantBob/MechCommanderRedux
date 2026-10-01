#include "stdafx.h"
#include "lib/cident.h"
#include "lib/heap.h"

namespace
{
    /// <summary>Frees a path allocated by <see cref="FullPathFileName::init"/>.</summary>
    void FreeFullName(char*& fullName)
    {
        if (systemHeap != nullptr && systemHeap->heapSize != 0 && systemHeap->owns(fullName))
        {
            systemHeap->free(fullName);
        }
        else
        {
            // Port fix: a path made before systemHeap existed came from operator new; the original handed it to the
            // heap, which ignored it.
            ::operator delete(fullName);
        }

        fullName = nullptr;
    }
}

FullPathFileName::~FullPathFileName()
{
    FreeFullName(fullName);
}

void FullPathFileName::destroy()
{
    FreeFullName(fullName);
}

void FullPathFileName::init(const char* dir_path, const char* name, const char* ext)
{
    destroy();

    // Port fix: a null extension crashed the original's length count.
    const size_t extLength = ext != nullptr ? std::strlen(ext) : 0;
    const size_t totalLength = std::strlen(dir_path) + std::strlen(name) + extLength + 1;

    if (systemHeap == nullptr || systemHeap->heapSize == 0)
    {
        fullName = static_cast<char*>(::operator new(totalLength));
    }
    else
    {
        fullName = static_cast<char*>(systemHeap->malloc(static_cast<uint32_t>(totalLength)));
    }

    std::strcpy(fullName, dir_path);
    std::strcat(fullName, name);

    if (ext != nullptr)
    {
        std::strcat(fullName, ext);
    }
}
