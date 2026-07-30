// UEREMCP — Material domain toolset (WS-08).

#include "UeremcpMaterialToolset.h"

#include "UeremcpEnvelope.h"
#include "UeremcpMaterialAssetLoad.h"
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
