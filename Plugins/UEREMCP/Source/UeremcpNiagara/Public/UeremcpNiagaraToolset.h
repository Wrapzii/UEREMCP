// UEREMCP — Niagara domain toolset (WS-07).
//
// Wave 2 scaffold: envelope echo + inspect_system stub with honest capability_notes.
// Composes Epic NiagaraToolsets internally — does not re-expose primitives (RB-07).

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpNiagaraToolset.generated.h"

/**
 * Agent-facing Niagara operations for UEREMCP.
 *
 * START HERE for particle/VFX goals: prefer CreateNiagaraEffect / InspectSystem over
 * Epic NiagaraToolsets primitives. Use ResolveIntent first if unsure which tool.
 *
 * Per ADR-0002 one UToolsetDefinition per domain. Primitives from NiagaraToolsets.*
 * are internalised; agents see goal-level actions.
 */
UCLASS()
class UEREMCPNIAGARA_API UUeremcpNiagaraToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.9.6-intent-vocab"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * Use when: validating the Niagara module envelope path.
	 * Do not use for: creating or inspecting systems.
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString Echo(const FString& RequestJson);

	/**
	 * Inspect a Niagara system into graph.schema.json + extensions.niagara.
	 *
	 * Use when: read emitters/renderers/user params; verify a system after create.
	 * Inputs: action=inspect_system, target.asset_path required.
	 * Outputs: structured topology + diagnostics; some stacks intentionally lossy.
	 * Do not use for: creating effects — use CreateNiagaraEffect.
	 * Next tool: CreateNiagaraEffect to author; CaptureEffectFrames for visual proof.
	 *
	 * @param RequestJson  Request with action inspect_system and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString InspectSystem(const FString& RequestJson);

	/**
	 * Goal-level Niagara effect creation (spell/projectile/beam/explosion VFX).
	 *
	 * Use when: create a particle/Niagara effect, helix/ribbon/projectile, spell VFX.
	 * Inputs: action=create_niagara_effect, target.asset_path, specification.effect_type;
	 * prefer options.dry_run first; idempotency_key recommended.
	 * Outputs: honest statuses — may be partially_completed until visual gates close.
	 * Do not use for: Epic NiagaraToolsets module primitives; material-only edits.
	 * Next tool: InspectSystem to verify; CaptureEffectFrames to show what it looks like.
	 *
	 * [VERIFIED: composes UNiagaraExternalEditUtilities — same substrate as NiagaraToolsets]
	 *
	 * @param RequestJson  Request with action create_niagara_effect, target.asset_path,
	 *                     and specification per create_niagara_effect.schema.json.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString CreateNiagaraEffect(const FString& RequestJson);
};
