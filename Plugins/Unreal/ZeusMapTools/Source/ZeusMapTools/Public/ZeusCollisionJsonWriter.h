// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FZeusExportResult;
struct FZeusExportOptions;

/**
 * Emits debug_collision.json (zeus_debug_collision v1).
 * Boxes are written as halfExtent (extentSemantics: "halfExtent") so the loader
 * doesn't have to guess. Translations are in centimeters.
 */
class FZeusCollisionJsonWriter
{
public:
	static bool Write(const FString& OutputPath,
		const FZeusExportResult& Result,
		const FZeusExportOptions& Options);
};
