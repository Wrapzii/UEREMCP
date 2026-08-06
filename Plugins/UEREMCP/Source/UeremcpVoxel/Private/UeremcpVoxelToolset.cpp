#include "UeremcpVoxelToolset.h"

#include "UeremcpVoxelService.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMutatingDispatch.h"
#include "VoxelWorld.h"
#include "Templates/Function.h"

namespace
{
	FString DispatchVoxelAction(
		const FString& RequestJson,
		const FString& ExpectedAction,
		TFunctionRef<FUeremcpVoxelOpResult(AVoxelWorld*, const TSharedPtr<FJsonObject>&, bool)> Fn,
		bool bNeedsVoxelWorld)
	{
		FUeremcpMutatingDispatch Dispatch;
		FString Blocked;
		if (!Dispatch.TryBegin(RequestJson, true, 0, false, Blocked))
		{
			return Blocked;
		}

		const FUeremcpRequest& Request = Dispatch.GetRequest();
		if (!Request.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(TEXT("Received action '%s'; expected '%s'."), *Request.Action, *ExpectedAction));
		}

		AVoxelWorld* World = nullptr;
		if (bNeedsVoxelWorld)
		{
			FString FindError;
			World = FUeremcpVoxelService::FindVoxelWorld(Request.TargetActorLabel, FindError);
			if (!World)
			{
				return FUeremcpEnvelope::MakeRejection(
					Request.RequestId, FindError, TEXT("TARGET_NOT_FOUND"), nullptr);
			}
		}

		const FUeremcpVoxelOpResult Result = Fn(World, Request.Specification, Dispatch.IsEffectiveDryRun());

		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.UnderstoodTarget = Request.TargetActorLabel.IsEmpty()
			? (World ? World->GetActorLabel() : Request.TargetAssetPath)
			: Request.TargetActorLabel;
		Response.PrimaryAsset = Response.UnderstoodTarget;
		Response.Status = Result.Status;
		Response.Summary = Result.Summary;
		Response.ErrorCode = Result.ErrorCode;
		Response.CapabilityNotes = Result.CapabilityNotes;
		Response.NextArgs = Result.NextArgs;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = Result.InternalOperations;
		Response.ExtraFields = Result.Extra.IsValid() ? Result.Extra : MakeShared<FJsonObject>();
		for (const FString& W : Result.Warnings)
		{
			Response.CapabilityNotes.Add(FString::Printf(TEXT("warning: %s"), *W));
		}
		return Dispatch.Complete(Response);
	}
}

FString UUeremcpVoxelToolset::CarveSpline(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("carve_spline"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::CarveSpline(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::FlattenArea(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("flatten_area"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::FlattenArea(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::SmoothRegion(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("smooth_region"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::SmoothRegion(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::TerrainStamp(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("terrain_stamp"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::TerrainStamp(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::NoiseSculpt(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("noise_sculpt"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::NoiseSculpt(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::PaintMaterial(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("paint_material"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::PaintMaterial(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::GenerateWaterBody(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("generate_water_body"),
		[](AVoxelWorld* /*W*/, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::GenerateWaterBody(Spec, bDry);
		},
		false);
}

FString UUeremcpVoxelToolset::ProceduralScatter(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("procedural_scatter"),
		[](AVoxelWorld* /*W*/, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::ProceduralScatter(Spec, bDry);
		},
		false);
}

FString UUeremcpVoxelToolset::GeneratePois(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("generate_pois"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::GeneratePois(W, Spec, bDry);
		},
		true);
}

FString UUeremcpVoxelToolset::ComposeInteriorTerrain(const FString& RequestJson)
{
	return DispatchVoxelAction(
		RequestJson,
		TEXT("compose_interior_terrain"),
		[](AVoxelWorld* W, const TSharedPtr<FJsonObject>& Spec, bool bDry)
		{
			return FUeremcpVoxelService::ComposeInteriorTerrain(W, Spec, bDry);
		},
		true);
}
