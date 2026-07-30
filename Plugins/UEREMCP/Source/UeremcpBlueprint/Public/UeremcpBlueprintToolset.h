// UEREMCP — Blueprint domain toolset (WS-06).
//
// P0: envelope echo/ping proving ADR-0002 + ADR-0003 on the Blueprint workstream.
// P1: read_graph composes Epic BlueprintTools — does not rebuild pin primitives.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpBlueprintToolset.generated.h"

/**
 * Blueprint-domain UEREMCP toolset (ADR-0002).
 *
 * Prefer ReadGraph / SubmitGraph over Epic BlueprintTools pin/node loops.
 * Use ResolveIntent if unsure which tool. Registration via
 * UToolsetRegistry::RegisterToolsetClass
 * [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:28]
 */
UCLASS(BlueprintType)
class UEREMCPBLUEPRINT_API UUeremcpBlueprintToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.1.1-intent-vocab"); }

	/**
	 * Liveness probe for the Blueprint domain module.
	 * Use when: confirming Blueprint toolset registration.
	 * Do not use for: reading or writing graphs.
	 * @return Response envelope JSON (protocol_version, status, summary, metrics).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString Ping();

	/**
	 * Parses a request envelope and echoes understood fields inside a response envelope.
	 * Use when: validating envelope shape without touching assets.
	 * Do not use for: production graph work.
	 *
	 * @param RequestJson Request envelope JSON (schemas/envelope/request.schema.json).
	 * @return Response envelope JSON. Malformed input yields status "rejected".
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString Echo(const FString& RequestJson);

	/**
	 * Read a complete Blueprint graph (ADR-0004) + diagnostics in one call.
	 *
	 * Use when: inspect EventGraph / function graphs; capture revision before edits;
	 * "add logic" discovery step.
	 * Inputs: action=read_graph, target.asset_path; options.response_detail=complete.
	 * Outputs: graph JSON + diagnostics + revision.
	 * Do not use for: pin-by-pin BlueprintTools loops.
	 * Next tool: SubmitGraph with expected_revision after editing the JSON.
	 *
	 * @param RequestJson Request envelope; target.asset_path required.
	 * @return Response envelope with diagnostics.graphs at response_detail complete.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString ReadGraph(const FString& RequestJson);

	/**
	 * Replace a Blueprint graph from complete JSON (action=submit_graph, mode=replace).
	 *
	 * Use when: write blueprint logic (e.g. when the spell hits) from complete graph state.
	 * Inputs: action=submit_graph, mode=replace, target.asset_path, expected_revision,
	 * specification.graph; prefer options.dry_run first; idempotency_key recommended.
	 * Outputs: modified_and_validated only after compile/save/re-read evidence.
	 * Do not use for: create_node/connect_pins chains.
	 * Next tool: ReadGraph to verify; on revision conflict re-read then resubmit.
	 *
	 * MCP toolset: UeremcpBlueprint.UeremcpBlueprintToolset
	 * [VERIFIED-RUNTIME: user-unreal-mcp list_toolsets, 2026-07-30]
	 * [VERIFIED: UeremcpBlueprintEpicBridge.cpp:194-323]
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString SubmitGraph(const FString& RequestJson);
};
