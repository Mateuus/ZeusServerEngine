// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionTerrainWriter.h"

#include "ZeusCollisionExportTypes.h"
#include "ZeusMapTools.h"

#include "Misc/FileHelper.h"

namespace
{
	constexpr uint16 ZSMT_VERSION = 1;
	// 4 magic + 2 ver + 2 hsize + 4 flags + 4 piece + 4 rsz + 16 reserved = 36
	constexpr uint16 ZSMT_HEADER_SIZE = 36;

	void WriteU8(TArray<uint8>& Buf, uint8 V) { Buf.Add(V); }

	void WriteU16(TArray<uint8>& Buf, uint16 V)
	{
		Buf.Add(static_cast<uint8>(V & 0xFF));
		Buf.Add(static_cast<uint8>((V >> 8) & 0xFF));
	}

	void WriteI32(TArray<uint8>& Buf, int32 V)
	{
		const uint32 U = static_cast<uint32>(V);
		Buf.Add(static_cast<uint8>(U & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 8) & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 16) & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 24) & 0xFF));
	}

	void WriteU32(TArray<uint8>& Buf, uint32 V)
	{
		Buf.Add(static_cast<uint8>(V & 0xFF));
		Buf.Add(static_cast<uint8>((V >> 8) & 0xFF));
		Buf.Add(static_cast<uint8>((V >> 16) & 0xFF));
		Buf.Add(static_cast<uint8>((V >> 24) & 0xFF));
	}

	void WriteF32(TArray<uint8>& Buf, float V)
	{
		uint32 U = 0;
		FMemory::Memcpy(&U, &V, sizeof(U));
		Buf.Add(static_cast<uint8>(U & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 8) & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 16) & 0xFF));
		Buf.Add(static_cast<uint8>((U >> 24) & 0xFF));
	}

	void WriteF64(TArray<uint8>& Buf, double V)
	{
		uint64 U = 0;
		FMemory::Memcpy(&U, &V, sizeof(U));
		for (int32 i = 0; i < 8; ++i)
		{
			Buf.Add(static_cast<uint8>((U >> (i * 8)) & 0xFF));
		}
	}

	void WriteString(TArray<uint8>& Buf, const FString& S)
	{
		const FTCHARToUTF8 Conv(*S);
		const int32 Len = Conv.Length();
		const uint16 ClampedLen = static_cast<uint16>(FMath::Clamp(Len, 0, 0xFFFF));
		WriteU16(Buf, ClampedLen);
		const uint8* Bytes = reinterpret_cast<const uint8*>(Conv.Get());
		for (int32 i = 0; i < ClampedLen; ++i)
		{
			Buf.Add(Bytes[i]);
		}
	}

	void WriteVec3(TArray<uint8>& Buf, const FVector& V)
	{
		WriteF64(Buf, V.X);
		WriteF64(Buf, V.Y);
		WriteF64(Buf, V.Z);
	}

	void WriteQuat(TArray<uint8>& Buf, const FQuat& Q)
	{
		WriteF64(Buf, Q.X);
		WriteF64(Buf, Q.Y);
		WriteF64(Buf, Q.Z);
		WriteF64(Buf, Q.W);
	}

	void WriteTransform(TArray<uint8>& Buf, const FTransform& T)
	{
		WriteVec3(Buf, T.GetLocation());
		WriteQuat(Buf, T.GetRotation());
		WriteVec3(Buf, T.GetScale3D());
	}
}

bool FZeusCollisionTerrainWriter::Write(const FString& OutputPath,
	const FZeusExportResult& Result,
	const FZeusExportOptions& Options)
{
	TArray<uint8> Buf;
	Buf.Reserve(64 * 1024);

	// Magic 'Z','S','M','T'
	WriteU8(Buf, 'Z');
	WriteU8(Buf, 'S');
	WriteU8(Buf, 'M');
	WriteU8(Buf, 'T');
	WriteU16(Buf, ZSMT_VERSION);
	WriteU16(Buf, ZSMT_HEADER_SIZE);

	const uint32 Flags = 0x1u; // HasRegions
	WriteU32(Buf, Flags);

	WriteU32(Buf, static_cast<uint32>(Result.TerrainPieces.Num()));
	WriteU32(Buf, static_cast<uint32>(FMath::Max(0.0, Options.RegionSizeCm)));

	// reserved[4]
	WriteU32(Buf, 0u);
	WriteU32(Buf, 0u);
	WriteU32(Buf, 0u);
	WriteU32(Buf, 0u);

	WriteString(Buf, Result.MapName);

	for (const FZeusTerrainPieceExport& Piece : Result.TerrainPieces)
	{
		WriteString(Buf, Piece.PieceName);
		WriteString(Buf, Piece.ActorName);
		WriteString(Buf, Piece.ComponentName);

		WriteU8(Buf, static_cast<uint8>(Piece.Kind));
		WriteTransform(Buf, Piece.WorldTransform);
		WriteVec3(Buf, Piece.BoundsCenter);
		WriteVec3(Buf, Piece.BoundsExtent);

		WriteU32(Buf, Piece.Region.RegionId);
		WriteI32(Buf, Piece.Region.GridX);
		WriteI32(Buf, Piece.Region.GridY);
		WriteI32(Buf, Piece.Region.GridZ);

		switch (Piece.Kind)
		{
		case EZeusTerrainPieceKind::TriangleMesh:
		{
			const uint32 N = static_cast<uint32>(Piece.TriangleMesh.Vertices.Num());
			WriteU32(Buf, N);
			for (uint32 i = 0; i < N; ++i)
			{
				WriteVec3(Buf, Piece.TriangleMesh.Vertices[i]);
			}
			const uint32 NIdx = static_cast<uint32>(Piece.TriangleMesh.Indices.Num());
			WriteU32(Buf, NIdx);
			for (uint32 i = 0; i < NIdx; ++i)
			{
				WriteU32(Buf, Piece.TriangleMesh.Indices[i]);
			}
			break;
		}
		case EZeusTerrainPieceKind::HeightField:
		{
			WriteU32(Buf, Piece.HeightField.SamplesX);
			WriteU32(Buf, Piece.HeightField.SamplesY);
			WriteF64(Buf, Piece.HeightField.SampleSpacingCm);
			WriteVec3(Buf, Piece.HeightField.OriginLocal);
			WriteF64(Buf, Piece.HeightField.HeightScaleCm);
			const int32 Total = static_cast<int32>(Piece.HeightField.SamplesX)
				* static_cast<int32>(Piece.HeightField.SamplesY);
			const int32 NumHeights = Piece.HeightField.Heights.Num();
			for (int32 i = 0; i < Total; ++i)
			{
				const float H = (i < NumHeights) ? Piece.HeightField.Heights[i] : 0.0f;
				WriteF32(Buf, H);
			}
			break;
		}
		default:
			UE_LOG(LogZeusMapTools, Warning, TEXT("[ZeusMapTools] Unknown terrain piece kind %u"),
				static_cast<uint32>(Piece.Kind));
			break;
		}
	}

	if (!FFileHelper::SaveArrayToFile(Buf, *OutputPath))
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to save ZSMT to %s"), *OutputPath);
		return false;
	}
	UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] ZSMT size=%d bytes (pieces=%d)"),
		Buf.Num(), Result.TerrainPieces.Num());
	return true;
}
