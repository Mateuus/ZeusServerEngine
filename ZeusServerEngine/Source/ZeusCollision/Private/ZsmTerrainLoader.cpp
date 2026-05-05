#include "ZsmTerrainLoader.hpp"

#include "ZsmBinReader.hpp"
#include "ZsmTerrainFormat.hpp"

#include <fstream>
#include <sstream>

namespace Zeus::Collision
{
namespace
{
ETerrainPieceKind PieceKindFromByte(std::uint8_t b)
{
    switch (b)
    {
    case ZsmTerrain::PieceKind::TriangleMesh: return ETerrainPieceKind::TriangleMesh;
    case ZsmTerrain::PieceKind::HeightField:  return ETerrainPieceKind::HeightField;
    default: return ETerrainPieceKind::Unknown;
    }
}

bool ReadPiece(ZsmBinReader& Reader, TerrainPiece& Piece, ZsmLoadResult& Result, TerrainAssetStats& Stats)
{
    if (!Reader.ReadStringU16(Piece.Name) ||
        !Reader.ReadStringU16(Piece.ActorName) ||
        !Reader.ReadStringU16(Piece.ComponentName))
    {
        return false;
    }

    std::uint8_t kindByte = 0;
    if (!Reader.ReadU8(kindByte)) return false;
    Piece.Kind = PieceKindFromByte(kindByte);

    if (!Reader.ReadTransform(Piece.WorldTransform)) return false;
    if (!Reader.ReadVec3(Piece.BoundsCenter))        return false;
    if (!Reader.ReadVec3(Piece.BoundsExtent))        return false;

    if (!Reader.ReadU32(Piece.Region.RegionId)) return false;
    if (!Reader.ReadI32(Piece.Region.GridX))    return false;
    if (!Reader.ReadI32(Piece.Region.GridY))    return false;
    if (!Reader.ReadI32(Piece.Region.GridZ))    return false;

    switch (Piece.Kind)
    {
    case ETerrainPieceKind::TriangleMesh:
    {
        std::uint32_t vertCount = 0;
        if (!Reader.ReadU32(vertCount)) return false;
        Piece.TriangleMesh.Vertices.resize(vertCount);
        for (std::uint32_t v = 0; v < vertCount; ++v)
        {
            if (!Reader.ReadVec3(Piece.TriangleMesh.Vertices[v])) return false;
        }
        std::uint32_t idxCount = 0;
        if (!Reader.ReadU32(idxCount)) return false;
        Piece.TriangleMesh.Indices.resize(idxCount);
        for (std::uint32_t i = 0; i < idxCount; ++i)
        {
            if (!Reader.ReadU32(Piece.TriangleMesh.Indices[i])) return false;
        }
        Stats.TriangleMeshCount++;
        Stats.TotalVertexCount += vertCount;
        Stats.TotalTriangleCount += idxCount / 3;
        break;
    }
    case ETerrainPieceKind::HeightField:
    {
        if (!Reader.ReadU32(Piece.HeightField.SamplesX)) return false;
        if (!Reader.ReadU32(Piece.HeightField.SamplesY)) return false;
        if (!Reader.ReadF64(Piece.HeightField.SampleSpacingCm)) return false;
        if (!Reader.ReadVec3(Piece.HeightField.OriginLocal))    return false;
        if (!Reader.ReadF64(Piece.HeightField.HeightScaleCm))   return false;
        const std::size_t total = static_cast<std::size_t>(Piece.HeightField.SamplesX) *
                                  static_cast<std::size_t>(Piece.HeightField.SamplesY);
        Piece.HeightField.Heights.resize(total);
        for (std::size_t i = 0; i < total; ++i)
        {
            if (!Reader.ReadF32(Piece.HeightField.Heights[i])) return false;
        }
        Stats.HeightFieldCount++;
        Stats.TotalHeightSampleCount += total;
        break;
    }
    default:
    {
        std::ostringstream warn;
        warn << "Skipping unknown terrain piece kind=" << static_cast<int>(kindByte)
             << " in piece '" << Piece.Name << "'";
        Result.Warnings.push_back(warn.str());
        ++Stats.WarningCount;
        return false;
    }
    }
    Stats.PieceCount++;
    return true;
}
} // namespace

ZsmLoadResult ZsmTerrainLoader::LoadFromFile(const std::filesystem::path& path, TerrainCollisionAsset& outAsset)
{
    ZsmLoadResult Result;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        Result.Error = "Could not open terrain file: " + path.string();
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
            Result.Error = "Read error on terrain file: " + path.string();
            return Result;
        }
    }
    return LoadFromBytes(Buffer.data(), Buffer.size(), outAsset);
}

ZsmLoadResult ZsmTerrainLoader::LoadFromBytes(const std::uint8_t* data, std::size_t size, TerrainCollisionAsset& outAsset)
{
    ZsmLoadResult Result;

    if (data == nullptr || size < ZsmTerrain::FixedHeaderSize)
    {
        Result.Error = "Buffer too small for ZSMT header";
        return Result;
    }

    ZsmBinReader Reader(data, size);

    std::uint8_t magic[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        if (!Reader.ReadU8(magic[i]))
        {
            Result.Error = "Truncated terrain magic";
            return Result;
        }
    }
    if (magic[0] != ZsmTerrain::Magic0 || magic[1] != ZsmTerrain::Magic1 ||
        magic[2] != ZsmTerrain::Magic2 || magic[3] != ZsmTerrain::Magic3)
    {
        Result.Error = "Invalid magic (expected ZSMT)";
        return Result;
    }

    std::uint16_t version = 0;
    std::uint16_t headerSize = 0;
    if (!Reader.ReadU16(version) || !Reader.ReadU16(headerSize))
    {
        Result.Error = "Truncated ZSMT header (version/headerSize)";
        return Result;
    }
    if (version != ZsmTerrain::SupportedVersion)
    {
        std::ostringstream err;
        err << "Unsupported ZSMT version " << version
            << " (expected " << ZsmTerrain::SupportedVersion << ")";
        Result.Error = err.str();
        return Result;
    }
    outAsset.Version = version;

    std::uint32_t flags = 0;
    std::uint32_t pieceCount = 0;
    std::uint32_t regionSize = 0;
    std::uint32_t reserved[4] = {0, 0, 0, 0};
    if (!Reader.ReadU32(flags) ||
        !Reader.ReadU32(pieceCount) ||
        !Reader.ReadU32(regionSize) ||
        !Reader.ReadU32(reserved[0]) ||
        !Reader.ReadU32(reserved[1]) ||
        !Reader.ReadU32(reserved[2]) ||
        !Reader.ReadU32(reserved[3]))
    {
        Result.Error = "Truncated ZSMT header (counters)";
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
        Result.Error = "Truncated ZSMT mapName";
        return Result;
    }

    outAsset.Pieces.clear();
    outAsset.Pieces.reserve(pieceCount);
    outAsset.Stats = {};

    for (std::uint32_t i = 0; i < pieceCount; ++i)
    {
        TerrainPiece Piece;
        if (!ReadPiece(Reader, Piece, Result, outAsset.Stats))
        {
            std::ostringstream err;
            err << "Failed to read terrain piece index=" << i
                << " (offset=" << Reader.Position() << ")";
            Result.Error = err.str();
            return Result;
        }
        Piece.Region.RegionSizeCm = outAsset.RegionSizeCm;
        outAsset.Pieces.push_back(std::move(Piece));
    }

    outAsset.Stats.WarningCount += Result.Warnings.size();

    RebuildTerrainPieceIndexByRegion(outAsset);

    Result.bSuccess = true;
    return Result;
}

void RebuildTerrainPieceIndexByRegion(TerrainCollisionAsset& asset)
{
    asset.PieceIndexByRegion.clear();
    asset.PieceIndexByRegion.reserve(asset.Pieces.size());
    for (std::size_t i = 0; i < asset.Pieces.size(); ++i)
    {
        const std::uint32_t regionId = asset.Pieces[i].Region.RegionId;
        asset.PieceIndexByRegion[regionId].push_back(i);
    }
}
} // namespace Zeus::Collision
