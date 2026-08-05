// UEREMCP — Niagara scratch-path conventions (WS-07).
//
// Aligns with UeremcpScratchPaths.h (WS-11) and docs/POC_ACCEPTANCE.md.
// Inspect: any /Game/…  Mutate create/adapt/submit: sandbox + Magecraft.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

namespace UeremcpNiagaraPaths
{
	/** WS-11 integration / wave-2 probe root. Trailing path segments allowed. */
	inline const TCHAR* TestsContentRoot = TEXT("/Game/__UeremcpTests");

	/** POC scratch root (POC_ACCEPTANCE.md global rules). Trailing path segments allowed. */
	inline const TCHAR* PocContentRoot = TEXT("/Game/__UeremcpPoc");

	/** Production Magecraft authoring root (create/adapt/submit; replace-delete stays sandbox). */
	inline const TCHAR* MagecraftContentRoot = TEXT("/Game/RE/VFX/Magecraft");

	/** Any project content (inspect / material bind under /Game). */
	inline const TCHAR* GameContentRoot = TEXT("/Game");

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

	inline bool IsAllowedMagecraftPath(const FString& AssetPath)
	{
		return IsUnderContentRoot(AssetPath, MagecraftContentRoot);
	}

	/**
	 * WRITE create/adapt/submit targets: sandbox (__UeremcpTests / __UeremcpPoc) or Magecraft.
	 * mode=replace destructive delete remains sandbox-only (enforced by callers).
	 */
	inline bool IsAllowedMutatePath(const FString& AssetPath)
	{
		return IsAllowedProbePath(AssetPath) || IsAllowedMagecraftPath(AssetPath);
	}

	/** READ inspect: any /Game/… path (production Magecraft OK). */
	inline bool IsAllowedInspectPath(const FString& AssetPath)
	{
		return IsUnderContentRoot(AssetPath, GameContentRoot);
	}

	/**
	 * Renderer material bind targets under /Game (engine /Niagara|/Engine checked by callers).
	 * [UNVERIFIED] callers additionally allow /Niagara and /Engine prefixes.
	 */
	inline bool IsAllowedMaterialBindPath(const FString& AssetPath)
	{
		return IsAllowedInspectPath(AssetPath);
	}

	inline FString AllowedContentRootsDescription()
	{
		return FString::Printf(TEXT("%s or %s"), TestsContentRoot, PocContentRoot);
	}

	inline FString AllowedMutateRootsDescription()
	{
		return FString::Printf(
			TEXT("%s, %s, or %s"),
			TestsContentRoot,
			PocContentRoot,
			MagecraftContentRoot);
	}

	inline FString MutateDeniedReason(const FString& AssetPath)
	{
		return FString::Printf(
			TEXT("Niagara mutate path '%s' is outside allowed WRITE roots (%s)."),
			*AssetPath,
			*AllowedMutateRootsDescription());
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
