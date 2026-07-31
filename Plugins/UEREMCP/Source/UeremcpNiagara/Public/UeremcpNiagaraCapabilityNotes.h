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
			TEXT("create_niagara_effect creates probe Niagara systems: projectile (POC B six roles) or precipitation/rain/weather (rain+mist roles via RecycleParticlesInView/HangingParticulates), optional User.* params, compile await, save."),
			TEXT("material_bindings: assigns probe UMaterialInterface paths via GetRendererData/SetRendererData; inline create_spec delegates to UeremcpMaterialNiagaraExport (probe MI paths only)."),
			TEXT("orphaned_inline_creates lists roles where inline MI creation succeeded but renderer bind/re-read failed — Create::Run continues as partially_completed; probe MIs are never deleted."),
			TEXT("POC B emitters non-empty / renderer-bound checks surface via extra.poc_b_gates; B7_renderers_bound true only after material bind re-read verify; extracted inspect material_path is never validated."),
			TEXT("created_and_validated / modified_and_validated require saved + compile-up-to-date, six roles, user parameters, verified material bindings/renderers, structural re-read, and complete change manifest; otherwise status remains partially_completed."),
			TEXT("execute_plan: create_niagara_effect registers with FUeremcpPlanExecutor at module startup; template instantiation remains partially_completed until atomic transaction callbacks (WS-03) and create_vfx_material handler (WS-08) land."),
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
