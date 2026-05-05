#pragma once

#include "NetChannel.hpp"
#include "NetDelivery.hpp"

#include <array>
#include <cstdint>

namespace Zeus::Net
{
/** Metadados por canal (prioridade, orçamento, fragmentação) — alinhado à referência GNS. */
struct FChannelConfig
{
    Zeus::Protocol::ENetChannel Channel = Zeus::Protocol::ENetChannel::Realtime;
    Zeus::Protocol::ENetDelivery DefaultDelivery = Zeus::Protocol::ENetDelivery::Unreliable;
    std::uint8_t Priority = 5;
    std::uint16_t Weight = 1;
    std::uint32_t MaxBytesPerTick = 4096;
    bool AllowFragmentation = false;
    bool AllowResend = false;
};

/** Tabela estática por `ENetChannel` (índice = valor do enum). */
class ChannelConfigs
{
public:
    [[nodiscard]] static const std::array<FChannelConfig, 5>& Table();

    [[nodiscard]] static std::uint8_t PriorityFor(const Zeus::Protocol::ENetChannel channel);

    [[nodiscard]] static std::uint32_t MaxBytesPerTickFor(const Zeus::Protocol::ENetChannel channel);

    /** Ordem de drenagem: prioridade crescente (menor número = mais urgente). */
    [[nodiscard]] static const std::array<int, 5>& DrainChannelIndices();
};
} // namespace Zeus::Net
