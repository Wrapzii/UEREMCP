#include "UeremcpGameplayToolset.h"

#include "Dom/JsonValue.h"
#include "UeremcpEnvelope.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpSpellPlanner.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	Result.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}
}

FString UUeremcpGameplayToolset::CreateSpell(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(),
			FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion,
				*FUeremcpEnvelope::ProtocolVersion()));
	}
	if (Request.Action != TEXT("create_spell"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("CreateSpell received action '%s'; expected 'create_spell'."), *Request.Action));
	}
	if (Request.Mode != TEXT("create") && Request.Mode != TEXT("create_or_update"))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell currently supports mode create or create_or_update only."));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell requires target.asset_path naming the FREAbilityDef DataTable."));
	}

	// Shared ADR-0010 path gate; domains do not fork path policy.
	const FUeremcpPathValidationResult PathResult =
		FUeremcpPathPolicy::ValidateSoftPath(Request.TargetAssetPath, true);
	if (!PathResult.bAllowed)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, PathResult.Reason);
	}
	if (!Request.TargetAssetPath.StartsWith(TEXT("/Game/__UeremcpTests/")))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_spell preflight is restricted to /Game/__UeremcpTests/ until production DataTable policy is approved."));
	}

	FUeremcpSpellPlan Plan;
	FString PlanError;
	if (!FUeremcpSpellPlanner::BuildPlan(Request.Specification, Plan, PlanError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid create_spell specification: %s"), *PlanError));
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = FString::Printf(
		TEXT("Prepared and statically validated RE spell row '%s' for '%s'; guarded DataTable mutation, save, and re-read are not implemented in this preflight slice."),
		*Plan.RowName,
		*Request.TargetAssetPath);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.InterpretationNotes.Add(FString::Printf(
		TEXT("row_name=%s; AbilityId=%s; deterministic target retained without auto-suffixing"),
		*Plan.RowName,
		*Plan.RowName));
	Response.PrimaryAsset = Request.TargetAssetPath;
	for (const FString& DependencyPath : Plan.DependencyAssetPaths)
	{
		FUeremcpAssetRef Dependency;
		Dependency.AssetPath = DependencyPath;
		Dependency.Role = TEXT("spell_presentation");
		Response.Dependencies.Add(MoveTemp(Dependency));
	}
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 1;
	Response.CapabilityNotes = {
		TEXT("Specification-to-FREAbilityDef planning and Pattern B static checks completed."),
		TEXT("DataTable upsert, save, and re-read skipped: guarded mutation is not implemented in this preflight slice."),
		TEXT("No gameplay-tag INI mutation is performed; RE Element, EffectTag, and ImpactStatus fields are used."),
		TEXT("Multi-client replication proof remains WS-11/RB-14; this response does not claim it."),
	};

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	Validation->SetNullField(TEXT("compiled"));
	Validation->SetNullField(TEXT("saved"));
	Validation->SetBoolField(TEXT("structurally_valid"), true);
	Validation->SetNullField(TEXT("dependencies_resolved"));
	Validation->SetNullField(TEXT("reread_after_write"));
	Validation->SetNullField(TEXT("runtime_smoke_test"));
	Validation->SetNullField(TEXT("editor_validation_run"));
	Validation->SetArrayField(TEXT("checks_performed"), StringValues(Plan.StaticChecks));
	Validation->SetArrayField(TEXT("checks_skipped"), StringValues({
		TEXT("datatable_upsert: ADR-0010 mutator queue not implemented"),
		TEXT("save: no mutation performed"),
		TEXT("reread_after_write: no mutation performed"),
		TEXT("dependency_resolution: requires editor asset load"),
		TEXT("runtime_smoke_test: WS-11/RB-14"),
	}));

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("validation"), Validation);

	return FUeremcpEnvelope::SerializeResponse(Response);
}
