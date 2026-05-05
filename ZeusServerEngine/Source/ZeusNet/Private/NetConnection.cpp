#include "NetConnection.hpp"

#include "PlatformTime.hpp"

namespace Zeus::Net
{
NetConnection::NetConnection(const ConnectionId connectionId, const UdpEndpoint& endpoint)
    : connectionId_(connectionId)
    , endpoint_(endpoint)
    , createdAtSeconds_(Zeus::Platform::NowMonotonicSeconds())
    , lastReceiveSeconds_(createdAtSeconds_)
    , lastSendSeconds_(createdAtSeconds_)
{
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
    else
    {
        // remoteSequence == lastRemoteSeq_: segundo pacote com mesma seq — não altera bitmask.
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
} // namespace Zeus::Net
