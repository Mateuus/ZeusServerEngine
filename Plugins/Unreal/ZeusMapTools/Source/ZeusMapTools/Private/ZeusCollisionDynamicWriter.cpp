// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionDynamicWriter.h"

#include "ZeusCollisionExportTypes.h"
#include "ZeusMapTools.h"

#include "Misc/FileHelper.h"

namespace
{
	constexpr uint16 ZSMD_VERSION = 1;
	constexpr uint16 ZSMD_HEADER_SIZE = 32; // 4 magic + 2 ver + 2 hsize + 4 flags + 4 vol + 4 shp + 4 rsz + 12 reserved

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

	void WriteShape(TArray<uint8>& Buf, const FZeusShapeExport& Shape)
	{
		WriteU8(Buf, static_cast<uint8>(Shape.Type));
		WriteTransform(Buf, Shape.LocalTransform);
		switch (Shape.Type)
		{
		case EZeusCollisionShapeType::Box:
			WriteVec3(Buf, Shape.Box.HalfExtents);
			break;
		case EZeusCollisionShapeType::Sphere:
			WriteF64(Buf, Shape.Sphere.Radius);
			break;
		case EZeusCollisionShapeType::Capsule:
			WriteF64(Buf, Shape.Capsule.Radius);
			WriteF64(Buf, Shape.Capsule.HalfHeight);
			break;
		case EZeusCollisionShapeType::Convex:
		{
			const uint32 N = static_cast<uint32>(Shape.Convex.Vertices.Num());
			WriteU32(Buf, N);
			for (uint32 i = 0; i < N; ++i)
			{
				WriteVec3(Buf, Shape.Convex.Vertices[i]);
			}
			break;
		}
		default:
			UE_LOG(LogZeusMapTools, Warning, TEXT("[ZeusMapTools] Unknown dynamic shape type %u"),
				static_cast<uint32>(Shape.Type));
			break;
		}
	}
}

bool FZeusCollisionDynamicWriter::Write(const FString& OutputPath,
	const FZeusExportResult& Result,
	const FZeusExportOptions& Options)
{
	TArray<uint8> Buf;
	Buf.Reserve(16 * 1024);

	// Magic 'Z','S','M','D'
	WriteU8(Buf, 'Z');
	WriteU8(Buf, 'S');
	WriteU8(Buf, 'M');
	WriteU8(Buf, 'D');
	WriteU16(Buf, ZSMD_VERSION);
	WriteU16(Buf, ZSMD_HEADER_SIZE);

	const uint32 Flags = 0x1u; // HasRegions
	WriteU32(Buf, Flags);

	// volumeCount
	WriteU32(Buf, static_cast<uint32>(Result.Volumes.Num()));

	// shapeCount
	int32 TotalShapes = 0;
	for (const FZeusVolumeExport& V : Result.Volumes)
	{
		TotalShapes += V.Shapes.Num();
	}
	WriteU32(Buf, static_cast<uint32>(TotalShapes));

	WriteU32(Buf, static_cast<uint32>(FMath::Max(0.0, Options.RegionSizeCm)));

	// reserved[3]
	WriteU32(Buf, 0u);
	WriteU32(Buf, 0u);
	WriteU32(Buf, 0u);

	WriteString(Buf, Result.MapName);

	for (const FZeusVolumeExport& Vol : Result.Volumes)
	{
		WriteString(Buf, Vol.VolumeName);
		WriteString(Buf, Vol.ActorName);

		WriteU8(Buf, static_cast<uint8>(Vol.Kind));
		WriteString(Buf, Vol.EventTag);

		WriteTransform(Buf, Vol.WorldTransform);
		WriteVec3(Buf, Vol.BoundsCenter);
		WriteVec3(Buf, Vol.BoundsExtent);

		WriteU32(Buf, Vol.Region.RegionId);
		WriteI32(Buf, Vol.Region.GridX);
		WriteI32(Buf, Vol.Region.GridY);
		WriteI32(Buf, Vol.Region.GridZ);

		WriteU32(Buf, static_cast<uint32>(Vol.Shapes.Num()));
		for (const FZeusShapeExport& Shape : Vol.Shapes)
		{
			WriteShape(Buf, Shape);
		}
	}

	if (!FFileHelper::SaveArrayToFile(Buf, *OutputPath))
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to save ZSMD to %s"), *OutputPath);
		return false;
	}
	UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] ZSMD size=%d bytes (volumes=%d, shapes=%d)"),
		Buf.Num(), Result.Volumes.Num(), TotalShapes);
	return true;
}
