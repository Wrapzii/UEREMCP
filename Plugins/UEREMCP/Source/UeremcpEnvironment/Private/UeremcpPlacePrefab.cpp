// UEREMCP — place_prefab_on_landscape (MCP-010).
//
// place + snap + clear foundation as one operation. Order is the specification:
//   1. Spawn at (x, y, 0)
//   2. Snap via LandscapeZAt (reuse — do not write a second trace)
//   3. ClearFoliageInVolumes over mesh bounds expanded by clear_foliage_radius_cm
//   4. Optional flatten_pad — unsupported until heightmap pad write lands;
//      partially_completed naming it, everything else applied.

#include "UeremcpEnvironmentToolset.h"
#include "UeremcpWorldOpsHelpers.h"

#include "UeremcpEnvelope.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Dom/JsonObject.h"

namespace
{
	bool CommonPreamble(
		const FString& RequestJson,
		const TCHAR* ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejection)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("%s tool received action '%s'."),
					ExpectedAction, *OutRequest.Action));
			return false;
		}
		return true;
	}
}

FString UUeremcpEnvironmentToolset::PlacePrefabOnLandscape(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("place_prefab_on_landscape"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = UeremcpWorldOps::EditorWorld();
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}

	FString MeshPath;
	FVector2D LocationXY(0, 0);
	double Yaw = 0.0;
	double ClearRadius = 0.0;
	bool bFlattenPad = false;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("mesh_path"), MeshPath);
		const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("location_xy"), XY) && XY && XY->Num() >= 2)
		{
			LocationXY = FVector2D((*XY)[0]->AsNumber(), (*XY)[1]->AsNumber());
		}
		Request.Specification->TryGetNumberField(TEXT("rotation_yaw"), Yaw);
		Request.Specification->TryGetNumberField(TEXT("clear_foliage_radius_cm"), ClearRadius);
		const TSharedPtr<FJsonObject>* Pad = nullptr;
		bFlattenPad = Request.Specification->TryGetObjectField(TEXT("flatten_pad"), Pad) && Pad;
	}
	if (MeshPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("place_prefab_on_landscape requires specification.mesh_path."));
	}

	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Could not load StaticMesh '%s'."), *MeshPath),
			TEXT("MESH_PATH_MISSING"),
			nullptr);
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would place %s at (%.0f, %.0f), snap to landscape, clear foliage r=%.0f%s."),
			*MeshPath, LocationXY.X, LocationXY.Y, ClearRadius,
			bFlattenPad ? TEXT(", flatten_pad UNSUPPORTED") : TEXT(""));
		Response.Metrics.McpRoundTrips = 1;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
		FVector(LocationXY.X, LocationXY.Y, 0.f),
		FRotator(0.f, float(Yaw), 0.f),
		SpawnParams);
	if (!Actor)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("Failed to spawn StaticMeshActor."));
	}
	Actor->SetActorLabel(FString::Printf(TEXT("UEREMCP_Prefab_%s"), *Mesh->GetName()));
	Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	Actor->SetMobility(EComponentMobility::Static);

	float Z = 0.f;
	const bool bSnapped = UeremcpWorldOps::LandscapeZAt(World, Actor->GetActorLocation(), Z);
	if (bSnapped)
	{
		const FVector Loc = Actor->GetActorLocation();
		Actor->SetActorLocation(FVector(Loc.X, Loc.Y, Z));
	}

	int32 Removed = 0;
	int32 Inspected = 0;
	if (ClearRadius > 0.0)
	{
		const FBox MeshBounds = Mesh->GetBoundingBox().TransformBy(Actor->GetActorTransform());
		const FVector Centre = MeshBounds.GetCenter();
		const FVector Extent = MeshBounds.GetExtent() + FVector(ClearRadius, ClearRadius, ClearRadius);
		TArray<FBox> Volumes;
		Volumes.Add(FBox(Centre - Extent, Centre + Extent));
		Removed = UeremcpWorldOps::ClearFoliageInBoxes(World, Volumes, false, Inspected);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.PrimaryAsset = Actor->GetPathName();
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.AssetsAffected = 1;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("actor_label"), Actor->GetActorLabel());
	Result->SetBoolField(TEXT("snapped"), bSnapped);
	Result->SetNumberField(TEXT("foliage_removed"), Removed);
	Result->SetNumberField(TEXT("z"), Actor->GetActorLocation().Z);

	if (!bSnapped)
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = TEXT(
			"Prefab spawned but no landscape beneath the pivot — left at Z=0. "
			"Named, not dropped into the void.");
		Response.InterpretationNotes.Add(TEXT("no landscape beneath pivot"));
	}
	else if (bFlattenPad)
	{
		Response.Status = TEXT("partially_completed");
		Response.ErrorCode = TEXT("FLATTEN_PAD_UNSUPPORTED");
		Response.Summary = FString::Printf(
			TEXT("Placed and snapped %s; cleared %d foliage instance(s). "
				 "flatten_pad is not implemented yet — heightmap pad write deferred."),
			*Actor->GetActorLabel(), Removed);
		Response.CapabilityNotes.Add(
			TEXT("Steps 1–3 applied. flatten_pad rejected as unsupported rather than faked."));
		Result->SetBoolField(TEXT("flatten_pad_applied"), false);
	}
	else
	{
		Response.Status = TEXT("created_with_warnings");
		Response.Summary = FString::Printf(
			TEXT("Placed %s on landscape (Z=%.1f); cleared %d foliage instance(s)."),
			*Actor->GetActorLabel(), Actor->GetActorLocation().Z, Removed);
	}

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("Snap uses LandscapeZAt (landscape-only). A generic downward trace would "
			 "have stopped on canopy."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}
