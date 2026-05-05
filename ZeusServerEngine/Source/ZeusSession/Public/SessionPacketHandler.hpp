#pragma once

#include "PacketParser.hpp"
#include "SessionNetworkSettings.hpp"

#include <cstdint>

namespace Zeus::Net
{
class NetConnectionManager;
class UdpServer;
struct UdpEndpoint;
struct PacketStats;
struct NetworkDiagnostics;
} // namespace Zeus::Net

namespace Zeus::Session
{
class SessionManager;

/** Dispatch de opcodes de controlo (connect / ping / disconnect). Sem gameplay. */
class SessionPacketHandler
{
public:
    void SetPacketStats(Zeus::Net::PacketStats* stats) { packetStats_ = stats; }

    void SetNetworkDiagnostics(Zeus::Net::NetworkDiagnostics* diag) { networkDiagnostics_ = diag; }

    void Configure(const SessionNetworkSettings& settings) { settings_ = settings; }

    void OnDatagram(
        Zeus::Net::UdpServer& udp,
        Zeus::Net::NetConnectionManager& connections,
        SessionManager& sessions,
        double nowMonotonicSeconds,
        std::uint64_t serverWallTimeMs,
        const Zeus::Protocol::PacketParser::Output& parsed,
        const Zeus::Net::UdpEndpoint& from);

    /** Após `PumpReceive`: reenvio de reliable + drenagem de filas por canal. */
    void OnTickPostNetwork(
        Zeus::Net::UdpServer& udp,
        Zeus::Net::NetConnectionManager& connections,
        SessionManager& sessions,
        double nowMonotonicSeconds,
        std::uint64_t serverWallTimeMs);

    void OnTickTimeouts(
        Zeus::Net::NetConnectionManager& connections,
        SessionManager& sessions,
        double nowMonotonicSeconds);

private:
    Zeus::Net::PacketStats* packetStats_ = nullptr;
    Zeus::Net::NetworkDiagnostics* networkDiagnostics_ = nullptr;
    SessionNetworkSettings settings_{};
};
} // namespace Zeus::Session
