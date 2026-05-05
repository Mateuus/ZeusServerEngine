#include "UdpSocket.hpp"

#include "PacketConstants.hpp"

#include <mutex>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace Zeus::Net
{
namespace
{
#if defined(_WIN32)
using Native = SOCKET;
constexpr std::intptr_t kInvalidValue = static_cast<std::intptr_t>(INVALID_SOCKET);
Native ToNative(std::intptr_t v) { return static_cast<Native>(v); }
std::intptr_t FromNative(Native s) { return static_cast<std::intptr_t>(s); }

void EnsureWsa()
{
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA wsa{};
        (void)WSAStartup(MAKEWORD(2, 2), &wsa);
    });
}
#else
using Native = int;
constexpr std::intptr_t kInvalidValue = -1;
Native ToNative(std::intptr_t v) { return static_cast<int>(v); }
std::intptr_t FromNative(Native s) { return static_cast<std::intptr_t>(s); }
#endif
} // namespace

UdpSocket::UdpSocket() = default;

UdpSocket::~UdpSocket()
{
    Close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : nativeSocket_(other.nativeSocket_)
{
    other.nativeSocket_ = kInvalidValue;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept
{
    if (this != &other)
    {
        Close();
        nativeSocket_ = other.nativeSocket_;
        other.nativeSocket_ = kInvalidValue;
    }
    return *this;
}

ZeusResult UdpSocket::Open()
{
    Close();
#if defined(_WIN32)
    EnsureWsa();
    const Native s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::Open: socket() failed.");
    }
    u_long mode = 1;
    if (ioctlsocket(s, FIONBIO, &mode) != 0)
    {
        closesocket(s);
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::Open: ioctlsocket FIONBIO failed.");
    }
    nativeSocket_ = FromNative(s);
#else
    const Native s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::Open: socket() failed.");
    }
    const int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0 || fcntl(s, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        close(s);
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::Open: fcntl O_NONBLOCK failed.");
    }
    nativeSocket_ = FromNative(s);
#endif
    return ZeusResult::Success();
}

void UdpSocket::Close()
{
#if defined(_WIN32)
    if (nativeSocket_ != kInvalidValue)
    {
        closesocket(ToNative(nativeSocket_));
        nativeSocket_ = kInvalidValue;
    }
#else
    if (nativeSocket_ >= 0)
    {
        close(ToNative(nativeSocket_));
        nativeSocket_ = kInvalidValue;
    }
#endif
}

bool UdpSocket::IsOpen() const
{
#if defined(_WIN32)
    return nativeSocket_ != kInvalidValue;
#else
    return nativeSocket_ >= 0;
#endif
}

ZeusResult UdpSocket::Bind(const std::uint16_t portHostOrder)
{
    if (!IsOpen())
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "UdpSocket::Bind: socket not open.");
    }
    const Native s = ToNative(nativeSocket_);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(portHostOrder);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::Bind: bind() failed.");
    }
    return ZeusResult::Success();
}

ZeusResult UdpSocket::TryRecvFrom(
    std::uint8_t* const buffer,
    const std::size_t bufferSize,
    std::size_t& outSize,
    UdpEndpoint& outFrom)
{
    outSize = 0;
    if (!IsOpen())
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "UdpSocket::TryRecvFrom: not open.");
    }
    const Native s = ToNative(nativeSocket_);
    if (bufferSize > static_cast<std::size_t>(0x7FFFFFFF))
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "UdpSocket::TryRecvFrom: buffer too large.");
    }
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);
    const int r = ::recvfrom(
        s,
        reinterpret_cast<char*>(buffer),
        static_cast<int>(bufferSize),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &fromLen);
#if defined(_WIN32)
    if (r == SOCKET_ERROR)
    {
        const int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK || err == WSAECONNRESET)
        {
            return ZeusResult::Success();
        }
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::TryRecvFrom: recvfrom failed.");
    }
#else
    if (r < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return ZeusResult::Success();
        }
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::TryRecvFrom: recvfrom failed.");
    }
#endif
    outSize = static_cast<std::size_t>(r);
    outFrom.ipv4 = from.sin_addr.s_addr;
    outFrom.port = from.sin_port;
    return ZeusResult::Success();
}

ZeusResult UdpSocket::SendTo(const UdpEndpoint& to, const std::uint8_t* data, const std::size_t size)
{
    if (!IsOpen())
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "UdpSocket::SendTo: not open.");
    }
    const Native s = ToNative(nativeSocket_);
    if (size > Zeus::Protocol::ZEUS_MAX_PACKET_BYTES || size > static_cast<std::size_t>(0x7FFFFFFF))
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "UdpSocket::SendTo: size too large.");
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = to.ipv4;
    addr.sin_port = to.port;
    const int r = ::sendto(
        s,
        reinterpret_cast<const char*>(data),
        static_cast<int>(size),
        0,
        reinterpret_cast<const sockaddr*>(&addr),
        sizeof(addr));
#if defined(_WIN32)
    if (r == SOCKET_ERROR || static_cast<std::size_t>(r) != size)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::SendTo: sendto failed or partial.");
    }
#else
    if (r < 0 || static_cast<std::size_t>(r) != size)
    {
        return ZeusResult::Failure(ZeusErrorCode::IoError, "UdpSocket::SendTo: sendto failed or partial.");
    }
#endif
    return ZeusResult::Success();
}
} // namespace Zeus::Net
