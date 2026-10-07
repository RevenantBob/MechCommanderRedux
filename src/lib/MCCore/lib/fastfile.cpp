#include "stdafx.h"
#include "lib/fastfile.h"
#include "lib/ffile.h"

MCFastFile** FastFiles = nullptr;
int32_t NumFastFiles = 0;
int32_t MaxFastFiles = 0;
int32_t FfLastError = 0;

int FastFileInit(const char* fname)
{
    if (NumFastFiles == MaxFastFiles)
    {
        FfLastError = -1;
        return 0;
    }

    MCFastFile* fastFile = new MCFastFile;
    FastFiles[NumFastFiles] = fastFile;
    const int32_t result = fastFile->Open(fname);

    if (result != 0)
    {
        FfLastError = result;
        return 0;
    }

    ++NumFastFiles;
    return 1;
}

void FastFileFini()
{
    if (FastFiles != nullptr)
    {
        for (int32_t i = 0; i < MaxFastFiles; ++i)
        {
            if (FastFiles[i] != nullptr)
            {
                FastFiles[i]->Close();
            }

            delete FastFiles[i];
            FastFiles[i] = nullptr;
        }
    }

    std::free(FastFiles);
    FastFiles = nullptr;
    NumFastFiles = 0;
}

MCFastFile* FastFileFind(const char* fname)
{
    if (FastFiles == nullptr)
    {
        return nullptr;
    }

    for (int32_t i = 0; i < NumFastFiles; ++i)
    {
        if (FastFiles[i]->OpenFast(fname) != -1)
        {
            return FastFiles[i];
        }
    }

    return nullptr;
}
