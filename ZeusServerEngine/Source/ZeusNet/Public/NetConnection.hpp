#pragma once

#include "NetTypes.hpp"
#include "UdpEndpoint.hpp"

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

private:
    ConnectionId connectionId_ = kInvalidConnectionId;
    UdpEndpoint endpoint_{};

    std::uint32_t nextLocalSequence_ = 1;
    std::uint32_t lastRemoteSeq_ = 0;
    std::uint32_t ackBits_ = 0;

    double createdAtSeconds_ = 0.0;
    double lastReceiveSeconds_ = 0.0;
    double lastSendSeconds_ = 0.0;

    bool connected_ = false;
};
} // namespace Zeus::Net
