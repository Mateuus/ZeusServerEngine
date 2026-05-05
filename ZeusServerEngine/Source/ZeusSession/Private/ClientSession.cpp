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
} // namespace Zeus::Session
