#include "SessionManager.hpp"

namespace Zeus::Session
{
namespace
{
constexpr std::size_t kMaxSessions = 1024;
}

std::size_t SessionManager::GetSessionCount() const
{
    return bySessionId_.size();
}

bool SessionManager::IsFull() const
{
    return bySessionId_.size() >= kMaxSessions;
}

ClientSession* SessionManager::CreateSession(
    const Zeus::Net::ConnectionId connectionId,
    const Zeus::Net::UdpEndpoint& endpoint,
    const double nowMonotonicSeconds)
{
    if (IsFull())
    {
        return nullptr;
    }
    if (connectionToSession_.find(connectionId) != connectionToSession_.end())
    {
        return nullptr;
    }
    const Zeus::Net::SessionId sid = nextSessionId_++;
    auto session = std::make_unique<ClientSession>(sid, connectionId, endpoint, nowMonotonicSeconds);
    ClientSession* raw = session.get();
    bySessionId_.emplace(sid, std::move(session));
    connectionToSession_.emplace(connectionId, sid);
    return raw;
}

ClientSession* SessionManager::FindByConnectionId(const Zeus::Net::ConnectionId connectionId)
{
    const auto it = connectionToSession_.find(connectionId);
    if (it == connectionToSession_.end())
    {
        return nullptr;
    }
    const auto jt = bySessionId_.find(it->second);
    if (jt == bySessionId_.end())
    {
        return nullptr;
    }
    return jt->second.get();
}

ClientSession* SessionManager::FindBySessionId(const Zeus::Net::SessionId sessionId)
{
    const auto it = bySessionId_.find(sessionId);
    if (it == bySessionId_.end())
    {
        return nullptr;
    }
    return it->second.get();
}

void SessionManager::RemoveByConnectionId(const Zeus::Net::ConnectionId connectionId)
{
    const auto it = connectionToSession_.find(connectionId);
    if (it == connectionToSession_.end())
    {
        return;
    }
    const Zeus::Net::SessionId sid = it->second;
    connectionToSession_.erase(it);
    bySessionId_.erase(sid);
}

void SessionManager::Clear()
{
    bySessionId_.clear();
    connectionToSession_.clear();
    nextSessionId_ = 1;
}
} // namespace Zeus::Session
