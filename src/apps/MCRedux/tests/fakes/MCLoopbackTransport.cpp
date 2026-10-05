#include "stdafx.h"
#include "MCLoopbackTransport.h"

namespace
{
    /// <summary>A dotted IPv4 address ("127.0.0.1"), or nothing.</summary>
    std::optional<uint32_t> ParseDotted(std::string_view text)
    {
        uint32_t ip = 0;
        int parts = 0;

        for (const auto part : std::views::split(text, '.'))
        {
            const std::string_view number(part.begin(), part.end());
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), value);

            if (number.empty() || error != std::errc{} || end != number.data() + number.size() || value > 255)
            {
                return std::nullopt;
            }

            ip = (ip << 8) | value;
            parts++;
        }

        return parts == 4 ? std::optional<uint32_t>(ip) : std::nullopt;
    }
}

size_t MCLoopbackTransport::OpenSockets() const
{
    std::lock_guard lock(_Lock);
    return _Sockets.size();
}

bool MCLoopbackTransport::Startup()
{
    return true;
}

std::optional<uint32_t> MCLoopbackTransport::Resolve(std::string_view host)
{
    if (MCPort::StrICmp(std::string(host).c_str(), "localhost") == 0)
    {
        return MCSocket::LoopbackIp;
    }

    return ParseDotted(host);
}

bool MCLoopbackTransport::PortTaken(Kind type, uint16_t port) const
{
    return std::ranges::any_of(_Sockets, [&](const auto& entry)
                               { return entry.second.Type == type && entry.second.Port == port; });
}

MCSocket::Handle MCLoopbackTransport::Open(Kind type, uint16_t port)
{
    if (port == 0)
    {
        do
        {
            port = _NextPort++;
        } while (PortTaken(type, port));
    }
    else if (type != Kind::Stream && PortTaken(type, port))
    {
        return MCSocket::InvalidHandle;
    }

    const MCSocket::Handle handle = _NextHandle++;
    Socket& socket = _Sockets[handle];
    socket.Type = type;
    socket.Port = port;
    return handle;
}

MCLoopbackTransport::Socket* MCLoopbackTransport::Find(MCSocket::Handle handle)
{
    const auto found = _Sockets.find(handle);
    return found == _Sockets.end() ? nullptr : &found->second;
}

MCSocket::Handle MCLoopbackTransport::ListenTcp(uint16_t port)
{
    std::lock_guard lock(_Lock);
    return Open(Kind::Listener, port);
}

MCSocket::Handle MCLoopbackTransport::AcceptTcp(MCSocket::Handle listener, MCSocket::Address* from)
{
    std::lock_guard lock(_Lock);
    Socket* socket = Find(listener);

    if (socket == nullptr || socket->Type != Kind::Listener || socket->Pending.empty())
    {
        return MCSocket::InvalidHandle;
    }

    const MCSocket::Handle accepted = socket->Pending.front();
    socket->Pending.pop_front();

    if (from != nullptr)
    {
        *from = MCSocket::Address{MCSocket::LoopbackIp, _Sockets[accepted].PeerPort};
    }

    return accepted;
}

MCSocket::Handle MCLoopbackTransport::ConnectTcp(const MCSocket::Address& to, int timeoutMs)
{
    (void)timeoutMs;
    std::lock_guard lock(_Lock);
    const auto listener =
        std::ranges::find_if(_Sockets, [&](const auto& entry)
                             { return entry.second.Type == Kind::Listener && entry.second.Port == to.Port; });

    if (listener == _Sockets.end())
    {
        return MCSocket::InvalidHandle;
    }

    const MCSocket::Handle listenerHandle = listener->first;
    const MCSocket::Handle client = Open(Kind::Stream, 0);
    const MCSocket::Handle server = Open(Kind::Stream, to.Port);
    _Sockets[client].Peer = server;
    _Sockets[client].PeerPort = to.Port;
    _Sockets[server].Peer = client;
    _Sockets[server].PeerPort = _Sockets[client].Port;
    _Sockets[listenerHandle].Pending.push_back(server);
    return client;
}

int MCLoopbackTransport::Send(MCSocket::Handle socket, const void* data, size_t size)
{
    std::lock_guard lock(_Lock);
    Socket* self = Find(socket);
    Socket* peer = self != nullptr ? Find(self->Peer) : nullptr;

    if (self == nullptr || self->Type != Kind::Stream || peer == nullptr)
    {
        return -1;
    }

    const auto* bytes = static_cast<const uint8_t*>(data);
    peer->Inbox.insert(peer->Inbox.end(), bytes, bytes + size);
    return static_cast<int>(size);
}

int MCLoopbackTransport::Receive(MCSocket::Handle socket, void* data, size_t size)
{
    std::lock_guard lock(_Lock);
    Socket* self = Find(socket);

    if (self == nullptr || self->Type != Kind::Stream)
    {
        return -1;
    }

    if (self->Inbox.empty())
    {
        // Nothing waiting: a closed peer is the end of the connection, as recv's 0 is.
        return Find(self->Peer) == nullptr ? -1 : 0;
    }

    const size_t count = std::min(size, self->Inbox.size());
    std::copy_n(self->Inbox.begin(), count, static_cast<uint8_t*>(data));
    self->Inbox.erase(self->Inbox.begin(), self->Inbox.begin() + static_cast<std::ptrdiff_t>(count));
    return static_cast<int>(count);
}

MCSocket::Handle MCLoopbackTransport::OpenUdp(uint16_t port)
{
    std::lock_guard lock(_Lock);
    return Open(Kind::Udp, port);
}

bool MCLoopbackTransport::SendTo(MCSocket::Handle socket, const MCSocket::Address& to, const void* data, size_t size)
{
    std::lock_guard lock(_Lock);
    Socket* self = Find(socket);

    if (self == nullptr || self->Type != Kind::Udp)
    {
        return false;
    }

    const MCSocket::Address from{MCSocket::LoopbackIp, self->Port};
    const auto* bytes = static_cast<const uint8_t*>(data);

    for (auto& [handle, target] : _Sockets)
    {
        if (target.Type == Kind::Udp && target.Port == to.Port)
        {
            target.Datagrams.push_back(Datagram{from, std::vector<uint8_t>(bytes, bytes + size)});
        }
    }

    // Like a real network, a datagram nobody listens for is simply lost.
    return true;
}

int MCLoopbackTransport::ReceiveFrom(MCSocket::Handle socket, void* data, size_t size, MCSocket::Address* from)
{
    std::lock_guard lock(_Lock);
    Socket* self = Find(socket);

    if (self == nullptr || self->Type != Kind::Udp)
    {
        return -1;
    }

    if (self->Datagrams.empty())
    {
        return 0;
    }

    Datagram datagram = std::move(self->Datagrams.front());
    self->Datagrams.pop_front();
    const size_t count = std::min(size, datagram.Bytes.size());
    std::copy_n(datagram.Bytes.begin(), count, static_cast<uint8_t*>(data));

    if (from != nullptr)
    {
        *from = datagram.From;
    }

    return static_cast<int>(count);
}

uint16_t MCLoopbackTransport::LocalPort(MCSocket::Handle socket)
{
    std::lock_guard lock(_Lock);
    const Socket* self = Find(socket);
    return self != nullptr ? self->Port : 0;
}

void MCLoopbackTransport::Close(MCSocket::Handle socket)
{
    std::lock_guard lock(_Lock);
    Socket* self = Find(socket);

    if (self == nullptr)
    {
        return;
    }

    // Connections a listener never accepted go with it.
    for (const MCSocket::Handle pending : self->Pending)
    {
        _Sockets.erase(pending);
    }

    _Sockets.erase(socket);
}
