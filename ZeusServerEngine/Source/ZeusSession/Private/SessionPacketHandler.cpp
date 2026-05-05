#include "SessionPacketHandler.hpp"

#include "ClientSession.hpp"
#include "ConnectRejectReason.hpp"
#include "NetChannel.hpp"
#include "NetConnection.hpp"
#include "NetConnectionManager.hpp"
#include "NetDelivery.hpp"
#include "NetTypes.hpp"
#include "Opcodes.hpp"
#include "PacketBuilder.hpp"
#include "PacketConstants.hpp"
#include "PacketHeader.hpp"
#include "SessionManager.hpp"
#include "SessionState.hpp"
#include "SessionWire.hpp"
#include "UdpEndpoint.hpp"
#include "UdpServer.hpp"
#include "ZeusLog.hpp"

#include <functional>
#include <sstream>
#include <string>

namespace Zeus::Session
{
namespace
{
constexpr std::uint32_t kHeartbeatIntervalMs = 5000;
constexpr double kConnectionIdleTimeoutSeconds = 30.0;

std::string OpcodeName(const std::uint16_t op)
{
    switch (static_cast<Zeus::Protocol::EOpcode>(op))
    {
    case Zeus::Protocol::EOpcode::C_CONNECT_REQUEST:
        return "C_CONNECT_REQUEST";
    case Zeus::Protocol::EOpcode::S_CONNECT_OK:
        return "S_CONNECT_OK";
    case Zeus::Protocol::EOpcode::S_CONNECT_REJECT:
        return "S_CONNECT_REJECT";
    case Zeus::Protocol::EOpcode::C_PING:
        return "C_PING";
    case Zeus::Protocol::EOpcode::S_PONG:
        return "S_PONG";
    case Zeus::Protocol::EOpcode::C_DISCONNECT:
        return "C_DISCONNECT";
    case Zeus::Protocol::EOpcode::S_DISCONNECT_OK:
        return "S_DISCONNECT_OK";
    default:
        return "opcode=" + std::to_string(op);
    }
}

ZeusResult SendOne(
    Zeus::Net::UdpServer& udp,
    const Zeus::Net::UdpEndpoint& to,
    Zeus::Net::NetConnection* conn,
    const std::uint16_t opcode,
    const Zeus::Protocol::ENetChannel channel,
    const Zeus::Protocol::ENetDelivery delivery,
    const Zeus::Net::ConnectionId headerConnectionId,
    const double nowMono,
    const std::function<ZeusResult(Zeus::Protocol::PacketWriter&)>& writePayload)
{
    Zeus::Protocol::PacketBuilder builder;
    builder.Reset();
    ZeusResult w = writePayload(builder.PayloadWriter());
    if (!w.Ok())
    {
        return w;
    }

    Zeus::Protocol::PacketHeader header{};
    header.magic = Zeus::Protocol::ZEUS_PACKET_MAGIC;
    header.version = Zeus::Protocol::ZEUS_PROTOCOL_VERSION;
    header.headerSize = static_cast<std::uint16_t>(Zeus::Protocol::ZEUS_WIRE_HEADER_BYTES);
    header.opcode = opcode;
    header.channel = static_cast<std::uint8_t>(channel);
    header.delivery = static_cast<std::uint8_t>(delivery);
    if (conn != nullptr)
    {
        header.sequence = conn->NextSequence();
        header.ack = conn->GetAck();
        header.ackBits = conn->GetAckBits();
        conn->MarkSent(nowMono);
    }
    else
    {
        header.sequence = 1;
        header.ack = 0;
        header.ackBits = 0;
    }
    header.connectionId = headerConnectionId;
    header.flags = 0;

    const ZeusResult fin = builder.Finalize(header);
    if (!fin.Ok())
    {
        return fin;
    }
    return udp.SendTo(to, builder.Buffer(), builder.ByteSize());
}

void LogPacketIn(const Zeus::Net::UdpEndpoint& from, const std::uint16_t opcode, const std::uint32_t sequence)
{
    std::ostringstream oss;
    oss << "Packet received endpoint=" << from.ToString() << " opcode=" << OpcodeName(opcode) << " sequence=" << sequence;
    ZeusLog::Info("Net", oss.str());
}
} // namespace

void SessionPacketHandler::OnDatagram(
    Zeus::Net::UdpServer& udp,
    Zeus::Net::NetConnectionManager& connections,
    SessionManager& sessions,
    const double nowMonotonicSeconds,
    const std::uint64_t serverWallTimeMs,
    const Zeus::Protocol::PacketParser::Output& parsed,
    const Zeus::Net::UdpEndpoint& from)
{
    const std::uint16_t op = parsed.header.opcode;

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_CONNECT_REQUEST))
    {
        LogPacketIn(from, op, parsed.header.sequence);
        Zeus::Net::NetConnection* nc = connections.FindByEndpoint(from);
        ClientSession* existingSession = nullptr;
        if (nc != nullptr)
        {
            existingSession = sessions.FindByConnectionId(nc->GetConnectionId());
            if (existingSession != nullptr && existingSession->GetState() != SessionState::Connected)
            {
                sessions.RemoveByConnectionId(nc->GetConnectionId());
                existingSession = nullptr;
            }
        }

        Zeus::Protocol::ConnectRequestPayload body{};
        const ZeusResult parsedBody =
            Zeus::Protocol::ReadConnectRequestPayload(parsed.payload, parsed.payloadSize, body);
        if (!parsedBody.Ok())
        {
            if (nc != nullptr)
            {
                (void)SendOne(
                    udp,
                    from,
                    nc,
                    static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                    Zeus::Protocol::ENetChannel::Loading,
                    Zeus::Protocol::ENetDelivery::ReliableOrdered,
                    nc->GetConnectionId(),
                    nowMonotonicSeconds,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid connect payload.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                    });
                sessions.RemoveByConnectionId(nc->GetConnectionId());
                connections.RemoveConnection(nc->GetConnectionId());
            }
            else
            {
                (void)SendOne(
                    udp,
                    from,
                    nullptr,
                    static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                    Zeus::Protocol::ENetChannel::Loading,
                    Zeus::Protocol::ENetDelivery::ReliableOrdered,
                    Zeus::Net::kInvalidConnectionId,
                    nowMonotonicSeconds,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid connect payload.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                    });
            }
            return;
        }

        if (existingSession != nullptr && existingSession->GetState() == SessionState::Connected)
        {
            if (nc != nullptr && nc->IsRemoteDuplicate(parsed.header.sequence))
            {
                return;
            }
            if (nc != nullptr)
            {
                nc->MarkReceived(parsed.header.sequence);
                nc->MarkReceive(nowMonotonicSeconds);
            }
            existingSession->SetLastPacketAtSeconds(nowMonotonicSeconds);
            Zeus::Protocol::ConnectOkPayload ok{};
            ok.connectionId = existingSession->GetConnectionId();
            ok.sessionId = existingSession->GetSessionId();
            ok.serverTimeMs = serverWallTimeMs;
            ok.heartbeatIntervalMs = kHeartbeatIntervalMs;
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_OK),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                ok.connectionId,
                nowMonotonicSeconds,
                [&ok](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectOkPayload(w, ok); });
            ZeusLog::Info("Net", std::string("Sent S_CONNECT_OK connectionId=") + std::to_string(ok.connectionId) + " sessionId=" + std::to_string(ok.sessionId) + " (idempotent)");
            return;
        }

        if (nc == nullptr)
        {
            nc = connections.CreateConnection(from);
            if (nc == nullptr)
            {
                (void)SendOne(
                    udp,
                    from,
                    nullptr,
                    static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                    Zeus::Protocol::ENetChannel::Loading,
                    Zeus::Protocol::ENetDelivery::ReliableOrdered,
                    Zeus::Net::kInvalidConnectionId,
                    nowMonotonicSeconds,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::ServerFull);
                        rj.reasonMessage = "Server full.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                    });
                return;
            }
        }

        if (body.clientProtocolVersion != Zeus::Protocol::ZEUS_PROTOCOL_VERSION)
        {
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                nc->GetConnectionId(),
                nowMonotonicSeconds,
                [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                    Zeus::Protocol::ConnectRejectPayload rj{};
                    rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidProtocolVersion);
                    rj.reasonMessage = "Invalid protocol version.";
                    return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                });
            sessions.RemoveByConnectionId(nc->GetConnectionId());
            connections.RemoveConnection(nc->GetConnectionId());
            return;
        }

        if (sessions.IsFull())
        {
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                nc->GetConnectionId(),
                nowMonotonicSeconds,
                [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                    Zeus::Protocol::ConnectRejectPayload rj{};
                    rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::ServerFull);
                    rj.reasonMessage = "Server full.";
                    return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                });
            connections.RemoveConnection(nc->GetConnectionId());
            return;
        }

        ClientSession* session = sessions.CreateSession(nc->GetConnectionId(), from, nowMonotonicSeconds);
        if (session == nullptr)
        {
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_REJECT),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                nc->GetConnectionId(),
                nowMonotonicSeconds,
                [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                    Zeus::Protocol::ConnectRejectPayload rj{};
                    rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::ServerFull);
                    rj.reasonMessage = "Server full.";
                    return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                });
            connections.RemoveConnection(nc->GetConnectionId());
            return;
        }

        nc->MarkReceived(parsed.header.sequence);
        nc->MarkReceive(nowMonotonicSeconds);
        session->SetState(SessionState::Connected);
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);

        Zeus::Protocol::ConnectOkPayload ok{};
        ok.connectionId = nc->GetConnectionId();
        ok.sessionId = session->GetSessionId();
        ok.serverTimeMs = serverWallTimeMs;
        ok.heartbeatIntervalMs = kHeartbeatIntervalMs;

        (void)SendOne(
            udp,
            from,
            nc,
            static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_OK),
            Zeus::Protocol::ENetChannel::Loading,
            Zeus::Protocol::ENetDelivery::ReliableOrdered,
            ok.connectionId,
            nowMonotonicSeconds,
            [&ok](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectOkPayload(w, ok); });

        std::ostringstream slog;
        slog << "Created sessionId=" << session->GetSessionId() << " connectionId=" << nc->GetConnectionId() << " endpoint=" << from.ToString();
        ZeusLog::Info("Session", slog.str());
        ZeusLog::Info("Session", std::string("Connected sessionId=") + std::to_string(session->GetSessionId()) + " connectionId=" + std::to_string(nc->GetConnectionId()));
        ZeusLog::Info(
            "Net",
            std::string("Sent S_CONNECT_OK connectionId=") + std::to_string(ok.connectionId) + " sessionId=" + std::to_string(ok.sessionId));
        return;
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_PING))
    {
        Zeus::Net::NetConnection* nc = connections.FindByConnectionId(parsed.header.connectionId);
        if (nc == nullptr || nc->GetEndpoint() != from)
        {
            return;
        }
        ClientSession* session = sessions.FindByConnectionId(nc->GetConnectionId());
        if (session == nullptr || session->GetState() != SessionState::Connected)
        {
            return;
        }
        if (nc->IsRemoteDuplicate(parsed.header.sequence))
        {
            return;
        }

        Zeus::Protocol::PingPayload pingBody{};
        if (!Zeus::Protocol::ReadPingPayload(parsed.payload, parsed.payloadSize, pingBody).Ok())
        {
            return;
        }

        LogPacketIn(from, op, parsed.header.sequence);
        nc->MarkReceived(parsed.header.sequence);
        nc->MarkReceive(nowMonotonicSeconds);
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);

        Zeus::Protocol::PongPayload pong{};
        pong.clientTimeMs = pingBody.clientTimeMs;
        pong.serverTimeMs = serverWallTimeMs;
        (void)SendOne(
            udp,
            from,
            nc,
            static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_PONG),
            Zeus::Protocol::ENetChannel::Gameplay,
            Zeus::Protocol::ENetDelivery::Unreliable,
            nc->GetConnectionId(),
            nowMonotonicSeconds,
            [&pong](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WritePongPayload(w, pong); });
        ZeusLog::Info(
            "Net",
            std::string("Sent S_PONG connectionId=") + std::to_string(nc->GetConnectionId()) + " sequence=" + std::to_string(parsed.header.sequence));
        return;
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_DISCONNECT))
    {
        Zeus::Net::NetConnection* nc = connections.FindByConnectionId(parsed.header.connectionId);
        if (nc == nullptr || nc->GetEndpoint() != from)
        {
            return;
        }
        ClientSession* session = sessions.FindByConnectionId(nc->GetConnectionId());
        if (session == nullptr || session->GetState() != SessionState::Connected)
        {
            return;
        }
        if (nc->IsRemoteDuplicate(parsed.header.sequence))
        {
            return;
        }

        Zeus::Protocol::DisconnectPayload disc{};
        if (!Zeus::Protocol::ReadDisconnectPayload(parsed.payload, parsed.payloadSize, disc).Ok())
        {
            return;
        }

        LogPacketIn(from, op, parsed.header.sequence);
        nc->MarkReceived(parsed.header.sequence);
        nc->MarkReceive(nowMonotonicSeconds);

        Zeus::Protocol::DisconnectOkPayload dok{};
        dok.serverTimeMs = serverWallTimeMs;
        (void)SendOne(
            udp,
            from,
            nc,
            static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_DISCONNECT_OK),
            Zeus::Protocol::ENetChannel::Gameplay,
            Zeus::Protocol::ENetDelivery::Reliable,
            nc->GetConnectionId(),
            nowMonotonicSeconds,
            [&dok](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteDisconnectOkPayload(w, dok); });

        const Zeus::Net::SessionId removedSessionId = session->GetSessionId();
        const Zeus::Net::ConnectionId removedConnId = nc->GetConnectionId();
        std::ostringstream dlog;
        dlog << "Disconnect requested sessionId=" << removedSessionId << " reasonCode=" << disc.reasonCode;
        ZeusLog::Info("Session", dlog.str());
        sessions.RemoveByConnectionId(removedConnId);
        connections.RemoveConnection(removedConnId);
        ZeusLog::Info(
            "Session",
            std::string("Removed sessionId=") + std::to_string(removedSessionId) + " connectionId=" + std::to_string(removedConnId));
        return;
    }
}

void SessionPacketHandler::OnTickTimeouts(
    Zeus::Net::NetConnectionManager& connections,
    SessionManager& sessions,
    const double nowMonotonicSeconds)
{
    std::vector<Zeus::Net::ConnectionId> removed;
    connections.UpdateTimeouts(nowMonotonicSeconds, kConnectionIdleTimeoutSeconds, removed);
    for (const Zeus::Net::ConnectionId id : removed)
    {
        sessions.RemoveByConnectionId(id);
        ZeusLog::Info("Net", std::string("Connection timed out connectionId=") + std::to_string(id));
    }
}
} // namespace Zeus::Session
