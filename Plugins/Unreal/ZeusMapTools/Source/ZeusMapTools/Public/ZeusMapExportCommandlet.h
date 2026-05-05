// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "ZeusMapExportCommandlet.generated.h"

/**
 * Headless commandlet for exporting collision from a level on the command line.
 *
 * Example:
 *   UnrealEditor-Cmd.exe Project.uproject -run=ZeusMapExport
 *     -Map=/Game/Maps/TestWorld
 *     -Output=Server\ZeusServerEngine\Data\Maps\TestWorld
 *     -DebugJson -BuildZsm -RegionSize=5000
 */
UCLASS()
class UZeusMapExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UZeusMapExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};
