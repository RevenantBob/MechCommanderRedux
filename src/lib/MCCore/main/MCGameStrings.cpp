#include "stdafx.h"
#include "main/MCGameStrings.h"
#include "platform/MCStringTable.h"

int32_t LanguageOffset = 0;

std::string LoadGameString(uint32_t id, int bufferSize)
{
    // LoadStringA of the executable's string table: the port reads MCX.EXE's resources itself.
    std::vector<char> buffer(static_cast<size_t>(std::max(bufferSize, 0)), '\0');
    MCStringTable::Game().LoadString(id + static_cast<uint32_t>(LanguageOffset), buffer);
    return buffer.empty() ? std::string() : std::string(buffer.data());
}
