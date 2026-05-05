#pragma once

#include "NetChannel.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace Zeus::Net
{
/** Um datagrama já serializado (cabeçalho + payload). */
struct FQueuedPacket
{
    Zeus::Protocol::ENetChannel Channel = Zeus::Protocol::ENetChannel::Realtime;
    /** Sequência Zeus no cabeçalho (já fixada no buffer e aqui espelhada). */
    std::uint32_t Sequence = 0;
    std::uint8_t Priority = 5;
    bool Reliable = false;
    bool Ordered = false;
    std::uint16_t OrderId = 0;
    std::uint64_t EnqueuedAtWallMs = 0;
    std::vector<std::uint8_t> WireBytes;
};

/** Filas por canal; drenagem por prioridade global e orçamento em bytes. */
class SendQueue
{
public:
    void Enqueue(FQueuedPacket&& packet);

    [[nodiscard]] std::size_t TotalQueued() const;

    /**
     * Tenta retirar o próximo pacote respeitando prioridade de canal e `globalBudgetBytes`.
     * `channelBudgetBytesLeft[ch]` é decrementado pelo tamanho do pacote desse canal.
     */
    bool TryPopNext(
        FQueuedPacket& out,
        std::uint32_t& globalBudgetBytes,
        std::array<std::uint32_t, 5>& channelBudgetBytesLeft);

private:
    [[nodiscard]] static std::size_t ChannelIndex(const Zeus::Protocol::ENetChannel ch);

    std::array<std::deque<FQueuedPacket>, 5> byChannel_{};
    std::size_t totalQueued_ = 0;
};
} // namespace Zeus::Net
