// UEREMCP — Material domain toolset (WS-08).

#include "UeremcpMaterialToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialMasterBuilder.h"
#include "UeremcpMaterialService.h"
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
