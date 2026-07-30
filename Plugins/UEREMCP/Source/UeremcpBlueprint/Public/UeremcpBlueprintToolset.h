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
 * One UToolsetDefinition subclass hosting static AICallable UFUNCTIONs. Registration is
 * explicit via UToolsetRegistry::RegisterToolsetClass
 * [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:28]
 */
UCLASS(BlueprintType)
class UEREMCPBLUEPRINT_API UUeremcpBlueprintToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

	/**
	 * Liveness probe for the Blueprint domain module.
	 * @return Response envelope JSON (protocol_version, status, summary, metrics).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString Ping();

	/**
	 * Parses a request envelope and echoes understood fields inside a response envelope.
	 * Exercises the single FString parameter path (RB-03 q6) without touching assets.
	 *
	 * @param RequestJson Request envelope JSON (schemas/envelope/request.schema.json).
	 * @return Response envelope JSON. Malformed input yields status "rejected".
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString Echo(const FString& RequestJson);

	/**
	 * action=read_graph — one MCP call returns graph JSON (ADR-0004) + diagnostics.
	 *
	 * @param RequestJson Request envelope; target.asset_path required.
	 * @return Response envelope with diagnostics.graphs at response_detail complete.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString ReadGraph(const FString& RequestJson);

	/**
	 * action=submit_graph — validates unchanged replace submissions and revision conflicts.
	 *
	 * MCP toolset: UeremcpBlueprint.UeremcpBlueprintToolset
	 * MCP tool: SubmitGraph
	 * Argument: requestJson containing action=submit_graph and mode=replace.
	 * [VERIFIED-RUNTIME: user-unreal-mcp list_toolsets, 2026-07-30]
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Blueprints")
	static FString SubmitGraph(const FString& RequestJson);
};
