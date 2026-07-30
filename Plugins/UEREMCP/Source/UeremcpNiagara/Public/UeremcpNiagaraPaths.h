// UEREMCP — Niagara scratch-path conventions (WS-07).
//
// Aligns with tests/README.md and UeremcpScratchPaths.h (WS-11).

#pragma once

#include "CoreMinimal.h"

namespace UeremcpNiagaraPaths
{
	/** Soft package root for WS-07 editor probes. Trailing path segments allowed. */
	inline const TCHAR* TestsContentRoot = TEXT("/Game/__UeremcpTests");

	/** True when AssetPath is under TestsContentRoot. */
	inline bool IsAllowedProbePath(const FString& AssetPath)
	{
		return AssetPath.StartsWith(TestsContentRoot);
	}

	/** Package folder from a soft object path (strips /AssetName suffix if present). */
	inline FString PackageFolderFromAssetPath(const FString& AssetPath)
	{
		FString Normalized = AssetPath;
		Normalized.TrimStartAndEndInline();
		if (Normalized.Contains(TEXT(".")))
		{
			Normalized = Normalized.Left(Normalized.Find(TEXT(".")));
		}
		return FPackageName::GetLongPackagePath(Normalized);
	}

	/** Asset name token from a soft object or package path. */
	inline FString AssetNameFromAssetPath(const FString& AssetPath)
	{
		FString Normalized = AssetPath;
		Normalized.TrimStartAndEndInline();
		if (Normalized.Contains(TEXT(".")))
		{
			Normalized = Normalized.Left(Normalized.Find(TEXT(".")));
		}
		return FPackageName::GetLongPackageAssetName(Normalized);
	}
}
