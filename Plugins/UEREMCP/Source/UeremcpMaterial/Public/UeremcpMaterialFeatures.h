// UEREMCP — specification.features resolution for create_vfx_material (WS-08).
//
// Default feature sets align with WS-15 niagara.projectile.elemental.v1 construction_plan.
// Master asset names include a stable feature signature so graph variants do not collide.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpMaterialPaths.h"

class UMaterial;

namespace UeremcpMaterialFeatures
{
	/** Parse specification.features JSON array (may be empty). */
	void ParseFeaturesFromSpec(const TSharedPtr<class FJsonObject>& Spec, TArray<FString>& OutFeatures);

	/** Purpose defaults when features omitted (elemental projectile family). */
	TArray<FString> DefaultFeaturesForPurpose(const FString& Purpose);

	/** Merge request features with purpose defaults when request is empty. */
	TArray<FString> ResolveFeaturesForPurpose(const FString& Purpose, const TArray<FString>& Requested);

	/** Stable short suffix for master asset naming (8 hex chars). */
	FString ComputeFeatureSignature(const TArray<FString>& Features);

	/** True for trail/ribbon purposes. */
	bool IsTrailPurpose(const FString& Purpose);

	/** Master base name without signature, e.g. M_Ueremcp_ProjCore. */
	FString MasterBaseAssetName(const FString& Purpose);

	/** Full package path including feature signature under the given scratch content root. */
	FString ResolveMasterPackagePath(
		const FString& Purpose,
		const TArray<FString>& Features,
		const FString& ScratchContentRoot = UeremcpMaterialPaths::TestsContentRoot);

	/** Feature tokens we can wire in Wave 2 feature-graph slice. */
	bool IsImplementedFeature(const FString& Feature);

	/** Tokens requested but not implemented — surfaced in capability_notes. */
	TArray<FString> FindUnimplementedFeatures(const TArray<FString>& Features);

	struct FFeatureGraphVerifyResult
	{
		bool bEmissiveConnected = false;
		bool bOpacityConnected = false;
		TMap<FString, bool> FeatureWired;
	};

	/** Post-build verification: output properties connected + expression types present. */
	bool VerifyFeatureGraph(UMaterial* Material, const TArray<FString>& Features, FFeatureGraphVerifyResult& OutResult);
}
