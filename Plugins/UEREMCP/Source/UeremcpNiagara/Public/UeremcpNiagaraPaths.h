// UEREMCP — Niagara scratch-path conventions (WS-07).
//
// Aligns with UeremcpScratchPaths.h (WS-11) and docs/POC_ACCEPTANCE.md.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

namespace UeremcpNiagaraPaths
{
	/** WS-11 integration / wave-2 probe root. Trailing path segments allowed. */
	inline const TCHAR* TestsContentRoot = TEXT("/Game/__UeremcpTests");

	/** POC scratch root (POC_ACCEPTANCE.md global rules). Trailing path segments allowed. */
	inline const TCHAR* PocContentRoot = TEXT("/Game/__UeremcpPoc");

	inline bool IsUnderContentRoot(const FString& AssetPath, const TCHAR* Root)
	{
		if (AssetPath.IsEmpty() || !Root || Root[0] == TEXT('\0'))
		{
			return false;
		}

		FString Normalized = AssetPath;
		Normalized.TrimStartAndEndInline();
		if (!Normalized.StartsWith(TEXT("/")))
		{
			Normalized = TEXT("/") + Normalized;
		}

		const int32 RootLen = FCString::Strlen(Root);
		if (!Normalized.StartsWith(Root, ESearchCase::CaseSensitive))
		{
			return false;
		}

		return Normalized.Len() == RootLen || Normalized[RootLen] == TEXT('/');
	}

	/** True when AssetPath is under TestsContentRoot or PocContentRoot (strict prefix). */
	inline bool IsAllowedProbePath(const FString& AssetPath)
	{
		return IsUnderContentRoot(AssetPath, TestsContentRoot)
			|| IsUnderContentRoot(AssetPath, PocContentRoot);
	}

	inline FString AllowedContentRootsDescription()
	{
		return FString::Printf(TEXT("%s or %s"), TestsContentRoot, PocContentRoot);
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
