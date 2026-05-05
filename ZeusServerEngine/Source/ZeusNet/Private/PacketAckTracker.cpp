#include "PacketAckTracker.hpp"

namespace Zeus::Net
{
void PacketAckTracker::OnPacketSent(FSentPacketInfo&& info)
{
    if (!info.Reliable || info.WireCopy.empty())
    {
        return;
    }
    sent_.insert_or_assign(info.Sequence, std::move(info));
}

void PacketAckTracker::OnAckReceived(const std::uint32_t ack, const std::uint32_t ackBits)
{
    for (auto it = sent_.begin(); it != sent_.end();)
    {
        if (SequenceIsAcked(it->first, ack, ackBits))
        {
            it = sent_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void PacketAckTracker::TickResend(
    UdpServer& udp,
    const UdpEndpoint& to,
    const std::uint64_t nowWallMs,
    const double resendIntervalSeconds,
    const std::uint32_t maxResends,
    PacketStats* stats)
{
    const double intervalMs = resendIntervalSeconds * 1000.0;
    for (auto& kv : sent_)
    {
        FSentPacketInfo& p = kv.second;
        if (!p.Reliable || p.WireCopy.empty())
        {
            continue;
        }
        if (p.ResendCount >= maxResends)
        {
            continue;
        }
        const double elapsed = static_cast<double>(nowWallMs > p.SentAtWallMs ? (nowWallMs - p.SentAtWallMs) : 0u);
        if (elapsed < intervalMs)
        {
            continue;
        }
        (void)udp.SendTo(to, p.WireCopy.data(), p.WireCopy.size());
        p.SentAtWallMs = nowWallMs;
        ++p.ResendCount;
        if (stats != nullptr)
        {
            ++stats->ReliableResends;
        }
    }
}

void PacketAckTracker::Clear()
{
    sent_.clear();
}

bool PacketAckTracker::SequenceIsAcked(const std::uint32_t sequence, const std::uint32_t ack, const std::uint32_t ackBits)
{
    if (sequence > ack)
    {
        return false;
    }
    if (sequence == ack)
    {
        return true;
    }
    const std::uint32_t behind = ack - sequence;
    if (behind == 0u || behind > 32u)
    {
        return false;
    }
    const std::uint32_t mask = 1u << (behind - 1u);
    return (ackBits & mask) != 0u;
}
} // namespace Zeus::Net
