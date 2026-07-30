// UEREMCP — reference toolset proving ADR-0002.
//
// Agent-facing tools:
//   Ping : no args — registration / reachability
//   Echo : one FString request envelope — ADR-0003 shape + RB-03 q6 schema capture
//   ExecutePlan : one complete plan request — ADR-0008 execution path
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

	/**
	 * Execute a complete multi-operation plan (action=execute_plan).
	 * Delegates to FUeremcpPlanActions — no additional parsing.
	 *
	 * @param RequestJson Request envelope JSON (schemas/batch/plan.schema.json
	 *        as specification).
	 * @return Response envelope JSON with consolidated result + change manifest.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString ExecutePlan(const FString& RequestJson);

	/**
	 * Poll a long-running job by id (action=get_job_result).
	 * Delegates to FUeremcpJobActions — no additional parsing.
	 *
	 * @param RequestJson Request envelope JSON with specification.job_id.
	 * @return Response envelope JSON for the current job snapshot.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString GetJobResult(const FString& RequestJson);

	/**
	 * Cooperatively cancel a running job (action=cancel_job).
	 * Delegates to FUeremcpJobActions — no additional parsing.
	 *
	 * @param RequestJson Request envelope JSON with specification.job_id.
	 * @return Response envelope JSON reflecting cancellation outcome.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString CancelJob(const FString& RequestJson);
};
