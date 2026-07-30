// UEREMCP — Phase C MaterialFunction composition stub (WS-08).

#include "UeremcpMaterialFunctionComposer.h"

#include "Materials/Material.h"

namespace
{
	/** Mirrors feature_composition.v1.json engine_material_function candidates (null path). */
	static const TSet<FString>& EngineMaterialFunctionCandidates()
	{
		static const TSet<FString> Candidates = {
			TEXT("fresnel"),
			TEXT("depth_fade"),
		};
		return Candidates;
	}

	static FUeremcpMaterialFunctionComposeResult MakeDeferredStubResult(
		const TArray<FString>& DeferredFeatures)
	{
		FUeremcpMaterialFunctionComposeResult Result;
		Result.bUsedExpressionFallback = true;
		Result.DeferredFeatures = DeferredFeatures;
		Result.Status = TEXT("partially_completed");
		Result.Summary = FString::Printf(
			TEXT("MaterialFunction composition stub: %d feature(s) deferred (engine MF paths unresolved); expression_fallback wired."),
			DeferredFeatures.Num());
		Result.InterpretationNotes.Add(TEXT(
			"Phase C stub — UMaterialExpressionMaterialFunctionCall::SetMaterialFunction not invoked "
			"[VERIFIED: MaterialExpressionMaterialFunctionCall.h:157-158]."));
		Result.InterpretationNotes.Add(TEXT(
			"Pending UMaterialEditingLibrary::UpdateMaterialFunction after compose "
			"[VERIFIED: MaterialEditingLibrary.h:383-388]."));
		Result.CapabilityNotes.Add(TEXT(
			"material_function_composition_v1: engine MaterialFunction paths require WS-02 AssetRegistry audit."));
		return Result;
	}
}

FUeremcpMaterialFunctionComposeResult UeremcpMaterialFunctionComposer::ProbeComposition(
	UMaterial* Material,
	const TArray<FString>& Features)
{
	FUeremcpMaterialFunctionComposeResult Result;
	Result.Status = TEXT("no_change_required");
	Result.Summary = TEXT("No engine MaterialFunction composition candidates in feature set.");
	Result.bUsedExpressionFallback = true;

	if (!Material)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("Null material for MaterialFunction composition probe.");
		return Result;
	}

	const TSet<FString>& Candidates = EngineMaterialFunctionCandidates();
	for (const FString& Feature : Features)
	{
		if (Candidates.Contains(Feature))
		{
			Result.DeferredFeatures.Add(Feature);
		}
	}

	if (Result.DeferredFeatures.Num() == 0)
	{
		return Result;
	}

	return MakeDeferredStubResult(Result.DeferredFeatures);
}

FUeremcpMaterialFunctionComposeResult UeremcpMaterialFunctionComposer::TryComposeFeature(
	UMaterial* Material,
	const FString& FeatureToken)
{
	if (!Material)
	{
		FUeremcpMaterialFunctionComposeResult Result;
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("Null material for TryComposeFeature.");
		return Result;
	}

	if (!EngineMaterialFunctionCandidates().Contains(FeatureToken))
	{
		FUeremcpMaterialFunctionComposeResult Result;
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("Feature '%s' is not an engine MaterialFunction composition candidate."),
			*FeatureToken);
		Result.bUsedExpressionFallback = true;
		return Result;
	}

	return MakeDeferredStubResult({ FeatureToken });
}
