#include "UeremcpSystemsToolset.h"

#include "UeremcpAudioService.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpNetworkingService.h"
#include "UeremcpSystemsHelpers.h"
#include "UeremcpWorldPartitionService.h"

namespace
{
	FString RejectAction(const FUeremcpRequest& Request, const TCHAR* Expected)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Received action '%s'; expected '%s'."),
				*Request.Action,
				Expected));
	}

	void FillReplicationResponse(
		const FUeremcpRequest& Request,
		const FUeremcpReplicationReport& Report,
		bool bDryRun,
		bool bApplyFixes,
		FUeremcpResponse& OutResponse)
	{
		OutResponse.RequestId = Request.RequestId;
		OutResponse.UnderstoodAction = Request.Action;
		OutResponse.UnderstoodTarget = Request.TargetAssetPath;
		OutResponse.PrimaryAsset = Request.TargetAssetPath;
		OutResponse.Revision = Report.ContentHash;
		OutResponse.Metrics.McpRoundTrips = 1;
		OutResponse.Metrics.InternalOperations = Report.VariableChecks.Num() + (Report.bPatternBDeclared ? 1 : 0);
		UeremcpSystems::AddCommonCapabilityNotes(OutResponse.CapabilityNotes);
		OutResponse.CapabilityNotes.Add(
			TEXT("Uses FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags [VERIFIED: BlueprintEditorUtils.h:1236]."));

		const bool bOk =
			(!Report.bPatternBDeclared || Report.bPatternBValid) && Report.MismatchCount == 0;

		if (bDryRun)
		{
			OutResponse.Status = TEXT("partially_completed");
			OutResponse.Summary = FString::Printf(
				TEXT("Dry-run validate_replication on '%s': %d match, %d mismatch%s."),
				*Request.TargetAssetPath,
				Report.MatchCount,
				Report.MismatchCount,
				Report.bPatternBDeclared
					? (Report.bPatternBValid ? TEXT("; Pattern B ok") : TEXT("; Pattern B FAILED"))
					: TEXT(""));
		}
		else if (bOk)
		{
			OutResponse.Status = bApplyFixes ? TEXT("modified_and_validated") : TEXT("no_change_required");
			if (!bApplyFixes && Report.MismatchCount == 0 && Report.MatchCount == 0 && Report.bPatternBValid)
			{
				OutResponse.Status = TEXT("no_change_required");
			}
			OutResponse.Summary = FString::Printf(
				TEXT("Replication validation passed for '%s' (%d checks)."),
				*Request.TargetAssetPath,
				Report.MatchCount);
		}
		else
		{
			OutResponse.Status = TEXT("failed_validation");
			OutResponse.Summary = FString::Printf(
				TEXT("Replication validation failed for '%s': %d mismatch(es)%s."),
				*Request.TargetAssetPath,
				Report.MismatchCount,
				Report.bPatternBDeclared && !Report.bPatternBValid
					? *FString::Printf(TEXT("; %s"), *Report.PatternBError)
					: TEXT(""));
		}

		OutResponse.ExtraFields = MakeShared<FJsonObject>();
		OutResponse.ExtraFields->SetBoolField(TEXT("pattern_b_valid"), Report.bPatternBValid);
		OutResponse.ExtraFields->SetNumberField(TEXT("match_count"), Report.MatchCount);
		OutResponse.ExtraFields->SetNumberField(TEXT("mismatch_count"), Report.MismatchCount);
		OutResponse.ExtraFields->SetStringField(TEXT("content_hash"), Report.ContentHash);
		TArray<TSharedPtr<FJsonValue>> Checks;
		for (const FUeremcpReplicationCheck& Check : Report.VariableChecks)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Check.VariableName);
			Row->SetStringField(TEXT("desired"), Check.DesiredMode);
			Row->SetStringField(TEXT("observed"), Check.ObservedMode);
			Row->SetBoolField(TEXT("match"), Check.bMatch);
			Checks.Add(MakeShared<FJsonValueObject>(Row));
		}
		OutResponse.ExtraFields->SetArrayField(TEXT("variable_checks"), Checks);
	}

	void FillWpResponse(
		const FUeremcpRequest& Request,
		const FUeremcpWorldPartitionReport& Report,
		const TCHAR* Status,
		const FString& Summary,
		FUeremcpResponse& OutResponse)
	{
		OutResponse.RequestId = Request.RequestId;
		OutResponse.Status = Status;
		OutResponse.Summary = Summary;
		OutResponse.UnderstoodAction = Request.Action;
		OutResponse.UnderstoodTarget = Report.WorldPath.IsEmpty() ? Request.TargetAssetPath : Report.WorldPath;
		OutResponse.PrimaryAsset = OutResponse.UnderstoodTarget;
		OutResponse.Revision = Report.ContentHash;
		OutResponse.Metrics.McpRoundTrips = 1;
		OutResponse.Metrics.InternalOperations = 1;
		UeremcpSystems::AddCommonCapabilityNotes(OutResponse.CapabilityNotes);
		OutResponse.ExtraFields = MakeShared<FJsonObject>();
		OutResponse.ExtraFields->SetBoolField(TEXT("is_partitioned"), Report.bIsPartitioned);
		OutResponse.ExtraFields->SetBoolField(TEXT("streaming_enabled"), Report.bStreamingEnabled);
		OutResponse.ExtraFields->SetBoolField(TEXT("streaming_enabled_in_editor"), Report.bStreamingEnabledInEditor);
		OutResponse.ExtraFields->SetBoolField(TEXT("can_generate_streaming"), Report.bCanGenerateStreaming);
		OutResponse.ExtraFields->SetNumberField(TEXT("actor_desc_container_count"), Report.ActorDescCount);
		OutResponse.ExtraFields->SetStringField(TEXT("editor_bounds"), Report.EditorBounds);
		OutResponse.ExtraFields->SetStringField(TEXT("runtime_bounds"), Report.RuntimeBounds);
		OutResponse.ExtraFields->SetStringField(TEXT("content_hash"), Report.ContentHash);
	}
}

FString UUeremcpSystemsToolset::CreateAudioCue(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), FString::Printf(TEXT("Malformed request: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (!Request.Action.Equals(TEXT("create_audio_cue"), ESearchCase::CaseSensitive))
	{
		return RejectAction(Request, TEXT("create_audio_cue"));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("create_audio_cue requires target.asset_path"));
	}

	FUeremcpAudioCuePlan Plan;
	FString SpecError;
	if (!FUeremcpAudioService::ParseCreateSpec(UeremcpSystems::SpecObject(Request), Request.TargetAssetPath, Plan, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, SpecError);
	}

	FUeremcpMutatingDispatch Dispatch;
	bool bDryRun = Request.bDryRun;
	if (!bDryRun)
	{
		FString Blocking;
		if (!Dispatch.TryBegin(RequestJson, false, 0, false, Blocking))
		{
			return Blocking;
		}
		bDryRun = Dispatch.IsEffectiveDryRun();
	}

	FUeremcpResponse Response;
	FString ExecError;
	if (!FUeremcpAudioService::ExecuteCreateAudioCue(Request, Plan, bDryRun, Response, ExecError))
	{
		if (!bDryRun && Dispatch.HoldsMutatorSlot())
		{
			FUeremcpResponse Fail;
			Fail.RequestId = Request.RequestId;
			Fail.Status = TEXT("failed_validation");
			Fail.Summary = ExecError;
			Fail.UnderstoodAction = Request.Action;
			return Dispatch.Complete(Fail);
		}
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, ExecError);
	}

	if (!bDryRun && Dispatch.HoldsMutatorSlot())
	{
		return Dispatch.Complete(Response);
	}
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpSystemsToolset::InspectAudio(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), FString::Printf(TEXT("Malformed request: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (!Request.Action.Equals(TEXT("inspect_audio"), ESearchCase::CaseSensitive))
	{
		return RejectAction(Request, TEXT("inspect_audio"));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("inspect_audio requires target.asset_path"));
	}

	FUeremcpAudioInspection Inspection;
	FString Error;
	if (!FUeremcpAudioService::Inspect(Request.TargetAssetPath, Inspection, Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = FString::Printf(
		TEXT("Inspected %s '%s' (%d wave refs)."),
		*Inspection.AssetClass,
		*Request.TargetAssetPath,
		Inspection.SoundWavePaths.Num());
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.Revision = Inspection.ContentHash;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 1;
	UeremcpSystems::AddCommonCapabilityNotes(Response.CapabilityNotes);
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetStringField(TEXT("asset_class"), Inspection.AssetClass);
	Response.ExtraFields->SetStringField(TEXT("attenuation_path"), Inspection.AttenuationPath);
	Response.ExtraFields->SetNumberField(TEXT("volume_multiplier"), Inspection.VolumeMultiplier);
	Response.ExtraFields->SetNumberField(TEXT("pitch_multiplier"), Inspection.PitchMultiplier);
	TArray<TSharedPtr<FJsonValue>> Waves;
	for (const FString& Wave : Inspection.SoundWavePaths)
	{
		Waves.Add(MakeShared<FJsonValueString>(Wave));
	}
	Response.ExtraFields->SetArrayField(TEXT("sound_waves"), Waves);
	Response.ExtraFields->SetStringField(TEXT("content_hash"), Inspection.ContentHash);
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpSystemsToolset::ValidateReplication(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), FString::Printf(TEXT("Malformed request: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (!Request.Action.Equals(TEXT("validate_replication"), ESearchCase::CaseSensitive))
	{
		return RejectAction(Request, TEXT("validate_replication"));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("validate_replication requires target.asset_path (Blueprint)"));
	}

	TArray<FUeremcpReplicationExpectation> Expectations;
	bool bRequirePatternB = false;
	bool bApplyFixes = false;
	FString Pattern, Authority, CastPath, SpecError;
	if (!FUeremcpNetworkingService::ParseValidateSpec(
		UeremcpSystems::SpecObject(Request),
		Expectations,
		bRequirePatternB,
		Pattern,
		Authority,
		CastPath,
		bApplyFixes,
		SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, SpecError);
	}

	const bool bMutating = bApplyFixes && !Request.bDryRun;
	FUeremcpMutatingDispatch Dispatch;
	bool bDryRun = Request.bDryRun;
	if (bMutating)
	{
		FString Blocking;
		if (!Dispatch.TryBegin(RequestJson, true, 0, false, Blocking))
		{
			return Blocking;
		}
		bDryRun = Dispatch.IsEffectiveDryRun();
	}

	FUeremcpReplicationReport Report;
	FString Error;
	if (!FUeremcpNetworkingService::ValidateBlueprintReplication(
		Request.TargetAssetPath,
		Expectations,
		bRequirePatternB,
		Pattern,
		Authority,
		CastPath,
		bApplyFixes,
		bDryRun,
		Report,
		Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpResponse Response;
	FillReplicationResponse(Request, Report, bDryRun, bApplyFixes, Response);
	if (bMutating && Dispatch.HoldsMutatorSlot())
	{
		return Dispatch.Complete(Response);
	}
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpSystemsToolset::InspectWorldPartition(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), FString::Printf(TEXT("Malformed request: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (!Request.Action.Equals(TEXT("inspect_world_partition"), ESearchCase::CaseSensitive))
	{
		return RejectAction(Request, TEXT("inspect_world_partition"));
	}

	FUeremcpWorldPartitionReport Report;
	FString Error;
	if (!FUeremcpWorldPartitionService::Inspect(Request.TargetAssetPath, Report, Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpResponse Response;
	FillWpResponse(
		Request,
		Report,
		TEXT("partially_completed"),
		FString::Printf(
			TEXT("World '%s': partitioned=%s streaming=%s containers=%d."),
			*Report.WorldPath,
			Report.bIsPartitioned ? TEXT("true") : TEXT("false"),
			Report.bStreamingEnabled ? TEXT("true") : TEXT("false"),
			Report.ActorDescCount),
		Response);
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpSystemsToolset::RepairWorldPartition(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), FString::Printf(TEXT("Malformed request: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (!Request.Action.Equals(TEXT("repair_world_partition"), ESearchCase::CaseSensitive))
	{
		return RejectAction(Request, TEXT("repair_world_partition"));
	}

	// Destructive default: dry_run true unless explicitly false.
	bool bDryRun = true;
	if (Request.bDryRun == false)
	{
		bDryRun = false;
	}
	// ParseRequest sets bDryRun false by default in envelope — enforce safety:
	// require options.dry_run explicitly false AND allow_destructive for mutate.
	TSharedPtr<FJsonObject> Root;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestJson);
		FJsonSerializer::Deserialize(Reader, Root);
	}
	bool bExplicitDryRunFalse = false;
	if (Root.IsValid())
	{
		const TSharedPtr<FJsonObject>* Options = nullptr;
		if (Root->TryGetObjectField(TEXT("options"), Options) && Options && (*Options).IsValid())
		{
			bool bOptDry = true;
			if ((*Options)->TryGetBoolField(TEXT("dry_run"), bOptDry))
			{
				bExplicitDryRunFalse = (bOptDry == false);
				bDryRun = bOptDry;
			}
		}
	}
	if (!bDryRun && !bExplicitDryRunFalse)
	{
		bDryRun = true;
	}
	if (!bDryRun && !Request.bAllowDestructive)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("repair_world_partition mutate requires options.dry_run=false and options.allow_destructive=true"));
	}

	bool bEnableStreaming = true;
	const TSharedPtr<FJsonObject> Spec = UeremcpSystems::SpecObject(Request);
	if (Spec.IsValid())
	{
		Spec->TryGetBoolField(TEXT("enable_streaming"), bEnableStreaming);
	}

	FUeremcpMutatingDispatch Dispatch;
	if (!bDryRun)
	{
		FString Blocking;
		if (!Dispatch.TryBegin(RequestJson, true, 0, false, Blocking))
		{
			return Blocking;
		}
		bDryRun = Dispatch.IsEffectiveDryRun();
	}

	FUeremcpWorldPartitionReport Before, After;
	FString Error;
	if (!FUeremcpWorldPartitionService::RepairOrCreate(
		Request.TargetAssetPath,
		bEnableStreaming,
		bDryRun,
		Before,
		After,
		Error))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FUeremcpResponse Response;
	if (bDryRun)
	{
		FillWpResponse(
			Request,
			After,
			TEXT("partially_completed"),
			FString::Printf(
				TEXT("Dry-run repair_world_partition for '%s' (enable_streaming=%s). No world mutated."),
				*Before.WorldPath,
				bEnableStreaming ? TEXT("true") : TEXT("false")),
			Response);
		Response.CapabilityNotes.Add(
			TEXT("Mutate path calls UWorldPartition::CreateOrRepairWorldPartition [VERIFIED: WorldPartition.h:167]."));
	}
	else
	{
		FillWpResponse(
			Request,
			After,
			TEXT("modified_and_validated"),
			FString::Printf(
				TEXT("Repaired World Partition on '%s'; streaming=%s."),
				*After.WorldPath,
				After.bStreamingEnabled ? TEXT("true") : TEXT("false")),
			Response);
		Response.Metrics.AssetsAffected = 1;
	}

	if (!bDryRun && Dispatch.HoldsMutatorSlot())
	{
		return Dispatch.Complete(Response);
	}
	return FUeremcpEnvelope::SerializeResponse(Response);
}
