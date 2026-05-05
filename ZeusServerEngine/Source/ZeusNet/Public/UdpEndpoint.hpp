#pragma once

#include <cstdint>
#include <string>

namespace Zeus::Net
{
/** IPv4 + porta (network byte order, como `sockaddr_in.sin_addr` / `sin_port`). */
struct UdpEndpoint
{
    std::uint32_t ipv4 = 0;
    std::uint16_t port = 0;

    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] bool operator==(const UdpEndpoint& o) const { return ipv4 == o.ipv4 && port == o.port; }
};
} // namespace Zeus::Net
