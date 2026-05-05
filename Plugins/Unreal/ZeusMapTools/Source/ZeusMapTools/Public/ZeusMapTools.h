// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

ZEUSMAPTOOLS_API DECLARE_LOG_CATEGORY_EXTERN(LogZeusMapTools, Log, All);

/**
 * Editor module that registers Slate menu entries (Export Selected, Export Level,
 * Preview, Validate) and the headless commandlet `ZeusMapExport`.
 *
 * Source of collision data: UStaticMeshComponent -> UStaticMesh -> UBodySetup ->
 * FKAggregateGeom (Box/Sphere/Sphyl/Convex). The yellow Unreal selection overlay
 * is only used as a visual reference; the real collision exported to the server
 * comes from the simple collision aggregate geometry.
 */
class FZeusMapToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void UnregisterMenus();

	FDelegateHandle ToolMenusStartupHandle;
};
