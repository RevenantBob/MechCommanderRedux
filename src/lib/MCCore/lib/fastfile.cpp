#include "stdafx.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"

FastFile** fastFiles = nullptr;
int32_t numFastFiles = 0;
int32_t maxFastFiles = 0;
int32_t ffLastError = 0;

int FastFileInit(const char* fname)
{
    if (numFastFiles == maxFastFiles)
    {
        ffLastError = -1;
        return 0;
    }

    FastFile* fastFile = new FastFile;
    fastFiles[numFastFiles] = fastFile;
    const int32_t result = fastFile->open(fname);

    if (result != 0)
    {
        ffLastError = result;
        return 0;
    }

    ++numFastFiles;
    return 1;
}

void FastFileFini()
{
    if (fastFiles != nullptr)
    {
        for (int32_t i = 0; i < maxFastFiles; ++i)
        {
            if (fastFiles[i] != nullptr)
            {
                fastFiles[i]->close();
            }

            delete fastFiles[i];
            fastFiles[i] = nullptr;
        }
    }

    std::free(fastFiles);
    fastFiles = nullptr;
    numFastFiles = 0;
}

FastFile* FastFileFind(const char* fname)
{
    if (fastFiles == nullptr)
    {
        return nullptr;
    }

    for (int32_t i = 0; i < numFastFiles; ++i)
    {
        if (fastFiles[i]->openFast(fname) != -1)
        {
            return fastFiles[i];
        }
    }

    return nullptr;
}
