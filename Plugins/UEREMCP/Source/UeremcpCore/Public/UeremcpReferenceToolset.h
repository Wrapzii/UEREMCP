// UEREMCP — reference toolset.
//
// SCAFFOLD — NOT YET COMPILED. See Plugins/UEREMCP/README.md.
//
// Purpose: the smallest thing that proves ADR-0002. Two tools —
//   ping : takes nothing, returns a response envelope
//   echo : takes a request envelope, returns it inside a response envelope
//
// RB-03 exists to make this build, register, and be callable from an MCP client.
// Question 6 of that brief is the one that matters: what JSON Schema does the
// registry generate for a single FString parameter? If the agent only sees
// "a string", ADR-0003's envelope loses its discoverability and we need a
// USTRUCT-based or hybrid signature instead. Find out here, before fifteen
// workstreams build on the assumption.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpReferenceToolset.generated.h"

/**
 * Reference implementation of a UEREMCP toolset.
 *
 * Per ADR-0002 the agent-facing surface is one UToolsetDefinition subclass per
 * DOMAIN (not per operation), and every tool is a static UFUNCTION marked
 * meta=(AICallable) taking one JSON request envelope and returning one JSON
 * response envelope.
 *
 * Toolsets stay thin. Real work belongs in domain services that include nothing
 * from ToolsetRegistry/ or ModelContextProtocol/, so the engine coupling lives in
 * one replaceable layer (ADR-0001 churn mitigation, ADR-0002 rule 4).
 *
 * Reference for this pattern in-engine: UAgentSkillToolset
 * [VERIFIED: $TR/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h]
 */
UCLASS()
class UEREMCPCORE_API UUeremcpReferenceToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	//~ Begin UToolsetDefinition
	// NOTE: the base declares this virtual but the header comments that it is called
	// on the class default object [VERIFIED: ToolsetDefinition.h]. Keep it pure.
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }
	//~ End UToolsetDefinition

	/**
	 * Liveness and version probe. Takes no arguments so it isolates the registration
	 * question from the schema question.
	 *
	 * @return A response envelope conforming to schemas/envelope/response.schema.json.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Ping();

	/**
	 * Parses a request envelope, validates it, and returns it inside a response
	 * envelope under `understood`. Exercises the full ADR-0003 contract without
	 * touching a single asset — so a failure here is unambiguously a protocol or
	 * registration problem, not a domain problem.
	 *
	 * @param RequestJson  A request envelope (schemas/envelope/request.schema.json).
	 * @return A response envelope. A malformed request yields status "rejected" with
	 *         an explanatory summary — never a thrown exception and never a crash.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Echo(const FString& RequestJson);
};
