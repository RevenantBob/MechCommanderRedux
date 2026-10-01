// Built without the precompiled header (see MCCore.vcxproj and MCSocket.h): the system socket headers must not meet
// the game's Win32 stand-ins.

#include "platform/MCSocket.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cstring>
#include <mutex>

namespace
{
#ifdef _WIN32
    using NativeSocket = SOCKET;
    constexpr NativeSocket NativeInvalid = INVALID_SOCKET;

    /// <summary>Whether the last call failed only because it would have blocked.</summary>
    bool WouldBlock()
    {
        const int error = WSAGetLastError();
        return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
    }

    void CloseNative(NativeSocket socket)
    {
        closesocket(socket);
    }

    bool SetNonBlocking(NativeSocket socket)
    {
        u_long enable = 1;
        return ioctlsocket(socket, FIONBIO, &enable) == 0;
    }
#else
    using NativeSocket = int;
    constexpr NativeSocket NativeInvalid = -1;

    bool WouldBlock()
    {
        return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS;
    }

    void CloseNative(NativeSocket socket)
    {
        close(socket);
    }

    bool SetNonBlocking(NativeSocket socket)
    {
        const int flags = fcntl(socket, F_GETFL, 0);
        return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
    }
#endif

    NativeSocket ToNative(MCSocket::Handle handle)
    {
        return static_cast<NativeSocket>(handle);
    }

    MCSocket::Handle FromNative(NativeSocket socket)
    {
        return socket == NativeInvalid ? MCSocket::InvalidHandle : static_cast<MCSocket::Handle>(socket);
    }

    sockaddr_in ToSockaddr(const MCSocket::Address& address)
    {
        sockaddr_in result{};
        result.sin_family = AF_INET;
        result.sin_addr.s_addr = htonl(address.Ip);
        result.sin_port = htons(address.Port);
        return result;
    }

    MCSocket::Address FromSockaddr(const sockaddr_in& address)
    {
        return MCSocket::Address{ntohl(address.sin_addr.s_addr), ntohs(address.sin_port)};
    }
}

namespace MCSocket
{
    bool Startup()
    {
#ifdef _WIN32
        static std::once_flag once;
        static bool started = false;
        std::call_once(once,
                       []
                       {
                           WSADATA data{};
                           started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
                       });
        return started;
#else
        return true;
#endif
    }

    std::optional<uint32_t> Resolve(std::string_view host)
    {
        if (host.empty() || !Startup())
        {
            return std::nullopt;
        }

        const std::string name(host);
        in_addr numeric{};

        if (inet_pton(AF_INET, name.c_str(), &numeric) == 1)
        {
            return ntohl(numeric.s_addr);
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* results = nullptr;

        if (getaddrinfo(name.c_str(), nullptr, &hints, &results) != 0 || results == nullptr)
        {
            return std::nullopt;
        }

        const uint32_t ip = ntohl(reinterpret_cast<sockaddr_in*>(results->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(results);
        return ip;
    }

    std::string ToString(const Address& address)
    {
        return std::to_string((address.Ip >> 24) & 0xff) + "." + std::to_string((address.Ip >> 16) & 0xff) + "." +
               std::to_string((address.Ip >> 8) & 0xff) + "." + std::to_string(address.Ip & 0xff) + ":" +
               std::to_string(address.Port);
    }

    Handle ListenTcp(uint16_t port)
    {
        if (!Startup())
        {
            return InvalidHandle;
        }

        const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (socket == NativeInvalid)
        {
            return InvalidHandle;
        }

        const sockaddr_in local = ToSockaddr(Address{0, port});

        if (bind(socket, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0 || listen(socket, 8) != 0 ||
            !SetNonBlocking(socket))
        {
            CloseNative(socket);
            return InvalidHandle;
        }

        return FromNative(socket);
    }

    Handle AcceptTcp(Handle listener, Address* from)
    {
        sockaddr_in remote{};
        socklen_t remoteSize = sizeof(remote);
        const NativeSocket socket = accept(ToNative(listener), reinterpret_cast<sockaddr*>(&remote), &remoteSize);

        if (socket == NativeInvalid)
        {
            return InvalidHandle;
        }

        if (!SetNonBlocking(socket))
        {
            CloseNative(socket);
            return InvalidHandle;
        }

        const int noDelay = 1;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

        if (from != nullptr)
        {
            *from = FromSockaddr(remote);
        }

        return FromNative(socket);
    }

    Handle ConnectTcp(const Address& to, int timeoutMs)
    {
        if (!Startup())
        {
            return InvalidHandle;
        }

        const NativeSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (socket == NativeInvalid)
        {
            return InvalidHandle;
        }

        if (!SetNonBlocking(socket))
        {
            CloseNative(socket);
            return InvalidHandle;
        }

        const int noDelay = 1;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        const sockaddr_in remote = ToSockaddr(to);

        if (connect(socket, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote)) != 0)
        {
            if (!WouldBlock())
            {
                CloseNative(socket);
                return InvalidHandle;
            }

#ifdef _WIN32
            fd_set writable;
            fd_set failed;
            FD_ZERO(&writable);
            FD_ZERO(&failed);
            FD_SET(socket, &writable);
            FD_SET(socket, &failed);
            timeval wait{timeoutMs / 1000, (timeoutMs % 1000) * 1000};

            if (select(0, nullptr, &writable, &failed, &wait) <= 0 || FD_ISSET(socket, &failed))
            {
                CloseNative(socket);
                return InvalidHandle;
            }
#else
            pollfd poller{socket, POLLOUT, 0};

            if (poll(&poller, 1, timeoutMs) <= 0)
            {
                CloseNative(socket);
                return InvalidHandle;
            }
#endif

            int error = 0;
            socklen_t errorSize = sizeof(error);
            getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &errorSize);

            if (error != 0)
            {
                CloseNative(socket);
                return InvalidHandle;
            }
        }

        return FromNative(socket);
    }

    int Send(Handle socket, const void* data, size_t size)
    {
#ifdef _WIN32
        const int sent = send(ToNative(socket), static_cast<const char*>(data), static_cast<int>(size), 0);
#else
        const int sent = static_cast<int>(send(ToNative(socket), data, size, MSG_NOSIGNAL));
#endif

        if (sent < 0)
        {
            return WouldBlock() ? 0 : -1;
        }

        return sent;
    }

    int Receive(Handle socket, void* data, size_t size)
    {
#ifdef _WIN32
        const int received = recv(ToNative(socket), static_cast<char*>(data), static_cast<int>(size), 0);
#else
        const int received = static_cast<int>(recv(ToNative(socket), data, size, 0));
#endif

        if (received == 0)
        {
            return -1;
        }

        if (received < 0)
        {
            return WouldBlock() ? 0 : -1;
        }

        return received;
    }

    Handle OpenUdp(uint16_t port)
    {
        if (!Startup())
        {
            return InvalidHandle;
        }

        const NativeSocket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (socket == NativeInvalid)
        {
            return InvalidHandle;
        }

        const int enable = 1;
        setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&enable), sizeof(enable));
        const sockaddr_in local = ToSockaddr(Address{0, port});

        if (bind(socket, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0 || !SetNonBlocking(socket))
        {
            CloseNative(socket);
            return InvalidHandle;
        }

        return FromNative(socket);
    }

    bool SendTo(Handle socket, const Address& to, const void* data, size_t size)
    {
        const sockaddr_in remote = ToSockaddr(to);
#ifdef _WIN32
        return sendto(ToNative(socket), static_cast<const char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<const sockaddr*>(&remote), sizeof(remote)) >= 0;
#else
        return sendto(ToNative(socket), data, size, 0, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote)) >= 0;
#endif
    }

    int ReceiveFrom(Handle socket, void* data, size_t size, Address* from)
    {
        sockaddr_in remote{};
        socklen_t remoteSize = sizeof(remote);
#ifdef _WIN32
        const int received = recvfrom(ToNative(socket), static_cast<char*>(data), static_cast<int>(size), 0,
                                      reinterpret_cast<sockaddr*>(&remote), &remoteSize);

        if (received < 0)
        {
            // A datagram larger than the buffer arrives cut short with WSAEMSGSIZE; an ICMP "port unreachable" from
            // an earlier send shows up as WSAECONNRESET. Neither is a reason to stop reading.
            const int error = WSAGetLastError();

            if (error == WSAEMSGSIZE)
            {
                return static_cast<int>(size);
            }

            return (WouldBlock() || error == WSAECONNRESET) ? 0 : -1;
        }
#else
        const int received = static_cast<int>(
            recvfrom(ToNative(socket), data, size, 0, reinterpret_cast<sockaddr*>(&remote), &remoteSize));

        if (received < 0)
        {
            return WouldBlock() ? 0 : -1;
        }
#endif

        if (from != nullptr)
        {
            *from = FromSockaddr(remote);
        }

        return received;
    }

    uint16_t LocalPort(Handle socket)
    {
        sockaddr_in local{};
        socklen_t localSize = sizeof(local);

        if (getsockname(ToNative(socket), reinterpret_cast<sockaddr*>(&local), &localSize) != 0)
        {
            return 0;
        }

        return ntohs(local.sin_port);
    }

    void Close(Handle socket)
    {
        if (socket != InvalidHandle)
        {
            CloseNative(ToNative(socket));
        }
    }
}
