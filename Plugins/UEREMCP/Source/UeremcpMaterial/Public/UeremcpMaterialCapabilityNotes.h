// UEREMCP — Material known limitations surfaced on every create response (RB-08).
//
// Owner: WS-08. Keep in sync with schemas/domains/materials/README.md and
// docs/research/RB-08-materials-and-textures.md.

#pragma once

#include "CoreMinimal.h"

namespace UeremcpMaterialCapability
{
	/** Stable machine keys — also listed in README fidelity / capability_notes. */
	inline const TCHAR* LossyAreaExpressionProperties = TEXT("expression_subclass_properties");
	inline const TCHAR* LossyAreaMaterialFunctionInternals = TEXT("material_function_internals");
	inline const TCHAR* LossyAreaEditorChrome = TEXT("editor_chrome");

	/** Human-readable capability_notes for Wave 2 scaffold and partial responses. */
	inline TArray<FString> DefaultCreateCapabilityNotes()
	{
		return {
			TEXT("create_vfx_material is a Wave 2 scaffold: Epic MaterialTools batching is not yet wired."),
			TEXT("expression_subclass_properties: MaterialTools has no expression property setter; subclass fields require set_editor_property."),
			TEXT("material_function_internals: nested MaterialFunction graphs are lossy until separate retrieve is implemented."),
			TEXT("editor_chrome: comment boxes, preview settings, and layout beyond x/y are not round-tripped."),
			TEXT("blend_mode_shading_domain: MaterialTools omits blend mode, shading model, two-sided, and material domain — manual UObject property writes required."),
			TEXT("element_templates: WS-15 templates/elements/*.json presets are not yet loaded; element defaults are schema-only."),
			TEXT("procedural_texture: create_procedural_texture semantic op is not implemented; texture.generate slots are accepted but not executed."),
			TEXT("No material instance or compile validation on this scaffold — status is partially_completed, not created_and_validated."),
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
