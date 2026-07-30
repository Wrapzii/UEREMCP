// UEREMCP — element preset defaults for create_vfx_material (RB-08 §D, ADR-0008).
//
// Data source: schemas/domains/materials/element_presets.v1.json (runtime loader + C++ fallback).

#pragma once

#include "CoreMinimal.h"

/** Scalar/vector MI parameters applied after master resolution. */
struct FUeremcpMaterialParameterSet
{
	FLinearColor ParticleColor = FLinearColor::White;
	FLinearColor ColorSecondary = FLinearColor::White;
	float EmissiveScale = 1.0f;
	float FlowSpeed = 0.5f;
	float Turbulence = 0.5f;
	float SoftEdge = 0.75f;
	float DepthFade = 100.0f;
	float DissolveAmount = 0.0f;
};

namespace UeremcpMaterialElementPresets
{
	/** Resolve purpose → master asset name (not full path). */
	FString ResolveMasterAssetName(const FString& Purpose);

	/** Resolve purpose + features → master package path under MastersFolder. */
	FString ResolveMasterPackagePath(const FString& Purpose, const TArray<FString>& Features);

	/** Element defaults; returns false when element is unknown (caller may use custom overrides only). */
	bool GetElementDefaults(const FString& Element, FUeremcpMaterialParameterSet& OutPreset);

	/** Apply WS-15 supported_modifiers on top of element defaults. */
	void ApplyModifiers(
		const TArray<FString>& Modifiers,
		const FString& Purpose,
		FUeremcpMaterialParameterSet& InOutPreset);

	/** Merge specification.parameter_overrides JSON fields into preset. */
	void MergeParameterOverrides(
		const TSharedPtr<class FJsonObject>& Overrides,
		FUeremcpMaterialParameterSet& InOutPreset);
}
