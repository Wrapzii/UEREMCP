// UEREMCP — Blueprint domain toolset (WS-06 P0).
//
// P0: envelope echo/ping proving ADR-0002 + ADR-0003 on the Blueprint workstream.
// P1+: read_graph / submit_graph compose Epic BlueprintTools — do not rebuild primitives.

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
};
