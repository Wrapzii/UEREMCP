// UEREMCP — scratch-path and package-path helpers (WS-08).
//
// Mirrors UeremcpScratchPaths conventions without depending on UeremcpValidation.

#pragma once

#include "CoreMinimal.h"

namespace UeremcpMaterialPaths
{
	inline const TCHAR* TestsContentRoot = TEXT("/Game/__UeremcpTests");
	inline const TCHAR* MaterialsFolder = TEXT("/Game/__UeremcpTests/Materials");
	inline const TCHAR* MastersFolder = TEXT("/Game/__UeremcpTests/Materials/Masters");
	inline const TCHAR* TexturesFolder = TEXT("/Game/__UeremcpTests/Textures");

	/** True when SoftPath is under /Game/__UeremcpTests/ (write guard). */
	bool IsUnderTestsRoot(const FString& SoftPackagePath);

	/** Split /Game/Folder/AssetName into folder_path and asset_name. */
	bool SplitPackagePath(const FString& SoftPackagePath, FString& OutFolder, FString& OutAssetName);

	FString JoinPackagePath(const FString& Folder, const FString& AssetName);
}
