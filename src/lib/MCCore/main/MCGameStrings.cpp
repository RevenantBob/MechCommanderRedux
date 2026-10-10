#include "stdafx.h"
#include "main/MCGameStrings.h"
#include "platform/MCStringTable.h"

int32_t LanguageOffset = 0;

std::string LoadGameString(uint32_t id, int bufferSize)
{
    // LoadStringA of the executable's string table: the port reads MCX.EXE's resources itself.
    std::vector<char> buffer(static_cast<size_t>(bufferSize), '\0');
    MCStringTable::Game().LoadString(id + static_cast<uint32_t>(LanguageOffset), buffer.data(), bufferSize);
    return buffer.data();
}
