#include "SendQueue.hpp"

#include "ChannelConfig.hpp"

namespace Zeus::Net
{
void SendQueue::Enqueue(FQueuedPacket&& packet)
{
    const std::size_t idx = ChannelIndex(packet.Channel);
    byChannel_[idx].push_back(std::move(packet));
    ++totalQueued_;
}

std::size_t SendQueue::TotalQueued() const
{
    return totalQueued_;
}

bool SendQueue::TryPopNext(
    FQueuedPacket& out,
    std::uint32_t& globalBudgetBytes,
    std::array<std::uint32_t, 5>& channelBudgetBytesLeft)
{
    const std::array<int, 5>& order = ChannelConfigs::DrainChannelIndices();
    for (const int chIdx : order)
    {
        std::deque<FQueuedPacket>& q = byChannel_[static_cast<std::size_t>(chIdx)];
        if (q.empty())
        {
            continue;
        }
        const std::size_t sz = q.front().WireBytes.size();
        if (sz == 0)
        {
            q.pop_front();
            --totalQueued_;
            continue;
        }
        if (globalBudgetBytes < sz)
        {
            continue;
        }
        std::uint32_t& chBudget = channelBudgetBytesLeft[static_cast<std::size_t>(chIdx)];
        if (chBudget < sz)
        {
            continue;
        }
        out = std::move(q.front());
        q.pop_front();
        --totalQueued_;
        globalBudgetBytes -= static_cast<std::uint32_t>(sz);
        chBudget -= static_cast<std::uint32_t>(sz);
        return true;
    }
    return false;
}

std::size_t SendQueue::ChannelIndex(const Zeus::Protocol::ENetChannel ch)
{
    const int v = static_cast<int>(ch);
    if (v < 0 || v >= 5)
    {
        return 0;
    }
    return static_cast<std::size_t>(v);
}
} // namespace Zeus::Net
