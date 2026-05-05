#pragma once

#include "Quaternion.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"

#include <cstdint>
#include <cstring>
#include <string>

namespace Zeus::Collision
{
/**
 * Cursor binario little-endian sobre um buffer in-memory. Reporta `bOverflow`
 * em caso de falta de bytes. Partilhado entre ZSMC, ZSMD e ZSMT loaders.
 */
class ZsmBinReader
{
public:
    ZsmBinReader(const std::uint8_t* data, std::size_t size)
        : Data(data)
        , Size(size)
    {
    }

    bool HasOverflow() const { return bOverflow; }
    std::size_t Position() const { return Pos; }
    std::size_t Remaining() const { return Size - Pos; }

    bool ReadU8(std::uint8_t& out)
    {
        if (!Ensure(1)) return false;
        out = Data[Pos++];
        return true;
    }

    bool ReadU16(std::uint16_t& out)
    {
        if (!Ensure(2)) return false;
        out = static_cast<std::uint16_t>(Data[Pos]) |
              static_cast<std::uint16_t>(static_cast<std::uint16_t>(Data[Pos + 1]) << 8);
        Pos += 2;
        return true;
    }

    bool ReadU32(std::uint32_t& out)
    {
        if (!Ensure(4)) return false;
        out = static_cast<std::uint32_t>(Data[Pos]) |
              (static_cast<std::uint32_t>(Data[Pos + 1]) << 8) |
              (static_cast<std::uint32_t>(Data[Pos + 2]) << 16) |
              (static_cast<std::uint32_t>(Data[Pos + 3]) << 24);
        Pos += 4;
        return true;
    }

    bool ReadI32(std::int32_t& out)
    {
        std::uint32_t u = 0;
        if (!ReadU32(u)) return false;
        std::memcpy(&out, &u, sizeof(out));
        return true;
    }

    bool ReadF32(float& out)
    {
        if (!Ensure(4)) return false;
        std::uint32_t u = 0;
        for (int i = 0; i < 4; ++i)
        {
            u |= static_cast<std::uint32_t>(Data[Pos + i]) << (i * 8);
        }
        std::memcpy(&out, &u, sizeof(out));
        Pos += 4;
        return true;
    }

    bool ReadF64(double& out)
    {
        if (!Ensure(8)) return false;
        std::uint64_t u = 0;
        for (int i = 0; i < 8; ++i)
        {
            u |= static_cast<std::uint64_t>(Data[Pos + i]) << (i * 8);
        }
        std::memcpy(&out, &u, sizeof(out));
        Pos += 8;
        return true;
    }

    bool ReadStringU16(std::string& out)
    {
        std::uint16_t len = 0;
        if (!ReadU16(len)) return false;
        if (!Ensure(len)) return false;
        out.assign(reinterpret_cast<const char*>(&Data[Pos]), len);
        Pos += len;
        return true;
    }

    bool ReadVec3(Math::Vector3& out)
    {
        return ReadF64(out.X) && ReadF64(out.Y) && ReadF64(out.Z);
    }

    bool ReadQuat(Math::Quaternion& out)
    {
        return ReadF64(out.X) && ReadF64(out.Y) && ReadF64(out.Z) && ReadF64(out.W);
    }

    bool ReadTransform(Math::Transform& out)
    {
        return ReadVec3(out.Location) && ReadQuat(out.Rotation) && ReadVec3(out.Scale3D);
    }

private:
    bool Ensure(std::size_t bytes)
    {
        if (Pos + bytes > Size)
        {
            bOverflow = true;
            return false;
        }
        return true;
    }

    const std::uint8_t* Data;
    std::size_t Size;
    std::size_t Pos = 0;
    bool bOverflow = false;
};
} // namespace Zeus::Collision
