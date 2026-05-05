#include "PacketHeaderWire.hpp"

namespace Zeus::Protocol
{
namespace
{
void U16(std::uint8_t* d, std::uint16_t v)
{
    d[0] = static_cast<std::uint8_t>(v & 0xFF);
    d[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void U32(std::uint8_t* d, std::uint32_t v)
{
    d[0] = static_cast<std::uint8_t>(v & 0xFF);
    d[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    d[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    d[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

std::uint16_t R16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t R32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
} // namespace

ZeusResult SerializePacketHeader(const PacketHeader& h, std::uint8_t out[ZEUS_WIRE_HEADER_BYTES])
{
    U32(out + 0, h.magic);
    U16(out + 4, h.version);
    U16(out + 6, h.headerSize);
    U16(out + 8, h.opcode);
    out[10] = h.channel;
    out[11] = h.delivery;
    U32(out + 12, h.sequence);
    U32(out + 16, h.ack);
    U32(out + 20, h.ackBits);
    U32(out + 24, h.connectionId);
    U16(out + 28, h.payloadSize);
    U16(out + 30, h.flags);
    return ZeusResult::Success();
}

ZeusResult DeserializePacketHeader(const std::uint8_t* data, std::size_t length, PacketHeader& out)
{
    if (length < ZEUS_WIRE_HEADER_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "DeserializePacketHeader: buffer too small.");
    }
    out.magic = R32(data + 0);
    out.version = R16(data + 4);
    out.headerSize = R16(data + 6);
    out.opcode = R16(data + 8);
    out.channel = data[10];
    out.delivery = data[11];
    out.sequence = R32(data + 12);
    out.ack = R32(data + 16);
    out.ackBits = R32(data + 20);
    out.connectionId = R32(data + 24);
    out.payloadSize = R16(data + 28);
    out.flags = R16(data + 30);
    return ZeusResult::Success();
}
} // namespace Zeus::Protocol
