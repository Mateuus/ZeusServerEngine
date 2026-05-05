#include "ChannelConfig.hpp"

namespace Zeus::Net
{
const std::array<FChannelConfig, 5>& ChannelConfigs::Table()
{
    static constexpr std::array<FChannelConfig, 5> kTable = {
        FChannelConfig{Zeus::Protocol::ENetChannel::Realtime,
            Zeus::Protocol::ENetDelivery::UnreliableSequenced,
            1,
            60,
            12000,
            false,
            false},
        FChannelConfig{Zeus::Protocol::ENetChannel::Gameplay,
            Zeus::Protocol::ENetDelivery::ReliableOrdered,
            2,
            25,
            8000,
            false,
            true},
        FChannelConfig{Zeus::Protocol::ENetChannel::Chat,
            Zeus::Protocol::ENetDelivery::ReliableOrdered,
            4,
            4,
            2000,
            false,
            true},
        FChannelConfig{Zeus::Protocol::ENetChannel::Visual,
            Zeus::Protocol::ENetDelivery::Unreliable,
            5,
            1,
            2000,
            false,
            false},
        FChannelConfig{Zeus::Protocol::ENetChannel::Loading,
            Zeus::Protocol::ENetDelivery::ReliableOrdered,
            3,
            10,
            16000,
            true,
            true},
    };
    return kTable;
}

std::uint8_t ChannelConfigs::PriorityFor(const Zeus::Protocol::ENetChannel channel)
{
    const int idx = static_cast<int>(channel);
    if (idx < 0 || idx >= 5)
    {
        return 99;
    }
    return Table()[static_cast<std::size_t>(idx)].Priority;
}

std::uint32_t ChannelConfigs::MaxBytesPerTickFor(const Zeus::Protocol::ENetChannel channel)
{
    const int idx = static_cast<int>(channel);
    if (idx < 0 || idx >= 5)
    {
        return 1024;
    }
    return Table()[static_cast<std::size_t>(idx)].MaxBytesPerTick;
}

const std::array<int, 5>& ChannelConfigs::DrainChannelIndices()
{
    /** Realtime(0) → Gameplay(1) → Loading(4) → Chat(2) → Visual(3) por prioridade 1,2,3,4,5. */
    static constexpr std::array<int, 5> kOrder{0, 1, 4, 2, 3};
    return kOrder;
}
} // namespace Zeus::Net
