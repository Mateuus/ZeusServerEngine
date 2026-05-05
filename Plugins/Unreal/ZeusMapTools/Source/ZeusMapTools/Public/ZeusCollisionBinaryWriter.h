// Copyright Zeus Server Engine. All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FZeusExportResult;
struct FZeusExportOptions;

/**
 * Emits static_collision.zsm using the ZSMC v1 layout:
 *   Header: 'Z','S','M','C' magic, version=1, headerSize, flags, entityCount,
 *           shapeCount, regionSizeCm (u32), mapName (u16 length + UTF-8 bytes).
 *   Per entity: name strings, world transform (3 doubles loc + 4 doubles quat
 *               XYZW + 3 doubles scale), bounds (center + extent doubles),
 *               regionId (u32) + grid (i32 X/Y/Z), shapeCount (u32) + shapes.
 *   Per shape: type (u8), local transform (loc/quat/scale doubles) + per-type
 *              payload (box: 3 doubles half extents; sphere: radius double;
 *              capsule: radius + halfHeight; convex: vertexCount u32 + verts).
 *
 * Everything is little-endian. The reader on the server (ZsmCollisionLoader)
 * must match this layout byte for byte.
 */
class FZeusCollisionBinaryWriter
{
public:
	static bool Write(const FString& OutputPath,
		const FZeusExportResult& Result,
		const FZeusExportOptions& Options);
};
