#pragma once

#include "PacketStats.hpp"
#include "UdpEndpoint.hpp"
#include "UdpServer.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Zeus::Net
{
struct FSentPacketInfo
{
    std::uint32_t Sequence = 0;
    std::uint64_t SentAtWallMs = 0;
    std::uint8_t Channel = 0;
    bool Reliable = false;
    std::uint8_t ResendCount = 0;
    std::vector<std::uint8_t> WireCopy;
};

/** Histórico de envios confiáveis + reenvio quando o ACK remoto ainda não cobre a sequência. */
class PacketAckTracker
{
public:
    void OnPacketSent(FSentPacketInfo&& info);

    void OnAckReceived(std::uint32_t ack, std::uint32_t ackBits);

    /** Reenvia cópias pendentes; incrementa `ResendCount` e atualiza `SentAtWallMs`. */
    void TickResend(
        UdpServer& udp,
        const UdpEndpoint& to,
        std::uint64_t nowWallMs,
        double resendIntervalSeconds,
        std::uint32_t maxResends,
        PacketStats* stats);

    void Clear();

private:
    std::unordered_map<std::uint32_t, FSentPacketInfo> sent_{};

    [[nodiscard]] static bool SequenceIsAcked(std::uint32_t sequence, std::uint32_t ack, std::uint32_t ackBits);
};
} // namespace Zeus::Net
