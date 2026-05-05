#pragma once

#include "NetTypes.hpp"
#include "SessionState.hpp"
#include "UdpEndpoint.hpp"

#include <cstdint>

namespace Zeus::Session
{
/** Sessão lógica de um cliente (sem gameplay nesta fase). */
class ClientSession
{
public:
    ClientSession(
        Zeus::Net::SessionId sessionId,
        Zeus::Net::ConnectionId connectionId,
        const Zeus::Net::UdpEndpoint& endpoint,
        double createdAtMonotonicSeconds);

    [[nodiscard]] Zeus::Net::SessionId GetSessionId() const { return sessionId_; }
    [[nodiscard]] Zeus::Net::ConnectionId GetConnectionId() const { return connectionId_; }
    [[nodiscard]] const Zeus::Net::UdpEndpoint& GetEndpoint() const { return endpoint_; }

    [[nodiscard]] SessionState GetState() const { return state_; }
    void SetState(SessionState s) { state_ = s; }

    [[nodiscard]] double GetCreatedAtSeconds() const { return createdAtSeconds_; }
    [[nodiscard]] double GetLastPacketAtSeconds() const { return lastPacketAtSeconds_; }
    void SetLastPacketAtSeconds(double t) { lastPacketAtSeconds_ = t; }

private:
    Zeus::Net::SessionId sessionId_ = 0;
    Zeus::Net::ConnectionId connectionId_ = Zeus::Net::kInvalidConnectionId;
    Zeus::Net::UdpEndpoint endpoint_{};
    SessionState state_ = SessionState::Disconnected;
    double createdAtSeconds_ = 0.0;
    double lastPacketAtSeconds_ = 0.0;
};
} // namespace Zeus::Session
