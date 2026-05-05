#pragma once

#include "ClientSession.hpp"
#include "NetTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Zeus::Session
{
/** Gere sessões por `sessionId` e `connectionId` (máx. 1024). */
class SessionManager
{
public:
    [[nodiscard]] std::size_t GetSessionCount() const;

    [[nodiscard]] bool IsFull() const;

    /**
     * Cria sessão em estado `Connecting`. O chamador deve passar a `Connected` após `S_CONNECT_OK`.
     * @return nullptr se cheio ou `connectionId` já existir.
     */
    ClientSession* CreateSession(
        Zeus::Net::ConnectionId connectionId,
        const Zeus::Net::UdpEndpoint& endpoint,
        double nowMonotonicSeconds);

    [[nodiscard]] ClientSession* FindByConnectionId(Zeus::Net::ConnectionId connectionId);
    [[nodiscard]] ClientSession* FindBySessionId(Zeus::Net::SessionId sessionId);

    void RemoveByConnectionId(Zeus::Net::ConnectionId connectionId);

    void Clear();

private:
    Zeus::Net::SessionId nextSessionId_ = 1;
    std::unordered_map<Zeus::Net::SessionId, std::unique_ptr<ClientSession>> bySessionId_;
    std::unordered_map<Zeus::Net::ConnectionId, Zeus::Net::SessionId> connectionToSession_;
};
} // namespace Zeus::Session
