// UEREMCP — runtime loader for schemas/domains/materials/element_presets.v1.json (WS-08).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpMaterialElementPresets.h"

namespace UeremcpMaterialElementPresetsLoader
{
	/** Resolve on-disk path (repo schemas or bundled Resources fallback). */
	FString ResolvePresetsJsonPath();

	/** True when the last EnsureLoaded() parsed element_presets.v1.json successfully. */
	bool IsLoadedFromJson();

	/** Path used by the last successful load (empty when using C++ fallback only). */
	FString GetLoadedPath();

	/** Element defaults from JSON; false when element unknown or JSON unavailable. */
	bool TryGetElementDefaults(const FString& Element, FUeremcpMaterialParameterSet& OutPreset);

	/** purpose_default_features from JSON; false when purpose unknown or JSON unavailable. */
	bool TryGetPurposeDefaultFeatures(const FString& Purpose, TArray<FString>& OutFeatures);
}
