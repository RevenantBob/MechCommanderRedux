#include "stdafx.h"
#include "lib/cident.h"

MCFullPathFileName::~MCFullPathFileName()
{
    Destroy();
}

void MCFullPathFileName::Destroy()
{
    FullName.clear();
}

void MCFullPathFileName::Init(const char* dirPath, const char* name, const char* ext)
{
    // Port fix: a null extension crashed the original's length count.
    FullName = dirPath;
    FullName += name;

    if (ext != nullptr)
    {
        FullName += ext;
    }
}
