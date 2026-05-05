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

    void SetPolicy(const double resendIntervalSeconds, const std::uint32_t maxResends)
    {
        resendIntervalSeconds_ = resendIntervalSeconds;
        maxResends_ = maxResends;
    }

    void TickResends(
        UdpServer& udp,
        const UdpEndpoint& to,
        const std::uint64_t nowWallMs,
        PacketStats* stats,
        std::uint32_t* giveUpsOut)
    {
        tracker_.TickResend(udp, to, nowWallMs, resendIntervalSeconds_, maxResends_, stats, giveUpsOut);
    }

    void Clear() { tracker_.Clear(); }

private:
    PacketAckTracker tracker_{};
    double resendIntervalSeconds_ = 0.25;
    std::uint32_t maxResends_ = 12;
};
} // namespace Zeus::Net
