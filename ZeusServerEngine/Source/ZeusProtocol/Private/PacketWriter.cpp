#include "PacketWriter.hpp"

#include <cstring>

namespace Zeus::Protocol
{
namespace
{
void StoreU16LE(std::uint8_t* dst, std::uint16_t v)
{
    dst[0] = static_cast<std::uint8_t>(v & 0xFF);
    dst[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void StoreU32LE(std::uint8_t* dst, std::uint32_t v)
{
    dst[0] = static_cast<std::uint8_t>(v & 0xFF);
    dst[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    dst[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
    dst[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

void StoreU64LE(std::uint8_t* dst, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
    {
        dst[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}
} // namespace

PacketWriter::PacketWriter()
{
    buffer_.resize(ZEUS_MAX_PACKET_BYTES);
    writeCursor_ = 0;
}

void PacketWriter::Clear()
{
    writeCursor_ = 0;
}

ZeusResult PacketWriter::OverwriteAt(std::size_t offset, const void* data, std::size_t byteCount)
{
    if (offset + byteCount > writeCursor_)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "PacketWriter::OverwriteAt: range past written size.");
    }
    std::memcpy(&buffer_[offset], data, byteCount);
    return ZeusResult::Success();
}

void PacketWriter::SetWriteCursor(std::size_t pos)
{
    if (pos <= buffer_.size())
    {
        writeCursor_ = pos;
    }
}

ZeusResult PacketWriter::EnsureRoom(std::size_t extraBytes)
{
    if (writeCursor_ + extraBytes > ZEUS_MAX_PACKET_BYTES)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "PacketWriter: exceeded ZEUS_MAX_PACKET_BYTES.");
    }
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteUInt8(std::uint8_t v)
{
    const ZeusResult ok = EnsureRoom(1);
    if (!ok.Ok())
    {
        return ok;
    }
    buffer_[writeCursor_++] = v;
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteUInt16(std::uint16_t v)
{
    const ZeusResult ok = EnsureRoom(2);
    if (!ok.Ok())
    {
        return ok;
    }
    StoreU16LE(&buffer_[writeCursor_], v);
    writeCursor_ += 2;
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteUInt32(std::uint32_t v)
{
    const ZeusResult ok = EnsureRoom(4);
    if (!ok.Ok())
    {
        return ok;
    }
    StoreU32LE(&buffer_[writeCursor_], v);
    writeCursor_ += 4;
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteUInt64(std::uint64_t v)
{
    const ZeusResult ok = EnsureRoom(8);
    if (!ok.Ok())
    {
        return ok;
    }
    StoreU64LE(&buffer_[writeCursor_], v);
    writeCursor_ += 8;
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteInt8(std::int8_t v)
{
    return WriteUInt8(static_cast<std::uint8_t>(v));
}

ZeusResult PacketWriter::WriteInt16(std::int16_t v)
{
    return WriteUInt16(static_cast<std::uint16_t>(v));
}

ZeusResult PacketWriter::WriteInt32(std::int32_t v)
{
    return WriteUInt32(static_cast<std::uint32_t>(v));
}

ZeusResult PacketWriter::WriteInt64(std::int64_t v)
{
    return WriteUInt64(static_cast<std::uint64_t>(v));
}

ZeusResult PacketWriter::WriteFloat(float v)
{
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return WriteUInt32(bits);
}

ZeusResult PacketWriter::WriteDouble(double v)
{
    static_assert(sizeof(double) == 8, "double must be 8 bytes");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return WriteUInt64(bits);
}

ZeusResult PacketWriter::WriteBytes(const void* data, std::size_t byteCount)
{
    const ZeusResult ok = EnsureRoom(byteCount);
    if (!ok.Ok())
    {
        return ok;
    }
    std::memcpy(&buffer_[writeCursor_], data, byteCount);
    writeCursor_ += byteCount;
    return ZeusResult::Success();
}

ZeusResult PacketWriter::WriteString(const std::string& utf8)
{
    if (utf8.size() > 65535U)
    {
        return ZeusResult::Failure(ZeusErrorCode::InvalidArgument, "PacketWriter::WriteString: string too long.");
    }
    const auto len = static_cast<std::uint16_t>(utf8.size());
    ZeusResult r = WriteUInt16(len);
    if (!r.Ok())
    {
        return r;
    }
    return WriteBytes(utf8.data(), utf8.size());
}
} // namespace Zeus::Protocol
