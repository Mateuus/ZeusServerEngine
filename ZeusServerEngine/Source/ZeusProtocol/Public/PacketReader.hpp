#pragma once

#include "ZeusResult.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Zeus::Protocol
{
/** Leitura binária little-endian a partir de um buffer só de leitura. */
class PacketReader
{
public:
    PacketReader(const std::uint8_t* data, std::size_t length);

    [[nodiscard]] ZeusResult ReadUInt8(std::uint8_t& out);
    [[nodiscard]] ZeusResult ReadUInt16(std::uint16_t& out);
    [[nodiscard]] ZeusResult ReadUInt32(std::uint32_t& out);
    [[nodiscard]] ZeusResult ReadUInt64(std::uint64_t& out);
    [[nodiscard]] ZeusResult ReadInt8(std::int8_t& out);
    [[nodiscard]] ZeusResult ReadInt16(std::int16_t& out);
    [[nodiscard]] ZeusResult ReadInt32(std::int32_t& out);
    [[nodiscard]] ZeusResult ReadInt64(std::int64_t& out);
    [[nodiscard]] ZeusResult ReadFloat(float& out);
    [[nodiscard]] ZeusResult ReadDouble(double& out);
    [[nodiscard]] ZeusResult ReadBytes(void* dst, std::size_t byteCount);
    [[nodiscard]] ZeusResult ReadString(std::string& outUtf8);

    [[nodiscard]] std::size_t Remaining() const;
    [[nodiscard]] std::size_t Tell() const { return readCursor_; }

private:
    const std::uint8_t* data_;
    std::size_t length_;
    std::size_t readCursor_ = 0;
};
} // namespace Zeus::Protocol
