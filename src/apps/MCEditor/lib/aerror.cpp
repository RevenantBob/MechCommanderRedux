#include "stdafx.h"
#include "lib/aerror.h"

void Fatal(int32_t errCode, const char* errMessage, const char* errMessage2)
{
    char message[256];
    std::snprintf(message, sizeof(message), "Fatal Error: %08X", static_cast<uint32_t>(errCode));

    if (errMessage != nullptr)
    {
        strcat_s(message, errMessage);
    }

    if (errMessage2 != nullptr)
    {
        strcat_s(message, errMessage2);
    }

    std::printf("%s", message);
    std::exit(errCode);
}
