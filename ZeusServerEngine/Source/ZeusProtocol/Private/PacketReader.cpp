#include "PacketReader.hpp"

#include <cstring>

namespace Zeus::Protocol
{
namespace
{
std::uint16_t LoadU16LE(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t LoadU32LE(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint64_t LoadU64LE(const std::uint8_t* p)
{
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i)
    {
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return v;
}
} // namespace

PacketReader::PacketReader(const std::uint8_t* data, std::size_t length)
    : data_(data)
    , length_(length)
{
}

std::size_t PacketReader::Remaining() const
{
    return length_ > readCursor_ ? length_ - readCursor_ : 0;
}

ZeusResult PacketReader::ReadUInt8(std::uint8_t& out)
{
    if (readCursor_ + 1 > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader: unexpected end (u8).");
    }
    out = data_[readCursor_++];
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadUInt16(std::uint16_t& out)
{
    if (readCursor_ + 2 > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader: unexpected end (u16).");
    }
    out = LoadU16LE(data_ + readCursor_);
    readCursor_ += 2;
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadUInt32(std::uint32_t& out)
{
    if (readCursor_ + 4 > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader: unexpected end (u32).");
    }
    out = LoadU32LE(data_ + readCursor_);
    readCursor_ += 4;
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadUInt64(std::uint64_t& out)
{
    if (readCursor_ + 8 > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader: unexpected end (u64).");
    }
    out = LoadU64LE(data_ + readCursor_);
    readCursor_ += 8;
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadInt8(std::int8_t& out)
{
    std::uint8_t u = 0;
    const ZeusResult r = ReadUInt8(u);
    out = static_cast<std::int8_t>(u);
    return r;
}

ZeusResult PacketReader::ReadInt16(std::int16_t& out)
{
    std::uint16_t u = 0;
    const ZeusResult r = ReadUInt16(u);
    out = static_cast<std::int16_t>(u);
    return r;
}

ZeusResult PacketReader::ReadInt32(std::int32_t& out)
{
    std::uint32_t u = 0;
    const ZeusResult r = ReadUInt32(u);
    out = static_cast<std::int32_t>(u);
    return r;
}

ZeusResult PacketReader::ReadInt64(std::int64_t& out)
{
    std::uint64_t u = 0;
    const ZeusResult r = ReadUInt64(u);
    out = static_cast<std::int64_t>(u);
    return r;
}

ZeusResult PacketReader::ReadFloat(float& out)
{
    std::uint32_t bits = 0;
    const ZeusResult r = ReadUInt32(bits);
    if (!r.Ok())
    {
        return r;
    }
    std::memcpy(&out, &bits, sizeof(out));
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadDouble(double& out)
{
    std::uint64_t bits = 0;
    const ZeusResult r = ReadUInt64(bits);
    if (!r.Ok())
    {
        return r;
    }
    std::memcpy(&out, &bits, sizeof(out));
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadBytes(void* dst, std::size_t byteCount)
{
    if (readCursor_ + byteCount > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader: unexpected end (bytes).");
    }
    std::memcpy(dst, data_ + readCursor_, byteCount);
    readCursor_ += byteCount;
    return ZeusResult::Success();
}

ZeusResult PacketReader::ReadString(std::string& outUtf8)
{
    std::uint16_t len = 0;
    ZeusResult r = ReadUInt16(len);
    if (!r.Ok())
    {
        return r;
    }
    if (readCursor_ + len > length_)
    {
        return ZeusResult::Failure(ZeusErrorCode::ParseError, "PacketReader::ReadString: length past end.");
    }
    outUtf8.assign(reinterpret_cast<const char*>(data_ + readCursor_), len);
    readCursor_ += len;
    return ZeusResult::Success();
}
} // namespace Zeus::Protocol
