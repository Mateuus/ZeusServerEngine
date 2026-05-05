#include "PacketBuilder.hpp"

#include "PacketHeaderWire.hpp"

namespace Zeus::Protocol
{
void PacketBuilder::Reset()
{
    writer_.Clear();
    writer_.SetWriteCursor(ZEUS_WIRE_HEADER_BYTES);
}

ZeusResult PacketBuilder::Finalize(PacketHeader header)
{
    if (writer_.Size() < ZEUS_WIRE_HEADER_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "PacketBuilder::Finalize: size < header.");
    }
    header.payloadSize = static_cast<std::uint16_t>(writer_.Size() - ZEUS_WIRE_HEADER_BYTES);
    if (header.payloadSize > ZEUS_MAX_PAYLOAD_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "PacketBuilder::Finalize: payload too large.");
    }
    header.headerSize = static_cast<std::uint16_t>(ZEUS_WIRE_HEADER_BYTES);

    std::uint8_t wire[ZEUS_WIRE_HEADER_BYTES];
    ZeusResult sh = SerializePacketHeader(header, wire);
    if (!sh.Ok())
    {
        return sh;
    }
    return writer_.OverwriteAt(0, wire, ZEUS_WIRE_HEADER_BYTES);
}
} // namespace Zeus::Protocol
