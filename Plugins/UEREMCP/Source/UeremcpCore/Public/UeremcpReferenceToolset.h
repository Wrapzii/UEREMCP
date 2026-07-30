// UEREMCP — reference toolset proving ADR-0002.
//
// Two tools:
//   Ping : no args — registration / reachability
//   Echo : one FString request envelope — ADR-0003 shape + RB-03 q6 schema capture
//
// Reference pattern: UAgentSkillToolset
// [VERIFIED: $TR/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h]

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpReferenceToolset.generated.h"

/**
 * Reference UEREMCP toolset (ADR-0002).
 *
 * One UToolsetDefinition subclass hosting static AICallable UFUNCTIONs. Thin by
 * design — no domain work here. Registration is explicit via
 * UToolsetRegistry::RegisterToolsetClass
 * [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:28]
 * [VERIFIED: $TR/.../Private/ToolsetRegistry/ToolsetRegistrySubsystem.cpp:49]
 * (UAgentSkillToolset is registered the same way; subclasses do NOT self-register).
 */
UCLASS(BlueprintType)
class UEREMCPCORE_API UUeremcpReferenceToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	// Called on the CDO [VERIFIED: ToolsetDefinition.h].
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

	/**
	 * Liveness probe. No parameters — isolates registration from schema questions.
	 * @return Response envelope JSON (protocol_version, status, summary, metrics).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Ping();

	/**
	 * Echoes a request envelope inside a response envelope. Exercises the single
	 * FString parameter path that ADR-0003 depends on (RB-03 q6).
	 *
	 * @param RequestJson Request envelope JSON string (schemas/envelope/request.schema.json).
	 * @return Response envelope JSON. Malformed input yields status "rejected".
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Echo(const FString& RequestJson);
};
