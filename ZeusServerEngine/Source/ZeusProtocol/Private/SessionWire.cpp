#include "SessionWire.hpp"

namespace Zeus::Protocol
{
namespace
{
template <typename Fn>
ZeusResult ReadPayloadReader(const std::uint8_t* payload, const std::size_t payloadSize, Fn&& readFn)
{
    if (payload == nullptr && payloadSize > 0)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "SessionWire: null payload.");
    }
    PacketReader reader(payload, payloadSize);
    return readFn(reader);
}
} // namespace

ZeusResult ReadConnectRequestPayload(const std::uint8_t* payload, const std::size_t payloadSize, ConnectRequestPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt16(out.clientProtocolVersion);
        if (!a.Ok())
        {
            return a;
        }
        ZeusResult b = r.ReadString(out.clientBuild);
        if (!b.Ok())
        {
            return b;
        }
        return r.ReadUInt64(out.clientNonce);
    });
}

ZeusResult WriteConnectRequestPayload(PacketWriter& w, const ConnectRequestPayload& in)
{
    ZeusResult a = w.WriteUInt16(in.clientProtocolVersion);
    if (!a.Ok())
    {
        return a;
    }
    ZeusResult b = w.WriteString(in.clientBuild);
    if (!b.Ok())
    {
        return b;
    }
    return w.WriteUInt64(in.clientNonce);
}

ZeusResult ReadConnectOkPayload(const std::uint8_t* payload, const std::size_t payloadSize, ConnectOkPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt32(out.connectionId);
        if (!a.Ok())
        {
            return a;
        }
        ZeusResult b = r.ReadUInt64(out.sessionId);
        if (!b.Ok())
        {
            return b;
        }
        ZeusResult c = r.ReadUInt64(out.serverTimeMs);
        if (!c.Ok())
        {
            return c;
        }
        return r.ReadUInt32(out.heartbeatIntervalMs);
    });
}

ZeusResult WriteConnectOkPayload(PacketWriter& w, const ConnectOkPayload& in)
{
    ZeusResult a = w.WriteUInt32(in.connectionId);
    if (!a.Ok())
    {
        return a;
    }
    ZeusResult b = w.WriteUInt64(in.sessionId);
    if (!b.Ok())
    {
        return b;
    }
    ZeusResult c = w.WriteUInt64(in.serverTimeMs);
    if (!c.Ok())
    {
        return c;
    }
    return w.WriteUInt32(in.heartbeatIntervalMs);
}

ZeusResult ReadConnectRejectPayload(const std::uint8_t* payload, const std::size_t payloadSize, ConnectRejectPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt16(out.reasonCode);
        if (!a.Ok())
        {
            return a;
        }
        return r.ReadString(out.reasonMessage);
    });
}

ZeusResult WriteConnectRejectPayload(PacketWriter& w, const ConnectRejectPayload& in)
{
    ZeusResult a = w.WriteUInt16(in.reasonCode);
    if (!a.Ok())
    {
        return a;
    }
    return w.WriteString(in.reasonMessage);
}

ZeusResult ReadPingPayload(const std::uint8_t* payload, const std::size_t payloadSize, PingPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult { return r.ReadUInt64(out.clientTimeMs); });
}

ZeusResult WritePingPayload(PacketWriter& w, const PingPayload& in)
{
    return w.WriteUInt64(in.clientTimeMs);
}

ZeusResult ReadPongPayload(const std::uint8_t* payload, const std::size_t payloadSize, PongPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt64(out.clientTimeMs);
        if (!a.Ok())
        {
            return a;
        }
        return r.ReadUInt64(out.serverTimeMs);
    });
}

ZeusResult WritePongPayload(PacketWriter& w, const PongPayload& in)
{
    ZeusResult a = w.WriteUInt64(in.clientTimeMs);
    if (!a.Ok())
    {
        return a;
    }
    return w.WriteUInt64(in.serverTimeMs);
}

ZeusResult ReadDisconnectPayload(const std::uint8_t* payload, const std::size_t payloadSize, DisconnectPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult { return r.ReadUInt16(out.reasonCode); });
}

ZeusResult WriteDisconnectPayload(PacketWriter& w, const DisconnectPayload& in)
{
    return w.WriteUInt16(in.reasonCode);
}

ZeusResult ReadDisconnectOkPayload(const std::uint8_t* payload, const std::size_t payloadSize, DisconnectOkPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult { return r.ReadUInt64(out.serverTimeMs); });
}

ZeusResult WriteDisconnectOkPayload(PacketWriter& w, const DisconnectOkPayload& in)
{
    return w.WriteUInt64(in.serverTimeMs);
}
} // namespace Zeus::Protocol
