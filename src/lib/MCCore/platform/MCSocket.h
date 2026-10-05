#pragma once

// The port's thin wrapper over the operating system's sockets (Winsock on Windows, BSD sockets elsewhere), used by
// MCDirectPlay. Its .cpp is built without the precompiled header (MCCore.vcxproj) so the system headers it needs
// (winsock2.h, which drags in windows.h) never meet the game's own Win32 stand-ins. This header therefore uses only
// standard types.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// <summary>Non-blocking TCP and UDP sockets over IPv4.</summary>
namespace MCSocket
{
    /// <summary>An open socket; <see cref="InvalidHandle"/> when none.</summary>
    using Handle = std::intptr_t;

    /// <summary>No socket.</summary>
    inline constexpr Handle InvalidHandle = -1;

    /// <summary>An IPv4 address and port, both in host byte order.</summary>
    struct Address
    {
        uint32_t Ip = 0;
        uint16_t Port = 0;

        friend bool operator==(const Address&, const Address&) = default;
    };

    /// <summary>The broadcast address (255.255.255.255).</summary>
    inline constexpr uint32_t BroadcastIp = 0xffffffffu;
    /// <summary>The loopback address (127.0.0.1).</summary>
    inline constexpr uint32_t LoopbackIp = 0x7f000001u;

    /// <summary>Starts the socket library (once per process; later calls do nothing).</summary>
    /// <returns>Whether sockets are usable.</returns>
    bool Startup();

    /// <summary>Resolves <paramref name="host"/> (a dotted address or a host name) to an IPv4 address.</summary>
    std::optional<uint32_t> Resolve(std::string_view host);

    /// <summary>The address as dotted text ("192.168.0.4:28800").</summary>
    std::string ToString(const Address& address);

    /// <summary>A non-blocking TCP socket listening on <paramref name="port"/> of every interface.</summary>
    Handle ListenTcp(uint16_t port);

    /// <summary>Accepts a pending connection on <paramref name="listener"/>, made non-blocking.</summary>
    /// <returns>The connection, or <see cref="InvalidHandle"/> when none is waiting.</returns>
    Handle AcceptTcp(Handle listener, Address* from);

    /// <summary>
    /// Connects to <paramref name="to"/>, waiting up to <paramref name="timeoutMs"/> milliseconds; the connected
    /// socket is non-blocking.
    /// </summary>
    /// <returns>The connection, or <see cref="InvalidHandle"/> when it failed or timed out.</returns>
    Handle ConnectTcp(const Address& to, int timeoutMs);

    /// <summary>Sends what the connection takes of <paramref name="size"/> bytes without blocking.</summary>
    /// <returns>The bytes sent (0 when the connection is busy), or -1 when the connection failed.</returns>
    int Send(Handle socket, const void* data, size_t size);

    /// <summary>Reads what has arrived on a connection, up to <paramref name="size"/> bytes, without blocking.</summary>
    /// <returns>The bytes read (0 when nothing is waiting), or -1 when the connection closed or failed.</returns>
    int Receive(Handle socket, void* data, size_t size);

    /// <summary>
    /// A non-blocking UDP socket bound to <paramref name="port"/> (0 = any free port), allowed to broadcast.
    /// </summary>
    Handle OpenUdp(uint16_t port);

    /// <summary>Sends one datagram.</summary>
    bool SendTo(Handle socket, const Address& to, const void* data, size_t size);

    /// <summary>Reads one waiting datagram (cut to <paramref name="size"/> bytes) without blocking.</summary>
    /// <returns>Its size, 0 when none is waiting, or -1 on error.</returns>
    int ReceiveFrom(Handle socket, void* data, size_t size, Address* from);

    /// <summary>The local port <paramref name="socket"/> is bound to.</summary>
    uint16_t LocalPort(Handle socket);

    /// <summary>Closes <paramref name="socket"/> (nothing for <see cref="InvalidHandle"/>).</summary>
    void Close(Handle socket);
}

/// <summary>
/// The network, as the port service MCDirectPlay talks through: the <c>MCSocket</c> calls (which forward to the
/// current context's transport), each meaning what that function's summary says.
/// </summary>
class MCNetTransport
{
public:
    virtual ~MCNetTransport() = default;

    /// <summary>See <see cref="MCSocket::Startup"/>.</summary>
    virtual bool Startup() = 0;
    /// <summary>See <see cref="MCSocket::Resolve"/>.</summary>
    virtual std::optional<uint32_t> Resolve(std::string_view host) = 0;
    /// <summary>See <see cref="MCSocket::ListenTcp"/>.</summary>
    virtual MCSocket::Handle ListenTcp(uint16_t port) = 0;
    /// <summary>See <see cref="MCSocket::AcceptTcp"/>.</summary>
    virtual MCSocket::Handle AcceptTcp(MCSocket::Handle listener, MCSocket::Address* from) = 0;
    /// <summary>See <see cref="MCSocket::ConnectTcp"/>.</summary>
    virtual MCSocket::Handle ConnectTcp(const MCSocket::Address& to, int timeoutMs) = 0;
    /// <summary>See <see cref="MCSocket::Send"/>.</summary>
    virtual int Send(MCSocket::Handle socket, const void* data, size_t size) = 0;
    /// <summary>See <see cref="MCSocket::Receive"/>.</summary>
    virtual int Receive(MCSocket::Handle socket, void* data, size_t size) = 0;
    /// <summary>See <see cref="MCSocket::OpenUdp"/>.</summary>
    virtual MCSocket::Handle OpenUdp(uint16_t port) = 0;
    /// <summary>See <see cref="MCSocket::SendTo"/>.</summary>
    virtual bool SendTo(MCSocket::Handle socket, const MCSocket::Address& to, const void* data, size_t size) = 0;
    /// <summary>See <see cref="MCSocket::ReceiveFrom"/>.</summary>
    virtual int ReceiveFrom(MCSocket::Handle socket, void* data, size_t size, MCSocket::Address* from) = 0;
    /// <summary>See <see cref="MCSocket::LocalPort"/>.</summary>
    virtual uint16_t LocalPort(MCSocket::Handle socket) = 0;
    /// <summary>See <see cref="MCSocket::Close"/>.</summary>
    virtual void Close(MCSocket::Handle socket) = 0;
};

/// <summary>The operating system's sockets (Winsock on Windows, BSD sockets elsewhere).</summary>
class MCSocketTransport final : public MCNetTransport
{
public:
    bool Startup() override;
    std::optional<uint32_t> Resolve(std::string_view host) override;
    MCSocket::Handle ListenTcp(uint16_t port) override;
    MCSocket::Handle AcceptTcp(MCSocket::Handle listener, MCSocket::Address* from) override;
    MCSocket::Handle ConnectTcp(const MCSocket::Address& to, int timeoutMs) override;
    int Send(MCSocket::Handle socket, const void* data, size_t size) override;
    int Receive(MCSocket::Handle socket, void* data, size_t size) override;
    MCSocket::Handle OpenUdp(uint16_t port) override;
    bool SendTo(MCSocket::Handle socket, const MCSocket::Address& to, const void* data, size_t size) override;
    int ReceiveFrom(MCSocket::Handle socket, void* data, size_t size, MCSocket::Address* from) override;
    uint16_t LocalPort(MCSocket::Handle socket) override;
    void Close(MCSocket::Handle socket) override;
};
