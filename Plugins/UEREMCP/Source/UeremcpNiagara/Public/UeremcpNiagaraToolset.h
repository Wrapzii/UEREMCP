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

	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-wave2-scaffold"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString Echo(const FString& RequestJson);

	/**
	 * Inspect a Niagara system into graph.schema.json shapes (stub).
	 *
	 * Wave 2 first slice: validates the envelope, echoes understood fields, and returns
	 * honest capability_notes / fidelity lossy areas. Does not yet call Epic
	 * NiagaraToolsets GetSystemSummary / GetEmitterTopology.
	 *
	 * @param RequestJson  Request with action inspect_system and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Niagara")
	static FString InspectSystem(const FString& RequestJson);
};
