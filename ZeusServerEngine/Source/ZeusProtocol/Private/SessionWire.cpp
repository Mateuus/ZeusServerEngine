#include "SessionWire.hpp"

#include "PacketConstants.hpp"

#include <algorithm>

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

ZeusResult ReadConnectChallengePayload(const std::uint8_t* payload, const std::size_t payloadSize, ConnectChallengePayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt64(out.serverNonce);
        if (!a.Ok())
        {
            return a;
        }
        return r.ReadUInt32(out.connectionId);
    });
}

ZeusResult WriteConnectChallengePayload(PacketWriter& w, const ConnectChallengePayload& in)
{
    ZeusResult a = w.WriteUInt64(in.serverNonce);
    if (!a.Ok())
    {
        return a;
    }
    return w.WriteUInt32(in.connectionId);
}

ZeusResult ReadConnectResponsePayload(const std::uint8_t* payload, const std::size_t payloadSize, ConnectResponsePayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt64(out.clientNonce);
        if (!a.Ok())
        {
            return a;
        }
        return r.ReadUInt64(out.serverNonce);
    });
}

ZeusResult WriteConnectResponsePayload(PacketWriter& w, const ConnectResponsePayload& in)
{
    ZeusResult a = w.WriteUInt64(in.clientNonce);
    if (!a.Ok())
    {
        return a;
    }
    return w.WriteUInt64(in.serverNonce);
}

ZeusResult ReadLoadingFragmentPayload(const std::uint8_t* payload, const std::size_t payloadSize, LoadingFragmentPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadUInt64(out.snapshotId);
        if (!a.Ok())
        {
            return a;
        }
        ZeusResult b = r.ReadUInt16(out.chunkIndex);
        if (!b.Ok())
        {
            return b;
        }
        ZeusResult c = r.ReadUInt16(out.chunkCount);
        if (!c.Ok())
        {
            return c;
        }
        std::uint16_t dataLen = 0;
        ZeusResult d = r.ReadUInt16(dataLen);
        if (!d.Ok())
        {
            return d;
        }
        if (dataLen > ZEUS_MAX_PAYLOAD_BYTES)
        {
            return ZeusResult::Failure(ZeusErrorCode::ParseError, "LoadingFragment: dataLen too large.");
        }
        if (r.Remaining() < static_cast<std::size_t>(dataLen))
        {
            return ZeusResult::Failure(ZeusErrorCode::ParseError, "LoadingFragment: truncated data.");
        }
        out.data.resize(dataLen);
        if (dataLen > 0)
        {
            return r.ReadBytes(out.data.data(), static_cast<std::size_t>(dataLen));
        }
        return ZeusResult::Success();
    });
}

ZeusResult WriteLoadingFragmentPayload(PacketWriter& w, const LoadingFragmentPayload& in)
{
    if (in.data.size() > ZEUS_MAX_PAYLOAD_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "LoadingFragment: data too large.");
    }
    ZeusResult a = w.WriteUInt64(in.snapshotId);
    if (!a.Ok())
    {
        return a;
    }
    ZeusResult b = w.WriteUInt16(in.chunkIndex);
    if (!b.Ok())
    {
        return b;
    }
    ZeusResult c = w.WriteUInt16(in.chunkCount);
    if (!c.Ok())
    {
        return c;
    }
    const auto len = static_cast<std::uint16_t>(std::min<std::size_t>(in.data.size(), 65535u));
    ZeusResult d = w.WriteUInt16(len);
    if (!d.Ok())
    {
        return d;
    }
    if (len > 0)
    {
        return w.WriteBytes(in.data.data(), static_cast<std::size_t>(len));
    }
    return ZeusResult::Success();
}

ZeusResult ReadLoadingAssembledOkPayload(const std::uint8_t* payload, const std::size_t payloadSize, LoadingAssembledOkPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult { return r.ReadUInt64(out.snapshotId); });
}

ZeusResult WriteLoadingAssembledOkPayload(PacketWriter& w, const LoadingAssembledOkPayload& in)
{
    return w.WriteUInt64(in.snapshotId);
}

ZeusResult ReadTravelToMapPayload(const std::uint8_t* payload, const std::size_t payloadSize, TravelToMapPayload& out)
{
    return ReadPayloadReader(payload, payloadSize, [&out](PacketReader& r) -> ZeusResult {
        ZeusResult a = r.ReadString(out.mapName);
        if (!a.Ok())
        {
            return a;
        }
        ZeusResult b = r.ReadString(out.mapPath);
        if (!b.Ok())
        {
            return b;
        }
        return r.ReadUInt64(out.serverTimeMs);
    });
}

ZeusResult WriteTravelToMapPayload(PacketWriter& w, const TravelToMapPayload& in)
{
    ZeusResult a = w.WriteString(in.mapName);
    if (!a.Ok())
    {
        return a;
    }
    ZeusResult b = w.WriteString(in.mapPath);
    if (!b.Ok())
    {
        return b;
    }
    return w.WriteUInt64(in.serverTimeMs);
}
} // namespace Zeus::Protocol
