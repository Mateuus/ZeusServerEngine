#include "NetConnectionManager.hpp"

#include "PacketStats.hpp"
#include "UdpServer.hpp"

namespace Zeus::Net
{
namespace
{
constexpr std::size_t kMaxConnections = 1024;
}

NetConnectionManager::NetConnectionManager() = default;

std::uint64_t NetConnectionManager::EndpointKey(const UdpEndpoint& e)
{
    return (static_cast<std::uint64_t>(e.ipv4) << 32) | static_cast<std::uint64_t>(e.port);
}

NetConnection* NetConnectionManager::FindByEndpoint(const UdpEndpoint& endpoint)
{
    const auto it = endpointToId_.find(EndpointKey(endpoint));
    if (it == endpointToId_.end())
    {
        return nullptr;
    }
    const auto jt = byId_.find(it->second);
    if (jt == byId_.end())
    {
        return nullptr;
    }
    return jt->second.get();
}

NetConnection* NetConnectionManager::FindByConnectionId(const ConnectionId connectionId)
{
    const auto it = byId_.find(connectionId);
    if (it == byId_.end())
    {
        return nullptr;
    }
    return it->second.get();
}

NetConnection* NetConnectionManager::CreateConnection(const UdpEndpoint& endpoint)
{
    if (byId_.size() >= kMaxConnections)
    {
        return nullptr;
    }
    if (FindByEndpoint(endpoint) != nullptr)
    {
        return nullptr;
    }
    const ConnectionId id = nextConnectionId_++;
    auto conn = std::make_unique<NetConnection>(id, endpoint);
    NetConnection* raw = conn.get();
    byId_.emplace(id, std::move(conn));
    endpointToId_.emplace(EndpointKey(endpoint), id);
    raw->SetConnected(true);
    return raw;
}

void NetConnectionManager::Clear()
{
    byId_.clear();
    endpointToId_.clear();
    nextConnectionId_ = 1;
}

void NetConnectionManager::RemoveConnection(const ConnectionId connectionId)
{
    const auto it = byId_.find(connectionId);
    if (it == byId_.end())
    {
        return;
    }
    const UdpEndpoint ep = it->second->GetEndpoint();
    endpointToId_.erase(EndpointKey(ep));
    byId_.erase(it);
}

void NetConnectionManager::UpdateTimeouts(
    const double nowSeconds,
    const double timeoutSeconds,
    std::vector<ConnectionId>& removedOut)
{
    removedOut.clear();
    std::vector<ConnectionId> toRemove;
    for (const auto& kv : byId_)
    {
        if (kv.second->IsTimedOut(nowSeconds, timeoutSeconds))
        {
            toRemove.push_back(kv.first);
        }
    }
    for (const ConnectionId id : toRemove)
    {
        removedOut.push_back(id);
        RemoveConnection(id);
    }
}

void NetConnectionManager::FlushAllOutbound(
    UdpServer& udp,
    const std::uint32_t budgetBytes,
    const std::uint64_t wallTimeMs,
    const double nowMonotonicSeconds,
    PacketStats* stats)
{
    std::uint32_t remaining = budgetBytes;
    for (auto& kv : byId_)
    {
        if (remaining == 0)
        {
            break;
        }
        remaining -= kv.second->FlushOutboundQueues(udp, remaining, wallTimeMs, nowMonotonicSeconds, stats);
    }
}

void NetConnectionManager::TickAllReliabilityResends(
    UdpServer& udp,
    const std::uint64_t wallTimeMs,
    PacketStats* stats)
{
    for (auto& kv : byId_)
    {
        kv.second->TickReliabilityResends(udp, wallTimeMs, stats);
    }
}
} // namespace Zeus::Net
