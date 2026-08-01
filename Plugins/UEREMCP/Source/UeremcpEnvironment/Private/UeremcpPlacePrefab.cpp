// UEREMCP — place_prefab_on_landscape (MCP-010).
//
// place + snap + clear foundation as one operation. Order is the specification:
//   1. Spawn at (x, y, 0)
//   2. Snap via LandscapeZAt (reuse — do not write a second trace)
//   3. ClearFoliageInVolumes over mesh bounds expanded by clear_foliage_radius_cm
//   4. Optional flatten_pad — heightmap pad write via FLandscapeEditDataInterface

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
	float FlattenRadius = 0.f;
	float FlattenFalloff = 0.f;
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
		if (Request.Specification->TryGetObjectField(TEXT("flatten_pad"), Pad) && Pad)
		{
			bFlattenPad = true;
			double R = 0.0, F = 0.0;
			(*Pad)->TryGetNumberField(TEXT("radius_cm"), R);
			(*Pad)->TryGetNumberField(TEXT("falloff_cm"), F);
			FlattenRadius = float(R);
			FlattenFalloff = float(F);
			if (FlattenRadius <= 0.f)
			{
				FlattenRadius = 1200.f;
			}
		}
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
			bFlattenPad
				? *FString::Printf(TEXT(", flatten_pad r=%.0f falloff=%.0f"), FlattenRadius, FlattenFalloff)
				: TEXT(""));
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
	bool bSnapped = UeremcpWorldOps::LandscapeZAt(World, Actor->GetActorLocation(), Z);
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

	bool bFlattenApplied = false;
	int32 FlattenVerts = 0;
	FString FlattenError;
	if (bFlattenPad && bSnapped)
	{
		bFlattenApplied = UeremcpWorldOps::FlattenPadAt(
			World, LocationXY, Z, FlattenRadius, FlattenFalloff, FlattenError, FlattenVerts);
		if (bFlattenApplied)
		{
			float NewZ = Z;
			if (UeremcpWorldOps::LandscapeZAt(World, Actor->GetActorLocation(), NewZ))
			{
				const FVector Loc = Actor->GetActorLocation();
				Actor->SetActorLocation(FVector(Loc.X, Loc.Y, NewZ));
				Z = NewZ;
			}
		}
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
	if (bFlattenPad)
	{
		Result->SetBoolField(TEXT("flatten_pad_applied"), bFlattenApplied);
		Result->SetNumberField(TEXT("flatten_pad_verts"), FlattenVerts);
		if (!FlattenError.IsEmpty())
		{
			Result->SetStringField(TEXT("flatten_pad_error"), FlattenError);
		}
	}

	if (!bSnapped)
	{
		Response.Status = TEXT("partially_completed");
		Response.Summary = TEXT(
			"Prefab spawned but no landscape beneath the pivot — left at Z=0. "
			"Named, not dropped into the void.");
		Response.InterpretationNotes.Add(TEXT("no landscape beneath pivot"));
		if (bFlattenPad)
		{
			Response.CapabilityNotes.Add(
				TEXT("flatten_pad skipped because snap failed (no landscape under pivot)."));
		}
	}
	else if (bFlattenPad && !bFlattenApplied)
	{
		Response.Status = TEXT("partially_completed");
		Response.ErrorCode = TEXT("FLATTEN_PAD_UNSUPPORTED");
		Response.Summary = FString::Printf(
			TEXT("Placed and snapped %s; cleared %d foliage instance(s). "
				 "flatten_pad failed: %s"),
			*Actor->GetActorLabel(), Removed,
			FlattenError.IsEmpty() ? TEXT("unknown") : *FlattenError);
		Response.CapabilityNotes.Add(
			TEXT("Steps 1–3 applied. flatten_pad heightmap write failed — see flatten_pad_error."));
	}
	else
	{
		Response.Status = TEXT("created_with_warnings");
		if (bFlattenApplied)
		{
			Response.Summary = FString::Printf(
				TEXT("Placed %s on landscape (Z=%.1f); cleared %d foliage; "
					 "flattened pad (%d verts, r=%.0f)."),
				*Actor->GetActorLabel(), Actor->GetActorLocation().Z, Removed,
				FlattenVerts, FlattenRadius);
			Response.CapabilityNotes.Add(
				TEXT("flatten_pad wrote heightmap via FLandscapeEditDataInterface::SetHeightData "
					 "[VERIFIED: LandscapeEdit.h]."));
		}
		else
		{
			Response.Summary = FString::Printf(
				TEXT("Placed %s on landscape (Z=%.1f); cleared %d foliage instance(s)."),
				*Actor->GetActorLabel(), Actor->GetActorLocation().Z, Removed);
		}
	}

	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	Response.CapabilityNotes.Add(
		TEXT("Snap uses LandscapeZAt (landscape-only). A generic downward trace would "
			 "have stopped on canopy."));
	return FUeremcpEnvelope::SerializeResponse(Response);
}
