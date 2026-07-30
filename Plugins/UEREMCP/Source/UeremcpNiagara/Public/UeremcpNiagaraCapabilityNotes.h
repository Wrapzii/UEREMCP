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

	/** Human-readable capability_notes for Wave 2 inspect stub and partial responses. */
	inline TArray<FString> DefaultInspectCapabilityNotes()
	{
		return {
			TEXT("inspect_system reads topology via UNiagaraExternalEditUtilities (Epic NiagaraToolsets composition surface)."),
			TEXT("event_handler_stacks: GetEmitterTopology omits ParticleEventScript stacks; extensions.niagara.event_handlers is not populated."),
			TEXT("module_reorder_without_readd: no ReorderModule AICallable on NiagaraToolsets; reorder requires remove+re-add or internal MoveModule."),
			TEXT("script_graph_internals: NiagaraScriptGraph (module/dynamic-input EdGraphs) is out of POC B/C scope."),
			TEXT("fidelity.round_trip_supported remains false until retrieve -> replace -> retrieve hash stability is proven."),
			TEXT("No headless particle-count validation on current Epic toolset surface (RB-07 D.17)."),
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
}
