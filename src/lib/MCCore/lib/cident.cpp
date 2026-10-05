#include "stdafx.h"
#include "lib/cident.h"

FullPathFileName::~FullPathFileName()
{
    destroy();
}

void FullPathFileName::destroy()
{
    fullName.clear();
}

void FullPathFileName::init(const char* dir_path, const char* name, const char* ext)
{
    // Port fix: a null extension crashed the original's length count.
    fullName = dir_path;
    fullName += name;

    if (ext != nullptr)
    {
        fullName += ext;
    }
}
