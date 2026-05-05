#include "PacketParser.hpp"

#include "Opcodes.hpp"
#include "PacketHeaderWire.hpp"

namespace Zeus::Protocol
{
bool PacketParser::IsKnownOpcode(const std::uint16_t opcode)
{
    switch (opcode)
    {
    case static_cast<std::uint16_t>(EOpcode::C_CONNECT_REQUEST):
    case static_cast<std::uint16_t>(EOpcode::S_CONNECT_OK):
    case static_cast<std::uint16_t>(EOpcode::S_CONNECT_REJECT):
    case static_cast<std::uint16_t>(EOpcode::S_CONNECT_CHALLENGE):
    case static_cast<std::uint16_t>(EOpcode::C_CONNECT_RESPONSE):
    case static_cast<std::uint16_t>(EOpcode::C_PING):
    case static_cast<std::uint16_t>(EOpcode::S_PONG):
    case static_cast<std::uint16_t>(EOpcode::C_DISCONNECT):
    case static_cast<std::uint16_t>(EOpcode::S_DISCONNECT_OK):
    case static_cast<std::uint16_t>(EOpcode::C_LOADING_FRAGMENT):
    case static_cast<std::uint16_t>(EOpcode::S_LOADING_ASSEMBLED_OK):
    case static_cast<std::uint16_t>(EOpcode::C_TEST_RELIABLE):
    case static_cast<std::uint16_t>(EOpcode::S_TEST_RELIABLE):
    case static_cast<std::uint16_t>(EOpcode::C_TEST_ORDERED):
    case static_cast<std::uint16_t>(EOpcode::S_TEST_ORDERED):
        return true;
    default:
        return false;
    }
}

ZeusResult PacketParser::Parse(const std::uint8_t* data, const std::size_t length, const bool strictOpcodes, Output& out)
{
    if (length < ZEUS_WIRE_HEADER_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: datagram smaller than header.");
    }

    ZeusResult dh = DeserializePacketHeader(data, length, out.header);
    if (!dh.Ok())
    {
        return dh;
    }

    if (out.header.magic != ZEUS_PACKET_MAGIC)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: invalid magic.");
    }
    if (out.header.version != ZEUS_PROTOCOL_VERSION)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: unsupported protocol version.");
    }
    if (out.header.headerSize != ZEUS_WIRE_HEADER_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: unexpected headerSize.");
    }
    if (static_cast<std::size_t>(out.header.headerSize) + static_cast<std::size_t>(out.header.payloadSize) > length)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: payload extends past buffer.");
    }
    if (out.header.payloadSize > ZEUS_MAX_PAYLOAD_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: payloadSize too large.");
    }

    if (strictOpcodes && !IsKnownOpcode(out.header.opcode))
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketParser: unknown opcode.");
    }

    out.payload = data + out.header.headerSize;
    out.payloadSize = out.header.payloadSize;
    return ZeusResult::Success();
}
} // namespace Zeus::Protocol
