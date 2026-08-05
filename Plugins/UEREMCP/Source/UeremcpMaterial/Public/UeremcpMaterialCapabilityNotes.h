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
			TEXT("feature_graph_v1: simplified expression wiring — no engine MaterialFunction composition; distortion uses BumpOffset UV parallax (not true refraction)."),
			TEXT("material_function_internals: nested MaterialFunction graphs are not composed."),
			TEXT("editor_chrome: comment boxes, preview settings, and layout beyond x/y are not round-tripped."),
			TEXT("procedural_texture_v1: CPU pixel fill via FImageUtils::CreateTexture2D — not Epic MaterialTools/RT draw; flipbook_atlas assembles procedural per-frame cells (no external sheet import)."),
			TEXT("execute_plan: create_vfx_material registers with FUeremcpPlanExecutor at module startup; mutating create stays partially_completed until WS-11 RE runtime validation."),
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

	inline TArray<FString> DefaultInspectCapabilityNotes()
	{
		return {
			TEXT("PREFER InspectMaterial for masters/MIs under /Game (incl. Free_Spells) — one call returns result.graphs[] + parameter values."),
			TEXT("fidelity.round_trip_supported=false (honest) until retrieve→submit→retrieve content_hash stability is proven."),
			TEXT("expression_subclass_properties: MaterialEditingLibrary has no generic expression property setter — constants/defaults beyond ParameterName/position are lossy."),
			TEXT("material_function_internals: nested MaterialFunction graphs are not expanded."),
			TEXT("editor_chrome: comment boxes beyond Desc, preview settings, and layout beyond x/y are not round-tripped."),
			TEXT("Visual proof: CaptureMaterialFrames (UeremcpValidation) — not a structural gate."),
		};
	}

	inline TArray<FString> DefaultSubmitCapabilityNotes()
	{
		return {
			TEXT("SubmitMaterialGraph: prefer in-place MIC parameters and existing-node link/property rewires. Never silent-deletes production masters."),
			TEXT("create_missing_expressions / delete_missing_expressions: scratch roots only; delete also requires mode=replace and is not auto-executed until round-trip proven."),
			TEXT("fidelity.round_trip_supported=false — status stays partially_completed / no_change_required (never *_validated for submit yet)."),
			TEXT("New masters from empty graphs[]: use CreateMasterMaterial / CreateVfxMaterial; submit applies to existing assets."),
			TEXT("Proof path after submit: InspectMaterial then CaptureMaterialFrames."),
		};
	}
}
