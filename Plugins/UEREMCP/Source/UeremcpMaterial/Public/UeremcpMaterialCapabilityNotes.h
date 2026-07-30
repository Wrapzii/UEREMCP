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

	/** Remaining limitations after Wave 2 MaterialTools wiring (elemental projectile slice). */
	inline TArray<FString> DefaultPostWireCapabilityNotes()
	{
		return {
			TEXT("Minimal master graph: ParticleColor*EmissiveScale → MP_EmissiveColor only; feature tokens in specification.features are accepted but not yet wired to graph nodes."),
			TEXT("expression_subclass_properties: unwired scalar/vector parameters exist on master but do not affect shading until graph wiring lands."),
			TEXT("material_function_internals: engine MaterialFunctions are not composed; masters are built from MaterialEditingLibrary expressions only."),
			TEXT("editor_chrome: comment boxes, preview settings, and layout beyond x/y are not round-tripped."),
			TEXT("textures.generate and procedural_texture slots are not executed (create_procedural_texture not implemented)."),
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
