// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusCollisionJsonWriter.h"

#include "ZeusCollisionExportTypes.h"
#include "ZeusMapTools.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedRef<FJsonValueArray> Vector3ToJson(const FVector& V)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShared<FJsonValueNumber>(V.X));
		Arr.Add(MakeShared<FJsonValueNumber>(V.Y));
		Arr.Add(MakeShared<FJsonValueNumber>(V.Z));
		return MakeShared<FJsonValueArray>(Arr);
	}

	TSharedRef<FJsonValueArray> QuatToJson(const FQuat& Q)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShared<FJsonValueNumber>(Q.X));
		Arr.Add(MakeShared<FJsonValueNumber>(Q.Y));
		Arr.Add(MakeShared<FJsonValueNumber>(Q.Z));
		Arr.Add(MakeShared<FJsonValueNumber>(Q.W));
		return MakeShared<FJsonValueArray>(Arr);
	}

	TSharedRef<FJsonObject> TransformToJson(const FTransform& T)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetField(TEXT("location"), Vector3ToJson(T.GetLocation()));
		Obj->SetField(TEXT("rotation"), QuatToJson(T.GetRotation()));
		Obj->SetField(TEXT("scale"),    Vector3ToJson(T.GetScale3D()));
		return Obj;
	}

	TSharedRef<FJsonObject> ShapeToJson(const FZeusShapeExport& Shape)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		switch (Shape.Type)
		{
		case EZeusCollisionShapeType::Box:
			Obj->SetStringField(TEXT("type"), TEXT("box"));
			Obj->SetStringField(TEXT("extentSemantics"), TEXT("halfExtent"));
			Obj->SetField(TEXT("halfExtents"), Vector3ToJson(Shape.Box.HalfExtents));
			break;
		case EZeusCollisionShapeType::Sphere:
			Obj->SetStringField(TEXT("type"), TEXT("sphere"));
			Obj->SetNumberField(TEXT("radius"), Shape.Sphere.Radius);
			break;
		case EZeusCollisionShapeType::Capsule:
			Obj->SetStringField(TEXT("type"), TEXT("capsule"));
			Obj->SetNumberField(TEXT("radius"), Shape.Capsule.Radius);
			Obj->SetNumberField(TEXT("halfHeight"), Shape.Capsule.HalfHeight);
			break;
		case EZeusCollisionShapeType::Convex:
		{
			Obj->SetStringField(TEXT("type"), TEXT("convex"));
			TArray<TSharedPtr<FJsonValue>> VertArr;
			VertArr.Reserve(Shape.Convex.Vertices.Num());
			for (const FVector& V : Shape.Convex.Vertices)
			{
				VertArr.Add(Vector3ToJson(V));
			}
			Obj->SetArrayField(TEXT("vertices"), VertArr);
			break;
		}
		default:
			Obj->SetStringField(TEXT("type"), TEXT("unknown"));
			break;
		}

		Obj->SetField(TEXT("localTransform"), MakeShared<FJsonValueObject>(TransformToJson(Shape.LocalTransform)));

		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : Shape.Warnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Obj->SetArrayField(TEXT("warnings"), WarnArr);
		return Obj;
	}

	const TCHAR* VolumeKindToString(EZeusVolumeKind K)
	{
		switch (K)
		{
		case EZeusVolumeKind::Trigger:    return TEXT("trigger");
		case EZeusVolumeKind::Water:      return TEXT("water");
		case EZeusVolumeKind::Lava:       return TEXT("lava");
		case EZeusVolumeKind::KillVolume: return TEXT("killVolume");
		case EZeusVolumeKind::SafeZone:   return TEXT("safeZone");
		default:                          return TEXT("unknown");
		}
	}

	const TCHAR* TerrainKindToString(EZeusTerrainPieceKind K)
	{
		switch (K)
		{
		case EZeusTerrainPieceKind::TriangleMesh: return TEXT("triangleMesh");
		case EZeusTerrainPieceKind::HeightField:  return TEXT("heightField");
		default:                                  return TEXT("unknown");
		}
	}

	TSharedRef<FJsonObject> RegionToJson(const FZeusEntityRegion& R)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("regionId"), R.RegionId);
		Obj->SetNumberField(TEXT("gridX"),    R.GridX);
		Obj->SetNumberField(TEXT("gridY"),    R.GridY);
		Obj->SetNumberField(TEXT("gridZ"),    R.GridZ);
		Obj->SetNumberField(TEXT("regionSizeCm"), R.RegionSizeCm);
		return Obj;
	}

	TSharedRef<FJsonObject> VolumeToJson(const FZeusVolumeExport& Vol)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("volumeName"), Vol.VolumeName);
		Obj->SetStringField(TEXT("actorName"),  Vol.ActorName);
		Obj->SetStringField(TEXT("kind"),       VolumeKindToString(Vol.Kind));
		Obj->SetStringField(TEXT("eventTag"),   Vol.EventTag);

		Obj->SetObjectField(TEXT("region"), RegionToJson(Vol.Region));

		TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
		Bounds->SetField(TEXT("center"), Vector3ToJson(Vol.BoundsCenter));
		Bounds->SetField(TEXT("extent"), Vector3ToJson(Vol.BoundsExtent));
		Obj->SetObjectField(TEXT("bounds"), Bounds);

		Obj->SetField(TEXT("worldTransform"),
			MakeShared<FJsonValueObject>(TransformToJson(Vol.WorldTransform)));

		TArray<TSharedPtr<FJsonValue>> ShapeArr;
		ShapeArr.Reserve(Vol.Shapes.Num());
		for (const FZeusShapeExport& Shape : Vol.Shapes)
		{
			ShapeArr.Add(MakeShared<FJsonValueObject>(ShapeToJson(Shape)));
		}
		Obj->SetArrayField(TEXT("shapes"), ShapeArr);
		return Obj;
	}

	TSharedRef<FJsonObject> TerrainPieceToJson(const FZeusTerrainPieceExport& Piece)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("pieceName"),     Piece.PieceName);
		Obj->SetStringField(TEXT("actorName"),     Piece.ActorName);
		Obj->SetStringField(TEXT("componentName"), Piece.ComponentName);
		Obj->SetStringField(TEXT("kind"),          TerrainKindToString(Piece.Kind));

		Obj->SetObjectField(TEXT("region"), RegionToJson(Piece.Region));

		TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
		Bounds->SetField(TEXT("center"), Vector3ToJson(Piece.BoundsCenter));
		Bounds->SetField(TEXT("extent"), Vector3ToJson(Piece.BoundsExtent));
		Obj->SetObjectField(TEXT("bounds"), Bounds);

		Obj->SetField(TEXT("worldTransform"),
			MakeShared<FJsonValueObject>(TransformToJson(Piece.WorldTransform)));

		switch (Piece.Kind)
		{
		case EZeusTerrainPieceKind::TriangleMesh:
		{
			TSharedRef<FJsonObject> Mesh = MakeShared<FJsonObject>();
			Mesh->SetNumberField(TEXT("vertexCount"), Piece.TriangleMesh.Vertices.Num());
			Mesh->SetNumberField(TEXT("indexCount"),  Piece.TriangleMesh.Indices.Num());
			Mesh->SetNumberField(TEXT("triangleCount"), Piece.TriangleMesh.Indices.Num() / 3);
			Obj->SetObjectField(TEXT("triangleMesh"), Mesh);
			break;
		}
		case EZeusTerrainPieceKind::HeightField:
		{
			TSharedRef<FJsonObject> HF = MakeShared<FJsonObject>();
			HF->SetNumberField(TEXT("samplesX"),       Piece.HeightField.SamplesX);
			HF->SetNumberField(TEXT("samplesY"),       Piece.HeightField.SamplesY);
			HF->SetNumberField(TEXT("sampleSpacingCm"), Piece.HeightField.SampleSpacingCm);
			HF->SetField(TEXT("originLocal"),          Vector3ToJson(Piece.HeightField.OriginLocal));
			HF->SetNumberField(TEXT("heightScaleCm"),  Piece.HeightField.HeightScaleCm);
			HF->SetNumberField(TEXT("sampleCount"),    Piece.HeightField.Heights.Num());
			Obj->SetObjectField(TEXT("heightField"), HF);
			break;
		}
		default: break;
		}
		return Obj;
	}

	TSharedRef<FJsonObject> EntityToJson(const FZeusEntityExport& Entity)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("entityName"),    Entity.EntityName);
		Obj->SetStringField(TEXT("actorName"),     Entity.ActorName);
		Obj->SetStringField(TEXT("componentName"), Entity.ComponentName);
		Obj->SetStringField(TEXT("sourcePath"),    Entity.SourcePath);

		TSharedRef<FJsonObject> Region = MakeShared<FJsonObject>();
		Region->SetNumberField(TEXT("regionId"), Entity.Region.RegionId);
		Region->SetNumberField(TEXT("gridX"),    Entity.Region.GridX);
		Region->SetNumberField(TEXT("gridY"),    Entity.Region.GridY);
		Region->SetNumberField(TEXT("gridZ"),    Entity.Region.GridZ);
		Region->SetNumberField(TEXT("regionSizeCm"), Entity.Region.RegionSizeCm);
		Obj->SetObjectField(TEXT("region"), Region);

		TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
		Bounds->SetField(TEXT("center"), Vector3ToJson(Entity.BoundsCenter));
		Bounds->SetField(TEXT("extent"), Vector3ToJson(Entity.BoundsExtent));
		Obj->SetObjectField(TEXT("bounds"), Bounds);

		Obj->SetField(TEXT("entityWorldTransform"),
			MakeShared<FJsonValueObject>(TransformToJson(Entity.EntityWorldTransform)));

		TArray<TSharedPtr<FJsonValue>> ShapeArr;
		ShapeArr.Reserve(Entity.Shapes.Num());
		for (const FZeusShapeExport& Shape : Entity.Shapes)
		{
			ShapeArr.Add(MakeShared<FJsonValueObject>(ShapeToJson(Shape)));
		}
		Obj->SetArrayField(TEXT("shapes"), ShapeArr);

		TArray<TSharedPtr<FJsonValue>> WarnArr;
		for (const FString& W : Entity.Warnings)
		{
			WarnArr.Add(MakeShared<FJsonValueString>(W));
		}
		Obj->SetArrayField(TEXT("warnings"), WarnArr);
		return Obj;
	}
}

bool FZeusCollisionJsonWriter::Write(const FString& OutputPath,
	const FZeusExportResult& Result,
	const FZeusExportOptions& Options)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("format"),  TEXT("zeus_debug_collision"));
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("mapName"), Result.MapName);
	Root->SetStringField(TEXT("units"),   TEXT("centimeters"));

	TSharedRef<FJsonObject> Coord = MakeShared<FJsonObject>();
	Coord->SetStringField(TEXT("x"), TEXT("forward"));
	Coord->SetStringField(TEXT("y"), TEXT("right"));
	Coord->SetStringField(TEXT("z"), TEXT("up"));
	Root->SetObjectField(TEXT("coordinateSystem"), Coord);

	TArray<TSharedPtr<FJsonValue>> EntArr;
	EntArr.Reserve(Result.Entities.Num());
	for (const FZeusEntityExport& Entity : Result.Entities)
	{
		EntArr.Add(MakeShared<FJsonValueObject>(EntityToJson(Entity)));
	}
	Root->SetArrayField(TEXT("entities"), EntArr);

	TArray<TSharedPtr<FJsonValue>> VolArr;
	VolArr.Reserve(Result.Volumes.Num());
	for (const FZeusVolumeExport& Vol : Result.Volumes)
	{
		VolArr.Add(MakeShared<FJsonValueObject>(VolumeToJson(Vol)));
	}
	Root->SetArrayField(TEXT("volumes"), VolArr);

	TArray<TSharedPtr<FJsonValue>> TerArr;
	TerArr.Reserve(Result.TerrainPieces.Num());
	for (const FZeusTerrainPieceExport& Piece : Result.TerrainPieces)
	{
		TerArr.Add(MakeShared<FJsonValueObject>(TerrainPieceToJson(Piece)));
	}
	Root->SetArrayField(TEXT("terrainPieces"), TerArr);

	TSharedRef<FJsonObject> Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("entityCount"),  Result.Stats.EntityCount);
	Stats->SetNumberField(TEXT("shapeCount"),   Result.Stats.ShapeCount);
	Stats->SetNumberField(TEXT("boxCount"),     Result.Stats.BoxCount);
	Stats->SetNumberField(TEXT("sphereCount"),  Result.Stats.SphereCount);
	Stats->SetNumberField(TEXT("capsuleCount"), Result.Stats.CapsuleCount);
	Stats->SetNumberField(TEXT("convexCount"),  Result.Stats.ConvexCount);
	Stats->SetNumberField(TEXT("volumeCount"),       Result.Stats.VolumeCount);
	Stats->SetNumberField(TEXT("terrainPieceCount"), Result.Stats.TerrainPieceCount);
	Stats->SetNumberField(TEXT("triangleMeshCount"), Result.Stats.TriangleMeshCount);
	Stats->SetNumberField(TEXT("heightFieldCount"),  Result.Stats.HeightFieldCount);
	Stats->SetNumberField(TEXT("warningCount"), Result.Stats.WarningCount);
	Stats->SetNumberField(TEXT("skippedActorCount"), Result.Stats.SkippedActorCount);
	Root->SetObjectField(TEXT("stats"), Stats);

	TArray<TSharedPtr<FJsonValue>> WarnArr;
	for (const FString& W : Result.Warnings)
	{
		WarnArr.Add(MakeShared<FJsonValueString>(W));
	}
	Root->SetArrayField(TEXT("warnings"), WarnArr);

	TSharedRef<FJsonObject> ConfigObj = MakeShared<FJsonObject>();
	ConfigObj->SetNumberField(TEXT("regionSizeCm"), Options.RegionSizeCm);
	ConfigObj->SetBoolField(TEXT("allowComplexAsSimple"), Options.bAllowComplexAsSimple);
	Root->SetObjectField(TEXT("config"), ConfigObj);

	FString OutString;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutString);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] JSON serialization failed."));
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutString, *OutputPath))
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to save JSON to %s"), *OutputPath);
		return false;
	}
	return true;
}
