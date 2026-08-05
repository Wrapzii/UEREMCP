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
			TEXT("PATH POLICY: inspect_system is READ-ONLY for any /Game/… path — including production /Game/RE/VFX/Magecraft/. Primary payload is result.topology_summary (emitters→modules→renderers) THEN result.graphs[] + user_parameters + fidelity."),
			TEXT("YES one call: topology_summary lists every emitter + EmitterSpawn/Update + ParticleSpawn/Update modules + renderer classes. response_detail=complete (default) also returns graphs[] with module inputs when include_input_values."),
			TEXT("topology_summary is always at the top of result so agents need no Python to scan stacks (docs/WHY.md — return adjacent context now)."),
			TEXT("response_detail=summary → topology_summary + counts (no full graphs). response_detail=complete (default) → topology_summary + full graphs[]. minimal → topology_summary only."),
			TEXT("YES READ: extensions.niagara.sim_target (GetEmitterTopology) + life_cycle{mode,loop_behavior,loop_duration,inactive_response} from Emitter State inputs when present."),
			TEXT("YES READ: event_handlers[] from FVersionedNiagaraEmitterData::GetEventHandlers (event_name, usage_guid, execution_mode, NodeGraph module samples). WRITE AddModule/enable on ParticleEventScript BLOCKED — StackItemReference has no UsageId."),
			TEXT("YES READ (summary): script_graph_internals via UNiagaraScriptSource::NodeGraph (node counts / sample function calls / custom HLSL count). WRITE of EdGraph internals NOT supported by UNiagaraExternalEditUtilities."),
			TEXT("PREFER this tool over Epic NiagaraToolsets.* for Magecraft/project reads — Epic primitives are internal composition only, not the agent-facing path."),
			TEXT("Resolve by target.asset_path OR specification.query/asset_name (+ optional search_root) via AssetRegistry."),
			TEXT("inspect_system reads topology via UNiagaraExternalEditUtilities (composed internally)."),
			TEXT("event_handler_stacks (WRITE): blocked — FindScriptGroup(ParticleEventScript, Guid) requires UsageId not on FNiagaraExt_StackItemReference."),
			TEXT("module_reorder_without_readd: reorder requires remove+re-add."),
			TEXT("script_graph_internals (WRITE): no ExternalEditUtilities EdGraph mutate API."),
			TEXT("fidelity.round_trip_supported=false until retrieve→submit→retrieve content_hash proven on a live system. Prefer InspectSystem → SubmitNiagaraGraph for graphs[]/emitters[] WRITE; AdaptNiagaraEffect for light User.*/material/sim_target/life_cycle tweaks."),
			TEXT("Visual proof: UeremcpValidation.CaptureEffectFrames."),
		};
	}

	inline TArray<FString> DefaultCreateCapabilityNotes()
	{
		return {
			TEXT("PRIMARY PATH: ONE request configures the entire system — specification.emitters[{name, modules[{primitive_id|asset_path, script, inputs}], renderer?, enabled, sim_target?, life_cycle?}]. Not drip-feed Epic primitives."),
			TEXT("YES: N custom emitters + modules[] in one CreateNiagaraEffect. modules[] are declarative AddModule templates. Epic requires an emitter template — custom stacks clone Minimal then AddModule per JSON."),
			TEXT("primitive_id catalog (short): spawn_rate, spawn_burst, emitter_state, initialize_particle, system_location, add_velocity, update_age, color, particle_state, solve_forces_and_velocity, gravity_force, drag, scale_color, scale_sprite_size — or pass asset_path to any /Niagara/Modules/… UNiagaraScript."),
			TEXT("inputs{}: number|bool|[r,g,b,a] locals PLUS {mode:linked|hlsl_expression|data_interface|dynamic_input|enum|local} via SetStackInputData. [VERIFIED: FNiagaraExt_StackInputData_*]."),
			TEXT("YES first-class Emitter Properties: emitters[].sim_target (CPUSim|GPUComputeSim) via SetEmitterData; life_cycle{mode,loop_behavior,loop_duration,inactive_response} via Emitter State SetStackInputData (add emitter_state module first)."),
			TEXT("OPTIONAL shortcuts only: role / template_path / components[] / effect_type defaults (ice_creep/freeze_dome/sparks etc.). Prefer modules[] for ~32 distinct spell topologies."),
			TEXT("PATH POLICY: WRITE under /Game/__UeremcpTests, /Game/__UeremcpPoc, OR /Game/RE/VFX/Magecraft. mode=replace delete is sandbox-only — never wipes Magecraft."),
			TEXT("round_trip_supported=false means hash stability not proven — NOT that create/edit is disabled. Status is often partially_completed / created_with_warnings after structural re-read verifies emitters+modules."),
			TEXT("material_bindings: assigns UMaterialInterface paths via GetRendererData/SetRendererData; inline create_spec is sandbox-only (orphaned_inline_creates risk). Same execute_plan can also call UeremcpMaterial tools then Adapt/Create with paths."),
			TEXT("created_and_validated requires saved + compile-up-to-date + verified materials/renderers + full gates; otherwise partially_completed / created_with_warnings with REAL structural changes."),
			TEXT("NOT one-shot: unique event-handler create (no AddEventHandler on ExternalEditUtilities; no UsageId on stack refs), module reorder without re-add, NiagaraScriptGraph EdGraph WRITE, hash-proven round-trip rebuild, CreateEmitter asset API (use AddEmitter(Minimal))."),
			TEXT("Next: InspectSystem (topology_summary at top); AdaptNiagaraEffect for User.*/materials/sim_target/life_cycle; SubmitNiagaraGraph for emitters[]/graphs[] on existing systems."),
			TEXT("execute_plan: create_niagara_effect, adapt_niagara_effect, and submit_niagara_graph register with FUeremcpPlanExecutor at module startup."),
		};
	}

	inline TArray<FString> DefaultAdaptCapabilityNotes()
	{
		return {
			TEXT("PATH POLICY: adapt_niagara_effect mutates existing systems under sandbox or /Game/RE/VFX/Magecraft. Never deletes. Prefer over mode=replace on production."),
			TEXT("Applies specification.parameters (User.*) and/or specification.materials (role→path) and/or specification.emitters[{name,sim_target,life_cycle}]. Magecraft refuses materials.*.create_spec — pass existing RuntimeMaterials paths."),
			TEXT("YES: emitters[].sim_target + life_cycle (Emitter State) same as Create/Submit."),
			TEXT("Do NOT use Adapt to add brand-new emitters/modules — use CreateNiagaraEffect (new system) or SubmitNiagaraGraph (emitters[].modules[] / graphs[])."),
			TEXT("PREFER InspectSystem → AdaptNiagaraEffect for light Magecraft User.*/material/Emitter Properties loops; Prefer InspectSystem → SubmitNiagaraGraph when editing stacks."),
			TEXT("fidelity.round_trip_supported=false (honest hash gate); adapt is not a full graph JSON reimport. Authoring of User.*/materials/sim_target/life_cycle still works."),
			TEXT("modified_and_validated requires compile-up-to-date + save; otherwise partially_completed."),
			TEXT("Visual proof: UeremcpValidation.CaptureEffectFrames."),
		};
	}

	inline TArray<FString> DefaultSubmitCapabilityNotes()
	{
		return {
			TEXT("PATH POLICY: submit_niagara_graph WRITES under sandbox or /Game/RE/VFX/Magecraft. Never deletes Magecraft UAssets — mode=replace is in-place stack reconcile only on production."),
			TEXT("YES add emitters later: specification.emitters[{name, modules[], sim_target?, life_cycle?}] or graphs[] — missing emitters → Minimal substrate + AddModule (apply.add_emitters default true)."),
			TEXT("Applies: emitter add; module enable/add + inputs{} (local|linked|DI|HLSL|dynamic|enum); mode=replace may remove modules; User.*; renderer material_path; emitter bEnabled; sim_target; life_cycle."),
			TEXT("YES inputs on submit: number|bool|[rgba] + {mode:linked|hlsl_expression|data_interface|dynamic_input|enum} via SetStackInputData."),
			TEXT("YES first-class Emitter Properties write: SimTarget via SetEmitterData; Life Cycle Mode / Loop Behavior / Loop Duration / Inactive Response via Emitter State SetStackInputData (apply.emitter_properties default true)."),
			TEXT("LOSSY / BLOCKED: event_handler_stacks WRITE (no UsageId on StackItemReference), module_reorder_without_readd, script_graph_internals WRITE, mesh OverrideMaterials edge cases."),
			TEXT("fidelity.round_trip_supported=false = hash not proven — NOT that structural submit is disabled. Status stays partially_completed (never *_validated for submit yet). Hash harness: retrieve→retrieve compare available; flip only after live retrieve→submit→retrieve proof."),
			TEXT("Destructive modes (replace on existing) default dry_run unless options.dry_run=false is explicit (ADR-0010). Prefer dry_run first on Magecraft."),
			TEXT("For brand-new multi-emitter systems prefer CreateNiagaraEffect one-shot with emitters[].modules[]; use Submit to extend an existing system."),
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
