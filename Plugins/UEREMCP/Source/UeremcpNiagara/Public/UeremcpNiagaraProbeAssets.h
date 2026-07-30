// UEREMCP — probe-scoped asset lifecycle helpers (WS-07).
//
// Destructive operations are refused outside /Game/__UeremcpTests/ (AGENTS.md rule 8).

#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;

namespace UeremcpNiagaraProbeAssets
{
	/** True when Mode is replace (case-insensitive). */
	bool IsReplaceMode(const FString& Mode);

	/** True when asset registry reports an object at AssetPath. */
	bool AssetExistsAtPath(const FString& AssetPath);

	/**
	 * Drop transient compile-time referencers (e.g. mesh renderer info DIs) before probe delete.
	 * Safe to call while the system asset remains on disk.
	 */
	void ReleaseExternalReferences(UNiagaraSystem* System);

	/**
	 * Delete a Niagara system at AssetPath. Refuses paths outside TestsContentRoot.
	 * Returns true when nothing existed or deletion succeeded.
	 * [VERIFIED: ObjectTools::DeleteObjectsUnchecked — Engine/Source/Editor/UnrealEd/Public/ObjectTools.h:364]
	 */
	bool DeleteProbeAssetAtPath(const FString& AssetPath, FString& OutError);
}
