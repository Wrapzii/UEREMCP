// UEREMCP — Niagara known limitations surfaced on every inspect response (RB-07).
//
// Owner: WS-07. Keep in sync with schemas/domains/niagara/graph-ext.schema.json
// fidelity.lossy_areas and docs/research/RB-07-niagara.md.

#pragma once

#include "CoreMinimal.h"

namespace UeremcpNiagaraCapability
{
	/** Stable machine keys — also listed in graph.fidelity.lossy_areas. */
	inline const TCHAR* LossyAreaEventHandlerStacks = TEXT("event_handler_stacks");
	inline const TCHAR* LossyAreaModuleReorder = TEXT("module_reorder_without_readd");
	inline const TCHAR* LossyAreaScriptGraphInternals = TEXT("script_graph_internals");
	inline const TCHAR* LossyAreaRendererMaterialBindings = TEXT("renderer_material_bindings");

	/** Human-readable capability_notes for Wave 2 inspect stub and partial responses. */
	inline TArray<FString> DefaultInspectCapabilityNotes()
	{
		return {
			TEXT("inspect_system reads topology via UNiagaraExternalEditUtilities (Epic NiagaraToolsets composition surface)."),
			TEXT("GetEmitterTopology supplies renderer refs; GetRendererData propertyValues are mapped to extensions.niagara.renderers[] without material validation (renderer_material_bindings lossy on emitter graphs)."),
			TEXT("event_handler_stacks: GetEmitterTopology omits ParticleEventScript stacks; extensions.niagara.event_handlers[] holds inferred placeholders from GetStackIssues / compile per_script only."),
			TEXT("module_reorder_without_readd: no ReorderModule AICallable on NiagaraToolsets; reorder requires remove+re-add or internal MoveModule."),
			TEXT("script_graph_internals: NiagaraScriptGraph (module/dynamic-input EdGraphs) is out of POC B/C scope."),
			TEXT("fidelity.round_trip_supported remains false until retrieve -> replace -> retrieve content_hash stability is proven (FUeremcpNiagaraHashRoundTrip scaffold only)."),
			TEXT("No headless particle-count validation on current Epic toolset surface (RB-07 D.17)."),
		};
	}

	inline TArray<FString> DefaultCreateCapabilityNotes()
	{
		return {
			TEXT("create_niagara_effect is a POC B probe slice: duplicate template, add six emitter roles (core, flame_shell, sparks, smoke, ribbon_trail, impact_burst), optional User.* params, compile await, save."),
			TEXT("material_bindings: assigns probe UMaterialInterface paths via GetRendererData/SetRendererData; inline create_spec delegates to UeremcpMaterialNiagaraExport (probe MI paths only)."),
			TEXT("orphaned_inline_creates lists roles where inline MI creation succeeded but renderer bind/re-read failed — Create::Run continues as partially_completed; probe MIs are never deleted."),
			TEXT("POC B emitters non-empty / renderer-bound checks are not yet implemented — status stays partially_completed, never *_validated."),
			TEXT("envelope mode 'replace' deletes and recreates probe assets under /Game/__UeremcpTests/ only; never deletes user content elsewhere."),
			TEXT("options.validate=true runs post-create inspect (FUeremcpNiagaraRoundTrip): structural emitter/user-var match + content_hash manifest; not content_hash round-trip stability."),
			TEXT("module_reorder_without_readd and event_handler_stacks fidelity gaps apply to created systems too."),
		};
	}

	inline TArray<FString> DefaultFidelityLossyAreas()
	{
		return {
			LossyAreaEventHandlerStacks,
			LossyAreaModuleReorder,
			LossyAreaScriptGraphInternals,
		};
	}

	inline TArray<FString> EmitterGraphLossyAreas(bool bHasRenderers)
	{
		TArray<FString> Areas = DefaultFidelityLossyAreas();
		if (bHasRenderers)
		{
			Areas.Add(LossyAreaRendererMaterialBindings);
		}
		return Areas;
	}
}
