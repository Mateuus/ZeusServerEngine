#pragma once

#include "NetConnection.hpp"
#include "NetTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Zeus::Net
{
class UdpServer;
struct PacketStats;

/** Gere conexões UDP lógicas por endpoint e por `ConnectionId`. */
class NetConnectionManager
{
public:
    NetConnectionManager();

    [[nodiscard]] NetConnection* FindByEndpoint(const UdpEndpoint& endpoint);
    [[nodiscard]] NetConnection* FindByConnectionId(ConnectionId connectionId);

    /** Cria nova conexão (incrementa id). Não substitui endpoint existente — usar Find primeiro. */
    NetConnection* CreateConnection(const UdpEndpoint& endpoint);

    void RemoveConnection(ConnectionId connectionId);

    void Clear();

    /** Remove conexões em silêncio há mais de `timeoutSeconds`; devolve ids removidos. */
    void UpdateTimeouts(double nowSeconds, double timeoutSeconds, std::vector<ConnectionId>& removedOut);

    [[nodiscard]] std::size_t GetConnectionCount() const { return byId_.size(); }

    void FlushAllOutbound(
        UdpServer& udp,
        std::uint32_t budgetBytes,
        std::uint64_t wallTimeMs,
        double nowMonotonicSeconds,
        PacketStats* stats);

    void TickAllReliabilityResends(UdpServer& udp, std::uint64_t wallTimeMs, PacketStats* stats);

private:
    [[nodiscard]] static std::uint64_t EndpointKey(const UdpEndpoint& e);

    ConnectionId nextConnectionId_ = 1;
    std::unordered_map<ConnectionId, std::unique_ptr<NetConnection>> byId_;
    std::unordered_map<std::uint64_t, ConnectionId> endpointToId_;
};
} // namespace Zeus::Net
