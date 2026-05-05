#include "ClientSession.hpp"

namespace Zeus::Session
{
ClientSession::ClientSession(
    const Zeus::Net::SessionId sessionId,
    const Zeus::Net::ConnectionId connectionId,
    const Zeus::Net::UdpEndpoint& endpoint,
    const double createdAtMonotonicSeconds)
    : sessionId_(sessionId)
    , connectionId_(connectionId)
    , endpoint_(endpoint)
    , state_(SessionState::Connecting)
    , createdAtSeconds_(createdAtMonotonicSeconds)
    , lastPacketAtSeconds_(createdAtMonotonicSeconds)
{
}

void ClientSession::SetHandshakeNonces(const std::uint64_t clientNonce, const std::uint64_t serverChallengeNonce)
{
    clientHandshakeNonce_ = clientNonce;
    serverChallengeNonce_ = serverChallengeNonce;
}

ClientSession::LoadingFragmentResult ClientSession::FeedLoadingFragment(
    const std::uint64_t snapshotId,
    const std::uint16_t chunkIndex,
    const std::uint16_t chunkCount,
    const std::uint8_t* data,
    const std::size_t dataLen,
    const double nowMonotonicSeconds,
    Zeus::Net::PacketStats* stats)
{
    if (chunkCount == 0 || chunkIndex >= chunkCount)
    {
        return LoadingFragmentResult::Rejected;
    }
    if (!loadingAssembly_.has_value() || loadingAssembly_->snapshotId != snapshotId ||
        loadingAssembly_->chunkCount != chunkCount)
    {
        LoadingAssembly fresh{};
        fresh.snapshotId = snapshotId;
        fresh.chunkCount = chunkCount;
        fresh.startedAtMono = nowMonotonicSeconds;
        fresh.chunks.assign(chunkCount, {});
        fresh.received.assign(chunkCount, false);
        loadingAssembly_ = std::move(fresh);
    }
    LoadingAssembly& a = *loadingAssembly_;
    if (a.chunks.size() != static_cast<std::size_t>(chunkCount))
    {
        return LoadingFragmentResult::Rejected;
    }
    if (nowMonotonicSeconds - a.startedAtMono > 60.0)
    {
        loadingAssembly_.reset();
        return LoadingFragmentResult::Rejected;
    }
    if (a.received[chunkIndex])
    {
        return LoadingFragmentResult::InProgress;
    }
    a.chunks[chunkIndex].assign(data, data + dataLen);
    a.received[chunkIndex] = true;
    bool all = true;
    for (const bool r : a.received)
    {
        if (!r)
        {
            all = false;
            break;
        }
    }
    if (!all)
    {
        return LoadingFragmentResult::InProgress;
    }
    if (stats != nullptr)
    {
        ++stats->LoadingAssembliesCompleted;
    }
    loadingAssembly_.reset();
    return LoadingFragmentResult::Completed;
}
} // namespace Zeus::Session
