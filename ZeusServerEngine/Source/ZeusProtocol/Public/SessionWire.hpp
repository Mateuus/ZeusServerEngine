#pragma once

#include "ConnectRejectReason.hpp"
#include "PacketReader.hpp"
#include "PacketWriter.hpp"
#include "ZeusResult.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Zeus::Protocol
{
struct ConnectRequestPayload
{
    std::uint16_t clientProtocolVersion = 0;
    std::string clientBuild;
    std::uint64_t clientNonce = 0;
};

struct ConnectOkPayload
{
    std::uint32_t connectionId = 0;
    std::uint64_t sessionId = 0;
    std::uint64_t serverTimeMs = 0;
    std::uint32_t heartbeatIntervalMs = 0;
};

struct ConnectRejectPayload
{
    std::uint16_t reasonCode = 0;
    std::string reasonMessage;
};

struct PingPayload
{
    std::uint64_t clientTimeMs = 0;
};

struct PongPayload
{
    std::uint64_t clientTimeMs = 0;
    std::uint64_t serverTimeMs = 0;
};

struct DisconnectPayload
{
    std::uint16_t reasonCode = 0;
};

struct DisconnectOkPayload
{
    std::uint64_t serverTimeMs = 0;
};

struct ConnectChallengePayload
{
    std::uint64_t serverNonce = 0;
    std::uint32_t connectionId = 0;
};

struct ConnectResponsePayload
{
    std::uint64_t clientNonce = 0;
    std::uint64_t serverNonce = 0;
};

struct LoadingFragmentPayload
{
    std::uint64_t snapshotId = 0;
    std::uint16_t chunkIndex = 0;
    std::uint16_t chunkCount = 0;
    std::vector<std::uint8_t> data;
};

struct LoadingAssembledOkPayload
{
    std::uint64_t snapshotId = 0;
};

ZeusResult ReadConnectRequestPayload(const std::uint8_t* payload, std::size_t payloadSize, ConnectRequestPayload& out);
ZeusResult WriteConnectRequestPayload(PacketWriter& w, const ConnectRequestPayload& in);

ZeusResult ReadConnectOkPayload(const std::uint8_t* payload, std::size_t payloadSize, ConnectOkPayload& out);
ZeusResult WriteConnectOkPayload(PacketWriter& w, const ConnectOkPayload& in);

ZeusResult ReadConnectRejectPayload(const std::uint8_t* payload, std::size_t payloadSize, ConnectRejectPayload& out);
ZeusResult WriteConnectRejectPayload(PacketWriter& w, const ConnectRejectPayload& in);

ZeusResult ReadPingPayload(const std::uint8_t* payload, std::size_t payloadSize, PingPayload& out);
ZeusResult WritePingPayload(PacketWriter& w, const PingPayload& in);

ZeusResult ReadPongPayload(const std::uint8_t* payload, std::size_t payloadSize, PongPayload& out);
ZeusResult WritePongPayload(PacketWriter& w, const PongPayload& in);

ZeusResult ReadDisconnectPayload(const std::uint8_t* payload, std::size_t payloadSize, DisconnectPayload& out);
ZeusResult WriteDisconnectPayload(PacketWriter& w, const DisconnectPayload& in);

ZeusResult ReadDisconnectOkPayload(const std::uint8_t* payload, std::size_t payloadSize, DisconnectOkPayload& out);
ZeusResult WriteDisconnectOkPayload(PacketWriter& w, const DisconnectOkPayload& in);

ZeusResult ReadConnectChallengePayload(const std::uint8_t* payload, std::size_t payloadSize, ConnectChallengePayload& out);
ZeusResult WriteConnectChallengePayload(PacketWriter& w, const ConnectChallengePayload& in);

ZeusResult ReadConnectResponsePayload(const std::uint8_t* payload, std::size_t payloadSize, ConnectResponsePayload& out);
ZeusResult WriteConnectResponsePayload(PacketWriter& w, const ConnectResponsePayload& in);

ZeusResult ReadLoadingFragmentPayload(const std::uint8_t* payload, std::size_t payloadSize, LoadingFragmentPayload& out);
ZeusResult WriteLoadingFragmentPayload(PacketWriter& w, const LoadingFragmentPayload& in);

ZeusResult ReadLoadingAssembledOkPayload(const std::uint8_t* payload, std::size_t payloadSize, LoadingAssembledOkPayload& out);
ZeusResult WriteLoadingAssembledOkPayload(PacketWriter& w, const LoadingAssembledOkPayload& in);
} // namespace Zeus::Protocol
