#include "ZsmCollisionLoader.hpp"

#include "Quaternion.hpp"
#include "Transform.hpp"
#include "Vector3.hpp"
#include "ZsmCollisionFormat.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
/** Cursor sobre um buffer in-memory; reporta `bOverflow` em caso de falta de bytes. */
class BinReader
{
public:
    BinReader(const std::uint8_t* data, std::size_t size)
        : Data(data)
        , Size(size)
    {
    }

    bool HasOverflow() const { return bOverflow; }
    std::size_t Position() const { return Pos; }
    std::size_t Remaining() const { return Size - Pos; }

    bool ReadU8(std::uint8_t& out)
    {
        if (!Ensure(1))
            return false;
        out = Data[Pos++];
        return true;
    }

    bool ReadU16(std::uint16_t& out)
    {
        if (!Ensure(2))
            return false;
        out = static_cast<std::uint16_t>(Data[Pos]) |
              static_cast<std::uint16_t>(static_cast<std::uint16_t>(Data[Pos + 1]) << 8);
        Pos += 2;
        return true;
    }

    bool ReadU32(std::uint32_t& out)
    {
        if (!Ensure(4))
            return false;
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
        if (!ReadU32(u))
            return false;
        std::memcpy(&out, &u, sizeof(out));
        return true;
    }

    bool ReadF64(double& out)
    {
        if (!Ensure(8))
            return false;
        std::uint64_t u = 0;
        for (int i = 0; i < 8; ++i)
        {
            u |= static_cast<std::uint64_t>(Data[Pos + i]) << (i * 8);
        }
        std::memcpy(&out, &u, sizeof(out));
        Pos += 8;
        return true;
    }

    bool ReadString(std::string& out)
    {
        std::uint16_t len = 0;
        if (!ReadU16(len))
            return false;
        if (!Ensure(len))
            return false;
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

bool LoadEntity(BinReader& Reader, CollisionEntity& Entity, ZsmLoadResult& Result, CollisionAssetStats& Stats)
{
    if (!Reader.ReadString(Entity.Name) ||
        !Reader.ReadString(Entity.ActorName) ||
        !Reader.ReadString(Entity.ComponentName))
    {
        return false;
    }

    if (!Reader.ReadTransform(Entity.WorldTransform))
        return false;
    if (!Reader.ReadVec3(Entity.BoundsCenter))
        return false;
    if (!Reader.ReadVec3(Entity.BoundsExtent))
        return false;

    if (!Reader.ReadU32(Entity.Region.RegionId))
        return false;
    if (!Reader.ReadI32(Entity.Region.GridX))
        return false;
    if (!Reader.ReadI32(Entity.Region.GridY))
        return false;
    if (!Reader.ReadI32(Entity.Region.GridZ))
        return false;

    std::uint32_t shapeCount = 0;
    if (!Reader.ReadU32(shapeCount))
        return false;

    Entity.Shapes.reserve(shapeCount);
    for (std::uint32_t i = 0; i < shapeCount; ++i)
    {
        CollisionShape Shape;
        std::uint8_t typeByte = 0;
        if (!Reader.ReadU8(typeByte))
            return false;
        Shape.Type = ShapeTypeFromByte(typeByte);

        if (!Reader.ReadTransform(Shape.LocalTransform))
            return false;

        switch (Shape.Type)
        {
        case ECollisionShapeType::Box:
            if (!Reader.ReadVec3(Shape.Box.HalfExtents))
                return false;
            ++Stats.BoxCount;
            break;
        case ECollisionShapeType::Sphere:
            if (!Reader.ReadF64(Shape.Sphere.Radius))
                return false;
            ++Stats.SphereCount;
            break;
        case ECollisionShapeType::Capsule:
            if (!Reader.ReadF64(Shape.Capsule.Radius))
                return false;
            if (!Reader.ReadF64(Shape.Capsule.HalfHeight))
                return false;
            ++Stats.CapsuleCount;
            break;
        case ECollisionShapeType::Convex:
        {
            std::uint32_t vertCount = 0;
            if (!Reader.ReadU32(vertCount))
                return false;
            Shape.Convex.Vertices.resize(vertCount);
            for (std::uint32_t v = 0; v < vertCount; ++v)
            {
                if (!Reader.ReadVec3(Shape.Convex.Vertices[v]))
                    return false;
            }
            ++Stats.ConvexCount;
            break;
        }
        default:
        {
            std::ostringstream warn;
            warn << "Skipping unknown shape type byte=" << static_cast<int>(typeByte)
                 << " in entity '" << Entity.Name << "'";
            Result.Warnings.push_back(warn.str());
            ++Stats.WarningCount;
            return false;
        }
        }

        Entity.Shapes.push_back(std::move(Shape));
        ++Stats.ShapeCount;
    }
    ++Stats.EntityCount;
    return true;
}
} // namespace

ZsmLoadResult ZsmCollisionLoader::LoadFromFile(const std::filesystem::path& path, CollisionAsset& outAsset)
{
    ZsmLoadResult Result;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        Result.Error = "Could not open file: " + path.string();
        return Result;
    }
    file.seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> Buffer(size);
    if (size > 0)
    {
        file.read(reinterpret_cast<char*>(Buffer.data()), static_cast<std::streamsize>(size));
        if (!file)
        {
            Result.Error = "Read error on file: " + path.string();
            return Result;
        }
    }
    return LoadFromBytes(Buffer.data(), Buffer.size(), outAsset);
}

ZsmLoadResult ZsmCollisionLoader::LoadFromBytes(const std::uint8_t* data, std::size_t size, CollisionAsset& outAsset)
{
    ZsmLoadResult Result;

    if (data == nullptr || size < Zsm::FixedHeaderSize)
    {
        Result.Error = "Buffer too small for ZSMC header";
        return Result;
    }

    BinReader Reader(data, size);

    std::uint8_t magic[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        if (!Reader.ReadU8(magic[i]))
        {
            Result.Error = "Truncated magic";
            return Result;
        }
    }
    if (magic[0] != Zsm::Magic0 || magic[1] != Zsm::Magic1 ||
        magic[2] != Zsm::Magic2 || magic[3] != Zsm::Magic3)
    {
        Result.Error = "Invalid magic (expected ZSMC)";
        return Result;
    }

    std::uint16_t version = 0;
    std::uint16_t headerSize = 0;
    if (!Reader.ReadU16(version) || !Reader.ReadU16(headerSize))
    {
        Result.Error = "Truncated header (version/headerSize)";
        return Result;
    }
    if (version != Zsm::SupportedVersion)
    {
        std::ostringstream err;
        err << "Unsupported version " << version << " (expected " << Zsm::SupportedVersion << ")";
        Result.Error = err.str();
        return Result;
    }
    outAsset.Version = version;

    std::uint32_t flags = 0;
    std::uint32_t entityCount = 0;
    std::uint32_t shapeCount = 0;
    std::uint32_t regionSize = 0;
    std::uint32_t boxCount = 0;
    std::uint32_t sphereCount = 0;
    std::uint32_t capsuleCount = 0;
    std::uint32_t convexCount = 0;
    if (!Reader.ReadU32(flags) ||
        !Reader.ReadU32(entityCount) ||
        !Reader.ReadU32(shapeCount) ||
        !Reader.ReadU32(regionSize) ||
        !Reader.ReadU32(boxCount) ||
        !Reader.ReadU32(sphereCount) ||
        !Reader.ReadU32(capsuleCount) ||
        !Reader.ReadU32(convexCount))
    {
        Result.Error = "Truncated header (counters)";
        return Result;
    }
    outAsset.Flags = flags;
    outAsset.RegionSizeCm = static_cast<double>(regionSize);
    if (outAsset.RegionSizeCm <= 0.0)
    {
        outAsset.RegionSizeCm = 5000.0;
    }

    if (!Reader.ReadString(outAsset.MapName))
    {
        Result.Error = "Truncated mapName";
        return Result;
    }

    outAsset.Entities.clear();
    outAsset.Entities.reserve(entityCount);
    outAsset.Stats = {};

    for (std::uint32_t i = 0; i < entityCount; ++i)
    {
        CollisionEntity Entity;
        if (!LoadEntity(Reader, Entity, Result, outAsset.Stats))
        {
            std::ostringstream err;
            err << "Failed to read entity index=" << i << " (offset=" << Reader.Position() << ")";
            Result.Error = err.str();
            return Result;
        }
        Entity.Region.RegionSizeCm = outAsset.RegionSizeCm;
        outAsset.Entities.push_back(std::move(Entity));
    }

    outAsset.Stats.WarningCount += Result.Warnings.size();

    if (outAsset.Stats.EntityCount != static_cast<std::size_t>(entityCount))
    {
        Result.Warnings.push_back("Header entityCount differs from parsed entities");
    }
    if (outAsset.Stats.ShapeCount != static_cast<std::size_t>(shapeCount))
    {
        Result.Warnings.push_back("Header shapeCount differs from parsed shapes");
    }

    Result.bSuccess = true;
    return Result;
}
} // namespace Zeus::Collision
