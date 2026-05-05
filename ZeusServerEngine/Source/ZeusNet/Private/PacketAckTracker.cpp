#include "PacketAckTracker.hpp"

#include "ZeusLog.hpp"

#include <sstream>
#include <string>

namespace Zeus::Net
{
namespace
{
void MaybeLogReliableResend(const std::uint32_t sequence, const std::uint8_t resendCount)
{
    if (resendCount != 1 && resendCount != 3 && resendCount != 5)
    {
        return;
    }
    std::ostringstream oss;
    oss << "[Reliable] resend sequence=" << sequence << " attempt=" << static_cast<int>(resendCount);
    ZeusLog::Info("Reliable", oss.str());
}
} // namespace

void PacketAckTracker::OnPacketSent(FSentPacketInfo&& info)
{
    if (!info.Reliable || info.WireCopy.empty())
    {
        return;
    }
    const std::uint32_t seq = info.Sequence;
    sent_.insert_or_assign(seq, std::move(info));
    {
        std::ostringstream oss;
        oss << "[Reliable] queued sequence=" << seq;
        ZeusLog::Debug("Reliable", oss.str());
    }
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
    PacketStats* stats,
    std::uint32_t* giveUpsOut)
{
    const double intervalMs = resendIntervalSeconds * 1000.0;
    for (auto it = sent_.begin(); it != sent_.end();)
    {
        FSentPacketInfo& p = it->second;
        if (!p.Reliable || p.WireCopy.empty())
        {
            ++it;
            continue;
        }
        const double elapsed = static_cast<double>(nowWallMs > p.SentAtWallMs ? (nowWallMs - p.SentAtWallMs) : 0u);
        if (elapsed < intervalMs)
        {
            ++it;
            continue;
        }
        if (p.ResendCount >= maxResends)
        {
            if (giveUpsOut != nullptr)
            {
                ++(*giveUpsOut);
            }
            if (stats != nullptr)
            {
                ++stats->ReliableGiveUps;
            }
            {
                std::ostringstream oss;
                oss << "[Reliable] failed sequence=" << it->first << " maxAttempts=" << static_cast<int>(maxResends);
                ZeusLog::Warning("Reliable", oss.str());
            }
            it = sent_.erase(it);
            continue;
        }
        (void)udp.SendTo(to, p.WireCopy.data(), p.WireCopy.size());
        p.SentAtWallMs = nowWallMs;
        ++p.ResendCount;
        MaybeLogReliableResend(it->first, p.ResendCount);
        if (stats != nullptr)
        {
            ++stats->ReliableResends;
        }
        if (p.ResendCount >= maxResends)
        {
            if (giveUpsOut != nullptr)
            {
                ++(*giveUpsOut);
            }
            if (stats != nullptr)
            {
                ++stats->ReliableGiveUps;
            }
            {
                std::ostringstream oss;
                oss << "[Reliable] failed sequence=" << it->first << " maxAttempts=" << static_cast<int>(maxResends);
                ZeusLog::Warning("Reliable", oss.str());
            }
            it = sent_.erase(it);
            continue;
        }
        ++it;
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
