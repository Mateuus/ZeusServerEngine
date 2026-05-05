#include "SessionPacketHandler.hpp"

#include "ChannelConfig.hpp"
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
#include "SendQueue.hpp"
#include "NetworkDiagnostics.hpp"
#include "SessionManager.hpp"
#include "SessionOpcodeRules.hpp"
#include "SessionState.hpp"
#include "SessionWire.hpp"
#include "UdpEndpoint.hpp"
#include "UdpServer.hpp"
#include "ZeusLog.hpp"

#include <cctype>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace Zeus::Session
{
namespace
{
constexpr std::uint32_t kHeartbeatIntervalMs = 5000;

void TrimClientBuildInPlace(std::string& s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    {
        s.erase(0, 1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    {
        s.pop_back();
    }
}

[[nodiscard]] bool ClientBuildLooksValid(std::string& clientBuild)
{
    TrimClientBuildInPlace(clientBuild);
    if (clientBuild.empty() || clientBuild.size() > 256)
    {
        return false;
    }
    return true;
}

[[nodiscard]] bool IsReliableDelivery(const Zeus::Protocol::ENetDelivery d)
{
    return d == Zeus::Protocol::ENetDelivery::Reliable || d == Zeus::Protocol::ENetDelivery::ReliableOrdered;
}

[[nodiscard]] bool IsOrderedDelivery(const Zeus::Protocol::ENetDelivery d)
{
    return d == Zeus::Protocol::ENetDelivery::ReliableOrdered || d == Zeus::Protocol::ENetDelivery::UnreliableSequenced;
}

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
    case Zeus::Protocol::EOpcode::S_CONNECT_CHALLENGE:
        return "S_CONNECT_CHALLENGE";
    case Zeus::Protocol::EOpcode::C_CONNECT_RESPONSE:
        return "C_CONNECT_RESPONSE";
    case Zeus::Protocol::EOpcode::C_PING:
        return "C_PING";
    case Zeus::Protocol::EOpcode::S_PONG:
        return "S_PONG";
    case Zeus::Protocol::EOpcode::C_DISCONNECT:
        return "C_DISCONNECT";
    case Zeus::Protocol::EOpcode::S_DISCONNECT_OK:
        return "S_DISCONNECT_OK";
    case Zeus::Protocol::EOpcode::C_LOADING_FRAGMENT:
        return "C_LOADING_FRAGMENT";
    case Zeus::Protocol::EOpcode::S_LOADING_ASSEMBLED_OK:
        return "S_LOADING_ASSEMBLED_OK";
    case Zeus::Protocol::EOpcode::S_TRAVEL_TO_MAP:
        return "S_TRAVEL_TO_MAP";
    case Zeus::Protocol::EOpcode::C_TEST_RELIABLE:
        return "C_TEST_RELIABLE";
    case Zeus::Protocol::EOpcode::S_TEST_RELIABLE:
        return "S_TEST_RELIABLE";
    case Zeus::Protocol::EOpcode::C_TEST_ORDERED:
        return "C_TEST_ORDERED";
    case Zeus::Protocol::EOpcode::S_TEST_ORDERED:
        return "S_TEST_ORDERED";
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
    const std::uint64_t wallMs,
    const bool bypassQueue,
    Zeus::Net::PacketStats* stats,
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
    }
    else
    {
        header.sequence = 1;
        header.ack = 0;
        header.ackBits = 0;
    }
    header.connectionId = headerConnectionId;
    if (conn != nullptr && IsOrderedDelivery(delivery))
    {
        header.flags = conn->NextReliableOrderId(channel);
    }
    else
    {
        header.flags = 0;
    }

    const ZeusResult fin = builder.Finalize(header);
    if (!fin.Ok())
    {
        return fin;
    }

    const std::uint8_t* buf = builder.Buffer();
    const std::size_t n = builder.ByteSize();

    if (bypassQueue || conn == nullptr)
    {
        const ZeusResult sr = udp.SendTo(to, buf, n);
        if (conn != nullptr)
        {
            conn->MarkSent(nowMono);
            if (sr.Ok() && IsReliableDelivery(delivery))
            {
                std::vector<std::uint8_t> copy(buf, buf + n);
                conn->RegisterReliableOutboundEcho(std::move(copy), header.sequence, channel, wallMs);
            }
        }
        if (stats != nullptr && sr.Ok())
        {
            ++stats->OutboundDatagrams;
        }
        return sr;
    }

    Zeus::Net::FQueuedPacket qp{};
    qp.Channel = channel;
    qp.Sequence = header.sequence;
    qp.Priority = Zeus::Net::ChannelConfigs::PriorityFor(channel);
    qp.Reliable = IsReliableDelivery(delivery);
    qp.Ordered = (delivery == Zeus::Protocol::ENetDelivery::ReliableOrdered);
    qp.OrderId = header.flags;
    qp.EnqueuedAtWallMs = wallMs;
    qp.WireBytes.assign(buf, buf + n);
    conn->QueueOutbound(std::move(qp));
    return ZeusResult::Success();
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
    if (packetStats_ != nullptr)
    {
        ++packetStats_->DatagramsReceived;
    }

    if (settings_.networkDebugAck)
    {
        std::ostringstream ackoss;
        ackoss << "[NetAck] rx sequence=" << parsed.header.sequence << " ack=" << parsed.header.ack << " ackBits=0x"
               << std::hex << parsed.header.ackBits << std::dec;
        ZeusLog::Info("NetAck", ackoss.str());
    }

    const std::uint16_t op = parsed.header.opcode;
    const auto opcodeEnum = static_cast<Zeus::Protocol::EOpcode>(op);

    Zeus::Net::NetConnection* ncRules = nullptr;
    if (opcodeEnum == Zeus::Protocol::EOpcode::C_CONNECT_REQUEST)
    {
        ncRules = connections.FindByEndpoint(from);
    }
    else if (parsed.header.connectionId != 0u)
    {
        ncRules = connections.FindByConnectionId(parsed.header.connectionId);
        if (ncRules != nullptr && ncRules->GetEndpoint() != from)
        {
            ncRules = nullptr;
        }
    }

    ClientSession* sessRules = nullptr;
    if (ncRules != nullptr)
    {
        sessRules = sessions.FindByConnectionId(ncRules->GetConnectionId());
    }

    const OpcodeRuleResult rr = SessionOpcodeRules::ValidateClientOpcode(
        opcodeEnum,
        static_cast<Zeus::Protocol::ENetChannel>(parsed.header.channel),
        static_cast<Zeus::Protocol::ENetDelivery>(parsed.header.delivery),
        ncRules != nullptr,
        sessRules);
    if (rr != OpcodeRuleResult::Ok)
    {
        if (packetStats_ != nullptr)
        {
            ++packetStats_->DatagramsRejectedOpcodeRules;
        }
        return;
    }

    if (ncRules != nullptr)
    {
        ncRules->ApplyInboundAck(parsed.header.ack, parsed.header.ackBits);
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_CONNECT_REQUEST))
    {
        LogPacketIn(from, op, parsed.header.sequence);
        Zeus::Net::NetConnection* nc = connections.FindByEndpoint(from);
        ClientSession* existingSession = nullptr;
        if (nc != nullptr)
        {
            existingSession = sessions.FindByConnectionId(nc->GetConnectionId());
            if (existingSession != nullptr && existingSession->GetState() != SessionState::Connected &&
                existingSession->GetState() != SessionState::Connecting)
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid connect payload.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                    });
            }
            return;
        }

        const bool idempotentConnected =
            existingSession != nullptr && existingSession->GetState() == SessionState::Connected;
        if (!idempotentConnected && !ClientBuildLooksValid(body.clientBuild))
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid client build.";
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
                    [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid client build.";
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
                serverWallTimeMs,
                false,
                packetStats_,
                [&ok](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectOkPayload(w, ok); });
            ZeusLog::Info(
                "Net",
                std::string("Sent S_CONNECT_OK connectionId=") + std::to_string(ok.connectionId) + " sessionId=" + std::to_string(ok.sessionId) + " (idempotent)");
            if (!travelMapName_.empty() || !travelMapPath_.empty())
            {
                Zeus::Protocol::TravelToMapPayload travel{};
                travel.mapName = travelMapName_;
                travel.mapPath = travelMapPath_;
                travel.serverTimeMs = serverWallTimeMs;
                (void)SendOne(
                    udp,
                    from,
                    nc,
                    static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_TRAVEL_TO_MAP),
                    Zeus::Protocol::ENetChannel::Loading,
                    Zeus::Protocol::ENetDelivery::ReliableOrdered,
                    ok.connectionId,
                    nowMonotonicSeconds,
                    serverWallTimeMs,
                    false,
                    packetStats_,
                    [&travel](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteTravelToMapPayload(w, travel); });
                ZeusLog::Info(
                    "Session",
                    std::string("Sent S_TRAVEL_TO_MAP map=").append(travel.mapName).append(" path=").append(travel.mapPath).append(" (idempotent)"));
            }
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
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
                serverWallTimeMs,
                true,
                packetStats_,
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
                serverWallTimeMs,
                true,
                packetStats_,
                [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                    Zeus::Protocol::ConnectRejectPayload rj{};
                    rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::ServerFull);
                    rj.reasonMessage = "Server full.";
                    return Zeus::Protocol::WriteConnectRejectPayload(w, rj);
                });
            connections.RemoveConnection(nc->GetConnectionId());
            return;
        }

        if (existingSession != nullptr && existingSession->GetState() == SessionState::Connecting)
        {
            if (nc->IsRemoteDuplicate(parsed.header.sequence))
            {
                return;
            }
            nc->MarkReceived(parsed.header.sequence);
            nc->MarkReceive(nowMonotonicSeconds);
            const std::uint64_t serverNonce = existingSession->GetServerChallengeNonce();
            existingSession->SetHandshakeNonces(body.clientNonce, serverNonce);
            Zeus::Protocol::ConnectChallengePayload ch{};
            ch.serverNonce = serverNonce;
            ch.connectionId = nc->GetConnectionId();
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_CHALLENGE),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                nc->GetConnectionId(),
                nowMonotonicSeconds,
                serverWallTimeMs,
                false,
                packetStats_,
                [&ch](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectChallengePayload(w, ch); });
            ZeusLog::Info("Net", "Sent S_CONNECT_CHALLENGE (handshake repeat/update)");
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
                serverWallTimeMs,
                true,
                packetStats_,
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
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);

        const std::uint64_t serverNonce =
            (static_cast<std::uint64_t>(nc->GetConnectionId()) * 1315423911ull) ^
            static_cast<std::uint64_t>(serverWallTimeMs ^ (body.clientNonce * 7ull));
        session->SetHandshakeNonces(body.clientNonce, serverNonce);

        Zeus::Protocol::ConnectChallengePayload ch{};
        ch.serverNonce = serverNonce;
        ch.connectionId = nc->GetConnectionId();
        (void)SendOne(
            udp,
            from,
            nc,
            static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_CONNECT_CHALLENGE),
            Zeus::Protocol::ENetChannel::Loading,
            Zeus::Protocol::ENetDelivery::ReliableOrdered,
            nc->GetConnectionId(),
            nowMonotonicSeconds,
            serverWallTimeMs,
            false,
            packetStats_,
            [&ch](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectChallengePayload(w, ch); });

        std::ostringstream slog;
        slog << "Created sessionId=" << session->GetSessionId() << " connectionId=" << nc->GetConnectionId() << " endpoint=" << from.ToString()
              << " (awaiting C_CONNECT_RESPONSE)";
        ZeusLog::Info("Session", slog.str());
        ZeusLog::Info("Net", "Sent S_CONNECT_CHALLENGE");
        return;
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_CONNECT_RESPONSE))
    {
        Zeus::Net::NetConnection* nc = connections.FindByConnectionId(parsed.header.connectionId);
        if (nc == nullptr || nc->GetEndpoint() != from)
        {
            return;
        }
        ClientSession* session = sessions.FindByConnectionId(nc->GetConnectionId());
        if (session == nullptr || session->GetState() != SessionState::Connecting)
        {
            return;
        }
        if (nc->IsRemoteDuplicate(parsed.header.sequence))
        {
            return;
        }

        std::vector<Zeus::Net::FOrderedInboundReleased> orderedBatch;
        const Zeus::Net::OrderedInboundResult oir = nc->SubmitInboundReliableOrdered(
            Zeus::Protocol::ENetChannel::Loading,
            parsed.header.flags,
            op,
            parsed.header.sequence,
            parsed.payload,
            parsed.payloadSize,
            orderedBatch);
        if (oir == Zeus::Net::OrderedInboundResult::DuplicateOld)
        {
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueueFull)
        {
            if (packetStats_ != nullptr)
            {
                ++packetStats_->OrderedInboundQueueFull;
            }
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueuedFuture)
        {
            nc->MarkReceived(parsed.header.sequence);
            nc->MarkReceive(nowMonotonicSeconds);
            return;
        }

        for (const Zeus::Net::FOrderedInboundReleased& w : orderedBatch)
        {
            Zeus::Protocol::ConnectResponsePayload resp{};
            if (!Zeus::Protocol::ReadConnectResponsePayload(w.Payload.data(), w.Payload.size(), resp).Ok())
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
                    [](Zeus::Protocol::PacketWriter& w2) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidPacket);
                        rj.reasonMessage = "Invalid connect response payload.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w2, rj);
                    });
                sessions.RemoveByConnectionId(nc->GetConnectionId());
                connections.RemoveConnection(nc->GetConnectionId());
                return;
            }
            if (resp.clientNonce != session->GetClientHandshakeNonce() || resp.serverNonce != session->GetServerChallengeNonce())
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
                    serverWallTimeMs,
                    true,
                    packetStats_,
                    [](Zeus::Protocol::PacketWriter& w2) -> ZeusResult {
                        Zeus::Protocol::ConnectRejectPayload rj{};
                        rj.reasonCode = static_cast<std::uint16_t>(Zeus::Protocol::ConnectRejectReason::InvalidHandshake);
                        rj.reasonMessage = "Invalid connect handshake nonces.";
                        return Zeus::Protocol::WriteConnectRejectPayload(w2, rj);
                    });
                sessions.RemoveByConnectionId(nc->GetConnectionId());
                connections.RemoveConnection(nc->GetConnectionId());
                return;
            }

            LogPacketIn(from, w.Opcode, w.RemoteSequence);
            nc->MarkReceived(w.RemoteSequence);
        }
        nc->MarkReceive(nowMonotonicSeconds);
        session->SetState(SessionState::Connected);
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);
        nc->ResetInboundReliableOrder(Zeus::Protocol::ENetChannel::Loading);

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
            serverWallTimeMs,
            false,
            packetStats_,
            [&ok](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteConnectOkPayload(w, ok); });
        ZeusLog::Info(
            "Session",
            std::string("Connected sessionId=") + std::to_string(session->GetSessionId()) + " connectionId=" + std::to_string(nc->GetConnectionId()));
        if (!travelMapName_.empty() || !travelMapPath_.empty())
        {
            Zeus::Protocol::TravelToMapPayload travel{};
            travel.mapName = travelMapName_;
            travel.mapPath = travelMapPath_;
            travel.serverTimeMs = serverWallTimeMs;
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_TRAVEL_TO_MAP),
                Zeus::Protocol::ENetChannel::Loading,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                ok.connectionId,
                nowMonotonicSeconds,
                serverWallTimeMs,
                false,
                packetStats_,
                [&travel](Zeus::Protocol::PacketWriter& w) -> ZeusResult { return Zeus::Protocol::WriteTravelToMapPayload(w, travel); });
            ZeusLog::Info(
                "Session",
                std::string("Sent S_TRAVEL_TO_MAP map=").append(travel.mapName).append(" path=").append(travel.mapPath));
        }
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
        if (networkDiagnostics_ != nullptr)
        {
            networkDiagnostics_->OnPongSample(pingBody.clientTimeMs, serverWallTimeMs, serverWallTimeMs);
        }

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
            serverWallTimeMs,
            false,
            packetStats_,
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
            serverWallTimeMs,
            true,
            packetStats_,
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

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_TEST_RELIABLE))
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
        LogPacketIn(from, op, parsed.header.sequence);
        nc->MarkReceived(parsed.header.sequence);
        nc->MarkReceive(nowMonotonicSeconds);
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);
        (void)SendOne(
            udp,
            from,
            nc,
            static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_TEST_RELIABLE),
            Zeus::Protocol::ENetChannel::Gameplay,
            Zeus::Protocol::ENetDelivery::Reliable,
            nc->GetConnectionId(),
            nowMonotonicSeconds,
            serverWallTimeMs,
            false,
            packetStats_,
            [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                (void)w;
                return ZeusResult::Success();
            });
        return;
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_TEST_ORDERED))
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

        std::vector<Zeus::Net::FOrderedInboundReleased> orderedBatch;
        const Zeus::Net::OrderedInboundResult oir = nc->SubmitInboundReliableOrdered(
            Zeus::Protocol::ENetChannel::Gameplay,
            parsed.header.flags,
            op,
            parsed.header.sequence,
            parsed.payload,
            parsed.payloadSize,
            orderedBatch);
        if (oir == Zeus::Net::OrderedInboundResult::DuplicateOld)
        {
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueueFull)
        {
            if (packetStats_ != nullptr)
            {
                ++packetStats_->OrderedInboundQueueFull;
            }
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueuedFuture)
        {
            nc->MarkReceived(parsed.header.sequence);
            nc->MarkReceive(nowMonotonicSeconds);
            return;
        }

        for (const Zeus::Net::FOrderedInboundReleased& w : orderedBatch)
        {
            LogPacketIn(from, w.Opcode, w.RemoteSequence);
            nc->MarkReceived(w.RemoteSequence);
        }
        nc->MarkReceive(nowMonotonicSeconds);
        session->SetLastPacketAtSeconds(nowMonotonicSeconds);
        for (const auto& ignoredOrdered : orderedBatch)
        {
            (void)ignoredOrdered;
            (void)SendOne(
                udp,
                from,
                nc,
                static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_TEST_ORDERED),
                Zeus::Protocol::ENetChannel::Gameplay,
                Zeus::Protocol::ENetDelivery::ReliableOrdered,
                nc->GetConnectionId(),
                nowMonotonicSeconds,
                serverWallTimeMs,
                false,
                packetStats_,
                [](Zeus::Protocol::PacketWriter& w) -> ZeusResult {
                    (void)w;
                    return ZeusResult::Success();
                });
        }
        return;
    }

    if (op == static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::C_LOADING_FRAGMENT))
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

        std::vector<Zeus::Net::FOrderedInboundReleased> orderedBatch;
        const Zeus::Net::OrderedInboundResult oir = nc->SubmitInboundReliableOrdered(
            Zeus::Protocol::ENetChannel::Loading,
            parsed.header.flags,
            op,
            parsed.header.sequence,
            parsed.payload,
            parsed.payloadSize,
            orderedBatch);
        if (oir == Zeus::Net::OrderedInboundResult::DuplicateOld)
        {
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueueFull)
        {
            if (packetStats_ != nullptr)
            {
                ++packetStats_->OrderedInboundQueueFull;
            }
            return;
        }
        if (oir == Zeus::Net::OrderedInboundResult::QueuedFuture)
        {
            nc->MarkReceived(parsed.header.sequence);
            nc->MarkReceive(nowMonotonicSeconds);
            return;
        }

        const double reassemblyTimeoutSec =
            settings_.reassemblyTimeoutMs > 0 ? static_cast<double>(settings_.reassemblyTimeoutMs) / 1000.0 : 60.0;
        for (const Zeus::Net::FOrderedInboundReleased& w : orderedBatch)
        {
            Zeus::Protocol::LoadingFragmentPayload frag{};
            if (!Zeus::Protocol::ReadLoadingFragmentPayload(w.Payload.data(), w.Payload.size(), frag).Ok())
            {
                return;
            }
            LogPacketIn(from, w.Opcode, w.RemoteSequence);
            nc->MarkReceived(w.RemoteSequence);
            nc->MarkReceive(nowMonotonicSeconds);
            session->SetLastPacketAtSeconds(nowMonotonicSeconds);
            const ClientSession::LoadingFragmentResult fr = session->FeedLoadingFragment(
                frag.snapshotId,
                frag.chunkIndex,
                frag.chunkCount,
                frag.data.data(),
                frag.data.size(),
                nowMonotonicSeconds,
                settings_.maxLoadingFragmentCount,
                settings_.maxReassemblyBytes,
                reassemblyTimeoutSec,
                packetStats_);
            if (fr == ClientSession::LoadingFragmentResult::Completed)
            {
                Zeus::Protocol::LoadingAssembledOkPayload ok{};
                ok.snapshotId = frag.snapshotId;
                (void)SendOne(
                    udp,
                    from,
                    nc,
                    static_cast<std::uint16_t>(Zeus::Protocol::EOpcode::S_LOADING_ASSEMBLED_OK),
                    Zeus::Protocol::ENetChannel::Loading,
                    Zeus::Protocol::ENetDelivery::ReliableOrdered,
                    nc->GetConnectionId(),
                    nowMonotonicSeconds,
                    serverWallTimeMs,
                    false,
                    packetStats_,
                    [&ok](Zeus::Protocol::PacketWriter& w2) -> ZeusResult {
                        return Zeus::Protocol::WriteLoadingAssembledOkPayload(w2, ok);
                    });
            }
        }
        return;
    }
}

void SessionPacketHandler::OnTickPostNetwork(
    Zeus::Net::UdpServer& udp,
    Zeus::Net::NetConnectionManager& connections,
    SessionManager& sessions,
    const double nowMonotonicSeconds,
    const std::uint64_t serverWallTimeMs)
{
    (void)nowMonotonicSeconds;
    std::vector<Zeus::Net::ConnectionId> reliableDead;
    connections.TickAllReliabilityResends(udp, serverWallTimeMs, packetStats_, reliableDead);
    for (const Zeus::Net::ConnectionId id : reliableDead)
    {
        ClientSession* sess = sessions.FindByConnectionId(id);
        const Zeus::Net::SessionId sid = sess != nullptr ? sess->GetSessionId() : 0;
        sessions.RemoveByConnectionId(id);
        connections.RemoveConnection(id);
        std::ostringstream oss;
        oss << "[Reliable] connection dropped after reliable give-up sessionId=" << sid << " connectionId=" << id;
        ZeusLog::Warning("Net", oss.str());
    }
    constexpr std::uint32_t kBudget = Zeus::Protocol::ZEUS_MAX_PACKET_BYTES * 64u;
    connections.FlushAllOutbound(udp, kBudget, serverWallTimeMs, nowMonotonicSeconds, packetStats_);
}

void SessionPacketHandler::OnTickTimeouts(
    Zeus::Net::NetConnectionManager& connections,
    SessionManager& sessions,
    const double nowMonotonicSeconds)
{
    const double timeoutSec =
        settings_.connectionTimeoutMs > 0 ? static_cast<double>(settings_.connectionTimeoutMs) / 1000.0 : 30.0;
    std::vector<Zeus::Net::ConnectionId> removed;
    connections.UpdateTimeouts(nowMonotonicSeconds, timeoutSec, removed);
    for (const Zeus::Net::ConnectionId id : removed)
    {
        ClientSession* sess = sessions.FindByConnectionId(id);
        const Zeus::Net::SessionId sid = sess != nullptr ? sess->GetSessionId() : 0;
        sessions.RemoveByConnectionId(id);
        std::ostringstream smsg;
        smsg << "[Session] Timeout sessionId=" << sid << " connectionId=" << id;
        ZeusLog::Info("Session", smsg.str());
        std::ostringstream nmsg;
        nmsg << "[Net] Connection removed connectionId=" << id << " reason=timeout";
        ZeusLog::Info("Net", nmsg.str());
    }
}
} // namespace Zeus::Session
