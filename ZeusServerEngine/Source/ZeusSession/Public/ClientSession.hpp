#pragma once

#include "NetTypes.hpp"
#include "PacketStats.hpp"
#include "SessionState.hpp"
#include "UdpEndpoint.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

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

    void SetHandshakeNonces(const std::uint64_t clientNonce, const std::uint64_t serverChallengeNonce);
    [[nodiscard]] std::uint64_t GetClientHandshakeNonce() const { return clientHandshakeNonce_; }
    [[nodiscard]] std::uint64_t GetServerChallengeNonce() const { return serverChallengeNonce_; }

    enum class LoadingFragmentResult : std::uint8_t
    {
        Rejected = 0,
        InProgress = 1,
        Completed = 2
    };

    LoadingFragmentResult FeedLoadingFragment(
        std::uint64_t snapshotId,
        std::uint16_t chunkIndex,
        std::uint16_t chunkCount,
        const std::uint8_t* data,
        std::size_t dataLen,
        double nowMonotonicSeconds,
        Zeus::Net::PacketStats* stats);

private:
    Zeus::Net::SessionId sessionId_ = 0;
    Zeus::Net::ConnectionId connectionId_ = Zeus::Net::kInvalidConnectionId;
    Zeus::Net::UdpEndpoint endpoint_{};
    SessionState state_ = SessionState::Disconnected;
    double createdAtSeconds_ = 0.0;
    double lastPacketAtSeconds_ = 0.0;

    std::uint64_t clientHandshakeNonce_ = 0;
    std::uint64_t serverChallengeNonce_ = 0;

    struct LoadingAssembly
    {
        std::uint64_t snapshotId = 0;
        std::uint16_t chunkCount = 0;
        double startedAtMono = 0.0;
        std::vector<std::vector<std::uint8_t>> chunks;
        std::vector<bool> received;
    };

    std::optional<LoadingAssembly> loadingAssembly_{};
};
} // namespace Zeus::Session
