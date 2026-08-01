// UEREMCP — reference toolset proving ADR-0002.
//
// START HERE for agents:
//   GetStarted → ResolveIntent → domain semantic tools (prefer Ueremcp*).
//
// Agent-facing tools:
//   GetStarted / ResolveIntent / DescribeOperation — intent router bootstrap
//   Ping / Echo / ExecutePlan / GetJobResult / CancelJob — reference + jobs
//
// Reference pattern: UAgentSkillToolset
// [VERIFIED: $TR/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h]
//
// Tool descriptions for describe_toolset come from these UFUNCTION doc comments
// via UStructToJsonSchemaMetadata / GetToolTipText
// [VERIFIED: Engine/.../JsonSchemaGeneratorEditor.h:66-75]
// [VERIFIED: $TR/.../FunctionLibraryToolset.h:42-50].

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpReferenceToolset.generated.h"

/**
 * START HERE — UEREMCP reference + intent router (ADR-0002).
 *
 * Use when: first MCP call, discovering which semantic tool to invoke, or polling jobs.
 * Prefer ResolveIntent over list_toolsets for goal routing. Prefer Ueremcp* domain tools
 * over Epic primitives for create/modify/validate.
 * Do not use for: domain asset authoring (use Niagara/Material/Blueprint/Templates toolsets).
 *
 * One UToolsetDefinition subclass hosting static AICallable UFUNCTIONs. Registration is
 * explicit via UToolsetRegistry::RegisterToolsetClass
 * [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:28]
 * [VERIFIED: $TR/.../Private/ToolsetRegistry/ToolsetRegistrySubsystem.cpp:49]
 */
UCLASS(BlueprintType)
class UEREMCPCORE_API UUeremcpReferenceToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.2.0-intent-router"); }

	/**
	 * START HERE — bootstrap briefing for fresh agents.
	 *
	 * Use when: first call, "what tools do I use?", getting oriented on UEREMCP.
	 * Inputs: requestJson envelope with action=get_started (specification optional).
	 * Outputs: prefer_toolsets, next_call=ResolveIntent, envelope reminder.
	 * Do not use for: creating assets — call ResolveIntent then a domain tool.
	 * Next tool: ResolveIntent with your plain-text goal.
	 * Example: {"protocol_version":"1.0","action":"get_started","specification":{}}
	 *
	 * @param RequestJson Request envelope (schemas/domains/_shared/get_started.schema.json).
	 * @return Response envelope; no editor mutation.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString GetStarted(const FString& RequestJson);

	/**
	 * Resolve a plain-text intent into ordered UEREMCP semantic operations.
	 *
	 * Use when: you know the goal in English but not which tool/schema to call;
	 * "make a spell effect with a helix and show me what it looks like".
	 * Inputs: specification.intent (required), mode=recommend|execute_if_complete,
	 * optional context hints and expected_registry_hash.
	 * Outputs: confidence, ordered plan with fully-qualified live registry names,
	 * request_json examples, input_schema, missing_fields, safety, recovery.
	 * Do not use for: skipping verification — routing accuracy ≠ end-to-end success.
	 * Next tool: call each plan step; on low confidence answer clarification_questions.
	 * Example: {"protocol_version":"1.0","action":"resolve_intent","specification":{"intent":"make a fire projectile effect","mode":"recommend"}}
	 *
	 * Candidates come only from the live ToolsetRegistry
	 * [VERIFIED: UToolsetRegistry::GetAllToolsetJsonSchemas].
	 * Cannot emit a tool name absent from the live registry. Abstains on hash mismatch
	 * or low confidence. Mode execute_if_complete is not auto-executed in this build.
	 *
	 * @param RequestJson Request envelope (schemas/domains/_shared/resolve_intent.schema.json).
	 * @return Response envelope with result plan; status no_change_required or rejected.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString ResolveIntent(const FString& RequestJson);

	/**
	 * Describe one registry-verified operation: schema, example, safety notes.
	 *
	 * Use when: you already know the tool name and need the envelope/example.
	 * Inputs: specification.tool REQUIRED; specification.detail = index|slim|full
	 *   (default slim); specification.if_none_match = prior content_hash to skip
	 *   an unchanged body (returns no_change_required with empty payload).
	 * Outputs: content_hash always; slim carries required fields + example, not
	 *   the duplicated nested envelope mirror.
	 * Do not use for: choosing which tool — use ResolveIntent first.
	 * Next tool: call_tool with the returned request_json.
	 * Example: {"protocol_version":"1.0","action":"describe_operation","specification":{"tool":"create_landscape","detail":"slim"}}
	 *
	 * @param RequestJson Request envelope (schemas/domains/_shared/describe_operation.schema.json).
	 * @return Response envelope; rejected if the name is not in the live registry.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString DescribeOperation(const FString& RequestJson);

	/**
	 * Liveness probe. No parameters — isolates registration from schema questions.
	 *
	 * Use when: confirming UEREMCP MCP registration is alive before domain work.
	 * Do not use for: domain operations.
	 * Inputs: no arguments.
	 * Example: call Ping with no arguments.
	 * @return Response envelope JSON (protocol_version, status, summary, metrics).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Ping();

	/**
	 * Echoes a request envelope inside a response envelope (ADR-0003 probe).
	 *
	 * Use when: validating envelope shape without touching assets.
	 * Do not use for: production domain work.
	 * Inputs: requestJson envelope; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"echo","specification":{}}
	 *
	 * @param RequestJson Request envelope JSON string (schemas/envelope/request.schema.json).
	 * @return Response envelope JSON. Malformed input yields status "rejected".
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString Echo(const FString& RequestJson);

	/**
	 * Execute a complete multi-operation plan (action=execute_plan).
	 *
	 * Use when: you already have an explicit multi-step plan JSON.
	 * Do not use for: first-choice surface — prefer InstantiateTemplate or domain tools.
	 * Next tool: GetJobResult if partially_completed.
	 * Inputs: action=execute_plan; specification.operations is required.
	 * Example: {"protocol_version":"1.0","action":"execute_plan","options":{"dry_run":true},"specification":{"operations":[]}}
	 *
	 * @param RequestJson Request envelope JSON (schemas/batch/plan.schema.json as specification).
	 * @return Response envelope JSON with consolidated result + change manifest.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString ExecutePlan(const FString& RequestJson);

	/**
	 * Poll a long-running job by id (action=get_job_result).
	 *
	 * Use when: a prior call returned partially_completed with job.job_id.
	 * Do not use for: starting new domain work.
	 * Inputs: action=get_job_result; specification.job_id is required.
	 * Example: {"protocol_version":"1.0","action":"get_job_result","specification":{"job_id":"<prior job.job_id>"}}
	 *
	 * @param RequestJson Request envelope JSON with specification.job_id.
	 * @return Response envelope JSON for the current job snapshot.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString GetJobResult(const FString& RequestJson);

	/**
	 * Cooperatively cancel a running job (action=cancel_job).
	 *
	 * Use when: stop a UEREMCP long job by job_id.
	 * Do not use for: assuming MCP notifications/cancelled alone is enough.
	 * Inputs: action=cancel_job; specification.job_id is required.
	 * Example: {"protocol_version":"1.0","action":"cancel_job","specification":{"job_id":"<prior job.job_id>"}}
	 *
	 * @param RequestJson Request envelope JSON with specification.job_id.
	 * @return Response envelope JSON reflecting cancellation outcome.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP")
	static FString CancelJob(const FString& RequestJson);
};
