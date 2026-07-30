// UEREMCP — Material known limitations surfaced on create_vfx_material responses (RB-08).
//
// Owner: WS-08. Keep in sync with schemas/domains/materials/README.md.

#pragma once

#include "CoreMinimal.h"

namespace UeremcpMaterialCapability
{
	inline const TCHAR* LossyAreaExpressionProperties = TEXT("expression_subclass_properties");
	inline const TCHAR* LossyAreaMaterialFunctionInternals = TEXT("material_function_internals");
	inline const TCHAR* LossyAreaEditorChrome = TEXT("editor_chrome");

	/** Remaining limitations after feature-graph wiring (elemental projectile slice). */
	inline TArray<FString> DefaultPostWireCapabilityNotes()
	{
		return {
			TEXT("feature_graph_v1: simplified expression wiring — no engine MaterialFunction composition; distortion/flow_maps/flipbook_subuv tokens are not implemented."),
			TEXT("material_function_internals: nested MaterialFunction graphs are not composed."),
			TEXT("editor_chrome: comment boxes, preview settings, and layout beyond x/y are not round-tripped."),
			TEXT("procedural_texture_v1: CPU pixel fill via FImageUtils::CreateTexture2D — not Epic MaterialTools/RT draw; flipbook_subuv assembly is not implemented."),
			TEXT("Substrate interaction with Unlit/Additive masters not runtime-verified on RE project."),
		};
	}

	inline TArray<FString> DefaultFidelityLossyAreas()
	{
		return {
			LossyAreaExpressionProperties,
			LossyAreaMaterialFunctionInternals,
			LossyAreaEditorChrome,
		};
	}
}
