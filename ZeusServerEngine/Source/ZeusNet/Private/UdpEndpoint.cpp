#include "UdpEndpoint.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <sstream>

namespace Zeus::Net
{
std::string UdpEndpoint::ToString() const
{
    char buf[INET_ADDRSTRLEN] = {};
#if defined(_WIN32)
    in_addr a{};
    a.s_addr = ipv4;
    (void)InetNtopA(AF_INET, &a, buf, INET_ADDRSTRLEN);
#else
    in_addr a{};
    a.s_addr = ipv4;
    (void)inet_ntop(AF_INET, &a, buf, sizeof(buf));
#endif
    const std::uint16_t portHost = ntohs(port);
    std::ostringstream oss;
    oss << buf << ':' << portHost;
    return oss.str();
}
} // namespace Zeus::Net
