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
 * Per ADR-0002 one UToolsetDefinition per domain. Primitives from NiagaraToolsets.*
 * are internalised via execute_tool_script batching; agents see goal-level actions
 * such as inspect_system and create_niagara_effect.
 */
UCLASS()
class UEREMCPNIAGARA_API UUeremcpNiagaraToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.3.0-create-probe"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString Echo(const FString& RequestJson);

	/**
	 * Inspect a Niagara system into graph.schema.json + extensions.niagara.
	 *
	 * One MCP-facing call composes UNiagaraExternalEditUtilities GetSystemSummary /
	 * GetEmitterTopology (same surface as Epic NiagaraToolsets) and maps module stacks
	 * to NiagaraModuleStack / NiagaraEmitterGraph / NiagaraSystemGraph shapes.
	 * Probes only /Game/__UeremcpTests/. Event handler stacks remain lossy.
	 *
	 * @param RequestJson  Request with action inspect_system and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString InspectSystem(const FString& RequestJson);

	/**
	 * Goal-level Niagara effect creation (POC B slice).
	 *
	 * Duplicate-and-modify via UNiagaraExternalEditUtilities CreateNiagaraSystem,
	 * AddEmitter, AddUserVariable — same substrate as Epic NiagaraToolsets.
	 * Probes only /Game/__UeremcpTests/. Never reports *_validated until materials,
	 * renderer binding, and structural re-read are proven (POC B gaps).
	 *
	 * @param RequestJson  Request with action create_niagara_effect, target.asset_path,
	 *                     and specification per create_niagara_effect.schema.json.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString CreateNiagaraEffect(const FString& RequestJson);
};
