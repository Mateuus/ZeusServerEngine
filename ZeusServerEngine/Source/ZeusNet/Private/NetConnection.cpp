#include "NetConnection.hpp"

#include "ChannelConfig.hpp"
#include "PacketAckTracker.hpp"
#include "PlatformTime.hpp"

#include <vector>

namespace Zeus::Net
{
NetConnection::NetConnection(const ConnectionId connectionId, const UdpEndpoint& endpoint)
    : connectionId_(connectionId)
    , endpoint_(endpoint)
    , createdAtSeconds_(Zeus::Platform::NowMonotonicSeconds())
    , lastReceiveSeconds_(createdAtSeconds_)
    , lastSendSeconds_(createdAtSeconds_)
{
    nextOutboundReliableOrder_.fill(0);
    nextInboundReliableOrderExpected_.fill(0);
}

std::uint32_t NetConnection::NextSequence()
{
    return nextLocalSequence_++;
}

void NetConnection::MarkReceived(const std::uint32_t remoteSequence)
{
    if (remoteSequence > lastRemoteSeq_)
    {
        const std::uint32_t delta = remoteSequence - lastRemoteSeq_;
        if (delta >= 33u)
        {
            lastRemoteSeq_ = remoteSequence;
            ackBits_ = 0;
        }
        else
        {
            ackBits_ = (ackBits_ << delta);
            lastRemoteSeq_ = remoteSequence;
        }
    }
    else if (remoteSequence < lastRemoteSeq_)
    {
        const std::uint32_t behind = lastRemoteSeq_ - remoteSequence;
        if (behind > 0 && behind <= 32u)
        {
            ackBits_ |= (1u << (behind - 1));
        }
    }
}

bool NetConnection::IsRemoteDuplicate(const std::uint32_t remoteSequence) const
{
    if (remoteSequence > lastRemoteSeq_)
    {
        return false;
    }
    if (remoteSequence == lastRemoteSeq_)
    {
        return true;
    }
    const std::uint32_t behind = lastRemoteSeq_ - remoteSequence;
    if (behind == 0 || behind > 32u)
    {
        return true;
    }
    const std::uint32_t mask = 1u << (behind - 1);
    return (ackBits_ & mask) != 0;
}

void NetConnection::MarkSent(const double nowSeconds)
{
    lastSendSeconds_ = nowSeconds;
}

void NetConnection::MarkReceive(const double nowSeconds)
{
    lastReceiveSeconds_ = nowSeconds;
}

bool NetConnection::IsTimedOut(const double nowSeconds, const double timeoutSeconds) const
{
    return (nowSeconds - lastReceiveSeconds_) > timeoutSeconds;
}

void NetConnection::ApplyInboundAck(const std::uint32_t ack, const std::uint32_t ackBits)
{
    reliability_.OnRemoteAck(ack, ackBits);
}

void NetConnection::QueueOutbound(FQueuedPacket&& packet)
{
    sendQueue_.Enqueue(std::move(packet));
}

std::uint32_t NetConnection::FlushOutboundQueues(
    UdpServer& udp,
    const std::uint32_t globalBudgetBytes,
    const std::uint64_t wallTimeMs,
    const double nowMonotonicSeconds,
    PacketStats* stats)
{
    std::uint32_t remaining = globalBudgetBytes;
    std::array<std::uint32_t, 5> chBudget{};
    for (int i = 0; i < 5; ++i)
    {
        chBudget[static_cast<std::size_t>(i)] =
            ChannelConfigs::MaxBytesPerTickFor(static_cast<Zeus::Protocol::ENetChannel>(i));
    }
    std::uint32_t sentTotal = 0;
    FQueuedPacket pkt{};
    while (remaining > 0 && sendQueue_.TryPopNext(pkt, remaining, chBudget))
    {
        const ZeusResult r = udp.SendTo(endpoint_, pkt.WireBytes.data(), pkt.WireBytes.size());
        if (!r.Ok())
        {
            break;
        }
        MarkSent(nowMonotonicSeconds);
        sentTotal += static_cast<std::uint32_t>(pkt.WireBytes.size());
        if (stats != nullptr)
        {
            ++stats->OutboundDatagrams;
        }
        if (pkt.Reliable)
        {
            FSentPacketInfo info{};
            info.Sequence = pkt.Sequence;
            info.SentAtWallMs = wallTimeMs;
            info.Channel = static_cast<std::uint8_t>(pkt.Channel);
            info.Reliable = true;
            info.ResendCount = 0;
            info.WireCopy = std::move(pkt.WireBytes);
            reliability_.RegisterReliableSent(std::move(info));
        }
    }
    return sentTotal;
}

void NetConnection::TickReliabilityResends(
    UdpServer& udp,
    const std::uint64_t wallTimeMs,
    PacketStats* stats,
    std::uint32_t* giveUpsOut)
{
    reliability_.TickResends(udp, endpoint_, wallTimeMs, stats, giveUpsOut);
}

void NetConnection::SetReliabilityPolicy(const double resendIntervalSeconds, const std::uint32_t maxResends)
{
    reliability_.SetPolicy(resendIntervalSeconds, maxResends);
}

void NetConnection::SetOrderedInboundLimits(const std::uint32_t maxPendingPerChannel, const std::uint32_t maxGap)
{
    maxOrderedPendingPerChannel_ = maxPendingPerChannel == 0 ? 64u : maxPendingPerChannel;
    maxOrderedGap_ = maxGap == 0 ? 128u : maxGap;
}

void NetConnection::RegisterReliableOutboundEcho(
    std::vector<std::uint8_t>&& wireCopy,
    const std::uint32_t sequence,
    const Zeus::Protocol::ENetChannel channel,
    const std::uint64_t wallTimeMs)
{
    FSentPacketInfo info{};
    info.Sequence = sequence;
    info.SentAtWallMs = wallTimeMs;
    info.Channel = static_cast<std::uint8_t>(channel);
    info.Reliable = true;
    info.ResendCount = 0;
    info.WireCopy = std::move(wireCopy);
    reliability_.RegisterReliableSent(std::move(info));
}

std::uint16_t NetConnection::NextReliableOrderId(const Zeus::Protocol::ENetChannel channel)
{
    const std::size_t idx = ChannelIndex(channel);
    const std::uint16_t v = nextOutboundReliableOrder_[idx];
    ++nextOutboundReliableOrder_[idx];
    return v;
}

OrderedInboundResult NetConnection::SubmitInboundReliableOrdered(
    const Zeus::Protocol::ENetChannel channel,
    const std::uint16_t orderId,
    const std::uint16_t opcode,
    const std::uint32_t remoteSequence,
    const std::uint8_t* payload,
    const std::size_t payloadSize,
    std::vector<FOrderedInboundReleased>& outReleased)
{
    outReleased.clear();
    const std::size_t idx = ChannelIndex(channel);
    const std::uint16_t exp = nextInboundReliableOrderExpected_[idx];
    std::map<std::uint16_t, FOrderedInboundStored>& pending = orderedInboundPending_[idx];

    if (orderId < exp)
    {
        return OrderedInboundResult::DuplicateOld;
    }

    auto pushReleased = [&outReleased](const std::uint16_t op, const std::uint32_t seq, const std::uint8_t* pl, const std::size_t n) {
        FOrderedInboundReleased r{};
        r.Opcode = op;
        r.RemoteSequence = seq;
        r.Payload.assign(pl, pl + n);
        outReleased.push_back(std::move(r));
    };

    if (orderId > exp)
    {
        const std::uint32_t ahead =
            static_cast<std::uint32_t>(orderId) - static_cast<std::uint32_t>(exp);
        if (ahead > static_cast<std::uint32_t>(maxOrderedGap_))
        {
            return OrderedInboundResult::QueueFull;
        }
        if (pending.size() >= maxOrderedPendingPerChannel_)
        {
            return OrderedInboundResult::QueueFull;
        }
        if (pending.find(orderId) != pending.end())
        {
            return OrderedInboundResult::DuplicateOld;
        }
        FOrderedInboundStored st{};
        st.Opcode = opcode;
        st.RemoteSequence = remoteSequence;
        if (payload != nullptr && payloadSize > 0)
        {
            st.Payload.assign(payload, payload + payloadSize);
        }
        pending.emplace(orderId, std::move(st));
        return OrderedInboundResult::QueuedFuture;
    }

    pushReleased(opcode, remoteSequence, payload, payloadSize);
    ++nextInboundReliableOrderExpected_[idx];
    std::uint16_t nextId = static_cast<std::uint16_t>(nextInboundReliableOrderExpected_[idx]);
    for (;;)
    {
        const auto it = pending.find(nextId);
        if (it == pending.end())
        {
            break;
        }
        FOrderedInboundStored moved = std::move(it->second);
        pending.erase(it);
        pushReleased(moved.Opcode, moved.RemoteSequence, moved.Payload.data(), moved.Payload.size());
        ++nextInboundReliableOrderExpected_[idx];
        nextId = static_cast<std::uint16_t>(nextInboundReliableOrderExpected_[idx]);
    }

    return OrderedInboundResult::Delivered;
}

void NetConnection::ResetInboundReliableOrder(const Zeus::Protocol::ENetChannel channel)
{
    const std::size_t idx = ChannelIndex(channel);
    nextInboundReliableOrderExpected_[idx] = 0;
    orderedInboundPending_[idx].clear();
}

std::size_t NetConnection::ChannelIndex(const Zeus::Protocol::ENetChannel channel)
{
    const int v = static_cast<int>(channel);
    if (v < 0 || v >= 5)
    {
        return 0;
    }
    return static_cast<std::size_t>(v);
}
} // namespace Zeus::Net
