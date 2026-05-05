#pragma once

#include "PacketParser.hpp"
#include "SessionNetworkSettings.hpp"

#include <cstdint>
#include <string>

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

    /** Define o destino de mapa anunciado em S_TRAVEL_TO_MAP logo apos S_CONNECT_OK.
        mapName e o nome logico (ex.: "TestWorld") e mapPath e o caminho Unreal completo
        (ex.: "/Game/ThirdPerson/TestWorld"); ambos podem estar vazios para desactivar. */
    void SetTravelInfo(std::string mapName, std::string mapPath)
    {
        travelMapName_ = std::move(mapName);
        travelMapPath_ = std::move(mapPath);
    }

    const std::string& GetTravelMapName() const { return travelMapName_; }
    const std::string& GetTravelMapPath() const { return travelMapPath_; }

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
    std::string travelMapName_;
    std::string travelMapPath_;
};
} // namespace Zeus::Session
