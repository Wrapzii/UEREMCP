// UEREMCP — Niagara domain toolset (WS-07).
//
// Goal-level inspect / create / adapt over UNiagaraExternalEditUtilities.
// Composes Epic NiagaraToolsets internally — does not re-expose primitives (RB-07).

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpNiagaraToolset.generated.h"

/**
 * Agent-facing Niagara operations for UEREMCP.
 *
 * START HERE for particle/VFX goals: prefer InspectSystem / CreateNiagaraEffect /
 * AdaptNiagaraEffect over Epic NiagaraToolsets primitives. Use ResolveIntent first
 * if unsure which tool.
 *
 * Per ADR-0002 one UToolsetDefinition per domain. Primitives from NiagaraToolsets.*
 * are internalised; agents see goal-level actions.
 */
UCLASS()
class UEREMCPNIAGARA_API UUeremcpNiagaraToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.10.0-custom-module-stacks"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * Use when: validating the Niagara module envelope path.
	 * Do not use for: creating or inspecting systems.
	 * Inputs: requestJson envelope; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"echo","specification":{}}
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString Echo(const FString& RequestJson);

	/**
	 * One-shot inspect: resolve a Niagara system and return full graph JSON.
	 *
	 * Use when: find/read emitters, modules, timings, User.*, renderers for any /Game path
	 *   (including Magecraft) — prefer over Epic NiagaraToolsets discovery chains.
	 * Inputs: action=inspect_system; target.asset_path OR specification.query/asset_name
	 *   (e.g. NS_nature_xl_cast under search_root=/Game/RE/VFX/Magecraft).
	 * Outputs: result.topology_summary (emitters→modules→renderers) FIRST, then graphs[] when complete.
	 * Do not use for: mutation — use SubmitNiagaraGraph / AdaptNiagaraEffect / CreateNiagaraEffect.
	 * Next tool: SubmitNiagaraGraph after editing; AdaptNiagaraEffect for User params/materials;
	 *   CaptureEffectFrames for visual proof.
	 * response_detail=summary|complete (default complete). summary = topology_summary + counts; complete adds graphs[].
	 * Example: {"protocol_version":"1.0","action":"inspect_system","target":{"asset_path":"/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"},"specification":{}}
	 * Example query: {"protocol_version":"1.0","action":"inspect_system","specification":{"query":"NS_nature_xl_cast","search_root":"/Game/RE/VFX/Magecraft"}}
	 *
	 * @param RequestJson  Request with action inspect_system.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString InspectSystem(const FString& RequestJson);

	/**
	 * ONE-SHOT Niagara system authoring — entire system in one request.
	 *
	 * Use when: create a NEW particle system under sandbox OR Magecraft.
	 * PRIMARY Inputs: specification.emitters[{name, modules[{primitive_id|asset_path, script, inputs}],
	 *   renderer?, enabled}] — LLM-defined stacks on Minimal substrate (AddEmitter+AddModule).
	 * Optional: role/template_path/components[] shortcuts (NOT required).
	 * Outputs: partially_completed / created_with_warnings after structural re-read.
	 *   round_trip_supported=false is a hash gate — authoring still adds emitters+modules.
	 * Do not use for: wiping Magecraft; custom HLSL / script-graph authorship (lossy).
	 * Next tool: InspectSystem (topology_summary); CaptureEffectFrames for visual proof.
	 * Example custom (no preset roles): see schemas/domains/niagara/fixtures/create_custom_three_emitter_stack.json
	 *
	 * [VERIFIED: UNiagaraExternalEditUtilities::CreateNiagaraSystem / AddEmitter / AddModule /
	 *  SetModuleEnabled / SetStackInputData / AddRenderer — NiagaraExternalSystemEditorUtilities.h]
	 *
	 * @param RequestJson  Request with action create_niagara_effect, target.asset_path,
	 *                     and specification per create_niagara_effect.schema.json.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString CreateNiagaraEffect(const FString& RequestJson);

	/**
	 * In-place adapt of an existing Niagara system (User.* + material bindings).
	 *
	 * Use when: tweak Magecraft/sandbox systems without delete/recreate.
	 * Inputs: action=adapt_niagara_effect, target.asset_path under mutate roots,
	 *   specification.parameters and/or specification.materials.
	 * Outputs: modified_and_validated / partially_completed with honest checks.
	 * Do not use for: full graph JSON rebuild — use SubmitNiagaraGraph;
	 *   Magecraft materials.*.create_spec (sandbox-only).
	 * Next tool: InspectSystem to verify; CaptureEffectFrames for visual proof.
	 * Example: {"protocol_version":"1.0","action":"adapt_niagara_effect","target":{"asset_path":"/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"},"options":{"dry_run":true},"specification":{"parameters":{"include_adaptation":true,"dirtiness":0.3}}}
	 *
	 * @param RequestJson  Request with action adapt_niagara_effect.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString AdaptNiagaraEffect(const FString& RequestJson);

	/**
	 * Apply edited inspect_system graphs[] to an existing Niagara system.
	 *
	 * Use when: agent edited graphs[] OR wants declarative emitters[].modules[] on an existing system.
	 * Inputs: action=submit_niagara_graph; specification.graphs and/or specification.emitters[{name,modules[]}].
	 *   Missing emitters → Minimal substrate + AddModule. mode=replace may remove modules;
	 *   Magecraft never deletes the UAsset. Destructive modes default dry_run (ADR-0010).
	 * Outputs: partially_completed with planned/applied changes; round_trip_supported stays false.
	 * Do not use for: creating a brand-new system (CreateNiagaraEffect); User-param/material-only
	 *   tweaks (AdaptNiagaraEffect is lighter).
	 * Next tool: InspectSystem to re-read; CaptureEffectFrames for visual proof.
	 * Example emitters: {"protocol_version":"1.0","action":"submit_niagara_graph","target":{"asset_path":"/Game/__UeremcpTests/NS_X"},"options":{"dry_run":true},"specification":{"emitters":[{"name":"CustomSpark","modules":[{"primitive_id":"spawn_rate","inputs":{"SpawnRate":8}}]}]}}
	 *
	 * @param RequestJson  Request with action submit_niagara_graph.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString SubmitNiagaraGraph(const FString& RequestJson);

	/**
	 * The authoring vocabulary for CreateNiagaraEffect / SubmitNiagaraGraph, in one call.
	 *
	 * This returns UEREMCP's OWN accepted vocabulary — not a browse of Niagara's module
	 * library. The primitive_id aliases, renderer hints, script usages and stack-input
	 * modes below exist only in this plugin's resolver, so nothing in Epic's toolsets
	 * can tell an agent what they are.
	 *
	 * Use when: before authoring emitters[].modules[] — returns every primitive_id
	 * ResolveModuleAssetPath accepts, the module asset each resolves to, its default
	 * script usage, plus the renderer hints, script usages and input modes this plugin's
	 * submit path understands.
	 * Inputs: action=describe_niagara_catalog; specification.search (substring over
	 * primitive_id and asset path), specification.verify_assets (default true — loads each
	 * module script and reports whether it actually resolves).
	 * Outputs: diagnostics.niagara_catalog with modules[], renderer_hints[],
	 * script_usages[], input_modes[].
	 * Do not use for: browsing Niagara's full module library — Epic
	 * NiagaraToolset_Assets.FindNiagaraScripts searches every UNiagaraScript from asset
	 * registry tags and is strictly better for that; modules[].asset_path here is a
	 * curated shortlist, and any /Niagara/Modules/… path it does not list is still
	 * accepted via a module's asset_path. Also not for reading an existing system's
	 * stacks — that is InspectSystem.
	 * Next tool: CreateNiagaraEffect or SubmitNiagaraGraph using modules[].primitive_ids[0].
	 * Example: {"protocol_version":"1.0","action":"describe_niagara_catalog","specification":{"search":"spawn"}}
	 *
	 * Deliberately NOT returned: per-module input names. Epic
	 * NiagaraToolset_System.GetModuleSchemaFromAsset already returns a module asset's
	 * input schema standalone, with no system context, and duplicating it here would be
	 * the defect AGENTS.md rule 2 warns about. Call that tool, or add the module and use
	 * InspectSystem.
	 * [VERIFIED: docs/audit/raw/schemas/NiagaraToolsets.NiagaraToolset_System.json — GetModuleSchemaFromAsset]
	 * [VERIFIED: docs/audit/raw/schemas/NiagaraToolsets.NiagaraToolset_Assets.json — FindNiagaraScripts]
	 *
	 * @param RequestJson  Request with action describe_niagara_catalog; no target required.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString DescribeNiagaraCatalog(const FString& RequestJson);
};
