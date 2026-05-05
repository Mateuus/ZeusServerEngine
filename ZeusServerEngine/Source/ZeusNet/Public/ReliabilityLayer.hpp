#pragma once

#include "PacketAckTracker.hpp"
#include "PacketStats.hpp"
#include "UdpEndpoint.hpp"
#include "UdpServer.hpp"

#include <cstdint>

namespace Zeus::Net
{
/** Fachada fina sobre `PacketAckTracker` (ACK/remoção/reenvio de reliable). */
class ReliabilityLayer
{
public:
    void RegisterReliableSent(FSentPacketInfo&& info) { tracker_.OnPacketSent(std::move(info)); }

    void OnRemoteAck(const std::uint32_t ack, const std::uint32_t ackBits) { tracker_.OnAckReceived(ack, ackBits); }

    void TickResends(
        UdpServer& udp,
        const UdpEndpoint& to,
        const std::uint64_t nowWallMs,
        PacketStats* stats)
    {
        tracker_.TickResend(udp, to, nowWallMs, 0.25, 12, stats);
    }

    void Clear() { tracker_.Clear(); }

private:
    PacketAckTracker tracker_{};
};
} // namespace Zeus::Net
