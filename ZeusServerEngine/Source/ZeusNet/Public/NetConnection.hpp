#pragma once

#include "NetChannel.hpp"
#include "NetTypes.hpp"
#include "PacketStats.hpp"
#include "ReliabilityLayer.hpp"
#include "SendQueue.hpp"
#include "UdpEndpoint.hpp"
#include "UdpServer.hpp"

#include <array>
#include <cstdint>

namespace Zeus::Net
{
/** Conexão UDP lógica (sem gameplay). */
class NetConnection
{
public:
    NetConnection(ConnectionId connectionId, const UdpEndpoint& endpoint);

    [[nodiscard]] ConnectionId GetConnectionId() const { return connectionId_; }
    [[nodiscard]] const UdpEndpoint& GetEndpoint() const { return endpoint_; }

    [[nodiscard]] std::uint32_t NextSequence();

    void MarkReceived(std::uint32_t remoteSequence);
    [[nodiscard]] bool IsRemoteDuplicate(std::uint32_t remoteSequence) const;

    [[nodiscard]] std::uint32_t GetAck() const { return lastRemoteSeq_; }
    [[nodiscard]] std::uint32_t GetAckBits() const { return ackBits_; }

    void MarkSent(double nowSeconds);
    void MarkReceive(double nowSeconds);

    [[nodiscard]] bool IsTimedOut(double nowSeconds, double timeoutSeconds) const;

    [[nodiscard]] bool IsConnected() const { return connected_; }
    void SetConnected(bool v) { connected_ = v; }

    /** ACK do fluxo servidor←cliente sobre pacotes enviados pelo servidor. */
    void ApplyInboundAck(const std::uint32_t ack, const std::uint32_t ackBits);

    void QueueOutbound(FQueuedPacket&& packet);

    /** Drena filas até esgotar orçamento; devolve bytes enviados ao socket. */
    std::uint32_t FlushOutboundQueues(
        UdpServer& udp,
        std::uint32_t globalBudgetBytes,
        std::uint64_t wallTimeMs,
        double nowMonotonicSeconds,
        PacketStats* stats);

    void TickReliabilityResends(UdpServer& udp, std::uint64_t wallTimeMs, PacketStats* stats);

    /** Envio imediato (ex. rejeição) — regista cópia para ACK/reenvio se `Reliable`. */
    void RegisterReliableOutboundEcho(
        std::vector<std::uint8_t>&& wireCopy,
        std::uint32_t sequence,
        Zeus::Protocol::ENetChannel channel,
        std::uint64_t wallTimeMs);

    [[nodiscard]] std::uint16_t NextReliableOrderId(const Zeus::Protocol::ENetChannel channel);

    /** `ReliableOrdered` recebido do cliente: `orderId` vem de `PacketHeader::flags`. */
    [[nodiscard]] bool TryAcceptInboundReliableOrdered(
        const Zeus::Protocol::ENetChannel channel,
        const std::uint16_t orderId);

    void ResetInboundReliableOrder(const Zeus::Protocol::ENetChannel channel);

private:
    [[nodiscard]] static std::size_t ChannelIndex(const Zeus::Protocol::ENetChannel channel);

    ConnectionId connectionId_ = kInvalidConnectionId;
    UdpEndpoint endpoint_{};

    std::uint32_t nextLocalSequence_ = 1;
    std::uint32_t lastRemoteSeq_ = 0;
    std::uint32_t ackBits_ = 0;

    double createdAtSeconds_ = 0.0;
    double lastReceiveSeconds_ = 0.0;
    double lastSendSeconds_ = 0.0;

    bool connected_ = false;

    SendQueue sendQueue_{};
    ReliabilityLayer reliability_{};
    std::array<std::uint16_t, 5> nextOutboundReliableOrder_{};
    std::array<std::uint16_t, 5> nextInboundReliableOrderExpected_{};
};
} // namespace Zeus::Net
