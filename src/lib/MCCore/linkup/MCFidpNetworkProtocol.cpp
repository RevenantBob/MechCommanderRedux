#include "stdafx.h"
#include "linkup/MCFidpNetworkProtocol.h"
#include "linkup/MCLinkupMessages.h"

MCFidpNetworkProtocol::MCFidpNetworkProtocol(const DPNAME& name, std::span<const uint8_t> connection)
    : ShortName(LinkupName(name.ShortName, 0x3f))
    , LongName(LinkupName(name.LongName, 0xff))
    , ConnectionBuffer(connection.begin(), connection.end())
{
}
