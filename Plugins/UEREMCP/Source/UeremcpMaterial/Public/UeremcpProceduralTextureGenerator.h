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
}
