// UEREMCP — scratch-path and package-path helpers (WS-08).
//
// Mirrors UeremcpScratchPaths conventions without depending on UeremcpValidation.
// Write guard: /Game/__UeremcpTests/ and /Game/__UeremcpPoc/ only (POC_ACCEPTANCE).

#pragma once

#include "CoreMinimal.h"

namespace UeremcpMaterialPaths
{
	inline const TCHAR* TestsContentRoot = TEXT("/Game/__UeremcpTests");
	inline const TCHAR* PocContentRoot = TEXT("/Game/__UeremcpPoc");

	inline const TCHAR* MaterialsFolder = TEXT("/Game/__UeremcpTests/Materials");
	inline const TCHAR* MastersFolder = TEXT("/Game/__UeremcpTests/Materials/Masters");
	inline const TCHAR* TexturesFolder = TEXT("/Game/__UeremcpTests/Textures");

	/** True when SoftPath is under /Game/__UeremcpTests/ (write guard). */
	bool IsUnderTestsRoot(const FString& SoftPackagePath);

	/** True when SoftPath is under /Game/__UeremcpPoc/ (POC scratch — POC_ACCEPTANCE). */
	bool IsUnderPocRoot(const FString& SoftPackagePath);

	/** True when SoftPath is under an allowed scratch root (tests or POC). */
	bool IsUnderAllowedScratchRoot(const FString& SoftPackagePath);

	/** Returns TestsContentRoot or PocContentRoot when path is allowed; empty otherwise. */
	FString ResolveScratchContentRoot(const FString& SoftPackagePath);

	FString MaterialsFolderForContentRoot(const FString& ScratchContentRoot);
	FString MastersFolderForContentRoot(const FString& ScratchContentRoot);
	FString TexturesFolderForContentRoot(const FString& ScratchContentRoot);

	/** Split /Game/Folder/AssetName into folder_path and asset_name. */
	bool SplitPackagePath(const FString& SoftPackagePath, FString& OutFolder, FString& OutAssetName);

	FString JoinPackagePath(const FString& Folder, const FString& AssetName);
}
