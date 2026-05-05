#pragma once

#include "PacketParser.hpp"

#include <cstdint>

namespace Zeus::Net
{
class NetConnectionManager;
class UdpServer;
struct UdpEndpoint;
} // namespace Zeus::Net

namespace Zeus::Session
{
class SessionManager;

/** Dispatch de opcodes de controlo (connect / ping / disconnect). Sem gameplay. */
class SessionPacketHandler
{
public:
    void OnDatagram(
        Zeus::Net::UdpServer& udp,
        Zeus::Net::NetConnectionManager& connections,
        SessionManager& sessions,
        double nowMonotonicSeconds,
        std::uint64_t serverWallTimeMs,
        const Zeus::Protocol::PacketParser::Output& parsed,
        const Zeus::Net::UdpEndpoint& from);

    void OnTickTimeouts(
        Zeus::Net::NetConnectionManager& connections,
        SessionManager& sessions,
        double nowMonotonicSeconds);
};
} // namespace Zeus::Session
