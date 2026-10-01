#include "stdafx.h"
#include "lib/routines.h"

void memclear(void* buffer, int length)
{
    if (length > 0)
    {
        std::memset(buffer, 0, static_cast<size_t>(length));
    }
}

void memfill(void* buffer, int length)
{
    if (length > 0)
    {
        std::memset(buffer, 0xff, static_cast<size_t>(length));
    }
}
