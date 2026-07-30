// UEREMCP — Phase C MaterialFunction composition (WS-08).
//
// Stub: probes engine MF candidates and reports honest partially_completed for the
// composition subsystem while UeremcpMaterialFeatureGraph expression_fallback proceeds.
//
// Verified substrate (UE 5.8 headers on this machine):
// - UMaterialExpressionMaterialFunctionCall::SetMaterialFunction
//   [VERIFIED: MaterialExpressionMaterialFunctionCall.h:157-158]
// - UMaterialEditingLibrary::UpdateMaterialFunction
//   [VERIFIED: MaterialEditingLibrary.h:383-388]
// - UMaterialEditingLibrary::CreateMaterialExpressionInFunction
//   [VERIFIED: MaterialEditingLibrary.h:367-373]

#pragma once

#include "CoreMinimal.h"

class UMaterial;

struct FUeremcpMaterialFunctionComposeResult
{
	/** Composition subsystem status — partially_completed until AssetRegistry audit lands. */
	FString Status;

	FString Summary;

	/** True when expression graph fallback remains authoritative (Phase C stub). */
	bool bUsedExpressionFallback = true;

	/** Feature tokens deferred pending engine MaterialFunction path resolution. */
	TArray<FString> DeferredFeatures;

	TArray<FString> InterpretationNotes;
	TArray<FString> CapabilityNotes;
};

namespace UeremcpMaterialFunctionComposer
{
	/**
	 * Phase C stub: identify MF composition candidates and report deferred status.
	 * Does not invoke SetMaterialFunction — see docs/proposals/ws-08-material-function-composition.md.
	 */
	FUeremcpMaterialFunctionComposeResult ProbeComposition(
		UMaterial* Material,
		const TArray<FString>& Features);

	/**
	 * Phase C stub: single-feature compose attempt. Always returns partially_completed
	 * until WS-02 supplies audited engine MaterialFunction paths.
	 */
	FUeremcpMaterialFunctionComposeResult TryComposeFeature(
		UMaterial* Material,
		const FString& FeatureToken);
}
