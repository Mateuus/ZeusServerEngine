// Copyright Zeus Server Engine. All rights reserved.

#include "ZeusMapExportCommandlet.h"

#include "ZeusCollisionExporter.h"
#include "ZeusMapTools.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/WorldComposition.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

UZeusMapExportCommandlet::UZeusMapExportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UZeusMapExportCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamsMap;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

	const FString MapPath = ParamsMap.FindRef(TEXT("Map"));
	const FString OutputDir = ParamsMap.FindRef(TEXT("Output"));
	const FString MapNameOverride = ParamsMap.FindRef(TEXT("MapName"));
	const FString RegionSizeStr = ParamsMap.FindRef(TEXT("RegionSize"));

	const bool bWriteJson = Switches.Contains(TEXT("DebugJson")) || Switches.Contains(TEXT("Json"));
	const bool bWriteZsm = Switches.Contains(TEXT("BuildZsm")) || Switches.Contains(TEXT("Zsm"));
	const bool bAllowComplex = Switches.Contains(TEXT("AllowComplexAsSimple"));
	const bool bValidateOnly = Switches.Contains(TEXT("Validate"));
	const bool bSkipDynamic = Switches.Contains(TEXT("SkipDynamicZsm")) || Switches.Contains(TEXT("NoDynamicZsm"));
	const bool bSkipTerrain = Switches.Contains(TEXT("SkipTerrainZsm")) || Switches.Contains(TEXT("NoTerrainZsm"));
	const bool bForceDynamic = Switches.Contains(TEXT("BuildDynamicZsm")) || Switches.Contains(TEXT("DynamicZsm"));
	const bool bForceTerrain = Switches.Contains(TEXT("BuildTerrainZsm")) || Switches.Contains(TEXT("TerrainZsm"));

	if (MapPath.IsEmpty())
	{
		UE_LOG(LogZeusMapTools, Error,
			TEXT("[ZeusMapTools] Missing -Map=/Game/Maps/<Name>. Aborting."));
		return 1;
	}

	UE_LOG(LogZeusMapTools, Log, TEXT("[ZeusMapTools] Commandlet starting Map=%s Output=%s"),
		*MapPath, *OutputDir);

	FString PackageName;
	if (!FPackageName::TryConvertFilenameToLongPackageName(MapPath, PackageName))
	{
		PackageName = MapPath;
	}

	UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	if (!Package)
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] Failed to load package %s"), *PackageName);
		return 2;
	}

	UWorld* World = UWorld::FindWorldInPackage(Package);
	if (!World)
	{
		UE_LOG(LogZeusMapTools, Error, TEXT("[ZeusMapTools] No UWorld in package %s"), *PackageName);
		return 3;
	}

	World->WorldType = EWorldType::Editor;
	if (World->GetWorldSettings() == nullptr)
	{
		World->InitializeNewWorld();
	}

	if (!World->bIsWorldInitialized)
	{
		World->InitWorld(UWorld::InitializationValues()
			.AllowAudioPlayback(false)
			.CreatePhysicsScene(false)
			.RequiresHitProxies(false)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(false)
			.EnableTraceCollision(false)
			.SetTransactional(false)
			.CreateFXSystem(false));
	}

	FZeusExportOptions Options;
	Options.bIncludeSelectedOnly = false;
	Options.bWriteJson = bWriteJson || (!bWriteJson && !bWriteZsm); // default writes both
	Options.bWriteZsm = bWriteZsm  || (!bWriteJson && !bWriteZsm);
	Options.bWriteDynamicZsm = !bSkipDynamic && (bForceDynamic || Options.bWriteJson || Options.bWriteZsm);
	Options.bWriteTerrainZsm = !bSkipTerrain && (bForceTerrain || Options.bWriteJson || Options.bWriteZsm);
	Options.bDrawDebug = false;
	Options.bAllowComplexAsSimple = bAllowComplex;
	Options.bValidateOnly = bValidateOnly;
	Options.OutputDir = OutputDir;
	Options.MapNameOverride = MapNameOverride;
	if (!RegionSizeStr.IsEmpty())
	{
		const double Parsed = FCString::Atod(*RegionSizeStr);
		if (Parsed > 0.0)
		{
			Options.RegionSizeCm = Parsed;
		}
	}

	FZeusCollisionExporter Exporter;
	const FZeusExportResult Result = Exporter.ExportFromWorld(World, Options);

	const int32 TotalExported = Result.Stats.EntityCount + Result.Stats.VolumeCount
		+ Result.Stats.TerrainPieceCount;
	UE_LOG(LogZeusMapTools, Log,
		TEXT("[ZeusMapTools] Commandlet done entities=%d volumes=%d terrainPieces=%d shapes=%d warnings=%d"),
		Result.Stats.EntityCount, Result.Stats.VolumeCount, Result.Stats.TerrainPieceCount,
		Result.Stats.ShapeCount, Result.Stats.WarningCount);

	return TotalExported > 0 ? 0 : 4;
}
