#pragma once

#include "PacketConstants.hpp"
#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Zeus::Protocol
{
/** Escrita binária little-endian para buffer interno (limite `ZEUS_MAX_PACKET_BYTES`). */
class PacketWriter
{
public:
    PacketWriter();

    [[nodiscard]] ZeusResult WriteUInt8(std::uint8_t v);
    [[nodiscard]] ZeusResult WriteUInt16(std::uint16_t v);
    [[nodiscard]] ZeusResult WriteUInt32(std::uint32_t v);
    [[nodiscard]] ZeusResult WriteUInt64(std::uint64_t v);
    [[nodiscard]] ZeusResult WriteInt8(std::int8_t v);
    [[nodiscard]] ZeusResult WriteInt16(std::int16_t v);
    [[nodiscard]] ZeusResult WriteInt32(std::int32_t v);
    [[nodiscard]] ZeusResult WriteInt64(std::int64_t v);
    [[nodiscard]] ZeusResult WriteFloat(float v);
    [[nodiscard]] ZeusResult WriteDouble(double v);
    [[nodiscard]] ZeusResult WriteBytes(const void* data, std::size_t byteCount);
    /** UTF-8 com prefixo `uint16` comprimento (máx. 65535 bytes de payload). */
    [[nodiscard]] ZeusResult WriteString(const std::string& utf8);

    [[nodiscard]] const std::uint8_t* Data() const { return buffer_.data(); }
    [[nodiscard]] std::size_t Size() const { return writeCursor_; }
    void Clear();

    /** Sobrescreve bytes já escritos (ex.: cabeçalho após payload). */
    ZeusResult OverwriteAt(std::size_t offset, const void* data, std::size_t byteCount);

    /** Reserva espaço no início (ex.: cabeçalho) sem avançar payload. */
    void SetWriteCursor(std::size_t pos);
    [[nodiscard]] std::size_t GetWriteCursor() const { return writeCursor_; }

private:
    [[nodiscard]] ZeusResult EnsureRoom(std::size_t extraBytes);

    std::vector<std::uint8_t> buffer_;
    std::size_t writeCursor_ = 0;
};
} // namespace Zeus::Protocol
