#include "ZsmDynamicLoader.hpp"

#include "ZsmBinReader.hpp"
#include "ZsmDynamicFormat.hpp"

#include <fstream>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
EDynamicVolumeKind VolumeKindFromByte(std::uint8_t b)
{
    switch (b)
    {
    case ZsmDynamic::VolumeKind::Trigger:    return EDynamicVolumeKind::Trigger;
    case ZsmDynamic::VolumeKind::Water:      return EDynamicVolumeKind::Water;
    case ZsmDynamic::VolumeKind::Lava:       return EDynamicVolumeKind::Lava;
    case ZsmDynamic::VolumeKind::KillVolume: return EDynamicVolumeKind::KillVolume;
    case ZsmDynamic::VolumeKind::SafeZone:   return EDynamicVolumeKind::SafeZone;
    default: return EDynamicVolumeKind::Unknown;
    }
}

bool ReadShape(ZsmBinReader& Reader, CollisionShape& Shape, ZsmLoadResult& Result, DynamicAssetStats& Stats)
{
    std::uint8_t typeByte = 0;
    if (!Reader.ReadU8(typeByte)) return false;
    Shape.Type = ShapeTypeFromByte(typeByte);

    if (!Reader.ReadTransform(Shape.LocalTransform)) return false;

    switch (Shape.Type)
    {
    case ECollisionShapeType::Box:
        if (!Reader.ReadVec3(Shape.Box.HalfExtents)) return false;
        break;
    case ECollisionShapeType::Sphere:
        if (!Reader.ReadF64(Shape.Sphere.Radius)) return false;
        break;
    case ECollisionShapeType::Capsule:
        if (!Reader.ReadF64(Shape.Capsule.Radius)) return false;
        if (!Reader.ReadF64(Shape.Capsule.HalfHeight)) return false;
        break;
    case ECollisionShapeType::Convex:
    {
        std::uint32_t vertCount = 0;
        if (!Reader.ReadU32(vertCount)) return false;
        Shape.Convex.Vertices.resize(vertCount);
        for (std::uint32_t v = 0; v < vertCount; ++v)
        {
            if (!Reader.ReadVec3(Shape.Convex.Vertices[v])) return false;
        }
        break;
    }
    default:
    {
        std::ostringstream warn;
        warn << "Skipping unknown shape type byte=" << static_cast<int>(typeByte)
             << " in dynamic volume";
        Result.Warnings.push_back(warn.str());
        ++Stats.WarningCount;
        return false;
    }
    }

    ++Stats.ShapeCount;
    return true;
}

bool ReadVolume(ZsmBinReader& Reader, DynamicVolume& Vol, ZsmLoadResult& Result, DynamicAssetStats& Stats)
{
    if (!Reader.ReadStringU16(Vol.Name) ||
        !Reader.ReadStringU16(Vol.ActorName))
    {
        return false;
    }

    std::uint8_t kindByte = 0;
    if (!Reader.ReadU8(kindByte)) return false;
    Vol.Kind = VolumeKindFromByte(kindByte);

    if (!Reader.ReadStringU16(Vol.EventTag)) return false;

    if (!Reader.ReadTransform(Vol.WorldTransform)) return false;
    if (!Reader.ReadVec3(Vol.BoundsCenter))        return false;
    if (!Reader.ReadVec3(Vol.BoundsExtent))        return false;

    if (!Reader.ReadU32(Vol.Region.RegionId)) return false;
    if (!Reader.ReadI32(Vol.Region.GridX))    return false;
    if (!Reader.ReadI32(Vol.Region.GridY))    return false;
    if (!Reader.ReadI32(Vol.Region.GridZ))    return false;

    std::uint32_t shapeCount = 0;
    if (!Reader.ReadU32(shapeCount)) return false;

    Vol.Shapes.reserve(shapeCount);
    for (std::uint32_t i = 0; i < shapeCount; ++i)
    {
        CollisionShape Shape;
        if (!ReadShape(Reader, Shape, Result, Stats)) return false;
        Vol.Shapes.push_back(std::move(Shape));
    }

    switch (Vol.Kind)
    {
    case EDynamicVolumeKind::Trigger:    ++Stats.TriggerCount; break;
    case EDynamicVolumeKind::Water:      ++Stats.WaterCount; break;
    case EDynamicVolumeKind::Lava:       ++Stats.LavaCount; break;
    case EDynamicVolumeKind::KillVolume: ++Stats.KillVolumeCount; break;
    case EDynamicVolumeKind::SafeZone:   ++Stats.SafeZoneCount; break;
    default:                             ++Stats.UnknownKindCount; break;
    }
    ++Stats.VolumeCount;
    return true;
}
} // namespace

ZsmLoadResult ZsmDynamicLoader::LoadFromFile(const std::filesystem::path& path, DynamicCollisionAsset& outAsset)
{
    ZsmLoadResult Result;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        Result.Error = "Could not open dynamic file: " + path.string();
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
            Result.Error = "Read error on dynamic file: " + path.string();
            return Result;
        }
    }
    return LoadFromBytes(Buffer.data(), Buffer.size(), outAsset);
}

ZsmLoadResult ZsmDynamicLoader::LoadFromBytes(const std::uint8_t* data, std::size_t size, DynamicCollisionAsset& outAsset)
{
    ZsmLoadResult Result;

    if (data == nullptr || size < ZsmDynamic::FixedHeaderSize)
    {
        Result.Error = "Buffer too small for ZSMD header";
        return Result;
    }

    ZsmBinReader Reader(data, size);

    std::uint8_t magic[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        if (!Reader.ReadU8(magic[i]))
        {
            Result.Error = "Truncated dynamic magic";
            return Result;
        }
    }
    if (magic[0] != ZsmDynamic::Magic0 || magic[1] != ZsmDynamic::Magic1 ||
        magic[2] != ZsmDynamic::Magic2 || magic[3] != ZsmDynamic::Magic3)
    {
        Result.Error = "Invalid magic (expected ZSMD)";
        return Result;
    }

    std::uint16_t version = 0;
    std::uint16_t headerSize = 0;
    if (!Reader.ReadU16(version) || !Reader.ReadU16(headerSize))
    {
        Result.Error = "Truncated ZSMD header (version/headerSize)";
        return Result;
    }
    if (version != ZsmDynamic::SupportedVersion)
    {
        std::ostringstream err;
        err << "Unsupported ZSMD version " << version
            << " (expected " << ZsmDynamic::SupportedVersion << ")";
        Result.Error = err.str();
        return Result;
    }
    outAsset.Version = version;

    std::uint32_t flags = 0;
    std::uint32_t volumeCount = 0;
    std::uint32_t shapeCountHeader = 0;
    std::uint32_t regionSize = 0;
    std::uint32_t reserved[3] = {0, 0, 0};
    if (!Reader.ReadU32(flags) ||
        !Reader.ReadU32(volumeCount) ||
        !Reader.ReadU32(shapeCountHeader) ||
        !Reader.ReadU32(regionSize) ||
        !Reader.ReadU32(reserved[0]) ||
        !Reader.ReadU32(reserved[1]) ||
        !Reader.ReadU32(reserved[2]))
    {
        Result.Error = "Truncated ZSMD header (counters)";
        return Result;
    }
    outAsset.Flags = flags;
    outAsset.RegionSizeCm = static_cast<double>(regionSize);
    if (outAsset.RegionSizeCm <= 0.0)
    {
        outAsset.RegionSizeCm = 5000.0;
    }

    if (!Reader.ReadStringU16(outAsset.MapName))
    {
        Result.Error = "Truncated ZSMD mapName";
        return Result;
    }

    outAsset.Volumes.clear();
    outAsset.Volumes.reserve(volumeCount);
    outAsset.Stats = {};

    for (std::uint32_t i = 0; i < volumeCount; ++i)
    {
        DynamicVolume Vol;
        if (!ReadVolume(Reader, Vol, Result, outAsset.Stats))
        {
            std::ostringstream err;
            err << "Failed to read dynamic volume index=" << i
                << " (offset=" << Reader.Position() << ")";
            Result.Error = err.str();
            return Result;
        }
        Vol.Region.RegionSizeCm = outAsset.RegionSizeCm;
        outAsset.Volumes.push_back(std::move(Vol));
    }

    if (outAsset.Stats.ShapeCount != shapeCountHeader)
    {
        outAsset.Warnings.push_back("Header shapeCount differs from parsed shapes");
    }
    outAsset.Stats.WarningCount += Result.Warnings.size();

    RebuildDynamicVolumeIndexByRegion(outAsset);

    Result.bSuccess = true;
    return Result;
}

void RebuildDynamicVolumeIndexByRegion(DynamicCollisionAsset& asset)
{
    asset.VolumeIndexByRegion.clear();
    asset.VolumeIndexByRegion.reserve(asset.Volumes.size());
    for (std::size_t i = 0; i < asset.Volumes.size(); ++i)
    {
        const std::uint32_t regionId = asset.Volumes[i].Region.RegionId;
        asset.VolumeIndexByRegion[regionId].push_back(i);
    }
}
} // namespace Zeus::Collision
