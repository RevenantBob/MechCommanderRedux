#pragma once

// Original source: mcx\linkup\sessionmanager.cpp (FIDPNetworkProtocol).

struct DPNAME;

/// <summary>
/// The kinds of DirectPlay connection: a <see cref="MCFidpNetworkProtocol"/>'s type, the SessionManager's current
/// connection, and (as bits) the set it found.
/// </summary>
/// <remarks>The values are the original's (SessionManager::SetConnectionType); scripted games name them in a FIT file.</remarks>
enum class MCNetProtocol : int32_t
{
    /// <summary>No connection chosen.</summary>
    None = -1,
    TcpIp = 0x01,
    Ipx = 0x02,
    Serial = 0x04,
    Modem = 0x08,
    /// <summary>The lobby's own service provider.</summary>
    Lobby = 0x10
};

/// <summary>The bit of <paramref name="protocol"/> in a set of protocols.</summary>
constexpr uint32_t ProtocolBit(MCNetProtocol protocol)
{
    return static_cast<uint32_t>(protocol);
}

/// <summary>
/// A DirectPlay connection (service provider) the machine offers: its names, the connection data DirectPlay
/// initializes it with, and its <see cref="MCNetProtocol"/>.
/// </summary>
class MCFidpNetworkProtocol
{
public:
    /// <summary>A connection named by <paramref name="name"/> (the short name cut to 63 characters, the long one to
    /// 255), with a copy of its DirectPlay <paramref name="connection"/> data.</summary>
    MCFidpNetworkProtocol(const DPNAME& name, std::span<const uint8_t> connection);

    MCFidpNetworkProtocol(const MCFidpNetworkProtocol&) = delete;
    MCFidpNetworkProtocol& operator=(const MCFidpNetworkProtocol&) = delete;

    std::string ShortName;
    std::string LongName;
    /// <summary>DirectPlay's connection data.</summary>
    std::vector<uint8_t> ConnectionBuffer;
    /// <summary>The kind of connection (<see cref="MCNetProtocol::None"/> for a service provider the game doesn't know).</summary>
    MCNetProtocol ProtocolType = MCNetProtocol::None;
};
