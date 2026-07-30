// UEREMCP — pixel generators for procedural textures (WS-08).

#pragma once

#include "CoreMinimal.h"

namespace UeremcpProceduralTextureGenerator
{
	bool GeneratePixels(
		const FString& Kind,
		int32 Width,
		int32 Height,
		int32 Seed,
		TArray<FColor>& OutPixels,
		FString& OutError);

	/** Pack procedural per-frame cells into a flipbook atlas grid [VERIFIED: ImageUtils.h:268]. */
	bool GenerateFlipbookAtlasPixels(
		int32 AtlasWidth,
		int32 AtlasHeight,
		int32 Columns,
		int32 Rows,
		int32 FrameCount,
		int32 Seed,
		TArray<FColor>& OutPixels,
		FString& OutError);
}
