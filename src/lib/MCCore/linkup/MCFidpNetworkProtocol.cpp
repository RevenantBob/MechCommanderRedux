#include "stdafx.h"
#include "linkup/MCFidpNetworkProtocol.h"
#include "linkup/MCLinkupMessages.h"

MCFidpNetworkProtocol::MCFidpNetworkProtocol(const char* shortName, const char* longName,
                                             std::span<const uint8_t> connection)
    : ShortName(LinkupName(shortName, 0x3f))
    , LongName(LinkupName(longName, 0xff))
    , ConnectionBuffer(connection.begin(), connection.end())
{
}
