// UEREMCP — Material domain toolset (WS-08).

#include "UeremcpMaterialToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialCapabilityNotes.h"
#include "UeremcpMaterialInspect.h"
#include "UeremcpMaterialMasterBuilder.h"
#include "UeremcpMaterialService.h"
#include "UeremcpMaterialSubmit.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpProceduralTextureService.h"
#include "UeremcpSecurityDomainAdoption.h"

namespace
{
	bool IsDestructiveMode(const FString& Mode)
	{
		return Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase)
			|| Mode.Equals(TEXT("rebuild_from_specification"), ESearchCase::IgnoreCase)
			|| Mode.Equals(TEXT("delete"), ESearchCase::IgnoreCase);
	}

	int32 PredictedDeletedForTarget(const FUeremcpRequest& Request, bool& bOutTargetExists)
	{
		bOutTargetExists =
			UeremcpMaterialAssetLoad::TryLoadRegisteredAsset(Request.TargetAssetPath) != nullptr;
		return FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace(
			bOutTargetExists,
			IsDestructiveMode(Request.Mode));
	}
}

FString UUeremcpMaterialToolset::Echo(const FString& RequestJson)
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

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("no_change_required");
	Response.Summary = FString::Printf(
		TEXT("Material toolset echoed request for action '%s'. No editor state was touched."),
		*Request.Action);
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 0;

	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::CreateVfxMaterial(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("create_vfx_material"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("create_vfx_material tool received action '%s'. Use action 'create_vfx_material' or call Echo for protocol checks."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_vfx_material requires target.asset_path (material instance under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/)."));
	}

	bool bTargetExists = false;
	const int32 PredictedDeleted = PredictedDeletedForTarget(Request, bTargetExists);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bTargetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	const FUeremcpMaterialCreateResult CreateResult = UeremcpMaterialService::ExecuteCreateVfxMaterial(Request);

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = CreateResult.Status;
	Response.Summary = CreateResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = CreateResult.PrimaryAsset;
	Response.Revision = CreateResult.Revision;
	Response.CreatedAssets = CreateResult.CreatedAssets;
	Response.ModifiedAssets = CreateResult.ModifiedAssets;
	Response.ReusedAssets = CreateResult.ReusedAssets;
	Response.Dependencies = CreateResult.Dependencies;
	Response.InterpretationNotes = CreateResult.InterpretationNotes;
	Response.CapabilityNotes = CreateResult.CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = CreateResult.InternalOperations;
	Response.Metrics.AssetsAffected =
		CreateResult.CreatedAssets.Num() + CreateResult.ModifiedAssets.Num();

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::CreateProceduralTexture(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("create_procedural_texture"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("create_procedural_texture tool received action '%s'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_procedural_texture requires target.asset_path (Texture2D under /Game/__UeremcpTests/Textures/ or /Game/__UeremcpPoc/Textures/)."));
	}

	bool bTargetExists = false;
	const int32 PredictedDeleted = PredictedDeletedForTarget(Request, bTargetExists);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bTargetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	const FUeremcpProceduralTextureResult CreateResult =
		UeremcpProceduralTextureService::ExecuteFromEnvelope(Request);

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = CreateResult.Status;
	Response.Summary = CreateResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = CreateResult.PrimaryAsset;
	Response.CreatedAssets = CreateResult.CreatedAssets;
	Response.ReusedAssets = CreateResult.ReusedAssets;
	Response.CapabilityNotes = CreateResult.CapabilityNotes;
	Response.InterpretationNotes = CreateResult.InterpretationNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = CreateResult.InternalOperations;
	Response.Metrics.AssetsAffected = CreateResult.CreatedAssets.Num();

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::CreateMasterMaterial(const FString& RequestJson)
{
	// Exposes UeremcpMaterialMasterBuilder::EnsureMasterMaterial as a first-class
	// action. The capability already existed and was already parametric; it was
	// only reachable as a side effect of CreateVfxMaterial, which resolves its
	// master from purpose+element via the template library. In an empty project
	// that library is empty, so there was no way to author a material from
	// nothing. This is the material primitive floor.
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

	if (!Request.Action.Equals(TEXT("create_master_material"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("create_master_material tool received action '%s'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_master_material requires target.asset_path (UMaterial under /Game/__UeremcpTests/ or /Game/__UeremcpPoc/)."));
	}

	TArray<FString> Features;
	if (Request.Specification.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* FeatureArray = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("features"), FeatureArray) && FeatureArray)
		{
			for (const TSharedPtr<FJsonValue>& Value : *FeatureArray)
			{
				FString Token;
				if (Value.IsValid() && Value->TryGetString(Token) && !Token.IsEmpty())
				{
					Features.AddUnique(Token);
				}
			}
		}
	}

	// Refuse rather than invent. An empty feature set has no defensible default:
	// silently picking one is the substitution defect this action exists to avoid.
	if (Features.Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_master_material requires a non-empty specification.features array. "
				 "Known tokens: radial_falloff, animated_noise, fresnel, erosion, depth_fade, "
				 "distortion, panning_textures, flow_maps, flipbook_subuv, dynamic_color, dynamic_intensity."));
	}

	bool bTargetExists = false;
	const int32 PredictedDeleted = PredictedDeletedForTarget(Request, bTargetExists);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bTargetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would ensure master material at %s with %d feature(s)."),
			*Request.TargetAssetPath,
			Features.Num());
		Response.InterpretationNotes.Add(
			FString::Printf(TEXT("requested features: %s"), *FString::Join(Features, TEXT(", "))));
		Response.CapabilityNotes.Add(
			TEXT("Dry run does not compile the graph; wired_features is only known after a real run."));
		return bDispatchStarted
			? MutatingDispatch.Complete(Response)
			: FUeremcpEnvelope::SerializeResponse(Response);
	}

	FUeremcpMaterialMasterBuildRequest BuildRequest;
	BuildRequest.MasterPackagePath = Request.TargetAssetPath;
	BuildRequest.Features = Features;
	BuildRequest.bTrailPurpose =
		Request.Specification.IsValid() && Request.Specification->HasField(TEXT("trail"))
			? Request.Specification->GetBoolField(TEXT("trail"))
			: false;

	const FUeremcpMaterialMasterBuildResult BuildResult =
		UeremcpMaterialMasterBuilder::EnsureMasterMaterial(BuildRequest);

	if (!BuildResult.bSuccess)
	{
		Response.Status = TEXT("failed_validation");
		Response.Summary = BuildResult.Error.IsEmpty()
			? TEXT("Master material build failed.")
			: BuildResult.Error;
		Response.InterpretationNotes.Append(BuildResult.InterpretationNotes);
		Response.CapabilityNotes.Append(BuildResult.CapabilityNotes);
		return bDispatchStarted
			? MutatingDispatch.Complete(Response)
			: FUeremcpEnvelope::SerializeResponse(Response);
	}

	// Honesty: a token the builder could not wire is reported, never counted as
	// delivered. partially_completed is the correct status for that outcome.
	// Never *_validated: this action builds and saves the graph but performs no
	// structural re-read, so the strongest honest claim is created_with_warnings
	// (AGENTS.md rule 6).
	Response.Status = BuildResult.SkippedFeatures.Num() > 0
		? TEXT("partially_completed")
		: (BuildResult.bCreated ? TEXT("created_with_warnings") : TEXT("no_change_required"));
	if (BuildResult.bCreated)
	{
		Response.CapabilityNotes.Add(
			TEXT("Graph written and saved; NOT structurally re-read. Status is not "
				 "*_validated. Re-open the master or run a capture to confirm shading."));
	}
	Response.Summary = FString::Printf(
		TEXT("%s master material %s; wired %d of %d feature(s)."),
		BuildResult.bCreated ? TEXT("Created") : TEXT("Reused"),
		*BuildResult.MasterPackagePath,
		BuildResult.WiredFeatures.Num(),
		Features.Num());
	Response.PrimaryAsset = BuildResult.MasterPackagePath;

	FUeremcpAssetRef MasterRef;
	MasterRef.AssetPath = BuildResult.MasterPackagePath;
	MasterRef.AssetClass = TEXT("Material");
	if (BuildResult.bCreated)
	{
		Response.CreatedAssets.Add(MasterRef);
	}
	else
	{
		Response.ReusedAssets.Add(MasterRef);
	}

	Response.InterpretationNotes.Append(BuildResult.InterpretationNotes);
	Response.InterpretationNotes.Add(
		FString::Printf(TEXT("wired_features: %s"),
			*FString::Join(BuildResult.WiredFeatures, TEXT(", "))));
	if (BuildResult.SkippedFeatures.Num() > 0)
	{
		Response.InterpretationNotes.Add(
			FString::Printf(TEXT("skipped_features (requested but NOT wired): %s"),
				*FString::Join(BuildResult.SkippedFeatures, TEXT(", "))));
	}
	Response.CapabilityNotes.Append(BuildResult.CapabilityNotes);
	Response.Metrics.InternalOperations = BuildResult.InternalOperations;
	Response.Metrics.AssetsAffected = BuildResult.bCreated ? 1 : 0;

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::UpdateMaterialInstanceParameters(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("update_material_instance_parameters"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("update_material_instance_parameters tool received action '%s'."),
				*Request.Action));
	}

	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("update_material_instance_parameters requires target.asset_path (MaterialInstanceConstant)."));
	}

	bool bTargetExists = false;
	const int32 PredictedDeleted = PredictedDeletedForTarget(Request, bTargetExists);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bTargetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	const FUeremcpMaterialInstanceUpdateResult UpdateResult =
		UeremcpMaterialService::ExecuteUpdateMaterialInstanceParameters(Request);

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = UpdateResult.Status;
	Response.Summary = UpdateResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.PrimaryAsset = Request.TargetAssetPath;
	Response.ModifiedAssets = UpdateResult.ModifiedAssets;
	Response.InterpretationNotes = UpdateResult.InterpretationNotes;
	Response.CapabilityNotes = UpdateResult.CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = UpdateResult.InternalOperations;
	Response.Metrics.AssetsAffected = UpdateResult.ModifiedAssets.Num();

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	if (UpdateResult.ParameterChangesJson.IsValid())
	{
		Extra->SetObjectField(TEXT("parameter_changes"), UpdateResult.ParameterChangesJson);
	}
	TArray<TSharedPtr<FJsonValue>> ErrorValues;
	for (const FString& Error : UpdateResult.Errors)
	{
		ErrorValues.Add(MakeShared<FJsonValueString>(Error));
	}
	Extra->SetArrayField(TEXT("errors"), ErrorValues);
	Extra->SetBoolField(TEXT("saved"), UpdateResult.bSaved);
	Response.ExtraFields = Extra;

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::InspectMaterial(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("inspect_material"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("inspect_material tool received action '%s'."),
				*Request.Action));
	}

	FUeremcpMaterialInspectSpec Spec;
	FString SpecError;
	if (!FUeremcpMaterialInspect::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid inspect_material specification: %s"), *SpecError));
	}

	if (Spec.ResponseDetail.IsEmpty() && Request.ResponseDetail.IsEmpty())
	{
		Request.ResponseDetail = TEXT("complete");
	}

	FUeremcpMaterialInspectResult InspectResult;
	if (!FUeremcpMaterialInspect::Run(Request, Spec, InspectResult))
	{
		TSharedPtr<FJsonObject> NextArgs;
		if (InspectResult.Candidates.Num() > 0)
		{
			NextArgs = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Candidates;
			for (const FString& Candidate : InspectResult.Candidates)
			{
				Candidates.Add(MakeShared<FJsonValueString>(Candidate));
			}
			NextArgs->SetArrayField(TEXT("candidates"), Candidates);
		}
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			InspectResult.Error.IsEmpty() ? TEXT("inspect_material failed.") : InspectResult.Error,
			FString(),
			NextArgs);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = InspectResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = InspectResult.ResolvedAssetPath;
	Response.CapabilityNotes = UeremcpMaterialCapability::DefaultInspectCapabilityNotes();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = InspectResult.InternalOperations;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("primary_asset"), InspectResult.ResolvedAssetPath);
	Result->SetStringField(TEXT("asset_path"), InspectResult.ResolvedAssetPath);
	Result->SetStringField(TEXT("asset_class"), InspectResult.AssetClass);
	if (!InspectResult.ParentMaterialPath.IsEmpty())
	{
		Result->SetStringField(TEXT("parent_material"), InspectResult.ParentMaterialPath);
	}
	Result->SetNumberField(TEXT("expression_count"), InspectResult.ExpressionCount);
	Result->SetNumberField(TEXT("parameter_count"), InspectResult.ParameterCount);
	if (InspectResult.Parameters.IsValid())
	{
		Result->SetObjectField(TEXT("parameters"), InspectResult.Parameters);
	}
	if (InspectResult.Fidelity.IsValid())
	{
		Result->SetObjectField(TEXT("fidelity"), InspectResult.Fidelity);
	}
	Result->SetArrayField(TEXT("graphs"), InspectResult.Graphs);

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetObjectField(TEXT("result"), Result);

	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	if (InspectResult.ExecutionTrace.Num() > 0)
	{
		Diagnostics->SetArrayField(TEXT("execution_trace"), InspectResult.ExecutionTrace);
	}
	if (Diagnostics->Values.Num() > 0)
	{
		Extra->SetObjectField(TEXT("diagnostics"), Diagnostics);
	}

	TSharedPtr<FJsonObject> Validation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ChecksPerformed;
	for (const FString& Check : InspectResult.ChecksPerformed)
	{
		ChecksPerformed.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_performed"), ChecksPerformed);
	TArray<TSharedPtr<FJsonValue>> ChecksSkipped;
	for (const FString& Check : InspectResult.ChecksSkipped)
	{
		ChecksSkipped.Add(MakeShared<FJsonValueString>(Check));
	}
	Validation->SetArrayField(TEXT("checks_skipped"), ChecksSkipped);
	Extra->SetObjectField(TEXT("validation"), Validation);

	Response.ExtraFields = Extra;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpMaterialToolset::SubmitMaterialGraph(const FString& RequestJson)
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

	if (!Request.Action.Equals(TEXT("submit_material_graph"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("submit_material_graph tool received action '%s'."),
				*Request.Action));
	}

	FUeremcpMaterialSubmitSpec Spec;
	FString SpecError;
	if (!FUeremcpMaterialSubmit::ParseSpecification(Request.Specification, Spec, SpecError))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Invalid submit_material_graph specification: %s"), *SpecError));
	}

	bool bTargetExists = false;
	const int32 PredictedDeleted = PredictedDeletedForTarget(Request, bTargetExists);
	FUeremcpMutatingDispatch MutatingDispatch;
	const bool bDispatchStarted = !Request.bDryRun;
	if (bDispatchStarted)
	{
		FString BlockingResponse;
		if (!MutatingDispatch.TryBegin(
			RequestJson,
			bTargetExists,
			PredictedDeleted,
			false,
			BlockingResponse))
		{
			return BlockingResponse;
		}
		Request.bDryRun = MutatingDispatch.IsEffectiveDryRun();
	}

	FUeremcpMaterialSubmitResult SubmitResult;
	if (!FUeremcpMaterialSubmit::Run(Request, Spec, SubmitResult))
	{
		FUeremcpResponse Reject;
		Reject.RequestId = Request.RequestId;
		Reject.Status = TEXT("rejected");
		Reject.Summary = SubmitResult.Error.IsEmpty()
			? TEXT("submit_material_graph failed.")
			: SubmitResult.Error;
		Reject.UnderstoodAction = Request.Action;
		Reject.UnderstoodTarget = Request.TargetAssetPath;
		Reject.InterpretationNotes = SubmitResult.InterpretationNotes;
		Reject.CapabilityNotes = SubmitResult.CapabilityNotes;
		Reject.Metrics.McpRoundTrips = 1;
		return bDispatchStarted
			? MutatingDispatch.Complete(Reject)
			: FUeremcpEnvelope::SerializeResponse(Reject);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.Status = SubmitResult.Status;
	Response.Summary = SubmitResult.Summary;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.CreatedAssets = SubmitResult.CreatedAssets;
	Response.ModifiedAssets = SubmitResult.ModifiedAssets;
	Response.InterpretationNotes = SubmitResult.InterpretationNotes;
	Response.CapabilityNotes = SubmitResult.CapabilityNotes;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = SubmitResult.InternalOperations;
	Response.Metrics.AssetsAffected =
		SubmitResult.CreatedAssets.Num() + SubmitResult.ModifiedAssets.Num();

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	if (SubmitResult.ResultPayload.IsValid())
	{
		Extra->SetObjectField(TEXT("result"), SubmitResult.ResultPayload);
	}
	if (SubmitResult.Errors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Errors;
		for (const FString& Error : SubmitResult.Errors)
		{
			Errors.Add(MakeShared<FJsonValueString>(Error));
		}
		Extra->SetArrayField(TEXT("errors"), Errors);
	}
	Response.ExtraFields = Extra;

	return bDispatchStarted
		? MutatingDispatch.Complete(Response)
		: FUeremcpEnvelope::SerializeResponse(Response);
}
