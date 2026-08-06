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
	 * Inputs: no arguments.
	 * Example: call Ping with no arguments.
	 * @return Response envelope JSON (protocol_version, status, summary, metrics).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString Ping();

	/**
	 * Parses a request envelope and echoes understood fields inside a response envelope.
	 * Use when: validating envelope shape without touching assets.
	 * Do not use for: production graph work.
	 * Inputs: requestJson envelope; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"echo","specification":{}}
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
	 * Specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"read_graph","target":{"asset_path":"/Game/BP_Mage","graph_id":"EventGraph"},"specification":{}}
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
	 * Example: {"protocol_version":"1.0","action":"submit_graph","mode":"replace","target":{"asset_path":"/Game/BP_Mage","graph_id":"EventGraph"},"expected_revision":"<read_graph revision>","options":{"dry_run":true},"specification":{"graph":{"schema_version":"1.0","nodes":[],"edges":[]}}}
	 *
	 * MCP toolset: UeremcpBlueprint.UeremcpBlueprintToolset
	 * [VERIFIED-RUNTIME: user-unreal-mcp list_toolsets, 2026-07-30]
	 * [VERIFIED: UeremcpBlueprintEpicBridge.cpp:194-323]
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString SubmitGraph(const FString& RequestJson);

	/**
	 * List node types placeable in a graph, with their exact pin names, in one call.
	 *
	 * Use when: you are about to author graph JSON for SubmitGraph and need the real
	 * node_class and pin names rather than guessing them; "what node does X" discovery.
	 * Inputs: action=describe_node_catalog, target.asset_path (+ target.graph_id, default
	 * EventGraph); specification.search / node_classes / categories / include_pins /
	 * max_results — all optional.
	 * Outputs: diagnostics.node_catalog.entries[] with node_class, menu_name, category,
	 * tooltip and pins[] (name, direction, category, container_type, default_value).
	 * Do not use for: placing nodes — that is SubmitGraph; or for reading an existing
	 * graph's contents — that is ReadGraph.
	 * Next tool: SubmitGraph, using entries[].node_class and pins[].name verbatim.
	 * Example: {"protocol_version":"1.0","action":"describe_node_catalog","target":{"asset_path":"/Game/BP_Mage","graph_id":"EventGraph"},"specification":{"search":"print string","max_results":25}}
	 *
	 * Composes Epic's FBlueprintActionDatabase — the same source the editor palette
	 * uses — so the catalog cannot drift from what the editor will place.
	 * [VERIFIED: Editor/BlueprintGraph/Public/BlueprintActionDatabase.h:66,94]
	 * [VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:149,152,178,235]
	 *
	 * @param RequestJson Request envelope; target.asset_path required.
	 * @return Response envelope with diagnostics.node_catalog; status no_change_required.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString DescribeNodeCatalog(const FString& RequestJson);
};
